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

namespace gr {
namespace dvbs2rx {

BOOST_AUTO_TEST_CASE(test_gf2m_construction)
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

BOOST_AUTO_TEST_CASE(test_gf2m_get_alpha_i_and_exponent)
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

BOOST_AUTO_TEST_CASE(test_gf2m_multiplication)
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

BOOST_AUTO_TEST_CASE(test_gf2m_inverse)
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

BOOST_AUTO_TEST_CASE(test_gf2m_division)
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

BOOST_AUTO_TEST_CASE(test_gf2m_conjugates)
{
    uint16_t prim_poly = 0b0011; // "x^4 + x + 1" (excluding the x^4 term)
    uint8_t m = 4;
    galois_field gf(m, prim_poly);

    // Examples:
    // - The only conjugate of alpha^0 is alpha^0 itself.
    // - The conjugates of alpha^1 are alpha^2, alpha^4, and alpha^8.
    // And so on.
    std::vector<uint16_t> beta_exponents = { 0, 1, 3, 5, 7 };
    std::vector<std::set<uint16_t>> expected_conjugates = {
        { gf.get_alpha_i(0) },
        { gf.get_alpha_i(1), gf.get_alpha_i(2), gf.get_alpha_i(4), gf.get_alpha_i(8) },
        { gf.get_alpha_i(3), gf.get_alpha_i(6), gf.get_alpha_i(9), gf.get_alpha_i(12) },
        { gf.get_alpha_i(5), gf.get_alpha_i(10) },
        { gf.get_alpha_i(7), gf.get_alpha_i(11), gf.get_alpha_i(13), gf.get_alpha_i(14) }
    };

    for (size_t i = 0; i < beta_exponents.size(); i++) {
        uint16_t beta = gf.get_alpha_i(beta_exponents[i]);
        BOOST_CHECK(gf.get_conjugates(beta) == expected_conjugates[i]);
    }
}

BOOST_AUTO_TEST_CASE(test_gf2m_min_poly)
{
    uint16_t prim_poly = 0b0011; // "x^4 + x + 1" (excluding the x^4 term)
    uint8_t m = 4;
    galois_field gf(m, prim_poly);

    // Trivial minimal polynomials
    BOOST_CHECK(gf.get_min_poly(0) == gf2_poly(0b10)); // 0 is a root of phi(x) = x
    BOOST_CHECK(gf.get_min_poly(1) == gf2_poly(0b11)); // 1 is a root of phi(x) = x + 1

    // By definition, the elements with the same set of conjugates have the same
    // associated minimal polynomials.
    std::vector<std::vector<uint16_t>> beta_exponents_per_conjugate_set = {
        { 1, 2, 4, 8 },   // conjugates set 0
        { 3, 6, 9, 12 },  // conjugates set 1
        { 5, 10 },        // conjugates set 2
        { 7, 11, 13, 14 } // conjugates set 3
    };
    std::vector<gf2_poly> expected_min_poly = {
        gf2_poly(0b10011), // x^4 + x + 1
        gf2_poly(0b11111), // x^4 + x^3 + x^2 + x + 1
        gf2_poly(0b111),   // x^2 + x + 1
        gf2_poly(0b11001)  // x^4 + x^3 + 1
    };

    for (size_t k = 0; k < beta_exponents_per_conjugate_set.size(); k++) {
        for (size_t i = 0; i < beta_exponents_per_conjugate_set[k].size(); i++) {
            uint16_t beta = gf.get_alpha_i(beta_exponents_per_conjugate_set[k][i]);
            BOOST_CHECK(gf.get_min_poly(beta) == expected_min_poly[k]);
        }
    }
}

BOOST_AUTO_TEST_CASE(test_gf2_poly_degrees)
{
    auto a = gf2_poly(0b101);  // x^2 + 1
    auto b = gf2_poly(0b11);   // x + 1
    auto c = gf2_poly(0b1101); // x^3 + x^2 + 1
    auto d = gf2_poly(1);      // unit polynomial
    auto e = gf2_poly(0);      // zero polynomial
    BOOST_CHECK_EQUAL(a.degree(), 2);
    BOOST_CHECK_EQUAL(b.degree(), 1);
    BOOST_CHECK_EQUAL(c.degree(), 3);
    BOOST_CHECK_EQUAL(d.degree(), 0);
    BOOST_CHECK_EQUAL(e.degree(), -1);
}

BOOST_AUTO_TEST_CASE(test_gf2_poly_addition)
{
    auto a = gf2_poly(0b101);               // x^2 + 1
    auto b = gf2_poly(0b11);                // x + 1
    auto c = gf2_poly(0b1101);              // x^3 + x^2 + 1
    BOOST_CHECK(a + b == gf2_poly(0b110));  // x^2 + x
    BOOST_CHECK(a + c == gf2_poly(0b1000)); // x^3
    BOOST_CHECK(b + c == gf2_poly(0b1110)); // x^3 + x^2 + x
}

BOOST_AUTO_TEST_CASE(test_gf2_poly_multiplication)
{
    // Polynomial by polynomial
    auto a = gf2_poly(0b101);                 // x^2 + 1
    auto b = gf2_poly(0b11);                  // x + 1
    auto c = gf2_poly(0b1101);                // x^3 + x^2 + 1
    BOOST_CHECK(a * b == gf2_poly(0b1111));   // x^3 + x^2 + x + 1
    BOOST_CHECK(a * c == gf2_poly(0b111001)); // x^5 + x^4 + x^3 + 1
    BOOST_CHECK(b * c == gf2_poly(0b10111));  // x^4 + x^2 + x + 1

    // Polynomial by a scalar
    BOOST_CHECK(a * 0 == gf2_poly(0));
    BOOST_CHECK(a * 1 == gf2_poly(a));
    BOOST_CHECK(a * 2 == gf2_poly(a)); // 2 is treated as a bool

    // The * operator must check if the product fits in 16 bits
    auto d = gf2_poly(0x101); // x^8 + 1
    BOOST_CHECK_THROW(d * d, std::runtime_error);
}

BOOST_AUTO_TEST_CASE(test_gf2_poly_constructor_from_gf2m_poly)
{
    uint16_t prim_poly = 0b0011; // "x^4 + x + 1" (excluding the x^4 term)
    uint8_t m = 4;
    galois_field gf(m, prim_poly);

    // Construction from a binary polynomial over GF(2^m) should work
    uint16_t gf2m_unit = gf.get_alpha_i(0);
    auto poly_ext_field = gf2m_poly(&gf, { gf2m_unit, 0, gf2m_unit }); // x^2 + 1
    auto poly_bin_field = gf2_poly(poly_ext_field);
    BOOST_CHECK(poly_bin_field == gf2_poly(0b101));
    BOOST_CHECK(poly_bin_field.degree() == 2);

    // Construction from a non-binary polynomial over GF(2^m) should NOT work
    uint16_t alpha_1 = gf.get_alpha_i(1);
    auto poly_ext_field2 = gf2m_poly(&gf, { gf2m_unit, 0, alpha_1 }); // alpha * x^2 + 1
    BOOST_CHECK_THROW(gf2_poly poly_bin_field2(poly_ext_field2), std::runtime_error);

    // The constructor must check if the incoming gf2m poly has degree <= 15
    auto poly_ext_field3 = gf2m_poly(
        &gf, { gf2m_unit, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, gf2m_unit });
    BOOST_CHECK_EQUAL(poly_ext_field3.degree(), 16);
    BOOST_CHECK_THROW(gf2_poly poly_bin_field3(poly_ext_field3), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(test_gf2m_poly_addition)
{
    uint16_t prim_poly = 0b0011; // "x^4 + x + 1" (excluding the x^4 term)
    uint8_t m = 4;
    galois_field gf(m, prim_poly);

    uint16_t alpha_0 = gf.get_alpha_i(0);
    uint16_t alpha_1 = gf.get_alpha_i(1);
    uint16_t alpha_4 = gf.get_alpha_i(4);

    auto a = gf2m_poly(&gf, { 1, 0, alpha_4 });    // alpha^4 x^2 + 1
    auto b = gf2m_poly(&gf, { 1, alpha_1 });       // alpha^1 x + 1
    auto c = gf2m_poly(&gf, { 1, 0, alpha_0, 1 }); // x^3 + alpha^0 x^2 + 1

    auto res1 = a + b;
    auto expected1 = gf2m_poly(&gf, { 0, alpha_1, alpha_4 });
    BOOST_CHECK(res1 == expected1);

    auto res2 = a + c;
    auto expected2 = gf2m_poly(&gf, { 0, 0, alpha_1, 1 }); // alpha^0 + alpha^4 = alpha^1
    BOOST_CHECK(res2 == expected2);
}

BOOST_AUTO_TEST_CASE(test_gf2m_poly_multiplication)
{
    uint16_t prim_poly = 0b0011; // "x^4 + x + 1" (excluding the x^4 term)
    uint8_t m = 4;
    galois_field gf(m, prim_poly);

    uint16_t alpha_1 = gf.get_alpha_i(1);
    uint16_t alpha_4 = gf.get_alpha_i(4);
    uint16_t alpha_5 = gf.get_alpha_i(5);

    // a(x) = alpha^4 x^2 + 1
    // b(x) = alpha^1 x + 1
    // a(x) * b(x) = (alpha^4 x^2 + 1) * (alpha^1 x + 1)
    //             = (alpha^4 * alpha^1) x^3 + alpha^4 x^2 + alpha^1 x + 1
    //             = alpha^5 x^3 + alpha^4 x^2 + alpha^1 x + 1
    auto a = gf2m_poly(&gf, { 1, 0, alpha_4 });
    auto b = gf2m_poly(&gf, { 1, alpha_1 });
    auto res = a * b;
    auto expected = gf2m_poly(&gf, { 1, alpha_1, alpha_4, alpha_5 });
    BOOST_CHECK(res == expected);

    // Multiplication by scalars
    BOOST_CHECK(a * 0 == gf2m_poly(&gf, {}));
    BOOST_CHECK(a * 1 == a);
}


} // namespace dvbs2rx
} // namespace gr