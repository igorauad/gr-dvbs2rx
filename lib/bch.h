/* -*- c++ -*- */
/*
 * Copyright (c) 2023 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef INCLUDED_DVBS2RX_BCH_H
#define INCLUDED_DVBS2RX_BCH_H

#include "gf.h"
#include "gf_util.h"
#include <gnuradio/dvbs2rx/api.h>
#include <array>
#include <cstdint>

namespace gr {
namespace dvbs2rx {

typedef unsigned char* u8_ptr_t;

/**
 * @brief BCH coder/decoder.
 *
 * @tparam T Base type for the Galois Field elements.
 * @tparam P Base type for the GF(2) generator polynomial.
 */
template <typename T, typename P>
class DVBS2RX_API bch_codec
{
private:
    const galois_field<T>* m_gf;         // Galois field
    uint8_t m_t;                         // error correction capability
    gf2_poly<P> m_g;                     // generator polynomial
    uint32_t m_n;                        // codeword length
    uint32_t m_k;                        // message length
    uint32_t m_parity;                   // number of parity bits
    uint32_t m_n_bytes;                  // codeword length in bytes
    uint32_t m_k_bytes;                  // message length in bytes
    uint32_t m_parity_bytes;             // number of parity bytes
    T m_msg_mask;                        // mask used to enforce k bits per message
    std::vector<gf2_poly<T>> m_min_poly; // minimal polynomials in g(x)
    std::vector<int> m_conjugate_map; // LUT mapping each GF(2^m) element to a conjugate
                                      // of lower exponent (if existing)
    std::vector<std::array<T, 256>>
        m_min_poly_rem_lut;           // Remainder LUT for each minimal polynomial in g(x)
    std::array<P, 256> m_gen_poly_rem_lut; // Remainder LUT for the generator polynomial
    bool m_gen_poly_lut_generated; // Whether the generator polynomial remainder LUT has
                                   // been generated already

    /**
     * @brief Build LUT to help computing the remainder of "r(x) % g(x)"
     *
     * where g(x) is the generator GF2 polynomial whose coefficients are represented by
     * type P and r(x) is an arbitrary array of bytes (interpreted as a GF2 polynomial).
     *
     * @note This LUT is only used and computed when the message and codewords are
     * encoded/decoded into/from u8 arrays. The disadvantage of computing when not needed
     * is that the LUT imposes an additional limitation on the maximum degree of g(x)
     * based on the size of type P. Since g(x) can have degree up to m*t, the P-typed
     * remainder LUT can only be computed for a g(x) with degree up to (sizeof(P) - 1)*8.
     */
    void build_gen_poly_rem_lut();

public:
    /**
     * @brief Construct a new BCH coder/decoder object
     *
     * @param gf Reference Galois field.
     * @param t Target error correction capability.
     */
    bch_codec(const galois_field<T>* const gf, uint8_t t);

    /**
     * @brief Encode an input message.
     *
     * @param msg k-bit input message.
     * @return T Resulting codeword.
     * @note The parity digits are placed at the least significant (lower order) n - k bit
     * positions, whereas the original (systematic) message is placed at the most
     * significant k bit positions, from bit n-1 to bit n-k.
     * @note Only use this implementation if the message type T can hold n bits.
     * Otherwise, use the implementation based on an array of bytes.
     */
    T encode(const T& msg) const;

    /**
     * @overload
     * @param msg Pointer to the input message with k/8 bytes.
     * @param codeword Pointer to the codeword buffer with space for n/8 bytes.
     * @note Since the code is systematic, the first k/8 bytes of the resulting codeword
     * hold the original message, whereas the remaining bytes contain the parity digits. A
     * serializer should read the codeword from the first byte to the last byte to
     * preserve the order.
     * @note The caller should make sure the pointers point to memory regions with enough
     * data and space.
     * @note This bytes-based encoding is only supported when n and k are multiples of 8.
     */
    void encode(const u8_ptr_t& msg, u8_ptr_t codeword) const;

    /**
     * @brief Compute syndrome of a receiver codeword.
     *
     * @param codeword Received codeword.
     * @return std::vector<T> Syndrome as a vector with 2t GF(2^m) elements.
     */
    std::vector<T> syndrome(const T& codeword) const;

    /**
     * @overload
     */
    std::vector<T> syndrome(const u8_vector_t& codeword) const;

    /**
     * @brief Compute the error-location polynomial.
     *
     * The error-location polynomial is a polynomial over GF(2^m) whose roots indicate the
     * location of bit errors. The implementation for finding this polynomial is based on
     * the simplified Berlekamp's iterative algorithm, which works for binary BCH codes.
     *
     * @param syndrome Syndrome vector with 2t elements.
     * @return gf2m_poly<T> Error-location polynomial, a polynomial over GF(2^m).
     */
    gf2m_poly<T> err_loc_polynomial(const std::vector<T>& syndrome) const;

    /**
     * @brief Compute the error-location numbers.
     *
     * The error-location numbers are the numbers from GF(2^m) corresponding to the
     * reciprocal of the roots of the error-location polynomial. An error-location number
     * alpha^j indicates there is an error in the j-th bit of the received codeword.
     *
     * @param sigma Error location polynomial.
     * @return std::vector<T> Error location numbers, a vector of elements from GF(2^m).
     */
    std::vector<T> err_loc_numbers(const gf2m_poly<T>& sigma) const;

    /**
     * @brief Decode an input codeword.
     *
     * @param codeword n-bit input codeword.
     * @return T Decoded message.
     */
    T decode(T codeword) const;

    /**
     * @brief Get the generator polynomial object.
     *
     * @return const gf2_poly<P>& Generator polynomial.
     */
    const gf2_poly<P>& get_gen_poly() const { return m_g; };

    /**
     * @brief Get the codeword length n.
     *
     * @return uint16_t Codeword length.
     */
    uint16_t get_n() const { return m_n; };

    /**
     * @brief Get the message length k.
     *
     * @return uint16_t Message length.
     */
    uint16_t get_k() const { return m_k; };
};

} // namespace dvbs2rx
} // namespace gr
#endif