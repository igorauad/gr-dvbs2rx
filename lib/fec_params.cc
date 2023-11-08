/* -*- c++ -*- */
/*
 * Copyright 2023 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "fec_params.h"
#include <stdexcept>

namespace gr {
namespace dvbs2rx {

void get_fec_info(dvb_standard_t standard,
                  dvb_framesize_t framesize,
                  dvb_code_rate_t rate,
                  fec_info_t& fec_info)
{
    if (framesize == FECFRAME_NORMAL) {
        fec_info.ldpc.n = 64800;
        switch (rate) {
        case C1_4:
            fec_info.bch.k = 16008;
            fec_info.bch.n = 16200;
            fec_info.bch.t = 12;
            break;
        case C1_3:
            fec_info.bch.k = 21408;
            fec_info.bch.n = 21600;
            fec_info.bch.t = 12;
            break;
        case C2_5:
            fec_info.bch.k = 25728;
            fec_info.bch.n = 25920;
            fec_info.bch.t = 12;
            break;
        case C1_2:
            fec_info.bch.k = 32208;
            fec_info.bch.n = 32400;
            fec_info.bch.t = 12;
            break;
        case C3_5:
            fec_info.bch.k = 38688;
            fec_info.bch.n = 38880;
            fec_info.bch.t = 12;
            break;
        case C2_3:
            fec_info.bch.k = 43040;
            fec_info.bch.n = 43200;
            fec_info.bch.t = 10;
            break;
        case C3_4:
            fec_info.bch.k = 48408;
            fec_info.bch.n = 48600;
            fec_info.bch.t = 12;
            break;
        case C4_5:
            fec_info.bch.k = 51648;
            fec_info.bch.n = 51840;
            fec_info.bch.t = 12;
            break;
        case C5_6:
            fec_info.bch.k = 53840;
            fec_info.bch.n = 54000;
            fec_info.bch.t = 10;
            break;
        case C8_9:
            fec_info.bch.k = 57472;
            fec_info.bch.n = 57600;
            fec_info.bch.t = 8;
            break;
        case C9_10:
            fec_info.bch.k = 58192;
            fec_info.bch.n = 58320;
            fec_info.bch.t = 8;
            break;
        case C2_9_VLSNR:
            fec_info.bch.k = 14208;
            fec_info.bch.n = 14400;
            fec_info.bch.t = 12;
            break;
        case C13_45:
            fec_info.bch.k = 18528;
            fec_info.bch.n = 18720;
            fec_info.bch.t = 12;
            break;
        case C9_20:
            fec_info.bch.k = 28968;
            fec_info.bch.n = 29160;
            fec_info.bch.t = 12;
            break;
        case C90_180:
            fec_info.bch.k = 32208;
            fec_info.bch.n = 32400;
            fec_info.bch.t = 12;
            break;
        case C96_180:
            fec_info.bch.k = 34368;
            fec_info.bch.n = 34560;
            fec_info.bch.t = 12;
            break;
        case C11_20:
            fec_info.bch.k = 35448;
            fec_info.bch.n = 35640;
            fec_info.bch.t = 12;
            break;
        case C100_180:
            fec_info.bch.k = 35808;
            fec_info.bch.n = 36000;
            fec_info.bch.t = 12;
            break;
        case C104_180:
            fec_info.bch.k = 37248;
            fec_info.bch.n = 37440;
            fec_info.bch.t = 12;
            break;
        case C26_45:
            fec_info.bch.k = 37248;
            fec_info.bch.n = 37440;
            fec_info.bch.t = 12;
            break;
        case C18_30:
            fec_info.bch.k = 38688;
            fec_info.bch.n = 38880;
            fec_info.bch.t = 12;
            break;
        case C28_45:
            fec_info.bch.k = 40128;
            fec_info.bch.n = 40320;
            fec_info.bch.t = 12;
            break;
        case C23_36:
            fec_info.bch.k = 41208;
            fec_info.bch.n = 41400;
            fec_info.bch.t = 12;
            break;
        case C116_180:
            fec_info.bch.k = 41568;
            fec_info.bch.n = 41760;
            fec_info.bch.t = 12;
            break;
        case C20_30:
            fec_info.bch.k = 43008;
            fec_info.bch.n = 43200;
            fec_info.bch.t = 12;
            break;
        case C124_180:
            fec_info.bch.k = 44448;
            fec_info.bch.n = 44640;
            fec_info.bch.t = 12;
            break;
        case C25_36:
            fec_info.bch.k = 44808;
            fec_info.bch.n = 45000;
            fec_info.bch.t = 12;
            break;
        case C128_180:
            fec_info.bch.k = 45888;
            fec_info.bch.n = 46080;
            fec_info.bch.t = 12;
            break;
        case C13_18:
            fec_info.bch.k = 46608;
            fec_info.bch.n = 46800;
            fec_info.bch.t = 12;
            break;
        case C132_180:
            fec_info.bch.k = 47328;
            fec_info.bch.n = 47520;
            fec_info.bch.t = 12;
            break;
        case C22_30:
            fec_info.bch.k = 47328;
            fec_info.bch.n = 47520;
            fec_info.bch.t = 12;
            break;
        case C135_180:
            fec_info.bch.k = 48408;
            fec_info.bch.n = 48600;
            fec_info.bch.t = 12;
            break;
        case C140_180:
            fec_info.bch.k = 50208;
            fec_info.bch.n = 50400;
            fec_info.bch.t = 12;
            break;
        case C7_9:
            fec_info.bch.k = 50208;
            fec_info.bch.n = 50400;
            fec_info.bch.t = 12;
            break;
        case C154_180:
            fec_info.bch.k = 55248;
            fec_info.bch.n = 55440;
            fec_info.bch.t = 12;
            break;
        default:
            std::runtime_error("Unsupported code rate");
            break;
        }
    } else if (framesize == FECFRAME_SHORT) {
        fec_info.ldpc.n = 16200;
        switch (rate) {
        case C1_4:
            fec_info.bch.k = 3072;
            fec_info.bch.n = 3240;
            fec_info.bch.t = 12;
            break;
        case C1_3:
            fec_info.bch.k = 5232;
            fec_info.bch.n = 5400;
            fec_info.bch.t = 12;
            break;
        case C2_5:
            fec_info.bch.k = 6312;
            fec_info.bch.n = 6480;
            fec_info.bch.t = 12;
            break;
        case C1_2:
            fec_info.bch.k = 7032;
            fec_info.bch.n = 7200;
            fec_info.bch.t = 12;
            break;
        case C3_5:
            fec_info.bch.k = 9552;
            fec_info.bch.n = 9720;
            fec_info.bch.t = 12;
            break;
        case C2_3:
            fec_info.bch.k = 10632;
            fec_info.bch.n = 10800;
            fec_info.bch.t = 12;
            break;
        case C3_4:
            fec_info.bch.k = 11712;
            fec_info.bch.n = 11880;
            fec_info.bch.t = 12;
            break;
        case C4_5:
            fec_info.bch.k = 12432;
            fec_info.bch.n = 12600;
            fec_info.bch.t = 12;
            break;
        case C5_6:
            fec_info.bch.k = 13152;
            fec_info.bch.n = 13320;
            fec_info.bch.t = 12;
            break;
        case C8_9:
            fec_info.bch.k = 14232;
            fec_info.bch.n = 14400;
            fec_info.bch.t = 12;
            break;
        case C11_45:
            fec_info.bch.k = 3792;
            fec_info.bch.n = 3960;
            fec_info.bch.t = 12;
            break;
        case C4_15:
            fec_info.bch.k = 4152;
            fec_info.bch.n = 4320;
            fec_info.bch.t = 12;
            break;
        case C14_45:
            fec_info.bch.k = 4872;
            fec_info.bch.n = 5040;
            fec_info.bch.t = 12;
            break;
        case C7_15:
            fec_info.bch.k = 7392;
            fec_info.bch.n = 7560;
            fec_info.bch.t = 12;
            break;
        case C8_15:
            fec_info.bch.k = 8472;
            fec_info.bch.n = 8640;
            fec_info.bch.t = 12;
            break;
        case C26_45:
            fec_info.bch.k = 9192;
            fec_info.bch.n = 9360;
            fec_info.bch.t = 12;
            break;
        case C32_45:
            fec_info.bch.k = 11352;
            fec_info.bch.n = 11520;
            fec_info.bch.t = 12;
            break;
        case C1_5_VLSNR_SF2:
            fec_info.bch.k = 2512;
            fec_info.bch.n = 2680;
            fec_info.bch.t = 12;
            break;
        case C11_45_VLSNR_SF2:
            fec_info.bch.k = 3792;
            fec_info.bch.n = 3960;
            fec_info.bch.t = 12;
            break;
        case C1_5_VLSNR:
            fec_info.bch.k = 3072;
            fec_info.bch.n = 3240;
            fec_info.bch.t = 12;
            break;
        case C4_15_VLSNR:
            fec_info.bch.k = 4152;
            fec_info.bch.n = 4320;
            fec_info.bch.t = 12;
            break;
        case C1_3_VLSNR:
            fec_info.bch.k = 5232;
            fec_info.bch.n = 5400;
            fec_info.bch.t = 12;
            break;
        default:
            std::runtime_error("Unsupported code rate");
            break;
        }
    } else {
        fec_info.ldpc.n = 32400;
        switch (rate) {
        case C1_5_MEDIUM:
            fec_info.bch.k = 5660;
            fec_info.bch.n = 5840;
            fec_info.bch.t = 12;
            break;
        case C11_45_MEDIUM:
            fec_info.bch.k = 7740;
            fec_info.bch.n = 7920;
            fec_info.bch.t = 12;
            break;
        case C1_3_MEDIUM:
            fec_info.bch.k = 10620;
            fec_info.bch.n = 10800;
            fec_info.bch.t = 12;
            break;
        default:
            std::runtime_error("Unsupported code rate");
            break;
        }
    }
    fec_info.ldpc.k = fec_info.bch.n;
}

} // namespace dvbs2rx
} // namespace gr