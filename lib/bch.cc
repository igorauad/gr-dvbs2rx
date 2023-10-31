/* -*- c++ -*- */
/*
 * Copyright (c) 2023 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "bch.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>

namespace gr {
namespace dvbs2rx {

void assert_byte_aligned_n_k(uint32_t n, uint32_t k)
{
    if (n % 8 != 0 || k % 8 != 0)
        throw std::runtime_error(
            "u8 array messages are only supported for n and k multiple of 8.");
}

/**
 * @brief Compute the generator polynomial g(x) for a BCH code.
 *
 * @tparam T Base type for the Galois Field elements and the minimal GF(2) polynomials.
 * @tparam P Base type for the GF(2) generator polynomial resulting from the product of
 * minimal polynomials.
 * @param gf Galois field.
 * @param t Target error correction capability.
 * @return gf2_poly<P> Generator polynomial.
 */
template <typename T, typename P>
gf2_poly<P> compute_gen_poly(const galois_field<T>* const gf, uint8_t t)
{
    // The generator polynomial g is the product of the set of unique minimal polynomials
    // associated with the t elements alpha^j for odd j varying from j=1 to j=(2*t -1).
    // Each minimum polynomial appears only once in the product so that the result is
    // equivalent to the LCM of the minimum polynomials.
    std::set<T> processed_conjugates;
    gf2_poly<P> g(1); // start with g(x) = 1
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
        if (min_poly.degree() + g.degree() + 1 > static_cast<int>(sizeof(P) * 8))
            throw std::runtime_error(
                "Type P cannot fit the product of minimal polynomials of type T");
        gf2_poly<P> min_poly_p(min_poly.get_poly());
        g = g * min_poly_p;
    }
    return g;
}

