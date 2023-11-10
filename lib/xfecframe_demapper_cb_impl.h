/* -*- c++ -*- */
/*
 * Copyright 2023 Igor Freire.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_XFECFRAME_DEMAPPER_CB_IMPL_H
#define INCLUDED_DVBS2RX_XFECFRAME_DEMAPPER_CB_IMPL_H

#include "cpu_features_macros.h"
#include "psk.hh"
#include "qpsk.h"
#include <gnuradio/dvbs2rx/xfecframe_demapper_cb.h>
#include <volk/volk_alloc.hh>
#include <array>

namespace gr {
namespace dvbs2rx {

// Store enough XFECFRAMEs in a pool to measure the post-decoder SNR. Assume the LDPC
// decoder will typically process a single SIMD batch at a time, and that it is unlikely
// we need to store more than two SIMD batches here. On an x86 machine, which could be
// running AVX2 with a SIMD batch of 32 frames, store 64 XFECFRAMEs. On an ARM processor,
// which could have ARM Neon with a batch of 16, store 32 XFECFRAMEs. Note this can use
// quite a bit of memory. For instance, with n_mod=2 and normal FECFRAMEs, the XFECFRAME
// has 32400 complex symbols, so a 64-frame pool would use 32400*64*8 ~= 15.8 MB.
#ifdef CPU_FEATURES_ARCH_ANY_ARM
#define XFECFRAME_POOL_SIZE 32
#else
#define XFECFRAME_POOL_SIZE 64
#endif

class xfecframe_demapper_cb_impl : public xfecframe_demapper_cb
{
private:
    dvb_constellation_t d_constellation;
    bool d_waiting_first_llr;
    unsigned int d_fecframe_len;
    unsigned int d_xfecframe_len;
    unsigned int d_rowaddr0;
    unsigned int d_rowaddr1;
    unsigned int d_rowaddr2;
    float d_snr; /**< Estimated SNR in dB */
    float d_N0;  /**< Estimated noise energy per complex dimension */
    float d_precision;
    uint64_t d_frame_cnt; /**< Total count of processed frames */
    volk::vector<int8_t> d_aux_8i_buffer;
    volk::vector<int8_t> d_aux_8i_buffer_2;
    Modulation<gr_complex, int8_t>* d_mod;
    std::unique_ptr<QpskConstellation> d_qpsk;
    gr::thread::mutex d_mutex;

    // Used for measuring the post-decoder SNR using the LLRs reported by the LDPC decoder
    std::array<volk::vector<gr_complex>, XFECFRAME_POOL_SIZE> d_xfecframe_buffer_pool;
    std::array<uint64_t, XFECFRAME_POOL_SIZE> d_xfecframe_saved;
    size_t d_idx_xfecframe_buffer; /**< Index to the next XFECFRAME bufer */
    const pmt::pmt_t d_pdu_port_id = pmt::mp("llr_pdu");
    void handle_llr_pdu(pmt::pmt_t pdu);

public:
    xfecframe_demapper_cb_impl(dvb_framesize_t framesize,
                               dvb_code_rate_t rate,
                               dvb_constellation_t constellation);
    ~xfecframe_demapper_cb_impl();

    void forecast(int noutput_items, gr_vector_int& ninput_items_required);

    int general_work(int noutput_items,
                     gr_vector_int& ninput_items,
                     gr_vector_const_void_star& input_items,
                     gr_vector_void_star& output_items);

    float get_snr() { return d_snr; }
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_XFECFRAME_DEMAPPER_CB_IMPL_H */
