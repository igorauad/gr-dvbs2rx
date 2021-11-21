/* -*- c++ -*- */
/*
 * Copyright 2018 Ahmet Inan, Ron Economos.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bch_decoder_bb_impl.h"
#include <gnuradio/io_signature.h>
#include <stdio.h>
#include <functional>

// BCH Code
#define BCH_CODE_N8 0
#define BCH_CODE_N10 1
#define BCH_CODE_N12 2
#define BCH_CODE_S12 3
#define BCH_CODE_M12 4

namespace gr {
namespace dvbs2rx {

bch_decoder_bb::sptr bch_decoder_bb::make(dvb_standard_t standard,
                                          dvb_framesize_t framesize,
                                          dvb_code_rate_t rate,
                                          dvb_outputmode_t outputmode)
{
    return gnuradio::get_initial_sptr(
        new bch_decoder_bb_impl(standard, framesize, rate, outputmode));
}

/*
 * The private constructor
 */
bch_decoder_bb_impl::bch_decoder_bb_impl(dvb_standard_t standard,
                                         dvb_framesize_t framesize,
                                         dvb_code_rate_t rate,
                                         dvb_outputmode_t outputmode)
    : gr::block("bch_decoder_bb",
                gr::io_signature::make(1, 1, sizeof(unsigned char)),
                gr::io_signature::make(1, 1, sizeof(unsigned char))),
      d_frame_cnt(0),
      d_frame_error_cnt(0)
{
    if (framesize == FECFRAME_NORMAL) {
        switch (rate) {
        case C1_4:
            kbch = 16008;
            nbch = 16200;
            bch_code = BCH_CODE_N12;
            break;
        case C1_3:
            kbch = 21408;
            nbch = 21600;
            bch_code = BCH_CODE_N12;
            break;
        case C2_5:
            kbch = 25728;
            nbch = 25920;
            bch_code = BCH_CODE_N12;
            break;
        case C1_2:
            kbch = 32208;
            nbch = 32400;
            bch_code = BCH_CODE_N12;
            break;
        case C3_5:
            kbch = 38688;
            nbch = 38880;
            bch_code = BCH_CODE_N12;
            break;
        case C2_3:
            kbch = 43040;
            nbch = 43200;
            bch_code = BCH_CODE_N10;
            break;
        case C3_4:
            kbch = 48408;
            nbch = 48600;
            bch_code = BCH_CODE_N12;
            break;
        case C4_5:
            kbch = 51648;
            nbch = 51840;
            bch_code = BCH_CODE_N12;
            break;
        case C5_6:
            kbch = 53840;
            nbch = 54000;
            bch_code = BCH_CODE_N10;
            break;
        case C8_9:
            kbch = 57472;
            nbch = 57600;
            bch_code = BCH_CODE_N8;
            break;
        case C9_10:
            kbch = 58192;
            nbch = 58320;
            bch_code = BCH_CODE_N8;
            break;
        case C2_9_VLSNR:
            kbch = 14208;
            nbch = 14400;
            bch_code = BCH_CODE_N12;
            break;
        case C13_45:
            kbch = 18528;
            nbch = 18720;
            bch_code = BCH_CODE_N12;
            break;
        case C9_20:
            kbch = 28968;
            nbch = 29160;
            bch_code = BCH_CODE_N12;
            break;
        case C90_180:
            kbch = 32208;
            nbch = 32400;
            bch_code = BCH_CODE_N12;
            break;
        case C96_180:
            kbch = 34368;
            nbch = 34560;
            bch_code = BCH_CODE_N12;
            break;
        case C11_20:
            kbch = 35448;
            nbch = 35640;
            bch_code = BCH_CODE_N12;
            break;
        case C100_180:
            kbch = 35808;
            nbch = 36000;
            bch_code = BCH_CODE_N12;
            break;
        case C104_180:
            kbch = 37248;
            nbch = 37440;
            bch_code = BCH_CODE_N12;
            break;
        case C26_45:
            kbch = 37248;
            nbch = 37440;
            bch_code = BCH_CODE_N12;
            break;
        case C18_30:
            kbch = 38688;
            nbch = 38880;
            bch_code = BCH_CODE_N12;
            break;
        case C28_45:
            kbch = 40128;
            nbch = 40320;
            bch_code = BCH_CODE_N12;
            break;
        case C23_36:
            kbch = 41208;
            nbch = 41400;
            bch_code = BCH_CODE_N12;
            break;
        case C116_180:
            kbch = 41568;
            nbch = 41760;
            bch_code = BCH_CODE_N12;
            break;
        case C20_30:
            kbch = 43008;
            nbch = 43200;
            bch_code = BCH_CODE_N12;
            break;
        case C124_180:
            kbch = 44448;
            nbch = 44640;
            bch_code = BCH_CODE_N12;
            break;
        case C25_36:
            kbch = 44808;
            nbch = 45000;
            bch_code = BCH_CODE_N12;
            break;
        case C128_180:
            kbch = 45888;
            nbch = 46080;
            bch_code = BCH_CODE_N12;
            break;
        case C13_18:
            kbch = 46608;
            nbch = 46800;
            bch_code = BCH_CODE_N12;
            break;
        case C132_180:
            kbch = 47328;
            nbch = 47520;
            bch_code = BCH_CODE_N12;
            break;
        case C22_30:
            kbch = 47328;
            nbch = 47520;
            bch_code = BCH_CODE_N12;
            break;
        case C135_180:
            kbch = 48408;
            nbch = 48600;
            bch_code = BCH_CODE_N12;
            break;
        case C140_180:
            kbch = 50208;
            nbch = 50400;
            bch_code = BCH_CODE_N12;
            break;
        case C7_9:
            kbch = 50208;
            nbch = 50400;
            bch_code = BCH_CODE_N12;
            break;
        case C154_180:
            kbch = 55248;
            nbch = 55440;
            bch_code = BCH_CODE_N12;
            break;
        default:
            kbch = 0;
            nbch = 0;
            bch_code = 0;
            break;
        }
    } else if (framesize == FECFRAME_SHORT) {
        switch (rate) {
        case C1_4:
            kbch = 3072;
            nbch = 3240;
            bch_code = BCH_CODE_S12;
            break;
        case C1_3:
            kbch = 5232;
            nbch = 5400;
            bch_code = BCH_CODE_S12;
            break;
        case C2_5:
            kbch = 6312;
            nbch = 6480;
            bch_code = BCH_CODE_S12;
            break;
        case C1_2:
            kbch = 7032;
            nbch = 7200;
            bch_code = BCH_CODE_S12;
            break;
        case C3_5:
            kbch = 9552;
            nbch = 9720;
            bch_code = BCH_CODE_S12;
            break;
        case C2_3:
            kbch = 10632;
            nbch = 10800;
            bch_code = BCH_CODE_S12;
            break;
        case C3_4:
            kbch = 11712;
            nbch = 11880;
            bch_code = BCH_CODE_S12;
            break;
        case C4_5:
            kbch = 12432;
            nbch = 12600;
            bch_code = BCH_CODE_S12;
            break;
        case C5_6:
            kbch = 13152;
            nbch = 13320;
            bch_code = BCH_CODE_S12;
            break;
        case C8_9:
            kbch = 14232;
            nbch = 14400;
            bch_code = BCH_CODE_S12;
            break;
        case C11_45:
            kbch = 3792;
            nbch = 3960;
            bch_code = BCH_CODE_S12;
            break;
        case C4_15:
            kbch = 4152;
            nbch = 4320;
            bch_code = BCH_CODE_S12;
            break;
        case C14_45:
            kbch = 4872;
            nbch = 5040;
            bch_code = BCH_CODE_S12;
            break;
        case C7_15:
            kbch = 7392;
            nbch = 7560;
            bch_code = BCH_CODE_S12;
            break;
        case C8_15:
            kbch = 8472;
            nbch = 8640;
            bch_code = BCH_CODE_S12;
            break;
        case C26_45:
            kbch = 9192;
            nbch = 9360;
            bch_code = BCH_CODE_S12;
            break;
        case C32_45:
            kbch = 11352;
            nbch = 11520;
            bch_code = BCH_CODE_S12;
            break;
        case C1_5_VLSNR_SF2:
            kbch = 2512;
            nbch = 2680;
            bch_code = BCH_CODE_S12;
            break;
        case C11_45_VLSNR_SF2:
            kbch = 3792;
            nbch = 3960;
            bch_code = BCH_CODE_S12;
            break;
        case C1_5_VLSNR:
            kbch = 3072;
            nbch = 3240;
            bch_code = BCH_CODE_S12;
            break;
        case C4_15_VLSNR:
            kbch = 4152;
            nbch = 4320;
            bch_code = BCH_CODE_S12;
            break;
        case C1_3_VLSNR:
            kbch = 5232;
            nbch = 5400;
            bch_code = BCH_CODE_S12;
            break;
        default:
            kbch = 0;
            nbch = 0;
            bch_code = 0;
            break;
        }
    } else {
        switch (rate) {
        case C1_5_MEDIUM:
            kbch = 5660;
            nbch = 5840;
            bch_code = BCH_CODE_M12;
            break;
        case C11_45_MEDIUM:
            kbch = 7740;
            nbch = 7920;
            bch_code = BCH_CODE_M12;
            break;
        case C1_3_MEDIUM:
            kbch = 10620;
            nbch = 10800;
            bch_code = BCH_CODE_M12;
            break;
        default:
            kbch = 0;
            nbch = 0;
            bch_code = 0;
            break;
        }
    }
    instance_n = new GF_NORMAL();
    instance_m = new GF_MEDIUM();
    instance_s = new GF_SHORT();
    decode_n_12 = new BCH_NORMAL_12();
    decode_n_10 = new BCH_NORMAL_10();
    decode_n_8 = new BCH_NORMAL_8();
    decode_m_12 = new BCH_MEDIUM_12();
    decode_s_12 = new BCH_SHORT_12();
    code = new uint8_t[8192];
    parity = new uint8_t[24];
    for (int i = 0; i < 8192; i++) {
        code[i] = 0;
    }
    for (int i = 0; i < 24; i++) {
        parity[i] = 0;
    }
    output_mode = outputmode;
    if (outputmode == OM_MESSAGE) {
        set_output_multiple(kbch);
    } else {
        set_output_multiple(nbch);
    }
}

