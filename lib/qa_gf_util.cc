/* -*- c++ -*- */
/*
 * Copyright (c) 2023 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gf_util.h"
#include <boost/mpl/list.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

namespace gr {
namespace dvbs2rx {

typedef boost::mpl::list<uint16_t, uint32_t, uint64_t> gf2_poly_base_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(test_remainder, T, gf2_poly_base_types)
{
    // Example 1:
    // f(x) = x^6 + x^5 + x^4 + x + 1
    // g(x) = x^3 + x + 1
    {
        // Regular remainder:
        auto f = gf2_poly<T>(0b1110011);
        auto g = gf2_poly<T>(0b1011);
        BOOST_CHECK(f % g == gf2_poly<T>(0b111));
        // LUT-assisted remainder:
        std::vector<uint8_t> f_bytes = { 0b1110011 };
        auto rem_lut = build_gf2_poly_rem_lut(g);
        BOOST_CHECK(gf2_poly_rem(f_bytes, g, rem_lut) == gf2_poly<T>(0b111));
    }

    // Example 2:
    // - Input (dividend) with two bytes.
    // - Divisor of degree 8.
    // - Remainder over a single byte.
    // - The first dividend byte leaks into the second.
    {
        // Regular remainder:
        auto r1 = gf2_poly<T>(0x0102);
        auto r2 = gf2_poly<T>(0x0201);
        auto g = gf2_poly<T>(0b100011101); // x^8 + x^4 + x^3 + x^2 + 1
        BOOST_CHECK(r1 % g == gf2_poly<T>(0b11111));
        BOOST_CHECK(r2 % g == gf2_poly<T>(0b111011));
        // LUT-assisted remainder:
        std::vector<uint8_t> r1_bytes = { 0x01, 0x02 };
        std::vector<uint8_t> r2_bytes = { 0x02, 0x01 };
        auto rem_lut = build_gf2_poly_rem_lut(g);
        BOOST_CHECK(gf2_poly_rem(r1_bytes, g, rem_lut) == gf2_poly<T>(0b11111));
        BOOST_CHECK(gf2_poly_rem(r2_bytes, g, rem_lut) == gf2_poly<T>(0b111011));
    }

    // Example 3:
    // - Input with four bytes (testable for uint32_t base type or larger).
    // - Divisor of degree 10 (not a multiple of 8).
    // - Remainder over two bytes.
    // - The first two dividend bytes leak over the last two bytes.
    if (sizeof(T) >= 4) {
        // Regular remainder:
        auto r1 = gf2_poly<T>(0x01020304);
        auto r2 = gf2_poly<T>(0x02010403);
        auto g = gf2_poly<T>(0b10000001001); // x^10 + x^3 + 1
        BOOST_CHECK(r1 % g == gf2_poly<T>(0b1110010100));
        BOOST_CHECK(r2 % g == gf2_poly<T>(0b1001111000));
        // LUT-assisted remainder:
        std::vector<uint8_t> r1_bytes = { 0x01, 0x02, 0x03, 0x04 };
        std::vector<uint8_t> r2_bytes = { 0x02, 0x01, 0x04, 0x03 };
        auto rem_lut = build_gf2_poly_rem_lut(g);
        BOOST_CHECK(gf2_poly_rem(r1_bytes, g, rem_lut) == gf2_poly<T>(0b1110010100));
        BOOST_CHECK(gf2_poly_rem(r2_bytes, g, rem_lut) == gf2_poly<T>(0b1001111000));
    }
}


} // namespace dvbs2rx
} // namespace gr