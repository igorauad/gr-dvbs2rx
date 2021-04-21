/* -*- c++ -*- */
/*
 * Copyright 2019-2021 Igor Freire.
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
    /* SOF and PLSC correlator taps */
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
    assert(d_sof_taps.size() == SOF_CORR_LEN);
    assert(d_plsc_taps.size() == PLSC_CORR_LEN);
    assert(d_plsc_e_buf.get_length() == d_plsc_taps.size());
    assert(d_plsc_o_buf.get_length() == d_plsc_taps.size());
}

void frame_sync::correlate(buffer* buf, gr_complex* taps, gr_complex* res)
{
    volk_32fc_x2_dot_prod_32fc(res, buf->get_tail(), taps, buf->get_length());
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
    d_plheader_buf.push(in);

    /* Differential value */
    const gr_complex diff = conj(in) * last_in;
    last_in = in;

    /* Get the differential value 64 symbol intervals ago. We want to make sure
     * that the SOF correlator and the PLSC correlator peak at the same time, so
     * that their outputs can be summed together and yield an even stronger
     * peak. */
    d_plsc_delay_buf.push(diff);
    const gr_complex diff_d = *d_plsc_delay_buf.get_tail();

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
    correlate(&d_sof_buf, d_sof_taps.data(), &sof_corr);

    /* PLSC correlation */
    gr_complex plsc_corr;
    if (sym_cnt & 1)
        correlate(&d_plsc_o_buf, d_plsc_taps.data(), &plsc_corr);
    else
        correlate(&d_plsc_e_buf, d_plsc_taps.data(), &plsc_corr);

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
            dump_complex_vec(
                d_sof_buf.get_tail(), d_sof_buf.get_length(), "Frame sync: SOF buffer");
            dump_complex_vec(d_plsc_e_buf.get_tail(),
                             d_plsc_e_buf.get_length(),
                             "Frame sync: PLSC even buffer");
            dump_complex_vec(d_plsc_o_buf.get_tail(),
                             d_plsc_o_buf.get_length(),
                             "Frame sync: PLSC odd buffer");
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
                    dump_complex_vec(d_sof_buf.get_tail(),
                                     d_sof_buf.get_length(),
                                     "Frame sync: SOF buffer");
                    dump_complex_vec(d_plsc_e_buf.get_tail(),
                                     d_plsc_e_buf.get_length(),
                                     "Frame sync: PLSC even buffer");
                    dump_complex_vec(d_plsc_o_buf.get_tail(),
                                     d_plsc_o_buf.get_length(),
                                     "Frame sync: PLSC odd buffer");
                }
            }
        }
    }

    return is_peak;
}

