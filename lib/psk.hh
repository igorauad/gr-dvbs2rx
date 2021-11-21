/* -*- c++ -*- */
/*
 * Copyright 2018 Ahmet Inan, Ron Economos.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PSK_HH
#define PSK_HH

#include "modulation.hh"
#include <gnuradio/gr_complex.h>
#include <type_traits>
#include <algorithm>
#include <cmath>

template <int NUM, typename TYPE, typename CODE>
struct PhaseShiftKeying;

template <typename TYPE, typename CODE>
struct PhaseShiftKeying<2, TYPE, CODE> : public Modulation<TYPE, CODE> {
    static const int NUM = 2;
    static const int BITS = 1;
    typedef TYPE complex_type;
    typedef typename TYPE::value_type value_type;
    typedef CODE code_type;

    static constexpr value_type DIST = 2;

    static code_type quantize(value_type precision, value_type value)
    {
        value *= DIST * precision;
        if (std::is_integral<code_type>::value)
            value = std::nearbyint(value);
        if (std::is_same<code_type, int8_t>::value)
            value = std::min<value_type>(std::max<value_type>(value, -128), 127);
        return value;
    }

    int bits() { return BITS; }

    void hard(code_type* b, complex_type c)
    {
        b[0] = c.real() < value_type(0) ? code_type(-1) : code_type(1);
    }

    void soft(code_type* b, complex_type c, value_type precision)
    {
        b[0] = quantize(precision, c.real());
    }

    complex_type map(code_type* b) { return complex_type(b[0], 0); }
};

template <typename TYPE, typename CODE>
struct PhaseShiftKeying<4, TYPE, CODE> : public Modulation<TYPE, CODE> {
    static const int NUM = 4;
    static const int BITS = 2;
    typedef TYPE complex_type;
    typedef typename TYPE::value_type value_type;
    typedef CODE code_type;

    // 1/sqrt(2)
    static constexpr value_type rcp_sqrt_2 = 0.70710678118654752440;
    static constexpr value_type DIST = 2 * rcp_sqrt_2;

    static code_type quantize(value_type precision, value_type value)
    {
        value *= DIST * precision;
        if (std::is_integral<code_type>::value)
            value = std::nearbyint(value);
        if (std::is_same<code_type, int8_t>::value)
            value = std::min<value_type>(std::max<value_type>(value, -128), 127);
        return value;
    }

    int bits() { return BITS; }

    void hard(code_type* b, complex_type c)
    {
        b[0] = c.real() < value_type(0) ? code_type(-1) : code_type(1);
        b[1] = c.imag() < value_type(0) ? code_type(-1) : code_type(1);
    }

    void soft(code_type* b, complex_type c, value_type precision)
    {
        b[0] = quantize(precision, c.real());
        b[1] = quantize(precision, c.imag());
    }

    complex_type map(code_type* b) { return rcp_sqrt_2 * complex_type(b[0], b[1]); }
};

template <typename TYPE, typename CODE>
struct PhaseShiftKeying<8, TYPE, CODE> : public Modulation<TYPE, CODE> {
    static const int NUM = 8;
    static const int BITS = 3;
    typedef TYPE complex_type;
    typedef typename TYPE::value_type value_type;
    typedef CODE code_type;

    // c(a(1)/2)
    static constexpr value_type cos_pi_8 = 0.92387953251128675613;
    // s(a(1)/2)
    static constexpr value_type sin_pi_8 = 0.38268343236508977173;
    // 1/sqrt(2)
    static constexpr value_type rcp_sqrt_2 = 0.70710678118654752440;

    static constexpr value_type DIST = 2 * sin_pi_8;

    complex_type rot = (complex_type)(std::exp(gr_complexd(0.0, -M_PI / 8)));
    complex_type m_8psk[8] = { complex_type(rcp_sqrt_2, rcp_sqrt_2),
                               complex_type(1.0, 0.0),
                               complex_type(-1.0, 0.0),
                               complex_type(-rcp_sqrt_2, -rcp_sqrt_2),
                               complex_type(0.0, 1.0),
                               complex_type(rcp_sqrt_2, -rcp_sqrt_2),
                               complex_type(-rcp_sqrt_2, rcp_sqrt_2),
                               complex_type(0.0, -1.0) };

    static code_type quantize(value_type precision, value_type value)
    {
        value *= DIST * precision;
        if (std::is_integral<code_type>::value)
            value = std::nearbyint(value);
        if (std::is_same<code_type, int8_t>::value)
            value = std::min<value_type>(std::max<value_type>(value, -128), 127);
        return value;
    }

    int bits() { return BITS; }

    void hard(code_type* b, complex_type c)
    {
        c *= rot;
        b[1] = c.real() < value_type(0) ? code_type(-1) : code_type(1);
        b[2] = c.imag() < value_type(0) ? code_type(-1) : code_type(1);
        b[0] = std::abs(c.real()) < std::abs(c.imag()) ? code_type(-1) : code_type(1);
    }

    void soft(code_type* b, complex_type c, value_type precision)
    {
        c *= rot;
        b[1] = quantize(precision, c.real());
        b[2] = quantize(precision, c.imag());
        b[0] =
            quantize(precision, rcp_sqrt_2 * (std::abs(c.real()) - std::abs(c.imag())));
    }

    complex_type map(code_type* b)
    {
        int index = ((((int)(b[0]) + 1) << 1) ^ 0x4) | (((int)(b[1]) + 1) ^ 0x2) |
                    ((((int)(b[2]) + 1) >> 1) ^ 0x1);
        return complex_type(m_8psk[index]);
    }
};

#endif
