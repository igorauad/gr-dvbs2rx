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
#include "plsc_decoder.h"
#include <dvbs2rx/plsync_cc.h>
#include <volk/volk_alloc.hh>

namespace gr {
namespace dvbs2rx {

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
    uint16_t d_i_sym;          /** Symbol index within PLFRAME */
    bool d_locked;             /** Whether frame timing is locked */
    float d_da_phase;          /** Last data-aided phase estimate */
    int d_tag_delay;           /** delay of rotator's new inc tag */
    double d_rot_freq[3] = {}; /** Upstream rotator's frequency (past,
                                * current and next) */

    const pmt::pmt_t d_port_id = pmt::mp("rotator_phase_inc");

    /* Objects */
    frame_sync* d_frame_sync;         /** frame synchronizer */
    freq_sync* d_freq_sync;           /** frequency synchronizer */
    plsc_decoder* d_plsc_decoder;     /** PLSC decoder */
    pl_descrambler* d_pl_descrambler; /** PL descrambler */

    /* Buffers */
    volk::vector<gr_complex> d_rx_pilots; /** Received pilot symbols */

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
