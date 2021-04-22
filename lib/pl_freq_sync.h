/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_PL_FREQ_SYNC_H
#define INCLUDED_DVBS2RX_PL_FREQ_SYNC_H

#include <gnuradio/gr_complex.h>
#include <volk/volk_alloc.hh>

const double fine_foffset_corr_range = 3.3e-4;
/* The normalized frequency offset that the fine frequency
 * estimator can "observe" is upper limited to:
 *
 * 1/(2*(1440 + 36)) = 3.3e-4
 *
 * When the frequency offset is higher than this, the phase changes by more than
 * 2*pi from pilot segment to pilot segment. Consequently, the fine estimation
 * approach does not work.
 */

namespace gr {
namespace dvbs2rx {

class freq_sync
{
private:
    /* Parameters */
    int debug_level;     /** debug level */
    unsigned int period; /** estimation periodicity in frames */

    /* Coarse frequency offset estimation state */
    double coarse_foffset; /** most recent freq. offset estimate */
    unsigned int i_frame;  /** frame counter */
    unsigned int N;        /** "preamble" length */
    unsigned int L;        /** used phase differentials (<= N) */
    bool coarse_corrected; /** residual offset is sufficiently low */

    /* NOTE: In principle, we could make N equal to the SOF length (26) and L =
     * N-1 (i.e. 25), in which case coarse frequency offset estimation would be
     * based on the SOF symbols only and would not require decoding of the
     * PLSC. However, this would waste all the other 64 known PLHEADER symbols,
     * which can improve coarse estimation performance substantially. So N in
     * the end will be set as 90 and L to 89. Nonetheless, the variables are
     * kept here for flexibility on experiments. */

    /* Fine frequency offset estimation state */
    double fine_foffset;
    float w_angle_avg; /** weighted angle average */

    /* Volk buffers */
    volk::vector<gr_complex> plheader_conj; /** conjugate of PLHEADER symbols */
    volk::vector<gr_complex> pilot_mod_rm;  /** modulation-removed received pilots */
    volk::vector<gr_complex> pp_plheader;   /** derotated PLHEADER symbols */

    /* Coarse estimation only */
    volk::vector<gr_complex> pilot_corr;   /** mod-removed autocorrelation */
    volk::vector<float> angle_corr;        /** autocorrelation angles */
    volk::vector<float> angle_diff;        /** angle differences */
    volk::vector<float> w_window_l;        /** weighting window when locked */
    volk::vector<float> w_window_u;        /** weighting window when unlocked */
    volk::vector<float> w_angle_diff;      /** weighted angle differences */
    volk::vector<gr_complex> unmod_pilots; /** conjugate of un-modulated pilots */

    /* Fine estimation only */
    volk::vector<float> angle_pilot;  /** average angle of pilot segments */
    volk::vector<float> angle_diff_f; /** diff of average pilot angles */

public:
    freq_sync(unsigned int period, int debug_level, const uint64_t* codewords);

    /**
     * \brief Data-aided coarse frequency offset estimation
     *
     * The estimation completes after `period` frames have been
     * processed. Hence, one in every `period` calls a new estimate will
     * be produced.
     *
     * \param in (gr_complex *) Pointer to the start of frame
     * \param i_codeword (uint8_t) PLSC dataword (also the codeword idx)
     * \param locked (bool) Whether frame timing is locked
     * \return (bool) whether new estimate was computed.
     *
     * \note in order to compute the data-aided coarse frequency offset
     * estimation based on the entire PLHEADER, the PLSC dataword must
     * be correctly decoded. Hence, before frame time locking is
     * acquired, this function will estimate frequency offset based on
     * the SOF symbols only.
     */
    bool estimate_coarse(const gr_complex* in, uint8_t i_codeword, bool locked);

    /**
     * \brief Estimate the average phase of the PLHEADER
     * \param in (gr_complex *) Pointer to PLHEADER symbol array
     * \param i_codeword (uint8_t) PLSC dataword (also the codeword idx)
     * \return (float) The phase estimate within -pi to +pi.
     *
     * \note i_codeword indicates the true (expected) PLHEADER symbols,
     * so that the phase estimation can be fully data-aided.
     */
    float estimate_plheader_phase(const gr_complex* in, uint8_t i_codeword);

    /**
     * \brief Estimate the average phase of a pilot block
     * \param in (gr_complex *) Pointer to the pilot symbol array
     * \param i_blk (int) Index of this pilot block within the PLFRAME
     * \return (float) The phase estimate within -pi to +pi.
     *
     * \note The gr_complex array pointed `in` is expected to contain
     * the PLHEADER within its first 90 positions, then all 36-symbol
     * pilot blocks consecutively in the indexes that follow. The pilot
     * block index `i_blk` is used internally in order to fetch the
     * correct input pilots for phase estimation. The result will be
     * stored in an internal pilot angle buffer.
     */
    float estimate_pilot_phase(const gr_complex* in, int i_blk);

    /**
     * \brief Data-aided fine frequency offset estimation
     *
     * Should be executed solely if the frame has pilots
     *
     * \param pilots (gr_complex *) Pointer to the pilot symbol buffer
     * \param n_pilot_blks (uint8_t) Number of pilot blocks in the frame
     * \return Void.
     */
    void estimate_fine(const gr_complex* pilots, uint8_t n_pilot_blks);

    /**
     * \brief De-rotate PLHEADER symbols
     * \param in (const gr_complex *) Input rotated PLHEADER buffer
     * \return Void
     *
     * \note The de-rotate PLHEADER is saved internally and can be
     * accessed using the `get_plheader` function below.
     */
    void derotate_plheader(const gr_complex* in);

    double get_coarse_foffset() { return coarse_foffset; }
    double get_fine_foffset() { return fine_foffset; }
    bool is_coarse_corrected() { return coarse_corrected; }
    const gr_complex* get_plheader() { return pp_plheader.data(); }
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_PL_FREQ_SYNC_H */
