/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_PL_FRAME_SYNC_H
#define INCLUDED_DVBS2RX_PL_FRAME_SYNC_H

#include "delay_line.h"
#include "pl_defs.h"
#include <gnuradio/gr_complex.h>

/* correlator lengths, based on the number of differentials that we know in
 * advance (25 for SOF and only 32 for PLSC) */
#define SOF_CORR_LEN (SOF_LEN - 1)
#define PLSC_CORR_LEN 32

namespace gr {
namespace dvbs2rx {

class frame_sync
{
private:
    /* Parameters */
    int debug_level; /** debug level */

    /* State */
    uint32_t sym_cnt;    /** symbol count */
    gr_complex last_in;  /** last symbol in */
    float timing_metric; /** most recent timing metric */

    bool locked;            /** whether frame timing is locked */
    bool locked_prev;       /** previous locked state */
    unsigned int frame_len; /** current PLFRAME length */

    delay_line<gr_complex> d_plsc_delay_buf; /** buffer used as delay line */
    delay_line<gr_complex> d_sof_buf;        /** SOF correlator buffer */
    delay_line<gr_complex> d_plsc_e_buf;     /** Even PLSC correlator buffer  */
    delay_line<gr_complex> d_plsc_o_buf;     /** Odd PLSC correlator buffer */
    delay_line<gr_complex> d_plheader_buf;   /** buffer used to store PLHEADER syms */
    volk::vector<gr_complex> d_sof_taps;     /** SOF cross-correlation taps */
    volk::vector<gr_complex> d_plsc_taps;    /** PLSC cross-correlation taps */

    /* Timing metric threshold for inferring a start of frame.
     *
     * When unlocked, use a conservative threshold, as it is important
     * to avoid false positive SOF detection. In contrast, when locked,
     * we only want to periodically check whether the correlation is
     * sufficiently strong where it is expected to be (at the start of
     * the next frame). Since it is very important not to unlock
     * unncessarily, use a lower threshold for this. */
    const float threshold_u = 30; /** unlocked threshold */
                                  /* TODO: make this a top-level parameter */
    const float threshold_l = 25; /** locked threshold */

    /**
     * \brief Cross-correlation between a delay line buffer and a given vector.
     * \param d_line Reference to a delay line buffer with the newest sample at
     *               index 0 and the oldest sample at the last index.
     * \param taps Reference to a volk vector with taps for cross-correlation.
     * \param res Pointer to result.
     * \note The tap vector should consist of the folded version of the target
     * sequence (SOF or PLSC).
     * \return Void.
     */
    void correlate(delay_line<gr_complex>& d_line,
                   volk::vector<gr_complex>& taps,
                   gr_complex* res);

public:
    frame_sync(int debug_level);

    /**
     * \brief Step frame timing recovery loop
     * \param in (gr_complex &) Input symbol
     * \return bool whether current symbol is the first of a new frame
     */
    bool step(const gr_complex& in);

    float get_timing_metric() { return timing_metric; }
    bool get_locked() { return locked; }
    void set_frame_len(unsigned int len) { frame_len = len; }

    const gr_complex* get_plheader() { return &d_plheader_buf.back(); }
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_PL_FRAME_SYNC_H */
