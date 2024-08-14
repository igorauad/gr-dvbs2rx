/* -*- c++ -*- */
/*
 * Copyright 2024 AsriFox.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gnuradio/dvbs2rx/dvb_params.h>
#include <gnuradio/io_signature.h>
#include <boost/algorithm/string.hpp>

namespace gr {
namespace dvbs2rx {

/***************************************************************************************/
/* Translation */

dvb_standard_t parse_standard(std::string standard)
{
    boost::algorithm::to_upper(standard);
    if (standard == "DVB-T2") {
        return STANDARD_DVBT2;
    }
    if (standard == "DVB-S2" || standard == "DVB-S2X") {
        return STANDARD_DVBS2;
    }
    throw std::invalid_argument("Unknown standard: " + standard);
}

dvb_framesize_t parse_framesize(std::string frame_size)
{
    boost::algorithm::to_lower(frame_size);
    if (frame_size == "normal") {
        return FECFRAME_NORMAL;
    }
    if (frame_size == "short") {
        return FECFRAME_SHORT;
    }
    if (frame_size == "medium") {
        return FECFRAME_MEDIUM;
    }
    throw std::invalid_argument("Unknown framesize: " + frame_size);
}

dvb_code_rate_t parse_rate(std::string code_rate)
{
    static const std::map<std::string, dvb_code_rate_t> code_rates = {
        { "1/4", C1_4 },         { "1/3", C1_3 },         { "2/5", C2_5 },
        { "1/2", C1_2 },         { "3/5", C3_5 },         { "2/3", C2_3 },
        { "3/4", C3_4 },         { "4/5", C4_5 },         { "5/6", C5_6 },
        { "7/8", C7_8 },         { "8/9", C8_9 },         { "9/10", C9_10 },
        { "13/45", C13_45 },     { "9/20", C9_20 },       { "90/180", C90_180 },
        { "96/180", C96_180 },   { "11/20", C11_20 },     { "100/180", C100_180 },
        { "104/180", C104_180 }, { "26/45", C26_45 },     { "18/30", C18_30 },
        { "28/45", C28_45 },     { "23/36", C23_36 },     { "116/180", C116_180 },
        { "20/30", C20_30 },     { "124/180", C124_180 }, { "25/36", C25_36 },
        { "128/180", C128_180 }, { "13/18", C13_18 },     { "132/180", C132_180 },
        { "22/30", C22_30 },     { "135/180", C135_180 }, { "140/180", C140_180 },
        { "7/9", C7_9 },         { "154/180", C154_180 }, { "11/45", C11_45 },
        { "4/15", C4_15 },       { "14/45", C14_45 },     { "7/15", C7_15 },
        { "8/15", C8_15 },       { "32/45", C32_45 },
    };
    auto rate = code_rates.find(code_rate);
    if (rate == code_rates.cend()) {
        throw std::invalid_argument("Unknown code rate: " + code_rate);
    }
    return rate->second;
}

dvb_code_rate_t parse_rate_vlsnr(std::string code_rate, dvb_framesize_t framesize)
{
    switch (framesize) {
    case FECFRAME_NORMAL:
        if (code_rate == "2/9")
            return C2_9_VLSNR;
        break;
    case FECFRAME_SHORT:
        if (code_rate == "1/5")
            return C1_5_VLSNR_SF2;
        if (code_rate == "11/45")
            return C11_45_VLSNR_SF2;
        if (code_rate == "4/15")
            return C4_15_VLSNR;
        if (code_rate == "1/3")
            return C1_3_VLSNR;
        break;
    case FECFRAME_MEDIUM:
        if (code_rate == "1/5")
            return C1_5_MEDIUM;
        if (code_rate == "11/45")
            return C11_45_MEDIUM;
        if (code_rate == "1/3")
            return C1_3_MEDIUM;
        break;
    }
    throw std::invalid_argument("Unknown code rate: " + code_rate);
}

dvb_constellation_t parse_constellation(std::string constellation)
{
    // clang-format off
    static const std::map<std::string, dvb_constellation_t> constellations = {
        { "QPSK", MOD_QPSK },
        { "16QAM", MOD_16QAM },
        { "64QAM", MOD_64QAM },
        { "256QAM", MOD_256QAM },
        { "8PSK", MOD_8PSK },
        // { "8APSK", MOD_8APSK },
        // { "16APSK", MOD_16APSK },
        // { "8_8APSK", MOD_8_8APSK },
        // { "32APSK", MOD_32APSK },
        // { "4_12_16APSK", MOD_4_12_16APSK },
        // { "4_8_4_16APSK", MOD_4_8_4_16APSK },
        // { "64APSK", MOD_64APSK },
        // { "8_16_20_20APSK", MOD_8_16_20_20APSK },
        // { "4_12_20_28APSK", MOD_4_12_20_28APSK },
        // { "128APSK", MOD_128APSK },
        // { "256APSK", MOD_256APSK },
        // { "BPSK", MOD_BPSK },
        // { "BPSK_SF2", MOD_BPSK_SF2 },
        // { "8VSB", MOD_8VSB },
    };
    // clang-format on
    boost::algorithm::to_upper(constellation);
    auto mod = constellations.find(constellation);
    if (mod == constellations.cend()) {
        throw std::invalid_argument("Unknown constellation: " + constellation);
    }
    return mod->second;
}

