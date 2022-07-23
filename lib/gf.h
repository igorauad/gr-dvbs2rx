/* -*- c++ -*- */
/*
 * Copyright (c) 2022 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_GF_H
#define INCLUDED_DVBS2RX_GF_H

#include <gnuradio/dvbs2rx/api.h>
#include <array>
#include <cstdint>

namespace gr {
namespace dvbs2rx {

#define MAX_M 16
#define MAX_FIELD_ELEMENTS 1 << MAX_M

/**
 * @brief Galois Field GF(2^m)
 *
 * @note See the reference implementation at
 * https://github.com/igorauad/bch/blob/master/gf.py.
 */
class DVBS2RX_API galois_field
{
private:
    uint8_t m_m;                   // dimension of the GF(2^m) field
    uint16_t m_prim_poly;          // 16-bit primitive polynomial excluding the MSB
    uint16_t m_two_to_m_minus_one; // shortcut for (2^m - 1)
    std::array<uint16_t, MAX_FIELD_ELEMENTS> m_table; // field elements
    std::array<uint16_t, MAX_FIELD_ELEMENTS>
        m_inv_table; // LUT to map element alpha^i to its GF table index i+1


public:
    /**
     * @brief Construct a new Galois field object
     *
     * @param m GF(2^m) field dimension up to 16.
     * @param prim_poly Primitive polynomial with the lowest order term on the right (lsb)
     * and excluding the highest order term x^m. For instance, a polynomial "x^4 + x + 1"
     * should be given as "0011" instead of "10011".
     */
    galois_field(uint8_t m, uint16_t prim_poly);

    /**
     * @brief Get the GF(2^m) element at a given index on the elements table
     *
     * @param index Target index.
     * @return uint16_t GF(2^m) element.
     */
    uint16_t operator[](int index) const;

    /**
     * @brief Get the i-th power of the primitive element (alpha^i)
     *
     * @param i Exponent i of the target element alpha^i for i from 0 to "2**m - 2".
     * @return uint16_t Element alpha^i.
     */
    uint16_t get_alpha_i(uint16_t i) const;

    /**
     * @brief Get the exponent i of a given element beta = alpha^i.
     *
     * @param beta Element beta that is a power of the primitive element alpha.
     * @return uint16_t Exponent i.
     */
    uint16_t get_exponent(uint16_t beta) const;

    /**
     * @brief Multiply two elements from GF(2^m)
     *
     * @param a First multiplicand.
     * @param b Second multiplicand.
     * @return uint16_t Product a*b.
     */
    uint16_t multiply(uint16_t a, uint16_t b) const;

    /**
     * @brief Get the inverse beta^-1 from a GF(2^m) element beta.
     *
     * @param beta Element to invert.
     * @return uint16_t Inverse beta^-1.
     */
    uint16_t inverse(uint16_t beta) const;

    /**
     * @brief Divide two elements from GF(2^m)
     *
     * @param a Dividend.
     * @param b Divisor.
     * @return uint16_t Quotient a/b.
     */
    uint16_t divide(uint16_t a, uint16_t b) const;
};

} // namespace dvbs2rx
} // namespace gr

#endif // INCLUDED_DVBS2RX_GF_H