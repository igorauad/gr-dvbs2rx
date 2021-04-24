/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "delay_line.h"
#include <gnuradio/attributes.h>
#include <volk/volk_alloc.hh>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

namespace bdata = boost::unit_test::data;

namespace gr {
namespace dvbs2rx {

// Test delay line allocated with varying number of L-length segment repetitions
BOOST_DATA_TEST_CASE(test_delay_line_values, bdata::make({ 1, 10, 100 }), n_reps)
{
    unsigned int len = 4;
    delay_line<float> d_line(len, n_reps);

    // Random vector with the same length as the delay line
    volk::vector<float> vec(len);
    for (size_t i = 0; i < vec.size(); ++i) {
        vec[i] = rand();
    }

    // While filling the first L-1 elements from vec, the delay line's front
    // element is still 0
    for (size_t i = 0; i < vec.size() - 1; ++i) {
        d_line.push(vec[i]);
        BOOST_CHECK(d_line.back() == vec[i]);
        BOOST_CHECK(d_line.front() == 0);
    }

    // When the last element from vec is pushed, the delay line becomes full,
    // and the oldest element (vec[0]) appears at the front of the queue
    d_line.push(vec[len - 1]);
    BOOST_CHECK(d_line.front() == vec[0]);

    // Now that the delay line is full, whenever a new element is pushed into
    // it, the oldest element is thrown away
    for (size_t i = 1; i < vec.size(); ++i) {
        float val = rand();
        d_line.push(val);
        BOOST_CHECK(d_line.back() == val);
        BOOST_CHECK(d_line.front() == vec[i]);
    }
}

// Check the delay line behavior in comparison to a regular ring buffer
BOOST_DATA_TEST_CASE(test_delay_line_vs_ring_buffer, bdata::make({ 1, 10, 100 }), n_reps)
{
    // Delay line object
    unsigned int len = 4;
    delay_line<int> d_line(len, n_reps);

    // Ordinary ring buffer
    int ring_buffer[len] = {};
    unsigned int i_tail = 0;
    unsigned int i_head = len - 1;

    // Test an arbitrarily large number of samples
    int n_samples = n_reps * len * 100;
    int i_sample = 0;
    while (i_sample < n_samples) {
        int val = rand();
        ring_buffer[i_tail] = val;
        d_line.push(val);
        BOOST_CHECK(d_line.back() == ring_buffer[i_tail]);
        BOOST_CHECK(d_line.front() == ring_buffer[i_head]);
        i_tail = (i_tail - 1) % len;
        i_head = (i_head - 1) % len;
        i_sample++;
    }
}

void convolution_test(std::vector<float>& in,
                      volk::vector<float>& taps,
                      std::vector<float>& expected)
{
    // Define a delay line with a length corresponding to the number of filter taps
    delay_line<float> d_line(taps.size());

    // Run the convolution
    unsigned int conv_len = taps.size() + in.size() - 1;
    for (unsigned int i = 0; i < conv_len; i++) {
        if (i < in.size()) {
            d_line.push(in[i]);
        } else {
            d_line.push(0);
        }
        float result;
        volk_32f_x2_dot_prod_32f(&result, &d_line.back(), taps.data(), d_line.length());
        BOOST_CHECK(result == expected[i]);
    }
}

// Use the delay line to compute a convolution
// Numpy check: np.convolve([1, 2, 3], [0, 1, 0.5])
BOOST_AUTO_TEST_CASE(test_delay_line_conv)
{
    // Filter input, filter taps, and expected output
    std::vector<float> in = { 1, 2, 3 };
    volk::vector<float> taps = { 0, 1, 0.5 };
    std::vector<float> expected = { 0, 1, 2.5, 4, 1.5 };
    convolution_test(in, taps, expected);
}

// Use the delay line to compute a cross-correlation
// Numpy check: np.correlate([1, 2, 3], [0, 1, 0.5], "full")
BOOST_AUTO_TEST_CASE(test_delay_line_corr)
{
    // Filter input, correlating sequence, and expected output
    std::vector<float> in = { 1, 2, 3 };
    volk::vector<float> corr_sequence = { 0, 1, 0.5 };
    std::vector<float> expected = { 0.5, 2, 3.5, 3, 0 };

    // Compute the cross-correlation using a convolution with the folded version
    // of the correlating sequence as the sequence of "filter taps"
    volk::vector<float> taps = corr_sequence;
    std::reverse(taps.begin(), taps.end());
    convolution_test(in, taps, expected);
}

} // namespace dvbs2rx
} // namespace gr
