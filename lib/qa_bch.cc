/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "bch.h"
#include <boost/mpl/list.hpp>
#include <boost/mpl/pair.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

namespace gr {
namespace dvbs2rx {

typedef boost::mpl::list<boost::mpl::pair<uint16_t, uint16_t>,
                         boost::mpl::pair<uint16_t, uint32_t>,
                         boost::mpl::pair<uint32_t, uint32_t>,
                         boost::mpl::pair<uint32_t, uint64_t>,
                         boost::mpl::pair<uint64_t, uint64_t>,
                         boost::mpl::pair<uint32_t, bitset256_t>>
    bch_base_types;

template <typename T>
T flip_bits(const T& in_codeword, uint32_t bch_n, uint32_t num_errors)
{
    std::set<uint32_t> flipped_bits;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, bch_n - 1);
    T out_codeword = in_codeword;
    for (uint32_t i = 0; i < num_errors; i++) {
        uint32_t bit_idx = dis(gen);
        while (flipped_bits.find(bit_idx) != flipped_bits.end())
            bit_idx = dis(gen);
        out_codeword ^= static_cast<T>(1) << bit_idx;
    }
    return out_codeword;
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_bch_gen_poly, type_pair, bch_base_types)
{
    typedef typename type_pair::first T;
    typedef typename type_pair::second P;

    // BCH code over GF(2^4)
    if (sizeof(T) * 8 >= 16) {
        gf2_poly<T> prim_poly_m4(0b10011); // x^4 + x + 1
        galois_field gf_m4(prim_poly_m4);

        // Single-error-correcting code
        uint8_t t = 1;
        bch_codec<T, P> codec_m4_t1(&gf_m4, t);
        // g(x) should be identical to the primitive polynomial
        BOOST_CHECK(codec_m4_t1.get_gen_poly() == gf2_poly<P>(prim_poly_m4.get_poly()));

        // Double-error-correcting code
        t = 2;
        bch_codec<T, P> codec_m4_t2(&gf_m4, t);
        // Expected g(x): x^8 + x^7 + x^6 + x^4 + 1
        BOOST_CHECK(codec_m4_t2.get_gen_poly() == gf2_poly<P>(0b111010001));

        // Triple-error-correcting code
        t = 3;
        bch_codec<T, P> codec_m4_t3(&gf_m4, t);
        // Expected g(x): x^10 + x^8 + x^5 + x^4 + x^2 + x + 1
        BOOST_CHECK(codec_m4_t3.get_gen_poly() == gf2_poly<P>(0b10100110111));
    }

    // BCH code over GF(2^6)
    if (sizeof(T) * 8 >= 64) {
        gf2_poly<T> prim_poly_m6(0b1000011); // x^6 + x + 1
        galois_field gf_m6(prim_poly_m6);
        // t = 1
        bch_codec<T, P> codec_m6_t1(&gf_m6, 1);
        gf2_poly<P> g1 = gf2_poly<P>(0b1000011); // x^6 + x + 1
        BOOST_CHECK(codec_m6_t1.get_gen_poly() == g1);
        BOOST_CHECK_EQUAL(codec_m6_t1.get_k(), 57);
        // t = 2
        bch_codec<T, P> codec_m6_t2(&gf_m6, 2);
        auto g2 = g1 * gf2_poly<P>(0b1010111); // g1 * (x^6 + x^4 + x^2 + x + 1)
        BOOST_CHECK(codec_m6_t2.get_gen_poly() == g2);
        BOOST_CHECK_EQUAL(codec_m6_t2.get_k(), 51);
        // t = 3
        bch_codec<T, P> codec_m6_t3(&gf_m6, 3);
        auto g3 = g2 * gf2_poly<P>(0b1100111); // g2 * (x^6 + x^5 + x^2 + x + 1)
        BOOST_CHECK(codec_m6_t3.get_gen_poly() == g3);
        BOOST_CHECK_EQUAL(codec_m6_t3.get_k(), 45);
        // t = 4
        bch_codec<T, P> codec_m6_t4(&gf_m6, 4);
        auto g4 = g3 * gf2_poly<P>(0b1001001); // g3 * (x^6 + x^3 + 1)
        BOOST_CHECK(codec_m6_t4.get_gen_poly() == g4);
        BOOST_CHECK_EQUAL(codec_m6_t4.get_k(), 39);
        // t = 5
        bch_codec<T, P> codec_m6_t5(&gf_m6, 5);
        auto g5 = g4 * gf2_poly<P>(0b1101); // g4 * (x^3 + x^2 + 1)
        BOOST_CHECK(codec_m6_t5.get_gen_poly() == g5);
        BOOST_CHECK_EQUAL(codec_m6_t5.get_k(), 36);
        // t = 6
        bch_codec<T, P> codec_m6_t6(&gf_m6, 6);
        auto g6 = g5 * gf2_poly<P>(0b1101101); // g5 * (x^6 + x^5 + x^3 + x^2 + 1)
        BOOST_CHECK(codec_m6_t6.get_gen_poly() == g6);
        BOOST_CHECK_EQUAL(codec_m6_t6.get_k(), 30);
        // t = 7
        bch_codec<T, P> codec_m6_t7(&gf_m6, 7);
        auto g7 = g6 * gf2_poly<P>(0b1011011); // g6 * (x^6 + x^4 + x^3 + x + 1)
        BOOST_CHECK(codec_m6_t7.get_gen_poly() == g7);
        BOOST_CHECK_EQUAL(codec_m6_t7.get_k(), 24);
        // t = 10
        bch_codec<T, P> codec_m6_t10(&gf_m6, 10);
        auto g10 = g7 * gf2_poly<P>(0b1110101); // g7 * (x^6 + x^5 + x^4 + x^2 + 1)
        BOOST_CHECK(codec_m6_t10.get_gen_poly() == g10);
        BOOST_CHECK_EQUAL(codec_m6_t10.get_k(), 18);
        // t = 11
        bch_codec<T, P> codec_m6_t11(&gf_m6, 11);
        auto g11 = g10 * gf2_poly<P>(0b111); // g10 * (x^2 + x + 1)
        BOOST_CHECK(codec_m6_t11.get_gen_poly() == g11);
        BOOST_CHECK_EQUAL(codec_m6_t11.get_k(), 16);
        // t = 13
        bch_codec<T, P> codec_m6_t13(&gf_m6, 13);
        auto g13 = g11 * gf2_poly<P>(0b1110011); // g11 * (x^6 + x^5 + x^4 + x + 1)
        BOOST_CHECK(codec_m6_t13.get_gen_poly() == g13);
        BOOST_CHECK_EQUAL(codec_m6_t13.get_k(), 10);
        // t = 15
        bch_codec<T, P> codec_m6_t15(&gf_m6, 15);
        auto g15 = g13 * gf2_poly<P>(0b1011); // g13 * (x^3 + x + 1)
        BOOST_CHECK(codec_m6_t15.get_gen_poly() == g15);
        BOOST_CHECK_EQUAL(codec_m6_t15.get_k(), 7);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_bch_encoder, type_pair, bch_base_types)
{
    typedef typename type_pair::first T;
    typedef typename type_pair::second P;

    // BCH code over GF(2^4)
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);
    bch_codec<T, P> codec(&gf, 2);  // Double-error-correcting code

    std::vector<T> expected_codewords = {
        0b000000000000000, 0b000000111010001, 0b000001001110011, 0b000001110100010,
        0b000010011100110, 0b000010100110111, 0b000011010010101, 0b000011101000100,
        0b000100000011101, 0b000100111001100, 0b000101001101110, 0b000101110111111,
        0b000110011111011, 0b000110100101010, 0b000111010001000, 0b000111101011001,
        0b001000000111010, 0b001000111101011, 0b001001001001001, 0b001001110011000,
        0b001010011011100, 0b001010100001101, 0b001011010101111, 0b001011101111110,
        0b001100000100111, 0b001100111110110, 0b001101001010100, 0b001101110000101,
        0b001110011000001, 0b001110100010000, 0b001111010110010, 0b001111101100011,
        0b010000001110100, 0b010000110100101, 0b010001000000111, 0b010001111010110,
        0b010010010010010, 0b010010101000011, 0b010011011100001, 0b010011100110000,
        0b010100001101001, 0b010100110111000, 0b010101000011010, 0b010101111001011,
        0b010110010001111, 0b010110101011110, 0b010111011111100, 0b010111100101101,
        0b011000001001110, 0b011000110011111, 0b011001000111101, 0b011001111101100,
        0b011010010101000, 0b011010101111001, 0b011011011011011, 0b011011100001010,
        0b011100001010011, 0b011100110000010, 0b011101000100000, 0b011101111110001,
        0b011110010110101, 0b011110101100100, 0b011111011000110, 0b011111100010111,
        0b100000011101000, 0b100000100111001, 0b100001010011011, 0b100001101001010,
        0b100010000001110, 0b100010111011111, 0b100011001111101, 0b100011110101100,
        0b100100011110101, 0b100100100100100, 0b100101010000110, 0b100101101010111,
        0b100110000010011, 0b100110111000010, 0b100111001100000, 0b100111110110001,
        0b101000011010010, 0b101000100000011, 0b101001010100001, 0b101001101110000,
        0b101010000110100, 0b101010111100101, 0b101011001000111, 0b101011110010110,
        0b101100011001111, 0b101100100011110, 0b101101010111100, 0b101101101101101,
        0b101110000101001, 0b101110111111000, 0b101111001011010, 0b101111110001011,
        0b110000010011100, 0b110000101001101, 0b110001011101111, 0b110001100111110,
        0b110010001111010, 0b110010110101011, 0b110011000001001, 0b110011111011000,
        0b110100010000001, 0b110100101010000, 0b110101011110010, 0b110101100100011,
        0b110110001100111, 0b110110110110110, 0b110111000010100, 0b110111111000101,
        0b111000010100110, 0b111000101110111, 0b111001011010101, 0b111001100000100,
        0b111010001000000, 0b111010110010001, 0b111011000110011, 0b111011111100010,
        0b111100010111011, 0b111100101101010, 0b111101011001000, 0b111101100011001,
        0b111110001011101, 0b111110110001100, 0b111111000101110, 0b111111111111111,
    };

    T max_msg = (1 << codec.get_k()) - 1;
    for (T msg = 0; msg <= max_msg; msg++) {
        T codeword = codec.encode(msg);
        BOOST_CHECK(codeword == expected_codewords[msg]);
    }
}

BOOST_AUTO_TEST_CASE(test_bch_encoder_u8_vector_out)
{
    typedef uint64_t T;
    typedef uint64_t P;

    // Create a BCH codec with byte-aligned n and k
    gf2_poly<T> prim_poly(0b1000011); // x^6 + x + 1
    galois_field gf(prim_poly);
    uint8_t t = 4; // For t = 4, m*t = 24, so the parity bits are byte-aligned
    bch_codec<T, P> codec(&gf, t, /*n=*/56);
    BOOST_CHECK_EQUAL(codec.get_n(), 56);
    BOOST_CHECK_EQUAL(codec.get_k(), 32);
    BOOST_CHECK(codec.get_n() % 8 == 0);
    BOOST_CHECK(codec.get_k() % 8 == 0);
    uint32_t n_bytes = codec.get_n() / 8;

    // Compare encoding into type T and u8 array
    T max_msg = (1 << codec.get_k()) - 1;
    for (T msg = 0; msg <= max_msg; msg++) {
        T codeword = codec.encode(msg);
        u8_vector_t msg_u8 = to_u8_vector(msg);
        u8_vector_t codeword_u8(n_bytes);
        codec.encode(msg_u8.data(), codeword_u8.data());
        BOOST_CHECK_EQUAL(codeword, from_u8_vector<T>(codeword_u8));
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_bch_syndrome, type_pair, bch_base_types)
{
    typedef typename type_pair::first T;
    typedef typename type_pair::second P;

    // BCH code over GF(2^4)
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);
    bch_codec<T, P> codec(&gf, 2);  // Double-error-correcting code
    T rx_codeword = 0b100000001;    // r(x) = x^8 + 1
    auto syndrome = codec.syndrome(rx_codeword);
    std::vector<T> expected_syndrome = {
        gf.get_alpha_i(2), gf.get_alpha_i(4), gf.get_alpha_i(7), gf.get_alpha_i(8)
    };
    BOOST_CHECK(syndrome == expected_syndrome);
}

BOOST_AUTO_TEST_CASE(test_bch_syndrome_u8_codeword)
{
    typedef uint64_t T;
    typedef uint64_t P;

    // Create a BCH codec with byte-aligned n and k
    gf2_poly<T> prim_poly(0b1000011); // x^6 + x + 1
    galois_field gf(prim_poly);
    uint8_t t = 4; // For t = 4, m*t = 24, so the parity bits are byte-aligned
    bch_codec<T, P> codec(&gf, t, /*n=*/56);
    BOOST_CHECK_EQUAL(codec.get_n(), 56);
    BOOST_CHECK_EQUAL(codec.get_k(), 32);
    BOOST_CHECK(codec.get_n() % 8 == 0);
    BOOST_CHECK(codec.get_k() % 8 == 0);
    uint32_t n_bytes = codec.get_n() / 8;

    // Compare the syndrome computed from a T-typed codeword with up to t errors to the
    // syndrome computed based on an equivalent u8 array codeword.
    T max_msg = (1 << codec.get_k()) - 1;
    for (T msg = 0; msg <= max_msg; msg++) {
        T codeword = codec.encode(msg);
        for (uint8_t num_errors = 0; num_errors <= t; num_errors++) {
            T rx_codeword = flip_bits(codeword, codec.get_n(), num_errors);
            auto syndrome = codec.syndrome(rx_codeword);
            u8_vector_t rx_codeword_u8 = to_u8_vector(rx_codeword, n_bytes);
            auto syndrome_u8 = codec.syndrome(rx_codeword_u8.data());
            BOOST_CHECK_EQUAL_COLLECTIONS(
                syndrome.begin(), syndrome.end(), syndrome_u8.begin(), syndrome_u8.end());
        }
    }
}

template <typename T, typename P>
void check_err_free_syndrome(const bch_codec<T, P>& codec, const galois_field<T>& gf)
{
    // The syndrome should be zero for error-free codewords
    T max_msg = (1 << codec.get_k()) - 1;
    for (T msg = 0; msg <= max_msg; msg++) {
        T rx_codeword = codec.encode(msg);
        auto syndrome = codec.syndrome(rx_codeword);
        for (const auto& element : syndrome)
            BOOST_CHECK_EQUAL(element, gf[0]);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_bch_syndrome_error_free, type_pair, bch_base_types)
{
    typedef typename type_pair::first T;
    typedef typename type_pair::second P;

    // BCH code over GF(2^4)
    if (sizeof(T) * 8 >= 16) {
        gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
        galois_field gf(prim_poly);
        bch_codec<T, P> codec(&gf, 2);  // Double-error-correcting code
        check_err_free_syndrome(codec, gf);
    }

    // BCH code over GF(2^6)
    if (sizeof(T) * 8 >= 64) {
        gf2_poly<T> prim_poly(0b1000011); // x^6 + x + 1
        galois_field gf(prim_poly);
        bch_codec<T, P> codec(&gf, 15);   // t = 15
        check_err_free_syndrome(codec, gf);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_bch_err_loc_poly_and_numbers,
                              type_pair,
                              bch_base_types)
{
    typedef typename type_pair::first T;
    typedef typename type_pair::second P;

    // BCH code over GF(2 ^ 4)
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);
    bch_codec<T, P> codec(&gf, 3);  // Triple-error-correcting code

    // Received codeword and syndrome
    T rx_codeword = 0b1000000101000; // r(x) = x^12 + x^5 + x^3
    auto syndrome = codec.syndrome(rx_codeword);
    std::vector<T> expected_syndrome = { gf.get_alpha_i(0),  gf.get_alpha_i(0),
                                         gf.get_alpha_i(10), gf.get_alpha_i(0),
                                         gf.get_alpha_i(10), gf.get_alpha_i(5) };
    BOOST_CHECK(syndrome == expected_syndrome);

    auto err_loc_poly = codec.err_loc_polynomial(syndrome);
    T unit = gf.get_alpha_i(0);
    T alpha_5 = gf.get_alpha_i(5);
    gf2m_poly<T> expected_err_loc_poly(&gf, { unit, unit, 0, alpha_5 });
    BOOST_CHECK(err_loc_poly == expected_err_loc_poly);

    auto err_loc_numbers = codec.err_loc_numbers(err_loc_poly);
    std::vector<T> expected_err_loc_numbers = { gf.get_alpha_i(12),
                                                gf.get_alpha_i(5),
                                                gf.get_alpha_i(3) };
    BOOST_CHECK(err_loc_numbers == expected_err_loc_numbers);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_bch_err_loc_poly_error_free, type_pair, bch_base_types)
{
    typedef typename type_pair::first T;
    typedef typename type_pair::second P;

    // BCH code over GF(2 ^ 4)
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);
    bch_codec<T, P> codec(&gf, 3);  // Triple-error-correcting code

    // Test with error-free codewords. The syndrome should be zero, and the
    // error-location polynomial should be sigma(x)=1, a polynomial of zero degree
    // (i.e., with no roots).
    T max_msg = (1 << codec.get_k()) - 1;
    T unit = gf.get_alpha_i(0);
    for (T msg = 0; msg <= max_msg; msg++) {
        T rx_codeword = codec.encode(msg);
        auto syndrome = codec.syndrome(rx_codeword);
        auto err_loc_poly = codec.err_loc_polynomial(syndrome);
        BOOST_CHECK(err_loc_poly.get_poly() == std::vector<T>({ unit }));
        BOOST_CHECK_EQUAL(err_loc_poly.degree(), 0);
        // The list of error-location numbers should be empty.
        auto err_loc_numbers = codec.err_loc_numbers(err_loc_poly);
        BOOST_CHECK_EQUAL(err_loc_numbers.size(), 0);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_bch_err_correction, type_pair, bch_base_types)
{
    typedef typename type_pair::first T;
    typedef typename type_pair::second P;

    // BCH code over GF(2 ^ 4)
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);
    bch_codec<T, P> codec(&gf, 3);  // Triple-error-correcting code

    // Received codeword and syndrome
    T rx_codeword = 0b1000000101000; // r(x) = x^12 + x^5 + x^3
    BOOST_CHECK_EQUAL(codec.decode(rx_codeword), 0);
}

template <typename T, typename P>
void check_decode(const bch_codec<T, P>& codec,
                  const galois_field<T>& gf,
                  uint8_t num_errors = 0)
{
    // The syndrome should be zero for error-free codewords
    T max_msg = (1 << codec.get_k()) - 1;
    for (T msg = 0; msg <= max_msg; msg++) {
        T tx_codeword = codec.encode(msg);
        T rx_codeword = flip_bits(tx_codeword, codec.get_n(), num_errors);
        T decoded_msg = codec.decode(rx_codeword);
        BOOST_CHECK_EQUAL(decoded_msg, msg);
    }
}


BOOST_AUTO_TEST_CASE_TEMPLATE(test_bch_encode_decode, type_pair, bch_base_types)
{
    typedef typename type_pair::first T;
    typedef typename type_pair::second P;

    // BCH code over GF(2^4)
    if (sizeof(T) * 8 >= 16) {
        gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
        galois_field gf(prim_poly);
        uint8_t t = 2;                  // Double-error-correcting code
        bch_codec<T, P> codec(&gf, t);

        // Error free
        check_decode(codec, gf);

        // Error correction
        for (uint8_t num_errors = 1; num_errors <= t; num_errors++) {
            check_decode(codec, gf, num_errors);
        }
    }

    // BCH code over GF(2^6)
    if (sizeof(T) * 8 >= 64) {
        gf2_poly<T> prim_poly(0b1000011); // x^6 + x + 1
        galois_field gf(prim_poly);
        uint8_t t = 15;
        bch_codec<T, P> codec(&gf, t);

        // Error free
        check_decode(codec, gf);

        // Error correction
        for (uint8_t num_errors = 1; num_errors <= t; num_errors++) {
            check_decode(codec, gf, num_errors);
        }
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_bch_encode_decode_shortened_bch,
                              type_pair,
                              bch_base_types)
{
    typedef typename type_pair::first T;
    typedef typename type_pair::second P;

    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);
    uint8_t t = 2;                  // Double-error-correcting code
    uint32_t n_nominal = (1 << gf.get_m()) - 1;
    uint32_t max_s = n_nominal - (gf.get_m() * t);
    // The generator polynomial has degree less than or equal to m*t. Hence, the maximum
    // shortening amount s could be slightly higher than the given max_s, but the given
    // range is ok for testing purposes.

    for (uint32_t s = 0; s < max_s; s++) {
        uint32_t n = n_nominal - s;
        bch_codec<T, P> codec(&gf, t, n);
        // Error free
        check_decode(codec, gf);
        // Error correction
        for (uint8_t num_errors = 1; num_errors <= t; num_errors++) {
            check_decode(codec, gf, num_errors);
        }
    }
}

BOOST_AUTO_TEST_CASE(test_bch_dvbs2)
{
    // From Table 6a (Normal FECFRAME) based on GF(2^16)
    gf2_poly_u32 prim_poly1(0b10000000000101101);      // x^16 + x^5 + x^3 + x^2 + 1
    galois_field gf1(prim_poly1);
    bch_codec<uint32_t, bitset256_t> codec1(&gf1, 12); // t = 12
}

} // namespace dvbs2rx
} // namespace gr