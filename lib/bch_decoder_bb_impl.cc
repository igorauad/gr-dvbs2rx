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
#include "debug_level.h"
#include <gnuradio/io_signature.h>
#include <gnuradio/logger.h>
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
                                          dvb_outputmode_t outputmode,
                                          int debug_level)
{
    return gnuradio::get_initial_sptr(
        new bch_decoder_bb_impl(standard, framesize, rate, outputmode, debug_level));
}

/*
 * The private constructor
 */
bch_decoder_bb_impl::bch_decoder_bb_impl(dvb_standard_t standard,
                                         dvb_framesize_t framesize,
                                         dvb_code_rate_t rate,
                                         dvb_outputmode_t outputmode,
                                         int debug_level)
    : gr::block("bch_decoder_bb",
                gr::io_signature::make(1, 1, sizeof(unsigned char)),
                gr::io_signature::make(1, 1, sizeof(unsigned char))),
      d_debug_level(debug_level),
      d_frame_cnt(0),
      d_frame_error_cnt(0)
{
    uint32_t prim_poly;
    uint32_t nbch, kbch;
    uint8_t t;
    if (framesize == FECFRAME_NORMAL) {
        prim_poly = 0b10000000000101101; // x^16 + x^5 + x^3 + x^2 + 1
        switch (rate) {
        case C1_4:
            kbch = 16008;
            nbch = 16200;
            t = 12;
            break;
        case C1_3:
            kbch = 21408;
            nbch = 21600;
            t = 12;
            break;
        case C2_5:
            kbch = 25728;
            nbch = 25920;
            t = 12;
            break;
        case C1_2:
            kbch = 32208;
            nbch = 32400;
            t = 12;
            break;
        case C3_5:
            kbch = 38688;
            nbch = 38880;
            t = 12;
            break;
        case C2_3:
            kbch = 43040;
            nbch = 43200;
            t = 10;
            break;
        case C3_4:
            kbch = 48408;
            nbch = 48600;
            t = 12;
            break;
        case C4_5:
            kbch = 51648;
            nbch = 51840;
            t = 12;
            break;
        case C5_6:
            kbch = 53840;
            nbch = 54000;
            t = 10;
            break;
        case C8_9:
            kbch = 57472;
            nbch = 57600;
            t = 8;
            break;
        case C9_10:
            kbch = 58192;
            nbch = 58320;
            t = 8;
            break;
        case C2_9_VLSNR:
            kbch = 14208;
            nbch = 14400;
            t = 12;
            break;
        case C13_45:
            kbch = 18528;
            nbch = 18720;
            t = 12;
            break;
        case C9_20:
            kbch = 28968;
            nbch = 29160;
            t = 12;
            break;
        case C90_180:
            kbch = 32208;
            nbch = 32400;
            t = 12;
            break;
        case C96_180:
            kbch = 34368;
            nbch = 34560;
            t = 12;
            break;
        case C11_20:
            kbch = 35448;
            nbch = 35640;
            t = 12;
            break;
        case C100_180:
            kbch = 35808;
            nbch = 36000;
            t = 12;
            break;
        case C104_180:
            kbch = 37248;
            nbch = 37440;
            t = 12;
            break;
        case C26_45:
            kbch = 37248;
            nbch = 37440;
            t = 12;
            break;
        case C18_30:
            kbch = 38688;
            nbch = 38880;
            t = 12;
            break;
        case C28_45:
            kbch = 40128;
            nbch = 40320;
            t = 12;
            break;
        case C23_36:
            kbch = 41208;
            nbch = 41400;
            t = 12;
            break;
        case C116_180:
            kbch = 41568;
            nbch = 41760;
            t = 12;
            break;
        case C20_30:
            kbch = 43008;
            nbch = 43200;
            t = 12;
            break;
        case C124_180:
            kbch = 44448;
            nbch = 44640;
            t = 12;
            break;
        case C25_36:
            kbch = 44808;
            nbch = 45000;
            t = 12;
            break;
        case C128_180:
            kbch = 45888;
            nbch = 46080;
            t = 12;
            break;
        case C13_18:
            kbch = 46608;
            nbch = 46800;
            t = 12;
            break;
        case C132_180:
            kbch = 47328;
            nbch = 47520;
            t = 12;
            break;
        case C22_30:
            kbch = 47328;
            nbch = 47520;
            t = 12;
            break;
        case C135_180:
            kbch = 48408;
            nbch = 48600;
            t = 12;
            break;
        case C140_180:
            kbch = 50208;
            nbch = 50400;
            t = 12;
            break;
        case C7_9:
            kbch = 50208;
            nbch = 50400;
            t = 12;
            break;
        case C154_180:
            kbch = 55248;
            nbch = 55440;
            t = 12;
            break;
        default:
            std::runtime_error("Unsupported code rate");
            break;
        }
    } else if (framesize == FECFRAME_SHORT) {
        prim_poly = 0b100000000101011; // x^14 + x^5 + x^3 + x + 1
        switch (rate) {
        case C1_4:
            kbch = 3072;
            nbch = 3240;
            t = 12;
            break;
        case C1_3:
            kbch = 5232;
            nbch = 5400;
            t = 12;
            break;
        case C2_5:
            kbch = 6312;
            nbch = 6480;
            t = 12;
            break;
        case C1_2:
            kbch = 7032;
            nbch = 7200;
            t = 12;
            break;
        case C3_5:
            kbch = 9552;
            nbch = 9720;
            t = 12;
            break;
        case C2_3:
            kbch = 10632;
            nbch = 10800;
            t = 12;
            break;
        case C3_4:
            kbch = 11712;
            nbch = 11880;
            t = 12;
            break;
        case C4_5:
            kbch = 12432;
            nbch = 12600;
            t = 12;
            break;
        case C5_6:
            kbch = 13152;
            nbch = 13320;
            t = 12;
            break;
        case C8_9:
            kbch = 14232;
            nbch = 14400;
            t = 12;
            break;
        case C11_45:
            kbch = 3792;
            nbch = 3960;
            t = 12;
            break;
        case C4_15:
            kbch = 4152;
            nbch = 4320;
            t = 12;
            break;
        case C14_45:
            kbch = 4872;
            nbch = 5040;
            t = 12;
            break;
        case C7_15:
            kbch = 7392;
            nbch = 7560;
            t = 12;
            break;
        case C8_15:
            kbch = 8472;
            nbch = 8640;
            t = 12;
            break;
        case C26_45:
            kbch = 9192;
            nbch = 9360;
            t = 12;
            break;
        case C32_45:
            kbch = 11352;
            nbch = 11520;
            t = 12;
            break;
        case C1_5_VLSNR_SF2:
            kbch = 2512;
            nbch = 2680;
            t = 12;
            break;
        case C11_45_VLSNR_SF2:
            kbch = 3792;
            nbch = 3960;
            t = 12;
            break;
        case C1_5_VLSNR:
            kbch = 3072;
            nbch = 3240;
            t = 12;
            break;
        case C4_15_VLSNR:
            kbch = 4152;
            nbch = 4320;
            t = 12;
            break;
        case C1_3_VLSNR:
            kbch = 5232;
            nbch = 5400;
            t = 12;
            break;
        default:
            std::runtime_error("Unsupported code rate");
            break;
        }
    } else {
        prim_poly = 0b1000000000101101; // x^15 + x^5 + x^3 + x^2 + 1
        switch (rate) {
        case C1_5_MEDIUM:
            kbch = 5660;
            nbch = 5840;
            t = 12;
            break;
        case C11_45_MEDIUM:
            kbch = 7740;
            nbch = 7920;
            t = 12;
            break;
        case C1_3_MEDIUM:
            kbch = 10620;
            nbch = 10800;
            t = 12;
            break;
        default:
            std::runtime_error("Unsupported code rate");
            break;
        }
    }
    d_gf = std::make_unique<galois_field<uint32_t>>(prim_poly);
    d_codec = std::make_unique<bch_codec<uint32_t, bitset256_t>>(d_gf.get(), t, nbch);
    d_k_bytes = kbch / 8;
    d_n_bytes = nbch / 8;
    set_output_multiple(d_k_bytes);
    set_relative_rate((double)kbch / nbch);
}

