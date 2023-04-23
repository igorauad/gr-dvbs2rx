/* -*- c++ -*- */
/*
 * Copyright (c) 2023 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "bch.h"
#include "crc.h"
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
      m_min_poly_crc_lut(2 * t)                   // 2t minimal polynomials
{
    if (m_n > sizeof(T) * 8)
        throw std::runtime_error("Type T cannot fit the codeword length");

    // Build the CRC (remainder of) computation LUT for each of the 2t minimal polynomials
    // (not necessarily distinct) embedded within the BCH generator polynomial g(x).
    for (int i = 0; i < (2 * t); i++) {
        m_min_poly.push_back(gf->get_min_poly(gf->get_alpha_i(i + 1)));
        // Unlike the gf2_poly type, which includes the MSB, the CRC table generator
        // assumes the input polynomial excludes the MSB.
        const T min_poly_coefs_no_msb =
            m_min_poly[i].get_poly() ^ (static_cast<T>(1) << m_min_poly[i].degree());
        m_min_poly_crc_lut[i] = build_crc_lut(min_poly_coefs_no_msb);
    }
}

template <typename T>
T bch_codec<T>::encode(const T& msg)
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
std::vector<T> bch_codec<T>::syndrome(const T& codeword)
{
    std::vector<T> syndrome_vec;
    const auto codeword_poly = gf2_poly(codeword);
    for (int i = 0; i < (2 * m_t); i++) {
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
        const auto bi = codeword_poly % m_min_poly[i]; // a polynomial over GF(2)
        const auto bi_gf2m = gf2m_poly(m_gf, bi); // convert to a polynomial over GF(2^m)
        // the i-th syndrome_vec is b_i(x) evaluated for x=alpha^i, i.e., b_i(alpha^i)
        const T alpha_i = m_gf->get_alpha_i(i + 1);
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