/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_PLSYNC_CC_H
#define INCLUDED_DVBS2RX_PLSYNC_CC_H

#include <gnuradio/block.h>
#include <gnuradio/dvbs2rx/api.h>

namespace gr {
namespace dvbs2rx {

/*!
 * \brief DVB-S2 Physical Layer (PL) Synchronizer
 * \ingroup dvbs2rx
 *
 * \details
 *
 * This block finds DVB-S2 PLFRAMEs on the input symbol-spaced IQ stream and outputs the
 * corresponding XFECFRAMEs towards a downstream constellation de-mapper block.
 * Internally, it implements PL frame timing recovery, coarse and fine frequency offset
 * estimation, carrier phase tracking, PLSC decoding, and PL descrambling. Furthermore, it
 * manages frequency corrections carried out by an external rotator block connected via a
 * message port.
 *
 * This block can also filter PLFRAMEs based on target PL signaling (PLS) values. In
 * constant coding and modulation (CCM) mode, the PLS filter must specify a single option
 * (i.e., a single MODCOD, frame size, and pilot configuration). In contrast, in adaptive
 * or variable coding and modulation (ACM/VCM) mode, the filter can be configured to allow
 * multiple PLS values, including all of them. In this case, since the output XFECFRAMEs
 * can vary in length and format, this block tags the first sample of each output
 * XFECFRAME with the frame's PLS information.
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
     * \param sps (double) Oversampling ratio at the input to the upstream MF.
     * \param debug_level (int) Debug level.
     * \param acm_vcm (bool) Whether running in ACM/VCM mode. Determines whether the PLS
     * filter can include multiple options.
     * \param multistream (bool) Whether the input signal carries multiple MPEG transport
     * streams (MIS mode). Determines whether dummy PLFRAMEs are expected in the received
     * signal, even if operating in CCM mode (refer to Table D.2 of the standard).
     * \param pls_filter_lo (uint64_t) Lower 64 bits of the PLS filter bitmask. A value of
     * 1 in the n-th position indicates PLS "n" (for n in 0 to 63) should be enabled.
     * \param pls_filter_hi (uint64_t) Upper 64 bits of the PLS filter bitmask. A value of
     * 1 in the n-th position indicates PLS "n" (for n in 64 to 127) should be enabled.
     *
     * \note When `acm_vcm=false`, the constructor throws an exception if `pls_filter_lo`
     * and `pls_filter_hi` collectively select more than one PLS value (i.e., if their
     * aggregate population count is greater than one).
     *
     * \note The oversampling ratio (sps) parameter is only used to schedule phase
     * increment updates (i.e., frequency corrections) to an external rotator. This block
     * attempts to schedule frequency corrections at the start of PLFRAMEs. Nevertheless,
     * while this block processes a symbol-spaced IQ stream, it assumes the external
     * rotator lies before the matched filter (MF) and, as such, processes a
     * fractionally-spaced IQ stream. Hence, when scheduling a frequency correction, this
     * block uses the sps paramter to adjust the symbol-spaced sample offset of a PLFRAME
     * to the corresponding fractionally-spaced offset in the rotator's input.
     */
    static sptr make(int gold_code,
                     int freq_est_period,
                     double sps,
                     int debug_level,
                     bool acm_vcm,
                     bool multistream,
                     uint64_t pls_filter_lo,
                     uint64_t pls_filter_hi);

    /*!
     * \brief Get the current frequency offset estimate.
     * \return (float) Frequency offset.
     */
    virtual float get_freq_offset() = 0;

    /*!
     * \brief Get the coarse frequency offset correction state.
     * \return (bool) True when the frequency offset is coarsely corrected.
     */
    virtual bool get_coarse_freq_corr_state() = 0;

    /*!
     * \brief Get the current lock status.
     * \return (bool) True when the frame synchronizer is locked.
     */
    virtual bool get_locked() = 0;

    /*!
     * \brief Get the current count of detected start-of-frame (SOF) instants.
     *
     * This count includes all detected SOFs, including false positives. Note that
     * detecting a SOF does not mean that instant will lead to a processed frame. Frames
     * are only processed after frame timing lock, which requires two consecutive SOFs
     * detected with the correct interval between them. Hence, the SOF count is always
     * greater than or equal to the processed frame count.
     *
     * \return (uint64_t) Detected SOF count.
     */
    virtual uint64_t get_sof_count() = 0;

    /*!
     * \brief Get the current count of processed (accepted) PLFRAMEs.
     *
     * A PLFRAME is processed after frame timing lock and after being accepted by the PLS
     * filter, in which case its XFECFRAME is output to the next block. Frames rejected by
     * the PLS filter and dummy frames are not included in this count.
     *
     * \return (uint64_t) Processed frame count.
     */
    virtual uint64_t get_frame_count() = 0;

    /*!
     * \brief Get the current count of rejected PLFRAMEs.
     * \return (uint64_t) Rejected frame count.
     */
    virtual uint64_t get_rejected_count() = 0;

    /*!
     * \brief Get the current count of received dummy PLFRAMEs.
     * \return (uint64_t) Dummy frame count.
     */
    virtual uint64_t get_dummy_count() = 0;

    /*!
     * \brief Get the timestamp of the last frame synchronization lock.
     * \return (std::chrono::system_clock::time_point) Last frame lock timestamp in UTC.
     * \note The timestamp is only valid after the first frame lock.
     */
    virtual std::chrono::system_clock::time_point get_lock_time() = 0;
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_PLSYNC_CC_H */
