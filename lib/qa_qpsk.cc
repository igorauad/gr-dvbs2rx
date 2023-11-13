/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "qpsk.h"
#include <boost/test/unit_test.hpp>
#include <random>

namespace tt = boost::test_tools;

namespace gr {
namespace dvbs2rx {

BOOST_AUTO_TEST_CASE(test_qpsk_map)
{
    QpskConstellation qpsk;
    volk::vector<int8_t> in_bits = { 0, 0, 0, 1, 1, 0, 1, 1 };
    volk::vector<gr_complex> mapped_syms(4);
    qpsk.map(mapped_syms.data(), in_bits);
    volk::vector<gr_complex> expected = {
        { SQRT2_2, SQRT2_2 },  // 00
        { SQRT2_2, -SQRT2_2 }, // 01
        { -SQRT2_2, SQRT2_2 }, // 10
        { -SQRT2_2, -SQRT2_2 } // 11
    };
    BOOST_CHECK_EQUAL_COLLECTIONS(
        mapped_syms.begin(), mapped_syms.end(), expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(test_qpsk_map_inv_convention)
{
    QpskConstellation qpsk;
    volk::vector<int8_t> in_bits = { 0, 0, 0, 1, 1, 0, 1, 1 };
    volk::vector<gr_complex> mapped_syms(4);
    qpsk.map(mapped_syms.data(), in_bits, /*inv_convention=*/true);
    volk::vector<gr_complex> expected = {
        { -SQRT2_2, -SQRT2_2 }, // 00
        { -SQRT2_2, SQRT2_2 },  // 01
        { SQRT2_2, -SQRT2_2 },  // 10
        { SQRT2_2, SQRT2_2 }    // 11
    };
    BOOST_CHECK_EQUAL_COLLECTIONS(
        mapped_syms.begin(), mapped_syms.end(), expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(test_qpsk_slice)
{
    QpskConstellation qpsk;
    volk::vector<gr_complex> in_syms = {
        { 1.0, 1.0 }, { 1.0, -1.0 }, { -1.0, -1.0 }, { -1.0, 1.0 }
    };
    volk::vector<gr_complex> out_syms(4);
    qpsk.slice(out_syms.data(), in_syms);
    volk::vector<gr_complex> expected = { { SQRT2_2, SQRT2_2 },
                                          { SQRT2_2, -SQRT2_2 },
                                          { -SQRT2_2, -SQRT2_2 },
                                          { -SQRT2_2, SQRT2_2 } };
    BOOST_CHECK_EQUAL_COLLECTIONS(
        out_syms.begin(), out_syms.end(), expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(test_qpsk_soft_demap)
{
    QpskConstellation qpsk;
    volk::vector<gr_complex> in_syms = {
        { 1.0, 1.0 }, { 1.0, -1.0 }, { -1.0, -1.0 }, { -1.0, 1.0 }
    };
    volk::vector<int8_t> out_llr(8);
    float scalar = 2 * M_SQRT2; // to get +-1 values
    qpsk.demap_soft(out_llr.data(), in_syms, scalar);
    volk::vector<int8_t> expected = { 1, 1, 1, -1, -1, -1, -1, 1 };
    BOOST_CHECK_EQUAL_COLLECTIONS(
        out_llr.begin(), out_llr.end(), expected.begin(), expected.end());
}

void fill_random_bits(volk::vector<int8_t>& in_bits, size_t n_syms)
{
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution(0, 1);
    in_bits.resize(2 * n_syms);
    for (size_t i = 0; i < 2 * n_syms; i++) {
        in_bits[i] = distribution(generator);
    }
}

void add_noise(volk::vector<gr_complex>& syms, double esn0)
{
    constexpr float es = 1; // asssume unitary Es
    float n0 = es / esn0;
    float sdev_per_dim = sqrt(n0 / 2);

    std::random_device seed; // random seed generator
    std::mt19937 prgn;       // pseudo-random number engine
    std::normal_distribution<float> normal_dist_gen(0, sdev_per_dim);
    for (size_t i = 0; i < syms.size(); i++) {
        syms[i] += gr_complex(normal_dist_gen(prgn), normal_dist_gen(prgn));
    }
}

BOOST_AUTO_TEST_CASE(test_qpsk_snr_estimation)
{
    QpskConstellation qpsk;

    // Random bits
    size_t n_syms = 1000;
    volk::vector<int8_t> in_bits;
    fill_random_bits(in_bits, n_syms);

    // Map to constellation symbols
    volk::vector<gr_complex> in_syms(1000);
    qpsk.map(in_syms.data(), in_bits);

    // Add noise
    double esn0_db = 8;
    double esn0 = pow(10, (esn0_db / 10));
    add_noise(in_syms, esn0);

    // Check the SNR estimate
    float snr_est = qpsk.estimate_snr(in_syms);
    BOOST_TEST(snr_est == esn0, tt::tolerance(0.1));
}

BOOST_AUTO_TEST_CASE(test_qpsk_snr_estimation_llr_ref)
{
    QpskConstellation qpsk;

    // Random bits
    size_t n_syms = 1000;
    volk::vector<int8_t> in_bits;
    fill_random_bits(in_bits, n_syms);

    // Map to constellation symbols
    volk::vector<gr_complex> in_syms(1000);
    qpsk.map(in_syms.data(), in_bits);

    // Map to LLRs
    volk::vector<int8_t> ref_llrs(2 * n_syms);
    qpsk.demap_soft(ref_llrs.data(), in_syms, /*scalar=*/1.0);

    // Add noise
    double esn0_db = 8;
    double esn0 = pow(10, (esn0_db / 10));
    add_noise(in_syms, esn0);

    // Check the SNR estimate
    float snr_est = qpsk.estimate_snr(in_syms, ref_llrs);
    BOOST_TEST(snr_est == esn0, tt::tolerance(0.1));
}


} // namespace dvbs2rx
} // namespace gr