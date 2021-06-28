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
      d_i_sym(0),
      d_locked(false),
      d_da_phase(0.0)
{
    d_frame_sync = new frame_sync(debug_level);
    d_plsc_decoder = new plsc_decoder(debug_level);
    d_freq_sync = new freq_sync(freq_est_period, debug_level);
    d_pl_descrambler = new pl_descrambler(gold_code);

    /* Buffer to store received pilots (PLHEADER and pilot blocks) */
    d_rx_pilots.resize(PLHEADER_LEN + (MAX_PILOT_BLKS * PILOT_BLK_LEN));

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

void plsync_cc_impl::calibrate_tag_delay(int sof_detection_idx,
                                         int noutput_items,
                                         int tolerance)
{
    // Search for tags that occurred since the last search up to the current
    // range of indexes being processed by the work function.
    static const pmt::pmt_t tag_key = pmt::intern("rot_phase_inc");
    const uint64_t n_read = nitems_read(0);
    const uint64_t tag_search_end = n_read + (uint64_t)(noutput_items);
    std::vector<tag_t> tags;
    get_tags_in_range(tags, 0, d_rot_ctrl.tag_search_start, tag_search_end, tag_key);

    // Prepare for the next tag search
    // NOTE: get_tags_in_range searches within the interval [start,end).
    d_rot_ctrl.tag_search_start = tag_search_end;

    // Convert the relative SOF detection index to an absolute index
    // corresponding to the first SOF/PLHEADER symbol. Assume the SOF detection
    // happens at the last PLHEADER symbol.
    const uint64_t abs_sof_idx = n_read + sof_detection_idx - 89;

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

        // Error between the observed and expected tag offsets. Since we correct
        // this error on every PLFRAME, the observed error is the residual after
        // correction, not the raw delay. The raw error (interpreted as the
        // delay) is the cumulative sum of the residuals. Eventually, the
        // residuals should converge to zero and oscillate around that.
        const int error = abs_sof_idx - tags[j].offset;
        d_rot_ctrl.tag_delay += error;

        // The tag confirms the frequency currently configured in the rotator
        const double current_phase_inc = pmt::to_double(tags[j].value);
        d_rot_ctrl.current.freq = -d_sps * current_phase_inc / (2.0 * GR_M_PI);
        d_rot_ctrl.current.idx = tags[j].offset;

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

void plsync_cc_impl::control_rotator_freq(int sof_detection_idx,
                                          bool coarse_corrected,
                                          bool new_coarse_est,
                                          bool new_fine_est)
{
    // Send control messages only when locked. Before that, the frequency offset
    // estimates can be very poor and are only supposed to be used internally.
    if (!d_locked)
        return;

    double rot_freq_adj = 0; // frequency adjustment
    if (coarse_corrected && new_fine_est)
        rot_freq_adj = d_freq_sync->get_fine_foffset();
    else if (!coarse_corrected && new_coarse_est)
        rot_freq_adj = d_freq_sync->get_coarse_foffset();
    else
        return;

    // Convert the relative SOF detection index to the absolute index of the
    // first symbol in the current PLHEADER/SOF:
    const uint64_t n_read = nitems_read(0);
    const uint64_t abs_sof_idx = n_read + sof_detection_idx - 89;

    // Schedule the phase increment to take place at the start of the next frame
    const uint64_t abs_next_sof_idx = abs_sof_idx + d_plsc_decoder->plframe_len;

    // Assume the upstream rotator lies before a matched filter and, hence,
    // operates on the sample stream (i.e. on samples, not symbols). Use the
    // known oversampling ratio and the calibrated tag delay to schedule the
    // phase increment update. */
    d_rot_ctrl.next.idx = d_sps * (abs_next_sof_idx + d_rot_ctrl.tag_delay);

    // Rotator frequency that should start taking effect on the next frame:
    //
    // NOTE: Extra caution is required when adding the frequency offset estimate
    // to the rotator's frequency. Assuming we are now processing the i-th
    // frame, note the fine estimate is based on frame i-1 and is only
    // effectively corrected by the upstream rotator on the start of frame
    // i+1. Hence, the fine estimate that was just obtained must be accumulated
    // to the frequency that still existed in the rotator during frame i-1.
    //
    // In other words, there is a two-frame delay in the correction process. To
    // overcome this, we track 3 stages of the rotator's frequency: at the
    // previous frame, the current and the next frame. The next frequency is the
    // correction scheduled via control message in the sequel.
    d_rot_ctrl.next.freq = d_rot_ctrl.past.freq + rot_freq_adj;

    // Sanity check
    if (d_rot_ctrl.current.idx < d_rot_ctrl.past.idx ||
        d_rot_ctrl.next.idx < d_rot_ctrl.current.idx) {
        printf("Warning: rotator frequency state has unexpected index(es)\n");
        return;
    }

    // Send the corresponding phase increment
    static const pmt::pmt_t inc_key = pmt::intern("inc");
    static const pmt::pmt_t offset_key = pmt::intern("offset");
    const double phase_inc = -d_rot_ctrl.next.freq * 2.0 * GR_M_PI / d_sps;
    pmt::pmt_t msg = pmt::make_dict();
    msg = pmt::dict_add(msg, inc_key, pmt::from_double(phase_inc));
    msg = pmt::dict_add(msg, offset_key, pmt::from_uint64(d_rot_ctrl.next.idx));
    message_port_pub(d_port_id, msg);

    if (d_debug_level > 1)
        printf("- Cumulative frequency offset: %g "
               "(coarse corrected? %u)\n",
               d_rot_ctrl.next.freq,
               coarse_corrected);

    if (d_debug_level > 2) {
        printf("[Rotator ctrl] "
               "New phase inc: %f\t"
               "Offset: %lu\t"
               "Sample offset: %lu\n",
               phase_inc,
               abs_next_sof_idx,
               d_rot_ctrl.next.idx);
    }

    // Prepare the state for the next frame
    d_rot_ctrl.past = d_rot_ctrl.current;
    /* NOTE: don't shift the "next" frequency correction into the "current". The
     * next value (at d_rot_ctrl.next.freq) is just an expectation for what the
     * next rotator frequency should be. We must receive the confirmation from
     * the rotator that the new increment was applied (see
     * calibrate_tag_delay). If there is no rotator upstream,
     * d_rot_ctrl.past.freq will be 0 forever. */
}

int plsync_cc_impl::general_work(int noutput_items,
                                 gr_vector_int& ninput_items,
                                 gr_vector_const_void_star& input_items,
                                 gr_vector_void_star& output_items)
{
    const gr_complex* in = (const gr_complex*)input_items[0];
    gr_complex* out = (gr_complex*)output_items[0];
    int n_produced = 0;

    bool is_sof;         // flag the start of frame (SOF)
    bool new_coarse_est; // a new coarse freq. offset was estimated
    bool new_fine_est;   // a new fine freq. offset was estimated
    gr_complex descrambled_sym;

    /* Tracking of data vs pilot symbols */
    uint16_t i_in_slot = 0;      /** symbol index in data slot */
    uint16_t i_slot = 0;         /** slot index */
    uint16_t i_in_pilot_blk = 0; /** symbol index in pilot block */
    uint16_t i_pilot_blk = 0;    /** pilot block index */
    bool is_pilot_sym = false;   /** current symbol is pilot (non-plheader) symbol */
    bool is_data_sym = false;    /** whether current symbol is data symbol */

    /* Symbol by symbol processing */
    for (int i = 0; i < noutput_items; i++) {
        /* Update indexes */
        if (d_i_sym >= (d_plsc_decoder->plframe_len - 90)) {
            // Already at the next PLHEADER
            is_data_sym = false;
            is_pilot_sym = false;
        } else if (d_plsc_decoder->has_pilots) {
            i_pilot_blk = d_i_sym / PILOT_BLK_PERIOD;
            const int j =
                d_i_sym - ((i_pilot_blk * PILOT_BLK_PERIOD) + PILOT_BLK_INTERVAL);
            is_pilot_sym = (j >= 0);
            i_in_pilot_blk = is_pilot_sym ? ((uint16_t)j) : 0;
            const uint16_t k = (d_i_sym - (i_pilot_blk * PILOT_BLK_LEN) - i_in_pilot_blk);
            i_in_slot = k % 90;
            i_slot = k / 90;
            is_data_sym = !is_pilot_sym;
        } else {
            i_in_slot = d_i_sym % 90;
            i_slot = d_i_sym / 90;
            is_data_sym = true;
            is_pilot_sym = false;
        }

        /* PL Descrambling */
        d_pl_descrambler->step(in[i], descrambled_sym, d_i_sym);
        d_i_sym++;

        /* Store symbols from pilot blocks (if any) for post-processing such as
         * fine frequency offset estimation
         *
         * NOTE: the descrambled symbols must be stored */
        if (is_pilot_sym && (i_pilot_blk < MAX_PILOT_BLKS) &&
            (i_in_pilot_blk < PILOT_BLK_LEN)) {
            const uint16_t z = (i_pilot_blk * PILOT_BLK_LEN) + i_in_pilot_blk; // 0 to 791
            d_rx_pilots[PLHEADER_LEN + z] = descrambled_sym;

            /* Once all symbols of the current pilot block have been
             * acquired, estimate the average phase of the block */
            if (i_in_pilot_blk == (PILOT_BLK_LEN - 1)) {
                d_da_phase =
                    d_freq_sync->estimate_pilot_phase(d_rx_pilots.data(), i_pilot_blk);
            }
        }

        /* Frame timing recovery */
        is_sof = d_frame_sync->step(in[i]);

        if (is_sof) {
            /* If this SOF came where the previous PLSC told it would
             * be, we are still locked */
            d_locked = d_frame_sync->get_locked();

            /* "New PLHEADER" - of the frame whose start has just been
             * detected */
            const gr_complex* p_plheader = d_frame_sync->get_plheader();

            /* Coarse frequency offset estimation
             *
             * When locked, the estimation is based on the past PLHEADER as well
             * as (freq_est_period - 1) PLHEADERs before it. The motivation for
             * using the past PLHEADER is that it is a safer bet. The fact that
             * frame sync is still locked suggests that the previous PLSC was
             * correctly decoded, so it is now safer to trust on the vector of
             * pilot symbols that were acquired throughout the previous frame.
             *
             * In contrast, when frame timing is still unlocked, it is still
             * somewhat beneficial to compute an estimate. We won't use this
             * estimate to control the external rotator, as it could be a very
             * poor estimate (e.g. on false positive SOF detection). Yet, we can
             * use it locally to derotate the PLHEADER in attempt to facilitate
             * PLSC decoding. For this unlocked estimation, only the SOF symbols
             * are used, since this way there is no need to rely on correct
             * decoding of the PLSC for the estimation itself. Also, while still
             * unlocked, the vector of pilot symbols (d_rx_pilots) won't hold
             * pilots (nor the PLHEADER). Yet, the current PLHEADER right now
             * (in this iteration) is available within the frame sync buffer
             * (p_plheader). So this is what we use when unlocked. */
            new_coarse_est =
                d_freq_sync->estimate_coarse(d_locked ? d_rx_pilots.data() : p_plheader,
                                             d_plsc_decoder->dec_plsc,
                                             d_locked);

            /* Fine frequency offset based on the previous frame
             *
             * Pre-requisites for valid estimation:
             *
             * 1) Frame timing is still locked, meaning it is very likely that
             * the previous PLSC was correctly decoded and, as a result, also
             * likely that the pilots were correctly acquired throughout the
             * frame.
             *
             * 2) The previous frame contained pilots. Note the PLSC decoder has
             * not processed the new PLHEADER yet, so it still has the info of
             * the previous frame. In other words, this must be called before
             * decoding the new PLSC.
             *
             * Pre-requisite for accurate estimation:
             *
             * - Coarse frequency offset must already have been corrected,
             * meaning the residual offset is small enough and falls within the
             * fine estimation range (up to 3.3e-4 in normalized frequency).
             */
            new_fine_est = d_locked && d_plsc_decoder->has_pilots;
            if (new_fine_est) {
                d_freq_sync->estimate_fine(d_rx_pilots.data(), d_plsc_decoder->n_pilots);
            }

            /* Try to de-rotate the PLHEADER's pi/2 BPSK symbols */
            d_freq_sync->derotate_plheader(p_plheader);

            /* Post-processed (de-rotated) PLHEADER pi/2 BPSK symbols */
            const gr_complex* p_pp_plheader = d_freq_sync->get_plheader();

            /* Decode the pi/2 BPSK-mapped and scrambled PLSC symbols */
            bool coarse_corrected = d_freq_sync->is_coarse_corrected();
            bool coherent_demap = d_locked && coarse_corrected;
            d_plsc_decoder->decode(p_pp_plheader + SOF_LEN - 1, coherent_demap);

            /* Tag the beginning of a XFECFRAME and include both the MODCOD and
             * whether FECFRAME is short, so that the downstream blocks can
             * de-map and decode it */
            if (d_plsc_decoder->modcod > 0)
                add_item_tag(0,
                             nitems_written(0) + n_produced,
                             pmt::string_to_symbol("XFECFRAME"),
                             pmt::cons(pmt::from_long(d_plsc_decoder->modcod),
                                       pmt::from_bool(d_plsc_decoder->short_fecframe)));

            /* Tell the frame synchronizer what the frame length is, so that it
             * knows when to expect a new SOF */
            d_frame_sync->set_frame_len(d_plsc_decoder->plframe_len);

            /* Calibrate the delay between the upstream rotator and this
             * block. Then, schedule a frequency update on the rotator. */
            calibrate_tag_delay(i, noutput_items);
            control_rotator_freq(i, coarse_corrected, new_coarse_est, new_fine_est);

            /* Save the symbols of the new PLHEADER into the pilot buffer, which
             * will be processed by the end of this frame for fine freq. offset
             * estimation.
             *
             * NOTE: this has to be executed only after calling the fine
             * frequency offset estimation, otherwise the new PLHEADER would
             * overwrite the previous PLHEADER that is needed for fine
             * estimation based on the previous frame.
             */
            memcpy(d_rx_pilots.data(), p_plheader, PLHEADER_LEN * sizeof(gr_complex));

            /* Estimate the phase of this PLHEADER - this phase estimate
             * can be used for fine frequency offset estimation in the
             * next frame in case it has pilots. e*/
            d_da_phase = d_freq_sync->estimate_plheader_phase(p_plheader,
                                                              d_plsc_decoder->dec_plsc);

            /* Reset the symbol indexes
             *
             * NOTE: the start of frame (SOF) is only detected when the
             * last (90th) symbol of the PLHEADER is processed by the
             * frame synchronizer block. Reset the symbol indexes: */
            d_i_sym = 0;
            i_in_slot = 0;
            i_slot = 0;
            i_in_pilot_blk = 0;
            i_pilot_blk = 0;
        }

        /* Output symbols with data-aided phase correction based on last pilot
         * blocks' phase estimate
         *
         * TODO: add phase tracking for when pilots are absent
         *
         * TODO: correct phase based on an "interpolated" version of the last
         * data-aided phase estimate. That is, try to predict how the phase
         * evolves over time in between pilot blocks and use such time-varying
         * prediction for correction.
         **/
        if (d_locked && is_data_sym & !d_plsc_decoder->dummy_frame) {
            out[n_produced] = gr_expj(-d_da_phase) * descrambled_sym;
            n_produced++;
        }
    }

    consume_each(noutput_items);

    // Tell runtime system how many output items we produced.
    return n_produced;
}
} /* namespace dvbs2rx */
} /* namespace gr */
