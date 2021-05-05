/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pl_frame_sync.h"
#include "util.h"
#include <algorithm>
#include <cassert>

namespace gr {
namespace dvbs2rx {

frame_sync::frame_sync(int debug_level)
    : debug_level(debug_level),
      sym_cnt(0),
      last_in(0),
      timing_metric(0.0),
      locked(false),
      locked_prev(false),
      frame_len(0),
      d_plsc_delay_buf(PLSC_LEN + 1),
      d_sof_buf(SOF_CORR_LEN),
      d_plsc_e_buf(PLSC_CORR_LEN),
      d_plsc_o_buf(PLSC_CORR_LEN),
      d_plheader_buf(PLHEADER_LEN)
{
    /* SOF and PLSC matched filter (correlator) taps: the folded (or reversed)
     * version of the target SOF and PLSC symbols */
    d_sof_taps = { (+0.0 - 1.0j), (+0.0 - 1.0j), (+0.0 - 1.0j), (+0.0 - 1.0j),
                   (+0.0 + 1.0j), (+0.0 + 1.0j), (+0.0 + 1.0j), (+0.0 + 1.0j),
                   (+0.0 - 1.0j), (+0.0 + 1.0j), (+0.0 + 1.0j), (+0.0 + 1.0j),
                   (+0.0 - 1.0j), (+0.0 + 1.0j), (+0.0 + 1.0j), (+0.0 - 1.0j),
                   (+0.0 - 1.0j), (+0.0 + 1.0j), (+0.0 - 1.0j), (+0.0 - 1.0j),
                   (+0.0 + 1.0j), (+0.0 - 1.0j), (+0.0 + 1.0j), (+0.0 + 1.0j),
                   (+0.0 - 1.0j) };
    d_plsc_taps = { (+0.0 - 1.0j), (+0.0 + 1.0j), (+0.0 + 1.0j), (+0.0 - 1.0j),
                    (+0.0 - 1.0j), (+0.0 - 1.0j), (+0.0 + 1.0j), (+0.0 - 1.0j),
                    (+0.0 - 1.0j), (+0.0 + 1.0j), (+0.0 + 1.0j), (+0.0 + 1.0j),
                    (+0.0 + 1.0j), (+0.0 + 1.0j), (+0.0 - 1.0j), (+0.0 - 1.0j),
                    (+0.0 - 1.0j), (+0.0 - 1.0j), (+0.0 + 1.0j), (+0.0 + 1.0j),
                    (+0.0 - 1.0j), (+0.0 + 1.0j), (+0.0 + 1.0j), (+0.0 - 1.0j),
                    (+0.0 + 1.0j), (+0.0 - 1.0j), (+0.0 + 1.0j), (+0.0 - 1.0j),
                    (+0.0 + 1.0j), (+0.0 + 1.0j), (+0.0 - 1.0j), (+0.0 - 1.0j) };
    std::reverse(d_sof_taps.begin(), d_sof_taps.end());
    std::reverse(d_plsc_taps.begin(), d_plsc_taps.end());
    assert(d_sof_taps.size() == SOF_CORR_LEN);
    assert(d_plsc_taps.size() == PLSC_CORR_LEN);
    assert(d_plsc_e_buf.length() == d_plsc_taps.size());
    assert(d_plsc_o_buf.length() == d_plsc_taps.size());
}

void frame_sync::correlate(delay_line<gr_complex>& d_line,
                           volk::vector<gr_complex>& taps,
                           gr_complex* res)
{
    volk_32fc_x2_dot_prod_32fc(res, &d_line.back(), taps.data(), d_line.length());
}

bool frame_sync::step(const gr_complex& in)
{
    sym_cnt++;
    /* NOTE: this index resets by the end of the PLHEADER, so it is 1 for the
     * first data symbol after the PLHEADER. */

    /* Once locked, wait to compute the cross-correlation only when the right
     * time comes. Within the 90 symbols prior to that, push some values into
     * buffers. */
    if (locked && (sym_cnt <= (frame_len - 90)))
        return false;

    /* Save raw input symbol into buffer. When we finally find the start
     * of frame, we will have the PLHEADER symbols in this buffer */
    d_plheader_buf.push_front(in);

    /* Differential value */
    const gr_complex diff = conj(in) * last_in;
    last_in = in;

    /* Get the differential value 64 symbol intervals ago. We want to make sure
     * that the SOF correlator and the PLSC correlator peak at the same time, so
     * that their outputs can be summed together and yield an even stronger
     * peak. */
    d_plsc_delay_buf.push(diff);
    const gr_complex& diff_d = d_plsc_delay_buf.front();

    /* Put current diff values on SOF correlator buffer */
    d_sof_buf.push(diff_d);

    /* Push diff into PLS even/odd circular correlator buffer.
     *
     * NOTE: the even and odd buffers are used separately so that we can provide
     * only the starting index to volk and let it operate on all values. Note we
     * couldn't otherwise tell volk to iterate in steps of 2, and that we need
     * to iterate in steps of 2 in order to process the differential PLSC values
     * that are known in advance.
     **/
    if (sym_cnt & 1)
        d_plsc_o_buf.push(diff);
    else
        d_plsc_e_buf.push(diff);

    /* Everything past this point is only necessary exactly when the correlators
     * are expected to peak */
    if (locked && (sym_cnt < frame_len))
        return false;

    /* SOF correlation */
    gr_complex sof_corr;
    correlate(d_sof_buf, d_sof_taps, &sof_corr);

    /* PLSC correlation */
    gr_complex plsc_corr;
    if (sym_cnt & 1)
        correlate(d_plsc_o_buf, d_plsc_taps, &plsc_corr);
    else
        correlate(d_plsc_e_buf, d_plsc_taps, &plsc_corr);

    /* Final timing metric
     *
     * Compute sum and difference between SOF and PLSC correlators. Note the LSB
     * of the TYPE field is what defines the sign of the PLSC correlation. Since
     * we can't know the sign in advance, we test for both by checking which
     * sign leads to the largest sum.
     */
    const float abs_sum = abs(sof_corr + plsc_corr);
    const float abs_diff = abs(sof_corr - plsc_corr);
    timing_metric = (abs_sum > abs_diff) ? abs_sum : abs_diff;

    /* Is this a peak? */
    bool is_peak = locked ? (timing_metric > threshold_l) : (timing_metric > threshold_u);
    /* TODO: check the average symbol magnitude and normalize */

    if (is_peak) {
        /* Frame locked if the peak is where it was expected to be */
        locked = (sym_cnt == frame_len);

        if (debug_level > 0) {
            if (locked && !locked_prev)
                printf("Frame lock acquired\n");
        }
        locked_prev = locked;

        if (debug_level > 1)
            printf("--\nFrame sync: {Peak after: %u, Timing Metric: %f, Locked: %u}\r\n",
                   sym_cnt,
                   timing_metric,
                   locked);

        if (debug_level > 2)
            printf("Frame sync: {Sym: %u, SOF: %+.1f %+.1fj, PLSC: %+.1f %+.1fj}\r\n",
                   sym_cnt,
                   sof_corr.real(),
                   sof_corr.imag(),
                   plsc_corr.real(),
                   plsc_corr.imag());

        if (debug_level > 3) {
            dump_complex_vec(d_sof_buf, "Frame sync: SOF buffer");
            dump_complex_vec(d_plsc_e_buf, "Frame sync: PLSC even buffer");
            dump_complex_vec(d_plsc_o_buf, "Frame sync: PLSC odd buffer");
        }

        sym_cnt = 0;
    } else {
        /* Was a peak expected? */
        if (locked) {
            locked = false;
            if (debug_level > 0) {
                printf("--\nFrame lock lost\n");
                if (debug_level > 1)
                    printf("Insufficient Timing Metric: %f\n", timing_metric);

                if (debug_level > 2)
                    printf("Frame sync: {Sym: %u, SOF: %+.1f %+.1fj, PLSC: %+.1f "
                           "%+.1fj}\r\n",
                           sym_cnt,
                           sof_corr.real(),
                           sof_corr.imag(),
                           plsc_corr.real(),
                           plsc_corr.imag());

                if (debug_level > 3) {
                    dump_complex_vec(d_sof_buf, "Frame sync: SOF buffer");
                    dump_complex_vec(d_plsc_e_buf, "Frame sync: PLSC even buffer");
                    dump_complex_vec(d_plsc_o_buf, "Frame sync: PLSC odd buffer");
                }
            }
        }
    }

    return is_peak;
}

} // namespace dvbs2rx
} // namespace gr
