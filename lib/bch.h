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
#include <gnuradio/dvbs2rx/api.h>
#include <array>
#include <cstdint>

namespace gr {
namespace dvbs2rx {

template <typename T>
class DVBS2RX_API bch_codec
{
private:
    const galois_field<T>* m_gf;         // Galois field
    uint8_t m_t;                         // error correction capability
    gf2_poly<T> m_g;                     // generator polynomial
    uint16_t m_n;                        // codeword length
    uint16_t m_k;                        // message length
    uint16_t m_parity;                   // number of parity bits
    T m_msg_mask;                        // mask used to enforce k bits per message
    std::vector<gf2_poly<T>> m_min_poly; // minimal polynomials in g(x)
    std::vector<int> m_conjugate_map; // LUT mapping each GF(2^m) element to a conjugate
                                      // of lower exponent (if existing)
    std::vector<std::array<T, 256>>
        m_min_poly_rem_lut;           // CRC LUT for each minimal polynomial

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
     */
    T encode(const T& msg) const;

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
    std::vector<T> syndrome(const std::vector<uint8_t>& codeword) const;

    /**
     * @brief Get the generator polynomial object.
     *
     * @return const gf2_poly<T>& Generator polynomial.
     */
    const gf2_poly<T>& get_gen_poly() const { return m_g; };

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