/* -*- c++ -*- */
/*
 * Copyright 2018 Ron Economos.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bbdescrambler_bb_impl.h"
#include "fec_params.h"
#include <gnuradio/io_signature.h>

namespace gr {
namespace dvbs2rx {

bbdescrambler_bb::sptr bbdescrambler_bb::make(dvb_standard_t standard,
                                              dvb_framesize_t framesize,
                                              dvb_code_rate_t rate)
{
    return gnuradio::get_initial_sptr(
        new bbdescrambler_bb_impl(standard, framesize, rate));
}

/*
 * The private constructor
 */
bbdescrambler_bb_impl::bbdescrambler_bb_impl(dvb_standard_t standard,
                                             dvb_framesize_t framesize,
                                             dvb_code_rate_t rate)
    : gr::sync_block("bbdescrambler_bb",
                     gr::io_signature::make(1, 1, sizeof(unsigned char)),
                     gr::io_signature::make(1, 1, sizeof(unsigned char)))
{
    fec_info_t fec_info;
    get_fec_info(standard, framesize, rate, fec_info);
    kbch_bytes = fec_info.bch.k / 8;
    init_bb_derandomiser();
    set_output_multiple(kbch_bytes);
}

/*
 * Our virtual destructor.
 */
bbdescrambler_bb_impl::~bbdescrambler_bb_impl() {}

void bbdescrambler_bb_impl::init_bb_derandomiser(void)
{
    memset(bb_derandomise, 0, sizeof(bb_derandomise));
    int sr = 0x4A80;
    for (int i = 0; i < FRAME_SIZE_NORMAL; i++) {
        int b = ((sr) ^ (sr >> 1)) & 1;
        int i_byte = i / 8;
        int i_bit = 7 - (i % 8);
        bb_derandomise[i_byte] |= b << i_bit;
        sr >>= 1;
        if (b) {
            sr |= 0x4000;
        }
    }
}

int bbdescrambler_bb_impl::work(int noutput_items,
                                gr_vector_const_void_star& input_items,
                                gr_vector_void_star& output_items)
{
    const unsigned char* in = (const unsigned char*)input_items[0];
    unsigned char* out = (unsigned char*)output_items[0];

    for (int i = 0; i < noutput_items; i += kbch_bytes) {
        for (int j = 0; j < (int)kbch_bytes; ++j) {
            out[i + j] = in[i + j] ^ bb_derandomise[j];
        }
    }

    // Tell runtime system how many output items we produced.
    return noutput_items;
}

} /* namespace dvbs2rx */
} /* namespace gr */
