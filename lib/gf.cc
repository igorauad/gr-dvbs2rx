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

template <typename T>
galois_field<T>::galois_field(const gf2_poly<T>& prim_poly)
    : m_m(prim_poly.degree()),
      m_two_to_m_minus_one((1 << m_m) - 1),
      m_table(1 << m_m),
      m_inv_table(1 << m_m)
{
    // The field elements can be represented with m bits each. However, the minimal
    // polynomials can have degree up to m such that they need a storage of "m + 1" bits
    // (the highest-degree term is not omitted in gf2_poly<T>). Hence, the base type T,
    // which is also used for the GF(2) polynomials returned by get_min_poly, should be
    // large enough to hold "m + 1" bits. Since m comes from the degree of the primitive
    // polynomial, and the primitive polynomial also uses storage type T, this should be
    // guaranteed. For instance, if T=uint16_t, the max degree the prim_poly variable can
    // have is 15, so m=15, and "m+1" is the size of uint16_t in bits.
    assert(static_cast<size_t>(m_m + 1) <= sizeof(T) * 8);

    // The computation that follows ignores the unitary coefficient of the highest-order
    // term in the primitive polynomial.
    const T prim_poly_exc_high_bit =
        prim_poly.get_poly() ^ (static_cast<T>(1) << prim_poly.degree());

    // Table of GF(2^m) elements
    //
    // The first element is the additive identity (0), the second is the multiplicative
    // identity (1), which is also alpha^0, i.e., the primitive element to the power zero.
    // The remaining elements are alpha^j (the primitive element to the j-th power), and
    // these are generated iteratively by an LFSR. See the comments on function gf_mtx()
    // of the reference Python implementation.
    m_table[0] = 0;
    m_table[1] = 1;
    for (T i = 1; i < m_two_to_m_minus_one; i++)
        m_table[i + 1] = ((m_table[i] << 1) & m_two_to_m_minus_one) ^
                         ((m_table[i] >> (m_m - 1)) * prim_poly_exc_high_bit);

    // Inverse LUT: maps element alpha^i to its m_table index.
    //
    // Since m_table has element alpha^0 at index=1, alpha^1 at index=2, and so on, this
    // function generates an array whose value at position "alpha^i" is the index "i+1",
    // i.e., "x[alpha^i] = i + 1".
    for (T i = 1; i <= m_two_to_m_minus_one; i++) {
        for (T beta = 1; beta <= m_two_to_m_minus_one; beta++) {
            if (m_table[i] == beta) {
                m_inv_table[beta] = i;
                break;
            }
        }
    }
}

template <typename T>
T galois_field<T>::operator[](T index) const
{
    assert(index <= m_two_to_m_minus_one);
    return m_table[index];
}

template <typename T>
T galois_field<T>::get_alpha_i(T i) const
{
    return m_table[(i % m_two_to_m_minus_one) + 1];
}

template <typename T>
T galois_field<T>::get_exponent(T beta) const
{
    if (beta == 0)
        throw std::runtime_error("Zero element does not have an exponent");
    // Recall alpha^i is stored at position i + 1.
    return m_inv_table[beta] - 1;
}

template <typename T>
T galois_field<T>::multiply(T a, T b) const
{
    if (a == 0 || b == 0)
        return 0;
    assert(a <= m_two_to_m_minus_one && b <= m_two_to_m_minus_one);
    return get_alpha_i(get_exponent(a) + get_exponent(b));
}

template <typename T>
T galois_field<T>::inverse(T beta) const
{
    // We want "beta^-1" such that "beta * beta^-1 = 1". For that, we use the property
    // that any GF(2^m) element raised to the power "2^m - 1" is equal to one, i.e.,
    // "beta^(2^m - 1) = 1". Hence, if beta is alpha^j (the j-th power of the primitive
    // element), then beta^-1 must be the element alpha^k such that "j + k = 2^m - 1".
    return get_alpha_i(m_two_to_m_minus_one - get_exponent(beta));
}

