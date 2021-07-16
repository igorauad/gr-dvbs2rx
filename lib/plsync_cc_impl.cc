/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "plsync_cc_impl.h"
#include <gnuradio/expj.h>
#include <gnuradio/io_signature.h>
#include <gnuradio/math.h>

namespace gr {
namespace dvbs2rx {

plsync_cc::sptr
plsync_cc::make(int gold_code, int freq_est_period, float sps, int debug_level)
{
    return gnuradio::get_initial_sptr(
        new plsync_cc_impl(gold_code, freq_est_period, sps, debug_level));
}


/*
 * The private constructor
 */
plsync_cc_impl::plsync_cc_impl(int gold_code,
                               int freq_est_period,
                               float sps,
                               int debug_level)
    : gr::block("plsync_cc",
                gr::io_signature::make(1, 1, sizeof(gr_complex)),
                gr::io_signature::make(1, 1, sizeof(gr_complex))),
      d_debug_level(debug_level),
      d_sps(sps),
      d_locked(false),
      d_da_phase(0.0),
      d_payload_state(payload_state_t::searching),
      d_sof_cnt(0)
{
    d_frame_sync = new frame_sync(debug_level);
    d_plsc_decoder = new plsc_decoder(debug_level);
    d_freq_sync = new freq_sync(freq_est_period, debug_level);
    d_pl_descrambler = new pl_descrambler(gold_code);

    /* Message port */
    message_port_register_out(d_port_id);

    /* This block only outputs data symbols, while pilot symbols are
     * retained. So until we find the need, tags are not propagated */
    set_tag_propagation_policy(TPP_DONT);
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
    ninput_items_required[0] = noutput_items;
}

void plsync_cc_impl::calibrate_tag_delay(uint64_t abs_sof_idx, int tolerance)
{
    // Always keep track of the state at the previous SOF
    d_rot_ctrl.past = d_rot_ctrl.current;

    // Search for tags that occurred since the last search up to the current SOF
    // plus some tag delay tolerance.
    static const pmt::pmt_t tag_key = pmt::intern("rot_phase_inc");
    const uint64_t tag_search_end = abs_sof_idx + tolerance;
    std::vector<tag_t> tags;
    get_tags_in_range(tags, 0, d_rot_ctrl.tag_search_start, tag_search_end, tag_key);

    // Prepare for the next tag search
    // NOTE: get_tags_in_range searches within the interval [start,end).
    d_rot_ctrl.tag_search_start = tag_search_end;

    // Process the tags
    for (unsigned j = 0; j < tags.size(); j++) {
        // We don't expect the tag to come too often. The shortest PLFRAME has
        // 3330 symbols (32 slots + PLHEADER), and we only update the rotator
        // frequency once per PLFRAME at maximum. However, if the symbol_sync_cc
        // block is used upstream, as of GR v3.9, it can replicate tags and
        // produce artificial closely-spaced tags. Ignore them here.
        const int tag_interval = tags[j].offset - d_rot_ctrl.current.idx;
        if (d_rot_ctrl.current.idx > 0 &&
            tag_interval < 1000) // 1000 is arbitrary (< 3330)
            continue;
        // NOTE: use "d_rot_ctrl.current" instead of "d_rot_ctrl.past" because
        // the former is assigned here, whereas the latter is assigned in
        // control_rotator_freq.

        // The tag confirms the frequency currently configured in the rotator
        const double current_phase_inc = pmt::to_double(tags[j].value);
        d_rot_ctrl.current.freq = -d_sps * current_phase_inc / (2.0 * GR_M_PI);
        d_rot_ctrl.current.idx = tags[j].offset;

        // Error between the observed and expected tag offsets. Since we correct
        // this error on every PLFRAME, the observed error is the residual after
        // correction, not the raw delay. The raw error (interpreted as the
        // delay) is the cumulative sum of the residuals. Eventually, the
        // residuals should converge to zero and oscillate around that.
        const int error = abs_sof_idx - tags[j].offset;
        d_rot_ctrl.tag_delay += error;

        if (abs(d_rot_ctrl.tag_delay) > tolerance) // sanity check
            printf("Warning: rot_phase_inc tag delay %d seems too high\n",
                   d_rot_ctrl.tag_delay);

        if (d_debug_level > 2) {
            printf("[Rotator ctrl] "
                   "Phase inc tag: %f\t"
                   "Offset: %lu\t"
                   "Expected: %lu\t"
                   "Error: %3d\t"
                   "Delay: %3d\n",
                   current_phase_inc,
                   tags[j].offset,
                   abs_sof_idx,
                   error,
                   d_rot_ctrl.tag_delay);
        }
    }
}

void plsync_cc_impl::control_rotator_freq(uint64_t abs_sof_idx,
                                          uint16_t plframe_len,
                                          double rot_freq_adj,
                                          bool ref_is_past_frame)
{
    // Schedule the phase increment to take place at the start of the next frame
    const uint64_t abs_next_sof_idx = abs_sof_idx + plframe_len;

    // Assume the upstream rotator lies before a matched filter and, hence,
    // operates on the sample stream (i.e. on samples, not symbols). Use the
    // known oversampling ratio and the calibrated tag delay to schedule the
    // phase increment update. */
    uint64_t target_idx = d_sps * (abs_next_sof_idx + d_rot_ctrl.tag_delay);

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
    //   PLHEADER). Hence, the handler calls for a new fine offset estimate,
    //   which is also sent to the rotator.
    //
    // In this case, both updates would be applicable to the same offset. By
    // preventing this scenario here, only the first update would be applied,
    // which, in this example, is the update due to the new coarse estimate.
    if (target_idx == d_rot_ctrl.next.idx)
        return;
    d_rot_ctrl.next.idx = target_idx;

    // Rotator frequency that should start taking effect on the next frame:
    //
    // NOTE: Extra caution is required when adding the frequency offset estimate
    // to the rotator's frequency, depending on which frame was used to generate
    // the new frequency offset estimate. Refer to the comments on this
    // function's declaration (on plsync_cc_impl.h).
    d_rot_ctrl.next.freq = (ref_is_past_frame) ? (d_rot_ctrl.past.freq + rot_freq_adj)
                                               : (d_rot_ctrl.current.freq + rot_freq_adj);

    // Sanity check
    assert(d_rot_ctrl.current.idx > d_rot_ctrl.past.idx &&
           d_rot_ctrl.next.idx > d_rot_ctrl.current.idx);

    // Send the corresponding phase increment to the rotator block
    static const pmt::pmt_t inc_key = pmt::intern("inc");
    static const pmt::pmt_t offset_key = pmt::intern("offset");
    const double phase_inc = -d_rot_ctrl.next.freq * 2.0 * GR_M_PI / d_sps;
    pmt::pmt_t msg = pmt::make_dict();
    msg = pmt::dict_add(msg, inc_key, pmt::from_double(phase_inc));
    msg = pmt::dict_add(msg, offset_key, pmt::from_uint64(d_rot_ctrl.next.idx));
    message_port_pub(d_port_id, msg);

    if (d_debug_level > 1) {
        printf("- Cumulative frequency offset: %g \n", d_rot_ctrl.next.freq);
    }

    if (d_debug_level > 2) {
        printf("[Rotator ctrl] "
               "New phase inc: %f\t"
               "Offset: %lu\t"
               "Sample offset: %lu\n",
               phase_inc,
               abs_next_sof_idx,
               d_rot_ctrl.next.idx);
    }
}

void plframe_idx_t::step(uint16_t plframe_len, bool has_pilots)
{
    i_in_frame++;
    if (i_in_frame >= (plframe_len - PLHEADER_LEN)) {
        // Already at the next PLHEADER
        is_data_sym = false;
        is_pilot_sym = false;
    } else if (has_pilots) {
        i_pilot_blk = i_in_frame / PILOT_BLK_PERIOD;
        const int j =
            i_in_frame - ((i_pilot_blk * PILOT_BLK_PERIOD) + PILOT_BLK_INTERVAL);
        is_pilot_sym = (j >= 0);
        i_in_pilot_blk = is_pilot_sym ? ((uint16_t)j) : 0;
        const uint16_t k = (i_in_frame - (i_pilot_blk * PILOT_BLK_LEN) - i_in_pilot_blk);
        i_in_slot = k % 90;
        i_slot = k / 90;
        is_data_sym = !is_pilot_sym;
    } else {
        i_in_slot = i_in_frame % 90;
        i_slot = i_in_frame / 90;
        is_data_sym = true;
        is_pilot_sym = false;
    }
}

void plframe_idx_t::reset()
{
    i_in_frame = 0;
    i_in_slot = 0;
    i_in_pilot_blk = 0;
    i_slot = 0;
    i_pilot_blk = 0;
    is_pilot_sym = false;
    is_data_sym = true;
}

void plsync_cc_impl::handle_plheader(uint64_t abs_sof_idx,
                                     const gr_complex* p_plheader,
                                     plframe_info_t& frame_info)
{
    // Cache the SOF index
    frame_info.abs_sof_idx = abs_sof_idx;

    /* Coarse frequency offset estimation
     *
     * The frequency synchronizer offers two coarse estimation options: based on
     * the SOF only or based on the entire PLHEADER. However, we would need
     * decode the PLSC first in order to use the full PLHEADER. Here, we opt for
     * estimating the coarse frequency offset before decoding the PLSC, mostly
     * to avoid estimation errors due to wrong PLSC decoding.
     *
     * TODO: Review this strategy once the PLSC decoding becomes more robust
     * based on a priori knowledge of the set of possible MODCODS. */
    bool new_coarse_est = d_freq_sync->estimate_coarse(p_plheader, false /* SOF only */);
    frame_info.coarse_corrected = d_freq_sync->is_coarse_corrected();

    /* Try to de-rotate the PLHEADER's pi/2 BPSK symbols and fetch the
     * post-processed (de-rotated) PLHEADER pi/2 BPSK symbols*/
    d_freq_sync->derotate_plheader(p_plheader);
    const gr_complex* p_pp_plheader = d_freq_sync->get_plheader();

    /* Decode the pi/2 BPSK-mapped and scrambled PLSC symbols
     *
     * The PLSC decoder offers coherent and non-coherent decoding alternatives,
     * both with hard or soft demapping. Use the default coherent soft decoder. */
    d_plsc_decoder->decode(p_pp_plheader + SOF_LEN - 1);
    d_plsc_decoder->get_info(&frame_info.pls);

    /* Tell the frame synchronizer what the frame length is, so that it
     * knows when to expect the next SOF */
    d_frame_sync->set_frame_len(frame_info.pls.plframe_len);

    /* Calibrate the delay between the upstream rotator and this block. */
    calibrate_tag_delay(abs_sof_idx);

    /* Control the frequency correction applied by the external rotator.
     *
     * Do so only when locked to maximize the chances of the actual phase
     * increment update landing at the start of the upcoming PLFRAME. Also, once
     * the coarse-corrected state is achieved, let only the fine frequency
     * offset estimations be used to regulate the external rotator. */
    if (d_locked && !frame_info.coarse_corrected && new_coarse_est) {
        control_rotator_freq(abs_sof_idx,
                             frame_info.pls.plframe_len,
                             d_freq_sync->get_coarse_foffset(),
                             false /* reference is the current frame */);
    }

    /* Copy the full PLHEADER to the frame information structure. The PLHEADER
     * can be used later, e.g., for fine frequency offset estimation. */
    memcpy(frame_info.plheader.data(), p_plheader, PLHEADER_LEN * sizeof(gr_complex));
}

int plsync_cc_impl::handle_payload(int noutput_items,
                                   gr_complex* out,
                                   const gr_complex* p_payload,
                                   const plframe_info_t& frame_info,
                                   const plframe_info_t& next_frame_info)
{
    const gr_complex* p_descrambled_payload = d_pl_descrambler->get_payload();

    // Start with the processing steps that don't depend on the output buffer
    if (d_payload_state != payload_state_t::partial) {
        // Descramble the payload
        d_pl_descrambler->descramble(p_payload, frame_info.pls.payload_len);

        /* Estimate the phase of the PLHEADER preceding the payload. This phase
         * is used both for phase correction of the output data symbols and fine
         * frequency offset estimation. */
        const gr_complex* p_plheader = frame_info.plheader.data();
        d_da_phase =
            d_freq_sync->estimate_plheader_phase(p_plheader, frame_info.pls.plsc);

        // If the PLFRAME has pilot symbols, estimate the phase of each pilot
        // block and, then, estimate the fine frequency offset.
        //
        // Note the fine estimate only makes sense after the coarse frequency
        // offset correction. The residual offset must fall within the fine
        // estimation range of up to ~3.3e-4 in normalized frequency.
        if (frame_info.pls.has_pilots && frame_info.coarse_corrected) {
            // Average phase of each descrambled pilot block
            for (int i = 0; i < frame_info.pls.n_pilots; i++) {
                const gr_complex* p_pilots =
                    p_descrambled_payload + ((i + 1) * PILOT_BLK_PERIOD) - PILOT_BLK_LEN;
                d_freq_sync->estimate_pilot_phase(p_pilots, i);
            }

            // Fine freq. offset based on the phase jump between pilot blocks
            d_freq_sync->estimate_fine_pilot_mode(frame_info.pls.n_pilots);

            // Schedule the rotator update
            //
            // NOTE: Since we always process the payload in between two SOFs,
            // we've already processed SOF n+1 at this point while we are
            // processing the n-th payload. Hence, we must schedule the
            // frequency update for SOF n+2. For that, we use the
            // next_frame_info, which holds the absolute index where SOF n+1
            // start and the length of PLFRAME n+1.
            control_rotator_freq(next_frame_info.abs_sof_idx,
                                 next_frame_info.pls.plframe_len,
                                 d_freq_sync->get_fine_foffset(),
                                 true /* reference is the previous frame */);
        }

        // We are done with the processing that does not depend on the output
        // buffer. Next time handle_payload is called for this payload, go
        // straight to the next step, which requires space in the output buffer.
        d_payload_state = payload_state_t::partial;
    }

    // Output the phase-corrected and descrambled data symbols.
    int n_produced = 0;

    for (int i = 0; i < noutput_items && d_idx.i_in_frame < frame_info.pls.payload_len;
         i++) {
        // If this PLFRAME has pilot blocks, update the phase estimate after
        // every pilot block. Do so only if coarse-corrected, as otherwise the
        // pilot phase estimates would be uninitialized.
        if (d_idx.is_pilot_sym && frame_info.coarse_corrected &&
            d_idx.i_in_pilot_blk == (PILOT_BLK_LEN - 1)) {
            d_da_phase = d_freq_sync->get_pilot_phase(d_idx.i_pilot_blk);
        }

        // Output
        if (d_idx.is_data_sym) {
            out[n_produced] =
                gr_expj(-d_da_phase) * p_descrambled_payload[d_idx.i_in_frame];
            n_produced++;
        }

        /* Update the frame indexes */
        d_idx.step(frame_info.pls.plframe_len, frame_info.pls.has_pilots);
    }

    // Finalize
    if (d_idx.i_in_frame == frame_info.pls.payload_len) {
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

    // If there is no payload waiting to be processed, consume the input stream
    // until the next SOF/PLHEADER is found.
    if (d_payload_state == payload_state_t::searching) {
        for (int i = 0; i < noutput_items; i++) {
            bool is_sof = d_frame_sync->step(in[i]);
            n_consumed++;
            if (is_sof) {
                d_sof_cnt++;

                // If this SOF came where the previous PLSC told it would be, we
                // are still locked.
                d_locked = d_frame_sync->is_locked();

                // Convert the relative SOF detection index to an absolute index
                // corresponding to the first SOF/PLHEADER symbol. Consider that
                // the SOF detection happens at the last PLHEADER symbol.
                const uint64_t abs_sof_idx = nitems_read(0) + i - 89;

                if (d_debug_level > 1)
                    printf("SOF count: %lu; Index: %lu\n", d_sof_cnt, abs_sof_idx);

                // Cache some information from the last PLFRAME before handling
                // the new PLHEADER. As soon as the PLHEADER is handled, the
                // PLSC decoder will update its PL signaling info, and other
                // state variables will change. However, because we always
                // process the payload between two SOFs, the payload of interest
                // is the one corresponding to the preceding PLHEADER, not the
                // succeeding PLHEADER that is about to be handled.
                //
                // In addition to the PL signaling information, the following
                // variables also need to be cached:
                //
                // - The coarse corrected state. We want to know if the
                //   frequency synchronizer was already coarse-corrected at the
                //   time of the PLFRAME being processed, not at the time of the
                //   next PLHEADER that is about to be processed.
                //
                // - The PLHEADER sequence itself. We need that in order to
                //   estimate the PLHEADER phase when processing the payload.
                //
                // This metadata is contained within the plframe_info_t
                // structure. Once `handle_plheader()` is called, it updates
                // `d_next_frame_info`. Hence, cache the `d_next_frame_info`
                // from the previous SOF, which is the current frame to be
                // processed by the `handle_payload()` function.
                d_curr_frame_info = d_next_frame_info;

                // The PLHEADER can always be processed right away because it
                // doesn't produce any output (no need to worry about
                // noutput_items). Also, at this point, and only at this point,
                // the PLHEADER is available inside the frame synchronizer.
                handle_plheader(
                    abs_sof_idx, d_frame_sync->get_plheader(), d_next_frame_info);

                // If this is the first SOF ever, keep going until the next. We
                // take the PLFRAME payload as the sequence of symbols between
                // two SOFs. Hence, we need at least two SOF detections.
                static bool first_sof = true;
                if (first_sof) {
                    first_sof = false;
                    continue;
                }
                // Before locking, it's hard to tell with some confidence that a
                // valid PLFRAME lies between the preceding and the current SOF.
                // Keep going until frame lock is achieved.
                if (!d_locked)
                    continue;

                // If the PLFRAME between the present and the past SOFs is a
                // dummy frame, it doesn't produce any output anyway, so skip it
                // and keep going until the next payload.
                if (d_curr_frame_info.pls.dummy_frame)
                    continue;

                // The payload can only be processed if the expected XFECFRAME
                // output fits in the output buffer. Hence, unlike the PLHEADER,
                // it cannot be processed right away. Mark the processing as
                // pending for now and don't consume any more input samples
                // until this payload is handled.
                //
                // Besides, tag the beginning of the XFECFRAME to follow in the
                // output. Include the MODCOD and the FECFRAME length so that
                // downstream blocks can de-map and decode it in ACM/VCM mode.
                d_payload_state = payload_state_t::pending;
                add_item_tag(
                    0,
                    nitems_written(0), // note n_produced = 0 at this point
                    pmt::string_to_symbol("XFECFRAME"),
                    pmt::cons(pmt::from_long(d_curr_frame_info.pls.modcod),
                              pmt::from_bool(d_curr_frame_info.pls.short_fecframe)));
                break;
            }
        }
    }
    if (d_payload_state != payload_state_t::searching) {
        n_produced += handle_payload(noutput_items,
                                     out,
                                     d_frame_sync->get_payload(),
                                     d_curr_frame_info,
                                     d_next_frame_info);
    }

    // Tell runtime system how many input/output items were consumed/produced.
    consume_each(n_consumed);
    return n_produced;
}

} /* namespace dvbs2rx */
} /* namespace gr */