dvbs2_rolloff_factor_t parse_rolloff(float rolloff, std::string standard)
{
    unsigned ro = round(rolloff * 100);
    if (standard.size() == 6) { // DVB-S2X
        switch (ro) {
        case 35:
            return RO_0_35;
        case 25:
            return RO_0_25;
        case 20:
            return RO_0_20;
        case 15:
            return RO_0_15;
        case 10:
            return RO_0_10;
        case 5:
            return RO_0_05;
        }
    } else if (standard.size() == 5) { // DVB-S2
        switch (ro) {
        case 35:
            return RO_0_35;
        case 25:
            return RO_0_25;
        case 20:
            return RO_0_20;
        }
    }
    throw std::invalid_argument("Unknown rolloff factor for " + standard + ": " +
                                std::to_string(rolloff));
}

dvb_params dvb_params::make(std::string standard,
                            std::string frame_size,
                            std::string code_rate,
                            std::string constellation,
                            float rolloff,
                            bool pilots,
                            bool vl_snr)
{
    auto framesize = parse_framesize(frame_size);
    auto rate = vl_snr ? parse_rate_vlsnr(code_rate, framesize) : parse_rate(code_rate);
    return dvb_params(parse_standard(standard),
                      framesize,
                      rate,
                      parse_constellation(constellation),
                      parse_rolloff(rolloff, standard),
                      pilots ? PILOTS_ON : PILOTS_OFF);
}

/***************************************************************************************/
/* Validation */

dvb_params::dvb_params(dvb_standard_t standard,
                       dvb_framesize_t framesize,
                       dvb_code_rate_t rate,
                       dvb_constellation_t constellation,
                       dvbs2_rolloff_factor_t rolloff,
                       dvbs2_pilots_t pilots)
    : standard(standard),
      framesize(framesize),
      rate(rate),
      constellation(constellation),
      rolloff(rolloff),
      pilots(pilots)
{
    // Validate constellation between DVB-S2 and DVB-T2
    if (standard == STANDARD_DVBS2) {
        switch (constellation) {
        case MOD_16QAM:
        case MOD_64QAM:
        case MOD_256QAM:
            throw std::invalid_argument("Invalid constellation for DVB-S2");
        default:
            break;
        }
    } else { // (standard == STANDARD_DVBT2)
        switch (constellation) {
        case MOD_8PSK:
            throw std::invalid_argument("Invalid constellation for DVB-T2");
        default:
            break;
        }
    }

    // Validate code rate between DVB-S2 and DVB-T2
    if (standard == STANDARD_DVBT2) {
        switch (rate) {
        case C1_4:
        case C1_3:
        case C1_3_MEDIUM:
        case C1_3_VLSNR:
        case C2_5:
        case C1_2:
        case C3_5:
        case C2_3:
        case C3_4:
        case C4_5:
        case C5_6:
            break;
        default:
            throw std::invalid_argument("Invalid code rate for DVB-T2");
        }
    }

    // Validate code rate with framesize
    switch (framesize) {
    case FECFRAME_MEDIUM:
        switch (rate) {
        case C1_5_MEDIUM:
        case C11_45_MEDIUM:
        case C1_3_MEDIUM:
            break;
        default:
            throw std::invalid_argument("Invalid code rate for medium frames");
        }
        break;
    case FECFRAME_SHORT:
        switch (rate) {
        case C9_10:
        case C2_9_VLSNR:
        case C13_45:
        case C9_20:
        case C90_180:
        case C96_180:
        case C11_20:
        case C100_180:
        case C104_180:
        case C18_30:
        case C28_45:
        case C23_36:
        case C116_180:
        case C20_30:
        case C124_180:
        case C25_36:
        case C128_180:
        case C13_18:
        case C132_180:
        case C22_30:
        case C135_180:
        case C140_180:
        case C7_9:
        case C154_180:
            throw std::invalid_argument("Invalid code rate for short frames");
        default:
            break;
        }
        break;
    case FECFRAME_NORMAL:
        switch (rate) {
        case C11_45:
        case C4_15:
        case C14_45:
        case C7_15:
        case C8_15:
        case C32_45:
        case C1_5_VLSNR_SF2:
        case C11_45_VLSNR_SF2:
        case C1_5_VLSNR:
        case C4_15_VLSNR:
        case C1_3_VLSNR:
            throw std::invalid_argument("Invalid code rate for normal frames");
        default:
            break;
        }
        break;
    }
}

dvb_params::~dvb_params() = default;

} /* namespace dvbs2rx */
} /* namespace gr */
