/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "crc.h"
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

namespace gr {
namespace dvbs2rx {

// Based on the examples in
// http://www.sunshine2k.de/articles/coding/crc/understanding_crc.html

BOOST_AUTO_TEST_CASE(test_crc8)
{
    const uint8_t gen_poly = 0x1D; // excluding the MSB
    const auto crc_lut = build_crc_lut<uint8_t>(gen_poly);

    std::vector<uint8_t> in_bytes = { 0x01, 0x02 };
    BOOST_CHECK_EQUAL(calc_crc(in_bytes, crc_lut), 0x76);
}

BOOST_AUTO_TEST_CASE(test_crc16)
{
    const uint16_t gen_poly = 0x1021; // excluding the MSB
    const auto crc_lut = build_crc_lut<uint16_t>(gen_poly);

    std::vector<uint8_t> in_bytes = { 0x01, 0x02 };
    BOOST_CHECK_EQUAL(calc_crc(in_bytes, crc_lut), 0x1373);
}

} // namespace dvbs2rx
} // namespace gr