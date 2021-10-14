/* -*- c++ -*- */
/*
 * Copyright 2018,2019 Ahmet Inan, Ron Economos.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifndef INCLUDED_DVBS2RX_LDPC_DECODER_CB_IMPL_H
#define INCLUDED_DVBS2RX_LDPC_DECODER_CB_IMPL_H

#include "algorithms.hh"
#include "dvb_defines.h"
#include "dvb_s2_tables.hh"
#include "dvb_s2x_tables.hh"
#include "dvb_t2_tables.hh"
#include "ldpc.hh"
#include "psk.hh"
#include "qam.hh"
#include <dvbs2rx/ldpc_decoder_cb.h>

#define FACTOR 2

#ifdef __AVX2__
const int SIZEOF_SIMD = 32;
#else
const int SIZEOF_SIMD = 16;
#endif

typedef int8_t code_type;
const int SIMD_WIDTH = SIZEOF_SIMD / sizeof(code_type);
typedef SIMD<code_type, SIMD_WIDTH> simd_type;

#if 0
#include "flooding_decoder.hh"
typedef SelfCorrectedUpdate<simd_type> update_type;
typedef MinSumAlgorithm<simd_type, update_type> algorithm_type;
const int DEFAULT_TRIALS = 50;
#else
#include "layered_decoder.hh"
typedef NormalUpdate<simd_type> update_type;
typedef OffsetMinSumAlgorithm<simd_type, update_type, FACTOR> algorithm_type;
const int DEFAULT_TRIALS = 25;
#endif

namespace gr {
namespace dvbs2rx {

class ldpc_decoder_cb_impl : public ldpc_decoder_cb
{
private:
    unsigned int frame_size;
    unsigned int signal_constellation;
    unsigned int code_rate;
    unsigned int nbch;
    unsigned int q_val;
    unsigned int dvb_standard;
    unsigned int output_mode;
    unsigned int info_mode;
    unsigned int frame;
    unsigned int chunk;
    unsigned int total_trials;
    int d_max_trials;
    float snr;
    float precision;
    float total_snr;
    unsigned int rowaddr0;
    unsigned int rowaddr1;
    unsigned int rowaddr2;
    unsigned int rowaddr3;
    unsigned int rowaddr4;
    unsigned int rowaddr5;
    unsigned int rowaddr6;
    unsigned int rowaddr7;
    LDPCInterface* ldpc;
    Modulation<gr_complex, int8_t>* mod;
    LDPCDecoder<simd_type, algorithm_type> decode;
    int8_t* soft;
    int8_t* dint;
    int8_t* tempu;
    int8_t* tempv;
    void* aligned_buffer;

    int interleave_lookup_table[FRAME_SIZE_NORMAL];
    int deinterleave_lookup_table[FRAME_SIZE_NORMAL];

    void generate_interleave_lookup();
    void generate_deinterleave_lookup();
    inline void interleave_parity_bits(int* tempu, const int*& in);
    inline void
    twist_interleave_columns(int* tempv, int* tempu, int rows, int mod, const int* twist);
    inline void twist_deinterleave_columns(
        int* tempv, int* tempu, int rows, int mod, const int* twist);

    const static int twist16n[8];
    const static int twist64n[12];
    const static int twist256n[16];

    const static int twist16s[8];
    const static int twist64s[12];
    const static int twist256s[8];

    const static int mux16[8];
    const static int mux64[12];
    const static int mux256[16];

    const static int mux16_35[8];
    const static int mux16_13[8];
    const static int mux16_25[8];
    const static int mux64_35[12];
    const static int mux64_13[12];
    const static int mux64_25[12];
    const static int mux256_35[16];
    const static int mux256_23[16];

    const static int mux256s[8];
    const static int mux256s_13[8];
    const static int mux256s_25[8];

public:
    ldpc_decoder_cb_impl(dvb_standard_t standard,
                         dvb_framesize_t framesize,
                         dvb_code_rate_t rate,
                         dvb_constellation_t constellation,
                         dvb_outputmode_t outputmode,
                         dvb_infomode_t infomode,
                         int max_trials);
    ~ldpc_decoder_cb_impl();

    void forecast(int noutput_items, gr_vector_int& ninput_items_required);

    int general_work(int noutput_items,
                     gr_vector_int& ninput_items,
                     gr_vector_const_void_star& input_items,
                     gr_vector_void_star& output_items);
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_LDPC_DECODER_CB_IMPL_H */
