/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "reed_muller.h"
#include <volk/volk.h>
#include <boost/test/unit_test.hpp>
#include <set>

namespace gr {
namespace dvbs2rx {

BOOST_AUTO_TEST_CASE(test_reed_muller)
{
    reed_muller codec;
    uint8_t max_correctable_errors = 15; // floor((min_distance - 1) / 2)

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
        uint8_t dataword = codec.decode(noisy_codeword);
        BOOST_CHECK_EQUAL(dataword, i);
    }
}

} // namespace dvbs2rx
} // namespace gr
