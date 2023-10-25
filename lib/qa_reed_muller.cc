/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "reed_muller.h"
#include <volk/volk.h>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>
#include <set>

namespace bdata = boost::unit_test::data;

namespace gr {
namespace dvbs2rx {

BOOST_DATA_TEST_CASE(test_reed_muller, bdata::make({ false, true }), soft)
{
    reed_muller codec;
    uint8_t max_correctable_errors = 15; // floor((min_distance - 1) / 2)
    volk::vector<float> soft_decisions(64);

    for (uint8_t i = 0; i < n_plsc_codewords; i++) {
        // Encode
        uint64_t codeword = codec.encode(i);

        // Add noise (flip max_correctable_errors bits)
        std::set<int> set_err_idx; // set of indexes with bit errors
        while (set_err_idx.size() < max_correctable_errors) {
            set_err_idx.insert((rand() % 64));
        }
        uint64_t noisy_codeword = codeword;
        for (int idx : set_err_idx) {
            noisy_codeword ^= 1ull << idx;
        }

        // Double check the Hamming distance
        uint64_t hamming_distance;
        volk_64u_popcnt(&hamming_distance, codeword ^ noisy_codeword);
        BOOST_CHECK_EQUAL(hamming_distance, max_correctable_errors);

        // Decode the noisy codeword
        uint8_t dataword;
        if (soft) {
            // Convert the noisy codeword into a real vector in Euclidean
            // space. Consider that as equivalent to noisy soft decisions.
            codec.euclidean_map(soft_decisions.data(), noisy_codeword);
            // Next, decode using the soft decisions:
            dataword = codec.decode(soft_decisions.data());
        } else {
            // The noisy binary codeword is equivalent to a set of 64 noisy hard
            // decisions. Decode using them:
            dataword = codec.decode(noisy_codeword);
        }
        BOOST_CHECK_EQUAL(dataword, i);
    }
}

BOOST_DATA_TEST_CASE(test_reed_muller_codeword_subset, bdata::make({ false, true }), soft)
{
    reed_muller codec({ 0, 32, 64, 96 });
    volk::vector<float> soft_decisions(64);

    for (uint8_t i = 0; i < n_plsc_codewords; i++) {
        // Encode
        uint64_t codeword = codec.encode(i);

        // Decode the codeword as-is (error-free)
        uint8_t dataword;
        if (soft) {
            codec.euclidean_map(soft_decisions.data(), codeword);
            dataword = codec.decode(soft_decisions.data());
        } else {
            dataword = codec.decode(codeword);
        }

        // The decoder can only return results within the selected codeword subset
        if (i != 0 && i != 32 && i != 64 && i != 96) {
            BOOST_CHECK(i != dataword); // i not in the subset
        } else {
            BOOST_CHECK_EQUAL(dataword, i); // i in the subset
        }
    }
}

BOOST_AUTO_TEST_CASE(test_reed_muller_codeword_subset_validation)
{
    BOOST_CHECK_THROW(reed_muller codec({ 0, 64, 128 }), std::runtime_error);
    BOOST_CHECK_THROW(reed_muller codec({ 0, 64, 255 }), std::runtime_error);
    BOOST_CHECK_NO_THROW(reed_muller codec({ 0, 64, 127 }));
}

} // namespace dvbs2rx
} // namespace gr
