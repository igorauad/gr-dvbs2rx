/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "delay_line.h"
#include <volk/volk_alloc.hh>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

namespace bdata = boost::unit_test::data;

namespace gr {
namespace dvbs2rx {

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
