/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pi2_bpsk.h"
#include "pl_defs.h"
#include "pl_freq_sync.h"
#include "pl_signaling.h"
#include "util.h"
#include <gnuradio/expj.h>
#include <gnuradio/math.h>

namespace gr {
namespace dvbs2rx {

freq_sync::freq_sync(unsigned int period, int debug_level)
    : debug_level(debug_level),
      period(period),
      coarse_foffset(0.0),
      i_frame(0),
      N(PLHEADER_LEN),
      L(PLHEADER_LEN - 1),
      coarse_corrected(false),
      fine_foffset(0.0),
      w_angle_avg(0.0),
      plheader_conj(PLHEADER_LEN * n_plsc_codewords),
      pilot_mod_rm(PLHEADER_LEN),
      pp_plheader(PLHEADER_LEN),
      pilot_corr(L + 1),
      angle_corr(L + 1),
      angle_diff(L),
      w_window_l(PLHEADER_LEN - 1),
      w_window_u(SOF_LEN - 1),
      w_angle_diff(L),
      unmod_pilots(PILOT_BLK_LEN),
      angle_pilot(MAX_PILOT_BLKS + 1),
      angle_diff_f(MAX_PILOT_BLKS)
{
    /* Initialize the vector containing the complex conjugate of all 128
     * possible PLHEADER BPSK symbol sequences */
    plsc_encoder plsc_mapper;
    for (int i = 0; i < n_plsc_codewords; i++) { // codewords
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
        w_window_l[m] =
            3.0 * ((2 * L_l + 1.0) * (2 * L_l + 1.0) - (2 * m + 1.0) * (2 * m + 1.0)) /
            (((2 * L_l + 1.0) * (2 * L_l + 1.0) - 1) * (2 * L_l + 1));
    }
    for (unsigned int m = 0; m < L_u; m++) {
        w_window_u[m] =
            3.0 * ((2 * L_u + 1.0) * (2 * L_u + 1.0) - (2 * m + 1.0) * (2 * m + 1.0)) /
            (((2 * L_u + 1.0) * (2 * L_u + 1.0) - 1) * (2 * L_u + 1));
    }

    /* Initialize the complex conjugate of unmodulated pilots. This is used to
     * "remove" the modulation of pilot blocks. */
    for (int i = 0; i < PILOT_BLK_LEN; i++)
        unmod_pilots[i] = (+SQRT2_2 - SQRT2_2i);
}

bool freq_sync::estimate_coarse(const gr_complex* in, uint8_t i_codeword, bool locked)
{
    /* TODO: we could also average over pilot blocks */
    const float* w_window;
    if (locked) {
        N = PLHEADER_LEN;
        L = PLHEADER_LEN - 1;
        w_window = w_window_l.data();
    } else {
        N = SOF_LEN;
        L = SOF_LEN - 1;
        w_window = w_window_u.data();
    }

    /* "Remove" modulation from pilots to obtain a "CW" signal */
    volk_32fc_x2_multiply_32fc(
        pilot_mod_rm.data(), in, &plheader_conj[i_codeword * PLHEADER_LEN], N);

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

    if (debug_level > 1) {
        printf("Frequency offset estimation:\n");
        if (debug_level > 3) {
            dump_complex_vec(in, N, "In Symbols");
            dump_complex_vec(pilot_mod_rm, N, "Mod rm");
            dump_complex_vec(pilot_corr, L + 1, "Autocorr");
            dump_real_vec(angle_corr, L + 1, "Corr Angle");
            dump_real_vec(angle_diff, L, "Angle diff");
        }
        printf("- Coarse frequency offset: %g\n", coarse_foffset);
    }

    /* Reset autocorrelation accumulator */
    std::fill(pilot_corr.begin(), pilot_corr.end(), 0);

    /* Declare that the frequency offset is coarsely corrected once the residual
     * offset falls within the fine correction range */
    coarse_corrected = (abs(coarse_foffset) < fine_foffset_corr_range);

    return true;
}

float freq_sync::estimate_plheader_phase(const gr_complex* in, uint8_t i_codeword)
{
    /* "Remove" modulation */
    volk_32fc_x2_multiply_32fc(
        pilot_mod_rm.data(), in, &plheader_conj[i_codeword * PLHEADER_LEN], PLHEADER_LEN);

    /* Angle of the sum of the PLHEADER symbols */
    gr_complex ck_sum = 0;
    for (int i = 0; i < PLHEADER_LEN; i++)
        ck_sum += pilot_mod_rm[i];

    angle_pilot[0] = gr::fast_atan2f(ck_sum);

    return angle_pilot[0];
}

float freq_sync::estimate_pilot_phase(const gr_complex* in, int i_blk)
{
    const gr_complex* pilot_sym = &in[PLHEADER_LEN + (i_blk * PILOT_BLK_LEN)];
    /* No need to remove modulation, since pilot blocks are already
     * un-modulated. However, do note that the original pilots have angle pi/4
     * (symbols are +0.707 +j0.707). So, in the end, pi/4 must be subtracted
     * from the resulting average phase. */

    /* Average phase is the angle of the sum of the pilot symbols */
    gr_complex ck_sum = 0;
    for (int i = 0; i < PILOT_BLK_LEN; i++)
        ck_sum += pilot_sym[i];
    float avg_phase = gr::fast_atan2f(ck_sum) - (GR_M_PI / 4.0);

    /* Keep it within -pi to pi */
    if (avg_phase > M_PI)
        avg_phase -= 2 * M_PI;
    else if (avg_phase < -M_PI)
        avg_phase += 2 * M_PI;
    /* TODO find a branchless way of computing this - maybe with fmod */

    angle_pilot[i_blk + 1] = avg_phase;

    return avg_phase;
}

void freq_sync::estimate_fine(const gr_complex* pilots, uint8_t n_pilot_blks)
{
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

    /* Sum of all angles differences */
    float sum_diff;
    volk_32f_accumulator_s32f(&sum_diff, angle_diff_f.data(), n_pilot_blks);

    /* Final estimate */
    fine_foffset = sum_diff / (2.0 * GR_M_PI * PILOT_BLK_PERIOD * n_pilot_blks);

    if (debug_level > 1)
        printf("- Fine frequency offset: %g\n", fine_foffset);

    if (debug_level > 3) {
        dump_complex_vec(
            pilots, PLHEADER_LEN + (n_pilot_blks * PILOT_BLK_LEN), "Rx Pilots");
        dump_real_vec(angle_pilot.data(), n_pilot_blks + 1, "Pilot angles");
    }
}

void freq_sync::derotate_plheader(const gr_complex* in)
{
    /* Frequency:
     *
     * The de-rotation's frequency depends on whether the frequency offset has
     * been coarsely corrected (is sufficiently low). In the positive case, it
     * will be based on the most recent fine frequency offset estimate, if
     * any. When not coarsely corrected, we can still try to de-rotate the
     * PLHEADER symbols here based on the most recent coarse frequency offset
     * estimate.
     *
     * NOTE: this does not conflict with the de-rotation that is applied by an
     * upstream rotator, because the current estimates are only going to be
     * applied to the upstream de-rotator in the next frame, due to how the
     * message port control mechanism works.
     *
     * Caveat: when de-rotating with the fine offset, note it does not
     * necessarily correspond to the estimation based on the previous frame. It
     * depends on whether the previous frame had pilots.
     **/
    const float phase_inc = coarse_corrected ? (2.0 * GR_M_PI * fine_foffset)
                                             : (2.0 * GR_M_PI * coarse_foffset);

    /* Phase:
     *
     * The de-rotation's starting phase depends on the phase of the
     * PLHEADER. Unfortunately, at this point, as we are trying to help with the
     * PLSC decoding, the PLSC has not been decoded yet. So our best bet is to
     * estimate the phase based only on SOF symbols. Note also that we cannot
     * simply rely on the MODCOD info of the previous frame, since VCM could be
     * used and, as a result, the current frame may have a distinct MODCOD.
     */
    volk_32fc_x2_multiply_32fc(pilot_mod_rm.data(), in, plheader_conj.data(), SOF_LEN);
    gr_complex ck_sum = 0;
    for (int i = 0; i < SOF_LEN; i++)
        ck_sum += pilot_mod_rm[i];

    const float plheader_phase = gr::fast_atan2f(ck_sum);

    if (debug_level > 2)
        printf("PLHEADER phase: %g\n", plheader_phase);

    /* Finally, prepare the rotator's phasors */
    gr_complex phasor = gr_expj(-phase_inc);
    gr_complex phasor_0 = gr_expj(-plheader_phase);

    /* De-rotate and save into the post-processed PLHEADER buffer */
    volk_32fc_s32fc_x2_rotator_32fc(
        pp_plheader.data(), in, phasor, &phasor_0, PLHEADER_LEN);
}

} // namespace dvbs2rx
} // namespace gr