template <typename T, typename P>
bch_codec<T, P>::bch_codec(const galois_field<T>* const gf, uint8_t t, uint32_t n)
    : m_gf(gf),
      m_t(t),
      m_g(std::move(compute_gen_poly<T, P>(gf, t))),
      m_n(n == 0 ? ((static_cast<uint32_t>(1) << gf->get_m()) - 1) : n),
      m_s(((static_cast<uint32_t>(1) << gf->get_m()) - 1) - m_n),
      m_k(m_n - m_g.degree()),
      m_parity(m_n - m_k),
      m_n_bytes(m_n / 8),
      m_k_bytes(m_k / 8),
      m_parity_bytes(m_n_bytes - m_k_bytes),
      m_msg_mask(bitmask<T>(m_k)), // k-bit mask
      // The tables below need 2t elements (the number of minimal polynomials in g(x)),
      // but the index i goes from 1 to 2*t in the computations that follow, skipping i=0.
      // For convenience, it is helpful to allocate 2t+1 elements and leave the first
      // empty so that the for loop and array indexes coincide.
      m_conjugate_map(2 * t + 1),
      m_min_poly_rem_lut(2 * t + 1),
      m_gen_poly_lut_generated(false)
{
    if (n > ((static_cast<uint32_t>(1) << gf->get_m()) - 1))
        throw std::runtime_error("Codeword length n exceeds the maximum of (2^m - 1)");

    if (static_cast<int>(m_n) <= m_g.degree())
        throw std::runtime_error(
            "Codeword length n must be greater than the generator polynomial's degree");

    if (gf->get_m() > (sizeof(uint32_t) * 8) - 1) // ensure m_n does not overflow
        throw std::runtime_error("GF(2^m) dimension m not supported (too large)");

    // Build the LUTs to assist in computing the remainder of "r(x) % phi_i(x)", for
    // arbitrary r(x), and for the 2t minimal polynomials phi_1(x) to phi_2t(x).
    m_min_poly.push_back(gf2_poly<T>(0)); // i=0 is empty for convenience
    for (int i = 1; i <= (2 * t); i++) {
        m_min_poly.push_back(gf->get_min_poly(gf->get_alpha_i(i)));
        m_min_poly_rem_lut[i] = build_gf2_poly_rem_lut(m_min_poly[i]);
    }

    // When k and n are multiples of 8, the message and parity bits are byte-aligned, so
    // encoding into bytes array becomes supported. For that, generate the LUT to help
    // computing the remainder of "r(x) % g(x)", where r(x) is an arbitrary GF(2)
    // polynomial and g(x) is the generator polynomial.
    //
    // NOTE: This LUT imposes an additional limitation on the maximum degree of g(x) based
    // on the size of type P. Since g(x) can have degree up to m*t, the P-typed remainder
    // LUT can only be computed for a g(x) with degree up to (sizeof(P) - 1)*8, as
    // detailed in the implementation of the LUT builder. Hence, to alleviate the issue,
    // compute the LUT only when bytes-based encoding is supported.
    if (m_k % 8 == 0 || m_n % 8 == 0) {
        m_gen_poly_rem_lut = build_gf2_poly_rem_lut(m_g);
        m_gen_poly_lut_generated = true;
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


// Encode into type T
template <typename T, typename P>
T _encode(const T& msg, const gf2_poly<P>& g, uint32_t msg_mask, uint32_t n_parity_bits)
{
    // The codeword is given by:
    //
    // c(x) = x^(n-k)*d(x) + rho(x),
    //
    // where d(x) is the message, x^(n-k)*d(x) shifts the message by n-k bits (i.e., to
    // create space for the parity bits), and rho(x) (the polynomial representing the
    // parity bits) is equal to the remainder of x^(n-k)*d(x) divided by g(x).
    const auto shifted_msg_poly = gf2_poly<P>((msg & msg_mask) << n_parity_bits);
    const auto parity_poly = shifted_msg_poly % g;
    return (shifted_msg_poly + parity_poly).get_poly();
}

// Template specialization for P = bitset256_t
template <typename T>
T _encode(const T& msg,
          const gf2_poly<bitset256_t>& g,
          uint32_t msg_mask,
          uint32_t n_parity_bits)
{
    const auto shifted_msg_poly =
        gf2_poly<bitset256_t>((msg & msg_mask) << n_parity_bits);
    const auto parity_poly = shifted_msg_poly % g;
    return static_cast<T>((shifted_msg_poly + parity_poly).get_poly().to_ulong());
}

template <typename T>
T _encode(const T& msg,
          const gf2_poly<bitset256a_t>& g,
          uint32_t msg_mask,
          uint32_t n_parity_bits)
{
    const auto shifted_msg_poly =
        gf2_poly<bitset256a_t>((msg & msg_mask) << n_parity_bits);
    const auto parity_poly = shifted_msg_poly % g;
    return static_cast<T>((shifted_msg_poly + parity_poly).get_poly().to_ulong());
}

template <typename T, typename P>
T bch_codec<T, P>::encode(const T& msg) const
{
    if (m_k > sizeof(T) * 8)
        throw std::runtime_error("Type T cannot fit the message length k.");

    if (m_n > sizeof(T) * 8)
        throw std::runtime_error("Type T cannot fit the codeword length n.");

    return _encode(msg, m_g, m_msg_mask, m_parity);
}

template <typename T, typename P>
void bch_codec<T, P>::encode(u8_cptr_t msg, u8_ptr_t codeword) const
{
    // For simplicity, make sure k and n are byte-aligned when representing messages and
    // codewords by byte arrays.
    assert_byte_aligned_n_k(m_n, m_k);

    if (!m_gen_poly_lut_generated)
        throw std::runtime_error("Generator polynomial remainder LUT not generated.");

    memcpy(codeword, msg, m_k_bytes); // systematic bytes
    memset(codeword + m_k_bytes, 0,
           m_parity_bytes); // zero-initialize the parity bytes
    const auto parity_poly = gf2_poly_rem(codeword, m_n_bytes, m_g, m_gen_poly_rem_lut);
    std::cout << parity_poly.get_poly() << std::endl;
    const auto parity_poly_u8_vec = to_u8_vector(parity_poly.get_poly(), m_parity_bytes);
    memcpy(codeword + m_k_bytes, parity_poly_u8_vec.data(), m_parity_bytes);
}

template <typename T, typename P>
std::vector<T> bch_codec<T, P>::syndrome(const T& codeword) const
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
        syndrome_vec.push_back(bi_gf2m.eval_by_exp(i));
    }
    return syndrome_vec;
}

