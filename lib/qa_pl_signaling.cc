/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pi2_bpsk.h"
#include "pl_signaling.h"
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

namespace bdata = boost::unit_test::data;

namespace gr {
namespace dvbs2rx {

BOOST_AUTO_TEST_CASE(test_plsc_encode)
{

    // Encode the PLSC=0 dataword such that the resulting scrambled codeword
    // corresponds to the PLSC scrambler sequence (since 0 + scrambler =
    // scrambler)
    std::vector<gr_complex> expected_bpsk_syms(PLSC_LEN);
    map_bpsk(plsc_scrambler, expected_bpsk_syms.data(), PLSC_LEN);

    std::vector<gr_complex> test_bpsk_syms(PLSC_LEN);
    plsc_encoder encoder;
    encoder.encode(test_bpsk_syms.data(), 0);

    BOOST_CHECK_EQUAL_COLLECTIONS(test_bpsk_syms.begin(),
                                  test_bpsk_syms.end(),
                                  expected_bpsk_syms.begin(),
                                  expected_bpsk_syms.end());
}

BOOST_DATA_TEST_CASE(test_plsc_decode,
                     bdata::make({ false, true }) * bdata::make({ false, true }),
                     coherent,
                     soft)
{
    // Assume the input pi/2 BPSK symbols correspond to the all-zeros PLSC,
    // whose scrambled version is identical to the scrambler sequence (again,
    // because "0 + scrambler = scrambler"). Add the last SOF symbol in the
    // beginning to allow for differential (non-coherent) decoding.
    std::vector<gr_complex> in_symbols(PLSC_LEN + 1);
    in_symbols[0] = { -SQRT2_2, +SQRT2_2 };     // last SOF symbol
    gr_complex* p_plsc = in_symbols.data() + 1; // where the PLSC starts
    map_bpsk(plsc_scrambler, p_plsc, PLSC_LEN);

    // Decode and check that the original PLSC corresponds to the all-zeros
    // dataword, namely modcod=0, fecframe=normal, and pilots=0.
    plsc_decoder decoder;
    pls_info_t info;
    decoder.decode(in_symbols.data(), coherent, soft);
    decoder.get_info(&info);
    BOOST_CHECK_EQUAL(info.modcod, 0);
    BOOST_CHECK_EQUAL(info.short_fecframe, false);
    BOOST_CHECK_EQUAL(info.has_pilots, false);
}

BOOST_AUTO_TEST_CASE(test_plsc_round_trip)
{
    // Encode and decode all possible datawords
    plsc_encoder encoder;
    plsc_decoder decoder;
    pls_info_t info;
    std::vector<gr_complex> bpsk_syms(PLSC_LEN + 1);
    for (uint8_t i = 0; i < n_plsc_codewords; i++) {
        encoder.encode(bpsk_syms.data() + 1, i);
        decoder.decode(bpsk_syms.data());
        decoder.get_info(&info);
        BOOST_CHECK_EQUAL(info.plsc, i);
    }
}

BOOST_AUTO_TEST_CASE(test_plsc_parsing)
{
    plsc_encoder encoder;
    plsc_decoder decoder;
    pls_info_t info;
    std::vector<gr_complex> bpsk_syms(PLSC_LEN + 1);

    // Encode and decode all possible PLSC values, except modcod=0 (dummy
    // frame), which is an exceptional case that does not support pilots.
    for (uint8_t modcod = 1; modcod < 32; modcod++) {
        for (bool short_fecframe : { false, true }) {
            for (bool has_pilots : { false, true }) {
                encoder.encode(bpsk_syms.data() + 1, modcod, short_fecframe, has_pilots);
                decoder.decode(bpsk_syms.data());
                decoder.get_info(&info);
                BOOST_CHECK_EQUAL(info.modcod, modcod);
                BOOST_CHECK_EQUAL(info.short_fecframe, short_fecframe);
                BOOST_CHECK_EQUAL(info.has_pilots, has_pilots);
            }
        }
    }

    // Check the dummy frame case
    uint8_t modcod = 0;
    for (bool short_fecframe : { false, true }) {
        for (bool has_pilots : { false, true }) {
            encoder.encode(bpsk_syms.data() + 1, modcod, short_fecframe, has_pilots);
            decoder.decode(bpsk_syms.data());
            decoder.get_info(&info);
            BOOST_CHECK_EQUAL(info.modcod, modcod);
            BOOST_CHECK_EQUAL(info.short_fecframe, short_fecframe);
            BOOST_CHECK_EQUAL(info.has_pilots, false); // false regardless
        }
    }
}

} // namespace dvbs2rx
} // namespace gr
