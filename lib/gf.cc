/* -*- c++ -*- */
/*
 * Copyright (c) 2022 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gf.h"
#include "gf_util.h"
#include <cassert>

namespace gr {
namespace dvbs2rx {

/********** Galois Field GF(2^m) **********/

template <typename T>
galois_field<T>::galois_field(const gf2_poly<T>& prim_poly)
    : m_m(prim_poly.degree()),
      m_two_to_m_minus_one((1 << m_m) - 1),
      m_table(1 << m_m),              // GF(2^m) has 2^m elements
      m_table_nonzero((1 << m_m) - 1) // among which 2^m - 1 are non-zero
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
    for (uint32_t i = 1; i < m_two_to_m_minus_one; i++)
        m_table[i + 1] = ((m_table[i] << 1) & m_two_to_m_minus_one) ^
                         ((m_table[i] >> (m_m - 1)) * prim_poly_exc_high_bit);

    // For performance, keep also a table of non-zero elements and use it when the lookup
    // is by the exponent i of the element alpha^i (i.e., on method get_alpha_i()) instead
    // of a lookup by index. While alpha^i is stored at position i + 1 in m_table, the
    // same element is stored in position i in m_table_nonzero. Hence, the exponent i maps
    // directly to the index at the m_table_nonzero vector, which makes the lookup
    // slightly faster. This strategy can make a difference when many lookups are
    // required, as in the polynomial root search (search_roots_in_exp_range() method).
    for (uint32_t i = 0; i < m_two_to_m_minus_one; i++)
        m_table_nonzero[i] = m_table[i + 1];

    // Inverse LUT (Exponent LUT): map each non-zero element alpha^i to its exponent i.
    for (uint32_t i = 0; i < m_two_to_m_minus_one; i++)
        m_exp_table.insert({ m_table_nonzero[i], i }); // m_table_nonzero[i] = alpha^i
}

template <typename T>
T galois_field<T>::multiply(const T& a, const T& b) const
{
    if (a == 0 || b == 0)
        return 0;
    return get_alpha_i(get_exponent(a) + get_exponent(b));
}

template <typename T>
T galois_field<T>::inverse(const T& beta) const
{
    // We want "beta^-1" such that "beta * beta^-1 = 1". For that, we use the property
    // that any GF(2^m) element raised to the power "2^m - 1" is equal to one, i.e.,
    // "beta^(2^m - 1) = 1". Hence, if beta is alpha^j (the j-th power of the primitive
    // element), then beta^-1 must be the element alpha^k such that "j + k = 2^m - 1".
    return get_alpha_i(m_two_to_m_minus_one - get_exponent(beta));
}

template <typename T>
T galois_field<T>::inverse_by_exp(uint32_t i) const
{
    // See the comments on inverse() above.
    return get_alpha_i(m_two_to_m_minus_one - i);
}

template <typename T>
T galois_field<T>::divide(const T& a, const T& b) const
{
    return multiply(a, inverse(b));
}

template <typename T>
std::set<T> galois_field<T>::get_conjugates(const T& beta) const
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
gf2_poly<T> galois_field<T>::get_min_poly(const T& beta) const
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
gf2_poly<T>::gf2_poly(const T& coefs) : m_poly(coefs)
{
    // Polynomial degree
    m_degree = -1; // convention for the zero polynomial
    for (int i = m_max_degree; i >= 0; i--) {
        if (is_bit_set(m_poly, i)) {
            m_degree = i;
            break;
        }
    }
}


template <typename T>
gf2_poly<T> gf2_poly<T>::operator*(const gf2_poly<T>& x) const
{
    if (m_degree + x.degree() > m_max_degree)
        throw std::runtime_error("GF(2) polynomial product exceeds max degree");

    const T& x_coefs = x.get_poly();
    T res = 0;
    for (int i = 0; i <= x.degree(); i++) {
        if (is_bit_set(x_coefs, i)) {
            res ^= m_poly << i;
        }
    }
    return res;
}

/********** Polynomial over GF(2^m) **********/