template <typename T, typename P>
std::vector<T> bch_codec<T, P>::syndrome(u8_cptr_t codeword) const
{
    assert_byte_aligned_n_k(m_n, m_k);
    std::vector<T> syndrome_vec;
    std::map<int, gf2_poly<T>> bi_map;
    for (int i = 1; i <= (2 * m_t); i++) {
        // See the notes in the above (alternative) syndrome implementation. The
        // difference here is that the remainder computation to obtain b_i(x) is based on
        // LUTs instead of manual bit shifts and XORs for each input bit.
        if (m_conjugate_map[i] == 0) // new b_i(x)
            bi_map.emplace(
                i,
                gf2_poly_rem(codeword, m_n_bytes, m_min_poly[i], m_min_poly_rem_lut[i]));
        const int bi_idx = (m_conjugate_map[i] == 0) ? i : m_conjugate_map[i];
        const auto& bi = bi_map.at(bi_idx);
        const auto bi_gf2m = gf2m_poly(m_gf, bi);
        syndrome_vec.push_back(bi_gf2m.eval_by_exp(i));
    }
    return syndrome_vec;
}

template <typename T, typename P>
gf2m_poly<T> bch_codec<T, P>::err_loc_polynomial(const std::vector<T>& syndrome) const
{
    T unit = m_gf->get_alpha_i(0);

    // Form a table iteratively with up to t + 2 rows.
    uint16_t nrows = m_t + 2;

    // Prefill the values of mu for each row:
    std::vector<float> mu_vec(nrows);
    mu_vec[0] = -0.5;
    for (int i = 0; i < m_t + 1; i++)
        mu_vec[i + 1] = i;

    // Iteratively computed error-location polynomial.
    //
    // The first two rows are prefilled with "sigma(x) = 1". The third row can be
    // prefilled with the first-degree polynomial "S[0]*x + 1", where S[0] is the first
    // syndrome element.
    std::vector<gf2m_poly<T>> sigma_vec = { gf2m_poly<T>(m_gf, std::vector<T>({ unit })),
                                            gf2m_poly<T>(m_gf, std::vector<T>({ unit })),
                                            gf2m_poly<T>(m_gf, { unit, syndrome[0] }) };

    // Discrepancy, a GF(2^m) value. The first two rows have discrepancies equal to 1 and
    // S[0] (first syndrome component), respectively.
    std::vector<T> d(nrows);
    d[0] = unit;
    d[1] = syndrome[0];

    int row = 2;
    while (row <= m_t) {
        float mu = mu_vec[row];
        int two_mu = 2 * mu;

        // Discrepancy from equation (6.42) of Lin & Costello's book
        //
        // NOTE: compute d_mu instead of d_(mu+1) as in (6.42). Then, adjust the indexes
        // based on mu in (6.42). For instance, S_(2mu + 3) becomes "S_(2*(mu-1) + 3) =
        // S_(2*mu + 1)". Also, note the formulation considers syndrome components S_1 to
        // S_2t, which is S[0] to S[2*t - 1] here. Thus, in the end, S_(2mu + 3) from
        // (6.42) becomes S[2*mu] below, while S_(2mu + 2) becomes S[2*mu - 1], and so on.
        d[row] = syndrome[two_mu]; // e.g., for mu=1, pick S[2]
        const auto& sigma = sigma_vec[row].get_poly();
        for (size_t j = 1; j < sigma.size(); j++) { // exclude the zero-degree term
            if (sigma[j] != 0)                      // j-th coefficient
                d[row] ^= m_gf->multiply(sigma[j], syndrome[two_mu - j]);
        }

        // Next candidate polynomial
        if (d[row] == 0)
            sigma_vec.push_back(sigma_vec[row]);
        else {
            // Find another row rho prior to the μ-th row such that the rho-th discrepancy
            // d[rho] is not zero and the difference between twice the row number (2*rho)
            // and the degree of sigma at this row has the largest value
            int row_rho = 0;   // row number where mu = rho
            int max_diff = -2; // maximum diff "2*rho - sigma[row_rho].degree"
            for (int j = row - 1; j >= 0; j--) {
                if (d[j] != 0) { // discrepancy is not zero
                    int diff = (2 * mu_vec[j]) - sigma_vec[j].degree();
                    if (diff > max_diff) {
                        max_diff = diff;
                        row_rho = j;
                    }
                }
            }
            float rho = mu_vec[row_rho]; // value of mu at the rho-th row

            // Equation (6.41)
            T d_mu_inv_d_rho = m_gf->divide(d[row], d[row_rho]);
            std::vector<T> x_two_mu_minus_rho_coefs(int(2 * (mu - rho)));
            x_two_mu_minus_rho_coefs.push_back(1);
            const auto x_two_mu_minus_rho =
                gf2m_poly<T>(m_gf, std::move(x_two_mu_minus_rho_coefs));
            sigma_vec.push_back(sigma_vec[row] + (x_two_mu_minus_rho * d_mu_inv_d_rho *
                                                  sigma_vec[row_rho]));
        }
        row += 1;
    }
    return sigma_vec[row];
}