template <typename T>
T galois_field<T>::divide(T a, T b) const
{
    return multiply(a, inverse(b));
}

template <typename T>
std::set<T> galois_field<T>::get_conjugates(T beta) const
{
    std::set<T> conjugates;

    // The set of conjugates always includes the original element.
    conjugates.insert(beta);

    // The conjugates of alpha^i are the distinct elements "alpha^i^(2^j)".
    T i = get_exponent(beta);
    for (uint8_t j = 1; j < m_m; j++) { // alpha^i can have up to m conjugates
        T conjugate = get_alpha_i(i * (1 << j));
        if (conjugates.count(conjugate) > 0)
            break;
        conjugates.insert(conjugate);
    }

    return conjugates;
}

template <typename T>
gf2_poly<T> galois_field<T>::get_min_poly(T beta) const
{
    if (beta == 0) { // 0 is always a root of "f(x) = x"
        return gf2_poly<T>(0b10);
    }
    // The minimal polynomial is the product of the terms "(x + beta^(2^l))" for each
    // distinct conjugate of beta given by beta^(2^l).
    const auto conjugates = get_conjugates(beta);
    auto prod = gf2m_poly<T>(this, std::vector<T>({ 1 }));
    for (const T& beta_2_l : conjugates) {
        prod = prod * gf2m_poly<T>(this, { beta_2_l, 1 });
    }
    return prod.to_gf2_poly(); // the result should be a polynomial over GF(2)
}

/********** Polynomial over GF(2) **********/

template <typename T>
gf2_poly<T>::gf2_poly(T coefs) : m_poly(coefs)
{
    // Polynomial degree
    m_degree = -1; // convention for the zero polynomial
    for (int i = m_max_degree; i >= 0; i--) {
        if (m_poly & (static_cast<T>(1) << i)) {
            m_degree = i;
            break;
        }
    }
}

template <typename T>
gf2_poly<T> gf2_poly<T>::operator+(const gf2_poly<T>& x) const
{
    return m_poly ^ x.get_poly();
}

template <typename T>
gf2_poly<T> gf2_poly<T>::operator*(bool x) const
{
    return m_poly * x;
}

template <typename T>
gf2_poly<T> gf2_poly<T>::operator*(const gf2_poly<T>& x) const
{
    if (m_degree + x.degree() > m_max_degree)
        throw std::runtime_error("GF(2) polynomial product exceeds max degree");

    const T& x_coefs = x.get_poly();
    T res = 0;
    for (int i = 0; i <= x.degree(); i++) {
        if (x_coefs & (static_cast<T>(1) << i)) {
            res ^= m_poly << i;
        }
    }
    return res;
}

template <typename T>
gf2_poly<T> gf2_poly<T>::operator%(const gf2_poly<T>& x) const
{
    if (x.degree() == -1) // zero divisor
        throw std::runtime_error("Remainder of division by a zero polynomial");
    if (m_poly == 0) // zero dividend
        return gf2_poly<T>(0);
    if (m_degree < x.degree()) // remainder is the polynomial itself
        return gf2_poly<T>(m_poly);

    T x_degree = x.degree();
    T remainder = m_poly;
    const T x_coefs = x.get_poly();
    for (T i = m_degree; i >= x_degree; i--) {
        if (remainder & (static_cast<T>(1) << i))
            remainder ^= x_coefs << (i - x_degree);
    }
    return remainder;
}

template <typename T>
bool gf2_poly<T>::operator==(const gf2_poly<T>& x) const
{
    return m_poly == x.get_poly();
}


/********** Polynomial over GF(2^m) **********/

template <typename T>
gf2m_poly<T>::gf2m_poly(const galois_field<T>* const gf, std::vector<T>&& coefs)
    : m_gf(gf), m_poly(std::move(coefs))
{
    set_degree();
}

