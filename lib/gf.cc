/* -*- c++ -*- */
/*
 * Copyright (c) 2022 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gf.h"
#include <cassert>

namespace gr {
namespace dvbs2rx {

galois_field::galois_field(uint8_t m, uint16_t prim_poly)
    : m_m(m), m_prim_poly(prim_poly), m_two_to_m_minus_one((1 << m) - 1)
{
    if (m > 16)
        throw std::runtime_error("Please choose m <= 16");

    // Table of GF(2^m) elements
    //
    // The first element is the additive identity (0), the second is the multiplicative
    // identity (1), which is also alpha^0, i.e., the primitive element to the power zero.
    // The remaining elements are alpha^j (the primitive element to the j-th power), and
    // these are generated iteratively by an LFSR. See the comments on function gf_mtx()
    // of the reference Python implementation.
    m_table[0] = 0;
    m_table[1] = 1;
    for (unsigned int i = 1; i < m_two_to_m_minus_one; i++)
        m_table[i + 1] = ((m_table[i] << 1) & m_two_to_m_minus_one) ^
                         ((m_table[i] >> (m - 1)) * m_prim_poly);

    // Inverse LUT: maps element alpha^i to its m_table index.
    //
    // Since m_table has element alpha^0 at index=1, alpha^1 at index=2, and so on, this
    // function generates an array whose value at position "alpha^i" is the index "i+1",
    // i.e., "x[alpha^i] = i + 1".
    for (unsigned int i = 1; i <= m_two_to_m_minus_one; i++) {
        for (uint16_t beta = 1; beta <= m_two_to_m_minus_one; beta++) {
            if (m_table[i] == beta) {
                m_inv_table[beta] = i;
                break;
            }
        }
    }
}

uint16_t galois_field::operator[](int index) const
{
    assert(index <= m_two_to_m_minus_one);
    return m_table[index];
}

uint16_t galois_field::get_alpha_i(uint16_t i) const
{
    return m_table[(i % m_two_to_m_minus_one) + 1];
}

uint16_t galois_field::get_exponent(uint16_t beta) const
{
    if (beta == 0)
        throw std::runtime_error("Zero element does not have an exponent");
    // Recall alpha^i is stored at position i + 1.
    return m_inv_table[beta] - 1;
}

uint16_t galois_field::multiply(uint16_t a, uint16_t b) const
{
    if (a == 0 || b == 0)
        return 0;
    assert(a <= m_two_to_m_minus_one && b <= m_two_to_m_minus_one);
    return get_alpha_i(get_exponent(a) + get_exponent(b));
}

uint16_t galois_field::inverse(uint16_t beta) const
{
    // We want "beta^-1" such that "beta * beta^-1 = 1". For that, we use the property
    // that any GF(2^m) element raised to the power "2^m - 1" is equal to one, i.e.,
    // "beta^(2^m - 1) = 1". Hence, if beta is alpha^j (the j-th power of the primitive
    // element), then beta^-1 must be the element alpha^k such that "j + k = 2^m - 1".
    return get_alpha_i(m_two_to_m_minus_one - get_exponent(beta));
}

uint16_t galois_field::divide(uint16_t a, uint16_t b) const
{
    return multiply(a, inverse(b));
}

} // namespace dvbs2rx
} // namespace gr