template <typename T, typename P>
std::vector<T> bch_codec<T, P>::err_loc_numbers(const gf2m_poly<T>& sigma) const
{
    // If the error-location polynomial sigma has degree greater than t, that means there
    // were more than t errors, and, in general, the errors cannot be located. Hence,
    // there is no point in going through the expensive computation of the error-location
    // numbers. Instead, the decoder should just skip the error correction step.
    if (sigma.degree() > m_t)
        return {};

    // Given the codeword has length n, the error location numbers can range from alpha^0
    // to alpha^n-1. Since alpha^(n+s) = alpha^(2^m - 1) = 1, the corresponding inverses
    // range from alpha^(n+s) to alpha^(s+1). See if any of these are the roots of sigma
    // and record the results.
    //
    // TODO: optimize this computation using a strategy like the one in Fig. 6.1.
    std::vector<uint32_t> root_exps =
        sigma.search_roots_in_exp_range(m_s + 1,       // starting exponent
                                        m_n + m_s,     // ending exponent
                                        sigma.degree() // max number of roots to find
        );
    std::vector<T> numbers(root_exps.size());
    for (size_t i = 0; i < root_exps.size(); i++)
        numbers[i] = m_gf->inverse_by_exp(root_exps[i]);
    return numbers;
}

/**
 * @brief Check if the codeword has errors according to the syndrome vector.
 *
 * @tparam T GF(2^m) element type.
 * @param syndrome Syndrome vector.
 * @return true if there are errors in the codeword, false otherwise.
 */
template <typename T>
bool syndrome_has_errors(const std::vector<T>& syndrome)
{
    for (const T& element : syndrome) {
        if (element != 0)
            return true;
    }
    return false;
}

/**
 * @brief Correct errors in the given codeword.
 *
 * @tparam T Codeword type and GF(2^m) element type.
 * @param codeword n-bit codeword to be corrected.
 * @param n Codeword length in bits.
 * @param gf Reference Galois field.
 * @param numbers Error location numbers.
 */
template <typename T>
void correct_errors(T& codeword,
                    uint32_t n,
                    const galois_field<T>* gf,
                    const std::vector<T>& numbers)
{
    for (const T& number : numbers) {
        // An error-location number alpha^j means there is an error in the polynomial
        // coefficient (bit) multiplying x^j, namely the j-th bit. Thus, we can correct
        // the error by flipping the j-th bit.
        uint32_t bit_idx = gf->get_exponent(number);
        if (bit_idx >= n) // should be up to n -1 only
            throw std::runtime_error("Error location number out of range");
        codeword ^= static_cast<T>(1) << bit_idx;
    }
}

