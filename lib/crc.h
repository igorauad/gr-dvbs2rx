/* -*- c++ -*- */
/*
 * Copyright (c) 2022 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_CRC_H
#define INCLUDED_DVBS2RX_CRC_H

#include <array>
#include <cstdint>
#include <vector>

namespace gr {
namespace dvbs2rx {

#define BITS_AFTER_MSB(T) ((sizeof(T) - 1) * 8) // bits after the most significant byte
#define MSB_MASK(T) \
    (static_cast<T>(1) << (sizeof(T) * 8 - 1)) // mask to check the most significant bit

/**
 * @brief Build the CRC computation look-up table (LUT)
 *
 * @tparam T CRC data type.
 * @param gen_poly_no_msb Generator polynomial in normal representation but excluding the
 * MSB. For instance, x^4 + x + 1 would be given as 0b11.
 * @return std::array<T, 256> Byte-by-byte CRC look-up table.
 * @note This implementation only works for generator polynomials with degrees multiple of
 * 8, e.g., for CRC8, CRC16, CRC32, etc.
 */
template <typename T>
std::array<T, 256> build_crc_lut(const T& gen_poly_no_msb)
{
    // See http://www.sunshine2k.de/articles/coding/crc/understanding_crc.html
    std::array<T, 256> table;
    for (int dividend = 0; dividend < 256; dividend++) {
        T shift_reg = static_cast<T>(dividend)
                      << BITS_AFTER_MSB(T); // dividend byte on the MSB
        for (unsigned char bit = 0; bit < 8; bit++) {
            if (shift_reg & MSB_MASK(T)) {
                shift_reg = (shift_reg << 1) ^ gen_poly_no_msb;
            } else {
                shift_reg <<= 1;
            }
        }
        table[dividend] = shift_reg;
    }
    return table;
};

/**
 * @brief Compute the CRC of a sequence of input bytes.
 *
 * @tparam T CRC data type.
 * @param in_bytes Vector of input bytes.
 * @param crc_lut Look-up table constructed with the build_crc_lut function.
 * @return T CRC value (checksum), the remainder of the division by the generator
 * polynomial.
 */
template <typename T>
T calc_crc(const std::vector<uint8_t>& in_bytes, const std::array<T, 256>& crc_lut)
{
    T crc = 0;
    for (const uint8_t& in_byte : in_bytes) {
        // Even if T is larger than a single byte, the dividend used for table look-up is
        // always a single byte, more specifically, the MSB of the CRC register. Also, on
        // each iteration, the looked-up dividend is the input byte XORed with whatever
        // leaked from the previous iteration into the MSB of the CRC register. The bits
        // leaking into the other bytes (past the MSB) are not taken into account for the
        // table look-up, but they must be added (mod-2) back in the end.
        T padded_in_byte = static_cast<T>(in_byte) << BITS_AFTER_MSB(T);
        uint8_t dividend = (crc ^ padded_in_byte) >> BITS_AFTER_MSB(T);
        // The table look-up returns the remainder that would result if the dividend byte
        // padded with BITS_AFTER_MSB was divided by the generator polynomial. This
        // remainder leaks into the succeeding input bytes. Also, the non-MSB bytes of the
        // previous iteration's remainder continue to leak over future input bytes. For
        // example, with a CRC-16, each remainder has 2 bytes, one affecting the current
        // input byte (XORed into the dividend) and the other affecting the next byte.
        // Hence, the non-MSB bytes of the previous remainder must be added back mod-2.
        crc = (crc << 8) ^ crc_lut[dividend];
    }
    return crc;
};

} // namespace dvbs2rx
} // namespace gr

#endif