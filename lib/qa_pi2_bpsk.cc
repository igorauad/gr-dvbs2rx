/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pi2_bpsk.h"
#include "pl_defs.h"
#include <boost/test/unit_test.hpp>

namespace gr {
namespace dvbs2rx {

BOOST_AUTO_TEST_CASE(test_sof_map_demap)
{
    std::vector<gr_complex> sof_bpsk(SOF_LEN);
    map_bpsk(sof_big_endian, sof_bpsk.data(), SOF_LEN);

    std::vector<gr_complex> expected = {
        { SQRT2_2, SQRT2_2 },   { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, SQRT2_2 },  { SQRT2_2, SQRT2_2 },   { -SQRT2_2, SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { SQRT2_2, SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { SQRT2_2, SQRT2_2 },   { -SQRT2_2, SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { -SQRT2_2, SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { -SQRT2_2, SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { -SQRT2_2, SQRT2_2 },  { SQRT2_2, SQRT2_2 },
        { -SQRT2_2, SQRT2_2 },  { SQRT2_2, SQRT2_2 },   { -SQRT2_2, SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { -SQRT2_2, SQRT2_2 }
    };

    BOOST_CHECK_EQUAL_COLLECTIONS(
        sof_bpsk.begin(), sof_bpsk.end(), expected.begin(), expected.end());

    uint64_t demapped_sof = demap_bpsk(sof_bpsk.data(), SOF_LEN);
    BOOST_CHECK(sof_big_endian == demapped_sof);
}

BOOST_AUTO_TEST_CASE(test_pi2bpsk_to_bpsk)
{
    // Binary sequence: 0, 0, 1, 1
    std::vector<gr_complex> pi2_bpsk_syms = { { SQRT2_2, SQRT2_2 },
                                              { -SQRT2_2, SQRT2_2 },
                                              { -SQRT2_2, -SQRT2_2 },
                                              { SQRT2_2, -SQRT2_2 } };
    std::vector<float> expected_bpsk_syms = { +1, +1, -1, -1 };
    std::vector<float> out_bpsk_syms(4);
    derotate_bpsk(pi2_bpsk_syms.data(), out_bpsk_syms.data(), pi2_bpsk_syms.size());

    for (unsigned int i = 0; i < out_bpsk_syms.size(); i++)
        BOOST_CHECK_CLOSE(out_bpsk_syms[i], expected_bpsk_syms[i], 0.0001);
}

BOOST_AUTO_TEST_CASE(test_mapping_range)
{
    // Allocate 2 elements
    std::vector<gr_complex> bpsk(2);

    // But map only one
    map_bpsk(0x8000000000000000, bpsk.data(), 1);

    // Expect the second element to remain null
    std::vector<gr_complex> expected = { { -SQRT2_2, -SQRT2_2 }, 0 };

    BOOST_CHECK_EQUAL_COLLECTIONS(
        bpsk.begin(), bpsk.end(), expected.begin(), expected.end());

    // Allocate and map a full 64-bit word
    std::vector<gr_complex> bpsk2(64);
    map_bpsk(0xFFFFFFFFFFFFFFFF, bpsk2.data(), 64);
    for (const auto& x : bpsk2) {
        BOOST_CHECK(x != gr_complex(0));
    }

    // Try to map more than 64 elements
    std::vector<gr_complex> bpsk3(65);
    BOOST_CHECK_THROW(map_bpsk(0xFFFFFFFFFFFFFFFF, bpsk2.data(), 65), std::runtime_error);
}


BOOST_AUTO_TEST_CASE(test_demapping_range)
{
    // Assume a vector with 65 symbols: the last SOF symbol followed by 64 PLSC
    // symbols mapped from an all-ones bit sequence. This configuration
    // (including the last SOF symbol) allows for differential demapping.
    std::vector<gr_complex> symbols = {
        { -SQRT2_2, SQRT2_2 }, // last SOF symbol
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 }
    };

    // Demap a single symbol:
    BOOST_CHECK(demap_bpsk(symbols.data() + 1, 1) == 0x8000000000000000);
    BOOST_CHECK(demap_bpsk_diff(symbols.data(), 1) == 0x8000000000000000);

    // Demap two symbols:
    BOOST_CHECK(demap_bpsk(symbols.data() + 1, 2) == 0xC000000000000000);
    BOOST_CHECK(demap_bpsk_diff(symbols.data(), 2) == 0xC000000000000000);

    // Demap all 64 symbols of the "PLSC part":
    BOOST_CHECK(demap_bpsk(symbols.data() + 1, 64) == 0xFFFFFFFFFFFFFFFF);
    BOOST_CHECK(demap_bpsk_diff(symbols.data(), 64) == 0xFFFFFFFFFFFFFFFF);

    // Try to demap more than 64 elements:
    BOOST_CHECK_THROW(demap_bpsk(symbols.data(), 65), std::runtime_error);
    BOOST_CHECK_THROW(demap_bpsk_diff(symbols.data(), 65), std::runtime_error);
}

} // namespace dvbs2rx
} // namespace gr