freq_sync::freq_sync(unsigned int period, int debug_level, const uint64_t* codewords)
    : debug_level(debug_level),
      period(period),
      coarse_foffset(0.0),
      i_frame(0),
      N(PLHEADER_LEN),
      L(PLHEADER_LEN - 1),
      coarse_corrected(false),
      fine_foffset(0.0),
      w_angle_avg(0.0),
      plheader_conj(PLHEADER_LEN * n_codewords),
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
    gr_complex conj_mod_sof[SOF_LEN] = {
        (+0.7071068 - 0.7071068j), (+0.7071068 + 0.7071068j), (-0.7071068 + 0.7071068j),
        (-0.7071068 - 0.7071068j), (+0.7071068 - 0.7071068j), (-0.7071068 - 0.7071068j),
        (-0.7071068 + 0.7071068j), (+0.7071068 + 0.7071068j), (+0.7071068 - 0.7071068j),
        (+0.7071068 + 0.7071068j), (+0.7071068 - 0.7071068j), (-0.7071068 - 0.7071068j),
        (-0.7071068 + 0.7071068j), (-0.7071068 - 0.7071068j), (-0.7071068 + 0.7071068j),
        (+0.7071068 + 0.7071068j), (-0.7071068 + 0.7071068j), (-0.7071068 - 0.7071068j),
        (-0.7071068 + 0.7071068j), (-0.7071068 - 0.7071068j), (+0.7071068 - 0.7071068j),
        (-0.7071068 - 0.7071068j), (+0.7071068 - 0.7071068j), (-0.7071068 - 0.7071068j),
        (-0.7071068 + 0.7071068j), (-0.7071068 - 0.7071068j)
    };

    for (int i = 0; i < n_codewords; i++) { // codewords
        /* Conjugate of the SOF symbols */
        for (int j = 0; j < SOF_LEN; j++) { // symbol
            plheader_conj[(i * PLHEADER_LEN) + j] = conj_mod_sof[j];
        }

        /* Conjugate of the scrambled PLSC codeword symbols */
        for (int j = 0; j < PLSC_LEN; j++) { // symbol
            /* scramble first */
            uint64_t scrambled_codeword = codewords[i] ^ plsc_scrambler;

            /* NOTE: even/oddness below is according to the standard, which
             * starts from 1, rather than 0. Hence, it is the opposite of the
             * even/oddness of index j. */
            gr_complex mod_sym;
            if (scrambled_codeword >> (63 - j) & 1) {
                /*
                 * Bit = 1
                 * Odd index:  (-0.707 -0.707i)
                 * Even index: (+0.707 -0.707i)
                 */
                if (j & 1) // even index
                    mod_sym = (+0.7071068 - 0.7071068j);
                else // odd index
                    mod_sym = (-0.7071068 - 0.7071068j);

            } else {
                /*
                 * Bit = 0
                 * Odd index:  (+0.707 +0.707i)
                 * Even index: (-0.707 +0.707i)
                 */
                if (j & 1) // even index
                    mod_sym = (-0.7071068 + 0.7071068j);
                else // odd index
                    mod_sym = (+0.7071068 + 0.7071068j);
            }

            // Save the conjugate of that
            plheader_conj[(i * PLHEADER_LEN) + SOF_LEN + j] = conj(mod_sym);
        }
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
        unmod_pilots[i] = (+0.7071068 - 0.7071068j);
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

plsc_decoder::plsc_decoder(int debug_level)
    : debug_level(debug_level),
      dec_plsc(0),
      modcod(0),
      short_fecframe(false),
      has_pilots(false),
      dummy_frame(false),
      n_mod(0),
      S(0),
      plframe_len(0),
      n_pilots(0)
{
    /* Generator matrix */
    uint32_t G[6] = { 0x55555555, 0x33333333, 0x0f0f0f0f,
                      0x00ff00ff, 0x0000ffff, 0xffffffff };
    /* Pre-compute codewords */
    /* TODO: add an optional MODCOD filter and compute codewords only for the
     * acceptable MODCODs when provided. This way, the PLSC decoding can
     * leverage on prior knowledge about the signal and become more robust */
    for (int index = 0; index < n_codewords; index++) {
        /* Each 32-bit codeword is a linear combination (modulo 2) of the rows
         * of G. Note The most significant bit of the MODCOD is multiplied with
         * the first row of G. */
        uint32_t code32 = 0;
        for (int row = 0; row < 6; row++) {
            if ((index >> (6 - row)) & 1)
                code32 ^= G[row];
        }

        /* Now form the interleaved 64-bit codeword */
        uint64_t code64 = 0;
        for (int bit = 31; bit >= 0; bit--) {
            int yi = (code32 >> bit) & 1;
            /* At odd indexes, the TYPE LSB is 1, hence the sequence must be (y1
             * !y1 y2 !y2 ... y32 !y32). Otherwise, the sequence is (y1 y1 y2 y2
             * ... y32 y32) */
            if (index & 1)
                code64 = (code64 << 2) | (yi << 1) | (yi ^ 1);
            else
                code64 = (code64 << 2) | (yi << 1) | yi;
        }

        codewords[index] = code64;
    }
}

uint64_t plsc_decoder::demap_bpsk(const gr_complex* in)
{
    /*
     * The standard mapping is as follows:
     *
     * Odd indexes:
     *
     * (0.707 +0.707i) if bit = 0
     * (-0.707 -0.707i) if bit = 1
     *
     * Even indexes:
     *
     * (-0.707 +0.707i) if bit = 0
     * (0.707 -0.707i) if bit = 1
     *
     * If we rotate symbols by -pi/4, the mapping becomes:
     *
     * Odd indexes:
     *
     * +1 if bit = 0
     * -1 if bit = 1
     *
     * Even indexes:
     *
     * +j if bit = 0
     * -j if bit = 1
     *
     */
    gr_complex rot_sym;
    const gr_complex rotation = gr_expj(-GR_M_PI / 4);
    int j;
    uint8_t bit;
    uint64_t scrambled_plsc = 0;

    if (debug_level > 4)
        printf("%s: pi/2 BPSK syms [", __func__);

    for (int i = 26; i < 90; i++) {
        j = (i - 26); // 0, 1, 2 ... 63
        rot_sym = in[i] * rotation;

        /* The PLSC here starts at index 26 and ends at 89. In the standard, in
         * contrast, it starts at 27 and ends at 90. */
        if (i & 1) // odd here, even for the standard
            bit = (rot_sym.imag() < 0);
        else // even here, odd for the standard
            bit = (rot_sym.real() < 0);

        if (debug_level > 4)
            printf("%+.2f %+.2fi, ", rot_sym.real(), rot_sym.imag());

        if (bit)
            scrambled_plsc |= (uint64_t)(1LU << (63 - j));
    }

    if (debug_level > 4) {
        printf("]\n");
        printf("%s: scrambled PLSC: 0x%016lX\n", __func__, scrambled_plsc);
    }

    return scrambled_plsc;
}

uint64_t plsc_decoder::demap_bpsk_diff(const gr_complex* in)
{
    /*
     * Here the pi/2 BPSK demodulation is done differentially. This is robust in
     * the presence of frequency offset.
     *
     * In order to decode pi/2 BPSK differentially, we need to know all possible
     * results for "conj(in[i]) * in[i-1]", where in[i] is the i-th IQ symbol of
     * the PLHEADER.
     *
     * Section 5.2.2 of the standard defines the pi/2 BPDK constellation mapping
     * based on indexes from 1 to 90. So, for example, the first symbol of the
     * PLSC has index 27. Based on the standard constellation mapping, it turns
     * out that:
     *
     *   - On an odd to even index transition (like 27 to 28),
     *     conj(y(2i))*y(2i-1) can result in either -j or +j, depending on the
     *     bit sequence, as follows:
     *
     *     00 -> -j
     *     01 -> +j
     *     10 -> +j
     *     11 -> -j
     *
     *   - On an even to odd index transition (like 28 to 29), in turn,
     *     conj(y(2i+1))*y(2i) yields:
     *
     *     00 -> +j
     *     01 -> -j
     *     10 -> -j
     *     11 -> +j
     *
     * Thus, to decode differentially, we always need to check: 1) the current
     * index (whether even or odd); 2) the previous bit; 3) whether the current
     * differential is +j or -j.
     *
     * Also, we can only decode the exact bits because we also know the last bit
     * of the SOF, which is 0. The first differential we take into account is
     * from the last SOF bit to the first PLSC bit. This first transition is an
     * "even to odd" transition, from bit 26 (last SOF bit) to bit 27 (first
     * PLSC bit).
     */
    gr_complex diff;
    int j;
    uint8_t bit = 0; // last bit of SOF is 0
    uint64_t scrambled_plsc = 0;

    if (debug_level > 4)
        printf("%s: differentials [", __func__);

    for (int i = 26; i < 90; i++) {
        j = (i - 26); // 0, 1, 2 ... 63
        diff = conj(in[i]) * in[i - 1];

        /* Our index here ranges from 0 to 89, hence it is not the same as in
         * the standard. The even/oddness according to the standard is,
         * therefore, the opposite of the even/oddness that we find here. The
         * labels below refer to the standard and match with the tables given
         * above: */
        if (i & 1)
            bit = (diff.imag() > 0) ? !bit : bit; // odd to even
        else
            bit = (diff.imag() > 0) ? bit : !bit; // even to odd

        if (debug_level > 4)
            printf("%+.2f %+.2fi, ", diff.real(), diff.imag());

        if (bit)
            scrambled_plsc |= (uint64_t)(1LU << (63 - j));
    }

    if (debug_level > 4) {
        printf("]\n");
        printf("%s: scrambled PLSC: 0x%016lX\n", __func__, scrambled_plsc);
    }

    return scrambled_plsc;
}

void plsc_decoder::decode(const gr_complex* in, bool coherent)
{
    /* First demap the pi/2 BPSK PLSC */
    uint64_t rx_scrambled_plsc;
    if (coherent)
        rx_scrambled_plsc = demap_bpsk(in);
    else
        rx_scrambled_plsc = demap_bpsk_diff(in);

    /* Descramble */
    uint64_t rx_plsc = rx_scrambled_plsc ^ plsc_scrambler;

    if (debug_level > 4)
        printf("%s: descrambled PLSC: 0x%016lX\n", __func__, rx_plsc);

    /* ML decoding : find codeword with lowest Hamming distance */
    uint64_t distance;
    uint64_t min_distance = 64;
    for (int i = 0; i < n_codewords; i++) {
        /* Hamming distance to the i-th possible codeword */
        volk_64u_popcnt(&distance, rx_plsc ^ codewords[i]);
        if (distance < min_distance) {
            min_distance = distance;
            dec_plsc = i;
        }
    }

    /* Parse the decoded PLSC */
    modcod = dec_plsc >> 2;
    short_fecframe = dec_plsc & 0x2;
    has_pilots = dec_plsc & 0x1;
    dummy_frame = modcod == 0;
    has_pilots &= !dummy_frame; // a dummy frame cannot have pilots

    /* Number of bits per constellation symbol and PLFRAME slots */
    if (modcod >= 1 && modcod <= 11) {
        n_mod = 2;
        S = 360;
    } else if (modcod >= 12 && modcod <= 17) {
        n_mod = 3;
        S = 240;
    } else if (modcod >= 18 && modcod <= 23) {
        n_mod = 4;
        S = 180;
    } else if (modcod >= 24 && modcod <= 28) {
        n_mod = 5;
        S = 144;
    } else {
        n_mod = 0;
        S = 36; // dummy frame
    }

    /* For short FECFRAMEs, S is 4 times lower */
    if (short_fecframe && !dummy_frame)
        S >>= 2;

    /* Number of pilot blocks */
    n_pilots = (has_pilots) ? ((S - 1) >> 4) : 0;

    /* PLFRAME length including header */
    plframe_len = ((S + 1) * 90) + (36 * n_pilots);

    if (debug_level > 0) {
        printf(
            "Decoded PLSC: {Index: %3u, MODCOD: %2u, Short FECFRAME: %1u, Pilots: %1u}\n",
            dec_plsc,
            modcod,
            short_fecframe,
            has_pilots);

        if (debug_level > 1)
            printf("Decoded PLSC: {hammin dist: %2lu, n_mod: %1u, S: %3u, PLFRAME "
                   "length: %u}\n",
                   min_distance,
                   n_mod,
                   S,
                   plframe_len);
    }
}

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
      d_da_phase(0.0),
      d_tag_delay(0)
{
    d_frame_sync = new frame_sync(debug_level);
    d_plsc_decoder = new plsc_decoder(debug_level);
    d_freq_sync =
        new freq_sync(freq_est_period, debug_level, d_plsc_decoder->get_codewords());
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
            d_plsc_decoder->decode(p_pp_plheader, coherent_demap);

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

            /* Calibrate the delay between rotator and this block
             *
             * The rotator places a tag on the sample where the new phase
             * increment starts to take effect. This sample typically traverses
             * a matched filter and decimator block, which has an associated
             * delay. We can calibrate this delay here by comparing the sample
             * on which the tag came with respect to where we expected it (in
             * the first symbol of the PLHEADER). Then we use this inferred tag
             * delay when scheduling the next increment update below.
             **/
            std::vector<tag_t> tags;
            const uint64_t n_read = nitems_read(0);
            const uint64_t end_range = n_read + (uint64_t)(noutput_items);

            get_tags_in_range(tags, 0, n_read, end_range, pmt::mp("rot_phase_inc"));

            for (unsigned j = 0; j < tags.size(); j++) {
                int tag_offset = tags[j].offset - n_read;
                int expected_offset = i - 89; // first PLHEADER symbol
                d_tag_delay += expected_offset - tag_offset;

                /* This tag confirms the frequency that is configured in the
                 * upstream rotator right now. */
                d_rot_freq[1] = -d_sps * pmt::to_double(tags[j].value) / (2.0 * GR_M_PI);

                if (abs(d_tag_delay) > 200) // sanity check
                    printf("%s: Warning: tag delay %d seems too high\n",
                           __func__,
                           d_tag_delay);

                if (d_debug_level > 2) {
                    printf("Rotator phase inc tag: %f\t"
                           "Offset: %4d (%lu)\t"
                           "Expected: %4d\t"
                           "Error: %3d\tDelay: %3d\n",
                           pmt::to_float(tags[j].value),
                           tag_offset,
                           tag_offset + n_read,
                           expected_offset,
                           (expected_offset - tag_offset),
                           d_tag_delay);
                }
            }

            /* Send new frequency correction to upstream rotator
             *
             * Control the phase increment of an external de-rotator. Assume the
             * de-rotator sits before a matched filter and, hence, operates on a
             * sample stream (i.e. on samples, rather than symbols). Use the
             * known oversampling ratio in order to schedule the sample index
             * where the update is to be executed by the rotator.
             *
             * Only send such control messages when locked. Before that, the
             * frequency offset estimates can be very poor and are only supposed
             * to be used internally.
             *
             * Extra caution is required when adding the frequency offset
             * estimate to the rotator's frequency. Assuming we are now
             * processing the i-th frame, note the fine estimate is based on
             * frame i-1 and is only effectively corrected by the upstream
             * rotator on the start of frame i+1. Hence, the fine estimate that
             * was just obtained must be accumulated to the frequency that still
             * existed in the rotator during frame i-1.
             *
             * To accomplish this, we track 3 stages of the rotator's
             * frequency. During the previous frame, the current and the one
             * expected for the next frame.
             */
            double rot_freq_adj = 0; // frequency adjustment
            if (d_locked && coarse_corrected && new_fine_est)
                rot_freq_adj = d_freq_sync->get_fine_foffset();
            else if (d_locked && !coarse_corrected && new_coarse_est)
                rot_freq_adj = d_freq_sync->get_coarse_foffset();

            if (rot_freq_adj != 0) {
                /* Rotator frequency that is supposed to start taking
                 * effect on the next frame: */
                d_rot_freq[2] = d_rot_freq[0] + rot_freq_adj;

                if (d_debug_level > 1)
                    printf("- Cumulative frequency offset: %g "
                           "(coarse corrected? %u)\n",
                           d_rot_freq[2],
                           coarse_corrected);

                double phase_inc = -d_rot_freq[2] * 2.0 * GR_M_PI / d_sps;
                uint64_t offset =
                    d_sps * (n_read + i - 89 + d_tag_delay + d_plsc_decoder->plframe_len);

                pmt::pmt_t msg = pmt::make_dict();
                msg = pmt::dict_add(msg, pmt::intern("inc"), pmt::from_double(phase_inc));
                msg = pmt::dict_add(msg, pmt::intern("offset"), pmt::from_uint64(offset));
                message_port_pub(d_port_id, msg);

                if (d_debug_level > 2) {
                    printf("Send new phase inc to rotator: %f -"
                           " offset: %lu\n",
                           phase_inc,
                           offset);
                }
            }

            /* Update rotator's state for next frame */
            d_rot_freq[0] = d_rot_freq[1];
            /* NOTE: don't shift [2] into [1]. d_rot_freq[2] is really just an
             * expectation for what the next rotator's frequency will be. But we
             * must receive confirmation from the rotator that the new increment
             * was applied. If there is no rotator upstream, d_rot_freq[0] will
             * be 0 forever. */

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
