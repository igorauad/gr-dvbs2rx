/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_PLSYNC_CC_IMPL_H
#define INCLUDED_DVBS2RX_PLSYNC_CC_IMPL_H

#include "pl_defs.h"
#include "pl_descrambler.h"
#include "pl_frame_sync.h"
#include "pl_freq_sync.h"
#include "pl_signaling.h"
#include <dvbs2rx/plsync_cc.h>
#include <volk/volk_alloc.hh>

namespace gr {
namespace dvbs2rx {

/* @brief Upstream rotator frequency */
struct rot_freq_t {
    double freq = 0;  /** Frequency correction value */
    uint64_t idx = 0; /** Absolute index where it started */
};

/* @brief Upstream rotator control */
struct rot_ctrl_t {
    int tag_delay = 0;             /** Delay of rotator's rot_phase_inc tag */
    uint64_t tag_search_start = 0; /** Starting index for the next tag search */
    rot_freq_t past;               /** Frequency state at the past PLFRAME */
    rot_freq_t current;            /** Frequency state at the current PLFRAME */
    rot_freq_t next;               /** Frequency state expected for the next PLFRAME */
};

/** @brief Index tracking for various segments of a PLFRAME */
struct plframe_idx_t {
    uint16_t i_in_frame = -1;    /** Symbol index in PLFRAME */
    uint16_t i_in_slot = 0;      /** Symbol index in data slot */
    uint16_t i_in_pilot_blk = 0; /** Symbol index in pilot block */
    uint16_t i_slot = 0;         /** Slot index */
    uint16_t i_pilot_blk = 0;    /** pilot block index */
    bool is_pilot_sym = false;   /** Current symbol is a pilot (non-plheader) symbol */
    bool is_data_sym = false;    /** Current symbol is a data symbol */
    void step(uint16_t plframe_len, bool has_pilots);
    void reset();
    bool is_valid_pilot_idx();
};

class plsync_cc_impl : public plsync_cc
{
private:
    /* Parameters */
    int d_debug_level; /** debug level */
    const float d_sps; /** samples per symbol */
    /* NOTE: the PLSYNC block requires a symbol-spaced stream at its
     * input. Hence, sps does not refer to the input stream. Instead, it
     * refers to the oversampling ratio that is adopted in the receiver
     * flowgraph prior to the matched filter. This is so that this block
     * can control the external rotator phase properly */

    /* State */
    bool d_locked;         /** Whether frame timing is locked */
    float d_da_phase;      /** Last data-aided phase estimate */
    rot_ctrl_t d_rot_ctrl; /** Upstream rotator control */
    plframe_idx_t d_idx;   /** PLFRAME indexes */

    const pmt::pmt_t d_port_id = pmt::mp("rotator_phase_inc");

    /* Objects */
    frame_sync* d_frame_sync;         /** frame synchronizer */
    freq_sync* d_freq_sync;           /** frequency synchronizer */
    plsc_decoder* d_plsc_decoder;     /** PLSC decoder */
    pl_descrambler* d_pl_descrambler; /** PL descrambler */

    /* Buffers */
    volk::vector<gr_complex> d_rx_pilots; /** Received pilot symbols */

    /* Send new frequency correction to the upstream rotator.
     *
     * The PL Sync block is designed to be used in conjunction with a rotator
     * block for frequency correction. The processing chain is expected to be
     * somewhat similar to the following:
     *
     *  --> Rotator --> Symbol Sync (MF/Decimator) --> DVB-S2 PL Sync
     *
     * That is, the rotator is upstream after an intermediate symbol sync block,
     * which, in turn, implements symbol timing recovery, matched filtering, and
     * decimation. As a result, the frequency correction applied by the rotator
     * promotes better performance both on the symbol timing recovery and on the
     * DVB-S2 PL synchronization.
     *
     * In this architecture, the PL Sync block is responsible for controlling
     * the rotator frequency through a message mechanism. This function
     * implements such control.
     *
     * More specifically, this function coordinates the moment when the
     * frequency correction is supposed to change on the upstream rotator. If
     * the frequency correction was allowed to change at any moment, it could
     * change in the middle of a frame and degrade the decoding performance
     * significantly. Hence, this function attempts to schedule every change at
     * the start of a PLHEADER instead. To do so, it relies on the tag delay
     * calibrated by function "calibrate_tag_delay".
     *
     * @param sof_detection_idx Relative index where the SOF was detected.
     * @param coarse_corrected Whether the coarse frequency correction has
     *        already been applied.
     * @param new_coarse_est Coarse frequency offset estimate.
     * @param new_fine_est Fine frequency offset estimate.
     *
     * @note This function should be called whenever the SOF is detected. The
     * SOF detection index is the symbol index where the detection completes,
     * which is assumed to be the last PLHEADER symbol.
     *
     * @note A relative index is relative to the start of the current work
     * buffer. An absolute index is counted monotonically since the beginning of
     * the flowgraph. The sof_detection_idx parameter is a relative index.
     */
    void control_rotator_freq(int sof_detection_idx,
                              bool coarse_corrected,
                              bool new_coarse_est,
                              bool new_fine_est);

    /**
     * @brief Calibrate the delay between the upstream rotator and this block.
     *
     * The rotator places a tag on the sample where the new phase increment
     * starts to take effect. However, this sample typically traverses a matched
     * filter and decimator block, which can alter the tag index. Hence, the tag
     * may not come exactly where expected.
     *
     * This function calibrates the tag delay by comparing the index on which
     * the tag came to where it was expected in the symbol-spaced
     * stream. Ultimately, the measured tag delay can be used to pre-compensate
     * for it when scheduling the next phase increment update.
     *
     * @param sof_detection_idx Relative index where the SOF was detected.
     * @param noutput_items Number of output items processed in this work call.
     * @param tolerance Tag delay tolerance. If exceeded, a warning is printed.
     *
     * @note Just like control_rotator_freq, this function should be called
     *       whenever the SOF is detected.
     **/
    void
    calibrate_tag_delay(int sof_detection_idx, int noutput_items, int tolerance = 300);


public:
    plsync_cc_impl(int gold_code, int freq_est_period, float sps, int debug_level);
    ~plsync_cc_impl();

    // Where all the action really happens
    void forecast(int noutput_items, gr_vector_int& ninput_items_required);

    int general_work(int noutput_items,
                     gr_vector_int& ninput_items,
                     gr_vector_const_void_star& input_items,
                     gr_vector_void_star& output_items);

    float get_freq_offset() { return d_freq_sync->get_coarse_foffset(); }
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_PLSYNC_CC_IMPL_H */