/*
 * Our virtual destructor.
 */
bch_decoder_bb_impl::~bch_decoder_bb_impl()
{
    delete[] parity;
    delete[] code;
    delete decode_s_12;
    delete decode_m_12;
    delete decode_n_8;
    delete decode_n_10;
    delete decode_n_12;
    delete instance_s;
    delete instance_m;
    delete instance_n;
}

void bch_decoder_bb_impl::forecast(int noutput_items,
                                   gr_vector_int& ninput_items_required)
{
    if (output_mode == OM_MESSAGE) {
        ninput_items_required[0] = (noutput_items / kbch) * nbch;
    } else {
        ninput_items_required[0] = noutput_items;
    }
}

int bch_decoder_bb_impl::general_work(int noutput_items,
                                      gr_vector_int& ninput_items,
                                      gr_vector_const_void_star& input_items,
                                      gr_vector_void_star& output_items)
{
    const unsigned char* in = (const unsigned char*)input_items[0];
    unsigned char* out = (unsigned char*)output_items[0];
    int corrections = 0;
    int consumed = 0;
    unsigned int output_size = output_mode ? kbch : nbch;

    for (int i = 0; i < noutput_items; i += output_size) {
        for (unsigned int j = 0; j < kbch; j++) {
            CODE::set_be_bit(code, j, *in++);
        }
        for (unsigned int j = 0; j < nbch - kbch; j++) {
            CODE::set_be_bit(parity, j, *in++);
        }
        switch (bch_code) {
        case BCH_CODE_N12:
            corrections = (*decode_n_12)(code, parity, 0, 0, kbch);
            break;
        case BCH_CODE_N10:
            corrections = (*decode_n_10)(code, parity, 0, 0, kbch);
            break;
        case BCH_CODE_N8:
            corrections = (*decode_n_8)(code, parity, 0, 0, kbch);
            break;
        case BCH_CODE_S12:
            corrections = (*decode_s_12)(code, parity, 0, 0, kbch);
            break;
        case BCH_CODE_M12:
            corrections = (*decode_m_12)(code, parity, 0, 0, kbch);
            break;
        }
        if (corrections > 0) {
            printf(
                "frame = %lu, BCH decoder corrections = %d\n", d_frame_cnt, corrections);
        } else if (corrections == -1) {
            d_frame_error_cnt++;
            printf("frame = %lu, BCH decoder too many bit errors (FER = %g)\n",
                   d_frame_cnt,
                   (double)d_frame_error_cnt / (d_frame_cnt + 1));
        }
        d_frame_cnt++;
        for (unsigned int j = 0; j < kbch; j++) {
            *out++ = CODE::get_be_bit(code, j);
        }
        if (output_size == nbch) {
            for (unsigned int j = 0; j < nbch - kbch; j++) {
                *out++ = CODE::get_be_bit(parity, j);
            }
        }
        consumed += nbch;
    }

    // each input stream.
    consume_each(consumed);

    // Tell runtime system how many output items we produced.
    return noutput_items;
}

} /* namespace dvbs2rx */
} /* namespace gr */
