/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "debug_level.h"
#include "pi2_bpsk.h"
#include "pl_defs.h"
#include "pl_freq_sync.h"
#include "pl_signaling.h"
#include "util.h"
#include <gnuradio/expj.h>
#include <gnuradio/math.h>
#include <boost/format.hpp>
#include <cassert>

namespace gr {
namespace dvbs2rx {

freq_sync::freq_sync(unsigned int period, int debug_level)
    : pl_submodule("freq_sync", debug_level),
      period(period),
      coarse_foffset(0.0),
      i_frame(0),
      N(PLHEADER_LEN),
      L(PLHEADER_LEN - 1),
      coarse_corrected(false),
      fine_foffset(0.0),
      w_angle_avg(0.0),
      fine_est_ready(false),
      plheader_conj(PLHEADER_LEN * n_plsc_codewords),
      pilot_mod_rm(PLHEADER_LEN),
      pp_sof(SOF_LEN),
      pp_plheader(PLHEADER_LEN),
      pilot_corr(L + 1),
      angle_corr(L + 1),
      angle_diff(L),
      w_window_f(PLHEADER_LEN - 1),
      w_window_s(SOF_LEN - 1),
      w_angle_diff(L),
      unmod_pilots(PILOT_BLK_LEN),
      angle_pilot(MAX_PILOT_BLKS + 1),
      angle_diff_f(MAX_PILOT_BLKS)
{
    /* Initialize the vector containing the complex conjugate of all 128
     * possible PLHEADER BPSK symbol sequences */
    plsc_encoder plsc_mapper;
    for (unsigned int i = 0; i < n_plsc_codewords; i++) { // codewords
        gr_complex* ptr = plheader_conj.data() + (i * PLHEADER_LEN);
        // SOF symbols:
        map_bpsk(sof_big_endian, ptr, SOF_LEN);
        // Scrambled PLSC symbols:
        plsc_mapper.encode(ptr + SOF_LEN, i /* assume i is the PLSC */);
    }

    // Conjugate the entire vector
    for (auto& x : plheader_conj) {
        x = conj(x);
    }

    /* Make sure the preamble correlation buffer is zero-initialized, as it is
     * later used as an accumulator */
    std::fill(pilot_corr.begin(), pilot_corr.end(), 0);

    /* Zero-initialize the first index of the autocorrelation angle buffer - it
     * will need to remain 0 forever */
    angle_corr[0] = 0;

    /* Initialize the weighting function taps: */
    unsigned int L_l = (PLHEADER_LEN - 1);
    unsigned int L_u = (SOF_LEN - 1);
    for (unsigned int m = 0; m < L_l; m++) {
        w_window_f[m] =
            3.0 * ((2 * L_l + 1.0) * (2 * L_l + 1.0) - (2 * m + 1.0) * (2 * m + 1.0)) /
            (((2 * L_l + 1.0) * (2 * L_l + 1.0) - 1) * (2 * L_l + 1));
    }
    for (unsigned int m = 0; m < L_u; m++) {
        w_window_s[m] =
            3.0 * ((2 * L_u + 1.0) * (2 * L_u + 1.0) - (2 * m + 1.0) * (2 * m + 1.0)) /
            (((2 * L_u + 1.0) * (2 * L_u + 1.0) - 1) * (2 * L_u + 1));
    }

    /* Initialize the complex conjugate of unmodulated pilots. This is used to
     * "remove" the modulation of pilot blocks. */
    for (auto& pilot : unmod_pilots)
        pilot = { +SQRT2_2, -SQRT2_2 };
}

bool freq_sync::estimate_coarse(const gr_complex* in, bool full, uint8_t plsc)
{
    /* TODO: we could also average over pilot blocks */
    const float* w_window;
    if (full) {
        N = PLHEADER_LEN;
        L = PLHEADER_LEN - 1;
        w_window = w_window_f.data();
    } else {
        N = SOF_LEN;
        L = SOF_LEN - 1;
        w_window = w_window_s.data();
    }

    /* "Remove" modulation from pilots to obtain a "CW" signal */
    volk_32fc_x2_multiply_32fc(
        pilot_mod_rm.data(), in, &plheader_conj[plsc * PLHEADER_LEN], N);

    /* Auto-correlation of the "modulation-removed" pilot symbols
     *
     * m is the lag of the auto-correlation. Assume it ranges from 1 to L, such
     * that the autocorrelation at lag m=0 is not computed. Moreover, even
     * though we could save the L autocorrelation results on indexes 0 to L-1 of
     * the result vector (pilot_corr), we choose instead to save on indexes 1 to
     * L, while leaving index 0 equal to 0. This will allow us to compute the
     * angle differences in one go (with one diff operator).
     */
    gr_complex r_sum;
    for (unsigned int m = 1; m <= L; m++) {
        volk_32fc_x2_conjugate_dot_prod_32fc(
            &r_sum, pilot_mod_rm.data() + m, pilot_mod_rm.data(), (N - m));
        // Accumulate
        pilot_corr[m] += r_sum;
    }

    /* The autocorrelation values are accumulated over a number of frames. This
     * is meant to increase the energy of the CW signal and allow operation
     * under lower SNR levels. Accordingly, we update the frequency offset
     * estimate only once after every `period` frames. */
    i_frame++;
    if (i_frame < period)
        return false;

    /* Enough frames have been received and accumulated on the
     * autocorrelation. Now finalize the estimation. */
    i_frame = 0;

    /* Compute autocorrelation angles */
    for (unsigned int m = 1; m <= L; m++)
        angle_corr[m] = gr::fast_atan2f(pilot_corr[m]);
    // TODO maybe substitute this with volk_32fc_s32f_atan2_32f

    /* Angle differences
     *
     * From L autocorrelation angles, there are L-1 differences. These are the
     * differences on indexes 1 to L-1. Additionally, there is the first
     * "difference" value (at index 0), which is simply equal to
     * angle_corr[1]. Due to the trick described above (of leaving
     * angle_corr[0]=0), we will also get this after the line that follows:
     */
    volk_32f_x2_subtract_32f(
        angle_diff.data(), angle_corr.data() + 1, angle_corr.data(), L);

    /* Put angle differences within [-pi, pi]
     *
     * NOTE: the problem for this wrapping is when the angle is oscillating near
     * 180 degrees, in which case it oscillates from -pi to pi. In contrast,
     * when the angle is put within [0, 2*pi], the analogous problem is when the
     * angle oscillates near 0 degress, namely between 0 and 2*pi. Since due to
     * the coarse freq. offset recovery the residual fine CFO is expected to be
     * low, we can assume the angle won't be near 180 degrees. Hence, it is
     * better to wrap the angle within [-pi, pi] range.
     */
    for (unsigned int m = 0; m < L; m++) {
        if (angle_diff[m] > M_PI)
            angle_diff[m] -= 2 * M_PI;
        else if (angle_diff[m] < -M_PI)
            angle_diff[m] += 2 * M_PI;
    }
    /* TODO maybe use volk_32f_s32f_s32f_mod_range_32f or fmod */

    /* Weighted average */
    volk_32f_x2_multiply_32f(w_angle_diff.data(), angle_diff.data(), w_window, L);

    /* Sum of weighted average */
    volk_32f_accumulator_s32f(&w_angle_avg, w_angle_diff.data(), L);

    /* Final freq offset estimate
     *
     * Due to angle in range [-pi,pi], the freq. offset lies within
     * [-0.5,0.5]. Enforce that to avoid numerical problems.
     */
    coarse_foffset = branchless_clip(w_angle_avg / (2 * M_PI), 0.5f);

    /* Declare that the frequency offset is coarsely corrected once the residual
     * offset falls within the fine correction range */
    coarse_corrected = abs(coarse_foffset) < fine_foffset_corr_range;

    GR_LOG_DEBUG_LEVEL(2, "Frequency offset estimation:");
    GR_LOG_DEBUG_LEVEL(2, "- Coarse frequency offset: {:g}", coarse_foffset);
    GR_LOG_DEBUG_LEVEL(2, "- Coarse corrected: {:d}", coarse_corrected);

    /* Reset autocorrelation accumulator */
    std::fill(pilot_corr.begin(), pilot_corr.end(), 0);

    return true;
}

float freq_sync::estimate_phase_data_aided(const gr_complex* in,
                                           const gr_complex* expected,
                                           unsigned int len)
{
    // Remove the modulation to obtain a noisy CW. At this point, the CW should
    // be barely rotating if the residual frequency offset is low enough.
    volk_32fc_x2_multiply_32fc(pilot_mod_rm.data(), in, expected, len);

    // Return the average angle of the modulation-removed CW symbols
    gr_complex ck_sum = 0;
    for (unsigned int i = 0; i < len; i++)
        ck_sum += pilot_mod_rm[i];

    return gr::fast_atan2f(ck_sum);
}

float freq_sync::estimate_sof_phase(const gr_complex* in)
{
    return estimate_phase_data_aided(in, plheader_conj.data(), SOF_LEN);
}

float freq_sync::estimate_plheader_phase(const gr_complex* in, uint8_t plsc)
{
    return estimate_phase_data_aided(
        in, &plheader_conj[plsc * PLHEADER_LEN], PLHEADER_LEN);
}

float freq_sync::estimate_pilot_phase(const gr_complex* in, int i_blk)
{
    // Validate the pilot block index
    assert(i_blk >= 0 && i_blk < MAX_PILOT_BLKS);

    /* NOTE: Unlike the PLHEADER symbols, there is no need to remove the
     * modulation from the pilot symbols, since the pilot blocks are already
     * un-modulated. However, do note that the original pilots have angle pi/4
     * (symbols are +0.707 +j0.707). So, in the end, pi/4 must be subtracted
     * from the resulting average phase. */

    /* Average phase is the angle of the sum of the pilot symbols */
    gr_complex ck_sum = 0;
    for (int i = 0; i < PILOT_BLK_LEN; i++)
        ck_sum += in[i];
    float avg_phase = gr::fast_atan2f(ck_sum) - (GR_M_PI / 4.0);

    /* Keep it within -pi to pi */
    if (avg_phase > M_PI)
        avg_phase -= 2 * M_PI;
    else if (avg_phase < -M_PI)
        avg_phase += 2 * M_PI;
    /* TODO find a branchless way of computing this - maybe with fmod */

    return avg_phase;
}

void freq_sync::estimate_fine_pilot_mode(const gr_complex* p_plheader,
                                         const gr_complex* p_payload,
                                         uint8_t n_pilot_blks,
                                         uint8_t plsc)
{
    // Fill in the average phase of the PLHEADER. Consider the last 36 symbols of the
    // PLHEADER only so that all phase estimates (PLHEADER and pilots) are based on the
    // same sequence length (36 symbols), and spaced by an equal interval (1476 symbols).
    angle_pilot[0] = estimate_phase_data_aided(
        p_plheader + (PLHEADER_LEN - PILOT_BLK_LEN),
        &plheader_conj[plsc * PLHEADER_LEN] + (PLHEADER_LEN - PILOT_BLK_LEN),
        PILOT_BLK_LEN);

    // Fill in the average phase of the descrambled pilot blocks
    for (int i = 0; i < n_pilot_blks; i++) {
        const gr_complex* p_pilots =
            p_payload + ((i + 1) * PILOT_BLK_PERIOD) - PILOT_BLK_LEN;
        angle_pilot[i + 1] = estimate_pilot_phase(p_pilots, i);
    }

    /* Angle differences */
    volk_32f_x2_subtract_32f(
        angle_diff_f.data(), angle_pilot.data() + 1, angle_pilot.data(), n_pilot_blks);

    /* Put angle differences within [-pi, pi] */
    for (int i = 0; i < n_pilot_blks; i++) {
        if (angle_diff_f[i] > M_PI)
            angle_diff_f[i] -= 2.0 * GR_M_PI;
        else if (angle_diff_f[i] < -M_PI)
            angle_diff_f[i] += 2.0 * GR_M_PI;
    }

    /* Sum of the angle differences between pilot blocks */
    float sum_diff;
    volk_32f_accumulator_s32f(&sum_diff, angle_diff_f.data(), n_pilot_blks);

    /* Final estimate
     *
     * The phase difference between two pilot blocks accumulates over PILOT_BLK_PERIOD,
     * namely over 1476 symbols. Each phase difference divided by (2*pi*interval) gives
     * the corresponding frequency offset estimate over that interval. In total, there are
     * n_pilot_blks estimates. The arithmetic average of them is computed by summing each
     * estimate weighted by a factor of "1 / n_pilot_blks".
     **/
    fine_foffset = sum_diff / (2.0 * GR_M_PI * PILOT_BLK_PERIOD * n_pilot_blks);
    fine_est_ready = true;

    GR_LOG_DEBUG_LEVEL(2, "Fine frequency offset: {:g}", fine_foffset);
}

bool freq_sync::estimate_fine_pilotless_mode(float curr_plheader_phase,
                                             float next_plheader_phase,
                                             uint16_t curr_plframe_len,
                                             double curr_coarse_foffset)
{
    // The pilotless frequency offset estimator is based on the phase change accumulated
    // from PLHEADER to PLHEADER. If the magnitude of this phase variation exceeds pi, the
    // measurement becomes untrustworthy, as clarified in the sequel. In this case, it is
    // better not to proceed with the estimation unless the residual frequency offset read
    // by the coarse estimator is within an acceptable range.
    //
    // Unlike the pilot-mode estimator, the acceptable frequency offset range varies here
    // depending on the PLFRAME length. For the pilot-mode estimator, the estimate comes
    // from the phase accumulated from pilot to pilot, whereas, here, it comes from the
    // phase accrued from PLHEADER to PLHEADER. The latter is a longer interval, which
    // depends on the PLFRAME length. Hence, the observable frequency offset range in
    // pilotless mode is always narrower than in pilot mode. Secondly, the range is
    // dynamic, since the PLFRAME length could be changing (e.g., in ACM/VCM). Hence,
    // instead of using the hard-coded limit from `fine_foffset_corr_range`, it is
    // necessary to recompute the maximum observable frequency offset every time.
    double max_foffset = 1.0 / (2 * curr_plframe_len);
    if (abs(curr_coarse_foffset) > max_foffset)
        return false;

    double delta_phase = next_plheader_phase - curr_plheader_phase;
    // The limit imposed by max_foffset means the phase change accumulated from PLHEADER
    // to PLHEADER should not exceed +-pi. If `delta_phase` does exceed +-pi, it's
    // probably due to the rotation direction. For example, if the current phase is -90°
    // and the next phase is 150°, the phase difference could either be 240° if rotating
    // counterclockwise (positive frequency offset) or -120° if rotating clockwise
    // (negative frequency offset). Since the former exceeds 180°, the more appropriate
    // answer is the 120° phase shift clockwise, corresponding to a negative frequency
    // offset. The following operation ensures the phase difference lies within +-pi.
    if (delta_phase > M_PI)
        delta_phase -= 2.0 * GR_M_PI;
    else if (delta_phase < -M_PI)
        delta_phase += 2.0 * GR_M_PI;

    fine_foffset = delta_phase / (2.0 * GR_M_PI * curr_plframe_len);
    fine_est_ready = true;

    GR_LOG_DEBUG_LEVEL(2, "- Fine frequency offset: {:g}", fine_foffset);

    return true;
}

void freq_sync::derotate_plheader(const gr_complex* in, bool open_loop)
{
    if (open_loop) {
        /* Frequency correction (open-loop mode only)
         *
         * The frequency correction value depends on whether the frequency offset is
         * within the fine estimation range, as indicated by the coarse frequency offset
         * estimate (more specifically, by the `coarse_corrected` state). In the positive
         * case, it will be based on the most recent fine frequency offset estimate, if
         * any. Otherwise, the correction will be based on the most recent coarse
         * frequency offset estimate.
         *
         * NOTE 1: if coarse_corrected is true, that does not imply a fine frequency
         * offset estimate is available already. Check both.
         *
         * NOTE 2: when de-rotating with the fine offset, note it does not necessarily
         * correspond to the estimation based on the previous frame. It depends on whether
         * the previous frame had pilots.
         *
         * NOTE 3: this frequency correction step is only applied in open-loop mode. In
         * closed-loop, when an external block already handles the frequency corrections,
         * it would lead to undesirable behavior.
         *
         * In closed-loop, when the coarse frequency offset estimation period is
         * non-unitary, the problem is that the derotation is not required in all frames.
         * For example, when the estimation period is 2, one frame leads to a new
         * estimate, while the other receives the correction due to the preceding
         * estimate, according to the architecture adopted by the PL Sync block. For
         * instance, suppose four consecutive frames, [F0, F1, F2, F3]. After F1, a new
         * coarse frequency offset estimate is produced and scheduled for correction at
         * the start of F2. Hence, assuming an ideal estimate and correction, frame F2 no
         * longer experiences the frequency offset estimated on F1. However, when
         * processing F2, the most recent coarse frequency offset estimate is still that
         * of F1, so the derotation would be based on F1, which would be clearly wrong. To
         * avoid this, the de-rotation should only be applied when a new coarse frequency
         * offset estimate is produced. In the example, it would be applied at frames F1
         * and F3 only, but not on F0 and F2.
         *
         * To complicate things further, the fine offset estimations and corrections apply
         * on different frames (with a different delay). For example, assume the same
         * sequence of four frames, that the synchronizer is already in coarse-corrected
         * state, and that all frames contain pilot blocks. In this case, frame F0 leads
         * to a new fine offset estimate, but which is only applied at the start of frame
         * F2 (two frames later). When the PLHEADER of F2 is processed, the most recent
         * fine offset estimate will be that due to F1, but F2 receives the frequency
         * correction due to the F0 estimate, so the derotation due to the F1 estimate
         * does not make sense. In this case, the appropriate correction value would be
         * "f_F1 - f_F0", i.e., the difference between the estimate due to F1 and the
         * estimate due to F0.
         *
         * In both cases, further logic would be required to decide whether or not to
         * apply derotation here in closed-loop mode, or to decide which frequency
         * correction to apply. To avoid the extra complexity, we assume the benefit from
         * this derotation is negligible in closed-loop, assuming the frequency correction
         * eventually converges to an accurate value.
         **/
        const float phase_inc = (coarse_corrected && fine_est_ready)
                                    ? (2.0 * GR_M_PI * fine_foffset)
                                    : (2.0 * GR_M_PI * coarse_foffset);
        gr_complex phasor = gr_expj(-phase_inc);
        gr_complex phasor_0 = gr_expj(0);

        // De-rotate and save into the post-processed PLHEADER buffer
        volk_32fc_s32fc_x2_rotator_32fc(
            pp_plheader.data(), in, phasor, &phasor_0, PLHEADER_LEN);
    }

    /* Phase correction:
     *
     * This function is designed to derotate a PLHEADER before the PLSC decoding, meaning
     * that, at this point, the PLSC has not been decoded yet. Hence, our best bet is to
     * estimate the phase based only on the SOF symbols, which are known a priori.
     * Besides, note that we cannot simply rely on the MODCOD info of the previous frame,
     * since VCM could be used and, as a result, the current frame may have a distinct
     * MODCOD. Also, when the PLSC is known a priori (when we are able to estimate the
     * full PLHEADER phase), this function is not called at all, so it does not make sense
     * to consider this scenario here.
     */
    const gr_complex* p_plheader = (open_loop) ? pp_plheader.data() : in;
    const float plheader_phase = estimate_sof_phase(p_plheader);

    GR_LOG_DEBUG_LEVEL(3, "PLHEADER phase: {:g}", plheader_phase);

    gr_complex phase_correction = gr_expj(-plheader_phase);
    volk_32fc_s32fc_multiply_32fc(
        pp_plheader.data(), p_plheader, phase_correction, PLHEADER_LEN);
}

} // namespace dvbs2rx
} // namespace gr
