/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gf.h"
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>
#include <set>

namespace bdata = boost::unit_test::data;

namespace gr {
namespace dvbs2rx {

BOOST_AUTO_TEST_CASE(test_gf16_construction)
{
    uint16_t prim_poly = 0b0011; // "x^4 + x + 1" (excluding the x^4 term)
    uint8_t m = 4;
    galois_field gf(m, prim_poly);
    BOOST_CHECK_EQUAL(gf[0], 0);
    BOOST_CHECK_EQUAL(gf[1], 0b0001);
    BOOST_CHECK_EQUAL(gf[2], 0b0010);
    BOOST_CHECK_EQUAL(gf[3], 0b0100);
    BOOST_CHECK_EQUAL(gf[4], 0b1000);
    BOOST_CHECK_EQUAL(gf[5], 0b0011);
    BOOST_CHECK_EQUAL(gf[6], 0b0110);
    BOOST_CHECK_EQUAL(gf[7], 0b1100);
    BOOST_CHECK_EQUAL(gf[8], 0b1011);
    BOOST_CHECK_EQUAL(gf[9], 0b0101);
    BOOST_CHECK_EQUAL(gf[10], 0b1010);
    BOOST_CHECK_EQUAL(gf[11], 0b0111);
    BOOST_CHECK_EQUAL(gf[12], 0b1110);
    BOOST_CHECK_EQUAL(gf[13], 0b1111);
    BOOST_CHECK_EQUAL(gf[14], 0b1101);
    BOOST_CHECK_EQUAL(gf[15], 0b1001);
}

BOOST_AUTO_TEST_CASE(test_get_alpha_i_and_exponent)
{
    uint16_t prim_poly = 0b0011; // "x^4 + x + 1" (excluding the x^4 term)
    uint8_t m = 4;
    galois_field gf(m, prim_poly);

    // Given an exponent i, get alpha^i
    BOOST_CHECK_EQUAL(gf.get_alpha_i(0), 0b0001);
    BOOST_CHECK_EQUAL(gf.get_alpha_i(1), 0b0010);
    BOOST_CHECK_EQUAL(gf.get_alpha_i(2), 0b0100);
    BOOST_CHECK_EQUAL(gf.get_alpha_i(3), 0b1000);
    BOOST_CHECK_EQUAL(gf.get_alpha_i(4), 0b0011);
    BOOST_CHECK_EQUAL(gf.get_alpha_i(5), 0b0110);
    BOOST_CHECK_EQUAL(gf.get_alpha_i(6), 0b1100);
    BOOST_CHECK_EQUAL(gf.get_alpha_i(7), 0b1011);
    BOOST_CHECK_EQUAL(gf.get_alpha_i(8), 0b0101);
    BOOST_CHECK_EQUAL(gf.get_alpha_i(9), 0b1010);
    BOOST_CHECK_EQUAL(gf.get_alpha_i(10), 0b0111);
    BOOST_CHECK_EQUAL(gf.get_alpha_i(11), 0b1110);
    BOOST_CHECK_EQUAL(gf.get_alpha_i(12), 0b1111);
    BOOST_CHECK_EQUAL(gf.get_alpha_i(13), 0b1101);
    BOOST_CHECK_EQUAL(gf.get_alpha_i(14), 0b1001);

    // After i = 2^m - 2, it should wrap around
    BOOST_CHECK_EQUAL(gf.get_alpha_i(15), gf.get_alpha_i(0));
    BOOST_CHECK_EQUAL(gf.get_alpha_i(16), gf.get_alpha_i(1));
    BOOST_CHECK_EQUAL(gf.get_alpha_i(17), gf.get_alpha_i(2));
    BOOST_CHECK_EQUAL(gf.get_alpha_i(18), gf.get_alpha_i(3));

    // Given an element alpha^i, get the exponent i
    BOOST_CHECK_EQUAL(0, gf.get_exponent(0b0001));
    BOOST_CHECK_EQUAL(1, gf.get_exponent(0b0010));
    BOOST_CHECK_EQUAL(2, gf.get_exponent(0b0100));
    BOOST_CHECK_EQUAL(3, gf.get_exponent(0b1000));
    BOOST_CHECK_EQUAL(4, gf.get_exponent(0b0011));
    BOOST_CHECK_EQUAL(5, gf.get_exponent(0b0110));
    BOOST_CHECK_EQUAL(6, gf.get_exponent(0b1100));
    BOOST_CHECK_EQUAL(7, gf.get_exponent(0b1011));
    BOOST_CHECK_EQUAL(8, gf.get_exponent(0b0101));
    BOOST_CHECK_EQUAL(9, gf.get_exponent(0b1010));
    BOOST_CHECK_EQUAL(10, gf.get_exponent(0b0111));
    BOOST_CHECK_EQUAL(11, gf.get_exponent(0b1110));
    BOOST_CHECK_EQUAL(12, gf.get_exponent(0b1111));
    BOOST_CHECK_EQUAL(13, gf.get_exponent(0b1101));
    BOOST_CHECK_EQUAL(14, gf.get_exponent(0b1001));

    // The zero element cannot be represented by alpha^i (does not have an exponent i), so
    // this operation is forbidden:
    BOOST_CHECK_THROW(gf.get_exponent(0), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(test_multiplication)
{
    uint16_t prim_poly = 0b0011; // "x^4 + x + 1" (excluding the x^4 term)
    uint8_t m = 4;
    galois_field gf(m, prim_poly);

    std::vector<std::pair<uint16_t, uint16_t>> multiplicand_exponents = {
        { 0, 4 }, { 1, 4 }, { 1, 5 }, { 1, 6 }, { 5, 7 }, { 12, 7 },
    };

    for (size_t i = 0; i < multiplicand_exponents.size(); i++) {
        uint16_t a = gf.get_alpha_i(multiplicand_exponents[i].first);
        uint16_t b = gf.get_alpha_i(multiplicand_exponents[i].second);
        // alpha^i * alpha^j should result in alpha^(i+j)
        uint16_t expected_res = gf.get_alpha_i(multiplicand_exponents[i].first +
                                               multiplicand_exponents[i].second);
        BOOST_CHECK_EQUAL(gf.multiply(a, b), expected_res);
    }
}

BOOST_AUTO_TEST_CASE(test_inverse)
{
    uint16_t prim_poly = 0b0011; // "x^4 + x + 1" (excluding the x^4 term)
    uint8_t m = 4;
    galois_field gf(m, prim_poly);

    // Try a couple of elements:
    // - The inverse of alpha^12 is alpha^3.
    // - The inverse of alpha^5 is alpha^10.
    // And so on.
    std::vector<uint16_t> element_exponents = { 12, 5, 0, 14 };
    std::vector<uint16_t> inverse_exponents = { 3, 10, 0, 1 };

    for (size_t i = 0; i < element_exponents.size(); i++) {
        uint16_t elem = gf.get_alpha_i(element_exponents[i]);
        uint16_t expected_inv = gf.get_alpha_i(inverse_exponents[i]);
        BOOST_CHECK_EQUAL(gf.inverse(elem), expected_inv);
    }
}

BOOST_AUTO_TEST_CASE(test_division)
{
    uint16_t prim_poly = 0b0011; // "x^4 + x + 1" (excluding the x^4 term)
    uint8_t m = 4;
    galois_field gf(m, prim_poly);

    // Try a couple of examples using the inversions from the previous test:
    // - alpha^4 divided by alpha^12 is equal to alpha^7 because the inverse of alpha^12
    //   is alpha^3.
    // - alpha^12 divided by alpha^5 is equal to alpha^7 because the inverse of alpha^5 is
    //   alpha^10.
    // - alpha^12 divided by alpha^0 is alpha^12 because alpha^0 is 1 (the multiplicative
    //   identity).
    std::vector<uint16_t> dividend_exponents = { 4, 12, 12 };
    std::vector<uint16_t> divisor_exponents = { 12, 5, 0 };
    std::vector<uint16_t> quotient_exponents = { 7, 7, 12 };

    for (size_t i = 0; i < dividend_exponents.size(); i++) {
        uint16_t a = gf.get_alpha_i(dividend_exponents[i]);
        uint16_t b = gf.get_alpha_i(divisor_exponents[i]);
        uint16_t quotient = gf.get_alpha_i(quotient_exponents[i]);
        BOOST_CHECK_EQUAL(gf.divide(a, b), quotient);
    }
}

} // namespace dvbs2rx
} // namespace gr