template <typename T>
gf2m_poly<T>::gf2m_poly(const galois_field<T>* const gf, const gf2_poly<T>& gf2_poly)
    : m_gf(gf)
{
    const T& coefs = gf2_poly.get_poly();
    for (int i = 0; i <= gf2_poly.degree(); i++) {
        if (coefs & (static_cast<T>(1) << i)) {
            m_poly.push_back((*gf)[1]);
        } else {
            m_poly.push_back((*gf)[0]);
        }
    }
    set_degree();
}

template <typename T>
void gf2m_poly<T>::set_degree()
{
    // Remove any leading zeros and set the polynomial degree
    m_degree = m_poly.size() - 1;
    while (!m_poly.empty() && m_poly[m_degree] == 0) {
        m_poly.pop_back();
        m_degree--;
    }
}

template <typename T>
gf2m_poly<T> gf2m_poly<T>::operator+(const gf2m_poly<T>& x) const
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
    std::vector<T> res(pad_poly.size());
    for (size_t i = 0; i < res.size(); i++) {
        res[i] = a[i] ^ b[i];
    }

    return gf2m_poly<T>(m_gf, std::move(res));
}

template <typename T>
gf2m_poly<T> gf2m_poly<T>::operator*(T x) const
{
    auto a = m_poly;
    for (size_t i = 0; i < a.size(); i++) {
        a[i] = m_gf->multiply(a[i], x);
    }
    return gf2m_poly<T>(m_gf, std::move(a));
}

template <typename T>
gf2m_poly<T> gf2m_poly<T>::operator*(const gf2m_poly& x) const
{
    const auto& a = m_poly;
    const auto& b = x.get_poly();

    size_t prod_len = a.size() + b.size() - 1;
    std::vector<T> res(prod_len);

    // Convolution
    for (size_t i = 0; i < a.size(); i++) {
        for (size_t j = 0; j < b.size(); j++) {
            res[i + j] ^= m_gf->multiply(a[i], b[j]);
        }
    }

    return gf2m_poly<T>(m_gf, std::move(res));
}

template <typename T>
bool gf2m_poly<T>::operator==(const gf2m_poly& x) const
{
    return m_poly == x.get_poly();
}

template <typename T>
T gf2m_poly<T>::operator()(T x) const
{
    if (x == 0)
        return m_poly[0]; // all other coefficients are zeroed

    // A non-zero x can be represented as a power alpha^i of the primitive element alpha
    const T i = m_gf->get_exponent(x);
    T res = 0;
    for (int j = 0; j <= m_degree; j++) {
        T coef_j = m_poly[j]; // j-th coefficient (multiplies x^j)
        if (coef_j)           // is non-zero
            res ^= m_gf->multiply(coef_j, m_gf->get_alpha_i(i * j));
    }
    return res;
}

template <typename T>
gf2_poly<T> gf2m_poly<T>::to_gf2_poly() const
{
    if (m_degree > static_cast<int>(sizeof(T) * 8 - 1))
        throw std::runtime_error("GF(2^m) polynomial degree exceeds max GF(2) degree");

    T gf2_coefs = 0;
    for (int i = m_degree; i >= 0; i--) {
        if (m_poly[i] > 1) {
            throw std::runtime_error(
                "Trying to reduce non-binary GF(2^m) polynomial to GF(2)");
        }
        if (m_poly[i])
            gf2_coefs ^= static_cast<T>(1) << i;
    }
    return gf2_poly<T>(gf2_coefs);
}


/********** Explicit Instantiations **********/

template class galois_field<uint16_t>;
template class galois_field<uint32_t>;
template class galois_field<uint64_t>;
template class galois_field<int>;

template class gf2_poly<uint16_t>;
template class gf2_poly<uint32_t>;
template class gf2_poly<uint64_t>;
template class gf2_poly<int>;

template class gf2m_poly<uint16_t>;
template class gf2m_poly<uint32_t>;
template class gf2m_poly<uint64_t>;
template class gf2m_poly<int>;

} // namespace dvbs2rx
} // namespace gr