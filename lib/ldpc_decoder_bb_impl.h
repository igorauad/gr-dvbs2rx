/* -*- c++ -*- */
/*
 * Copyright 2018,2019,2023 Ahmet Inan, Ron Economos, Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_LDPC_DECODER_BB_IMPL_H
#define INCLUDED_DVBS2RX_LDPC_DECODER_BB_IMPL_H


#include "dvb_defines.h"
#include "dvb_s2_tables.hh"
#include "dvb_s2x_tables.hh"
#include "dvb_t2_tables.hh"
#include "ldpc_decoder/ldpc.hh"
#include <gnuradio/dvbs2rx/ldpc_decoder_bb.h>

namespace gr {
namespace dvbs2rx {

class ldpc_decoder_bb_impl : public ldpc_decoder_bb
{
private:
    const int d_debug_level;     /**< Debug level for logs */
    unsigned int d_nldpc;        /**< Codeword length in bits */
    unsigned int d_nldpc_bytes;  /**< Codeword length in bytes */
    unsigned int d_kldpc;        /**< Message length in bits */
    unsigned int d_kldpc_bytes;  /**< Message length in bytes */
    unsigned int d_output_mode;  /**< Output full codeword or just message */
    uint64_t d_frame_cnt;        /**< Frame count */
    uint64_t d_batch_cnt;        /**< Frame batch count */
    unsigned int d_total_trials; /**< Total LDPC decoding trials */
    int d_max_trials;            /**< Max decoding trials per frame */
    LDPCInterface* d_ldpc;
    int d_simd_size; /**< Number of bytes on the SIMD register */
    int8_t* d_soft;
    void* d_aligned_buffer;
    int (*decode)(void*, int8_t*, int);
    pmt::pmt_t d_pdu_meta;
    const pmt::pmt_t d_pdu_port_id = pmt::mp("llr_pdu");

public:
    ldpc_decoder_bb_impl(dvb_standard_t standard,
                         dvb_framesize_t framesize,
                         dvb_code_rate_t rate,
                         dvb_constellation_t constellation,
                         dvb_outputmode_t outputmode,
                         dvb_infomode_t infomode,
                         int max_trials,
                         int debug_level);
    ~ldpc_decoder_bb_impl();

    void forecast(int noutput_items, gr_vector_int& ninput_items_required);

    int general_work(int noutput_items,
                     gr_vector_int& ninput_items,
                     gr_vector_const_void_star& input_items,
                     gr_vector_void_star& output_items);

    unsigned int get_average_trials() { return d_total_trials / d_batch_cnt; }
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_LDPC_DECODER_BB_IMPL_H */