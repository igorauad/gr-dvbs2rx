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

void fill_random_bytes(u8_vector_t& vec)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    for (size_t i = 0; i < vec.size(); i++) {
        vec[i] = dis(gen);
    }
}

template <typename T>
T flip_random_bits(const T& in_data, uint32_t valid_bits, uint32_t num_bits)
{
    std::set<uint32_t> flipped_bits;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, valid_bits - 1);
    T out_data = in_data;
    for (uint32_t i = 0; i < num_bits; i++) {
        uint32_t bit_idx = dis(gen);
        while (flipped_bits.find(bit_idx) != flipped_bits.end())
            bit_idx = dis(gen);
        out_data ^= static_cast<T>(1) << bit_idx;
    }
    return out_data;
}

void flip_bit(u8_vector_t& vec, uint32_t bit_idx)
{
    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_pos = bit_idx % 8;
    vec[byte_idx] ^= (1 << bit_pos);
}

void flip_random_bits(u8_vector_t& vec, uint32_t num_bits)
{
    std::set<uint32_t> flipped_bits;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, vec.size() * 8 - 1);
    for (uint32_t i = 0; i < num_bits; i++) {
        uint32_t bit_idx = dis(gen);
        while (flipped_bits.find(bit_idx) != flipped_bits.end())
            bit_idx = dis(gen);
        flip_bit(vec, bit_idx);
        flipped_bits.insert(bit_idx);
    }
}

