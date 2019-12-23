/* -*- c++ -*- */
/*
 * Copyright 2019-2021 Igor Freire.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_PLSYNC_CC_H
#define INCLUDED_DVBS2RX_PLSYNC_CC_H

#include <gnuradio/block.h>
#include <dvbs2rx/api.h>

namespace gr {
namespace dvbs2rx {

/*!
 * \brief DVB-S2 Physical Layer (PL) Synchronizer
 * \ingroup dvbs2rx
 *
 * \details
 *
 * This block finds DVB-S2 PLFRAMEs on a symbol-spaced IQ stream and outputs the
 * corresponding XFECFRAMEs for a downstream constellation de-mapper block. To
 * enable ACM/VCM implementations, this block tags the first sample of each
 * output XFECFRAME sample with the corresponding physical layer signaling (PLS)
 * information (MODCOD, frame type, and pilots flag).
 *
 * Internally, this block implements PL frame timing recovery, coarse and fine
 * frequency offset estimation, carrier phase tracking, PLSC decoding, and PL
 * descrambling. Furthermore, it manages frequency corrections to be carried out
 * by an external rotator block connected via message port.
 */
class DVBS2RX_API plsync_cc : virtual public gr::block
{
public:
    typedef std::shared_ptr<plsync_cc> sptr;

    /*!
     * \brief Make physical layer deframer block.
     *
     * \param gold_code (int) Gold code used for physical layer scrambling.
     * \param freq_est_period (int) Freq. offset estimation period in frames.
     * \param sps (float) Oversampling ratio at the input to the upstream MF.
     * \param debug_level (int) Debug level.
     *
     * \note The oversampling ratio (sps) parameter is only used to schedule
     * phase increment updates (i.e., frequency corrections) to an external
     * rotator. This block attempts to schedule frequency corrections at the
     * start of PLFRAMEs. Nevertheless, while this block processes a
     * symbol-spaced IQ stream, it assumes the external rotator lies before the
     * matched filter (MF) and, as such, processes a fractionally-spaced IQ
     * stream. Hence, when scheduling a frequency correction, this block uses
     * the sps paramter to adjust the symbol-spaced sample offset of a PLFRAME
     * to the corresponding fractionally-spaced offset in the rotator's input.
     */
    static sptr make(int gold_code, int freq_est_period, float sps, int debug_level);

    /*!
     * \brief Get current frequency offset estimate
     * \return (float) Frequency offset
     */
    virtual float get_freq_offset() = 0;
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_PLSYNC_CC_H */
