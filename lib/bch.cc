/* -*- c++ -*- */
/*
 * Copyright (c) 2023 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "bch.h"
#include "gf2_util.h"
#include <algorithm>
#include <map>
#include <stdexcept>

namespace gr {
namespace dvbs2rx {

template <typename T>
gf2_poly<T> compute_gen_poly(const galois_field<T>* const gf, uint8_t t)
{
    // The generator polynomial g is the product of the set of unique minimal polynomials
    // associated with the t elements alpha^j for odd j varying from j=1 to j=(2*t -1).
    // Each minimum polynomial appears only once in the product so that the result is
    // equivalent to the LCM of the minimum polynomials.
    std::set<T> processed_conjugates;
    gf2_poly<T> g(1); // start with g(x) = 1
    for (int i = 0; i < t; i++) {
        T exponent = (2 * i) + 1;
        T beta = gf->get_alpha_i(exponent);
        // Since the conjugates of element beta = alpha^i have the same minimal
        // polynomial, make sure a conjugate of beta has not been processed before.
        if (processed_conjugates.count(beta) > 0)
            continue;
        auto conjugates = gf->get_conjugates(beta);
        processed_conjugates.insert(conjugates.begin(), conjugates.end());
        auto min_poly = gf->get_min_poly(beta);
        g = g * min_poly;
    }
    return g;
}

template <typename T>
bch_codec<T>::bch_codec(const galois_field<T>* const gf, uint8_t t)
    : m_gf(gf),
      m_t(t),
      m_g(std::move(compute_gen_poly(gf, t))),
      m_n((static_cast<T>(1) << gf->get_m()) - 1),
      m_k(m_n - m_g.degree()),
      m_parity(m_n - m_k),
      m_msg_mask((static_cast<T>(1) << m_k) - 1), // k-bit mask
      // The tables below need 2t elements (the number of minimal polynomials in g(x)),
      // but the index i goes from 1 to 2*t in the computations that follow, skipping i=0.
      // For convenience, it is helpful to allocate 2t+1 elements and leave the first
      // empty so that the for loop and array indexes coincide.
      m_conjugate_map(2 * t + 1),
      m_min_poly_rem_lut(2 * t + 1)
{
    if (m_n > sizeof(T) * 8)
        throw std::runtime_error("Type T cannot fit the codeword length");

    // Build the LUTs to assist in computing the remainder of "r(x) % phi_i(x)", for
    // arbitrary r(x), and for the 2t minimal polynomials phi_1(x) to phi_2t(x).
    m_min_poly.push_back(gf2_poly<T>(0)); // i=0 is empty for convenience
    for (int i = 1; i <= (2 * t); i++) {
        m_min_poly.push_back(gf->get_min_poly(gf->get_alpha_i(i)));
        m_min_poly_rem_lut[i] = build_gf2_poly_rem_lut(m_min_poly[i]);
    }

    // To speed up the syndrome computation, it is useful to keep a map of the elements
    // associated with the same minimal polynomial (conjugates). For the i-th index, a
    // value m_conjugate_map[i] equal to j (with j > 0) indicates that alpha^i has a
    // conjugate alpha^j with j lower than i, in which case the remainder "r % phi_j"
    // would already have been computed, so the computation of "r % phi_i" is unnecessary
    // (given phi_i and phi_j are the same minimal polynomial). If m_conjugate_map[i] is
    // zero, it means index i is the first to process the conjugate set of alpha^i, so the
    // associated "r % phi_i" polynomial needs to be computed for the first time.
    for (int i = 1; i <= (2 * t); i++) {
        T beta = gf->get_alpha_i(i);
        auto conjugates = gf->get_conjugates(beta);
        std::vector<T> conjugate_exponents;
        std::transform(conjugates.cbegin(),
                       conjugates.cend(),
                       std::back_inserter(conjugate_exponents),
                       [gf](const T& conjugate) {
                           return gf->get_exponent(conjugate);
                       }); // map conjugates to their exponents
        // After sorting, the first exponent in the vector is the lowest exponent.
        std::sort(conjugate_exponents.begin(), conjugate_exponents.end());
        if (conjugate_exponents[0] < static_cast<T>(i))
            m_conjugate_map[i] = conjugate_exponents[0];
    }
}

template <typename T>
T bch_codec<T>::encode(const T& msg) const
{
    // The codeword is given by:
    //
    // c(x) = x^(n-k)*d(x) + rho(x),
    //
    // where d(x) is the message, x^(n-k)*d(x) shifts the message by n-k bits (i.e., to
    // create space for the parity bits), and rho(x) (the polynomial representing the
    // parity bits) is equal to the remainder of x^(n-k)*d(x) divided by g(x).
    const auto shifted_msg_poly = gf2_poly<T>((msg & m_msg_mask) << m_parity);
    const auto parity_poly = shifted_msg_poly % m_g;
    return (shifted_msg_poly + parity_poly).get_poly();
}

template <typename T>
std::vector<T> bch_codec<T>::syndrome(const T& codeword) const
{
    std::vector<T> syndrome_vec;
    const auto codeword_poly = gf2_poly(codeword);
    std::map<int, gf2_poly<T>> bi_map;
    for (int i = 1; i <= (2 * m_t); i++) {
        // Due to how the generator polynomial is constructed (as the LCM of 2t minimal
        // polynomials), every valid codeword c(x) must have alpha^i as one of its roots.
        // In other words, c(alpha^i) = 0, for i varying from 1 to 2t. Then, by denoting
        // the noisy incoming codeword as "r(x) = c(x) + e(x)", where e(x) is the error
        // polynomial, it follows that "r(alpha^i) = e(alpha^i)", given that c(alpha^i) is
        // zero. Furthermore, note r(x) can be expressed as:
        //
        // r(x) = q(x) * phi_i(x) + b_i(x),
        //
        // where phi_i(x) is the i-th minimal polynomial, and q(x) and b_i(x) are the
        // quotient and remainder in the division of r(x) by phi_i(x), respectively. Then,
        // since "phi_i(alpha^i) = 0" (by definition), it follows that:
        //
        // r(alpha^i) = b_i(alpha^i) = e(alpha^i),
        //
        // which is the i-th syndrome component.
        //
        // Besides, since alpha^i has conjugates with the same associated minimal
        // polynomial, the remainder polynomial b_i(x) is the same for all conjugates of
        // alpha^i, so it only needs to be computed once. In other words, b_i(x) coincides
        // for the conjugates, but b_i(alpha^i) needs to be evaluated separately.
        if (m_conjugate_map[i] == 0) // new b_i(x)
            bi_map.emplace(i, codeword_poly % m_min_poly[i]);
        const int bi_idx = (m_conjugate_map[i] == 0) ? i : m_conjugate_map[i];
        const auto& bi = bi_map.at(bi_idx);
        // b_i(x) is a polynomial over GF(2), so it must be converted to a polynomial
        // over GF(2^m) to allow for the evaluation b_i(alpha^i) for alpha^i in
        // GF(2^m).
        const auto bi_gf2m = gf2m_poly(m_gf, bi);
        const T alpha_i = m_gf->get_alpha_i(i);
        syndrome_vec.push_back(bi_gf2m(alpha_i));
    }
    return syndrome_vec;
}

template <typename T>
std::vector<T> bch_codec<T>::syndrome(const std::vector<uint8_t>& codeword) const
{
    std::vector<T> syndrome_vec;
    std::map<int, gf2_poly<T>> bi_map;
    for (int i = 1; i <= (2 * m_t); i++) {
        // See the notes in the above (alternative) syndrome implementation. The
        // difference here is that the remainder computation to obtain b_i(x) is based on
        // LUTs instead of manual bit shifts and XORs for each input bit.
        if (m_conjugate_map[i] == 0) // new b_i(x)
            bi_map.emplace(i,
                           gf2_poly_rem(codeword, m_min_poly[i], m_min_poly_rem_lut[i]));
        const int bi_idx = (m_conjugate_map[i] == 0) ? i : m_conjugate_map[i];
        const auto& bi = bi_map.at(bi_idx);
        const auto bi_gf2m = gf2m_poly(m_gf, bi);
        const T alpha_i = m_gf->get_alpha_i(i);
        syndrome_vec.push_back(bi_gf2m(alpha_i));
    }
    return syndrome_vec;
}

/********** Explicit Instantiations **********/
template class bch_codec<uint16_t>;
template class bch_codec<uint32_t>;
template class bch_codec<uint64_t>;
template class bch_codec<int>;

} // namespace dvbs2rx
} // namespace gr