template <typename T>
gf2m_poly<T>::gf2m_poly(const galois_field<T>* const gf, std::vector<T>&& coefs)
    : m_gf(gf), m_poly(std::move(coefs))
{
    set_degree();
    set_coef_exponents();
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
void gf2m_poly<T>::set_coef_exponents()
{
    m_nonzero_coef_idx.clear();
    m_nonzero_coef_exp.clear();
    for (int j = 0; j <= m_degree; j++) {
        const T& coef_j = m_poly[j]; // j-th coefficient (multiplies x^j)
        if (coef_j) {                // is non-zero
            m_nonzero_coef_idx.push_back(j);
            m_nonzero_coef_exp.push_back(m_gf->get_exponent(coef_j));
        }
    }
    // Cache the number of non-zero coefficients for performance so that we don't need to
    // call size() on every polynomial evaluation via operator().
    m_n_nonzero_coef = m_nonzero_coef_idx.size();
}

template <typename T>
gf2m_poly<T> gf2m_poly<T>::operator+(const gf2m_poly<T>& x) const
{
    auto a = m_poly;
    auto b = x.get_poly();

    // Pad the shortest polynomial if they don't have the same length
    int n_pad = abs(static_cast<int>(a.size()) - static_cast<int>(b.size()));
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
T gf2m_poly<T>::eval(T x) const
{
    if (x == 0)
        return m_poly[0]; // all other coefficients are zeroed

    // A non-zero x can be represented as a power alpha^i of the primitive element alpha.
    // For each non-zero term x^j in the polynomial {1, x, x^2, ...}, the alpha^i to the
    // power j becomes alpha^(ij). Then, this vector is multiplied by the vector of
    // coefficients, and this is done for the non-zero coefficients only. Each non-zero
    // coefficient can also be represented as a power of the primitive element alpha, so
    // the vector of non-zero coefficient exponents is like {alpha^a, alpha^b, ...}.
    // Hence, for instance, alpha^a multiplied by alpha^(ij) becomes alpha^(a + ij).
    const uint32_t i = m_gf->get_exponent(x);
    T res = 0;
    for (size_t j = 0; j < m_n_nonzero_coef; j++) {
        res ^= m_gf->get_alpha_i(m_nonzero_coef_exp[j] + i * m_nonzero_coef_idx[j]);
    }
    return res;
}

template <typename T>
T gf2m_poly<T>::eval_by_exp(uint32_t i) const
{
    // See comments on eval() above.
    T res = 0;
    for (size_t j = 0; j < m_n_nonzero_coef; j++) {
        res ^= m_gf->get_alpha_i(m_nonzero_coef_exp[j] + i * m_nonzero_coef_idx[j]);
    }
    return res;
}

template <typename T>
std::vector<uint32_t> gf2m_poly<T>::search_roots_in_exp_range(uint32_t i_start,
                                                              uint32_t i_end,
                                                              uint32_t max_roots) const
{
    // Based on eval_by_exp() but optimized for a contiguous range of exponents.
    //
    // The underlying polynomial has a set of non-zero coefficients. For instance, let's
    // say the polynomial p(x) is over GF(2^4) and given by:
    //
    // p(x) = alpha^5 x^3 + alpha^4 x^2 + 1
    //
    // Then, the non-zero coefficients are {alpha^5, alpha^4, 1}, and their corresponding
    // exponents are {5, 4, 0}. The exponents are the powers of the primitive element
    // alpha that each coefficient is raised to. In this code, the vector with the
    // exponents of the non-zero coefficients is called m_nonzero_coef_exp.
    //
    // The corresponding coefficient orders are {3, 2, 0} because they multiply x^3, x^2,
    // and x^0, respectively. These orders are equivalent to the indexes where the
    // non-zero coefficients are stored in the coefficients vector m_poly. In this code,
    // the vector with the indexes (or orders) of the non-zero coefficients is called
    // m_nonzero_coef_idx.
    //
    // In summary, the two vectors of interest are as follows:
    //
    // - m_nonzero_coef_exp = {5, 4, 0}
    // - m_nonzero_coef_idx = {3, 2, 0}
    //
    // Next, let's say we are evaluating p(x) for x=alpha^i, then for alpha^(i+1), and so
    // on. To start, we have:
    //
    // p(alpha^i) = alpha^5 (alpha^i)^3 + alpha^4 (alpha^i)^2 + 1
    //            = alpha^(5 + i*3) + alpha^(4 + i*2) + 1
    //
    // We can break this computation into four steps:
    //
    //   1. Take the vector of coefficient indexes (or orders) {3, 2, 0} and multiply it
    //      by the scalar i. The result is {3i, 2i, 0}.
    //   2. Add the previous result to the vector of coefficient exponents {5, 4, 0}. The
    //      result is {5 + 3i, 4 + 2i, 0}.
    //   3. Take the primitive element alpha and raise it to the power of each element in
    //      the previous result. The result is {alpha^(5 + 3i), alpha^(4 + 2i), 1}.
    //   4. Add all GF(2^m) elements in the previous result. If the result is zero, then
    //      alpha^i is a root of p(x).
    //
    // Next, we compute p(alpha^(i+1)), but note we can reuse some of the previous
    // results. First, the addition in step 2 is the same because the vector of
    // coefficient exponents does not change. Secondly, the multiplication in step 1 is
    // now {3(i+1), 2(i+1), 0}, which is the same as {3i, 2i, 0} plus {3, 2, 0}. Hence, we
    // can accumulate {3, 2, 0} to the result obtained previously in step 1 instead of
    // computing multiplications again.
    //
    // With that, the updated algorithm becomes as follows:
    //
    //   1. On initialization, define an accumulator vector with the same dimensions as
    //      m_nonzero_coef_exp and m_nonzero_coef_idx. Initialize it with the values of
    //      m_nonzero_coef_exp so that the addition in step 2 (above) is only executed
    //      once. In the given example, the accumulator would be started with {5, 4, 0}.
    //
    //   2. Add "(i_start-1) * m_nonzero_coef_idx" element-wise to the accumulator. In the
    //      given example, the accumulator result would be {5 + 3*(i-1), 4 + 2*(i-1), 0}.
    //
    //      NOTE: The lowest possible i_start is zero since i is unsigned and alpha^0 is a
    //      potential root. In this case, and only in this case, this step adds
    //      -m_nonzero_coef_idx to the accumulator, i.e., subtracts m_nonzero_coef_idx
    //      from the accumulator. Given the indexes are unsigned, such a subtraction could
    //      underflow some or all values in the accumulator, and the resulting vector
    //      could end up with large numbers. However, this is fine. In step 3.1 below, we
    //      start by add +m_nonzero_coef_idx again, so an underflowed result would be
    //      undone right in the first iteration.
    //
    //   3. For each i in the range from i_start to i_end (inclusive):
    //
    //      3.1. Add m_nonzero_coef_idx element-wise to the accumulator. In the given
    //           example, the result in the first iteration after adding {3, 2, 0} would
    //           be {5 + 3*i, 4 + 2*i, 0}. In the second iteration, the accumulator would
    //           hold {5 + 3*(i+1), 4 + 2*(i+1), 0}, and so on.
    //
    //      3.2. Take the primitive element alpha and raise it to the power of each
    //           element in the accumulator. In the first iteration, the result becomes
    //           {alpha^(5 + 3i), alpha^(4 + 2i), 1}. In the next, it becomes {alpha^(5 +
    //           3(i+1)), alpha^(4 + 2(i+1)), 1}, an so on.
    //
    //      3.3 Add all GF(2^m) elements in the previous result. If the result is zero,
    //          then alpha^i is a root of p(x). Save the exponent i to return it later in
    //          the vector of exponents of the roots of p(x).
    //
    if (i_start > i_end)
        throw std::runtime_error("Start exponent is greater than end exponent");

    // Step 1
    std::vector<uint32_t> accum = m_nonzero_coef_exp;

    // Step 2
    for (size_t j = 0; j < m_n_nonzero_coef; j++) {
        accum[j] += (i_start - 1) * m_nonzero_coef_idx[j];
    }

    // Step 3
    std::vector<uint32_t> root_exps;
    uint32_t n_found = 0; // number of roots found so far
    for (uint32_t i = i_start; i <= i_end; i++) {
        T res = 0; // evaluation result
        for (size_t j = 0; j < m_n_nonzero_coef; j++) {
            accum[j] += m_nonzero_coef_idx[j];
            res ^= m_gf->get_alpha_i(accum[j]);
        }
        if (res == 0) {
            root_exps.push_back(i);
            if (++n_found == max_roots)
                break;
        }
    }

    return root_exps;
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
template class gf2_poly<bitset256_t>;

template class gf2m_poly<uint16_t>;
template class gf2m_poly<uint32_t>;
template class gf2m_poly<uint64_t>;
template class gf2m_poly<int>;

} // namespace dvbs2rx
} // namespace gr