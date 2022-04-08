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

#include "bitman.hh"
#include "bose_chaudhuri_hocquenghem_decoder.hh"
#include "galois_field.hh"
#include <gnuradio/dvbs2rx/bch_decoder_bb.h>

namespace gr {
namespace dvbs2rx {

class bch_decoder_bb_impl : public bch_decoder_bb
{
private:
    unsigned int kbch;
    unsigned int nbch;
    unsigned int bch_code;
    unsigned int output_mode;
    uint64_t d_frame_cnt;
    uint64_t d_frame_error_cnt;
    typedef CODE::GaloisField<16, 0b10000000000101101, uint16_t> GF_NORMAL;
    typedef CODE::GaloisField<15, 0b1000000000101101, uint16_t> GF_MEDIUM;
    typedef CODE::GaloisField<14, 0b100000000101011, uint16_t> GF_SHORT;
    typedef CODE::BoseChaudhuriHocquenghemDecoder<24, 1, 65343, GF_NORMAL> BCH_NORMAL_12;
    typedef CODE::BoseChaudhuriHocquenghemDecoder<20, 1, 65375, GF_NORMAL> BCH_NORMAL_10;
    typedef CODE::BoseChaudhuriHocquenghemDecoder<16, 1, 65407, GF_NORMAL> BCH_NORMAL_8;
    typedef CODE::BoseChaudhuriHocquenghemDecoder<24, 1, 32587, GF_MEDIUM> BCH_MEDIUM_12;
    typedef CODE::BoseChaudhuriHocquenghemDecoder<24, 1, 16215, GF_SHORT> BCH_SHORT_12;
    GF_NORMAL* instance_n;
    GF_MEDIUM* instance_m;
    GF_SHORT* instance_s;
    BCH_NORMAL_12* decode_n_12;
    BCH_NORMAL_10* decode_n_10;
    BCH_NORMAL_8* decode_n_8;
    BCH_MEDIUM_12* decode_m_12;
    BCH_SHORT_12* decode_s_12;
    uint8_t* code;
    uint8_t* parity;

public:
    bch_decoder_bb_impl(dvb_standard_t standard,
                        dvb_framesize_t framesize,
                        dvb_code_rate_t rate,
                        dvb_outputmode_t outputmode);
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
