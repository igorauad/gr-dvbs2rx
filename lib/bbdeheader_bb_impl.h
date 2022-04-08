/* -*- c++ -*- */
/*
 * Copyright 2018 Ron Economos.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_BBDEHEADER_BB_IMPL_H
#define INCLUDED_DVBS2RX_BBDEHEADER_BB_IMPL_H

#include "dvb_defines.h"
#include <gnuradio/dvbs2rx/bbdeheader_bb.h>

typedef struct {
    int ts_gs;
    int sis_mis;
    int ccm_acm;
    int issyi;
    int npd;
    int ro;
    int isi;
    unsigned int upl;
    unsigned int dfl;
    int sync;
    unsigned int syncd;
} BBHeader;

typedef struct {
    BBHeader bb_header;
} FrameFormat;

namespace gr {
namespace dvbs2rx {

class bbdeheader_bb_impl : public bbdeheader_bb
{
private:
    unsigned int kbch;
    unsigned int max_dfl;
    unsigned int dvb_standard;
    unsigned int df_remaining;
    unsigned int count;
    unsigned int synched;
    unsigned char crc;
    unsigned int distance;
    unsigned int spanning;
    unsigned int index;
    uint64_t d_packet_cnt; /**< total packets received */
    uint64_t d_error_cnt;  /**< total packets with bit errors */
    FrameFormat m_format[1];
    unsigned char crc_tab[256];
    unsigned char packet[188];
    void build_crc8_table(void);
    unsigned int check_crc8_bits(const unsigned char*, int);

public:
    bbdeheader_bb_impl(dvb_standard_t standard,
                       dvb_framesize_t framesize,
                       dvb_code_rate_t rate);
    ~bbdeheader_bb_impl();

    // Where all the action really happens
    void forecast(int noutput_items, gr_vector_int& ninput_items_required);

    int general_work(int noutput_items,
                     gr_vector_int& ninput_items,
                     gr_vector_const_void_star& input_items,
                     gr_vector_void_star& output_items);

    uint64_t get_packet_count() { return d_packet_cnt; }
    uint64_t get_error_count() { return d_error_cnt; }
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_BBDEHEADER_BB_IMPL_H */