/*
 * Our virtual destructor.
 */
bch_decoder_bb_impl::~bch_decoder_bb_impl() {}

void bch_decoder_bb_impl::forecast(int noutput_items,
                                   gr_vector_int& ninput_items_required)
{
    ninput_items_required[0] = (noutput_items / d_k_bytes) * d_n_bytes;
}

int bch_decoder_bb_impl::general_work(int noutput_items,
                                      gr_vector_int& ninput_items,
                                      gr_vector_const_void_star& input_items,
                                      gr_vector_void_star& output_items)
{
    const unsigned char* in = (const unsigned char*)input_items[0];
    unsigned char* out = (unsigned char*)output_items[0];

    int consumed = 0;
    int n_codewords = noutput_items / d_k_bytes;
    for (int i = 0; i < n_codewords; i++) {
        const int corrections = d_codec->decode(in, out);
        if (corrections > 0) {
            GR_LOG_DEBUG_LEVEL(1,
                               "frame = {:d}, BCH decoder corrections = {:d}",
                               d_frame_cnt,
                               corrections);
        } else if (corrections == -1) {
            d_frame_error_cnt++;
            GR_LOG_DEBUG_LEVEL(
                1,
                "frame = {:d}, BCH decoder too many bit errors (FER = {:g})",
                d_frame_cnt,
                ((double)d_frame_error_cnt / (d_frame_cnt + 1)));
        }
        d_frame_cnt++;
        in += d_n_bytes;
        out += d_k_bytes;
        consumed += d_n_bytes;
    }

    consume_each(consumed);
    return noutput_items;
}

} /* namespace dvbs2rx */
} /* namespace gr */
