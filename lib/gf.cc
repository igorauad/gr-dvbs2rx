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
#include <stdexcept>

namespace gr {
namespace dvbs2rx {

/********** Galois Field GF(2^m) **********/

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

std::set<uint16_t> galois_field::get_conjugates(uint16_t beta) const
{
    std::set<uint16_t> conjugates;

    // The set of conjugates always includes the original element.
    conjugates.insert(beta);

    // The conjugates of alpha^i are the distinct elements "alpha^i^(2^j)".
    uint16_t i = get_exponent(beta);
    for (uint8_t j = 1; j < m_m; j++) { // alpha^i can have up to m conjugates
        uint16_t conjugate = get_alpha_i(i * (1 << j));
        if (conjugates.count(conjugate) > 0)
            break;
        conjugates.insert(conjugate);
    }

    return conjugates;
}

gf2_poly galois_field::get_min_poly(uint16_t beta) const
{
    if (beta == 0) { // 0 is always a root of "f(x) = x"
        return 0b10;
    }
    // The minimal polynomial is the product of the terms "(x + beta^(2^l))" for each
    // distinct conjugate of beta given by beta^(2^l).
    const auto conjugates = get_conjugates(beta);
    auto prod = gf2m_poly(this, { 1 });
    for (const uint16_t& beta_2_l : conjugates) {
        prod = prod * gf2m_poly(this, { beta_2_l, 1 });
    }
    return gf2_poly(prod);
}


/********** Polynomial over GF(2) **********/


gf2_poly::gf2_poly(uint16_t coefs) : m_poly(coefs) { set_degree(); }

gf2_poly::gf2_poly(const gf2m_poly& poly) : m_poly(0)
{
    if (poly.degree() > m_max_degree)
        throw std::runtime_error("GF(2^m) polynomial degree exceeds max GF(2) degree");

    const auto& poly_coefs = poly.get_poly();
    for (int i = poly.degree(); i >= 0; i--) {
        if (poly_coefs[i] > 1) {
            throw std::runtime_error(
                "Trying to reduce non-binary GF(2^m) polynomial to GF(2)");
        }
        if (poly_coefs[i])
            m_poly ^= 1 << i;
    }
    set_degree();
}

void gf2_poly::set_degree()
{
    if (m_poly == 0) {
        m_degree = -1; // convention for the zero polynomial
        return;
    }

    for (int i = 0; i < 16; i++) {
        if (m_poly & (1 << i))
            m_degree = i;
    }
}

gf2_poly gf2_poly::operator+(const gf2_poly& x) const
{
    return gf2_poly(m_poly ^ x.get_poly());
}

gf2_poly gf2_poly::operator*(bool x) const { return gf2_poly(m_poly * x); }

gf2_poly gf2_poly::operator*(const gf2_poly& x) const
{
    if (m_degree + x.degree() > m_max_degree)
        throw std::runtime_error("GF(2) polynomial product exceeds max degree");

    uint16_t x_coefs = x.get_poly();
    uint16_t res;
    for (int i = 0; i < 16; i++) {
        if (x_coefs & (1 << i)) {
            res ^= m_poly << i;
        }
    }
    return gf2_poly(res);
}

bool gf2_poly::operator==(const gf2_poly& x) const { return m_poly == x.get_poly(); }


/********** Polynomial over GF(2^m) **********/


gf2m_poly::gf2m_poly(const galois_field* const gf, std::vector<uint16_t>&& coefs)
    : m_gf(gf), m_poly(std::move(coefs))
{
    // Remove any leading zeros and set the polynomial degree
    m_degree = m_poly.size() - 1;
    while (!m_poly.empty() && m_poly[m_degree] == 0) {
        m_poly.pop_back();
        m_degree--;
    }
}

gf2m_poly gf2m_poly::operator+(const gf2m_poly& x) const
{
    auto a = m_poly;
    auto b = x.get_poly();

    // Pad the shortest polynomial if they don't have the same length
    int n_pad = abs(a.size() - b.size());
    auto& pad_poly = (a.size() > b.size()) ? b : a;
    while (n_pad--) {
        pad_poly.push_back(0);
    }

    // The coefficients of same degree add to each other modulo-2
    std::vector<uint16_t> res(pad_poly.size());
    for (size_t i = 0; i < res.size(); i++) {
        res[i] = a[i] ^ b[i];
    }

    return gf2m_poly(m_gf, std::move(res));
}

gf2m_poly gf2m_poly::operator*(uint16_t x) const
{
    auto a = m_poly;
    for (size_t i = 0; i < a.size(); i++) {
        a[i] = m_gf->multiply(a[i], x);
    }
    return gf2m_poly(m_gf, std::move(a));
}

gf2m_poly gf2m_poly::operator*(const gf2m_poly& x) const
{
    const auto& a = m_poly;
    const auto& b = x.get_poly();

    uint16_t prod_len = a.size() + b.size() - 1;
    std::vector<uint16_t> res(prod_len);

    // Convolution
    for (size_t i = 0; i < a.size(); i++) {
        for (size_t j = 0; j < b.size(); j++) {
            res[i + j] ^= m_gf->multiply(a[i], b[j]);
        }
    }

    return gf2m_poly(m_gf, std::move(res));
}

bool gf2m_poly::operator==(const gf2m_poly& x) const { return m_poly == x.get_poly(); }


} // namespace dvbs2rx
} // namespace gr