/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2023 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "debug_level.h"
#include "plsync_cc_impl.h"
#include <gnuradio/expj.h>
#include <gnuradio/io_signature.h>
#include <gnuradio/math.h>
#include <set>

namespace gr {
namespace dvbs2rx {

plsync_cc::sptr plsync_cc::make(int gold_code,
                                int freq_est_period,
                                double sps,
                                int debug_level,
                                bool acm_vcm,
                                bool multistream,
                                uint64_t pls_filter_lo,
                                uint64_t pls_filter_hi)
{
    return gnuradio::get_initial_sptr(new plsync_cc_impl(gold_code,
                                                         freq_est_period,
                                                         sps,
                                                         debug_level,
                                                         acm_vcm,
                                                         multistream,
                                                         pls_filter_lo,
                                                         pls_filter_hi));
}


/*
 * The private constructor
 */
plsync_cc_impl::plsync_cc_impl(int gold_code,
                               int freq_est_period,
                               double sps,
                               int debug_level,
                               bool acm_vcm,
                               bool multistream,
                               uint64_t pls_filter_lo,
                               uint64_t pls_filter_hi)
    : gr::block("plsync_cc",
                gr::io_signature::make(1, 1, sizeof(gr_complex)),
                gr::io_signature::make(1, 1, sizeof(gr_complex))),
      d_debug_level(debug_level),
      d_sps(sps),
      d_acm_vcm(acm_vcm),
      d_plsc_decoder_enabled(true),
      d_locked(false),
      d_closed_loop(false),
      d_payload_state(payload_state_t::searching),
      d_phase_corr(0.0),
      d_cum_freq_offset(0.0),
      d_sof_cnt(0),
      d_frame_cnt(0),
      d_rejected_cnt(0),
      d_dummy_cnt(0)
{
    // Validate the PLS filters based on their population counts (Hamming weights)
    //
    // NOTE: a popcnt of 2 is ok in CCM if the target PLSs differ on the pilots flag only.
    // This is verified in the sequel after the PLS filters are parsed. On the other hand,
    // a popcnt greater than 2 is certainly a problem in CCM mode.
    uint64_t popcnt1, popcnt2;
    volk_64u_popcnt(&popcnt1, pls_filter_lo);
    volk_64u_popcnt(&popcnt2, pls_filter_hi);
    if (!acm_vcm && (popcnt1 + popcnt2) > 2)
        throw std::runtime_error(
            "PLS filter configured for multiple MODCOD or frame sizes in CCM mode");
    if ((popcnt1 + popcnt2) == 0)
        throw std::runtime_error("At least one PLS should be enabled in the filters");

    // PLS filters
    //
    // NOTE the "d_pls_enabled" and "expected_plsc" arrays play distinct roles:
    //
    // - The "d_pls_enabled" array determines whether this block shall output XFECFRAMEs
    //   embedded on PLFRAMEs with the given PLS.
    //
    // - The "expected_plsc" vector specifies the distinct PLS values that can be found in
    //   the input stream, which are equivalent to the expected PLSC (codeword) indexes to
    //   be processed by the PLSC decoder. More specifically, the "expected_plsc" vector
    //   holds a priori knowledge used to reduce the set of possibilities searched by the
    //   PLSC decoder. In ACM/VCM mode, it will either contain all 128 possible DVB-S2 PLS
    //   values, or a selected number of them. In CCM mode, it must contain a single
    //   value, in which case the PLSC decoder is effectively disabled for simplicity.
    //
    // - When operating in CCM mode with multiple TS streams (MIS mode), instead of single
    //   stream (SIS) mode, the input PLFRAME stream may contain dummy frames in addition
    //   to the selected PLS. Similarly, in ACM/VCM mode, even if running with a single TS
    //   stream, dummy frames must be expected and processed by the PLSC decoder). See the
    //   second row in Table D.2 of the standard.
    std::vector<uint8_t> expected_plsc;
    for (uint8_t pls = 0; pls < n_plsc_codewords; pls++) {
        bool enabled = (pls < 64) ? (pls_filter_lo & (1ULL << pls))
                                  : (pls_filter_hi & (1ULL << (pls - 64)));
        d_pls_enabled[pls] = enabled;
        if (enabled) {
            expected_plsc.push_back(pls);
        }
    }

    // In CCM mode, up to two PLSs are allowed, as long as they refer to the same MODCOD
    // and frame size (differring only on the pilots flag).
    if (!acm_vcm && expected_plsc.size() == 2) {
        pls_info_t info1(expected_plsc[0]);
        pls_info_t info2(expected_plsc[1]);
        if (info1.modcod != info2.modcod ||
            info1.short_fecframe != info2.short_fecframe) {
            throw std::runtime_error(
                "A single MODCOD and frame size should be selected in "
                "CCM mode");
        }
    }
    // Include the PLSs of the dummy PLFRAMEs allowed in MIS or ACM/VCM mode
    //
    // Dummy PLFRAMES have modcod=0, so the corresponding PLS should be 0. However, there
    // are no guarantees that the Tx sets short_fecframe=0 and pilots=0 when sending dummy
    // frames, so PLS values from 0 to 3 can be expected (TODO: confirm).
    if (multistream || acm_vcm) {
        for (uint8_t pls = 0; pls < 4; pls++) {
            // Add if not enabled in the pls_filter already:
            if (!(pls_filter_lo & (1 << pls))) {
                expected_plsc.push_back(pls);
            }
        }
    }
    // If a single PLS is expected in the end (CCM/SIS mode with pilot configuration
    // known), then cache the constant PLS info and simply disable the PLSC decoder. It
    // would return the same PLS for every frame anyway.
    if (expected_plsc.size() == 1) {
        d_plsc_decoder_enabled = false;
        d_ccm_sis_pls = pls_info_t(expected_plsc[0]);
    }
    std::sort(expected_plsc.begin(), expected_plsc.end());

    // Heap-allocated modules
    d_frame_sync = new frame_sync(debug_level);
    d_plsc_decoder = new plsc_decoder(std::move(expected_plsc), debug_level);
    d_freq_sync = new freq_sync(freq_est_period, debug_level);
    d_pl_descrambler = new pl_descrambler(gold_code);

    // When the PLSC decoder is disabled, set a fixed PLFRAME length on the frame
    // synchronizer instead of updating the length for every detected frame.
    if (!d_plsc_decoder_enabled) {
        d_frame_sync->set_frame_len(d_ccm_sis_pls.plframe_len);
    }

    /* Message port */
    message_port_register_out(d_port_id);

    /* This block only outputs data symbols, while pilot symbols are
     * retained. So until we find the need, tags are not propagated */
    set_tag_propagation_policy(TPP_DONT);

    // Make sure the output buffer always fits at least a slot (90 symbols)
    set_output_multiple(SLOT_LEN);
}

/*
 * Our virtual destructor.
 */
plsync_cc_impl::~plsync_cc_impl()
{
    delete d_frame_sync;
    delete d_plsc_decoder;
    delete d_freq_sync;
    delete d_pl_descrambler;
}

void plsync_cc_impl::forecast(int noutput_items, gr_vector_int& ninput_items_required)
{
    if (d_locked) {
        // The work function can process multiple PLFRAMEs in one call. Hence, the number
        // of required input symbols depends on the output space available for the current
        // and future frames. Let's start by the requirement for the current frame.
        if (d_payload_state == payload_state_t::searching) {
            // If the frame synchronizer is locked, and while still searching for the next
            // payload, the work function will only produce output if it processes enough
            // symbols to actually find a payload between two consecutive SOFs. Hence,
            // require the symbols remaining until the end of the subsequent PLHEADER.
            ninput_items_required[0] = d_next_frame_info.pls.payload_len -
                                       d_frame_sync->get_sym_count() + PLHEADER_LEN;
        } else {
            // The PL Sync block has the full payload buffered internally and is trying to
            // complete the payload processing. Hence, it doesn't need any further input
            // symbols for the current frame.
            ninput_items_required[0] = 0;
        }
        // If the output buffer can fit more than the remaining samples of the current
        // XFECFRAME (i.e., `n_remaining` below), the work function will be able to
        // proceed to the next PLFRAME. In this case, increase the input size forecast
        // assuming the subsequent frame(s) will have the same length as the current (in
        // VCM/ACM mode, this will likely be a poor forecast).
        int n_remaining = d_next_frame_info.pls.xfecframe_len - (d_idx.i_slot * SLOT_LEN);
        int n_excess = noutput_items - n_remaining; // beyond the current XFECFRAME
        if (n_excess > 0) {
            int n_extra_frames =
                1 + ((n_excess - 1) / d_next_frame_info.pls.xfecframe_len); // ceil
            ninput_items_required[0] +=
                d_next_frame_info.pls.plframe_len * n_extra_frames;
        }
    } else {
        // While the frame synchronizer is still trying to lock, assume conservatively
        // that we need an equal number of input items as output items. In reality,
        // however, at this point, it's more likely we will only consume the input but
        // won't produce any output until locked.
        ninput_items_required[0] = noutput_items;
    }

    // Unfortunately, it seems the runtime executor won't let the forecast request more
    // input items than output items (sort of, it's a little more complex than that). As a
    // workaround, cap the required input items based on the available output items.
    ninput_items_required[0] = std::min(ninput_items_required[0], noutput_items);
}

void plsync_cc_impl::handle_tags(int ninput_items)
{
    // Search for tags in the input buffer and copy them to the local queue. Do not repeat
    // the search over the samples already processed in a previous call.
    static const pmt::pmt_t tag_key = pmt::intern("rot_phase_inc");
    std::vector<tag_t> tags;
    uint64_t abs_start = std::max(nitems_read(0), d_rot_ctrl.last_tag_search_end);
    uint64_t abs_end = nitems_read(0) + ninput_items;
    get_tags_in_range(tags, 0, abs_start, abs_end, tag_key);
    for (const auto& tag : tags) {
        d_rot_ctrl.tag_queue.push(tag);
    }
    d_rot_ctrl.last_tag_search_end = abs_end;
}

void plsync_cc_impl::calibrate_tag_delay(const uint64_t abs_sof_idx, int tolerance)
{
    // Search for tags that occurred since the last search up to the current SOF
    // plus some tag delay tolerance.
    const uint64_t tag_search_end = abs_sof_idx + tolerance;
    std::vector<tag_t> tags;
    while (!d_rot_ctrl.tag_queue.empty()) {
        const auto& tag = d_rot_ctrl.tag_queue.front();
        if (tag.offset < d_rot_ctrl.tag_search_start) {
            // Unexpected lost tag. Do not worry about warning here. It will be warned
            // later when searching for unprocessed tags.
            d_rot_ctrl.tag_queue.pop();
        } else if (tag.offset >= d_rot_ctrl.tag_search_start &&
                   tag.offset < tag_search_end) {
            // Tag in the search range
            tags.push_back(tag);
            d_rot_ctrl.tag_queue.pop();
        } else {
            // Tag beyond the search range (in the future)
            break;
        }
    }

    // Prepare for the next tag search
    // NOTE the above search is over the interval [start,end).
    d_rot_ctrl.tag_search_start = tag_search_end;

    // Filter out duplicate tags
    //
    // As of GR v3.9, the symbol_sync_cc block can replicate tags and produce artificial
    // closely-spaced identical tags. Filter these out to avoid the "unexpected tag"
    // warning that follows.
    std::set<uint64_t> tag_set;
    auto it = tags.begin();
    while (it != tags.end()) {
        // The absolute offset to which the phase increment update was originally
        // scheduled identifies the update request, which should be unique.
        const uint64_t requested_offset = pmt::to_double(pmt::cdr(it->value));
        if (tag_set.count(requested_offset) > 0) {
            it = tags.erase(it); // remove duplicate
        } else {
            tag_set.insert(requested_offset);
            ++it;
        }
    }

    // If there are no tags for the current SOF, assume the same state as before:
    if (tags.empty()) {
        d_rot_ctrl.past = d_rot_ctrl.current;
    }

    // Process the tags
    for (unsigned j = 0; j < tags.size(); j++) {
        // Phase increment update offset originally requested
        const uint64_t requested_offset = pmt::to_double(pmt::cdr(tags[j].value));
        // NOTE: the requested sample offset is not the same as the actual incoming
        // tags[j].offset. The former is the offset requested by the control_rotator_freq
        // function, which applies to the sample count of the rotator block (processing
        // the sample stream). In contrast, tags[j].offset refers to the sample count on
        // the input stream (the symbol stream), which is normally after decimation. The
        // requested offset is useful to identify the scheduled phase increment update.

        // Every phase increment update tag should match with an update scheduled by this
        // block. Ideally, no other block should schedule phase increment updates so that
        // they don't interfere with the PL sync.
        auto map_it = d_rot_ctrl.update_map.find(requested_offset);
        if (map_it == d_rot_ctrl.update_map.end()) {
            d_logger->warn(
                "Unexpected phase inc update tag at offset {:d} (requested offset {:d})",
                tags[j].offset,
                requested_offset);
            continue;
        }

        // Always keep track of the current and previous rotator states.
        d_rot_ctrl.past = d_rot_ctrl.current;

        // We have found the map entry matching the incoming tag. However, that does not
        // mean the tag is scheduled for the current SOF at index "abs_sof_idx". The tag
        // search range can cover multiple PLFRAMEs, so we could be processing a tag sent
        // for a past SOF (e.g., after a brief frame lock loss). In any case, since we
        // know the target SOF index (saved on the update map), we can still measure the
        // tag placement error for past SOFs. Hence, go ahead and process a tag sent for a
        // past SOF. In contrast, do not expect to see a tag for a future SOF here.
        if (map_it->second.sof_idx > abs_sof_idx) {
            d_logger->warn("Got tag for a future SOF index ({:d})",
                           map_it->second.sof_idx);
        }

        // Error between the observed and expected tag offsets.
        //
        // Since we correct this error in closed loop, the observed error is the
        // residual after correction, not the raw tag delay. The total tag delay is
        // the cumulative sum of the residual errors observed each time. Eventually,
        // the error should converge to zero and oscillate around that.
        //
        // NOTE: use "map_it->second.sof_idx" instead of "abs_sof_idx" because we could be
        // processing a tag associated with a past SOF.
        const int error = map_it->second.sof_idx - tags[j].offset;
        d_rot_ctrl.tag_delay += error;

        // The tag confirms the rotator's frequency and when it started
        const double current_phase_inc = pmt::to_double(pmt::car(tags[j].value));
        d_rot_ctrl.current.freq = d_sps * current_phase_inc / (2.0 * GR_M_PI);
        d_rot_ctrl.current.idx = tags[j].offset;

        // Sanity check
        if (d_rot_ctrl.current.idx < d_rot_ctrl.past.idx) {
            d_logger->warn("Unexpected rotator state index order: "
                           "current={:d}, past={:d}",
                           d_rot_ctrl.current.idx,
                           d_rot_ctrl.past.idx);
        }

        // Flag that the frequency correction loop is now effectively closed (the
        // rotator blocks is actively helping this block).
        d_closed_loop = true;

        // Remove from the map
        d_rot_ctrl.update_map.erase(map_it);

        GR_LOG_DEBUG_LEVEL(3,
                           "Rotator ctrl - Tagged Phase Inc: {:+f}; Offset Error: {:+d}; "
                           "Delay: {:+d}",
                           current_phase_inc,
                           error,
                           d_rot_ctrl.tag_delay);
    }

    // Some phase increment update messages may arrive too late on the rotator block and
    // end up unprocessed. Clean those from the update map and warn:
    for (auto it = d_rot_ctrl.update_map.cbegin(); it != d_rot_ctrl.update_map.cend();) {
        if (it->second.sof_idx < abs_sof_idx) {
            GR_LOG_DEBUG_LEVEL(
                3,
                "Rotator ctrl - Timing out unprocessed update scheduled for offset {:d}",
                it->first);
            it = d_rot_ctrl.update_map.erase(it);
        } else {
            it++;
        }
    }
}

void plsync_cc_impl::control_rotator_freq(uint64_t abs_sof_idx,
                                          uint16_t plframe_len,
                                          double rot_freq_adj,
                                          bool ref_is_past_frame)
{
    // By default, schedule the phase increment change to take place at the start of the
    // next frame (the next SOF). This increment change may arrive at the rotator after
    // the target sample index has already been processed, in which case the rotator skips
    // the update, but this should be fine. There is little performance penalty in missing
    // some phase increment changes. The payload handler applies a feed-forward fine
    // frequency correction, which takes care of the residual frequency offset disturbing
    // the frame. Meanwhile, the correction sent to the external rotator is only meant to
    // track the incoming carrier so that the residual frequency offset remains within the
    // fine estimation range. See more comments on the handle_payload function.
    const uint64_t target_sof_idx = abs_sof_idx + plframe_len;

    // Assume the upstream rotator lies before a matched filter and, hence, operates on
    // the sample stream (i.e. on samples, not symbols). Use the known oversampling ratio
    // and the calibrated symbol-spaced tag delay to schedule the phase increment update.
    uint64_t target_samp_idx = d_sps * (target_sof_idx + d_rot_ctrl.tag_delay);

    // Prevent two frequency corrections at the same sample offset. The scenario
    // where this becomes possible is as follows:
    //
    // - The frequency synchronizer has achieved the coarse-corrected state and
    //   the PLFRAMEs have pilots.
    //
    // - A new SOF comes and the coarse estimate at this SOF exceeds the fine
    //   estimation range. Hence, at this point the frequency synchronizer will
    //   toggle its coarse corrected state back to false and the PLHEADER
    //   handler will send the new coarse estimate to the rotator.
    //
    // - Next, the payload handler processes the PLFRAME preceding the SOF that
    //   triggered the coarse correction. The PLFRAME has pilots, and it came
    //   while the synchronizer was still coarse corrected (before the new
    //   PLHEADER). Hence, the payload handler calls for a new fine offset
    //   estimate, which is also sent to the rotator.
    //
    // In this case, both updates would be applicable to the same offset. By
    // preventing this scenario here, only the first update would be applied,
    // which, in this example, is the update due to the new coarse estimate.
    if (d_rot_ctrl.update_map.count(target_samp_idx) > 0)
        return;

    // Sanity check
    if (d_rot_ctrl.current.idx < d_rot_ctrl.past.idx ||
        target_samp_idx < d_rot_ctrl.current.idx) {
        d_logger->warn("Unexpected rotator control index order: target={:d}, "
                       "current={:d}, past={:d}",
                       target_samp_idx,
                       d_rot_ctrl.current.idx,
                       d_rot_ctrl.past.idx);
    }
    // NOTE: the "current" and "past" indexes will be equal on consecutive frames that
    // don't experience any frequency correction.

    // Cumulative frequency offset estimate
    //
    // NOTE: Extra caution is required when accumulating the new frequency offset estimate
    // to a previous estimate. The reference depends on which frame was used to generate
    // the new frequency offset estimate. Refer to the comments on this function's
    // declaration (on plsync_cc_impl.h). Also, note the rotator's rotating frequency is
    // the negative of the estimated frequency offset since it corrects the offset.
    const double ref_rot_freq =
        (ref_is_past_frame) ? d_rot_ctrl.past.freq : d_rot_ctrl.current.freq;
    const double prev_cum_freq_offset = -ref_rot_freq;
    d_cum_freq_offset = prev_cum_freq_offset + rot_freq_adj;

    // Rotator phase increment that should start taking effect on the next frame:
    const double phase_inc = -d_cum_freq_offset * 2.0 * GR_M_PI / d_sps;
    d_rot_ctrl.update_map.emplace(std::piecewise_construct,
                                  std::forward_as_tuple(target_samp_idx),
                                  std::forward_as_tuple(phase_inc, target_sof_idx));

    // Send the corresponding phase increment to the rotator block
    static const pmt::pmt_t inc_key = pmt::intern("inc");
    static const pmt::pmt_t offset_key = pmt::intern("offset");
    pmt::pmt_t msg = pmt::make_dict();
    msg = pmt::dict_add(msg, inc_key, pmt::from_double(phase_inc));
    msg = pmt::dict_add(msg, offset_key, pmt::from_uint64(target_samp_idx));
    message_port_pub(d_port_id, msg);

    GR_LOG_DEBUG_LEVEL(2, "Cumulative frequency offset: {:g}", d_cum_freq_offset);
    GR_LOG_DEBUG_LEVEL(3,
                       "Rotator ctrl - Sent New Phase Inc: {:+f}; Target SOF Index: "
                       "{:d}; Rot Sample Offset: {:d}",
                       phase_inc,
                       target_sof_idx,
                       target_samp_idx);
}

void plframe_idx_t::step(uint16_t n_slots, bool has_pilots)
{
    i_slot += n_slots;
    i_pilot_blk = (has_pilots) ? i_slot / SLOTS_PER_PILOT_BLK : 0;
    i_in_payload = i_slot * SLOT_LEN + i_pilot_blk * PILOT_BLK_LEN;
}

void plframe_idx_t::reset()
{
    i_in_payload = 0;
    i_pilot_blk = 0;
    i_slot = 0;
}

void plsync_cc_impl::handle_plheader(uint64_t abs_sof_idx,
                                     const gr_complex* p_plheader,
                                     plframe_info_t& frame_info)
{
    // Cache the SOF index
    frame_info.abs_sof_idx = abs_sof_idx;

    /* Calibrate the delay between the upstream rotator and the PL Sync block.
     *
     * NOTE: this function is executed first because it updates the d_closed_loop state,
     * which is required below on the `derotate_plheader()` call.
     **/
    calibrate_tag_delay(abs_sof_idx);

    /* Coarse frequency offset estimation
     *
     * The frequency synchronizer offers two coarse estimation options: based on the SOF
     * only and based on the entire PLHEADER. The PLHEADER version requires the expected
     * PLHEADER symbols, which, in turn, requires accurate decoding of the PLSC
     * beforehand. This is a chicken-and-egg situation, because an accurate coarse
     * frequency offset estimation is typically required to decode the PLSC well.
     *
     * To work around this limitation, the adopted strategy is as follows:
     *
     * - On startup, while not coarse-corrected yet, estimate the coarse frequency offset
     *   based on the SOF only. Do not rely on the decoded PLSC, as it could be poorly
     *   decoded. Assume that, so long as the SOF-based estimation accumulates a
     *   sufficient number of frames, eventually it will lead to a reasonable estimate,
     *   accurate enough to bring the residual frequency offset within the fine estimation
     *   range (i.e., to enter the "coarse-corrected" state).
     *
     * - Once in coarse-corrected state, assume the PLSC decoding is now sufficiently
     *   accurate, such that it is possible to tell the true PLHEADER symbols on each
     *   frame with good confidence. Hence, start estimating the coarse frequency offset
     *   based on the entire PLHEADER. At this point, the PLHEADER phase should rotate by
     *   no more than roughly 0.2 degrees in the course of 90 symbols. Hence, the residual
     *   frequency offset should be harmless to the PLSC decoding.
     *
     * This strategy determines whether or not to use the full PLHEADER for the
     * estimation. The next question is when to execute the estimation, before or after
     * the PLSC decoding. The order is important because the PLSC decoding step can use
     * the frequency offset estimate to derotate the PLHEADER symbols before attempting to
     * decode the underlying PLS code (see the open-loop operation there).
     *
     * Naturally, the coarse frequency offset estimation is called right away (before PLSC
     * decoding) when based on the SOF symbols only. The advantage of this choice is that
     * the subsequent PLSC decoding step can already benefit from the coarse frequency
     * offset estimate when running in open loop (again, to derotate the PLHEADER). In
     * contrast, the coarse estimation is called after PLSC decoding when based on the
     * full PLHEADER, since it needs the PLSC information.
     *
     * Besides, when the PLSC decoder is disabled (i.e., when the PLSC is known a priori),
     * the coarse frequency offset estimation can always use the full PLHEADER. In this
     * case, even if the estimate is poor (e.g., before locking the frame timing), it does
     * not compromise the PLSC decoding, since there is no decoding. Also, in this
     * scenario, there is no question as to whether the estimation should be called before
     * or after the PLSC decoding, because, again, there is no decoding.
     *
     * Finally, it's important to understand the motivation for using the full PLHEADER in
     * the coarse frequency offset estimation. If it has the risk of corrupting the
     * estimation due to an erroneously decoded PLSC, why bother about it? The reason is
     * that it achieves significantly superior performance. By using the full PLHEADER,
     * the estimates are based on more data points, namely more extensive averaging, which
     * is helpful to avoid eventual noisy estimates. This is particularly useful because
     * the coarse estimation continues even after entering the coarse-corrected state, as
     * a way to periodically verify the residual frequency offset is low enough to be
     * captured by the fine frequency offset estimator. In this process, it is crucial to
     * avoid noisy coarse estimates that could bring the synchronizer out of the
     * coarse-corrected state, as it takes long to recover this state (depending on the
     * freq_sync.period parameter), and the receiver cannot operate reliably before that.
     * Namely, a noisy estimate exceeding the fine estimation range would easily lead to
     * the loss of a few frames until a better estimate is obtained.
     **/
    const bool was_coarse_corrected = frame_info.coarse_corrected; // last state
    const bool est_coarse_with_full_plheader =
        was_coarse_corrected || !d_plsc_decoder_enabled;
    bool new_coarse_est;
    if (!est_coarse_with_full_plheader) {
        new_coarse_est = d_freq_sync->estimate_coarse(p_plheader, false /* SOF only */);
        frame_info.coarse_corrected = d_freq_sync->is_coarse_corrected();
    }

    /* Decode the pi/2 BPSK-mapped and scrambled PLSC symbols
     *
     * The PLSC decoder offers coherent and non-coherent decoding alternatives,
     * both with hard or soft demapping. Use the default coherent soft decoder.
     *
     * In CCM/SIS mode, skip the decoding and assume the PLS is always the same. */
    if (d_plsc_decoder_enabled) {
        /* First, de-rotate the PLHEADER's pi/2 BPSK symbols. Run the derotation in
         * open-loop mode while the external de-rotator block is not adjusted yet. */
        d_freq_sync->derotate_plheader(p_plheader, !d_closed_loop);
        const gr_complex* p_pp_plheader = d_freq_sync->get_plheader();

        /* Decode the de-rotated PLHEADER */
        d_plsc_decoder->decode(p_pp_plheader + SOF_LEN - 1);
        d_plsc_decoder->get_info(&frame_info.pls);

        /* Tell the frame synchronizer what the frame length is, so that it
         * knows when to expect the next SOF */
        d_frame_sync->set_frame_len(frame_info.pls.plframe_len);
    } else {
        frame_info.pls = d_ccm_sis_pls;
    }

    // As mentioned earlier, estimate the coarse frequency offset here (after the PLSC
    // decoding) if the frequency synchronizer was already coarse-corrected before or if
    // the PLSC decoder is disabled (when the PLS is known a priori).
    if (est_coarse_with_full_plheader) {
        new_coarse_est = d_freq_sync->estimate_coarse(
            p_plheader, true /* full PLHEADER */, frame_info.pls.plsc);
        frame_info.coarse_corrected = d_freq_sync->is_coarse_corrected();
    }

    // Save the coarse frequency offset estimate so that we can know the offset affecting
    // each frame. The frequency synchronizer object only tracks the most recent estimate.
    frame_info.coarse_foffset = d_freq_sync->get_coarse_foffset();

    /* Control the frequency correction applied by the external rotator.
     *
     * Do so only when locked to maximize the chances of the actual phase increment update
     * landing at the start of the upcoming PLFRAME. Also, once the coarse-corrected state
     * is achieved, let only the fine frequency offset estimations be used to regulate the
     * external rotator. The fine estimates are a lot more accurate than the coarse
     * estimates, as their names imply. Lastly, if this is a dummy frame, do not send the
     * correction. The dummy frames are too short, so there is a good chance the
     * correction will arrive late at the rotator message port, i.e., after the rotator
     * has already processed the corresponding index, which we should avoid. */
    if (d_locked && !frame_info.coarse_corrected && new_coarse_est &&
        !frame_info.pls.dummy_frame) {
        control_rotator_freq(abs_sof_idx,
                             frame_info.pls.plframe_len,
                             frame_info.coarse_foffset,
                             false /* reference is the current frame */);
    }

    /* Copy the full original PLHEADER (before derotation) to the frame info structure.
     * The PLHEADER can be used later, e.g., for fine frequency offset estimation. */
    memcpy(frame_info.plheader.data(), p_plheader, PLHEADER_LEN * sizeof(gr_complex));

    // Estimate also the PLHEADER phase and save for later
    frame_info.plheader_phase =
        d_freq_sync->estimate_plheader_phase(p_plheader, frame_info.pls.plsc);
}

int plsync_cc_impl::handle_payload(int noutput_items,
                                   gr_complex* out,
                                   const gr_complex* p_payload,
                                   plframe_info_t& frame_info,
                                   const plframe_info_t& next_frame_info)
{
    const gr_complex* p_descrambled_payload = d_pl_descrambler->get_payload();

    // Start with the processing steps that don't depend on the output buffer
    if (d_payload_state != payload_state_t::partial) {
        // Descramble the payload
        d_pl_descrambler->descramble(p_payload, frame_info.pls.payload_len);

        // Update the phase correction based on the PLHEADER phase
        d_phase_corr = gr_expj(-frame_info.plheader_phase);

        // Fine frequency offset estimation
        //
        // Note the fine estimate only makes sense after the coarse frequency offset
        // correction. The residual normalized frequency offset must fall within the fine
        // estimation range of up to ~3.3e-4. Furthermore, note the coarse-corrected state
        // that matters is the one that the frequency synchronizer had at the start of
        // this PLFRAME. Since, at this point, the frequency synchronizer has already
        // processed the subsequent PLHEADER, we must check the state cached on the
        // `frame_info` structure.
        bool new_fine_est = false;
        if (frame_info.coarse_corrected) {
            if (frame_info.pls.has_pilots) {
                d_freq_sync->estimate_fine_pilot_mode(frame_info.plheader.data(),
                                                      p_descrambled_payload,
                                                      frame_info.pls.n_pilots,
                                                      frame_info.pls.plsc);
                new_fine_est = true;
            } else {
                new_fine_est = d_freq_sync->estimate_fine_pilotless_mode(
                    frame_info.plheader_phase,
                    next_frame_info.plheader_phase,
                    frame_info.pls.plframe_len,
                    frame_info.coarse_foffset);
            }
        }
        frame_info.fine_foffset = new_fine_est ? d_freq_sync->get_fine_foffset() : 0;

        // If there is a new fine frequency offset estimate, update the external rotator
        if (new_fine_est) {
            // Since we always process the payload in between two SOFs, we've already
            // processed SOF n+1 at this point. Hence, schedule the frequency update for
            // SOF n+2. For that, use the next_frame_info, which holds the absolute index
            // where SOF n+1 starts and the length of PLFRAME n+1.
            control_rotator_freq(next_frame_info.abs_sof_idx,
                                 next_frame_info.pls.plframe_len,
                                 frame_info.fine_foffset,
                                 true /* reference is the previous frame */);
        }

        // We are done with the processing that does not depend on the output
        // buffer. Next time handle_payload is called for this payload, go
        // straight to the next step, which requires space in the output buffer.
        d_payload_state = payload_state_t::partial;
    }

    // Feed-forward fine frequency correction
    //
    // As indicated above, this payload's fine frequency offset estimate adjusts the
    // external rotator at the start of PLFRAME n+2. This closed-loop control mechanism is
    // essential for frequency tracking, as it ensures the residual frequency offset is
    // always small. For instance, if the frequency offset changes linearly by 1e-3 in the
    // course of 10 frames, then updating the rotator on every frame with a 1e-4
    // adjustment ensures the residual frequency offset remains in the order of 1e-4 per
    // frame. Namely, the residual remains small enough to be estimated by the fine
    // frequency offset estimator. Ultimately, the frequency synchronizer does not lose
    // its coarse-corrected state even if the true frequency offset changes widely.
    //
    // On the other hand, the correction delay of two frames can be unbearable if the true
    // frequency offset is changing rapidly. The residual frequency offset disturbing the
    // current frame could be high enough to yield rapid phase changes that would
    // significantly impair the detection. Hence, it is better not to wait until the
    // external rotator's correction takes effect on frame n+2. Instead, it is better to
    // de-rotate the current frame immediately.
    //
    // Using this approach, note there are effectively two frequency correction layers:
    // the external and the internal. The two correction layers can coexist seamlessly.
    // The external correction continuously tracks the frequency offset in closed-loop
    // despite the two-frame delay. Meanwhile, the internal layer applies a feed-forward
    // correction (i.e., in open-loop) focusing on the residual frequency offset remaining
    // from the external (delayed) correction over this frame only.
    const float phase_inc =
        frame_info.coarse_corrected ? (2.0 * GR_M_PI * frame_info.fine_foffset) : 0;
    gr_complex expj_phase_inc = gr_expj(-phase_inc);

    // Output the phase-corrected and descrambled data symbols.
    int n_produced = 0;
    uint16_t n_slots_out = noutput_items / SLOT_LEN;
    if (frame_info.pls.has_pilots) {
        // Process each 16-slot segment between pilot blocks at a time
        uint16_t i_slot = 0;
        while (i_slot < n_slots_out && d_idx.i_slot < frame_info.pls.n_slots) {
            // Try to process as many slots as possible up to the next pilot block, when
            // the phase correction changes. If we are already past the last pilot block,
            // try to process as many slots as possible up to the end of the PLFRAME.
            uint16_t max_slots_to_process =
                (d_idx.i_pilot_blk == frame_info.pls.n_pilots)
                    ? (frame_info.pls.n_slots - d_idx.i_slot)
                    : (SLOTS_PER_PILOT_BLK - (d_idx.i_slot % SLOTS_PER_PILOT_BLK));
            uint16_t slots_remaining = n_slots_out - i_slot;
            uint16_t slots_to_process = std::min(slots_remaining, max_slots_to_process);
            uint16_t slot_seq_len = slots_to_process * SLOT_LEN;
            assert((noutput_items - n_produced) >= slot_seq_len);

            // Pointer to the next sequence of slots
            const gr_complex* p_slot_seq = p_descrambled_payload + d_idx.i_in_payload;

            // Reset the rotator phase whenever a new 16-slot sequence starts. Set it
            // equal to the phase estimate obtained from the most recent (preceding)
            // 36-symbol pilot block. Skip the very first 16-slot sequence, given it is
            // preceded by the PLHEADER instead of a pilot block. Also, do so only if
            // coarse-corrected, as otherwise the pilot phase estimates are uninitialized.
            //
            // NOTE: there is always an interation where the current slot (d_idx.i_slot)
            // is the starting slot of a 16-slot sequence due to the limit imposed by the
            // "max_slots_to_process" variable. Hence, the conditional below is guaranteed
            // to be hit for every 16-slot sequence.
            if (frame_info.coarse_corrected && d_idx.i_pilot_blk > 0 &&
                (d_idx.i_slot % SLOTS_PER_PILOT_BLK) == 0) {
                float pilot_phase = d_freq_sync->get_pilot_phase(d_idx.i_pilot_blk - 1);
                d_phase_corr = gr_expj(-pilot_phase);
            }

            // De-rotate the slot sequence
            volk_32fc_s32fc_x2_rotator_32fc(out + n_produced,
                                            p_slot_seq,
                                            expj_phase_inc,
                                            &d_phase_corr,
                                            slot_seq_len);

            n_produced += slot_seq_len;

            d_idx.step(slots_to_process, frame_info.pls.has_pilots);
            i_slot += slots_to_process;
        }
    } else {
        // Process as many slots as possible in one go
        uint16_t max_slots_to_process = frame_info.pls.n_slots - d_idx.i_slot;
        uint16_t slots_to_process = std::min(n_slots_out, max_slots_to_process);
        uint16_t slot_seq_len = slots_to_process * SLOT_LEN;

        // Pointer to the next sequence of slots
        const gr_complex* p_slot_seq = p_descrambled_payload + d_idx.i_in_payload;

        // De-rotate the slot sequence
        volk_32fc_s32fc_x2_rotator_32fc(
            out, p_slot_seq, expj_phase_inc, &d_phase_corr, slot_seq_len);
        n_produced += slot_seq_len;

        d_idx.step(slots_to_process, frame_info.pls.has_pilots);
    }

    // Finalize
    if (d_idx.i_slot == frame_info.pls.n_slots) {
        d_idx.reset();
        d_payload_state = payload_state_t::searching;
    }

    return n_produced;
}

int plsync_cc_impl::general_work(int noutput_items,
                                 gr_vector_int& ninput_items,
                                 gr_vector_const_void_star& input_items,
                                 gr_vector_void_star& output_items)
{
    const gr_complex* in = (const gr_complex*)input_items[0];
    gr_complex* out = (gr_complex*)output_items[0];
    int n_produced = 0, n_consumed = 0;

    // Copy the desired tags to the local queue before anything else
    handle_tags(ninput_items[0]);

    // Keep processing as long as:
    //
    // 1) There are input samples to consume. If the input buffer is empty, we may still
    //    produce some output if there is a payload whose processing is pending.
    //
    // 2) There is space in the output buffer. If the output buffer is full, we may still
    //    consume input samples as long as we are still searching for the next frame.
    //
    const bool empty_input = ninput_items[0] == 0;
    bool full_output = false;
    while ((n_consumed < ninput_items[0] ||
            (empty_input && d_payload_state != payload_state_t::searching)) &&
           (n_produced < noutput_items ||
            (full_output && d_payload_state == payload_state_t::searching))) {
        // If there is no payload waiting to be processed, consume the input stream until
        // the next SOF/PLHEADER is found by the frame synchronizer.
        if (d_payload_state == payload_state_t::searching) {
            for (int i = n_consumed; i < ninput_items[0]; i++) {
                // Step the frame synchronizer and keep refreshing the locked state. It
                // could change any time.
                bool is_sof = d_frame_sync->step(in[i]);
                d_locked = d_frame_sync->is_locked();
                n_consumed++;
                if (!is_sof)
                    continue;
                d_sof_cnt++;

                // Convert the relative SOF detection index to an absolute index
                // corresponding to the first SOF/PLHEADER symbol. Consider that
                // the SOF detection happens at the last PLHEADER symbol.
                const uint64_t abs_sof_idx = nitems_read(0) + i - 89;
                GR_LOG_DEBUG_LEVEL(
                    2, "SOF count: {:d}; Index: {:d}", d_sof_cnt, abs_sof_idx);

                // Cache some information from the last PLFRAME before handling the new
                // PLHEADER. As soon as the PLHEADER is handled, the PLSC decoder will
                // update its PL signaling info, and other state variables will change.
                // However, because we always process the payload between two SOFs, the
                // payload of interest is the one corresponding to the preceding PLHEADER,
                // not the succeeding PLHEADER that is about to be handled.
                //
                // In addition to the PL signaling information, the following variables
                // also need to be cached:
                //
                // - The coarse corrected state. We want to know if the frequency
                //   synchronizer was already coarse-corrected at the time of the PLFRAME
                //   being processed, not at the time of the next PLHEADER that is about
                //   to be processed.
                //
                // - The PLHEADER sequence itself. We need it in order to estimate the
                //   PLHEADER phase when processing the payload.
                //
                // This metadata is contained within the plframe_info_t structure. Once
                // `handle_plheader()` is called, it updates `d_next_frame_info`. Hence,
                // cache the `d_next_frame_info` from the previous SOF, which is the
                // current frame to be processed by the `handle_payload()` function.
                d_curr_frame_info = d_next_frame_info;

                // The PLHEADER can always be processed right away because it doesn't
                // produce any output (no need to worry about noutput_items). Also, at
                // this point, and only at this point, the PLHEADER is available inside
                // the frame synchronizer and can be fetched via the
                // `frame_sync.get_plheader()` method.
                handle_plheader(
                    abs_sof_idx, d_frame_sync->get_plheader(), d_next_frame_info);

                // If this is the first SOF ever, keep going until the next. We take the
                // PLFRAME payload as the sequence of symbols between two SOFs. Hence, we
                // need at least two SOF detections.
                static bool first_sof = true;
                if (first_sof) {
                    first_sof = false;
                    continue;
                }

                // Before locking, it's hard to tell with some confidence that a valid
                // PLFRAME lies between the preceding and the current SOF. Keep going
                // until frame lock is achieved.
                if (!d_locked)
                    continue;

                // Reject the frame if its PLS value is not enabled for processing. In CCM
                // mode, this rejection ensures the downstream blocks won't get any
                // accidental XFECFRAME of differing size, which could break the block's
                // notion of the XFECFRAME boundaries. In ACM/VCM mode, this rejection is
                // useful to prevent wrong frame detection when it's known a priori that
                // certain PLS values cannot be found in the input stream.
                if (!d_pls_enabled[d_curr_frame_info.pls.plsc]) {
                    GR_LOG_DEBUG_LEVEL(
                        2, "PLFRAME rejected (PLS={:d})", d_curr_frame_info.pls.plsc);
                    d_rejected_cnt++;
                    continue;
                }

                // If the PLFRAME between the present and the past SOFs is a dummy frame,
                // it doesn't produce any output. Skip it and keep going until the next.
                if (d_curr_frame_info.pls.dummy_frame) {
                    d_dummy_cnt++;
                    continue;
                }

                // The payload can only be processed if the expected XFECFRAME output fits
                // in the output buffer. Hence, unlike the PLHEADER, it may not be
                // processed right away. Mark the processing as pending for now and don't
                // consume any more input samples until this payload is handled.
                d_payload_state = payload_state_t::pending;
                d_frame_cnt++;

                // If running in ACM/VCM mode, tag the beginning of the XFECFRAME to
                // follow in the output. Include the MODCOD and the FECFRAME length so
                // that downstream blocks can de-map and decode this frame.
                if (d_acm_vcm) {
                    add_item_tag(
                        0, // output 0 (the only output port)
                        nitems_written(0) + n_produced,
                        pmt::string_to_symbol("XFECFRAME"),
                        pmt::cons(pmt::from_long(d_curr_frame_info.pls.modcod),
                                  pmt::from_bool(d_curr_frame_info.pls.short_fecframe)));
                }
                break;
            }
        }

        if (d_payload_state != payload_state_t::searching) {
            n_produced +=
                handle_payload((noutput_items - n_produced), // remaining output items
                               out + n_produced, // pointer to the next output item
                               d_frame_sync->get_payload(), // buffered frame payload
                               d_curr_frame_info,
                               d_next_frame_info);
            assert(n_produced <= noutput_items);
            full_output = n_produced == noutput_items;
        }
    }

    // Tell runtime system how many input/output items were consumed/produced.
    consume_each(n_consumed);
    return n_produced;
}

} // namespace dvbs2rx
} // namespace gr
