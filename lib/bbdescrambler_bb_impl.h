/* -*- c++ -*- */
/*
 * Copyright 2018 Ron Economos.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_BBDESCRAMBLER_BB_IMPL_H
#define INCLUDED_DVBS2RX_BBDESCRAMBLER_BB_IMPL_H

#include "dvb_defines.h"
#include <gnuradio/dvbs2rx/bbdescrambler_bb.h>

namespace gr {
namespace dvbs2rx {

class bbdescrambler_bb_impl : public bbdescrambler_bb
{
private:
    unsigned int kbch;
    unsigned int kbch_bytes;
    unsigned char bb_derandomise[FRAME_SIZE_NORMAL / 8];
    void init_bb_derandomiser(void);

public:
    bbdescrambler_bb_impl(dvb_standard_t standard,
                          dvb_framesize_t framesize,
                          dvb_code_rate_t rate);
    ~bbdescrambler_bb_impl();

    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items);
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_BBDESCRAMBLER_BB_IMPL_H */
