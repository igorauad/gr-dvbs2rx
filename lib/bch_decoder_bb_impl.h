/* -*- c++ -*- */
/*
 * Copyright 2018 Ahmet Inan, Ron Economos.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_BCH_DECODER_BB_IMPL_H
#define INCLUDED_DVBS2RX_BCH_DECODER_BB_IMPL_H

#include "bch.h"
#include <gnuradio/dvbs2rx/bch_decoder_bb.h>
#include <memory>

namespace gr {
namespace dvbs2rx {

class bch_decoder_bb_impl : public bch_decoder_bb
{
private:
    const int d_debug_level;
    unsigned int d_k_bytes; // message length in bytes
    unsigned int d_n_bytes; // codeword length in bytes
    std::unique_ptr<galois_field<uint32_t>> d_gf;
    std::unique_ptr<bch_codec<uint32_t, bitset256_t>> d_codec;
    uint64_t d_frame_cnt;
    uint64_t d_frame_error_cnt;

public:
    bch_decoder_bb_impl(dvb_standard_t standard,
                        dvb_framesize_t framesize,
                        dvb_code_rate_t rate,
                        dvb_outputmode_t outputmode,
                        int debug_level);
    ~bch_decoder_bb_impl();

    void forecast(int noutput_items, gr_vector_int& ninput_items_required);

    int general_work(int noutput_items,
                     gr_vector_int& ninput_items,
                     gr_vector_const_void_star& input_items,
                     gr_vector_void_star& output_items);

    uint64_t get_frame_count() { return d_frame_cnt; }
    uint64_t get_error_count() { return d_frame_error_cnt; }
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_BCH_DECODER_BB_IMPL_H */
