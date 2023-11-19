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
      m_gen_poly_lut_generated(false)
{
    if (n > ((static_cast<uint32_t>(1) << gf->get_m()) - 1))
        throw std::runtime_error("Codeword length n exceeds the maximum of (2^m - 1)");

    if (static_cast<int>(m_n) <= m_g.degree())
        throw std::runtime_error(
            "Codeword length n must be greater than the generator polynomial's degree");

    if (gf->get_m() > (sizeof(uint32_t) * 8) - 1) // ensure m_n does not overflow
        throw std::runtime_error("GF(2^m) dimension m not supported (too large)");

    // When k and n are multiples of 8, the message and parity bits are byte-aligned, so
    // encoding and decoding into/from a bytes array becomes supported. For that, generate
    // a LUT to help in computing the remainder of "r(x) % g(x)", where r(x) is an
    // arbitrary GF(2) polynomial and g(x) is the generator polynomial. On encoding, r(x)
    // is the padded message polynomial, and on decoding, r(x) is the received codeword.
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

    // Generate a LUT to solve quadratic error-location polynomials faster than with
    // brute-force root search. See err_loc_numbers() for details.
    const uint32_t two_to_m = static_cast<uint32_t>(1) << gf->get_m();
    m_quadratic_poly_lut.resize(two_to_m);
    for (T r = 0; r < two_to_m; r++) {
        T idx = m_gf->multiply(r, r) ^ r; // R*(R+1)
        m_quadratic_poly_lut[idx] = r;
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
    const auto parity_poly_u8_vec = to_u8_vector(parity_poly.get_poly(), m_parity_bytes);
    memcpy(codeword + m_k_bytes, parity_poly_u8_vec.data(), m_parity_bytes);
}

template <typename T, typename P>
std::vector<T>
_eval_syndrome(const gf2_poly<P>& parity_poly, const galois_field<T>* const gf, uint8_t t)
{
    // A zero parity polynomial means there are no errors in the codeword. In this case,
    // don't bother computing the syndrome to avoid 2t unnecessary calls to "eval_by_exp".
    if (parity_poly.is_zero())
        return {};

    const auto parity_poly_gf2m = gf2m_poly(gf, parity_poly);
    std::vector<T> syndrome_vec;
    for (int i = 1; i <= (2 * t); i++)
        syndrome_vec.push_back(parity_poly_gf2m.eval_by_exp(i));
    return syndrome_vec;
}

template <typename T, typename P>
std::vector<T> bch_codec<T, P>::syndrome(const T& codeword) const
{
    // Due to how the generator polynomial is constructed as the LCM of 2t minimal
    // polynomials, every valid codeword c(x) must have alpha^i for i from 1 to 2t as its
    // roots, i.e., c(alpha^i) = 0, for i = 1, ..., 2t. Then, by denoting the noisy
    // incoming codeword as "r(x) = c(x) + e(x)", where e(x) is the error polynomial, it
    // follows that "r(alpha^i) = e(alpha^i)", given that c(alpha^i) is zero. Furthermore,
    // note r(x) can be expressed as:
    //
    // r(x) = a(x) * g(x) + s(x),
    //
    // where g(x) is the generator polynomial, and a(x) and s(x) are the quotient and
    // remainder resulting from the division of r(x) by g(x), respectively. Since
    // "g(alpha^i) = 0" (by definition), it follows that:
    //
    // r(alpha^i) = s(alpha^i) = e(alpha^i),
    //
    // which is the i-th syndrome component. Hence, compute the syndrome components by
    // evaluating the remainder s(x) of "r(x) % g(x)" for alpha^i for i from 1 to 2t.
    const auto codeword_poly = gf2_poly<T>(codeword);
    const auto parity_poly = codeword_poly % m_g;
    return _eval_syndrome(parity_poly, m_gf, m_t);
}

template <typename T, typename P>
std::vector<T> bch_codec<T, P>::syndrome(u8_cptr_t codeword) const
{
    assert_byte_aligned_n_k(m_n, m_k);
    const auto parity_poly = gf2_poly_rem(codeword, m_n_bytes, m_g, m_gen_poly_rem_lut);
    return _eval_syndrome(parity_poly, m_gf, m_t);
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

    if (sigma.degree() == 1) {
        // An unitary-degree error-location polynomial has a single root that can be
        // solved immediately with no need for a brute-force search. The polynomial can be
        // expressed as "ax + b", whose root is "b/a". Correspondingly, the error-location
        // number (reciprocal of the root) is "a/b".
        return { m_gf->divide(sigma[1], sigma[0]) };
    }

    if (sigma.degree() == 2) {
        // A quadratic error-location polynomial has two roots that can be solved
        // immediately using a LUT. The polynomial can be expressed as "ax^2 + bx + c",
        // whose roots x0 and x1 in GF(2^m) are related by the sum and product below:
        //
        // x0 + x1 = b/a,                                 (1)
        // x0 * x1 = c/a.                                 (2)
        //
        // Now, define "R = a*x_0/b", or, equivalently:
        //
        // x_0 = R*b/a.                                   (3)
        //
        // Then, by substituting (3) in (1), it follows that:
        //
        // R*b/a + x1 = b/a
        // x1 = (b/a)*(R + 1)                             (4)
        //
        // Next, substituting (3) and (4) in (2), we obtain:
        //
        // R*(R + 1) = (c/a)*(a/b)^2 = c*a/b^2            (5)
        //
        // So, to solve the polynomial, compute c*a/b^2 and look up the corresponding R in
        // the pre-computed LUT. The LUT stores the value R on each index R*(R+1) for all
        // possible R in GF(2^m). Thus, by using c*a/b^2 as the index, we can find the
        // corresponding R and use it to solve for x0 and x1.
        //
        // For further reference, see the discussion in igorauad/gr-dvbs2rx#31.
        //
        // Also, note the coefficients b and c of the quadratic polynomial must be
        // non-zero. Otherwise, we would not have two distinct and invertible roots. For
        // instance, with "sigma(x) = ax^2 + bx" (i.e., c=0), we would have "x0 = 0",
        // which is not invertible. Also, with "sigma(x) = ax^2 + c" (i.e., b=0), we would
        // not have distinct roots, and with "sigma(x) = ax^2" (i.e., b=c=0), the equal
        // roots would both be zero (non-invertible). In any of these cases, return early
        // with an empty vector of error-location numbers (meaning decoding failure).
        if (sigma[1] == 0 || sigma[0] == 0) // b=0 or c=0
            return {};
        T b_over_a = m_gf->divide(sigma[1], sigma[2]);
        T r_sq_plus_r = m_gf->divide(m_gf->multiply(sigma[0], sigma[2]),
                                     m_gf->multiply(sigma[1], sigma[1]));
        T r = m_quadratic_poly_lut[r_sq_plus_r];
        T x0 = m_gf->multiply(r, b_over_a);
        T x1 = m_gf->multiply(b_over_a, (r ^ 1));
        return { m_gf->inverse(x0), m_gf->inverse(x1) };
    }

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
    if (s.size() > 0) { // an empty syndrome means no errors
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
    if (s.size() > 0) { // an empty syndrome means no errors
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

} // namespace dvbs2rx
} // namespace gr