uint32_t count_errors(const u8_vector_t& a, const u8_vector_t& b)
{
    if (a.size() != b.size()) {
        throw std::runtime_error("Vectors must have the same size");
    }
    int bit_errors = 0;
    for (uint32_t i = 0; i < a.size(); i++) {
        if (a[i] != b[i]) {
            bit_errors += std::bitset<8>(a[i] ^ b[i]).count();
        }
    }
    return bit_errors;
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
    bch_codec<T, P> codec(&gf, 2); // Double-error-correcting code

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
    bch_codec<T, P> codec(&gf, t, /*n=*/32);
    BOOST_CHECK_EQUAL(codec.get_n(), 32);
    BOOST_CHECK_EQUAL(codec.get_k(), 8);
    BOOST_CHECK(codec.get_n() % 8 == 0);
    BOOST_CHECK(codec.get_k() % 8 == 0);
    uint32_t n_bytes = codec.get_n() / 8;
    uint32_t k_bytes = codec.get_k() / 8;

    // Compare encoding into type T and u8 array
    T max_msg = (1 << codec.get_k()) - 1;
    for (T msg = 0; msg <= max_msg; msg++) {
        T codeword = codec.encode(msg);
        u8_vector_t msg_u8 = to_u8_vector(msg, k_bytes);
        u8_vector_t codeword_u8(n_bytes);
        codec.encode(msg_u8.data(), codeword_u8.data());
        // Ensure the systematic part is preserved on encoding
        BOOST_CHECK_EQUAL_COLLECTIONS(msg_u8.begin(),
                                      msg_u8.end(),
                                      codeword_u8.begin(),
                                      codeword_u8.begin() + k_bytes);
        // Check the codewords match
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
    bch_codec<T, P> codec(&gf, 2); // Double-error-correcting code
    T rx_codeword = 0b100000001;   // r(x) = x^8 + 1
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
    bch_codec<T, P> codec(&gf, t, /*n=*/32);
    BOOST_CHECK_EQUAL(codec.get_n(), 32);
    BOOST_CHECK_EQUAL(codec.get_k(), 8);
    BOOST_CHECK(codec.get_n() % 8 == 0);
    BOOST_CHECK(codec.get_k() % 8 == 0);
    uint32_t n_bytes = codec.get_n() / 8;

    // Compare the syndrome computed from a T-typed codeword with up to t errors to the
    // syndrome computed based on an equivalent u8 array codeword.
    T max_msg = (1 << codec.get_k()) - 1;
    for (T msg = 0; msg <= max_msg; msg++) {
        T codeword = codec.encode(msg);
        for (uint8_t num_errors = 0; num_errors <= t; num_errors++) {
            T rx_codeword = flip_random_bits(codeword, codec.get_n(), num_errors);
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
    // The syndrome should be empty for error-free codewords
    T max_msg = (1 << codec.get_k()) - 1;
    for (T msg = 0; msg <= max_msg; msg++) {
        T rx_codeword = codec.encode(msg);
        auto syndrome = codec.syndrome(rx_codeword);
        BOOST_CHECK_EQUAL(syndrome.size(), 0);
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
        bch_codec<T, P> codec(&gf, 2); // Double-error-correcting code
        check_err_free_syndrome(codec, gf);
    }

    // BCH code over GF(2^6)
    if (sizeof(T) * 8 >= 64) {
        gf2_poly<T> prim_poly(0b1000011); // x^6 + x + 1
        galois_field gf(prim_poly);
        bch_codec<T, P> codec(&gf, 15); // t = 15
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
    bch_codec<T, P> codec(&gf, 3); // Triple-error-correcting code

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
    uint8_t t = 3; // Triple-error-correcting code
    bch_codec<T, P> codec(&gf, t);

    // Simulate an all-zeros syndrome vector indicating no error has occurred. In this
    // case, the resulting error-location polynomial should be sigma(x)=1, a polynomial of
    // zero degree (i.e., with no roots).
    //
    // NOTE: The syndrome calculation functions return an empty vector for error-free
    // codewords. However, an all-zeros syndrome vector is equally valid for error-free
    // codewords. The difference is the latter requires unnecessary computations to
    // evaluate the syndrome components.
    auto syndrome = std::vector<T>(2 * t, 0); // all-zeros syndrome vector
    auto err_loc_poly = codec.err_loc_polynomial(syndrome);
    T unit = gf.get_alpha_i(0);
    BOOST_CHECK(err_loc_poly.get_poly() == std::vector<T>({ unit }));
    BOOST_CHECK_EQUAL(err_loc_poly.degree(), 0);
    // The list of error-location numbers should be empty.
    auto err_loc_numbers = codec.err_loc_numbers(err_loc_poly);
    BOOST_CHECK_EQUAL(err_loc_numbers.size(), 0);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_bch_err_correction, type_pair, bch_base_types)
{
    typedef typename type_pair::first T;
    typedef typename type_pair::second P;

    // BCH code over GF(2 ^ 4)
    gf2_poly<T> prim_poly(0b10011); // x^4 + x + 1
    galois_field gf(prim_poly);
    bch_codec<T, P> codec(&gf, 3); // Triple-error-correcting code

    // Received codeword and syndrome
    T rx_codeword = 0b1000000101000; // r(x) = x^12 + x^5 + x^3
    BOOST_CHECK_EQUAL(codec.decode(rx_codeword), 0);
}

template <typename T, typename P>
void check_decode(const bch_codec<T, P>& codec,
                  const galois_field<T>& gf,
                  uint8_t num_errors = 0)
{
    T max_msg = (1 << codec.get_k()) - 1;
    for (T msg = 0; msg <= max_msg; msg++) {
        T tx_codeword = codec.encode(msg);
        T rx_codeword = flip_random_bits(tx_codeword, codec.get_n(), num_errors);
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
        uint8_t t = 2; // Double-error-correcting code
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
    uint8_t t = 2; // Double-error-correcting code
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

BOOST_AUTO_TEST_CASE(test_bch_encode_decode_u8_array)
{
    typedef uint64_t T;
    typedef uint64_t P;

    // Create a BCH codec with byte-aligned n and k
    gf2_poly<T> prim_poly(0b1000011); // x^6 + x + 1
    galois_field gf(prim_poly);
    uint8_t t = 4; // For t = 4, m*t = 24, so the parity bits are byte-aligned
    bch_codec<T, P> codec(&gf, t, /*n=*/32);
    BOOST_CHECK_EQUAL(codec.get_n(), 32);
    BOOST_CHECK_EQUAL(codec.get_k(), 8);
    BOOST_CHECK(codec.get_n() % 8 == 0);
    BOOST_CHECK(codec.get_k() % 8 == 0);
    uint32_t n_bytes = codec.get_n() / 8;
    uint32_t k_bytes = codec.get_k() / 8;

    // Add up to t errors to each possible message and decode
    T max_msg = (1 << codec.get_k()) - 1;
    for (T msg = 0; msg <= max_msg; msg++) {
        T codeword = codec.encode(msg);
        for (uint8_t num_errors = 0; num_errors <= t; num_errors++) {
            T rx_codeword = flip_random_bits(codeword, codec.get_n(), num_errors);
            u8_vector_t rx_codeword_u8 = to_u8_vector(rx_codeword, n_bytes);
            u8_vector_t decoded_msg(k_bytes);
            codec.decode(rx_codeword_u8.data(), decoded_msg.data());
            BOOST_CHECK_EQUAL(msg, from_u8_vector<T>(decoded_msg));
        }
    }
}

BOOST_AUTO_TEST_CASE(test_bch_correct_single_bit_errors)
{
    typedef uint64_t T;
    typedef uint64_t P;

    // Create a BCH codec with byte-aligned n and k
    gf2_poly<T> prim_poly(0b1000011); // x^6 + x + 1
    galois_field gf(prim_poly);
    uint8_t t = 4; // For t = 4, m*t = 24, so the parity bits are byte-aligned
    bch_codec<T, P> codec(&gf, t, /*n=*/32);
    BOOST_CHECK_EQUAL(codec.get_n(), 32);
    BOOST_CHECK_EQUAL(codec.get_k(), 8);
    BOOST_CHECK(codec.get_n() % 8 == 0);
    BOOST_CHECK(codec.get_k() % 8 == 0);
    uint32_t n_bytes = codec.get_n() / 8;
    uint32_t k_bytes = codec.get_k() / 8;

    // Add all possible single-bit errors and ensure they can be corrected
    T max_msg = (1 << codec.get_k()) - 1;
    for (T msg = 0; msg <= max_msg; msg++) {
        T codeword = codec.encode(msg);
        for (uint32_t bit_pos = 0; bit_pos < codec.get_n(); bit_pos++) {
            u8_vector_t rx_codeword_u8 = to_u8_vector(codeword, n_bytes);
            flip_bit(rx_codeword_u8, bit_pos);
            u8_vector_t decoded_msg(k_bytes);
            int n_corrected = codec.decode(rx_codeword_u8.data(), decoded_msg.data());
            BOOST_CHECK_EQUAL(n_corrected, 1);
            BOOST_CHECK_EQUAL(msg, from_u8_vector<T>(decoded_msg));
        }
    }
}

BOOST_AUTO_TEST_CASE(test_bch_correct_two_bit_errors)
{
    typedef uint64_t T;
    typedef uint64_t P;

    // Create a BCH codec with byte-aligned n and k
    gf2_poly<T> prim_poly(0b1000011); // x^6 + x + 1
    galois_field gf(prim_poly);
    uint8_t t = 4; // For t = 4, m*t = 24, so the parity bits are byte-aligned
    bch_codec<T, P> codec(&gf, t, /*n=*/32);
    BOOST_CHECK_EQUAL(codec.get_n(), 32);
    BOOST_CHECK_EQUAL(codec.get_k(), 8);
    BOOST_CHECK(codec.get_n() % 8 == 0);
    BOOST_CHECK(codec.get_k() % 8 == 0);
    uint32_t n_bytes = codec.get_n() / 8;
    uint32_t k_bytes = codec.get_k() / 8;

    // Add all possible two-bit errors and ensure they can be corrected
    T max_msg = (1 << codec.get_k()) - 1;
    for (T msg = 0; msg <= max_msg; msg++) {
        T codeword = codec.encode(msg);
        for (uint32_t bit1_pos = 0; bit1_pos < codec.get_n(); bit1_pos++) {
            for (uint32_t bit2_pos = bit1_pos + 1; bit2_pos < codec.get_n(); bit2_pos++) {
                u8_vector_t rx_codeword_u8 = to_u8_vector(codeword, n_bytes);
                flip_bit(rx_codeword_u8, bit1_pos);
                flip_bit(rx_codeword_u8, bit2_pos);
                u8_vector_t decoded_msg(k_bytes);
                int n_corrected = codec.decode(rx_codeword_u8.data(), decoded_msg.data());
                BOOST_CHECK_EQUAL(n_corrected, 2);
                BOOST_CHECK_EQUAL(msg, from_u8_vector<T>(decoded_msg));
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(test_bch_encode_decode_u8_array_uncorrectable)
{
    typedef uint64_t T;
    typedef uint64_t P;

    // Create a BCH codec with byte-aligned n and k
    gf2_poly<T> prim_poly(0b1000011); // x^6 + x + 1
    galois_field gf(prim_poly);
    uint8_t t = 4; // For t = 4, m*t = 24, so the parity bits are byte-aligned
    bch_codec<T, P> codec(&gf, t, /*n=*/56);
    uint32_t n_bytes = codec.get_n() / 8;
    uint32_t k_bytes = codec.get_k() / 8;

    // Confirm the minimum distance (Hamming weight of the generator polynomial)
    uint8_t d_min = std::bitset<64>(codec.get_gen_poly().get_poly()).count();
    BOOST_CHECK(d_min >= 2 * t + 1); // valid for BCH with m >= 3 and t < 2^(m -1)

    // Generate a random codeword
    u8_vector_t msg(k_bytes);
    u8_vector_t tx_codeword(n_bytes);
    u8_vector_t rx_codeword(n_bytes);
    fill_random_bytes(msg);
    codec.encode(msg.data(), tx_codeword.data());
    std::copy(tx_codeword.begin(), tx_codeword.end(), rx_codeword.begin());

    // Add a number of random errors that exceeds t but does not exceed d_min so that the
    // result does not end up being another valid codeword
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(t + 1, d_min - 1);
    uint32_t num_errors = dis(gen);
    flip_random_bits(rx_codeword, num_errors);
    BOOST_CHECK_EQUAL(count_errors(tx_codeword, rx_codeword), num_errors);

    // Decode with error correction
    u8_vector_t decoded_msg(k_bytes);
    int n_corrected = codec.decode(rx_codeword.data(), decoded_msg.data());
    BOOST_CHECK(n_corrected == -1); // not all errors corrected
    BOOST_CHECK(from_u8_vector<T>(msg) != from_u8_vector<T>(decoded_msg));

    // Measure the residual errors
    uint32_t n_uncorrected = count_errors(msg, decoded_msg);
    BOOST_CHECK(n_uncorrected > 0);           // some errors left
    BOOST_CHECK(n_uncorrected <= num_errors); // but some could have been corrected
}

void test_dvbs2(const std::string& fecframe_size, uint32_t n, uint8_t t)
{
    // Primitive polynomials
    // - Normal FECFRAME (DVB-S2 Table 6a): x^16 + x^5 + x^3 + x^2 + 1, based on GF(2^16).
    // - Medium FECFRAME (DVB-S2X Table 7): x^15 + x^5 + x^3 + x^2 + 1, based on GF(2^15).
    // - Short FECFRAME (DVB-S2 Table 6b): x^14 + x^5 + x^3 + x + 1, based on GF(2^14).
    uint32_t prim_poly_coefs =
        fecframe_size == "normal"
            ? 0b10000000000101101
            : (fecframe_size == "medium" ? 0b1000000000101101 : 0b100000000101011);
    gf2_poly_u32 prim_poly(prim_poly_coefs);
    galois_field gf(prim_poly);
    bch_codec<uint32_t, bitset256_t> codec(&gf, t, n);
    // NOTE: the generator polynomial can have degree up to 192, so use P=bitset256_t to
    // store it. Also, use T=uint32_t to store the GF(2^m) elements (with up to 16 bits)
    // and to represent the minimal polynomials (with up to 17 bits).
    BOOST_CHECK_EQUAL(codec.get_n(), n);

    // All DVB-S2 codeword and message lengths are byte-aligned
    BOOST_CHECK(codec.get_n() % 8 == 0);
    BOOST_CHECK(codec.get_k() % 8 == 0);

    // Generate a random codeword
    uint32_t k_bytes = codec.get_k() / 8;
    uint32_t n_bytes = codec.get_n() / 8;
    u8_vector_t msg(k_bytes);
    u8_vector_t codeword(n_bytes);
    fill_random_bytes(msg);
    codec.encode(msg.data(), codeword.data());

    // Add up to t random errors
    flip_random_bits(codeword, t);

    // Decode it with error correction
    u8_vector_t decoded_msg(k_bytes);
    int n_corrected = codec.decode(codeword.data(), decoded_msg.data());
    BOOST_CHECK_EQUAL_COLLECTIONS(
        msg.begin(), msg.end(), decoded_msg.begin(), decoded_msg.end());
    BOOST_CHECK_EQUAL(n_corrected, t);
}

BOOST_AUTO_TEST_CASE(test_bch_dvbs2_encode_decode)
{
    const auto params_table = std::vector<std::tuple<std::string, uint32_t, uint8_t>>{
        { "normal", 14400, 12 }, // DVB-S2X Normal 2/9
        { "normal", 16200, 12 }, // DVB-S2 Normal 1/4
        { "normal", 18720, 12 }, // DVB-S2X Normal 13/45
        { "normal", 21600, 12 }, // DVB-S2 Normal 1/3
        { "normal", 25920, 12 }, // DVB-S2 Normal 2/5
        { "normal", 29160, 12 }, // DVB-S2X Normal 9/20
        { "normal", 32400, 12 }, // DVB-S2 Normal 1/2 (or DVB-S2X 90/180)
        { "normal", 34560, 12 }, // DVB-S2X Normal 96/180
        { "normal", 35640, 12 }, // DVB-S2X Normal 11/20
        { "normal", 36000, 12 }, // DVB-S2X Normal 100/180
        { "normal", 37440, 12 }, // DVB-S2X Normal 104/180 and 26/45
        { "normal", 38880, 12 }, // DVB-S2 Normal 3/5 (or DVB-S2X 18/30)
        { "normal", 40320, 12 }, // DVB-S2X Normal 28/45
        { "normal", 41400, 12 }, // DVB-S2X Normal 23/36
        { "normal", 41760, 12 }, // DVB-S2X Normal 116/180
        { "normal", 43200, 10 }, // DVB-S2 Normal 2/3
        { "normal", 43200, 12 }, // DVB-S2X Normal 20/30
        { "normal", 44640, 12 }, // DVB-S2X Normal 124/180
        { "normal", 45000, 12 }, // DVB-S2X Normal 25/36
        { "normal", 46080, 12 }, // DVB-S2X Normal 128/180
        { "normal", 46800, 12 }, // DVB-S2X Normal 13/18
        { "normal", 47520, 12 }, // DVB-S2X Normal 132/180 and 22/30
        { "normal", 48600, 12 }, // DVB-S2 Normal 3/4 (or DVB-S2X 135/180)
        { "normal", 50400, 12 }, // DVB-S2X Normal 140/180 and 7/9
        { "normal", 51840, 12 }, // DVB-S2 Normal 4/5
        { "normal", 54000, 10 }, // DVB-S2 Normal 5/6
        { "normal", 55440, 12 }, // DVB-S2X Normal 154/180
        { "normal", 57600, 8 },  // DVB-S2 Normal 8/9
        { "normal", 58320, 8 },  // DVB-S2 Normal 9/10
        { "short", 3240, 12 },   // DVB-S2 Short 1/4
        { "short", 3960, 12 },   // DVB-S2X Short 11/45
        { "short", 4320, 12 },   // DVB-S2X Short 4/15
        { "short", 5040, 12 },   // DVB-S2X Short 14/45
        { "short", 5400, 12 },   // DVB-S2 Short 1/3
        { "short", 6480, 12 },   // DVB-S2 Short 2/5
        { "short", 7200, 12 },   // DVB-S2 Short 1/2
        { "short", 7560, 12 },   // DVB-S2X Short 7/15
        { "short", 8640, 12 },   // DVB-S2X Short 8/15
        { "short", 9360, 12 },   // DVB-S2X Short 26/45
        { "short", 9720, 12 },   // DVB-S2 Short 3/5
        { "short", 10800, 12 },  // DVB-S2 Short 2/3
        { "short", 11520, 12 },  // DVB-S2X Short 32/45
        { "short", 11880, 12 },  // DVB-S2 Short 3/4
        { "short", 12600, 12 },  // DVB-S2 Short 4/5
        { "short", 13320, 12 },  // DVB-S2 Short 5/6
        { "short", 14400, 12 }   // DVB-S2 Short 8/9
    };
    // TODO support medium FECFRAME with kbch non multiple of 8
    for (const auto& params : params_table) {
        test_dvbs2(std::get<0>(params), std::get<1>(params), std::get<2>(params));
    }
}

} // namespace dvbs2rx
} // namespace gr