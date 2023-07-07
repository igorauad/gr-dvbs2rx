/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "debug_level.h"
#include "pl_frame_sync.h"
#include "util.h"
#include <boost/format.hpp>
#include <algorithm>
#include <cassert>

namespace gr {
namespace dvbs2rx {

frame_sync::frame_sync(int debug_level, uint8_t unlock_thresh)
    : pl_submodule("frame_sync", debug_level),
      d_unlock_thresh(unlock_thresh),
      d_sym_cnt(0),
      d_last_in(0),
      d_timing_metric(0.0),
      d_sof_interval(0),
      d_state(frame_sync_state_t::searching),
      d_frame_len(0),
      d_unlock_cnt(0),
      d_plsc_delay_buf(PLSC_LEN + 1),
      d_sof_buf(SOF_CORR_LEN),
      d_plsc_e_buf(PLSC_CORR_LEN),
      d_plsc_o_buf(PLSC_CORR_LEN),
      d_plheader_buf(PLHEADER_LEN),
      d_payload_buf(MAX_PLFRAME_PAYLOAD)
{
    /* SOF and PLSC matched filter (correlator) taps: the folded (or reversed)
     * version of the target SOF and PLSC symbols */
    d_sof_taps = { { 0, -1.0 }, { 0, -1.0 }, { 0, -1.0 }, { 0, -1.0 }, { 0, 1.0 },
                   { 0, 1.0 },  { 0, 1.0 },  { 0, 1.0 },  { 0, -1.0 }, { 0, 1.0 },
                   { 0, 1.0 },  { 0, 1.0 },  { 0, -1.0 }, { 0, 1.0 },  { 0, 1.0 },
                   { 0, -1.0 }, { 0, -1.0 }, { 0, 1.0 },  { 0, -1.0 }, { 0, -1.0 },
                   { 0, 1.0 },  { 0, -1.0 }, { 0, 1.0 },  { 0, 1.0 },  { 0, -1.0 } };
    d_plsc_taps = { { 0, -1.0 }, { 0, 1.0 },  { 0, 1.0 },  { 0, -1.0 }, { 0, -1.0 },
                    { 0, -1.0 }, { 0, 1.0 },  { 0, -1.0 }, { 0, -1.0 }, { 0, 1.0 },
                    { 0, 1.0 },  { 0, 1.0 },  { 0, 1.0 },  { 0, 1.0 },  { 0, -1.0 },
                    { 0, -1.0 }, { 0, -1.0 }, { 0, -1.0 }, { 0, 1.0 },  { 0, 1.0 },
                    { 0, -1.0 }, { 0, 1.0 },  { 0, 1.0 },  { 0, -1.0 }, { 0, 1.0 },
                    { 0, -1.0 }, { 0, 1.0 },  { 0, -1.0 }, { 0, 1.0 },  { 0, 1.0 },
                    { 0, -1.0 }, { 0, -1.0 } };
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
    d_sym_cnt++;
    /* NOTE: this index resets by the end of the PLHEADER, so it is 1 for the
     * first data symbol after the PLHEADER (after the above ++ operator). Note
     * d_sym_cnt is incremented here before anything else to make sure it
     * increments even if this call hits one of the early return statements. */

    /* Once a SOF is found, buffer the subsequent symbols until the next
     * SOF. Since the SOF detection happens when the last PLHEADER symbol is
     * processed, and since d_sym_cnt starts at 1 after a timing metric peak,
     * this is equivalent to buffering the payload between consecutive SOFs. */
    const bool locked_or_almost = is_locked_or_almost();
    if (locked_or_almost && (d_sym_cnt <= MAX_PLFRAME_PAYLOAD)) {
        d_payload_buf[d_sym_cnt - 1] = in;
    }

    /* Once locked, wait to compute the next cross-correlation only when the
     * right time comes to find the subsequent PLFRAME. More specifically,
     * within the 90 symbols prior to the next expected frame timing peak, start
     * pushing new values into the cross-correlators. This strategy reduces the
     * computational cost and avoids false-positive SOF detections that could
     * arise in the course of the frame. */
    const bool locked = is_locked();
    if (locked && ((d_sym_cnt + 90) <= d_frame_len)) {
        return false;
    }

    /* Save the raw input symbol into the PLHEADER buffer. When we finally find
     * the start of frame, we will have the PLHEADER symbols in this buffer */
    d_plheader_buf.push_front(in);

    /* Differential value */
    const gr_complex diff = conj(in) * d_last_in;
    d_last_in = in;

    /* Get the differential value 64 symbol intervals ago. We want to make sure
     * that the SOF correlator and the PLSC correlator peak at the same time, so
     * that their outputs can be summed together and yield an even stronger
     * peak. */
    d_plsc_delay_buf.push(diff);
    const gr_complex& diff_d = d_plsc_delay_buf.front();

    /* Push the differential into the SOF correlator buffer */
    d_sof_buf.push(diff_d);

    /* Push the differential into the PLSC correlator buffers.
     *
     * NOTE: the PLSC correlation is based on the 32 differentials due to the
     * pairs of PLSC symbols. At this point, we can't tell whether the pairs
     * start on even d_sym_cnt or odd d_sym_cnt. Hence, we need to try both.
     **/
    if (d_sym_cnt & 1)
        d_plsc_o_buf.push(diff);
    else
        d_plsc_e_buf.push(diff);

    /* Everything past this point is only necessary exactly when the correlators
     * are expected to peak. If a PLFRAME has been found already (i.e., we are
     * locked or almost locked), proceed only if this is the expected timing for
     * the next PLFRAME timing peak. */
    if (locked && (d_sym_cnt < d_frame_len))
        return false;

    /* SOF correlation */
    gr_complex sof_corr;
    correlate(d_sof_buf, d_sof_taps, &sof_corr);

    /* PLSC correlation */
    gr_complex plsc_corr;
    if (d_sym_cnt & 1)
        correlate(d_plsc_o_buf, d_plsc_taps, &plsc_corr);
    else
        correlate(d_plsc_e_buf, d_plsc_taps, &plsc_corr);

    /* Final timing metric
     *
     * Compute the sum and difference between the SOF and PLSC correlators. Note
     * the LSB of the TYPE field is what defines the sign/phase of the PLSC
     * correlation. Since we can't know the sign in advance, we test for both by
     * checking which sign leads to the largest sum.
     */
    const float abs_sum = abs(sof_corr + plsc_corr);
    const float abs_diff = abs(sof_corr - plsc_corr);
    d_timing_metric = (abs_sum > abs_diff) ? abs_sum : abs_diff;
    /* NOTE: the complex version of the timing metric could be used to obtain a
     * rough frequency offset estimate. When the frequency offset is constant
     * over the PLHEADER, all differentials have a factor of exp(-j*2*pi*f0),
     * where f0 is the frequency offset. If we define:
     *
     * complex_timing_metric = (abs_sum > abs_diff) ? (sof_corr + plsc_corr) :
     *                                                (sof_corr - plsc_corr);
     * Then, the frequency offset estimate becomes:
     *
     * freq_offset_estimate = -std::arg(complex_timing_metric) / (2 * M_PI));
     *
     * However, this estimate is poor under strong noise. A more robust approach
     * is to accumulate the energy of multiple PLHEADERs, as done in the
     * `freq_sync` class.
     */

    /* Is this a peak? */
    const bool is_peak =
        locked ? (d_timing_metric > threshold_l) : (d_timing_metric > threshold_u);
    /* TODO: check the average symbol magnitude and normalize */

    /* Is a peak expected? When locked, the program can only hit this point when
     * processing the last PLHEADER symbol, which is when the timing metric peak
     * is expected. Before locking, there are no peak expectations. */
    const bool peak_expected = locked;

    /* Useful log separator */
    GR_LOG_DEBUG_LEVEL_IF(2,
                          (is_peak || peak_expected),
                          "--------------------------------------------------");

    /* State machine */
    if (is_peak) {
        if (d_state == frame_sync_state_t::searching) {
            d_state = frame_sync_state_t::found;
            GR_LOG_DEBUG_LEVEL(1, "PLFRAME found");
        } else if (d_state == frame_sync_state_t::found && d_sym_cnt == d_frame_len) {
            d_state = frame_sync_state_t::locked;
            d_lock_time = std::chrono::system_clock::now();
            GR_LOG_DEBUG_LEVEL(1, "PLFRAME lock acquired");
        }
        d_sof_interval = d_sym_cnt;
        d_unlock_cnt = 0; // reset the unlock count just in case it was non-zero
        GR_LOG_DEBUG_LEVEL(2,
                           "Peak after: {:d}; "
                           "Timing Metric: {:f}; "
                           "Locked: {:d}",
                           d_sof_interval,
                           d_timing_metric,
                           (d_state == frame_sync_state_t::locked));
    } else if (peak_expected) {
        // Unlock only if the timing metric fails to exceed the threshold for
        // `d_unlock_thresh` consecutive frames. It's important to avoid
        // unlocking prematurely when running under high noise.
        d_unlock_cnt++;
        GR_LOG_DEBUG_LEVEL(2,
                           "Insufficient timing metric: {:f} (occurrence {:d}/{:d})",
                           d_timing_metric,
                           d_unlock_cnt,
                           d_unlock_thresh);

        if (d_unlock_cnt == d_unlock_thresh) {
            d_state = frame_sync_state_t::searching;
            d_unlock_cnt = 0;
            GR_LOG_DEBUG_LEVEL(1, "PLFRAME lock lost");
        }
    }

    /* Further debugging logs and symbol count reset */
    if (is_peak || peak_expected) {
        GR_LOG_DEBUG_LEVEL(3,
                           "Sym: {:d}; SOF: {:+.1f} {:+.1f}j; PLSC: {:+.1f} "
                           "%+.1fj",
                           d_sym_cnt,
                           sof_corr.real(),
                           sof_corr.imag(),
                           plsc_corr.real(),
                           plsc_corr.imag());
        d_sym_cnt = 0; // prepare to index the data symbols
    }

    // Return true for both the actual and inferred peaks. The goal is to
    // indicate that this step is processing the last PLHEADER symbol, which is
    // when the timing metric should peak. Hence, even if this particular
    // PLHEADER does not lead to a sufficiently high timing metric, as long as
    // we are still locked (i.e., the current state is either "found" or
    // "locked"), this is the last PLHEADER symbol to the best of our knowledge.
    // Note that the PL Sync block relies on this return value when deciding
    // whether or not to process the PLHEADER. Hence, if we returned only
    // "is_peak", the PLHEADER would be missed whenever the timing metric
    // failed, even if still locked, which is clearly undesirable.
    return (is_peak || peak_expected) && (d_state != frame_sync_state_t::searching);
}

void frame_sync::set_frame_len(uint32_t len)
{
    if (len > MAX_PLFRAME_LEN)
        throw std::runtime_error("Invalid PLFRAME length");
    d_frame_len = len;
}

} // namespace dvbs2rx
} // namespace gr
