/* -*- c++ -*- */
/*
 * Copyright 2021 2019-2021 Igor Freire.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_SYMBOL_SYNC_CC_IMPL_H
#define INCLUDED_DVBS2RX_SYMBOL_SYNC_CC_IMPL_H

#include <gnuradio/gr_complex.h>
#include <dvbs2rx/symbol_sync_cc.h>
#include <volk/volk_alloc.hh>

namespace gr {
namespace dvbs2rx {

class DVBS2RX_API symbol_sync_cc_impl : public symbol_sync_cc
{
private:
    int d_sps;            /**< Samples per symbol (oversampling ratio) */
    int d_midpoint;       /**< Midpoint index between interpolants */
    unsigned d_history;   /**< History of samples in the input buffer */
    float d_K1;           /**< PI filter's proportional constant */
    float d_K2;           /**< PI filter's integrator constant */
    float d_vi;           /**< Last integrator value */
    float d_nominal_step; /**< Nominal mod-1 counter step (equal to "1/d_sps") */
    float d_cnt;          /**< Modulo-1 counter */
    float d_mu;           /**< Fractional symbol timing offset estimate */
    int d_jump;           /**< Samples to jump until the next strobe */
    bool d_init;          /**< Whether the loop is initialized (after the first work) */
    gr_complex d_last_xi; /**< Last output interpolant */
    std::vector<int> d_strobe_idx; /**< Indexes of the output interpolants */

    // Polyphase RRC filter
    std::vector<volk::vector<float>> d_rrc_subfilters; /** Vector of RRC subfilters */
    size_t d_n_subfilt;     /** Number of subfilters in the polyphase RRC filter bank */
    size_t d_subfilt_len;   /** Number of taps in each RRC subfilter */
    size_t d_subfilt_delay; /** RRC subfilter delay */

public:
    symbol_sync_cc_impl(float sps,
                        float loop_bw,
                        float damping_factor,
                        float rolloff,
                        int rrc_delay,
                        int n_subfilt);
    ~symbol_sync_cc_impl();

    void forecast(int noutput_items, gr_vector_int& ninput_items_required);

    int general_work(int noutput_items,
                     gr_vector_int& ninput_items,
                     gr_vector_const_void_star& input_items,
                     gr_vector_void_star& output_items);

    std::pair<int, int>
    loop(const gr_complex* in, gr_complex* out, int ninput_items, int noutput_items);
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_SYMBOL_SYNC_CC_IMPL_H */
