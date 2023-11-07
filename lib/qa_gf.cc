/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gf.h"
#include <boost/mpl/list.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>
#include <set>

namespace gr {
namespace dvbs2rx {

typedef boost::mpl::list<uint16_t, uint32_t, uint64_t> gf_elem_types;
typedef boost::mpl::list<uint16_t, uint32_t, uint64_t, bitset256_t> gf2_poly_base_types;


BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2m_construction, T, gf_elem_types)
{
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);
    std::vector<T> expected = { 0,      0b0001, 0b0010, 0b0100, 0b1000, 0b0011,
                                0b0110, 0b1100, 0b1011, 0b0101, 0b1010, 0b0111,
                                0b1110, 0b1111, 0b1101, 0b1001 };
    for (size_t i = 0; i < expected.size(); i++) {
        BOOST_CHECK_EQUAL(gf[i], expected[i]);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2m_get_alpha_i_and_exponent, T, gf_elem_types)
{
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);

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
    BOOST_CHECK_THROW(gf.get_exponent(0), std::out_of_range);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2m_multiplication, T, gf_elem_types)
{
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);

    std::vector<std::pair<uint32_t, uint32_t>> multiplicand_exponents = {
        { 0, 4 }, { 1, 4 }, { 1, 5 }, { 1, 6 }, { 5, 7 }, { 12, 7 },
    };

    for (size_t i = 0; i < multiplicand_exponents.size(); i++) {
        T a = gf.get_alpha_i(multiplicand_exponents[i].first);
        T b = gf.get_alpha_i(multiplicand_exponents[i].second);
        // alpha^i * alpha^j should result in alpha^(i+j)
        T expected_res = gf.get_alpha_i(multiplicand_exponents[i].first +
                                        multiplicand_exponents[i].second);
        BOOST_CHECK_EQUAL(gf.multiply(a, b), expected_res);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2m_inverse, T, gf_elem_types)
{
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);

    // Try a couple of elements:
    // - The inverse of alpha^12 is alpha^3.
    // - The inverse of alpha^5 is alpha^10.
    // And so on.
    std::vector<uint32_t> element_exponents = { 12, 5, 0, 14 };
    std::vector<uint32_t> inverse_exponents = { 3, 10, 0, 1 };

    for (size_t i = 0; i < element_exponents.size(); i++) {
        T elem = gf.get_alpha_i(element_exponents[i]);
        T expected_inv = gf.get_alpha_i(inverse_exponents[i]);
        BOOST_CHECK_EQUAL(gf.inverse(elem), expected_inv);
        BOOST_CHECK_EQUAL(gf.inverse_by_exp(element_exponents[i]), expected_inv);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2m_division, T, gf_elem_types)
{
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);

    // Try a couple of examples using the inversions from the previous test:
    // - alpha^4 divided by alpha^12 is equal to alpha^7 because the inverse of alpha^12
    //   is alpha^3.
    // - alpha^12 divided by alpha^5 is equal to alpha^7 because the inverse of alpha^5 is
    //   alpha^10.
    // - alpha^12 divided by alpha^0 is alpha^12 because alpha^0 is 1 (the multiplicative
    //   identity).
    std::vector<uint32_t> dividend_exponents = { 4, 12, 12 };
    std::vector<uint32_t> divisor_exponents = { 12, 5, 0 };
    std::vector<uint32_t> quotient_exponents = { 7, 7, 12 };

    for (size_t i = 0; i < dividend_exponents.size(); i++) {
        uint32_t a = gf.get_alpha_i(dividend_exponents[i]);
        uint32_t b = gf.get_alpha_i(divisor_exponents[i]);
        uint32_t quotient = gf.get_alpha_i(quotient_exponents[i]);
        BOOST_CHECK_EQUAL(gf.divide(a, b), quotient);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2m_conjugates, T, gf_elem_types)
{
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);

    // Examples:
    // - The only conjugate of alpha^0 is alpha^0 itself.
    // - The conjugates of alpha^1 are alpha^2, alpha^4, and alpha^8.
    // And so on.
    std::vector<uint32_t> beta_exponents = { 0, 1, 3, 5, 7 };
    std::vector<std::set<T>> expected_conjugates = {
        { gf.get_alpha_i(0) },
        { gf.get_alpha_i(1), gf.get_alpha_i(2), gf.get_alpha_i(4), gf.get_alpha_i(8) },
        { gf.get_alpha_i(3), gf.get_alpha_i(6), gf.get_alpha_i(9), gf.get_alpha_i(12) },
        { gf.get_alpha_i(5), gf.get_alpha_i(10) },
        { gf.get_alpha_i(7), gf.get_alpha_i(11), gf.get_alpha_i(13), gf.get_alpha_i(14) }
    };

    for (size_t i = 0; i < beta_exponents.size(); i++) {
        T beta = gf.get_alpha_i(beta_exponents[i]);
        BOOST_CHECK(gf.get_conjugates(beta) == expected_conjugates[i]);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2m_min_poly, T, gf_elem_types)
{
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);

    // Trivial minimal polynomials
    BOOST_CHECK(gf.get_min_poly(0) == gf2_poly<T>(0b10)); // 0 is a root of phi(x) = x
    BOOST_CHECK(gf.get_min_poly(1) == gf2_poly<T>(0b11)); // 1 is a root of phi(x) = x + 1

    // By definition, the elements with the same set of conjugates have the same
    // associated minimal polynomials.
    std::vector<std::vector<uint32_t>> beta_exponents_per_conjugate_set = {
        { 1, 2, 4, 8 },   // conjugates set 0
        { 3, 6, 9, 12 },  // conjugates set 1
        { 5, 10 },        // conjugates set 2
        { 7, 11, 13, 14 } // conjugates set 3
    };
    std::vector<gf2_poly<T>> expected_min_poly = {
        gf2_poly<T>(0b10011), // x^4 + x + 1
        gf2_poly<T>(0b11111), // x^4 + x^3 + x^2 + x + 1
        gf2_poly<T>(0b111),   // x^2 + x + 1
        gf2_poly<T>(0b11001)  // x^4 + x^3 + 1
    };

    for (size_t k = 0; k < beta_exponents_per_conjugate_set.size(); k++) {
        for (size_t i = 0; i < beta_exponents_per_conjugate_set[k].size(); i++) {
            T beta = gf.get_alpha_i(beta_exponents_per_conjugate_set[k][i]);
            BOOST_CHECK(gf.get_min_poly(beta) == expected_min_poly[k]);
        }
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2m_dvbs2_min_poly, T, gf_elem_types)
{

    // From Table 6a (Normal FECFRAME) - based on GF(2^16).
    //
    // Note: the primitive polynomial has degree 16, so it is represented by a 17-bit
    // integer in the adopted notation (including the MSB). Skip the normal FECFRAME test
    // when T does not fit 17 bits (e.g., for T = uint16_t).
    if (sizeof(T) > 2) {
        gf2_poly<T> prim_poly1(0b10000000000101101); // x^16 + x^5 + x^3 + x^2 + 1
        galois_field gf1(prim_poly1);
        std::vector<gf2_poly<T>> expected_min_poly1 = {
            gf2_poly<T>(0b10000000000101101), // g1
            gf2_poly<T>(0b10000000101110011), // g2
            gf2_poly<T>(0b10000111110111101), // g3
            gf2_poly<T>(0b10101101001010101), // g4
            gf2_poly<T>(0b10001111100101111), // g5
            gf2_poly<T>(0b11111011110110101), // g6
            gf2_poly<T>(0b11010111101100101), // g7
            gf2_poly<T>(0b10111001101100111), // g8
            gf2_poly<T>(0b10000111010100001), // g9
            gf2_poly<T>(0b10111010110100111), // g10
            gf2_poly<T>(0b10011101000101101), // g11
            gf2_poly<T>(0b10001101011100011), // g12
        };

        // The t-error-correcting BCH code uses the minimal polynomials associated with
        // the elements alpha^1, alpha^3, ..., alpha^(2*t - 1). DVB-S2 uses t up to 12.
        for (size_t i = 1; i <= 12; i++) {
            T beta = gf1.get_alpha_i(2 * i - 1);
            BOOST_CHECK(gf1.get_min_poly(beta) == expected_min_poly1[i - 1]);
        }
    }

    // From Table 6b (Short FECFRAME) - based on GF(2^14)
    gf2_poly<T> prim_poly2(0b100000000101011); // x^14 + x^5 + x^3 + x + 1
    galois_field gf2(prim_poly2);
    std::vector<gf2_poly<T>> expected_min_poly2 = {
        gf2_poly<T>(0b100000000101011), // g1
        gf2_poly<T>(0b100100101000001), // g2
        gf2_poly<T>(0b100011001000111), // g3
        gf2_poly<T>(0b101010110010001), // g4
        gf2_poly<T>(0b110101101010101), // g5
        gf2_poly<T>(0b110001110001001), // g6
        gf2_poly<T>(0b110110011100101), // g7
        gf2_poly<T>(0b100111100100001), // g8
        gf2_poly<T>(0b100011000001111), // g9
        gf2_poly<T>(0b101101001001001), // g10
        gf2_poly<T>(0b101100000010001), // g11
        gf2_poly<T>(0b110010111101111), // g12
    };

    for (size_t i = 1; i <= 12; i++) {
        T beta = gf2.get_alpha_i(2 * i - 1);
        BOOST_CHECK(gf2.get_min_poly(beta) == expected_min_poly2[i - 1]);
    }

    // Medium FECFRAME from Table 7 in the DVB-S2X standard
    gf2_poly<T> prim_poly3(0b1000000000101101); // x^15 + x^5 + x^3 + x^2 + 1
    galois_field gf3(prim_poly3);
    std::vector<gf2_poly<T>> expected_min_poly3 = {
        gf2_poly<T>(0b1000000000101101), // g1
        gf2_poly<T>(0b1000110010010011), // g2
        gf2_poly<T>(0b1011010101010101), // g3
        gf2_poly<T>(0b1000110101101101), // g4
        gf2_poly<T>(0b1001010011010111), // g5
        gf2_poly<T>(0b1011000011010001), // g6
        gf2_poly<T>(0b1101100010110101), // g7
        gf2_poly<T>(0b1100101101010101), // g8
        gf2_poly<T>(0b1011101010110111), // g9
        gf2_poly<T>(0b1011110010011111), // g10
        gf2_poly<T>(0b1000101000010111), // g11
        gf2_poly<T>(0b1110110100010101), // g12
    };

    for (size_t i = 1; i <= 12; i++) {
        T beta = gf3.get_alpha_i(2 * i - 1);
        BOOST_CHECK(gf3.get_min_poly(beta) == expected_min_poly3[i - 1]);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2_poly_degrees, T, gf2_poly_base_types)
{
    auto a = gf2_poly<T>(0b101);  // x^2 + 1
    auto b = gf2_poly<T>(0b11);   // x + 1
    auto c = gf2_poly<T>(0b1101); // x^3 + x^2 + 1
    auto d = gf2_poly<T>(1);      // unit polynomial
    auto e = gf2_poly<T>(0);      // zero polynomial
    BOOST_CHECK_EQUAL(a.degree(), 2);
    BOOST_CHECK_EQUAL(b.degree(), 1);
    BOOST_CHECK_EQUAL(c.degree(), 3);
    BOOST_CHECK_EQUAL(d.degree(), 0);
    BOOST_CHECK_EQUAL(e.degree(), -1);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2_poly_is_zero, T, gf2_poly_base_types)
{
    auto a = gf2_poly<T>(0b101); // x^2 + 1
    auto b = gf2_poly<T>(1);     // unit polynomial
    auto c = gf2_poly<T>(0);     // zero polynomial
    BOOST_CHECK(!a.is_zero());
    BOOST_CHECK(!b.is_zero());
    BOOST_CHECK(c.is_zero());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2_poly_addition, T, gf2_poly_base_types)
{
    auto a = gf2_poly<T>(0b101);               // x^2 + 1
    auto b = gf2_poly<T>(0b11);                // x + 1
    auto c = gf2_poly<T>(0b1101);              // x^3 + x^2 + 1
    BOOST_CHECK(a + b == gf2_poly<T>(0b110));  // x^2 + x
    BOOST_CHECK(a + c == gf2_poly<T>(0b1000)); // x^3
    BOOST_CHECK(b + c == gf2_poly<T>(0b1110)); // x^3 + x^2 + x
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2_poly_multiplication, T, gf2_poly_base_types)
{
    // Polynomial by polynomial
    auto a = gf2_poly<T>(0b101);                 // x^2 + 1
    auto b = gf2_poly<T>(0b11);                  // x + 1
    auto c = gf2_poly<T>(0b1101);                // x^3 + x^2 + 1
    BOOST_CHECK(a * b == gf2_poly<T>(0b1111));   // x^3 + x^2 + x + 1
    BOOST_CHECK(a * c == gf2_poly<T>(0b111001)); // x^5 + x^4 + x^3 + 1
    BOOST_CHECK(b * c == gf2_poly<T>(0b10111));  // x^4 + x^2 + x + 1

    // Polynomial by a scalar
    BOOST_CHECK(a * 0 == gf2_poly<T>(0));
    BOOST_CHECK(a * 1 == gf2_poly<T>(a));
    BOOST_CHECK(a * 2 == gf2_poly<T>(a)); // 2 is treated as a bool

    // The * operator must check if the product fits in T
    size_t max_degree = get_max_gf2_poly_degree<T>();
    auto d = gf2_poly<T>(static_cast<T>(1) << max_degree); // x^max_degree
    BOOST_CHECK_THROW(d * d, std::runtime_error);

    uint32_t half_max_degree = max_degree / 2;
    auto e = gf2_poly<T>(static_cast<T>(1) << half_max_degree); // x^half_max_degree
    BOOST_CHECK_NO_THROW(e * e);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2_poly_remainder, T, gf2_poly_base_types)
{
    // f(x) = x^6 + x^5 + x^4 + x + 1
    // g(x) = x^3 + x + 1
    //
    // f(x) = (x^3 + x^2)*g(x) + (x^2 + x + 1)
    auto f = gf2_poly<T>(0b1110011);
    auto g = gf2_poly<T>(0b1011);
    BOOST_CHECK(f % g == gf2_poly<T>(0b111));

    // In the reverse order, g(x) % f(x) = g(x) given that g(x) has lower degree than f(x)
    BOOST_CHECK(g % f == g);

    // Theorem: a primitive polynomial of degree m necessarily divides "x^(2^m - 1) + 1".
    // Example for m=3: (x^7 + 1) divided by (x^3 + x + 1) must yield zero remainder.
    auto a = gf2_poly<T>(0b10000001);
    auto b = gf2_poly<T>(0b1011);
    BOOST_CHECK(a % b == gf2_poly<T>(0));

    // A zero polynomial divided by a non-zero polynomial should result in zero
    auto zero_poly = gf2_poly<T>(0);
    auto d = gf2_poly<T>(0b1101);
    BOOST_CHECK(zero_poly % d == zero_poly);

    // A non-zero polynomial divided by a zero polynomial should throw error
    BOOST_CHECK_THROW(d % zero_poly, std::runtime_error);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2_poly_to_gf2m_poly, T, gf_elem_types)
{
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);

    auto poly_gf2 = gf2_poly<T>(0b101); // x^2 + 1
    auto poly_gf2m = gf2m_poly(&gf, poly_gf2);
    auto expected = gf2m_poly(&gf, { gf[1], gf[0], gf[1] });
    BOOST_CHECK(poly_gf2m == expected);
    BOOST_CHECK_EQUAL(poly_gf2.get_poly(), poly_gf2m.to_gf2_poly().get_poly());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2m_poly_to_gf2_poly, T, gf_elem_types)
{
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);

    // Construction from a binary polynomial over GF(2^m) should work
    T gf2m_unit = gf.get_alpha_i(0);
    auto poly_ext_field = gf2m_poly(&gf, { gf2m_unit, 0, gf2m_unit }); // x^2 + 1
    auto poly_bin_field = poly_ext_field.to_gf2_poly();
    BOOST_CHECK(poly_bin_field == gf2_poly<T>(0b101));
    BOOST_CHECK(poly_bin_field.degree() == 2);

    // Construction from a non-binary polynomial over GF(2^m) should NOT work
    T alpha_1 = gf.get_alpha_i(1);
    auto poly_ext_field2 = gf2m_poly(&gf, { gf2m_unit, 0, alpha_1 }); // alpha * x^2 + 1
    BOOST_CHECK_THROW(poly_ext_field2.to_gf2_poly(), std::runtime_error);

    // The conversion to a polynomial over GF(2) must check if the binary GF(2^m)
    // polynomial fits within type T used by the GF(2) polynomial.
    std::vector<T> coeffs(sizeof(T) * 8, 0);
    coeffs[0] = gf2m_unit;
    coeffs.push_back(gf2m_unit);
    auto poly_ext_field3 = gf2m_poly(&gf, std::move(coeffs));
    BOOST_CHECK_EQUAL(poly_ext_field3.degree(), sizeof(T) * 8);
    BOOST_CHECK_THROW(poly_ext_field3.to_gf2_poly(), std::runtime_error);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2m_poly_addition, T, gf_elem_types)
{
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);

    T alpha_0 = gf.get_alpha_i(0);
    T alpha_1 = gf.get_alpha_i(1);
    T alpha_4 = gf.get_alpha_i(4);

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

BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2m_poly_multiplication, T, gf_elem_types)
{
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);

    T alpha_1 = gf.get_alpha_i(1);
    T alpha_4 = gf.get_alpha_i(4);
    T alpha_5 = gf.get_alpha_i(5);

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

BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2m_poly_eval, T, gf_elem_types)
{
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);

    T unit = gf.get_alpha_i(0);
    T alpha = gf.get_alpha_i(1);
    T alpha_2 = gf.get_alpha_i(2);
    T alpha_3 = gf.get_alpha_i(3);
    T alpha_4 = gf.get_alpha_i(4);
    T alpha_5 = gf.get_alpha_i(5);

    // a(x) = x^3 + alpha^4
    auto a = gf2m_poly(&gf, { alpha_4, 0, 0, 1 });
    BOOST_CHECK(a.eval(0) == alpha_4);
    // Note: for x = 0 only eval works. eval_by_exp does not work.
    BOOST_CHECK(a.eval(unit) == (unit ^ alpha_4));
    BOOST_CHECK(a.eval_by_exp(0) == a.eval(unit));
    BOOST_CHECK(a.eval(alpha) == (alpha_3 ^ alpha_4));
    BOOST_CHECK(a.eval_by_exp(1) == a.eval(alpha));

    // b(x) = alpha^2 x^2 + alpha^5
    auto b = gf2m_poly(&gf, { alpha_5, 0, alpha_2 });
    BOOST_CHECK(b.eval(0) == alpha_5);
    BOOST_CHECK(b.eval(unit) == (alpha_2 ^ alpha_5));
    BOOST_CHECK(b.eval_by_exp(0) == b.eval(unit));
    BOOST_CHECK(b.eval(alpha) == (alpha_4 ^ alpha_5));
    BOOST_CHECK(b.eval_by_exp(1) == b.eval(alpha));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_gf2m_poly_root_search, T, gf_elem_types)
{
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);

    uint32_t m_two_to_m_minus_one = (1 << gf.get_m()) - 1;

    // Search the roots of each minimal polynomial in GF(2^m)
    for (uint32_t i = 1; i < m_two_to_m_minus_one; i++) {
        // By definition, the minimal polynomial of alpha^i is the polynomial over GF(2)
        // of smallest degree having alpha^i as a root. Also, the conjugates of alpha^i
        // are the other roots of the minimal polynomial. Hence, the root search should
        // obtain all conjugates of alpha^i.
        T alpha_i = gf.get_alpha_i(i);
        auto min_poly = gf.get_min_poly(alpha_i); // minimal polynomial is over GF(2)
        auto poly = gf2m_poly(&gf, min_poly); // convert it to a polynomial over GF(2^m)
        auto root_exps = poly.search_roots_in_exp_range(1, m_two_to_m_minus_one);
        std::vector<T> roots; // convert the exponents to the actual elements (the roots)
        std::transform(root_exps.cbegin(),
                       root_exps.cend(),
                       std::back_inserter(roots),
                       [&gf](uint32_t root_exp) { return gf.get_alpha_i(root_exp); });
        std::set<T> root_set(roots.cbegin(), roots.cend());
        auto expected_roots = gf.get_conjugates(alpha_i);
        BOOST_CHECK_EQUAL_COLLECTIONS(root_set.cbegin(),
                                      root_set.cend(),
                                      expected_roots.cbegin(),
                                      expected_roots.cend());
    }
}

} // namespace dvbs2rx
} // namespace gr