/**
 * @brief Correct errors in the given codeword.
 *
 * @tparam T GF(2^m) element type.
 * @param decoded_msg Pointer to the k/8 bytes message extracted from the systematic part
 * of the codeword u8 array in network byte order.
 * @param n Codeword length in bits.
 * @param k Message length in bits.
 * @param gf Reference Galois field.
 * @param numbers Error location numbers.
 * @note Unlike the alternative implementation based on T-typed codewords, this
 * implementation (based on u8 arrays) corrects the k-bit message part only while ignoring
 * errors in the parity bits. The main motivation for this approach is the ability to
 * modify the decoded message in place with no need for changing the codeword array.
 */
template <typename T>
void correct_errors(u8_ptr_t decoded_msg,
                    uint32_t n,
                    uint32_t k,
                    const galois_field<T>* gf,
                    const std::vector<T>& numbers)
{
    for (const T& number : numbers) {
        // Same as above but taking the network byte order into account. When interpreting
        // the codeword as a polynomial over GF(2), the first bit in the first byte of the
        // array pointed by decoded_msg is the highest-order coefficient (multiplying
        // x^(n-1)) and the last valid bit in the array is the coefficient of x^(n-k). The
        // lower n-k coefficients from x^(n-k-1) down to x^0 are the parity bits, which
        // should not be in the array pointed by the decoded_msg argument.
        uint32_t bit_idx = gf->get_exponent(number);
        if (bit_idx >= n) // should be up to n -1 only
            throw std::runtime_error("Error location number out of range");
        if (bit_idx < (n - k)) // error in the parity bits (no need to correct)
            continue;
        uint32_t bit_idx_net_order = n - 1 - bit_idx;
        uint32_t byte_idx = bit_idx_net_order / 8;
        uint32_t bit_idx_in_byte = 7 - (bit_idx_net_order % 8);
        decoded_msg[byte_idx] ^= static_cast<unsigned char>(1) << bit_idx_in_byte;
    }
}

template <typename T, typename P>
T bch_codec<T, P>::decode(T codeword) const
{
    const auto s = syndrome(codeword);
    if (syndrome_has_errors(s)) {
        const auto poly = err_loc_polynomial(s);
        const auto numbers = err_loc_numbers(poly);
        correct_errors(codeword, m_n, m_gf, numbers);
    }
    return (codeword >> m_parity) & m_msg_mask;
}


template <typename T, typename P>
int bch_codec<T, P>::decode(u8_cptr_t codeword, u8_ptr_t decoded_msg) const
{
    assert_byte_aligned_n_k(m_n, m_k);
    memcpy(decoded_msg, codeword, m_k_bytes); // systematic bytes
    const auto s = syndrome(codeword);
    if (syndrome_has_errors(s)) {
        const auto poly = err_loc_polynomial(s);
        const auto numbers = err_loc_numbers(poly);
        correct_errors(decoded_msg, m_n, m_k, m_gf, numbers);
        // Generally, the error location polynomial has degree greater than t when the
        // codeword has more than t errors, in which case the errors cannot be located.
        // Also, even if the error location polynomial has degree <= t, not necessarily
        // all error location numbers can be found. The err_loc_numbers function should
        // obtain a number of error location numbers equivalent to the degree of the error
        // location polynomial. Otherwise, not all errors can be corrected.
        return poly.degree() == static_cast<int>(numbers.size()) ? numbers.size() : -1;
    } else {
        return 0;
    }
}

/********** Explicit Instantiations **********/
template class bch_codec<uint16_t, uint16_t>;
template class bch_codec<uint16_t, uint32_t>;
template class bch_codec<uint32_t, uint32_t>;
template class bch_codec<uint32_t, uint64_t>;
template class bch_codec<uint64_t, uint64_t>;
template class bch_codec<uint32_t, bitset256_t>;
template class bch_codec<uint32_t, bitset256a_t>;

} // namespace dvbs2rx
} // namespace gr