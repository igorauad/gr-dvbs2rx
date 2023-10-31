/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "bitset256.h"
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <set>


namespace gr {
namespace dvbs2rx {

BOOST_AUTO_TEST_CASE(test_bitset256_constructors)
{
    bitset256_t x1; // Default constructor
    BOOST_CHECK(x1.get_word(0) == 0);
    BOOST_CHECK(x1.get_word(1) == 0);
    BOOST_CHECK(x1.get_word(2) == 0);
    BOOST_CHECK(x1.get_word(3) == 0);

    bitset256_t x2(0x0102030405060708); // Constructor with uint64_t
    BOOST_CHECK(x2.get_word(0) == 0x0102030405060708);
    BOOST_CHECK(x2.get_word(1) == 0);
    BOOST_CHECK(x2.get_word(2) == 0);
    BOOST_CHECK(x2.get_word(3) == 0);

    bitset256_t x3(x2); // Copy constructor
    BOOST_CHECK(x3.get_word(0) == x2.get_word(0));
    BOOST_CHECK(x3.get_word(1) == x2.get_word(1));
    BOOST_CHECK(x3.get_word(2) == x2.get_word(2));
    BOOST_CHECK(x3.get_word(3) == x2.get_word(3));
}

BOOST_AUTO_TEST_CASE(test_bitset256_shift)
{
    bitset256_t x(1);

    // Shift over the entire range
    for (int i = 0; i < 256; i++) {
        bitset256_t y = x << i;
        BOOST_CHECK(y.test(i));
        if (!y.test(i))
            std::cout << "i = " << i << std::endl;

        for (int j = 0; j < 256; j++) {
            if (j != i) {
                BOOST_CHECK(!y.test(j));
                if (y.test(j))
                    std::cout << "i = " << i << ", j = " << j << std::endl;
            }
        }
    }

    // No shift and full shift
    BOOST_CHECK(x << 0 == 1);
    BOOST_CHECK(x << 256 == 0);

    bitset256_t y;
    y.set_word(0, 0x0A0B0C0D01020305);
    y.set_word(1, 0x05060708A0B0C0D0);
    y.set_word(2, 0x1020304050607080);
    y.set_word(3, 0xA1B1C1D111213141);

    bitset256_t z1 = y << 8;
    BOOST_CHECK(z1.get_word(0) == 0x0B0C0D0102030500);
    BOOST_CHECK(z1.get_word(1) == 0x060708A0B0C0D00A);
    BOOST_CHECK(z1.get_word(2) == 0x2030405060708005);
    BOOST_CHECK(z1.get_word(3) == 0xB1C1D11121314110);

    bitset256_t z2 = y << 72;
    BOOST_CHECK(z2.get_word(0) == 0);
    BOOST_CHECK(z2.get_word(1) == 0x0B0C0D0102030500);
    BOOST_CHECK(z2.get_word(2) == 0x060708A0B0C0D00A);
    BOOST_CHECK(z2.get_word(3) == 0x2030405060708005);

    bitset256_t z3 = y << 136;
    BOOST_CHECK(z3.get_word(0) == 0);
    BOOST_CHECK(z3.get_word(1) == 0);
    BOOST_CHECK(z3.get_word(2) == 0x0B0C0D0102030500);
    BOOST_CHECK(z3.get_word(3) == 0x060708A0B0C0D00A);

    bitset256_t z4 = y << 200;
    BOOST_CHECK(z4.get_word(0) == 0);
    BOOST_CHECK(z4.get_word(1) == 0);
    BOOST_CHECK(z4.get_word(2) == 0);
    BOOST_CHECK(z4.get_word(3) == 0x0B0C0D0102030500);

    bitset256_t z5 = y << 255;
    BOOST_CHECK(z5.get_word(0) == 0);
    BOOST_CHECK(z5.get_word(1) == 0);
    BOOST_CHECK(z5.get_word(2) == 0);
    BOOST_CHECK(z5.get_word(3) == 0x8000000000000000);
}

BOOST_AUTO_TEST_CASE(test_bitset256_xor_binary)
{
    bitset256_t x1;
    x1.set_word(0, 0xF0F0F0F0F0F0F0F0);
    x1.set_word(1, 0xE0E0E0E0E0E0E0E0);
    x1.set_word(2, 0xD0D0D0D0D0D0D0D0);
    x1.set_word(3, 0xC0C0C0C0C0C0C0C0);

    bitset256_t x2;
    x2.set_word(0, 0x0F0F0F0F0F0F0F0F);
    x2.set_word(1, 0x0E0E0E0E0E0E0E0E);
    x2.set_word(2, 0x0D0D0D0D0D0D0D0D);
    x2.set_word(3, 0x0C0C0C0C0C0C0C0C);

    bitset256_t x3 = x1 ^ x2;
    BOOST_CHECK(x3.get_word(0) == 0xFFFFFFFFFFFFFFFF);
    BOOST_CHECK(x3.get_word(1) == 0xEEEEEEEEEEEEEEEE);
    BOOST_CHECK(x3.get_word(2) == 0xDDDDDDDDDDDDDDDD);
    BOOST_CHECK(x3.get_word(3) == 0xCCCCCCCCCCCCCCCC);
}


BOOST_AUTO_TEST_CASE(test_bitset256_xor_equal)
{
    bitset256_t x1;
    x1.set_word(0, 0xF0F0F0F0F0F0F0F0);
    x1.set_word(1, 0xE0E0E0E0E0E0E0E0);
    x1.set_word(2, 0xD0D0D0D0D0D0D0D0);
    x1.set_word(3, 0xC0C0C0C0C0C0C0C0);

    bitset256_t x2;
    x2.set_word(0, 0x0F0F0F0F0F0F0F0F);
    x2.set_word(1, 0x0E0E0E0E0E0E0E0E);
    x2.set_word(2, 0x0D0D0D0D0D0D0D0D);
    x2.set_word(3, 0x0C0C0C0C0C0C0C0C);

    x1 ^= x2;
    BOOST_CHECK(x1.get_word(0) == 0xFFFFFFFFFFFFFFFF);
    BOOST_CHECK(x1.get_word(1) == 0xEEEEEEEEEEEEEEEE);
    BOOST_CHECK(x1.get_word(2) == 0xDDDDDDDDDDDDDDDD);
    BOOST_CHECK(x1.get_word(3) == 0xCCCCCCCCCCCCCCCC);
}

BOOST_AUTO_TEST_CASE(test_bitset256_or_equal)
{
    bitset256_t x1;
    x1.set_word(0, 0xF0F0F0F0F0F0F0F0);
    x1.set_word(1, 0xE0E0E0E0E0E0E0E0);
    x1.set_word(2, 0xD0D0D0D0D0D0D0D0);
    x1.set_word(3, 0xC0C0C0C0C0C0C0C0);

    bitset256_t x2;
    x2.set_word(0, 0x0F0F0F0F0F0F0F0F);
    x2.set_word(1, 0x0E0E0E0E0E0E0E0E);
    x2.set_word(2, 0x0D0D0D0D0D0D0D0D);
    x2.set_word(3, 0x0C0C0C0C0C0C0C0C);

    x1 |= x2;
    BOOST_CHECK(x1.get_word(0) == 0xFFFFFFFFFFFFFFFF);
    BOOST_CHECK(x1.get_word(1) == 0xEEEEEEEEEEEEEEEE);
    BOOST_CHECK(x1.get_word(2) == 0xDDDDDDDDDDDDDDDD);
    BOOST_CHECK(x1.get_word(3) == 0xCCCCCCCCCCCCCCCC);
}

BOOST_AUTO_TEST_CASE(test_bitset256_and)
{
    bitset256_t x1;
    x1.set_word(0, 0xF0F0F0F0F0F0F0F0);
    x1.set_word(1, 0xE0E0E0E0E0E0E0E0);
    x1.set_word(2, 0xD0D0D0D0D0D0D0D0);
    x1.set_word(3, 0xC0C0C0C0C0C0C0C0);

    bitset256_t x2;
    x2.set_word(0, 0x0F0F0F0F0F0F0F0F);
    x2.set_word(1, 0x0E0E0E0E0E0E0E0E);
    x2.set_word(2, 0x0D0D0D0D0D0D0D0D);
    x2.set_word(3, 0x0C0C0C0C0C0C0C0C);

    bitset256_t x3 = x1 & x2;
    BOOST_CHECK(x3.get_word(0) == 0);
    BOOST_CHECK(x3.get_word(1) == 0);
    BOOST_CHECK(x3.get_word(2) == 0);
    BOOST_CHECK(x3.get_word(3) == 0);
}

BOOST_AUTO_TEST_CASE(test_bitset256_set_test_access)
{
    bitset256_t x1;
    std::set<std::size_t> positions = { 0,          63,         (64 + 4),  (64 + 32),
                                        (128 + 12), (128 + 48), (192 + 36) };

    for (auto i : positions)
        x1.set(i);

    for (int i = 0; i < 256; i++) {
        if (positions.find(i) == positions.end()) {
            BOOST_CHECK(!x1.test(i));
            BOOST_CHECK(!x1[i]);
        } else {
            BOOST_CHECK(x1.test(i));
            BOOST_CHECK(x1[i]);
        }
    }
}

BOOST_AUTO_TEST_CASE(test_bitset256_equal_comp)
{
    bitset256_t x1;
    x1.set_word(0, 0xF0F0F0F0F0F0F0F0);
    x1.set_word(1, 0xE0E0E0E0E0E0E0E0);
    x1.set_word(2, 0xD0D0D0D0D0D0D0D0);
    x1.set_word(3, 0xC0C0C0C0C0C0C0C0);

    bitset256_t x2;
    x2.set_word(0, 0x0F0F0F0F0F0F0F0F);
    x2.set_word(1, 0x0E0E0E0E0E0E0E0E);
    x2.set_word(2, 0x0D0D0D0D0D0D0D0D);
    x2.set_word(3, 0x0C0C0C0C0C0C0C0C);

    bitset256_t x3 = x1;

    BOOST_CHECK(x1 == x3);
    BOOST_CHECK(!(x1 == x2));
}

BOOST_AUTO_TEST_CASE(test_bitset256_get_byte)
{
    bitset256_t x1;
    x1.set_word(0, 0x0807060504030201);
    x1.set_word(1, 0x100F0E0D0C0B0A09);
    x1.set_word(2, 0x1817161514131211);
    x1.set_word(3, 0x201F1E1D1C1B1A19);

    for (int i = 0; i < 32; i++) {
        BOOST_CHECK(x1.get_byte(i) == i + 1);
    }
}


} // namespace dvbs2rx
} // namespace gr