/* -*- c++ -*- */
/*
 * Copyright (c) 2023 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_GF_UTIL_H
#define INCLUDED_DVBS2RX_GF_UTIL_H

#include "gf.h"
#include <array>
#include <stdexcept>
#include <vector>


namespace gr {
namespace dvbs2rx {

typedef std::vector<unsigned char> u8_vector_t;
typedef unsigned char* u8_ptr_t;        // Pointer to modifiable u8 buffer
typedef const unsigned char* u8_cptr_t; // Pointer to constant u8 buffer

/**
 * @brief Get bitmask for the least significant bits of a type T
 *
 * @param n_bits Number of bits over which the mask is high.
 * @return T Bitmask.
 */
template <typename T>
inline T bitmask(int n_bits)
{
    return (static_cast<T>(1) << n_bits) - 1;
}

/**
 * @overload
 * @note Template specialization for T = bitset256_t.
 */
template <>
inline bitset256_t bitmask(int n_bits)
{
    bitset256_t mask;
    for (int i = 0; i < n_bits; i++)
        mask.set(i);
    return mask;
}

/**
 * @brief Get the byte at a given index of a type T value.
 *
 * @tparam T Value type.
 * @param value Value.
 * @param byte_index Target byte index.
 * @return uint8_t Extracted byte value.
 */
template <typename T>
inline uint8_t get_byte(const T& value, uint32_t byte_index)
{
    return (value >> (byte_index * 8)) & 0xFF;
}

/**
 * @overload
 * @note Template specialization for T = bitset256_t.
 */
template <>
inline uint8_t get_byte(const bitset256_t& value, uint32_t byte_index)
{
    uint8_t byte = 0;
    for (uint32_t i = byte_index * 8; i < (byte_index + 1) * 8; i++)
        byte |= value[i] << (i - byte_index * 8);
    return byte;
}

/**
 * @brief Get the most significant byte of a given value.
 *
 * @tparam T Value type.
 * @param value Value.
 * @param lsb_index Index of the least significant bit of the most significant byte,
 * equivalent to the number of bits to be shifted to the right.
 * @return uint8_t Extracted byte value.
 * @note This function differs from get_byte in that it assumes the given value has zeros
 * in the other byte positions (if any) within the container type T beyond the byte of
 * interest. In other words, this function does not apply a mask (AND 0xFF) to the result.
 */
template <typename T>
inline uint8_t get_msby(const T& value, uint32_t lsb_index)
{
    return value >> lsb_index;
}

/**
 * @overload
 * @note Template specialization for T = bitset256_t.
 */
template <>
inline uint8_t get_msby(const bitset256_t& value, uint32_t lsb_index)
{
    uint8_t byte = 0;
    for (uint32_t i = lsb_index; i < (lsb_index + 8); i++)
        byte |= value[i] << (i - lsb_index);
    return byte;
}

/**
 * @brief Convert type to u8 vector in network byte order (big-endian)
 *
 * @tparam T Bit storage type.
 * @param val Value to be converted.
 * @param n_bytes Number of least significant bytes to be converted. E.g., n_bytes=3
 * converts the three least significant bytes of val into a vector in network order.
 * @return u8_vector_t Resulting u8 vector.
 */
template <typename T>
inline u8_vector_t to_u8_vector(T val, size_t n_bytes = sizeof(T))
{
    if (n_bytes > sizeof(T))
        throw std::invalid_argument("n_bytes too large for type T");
    u8_vector_t vec;
    for (int i = n_bytes - 1; i >= 0; i--) {
        vec.push_back(get_byte(val, i));
    }
    return vec;
}

/**
 * @brief Convert u8 array in network byte order (big-endian) to type
 *
 * @tparam T Bit storage type.
 * @param in u8 array to be converted.
 * @param size Number of bytes to be converted.
 * @return T Resulting value.
 */
template <typename T>
inline T from_u8_array(u8_cptr_t in, size_t size)
{
    if (size > sizeof(T))
        throw std::invalid_argument("u8 array too large for type T");
    T val = 0;
    for (size_t i = 0; i < size; i++) {
        val |= static_cast<T>(in[i]) << ((size - 1 - i) * 8);
    }
    return val;
}

/**
 * @brief Convert u8 vector in network byte order (big-endian) to type
 *
 * @tparam T Bit storage type.
 * @param vec u8 vector to be converted.
 * @return T Resulting value.
 */
template <typename T>
inline T from_u8_vector(const u8_vector_t& vec)
{
    return from_u8_array<T>(vec.data(), vec.size());
}

/**
 * @brief Build LUT to assist with GF(2) polynomial remainder computation
 *
 * The resulting LUT can be used to compute "y % x" more efficiently for any "y" and a
 * given "x". More specifically, it maps each possible input byte representative of a
 * dividend polynomial y to the bits that would leak into the succeeding bytes within the
 * remainder computation. In the end, the LUT allows for computing the remainder with one
 * iteration per byte instead of one interation per bit.
 *
 * @tparam T Type whose bits represent the binary polynomial coefficients.
 * @param x Divisor polynomial.
 * @note The divisor should have degree less than or equal to "(sizeof(T) - 1) * 8".
 * @return std::array<T, 256> Byte-by-byte remainder look-up table.
 */
template <typename T>
std::array<T, 256> build_gf2_poly_rem_lut(const gf2_poly<T>& x)
{
    // As the divisor x is bit-shifted and XORed over the bits of an input byte, the
    // result leaks over at least x.degree() bits following the given byte. More
    // generally, the division of an input byte by x leaks over as many succeeding bytes
    // as we want to compute in advance, but the result always occupies the last
    // x.degree() bits within the designated leak space. For instance, suppose the divisor
    // is x = x^8 + x^3 + 1 (i.e., x = 0x109). If we wanted to compute 4 leak bytes in
    // advance for an input byte equal to 1, we would compute 0x100000000 % 0x109, which
    // results in 0x0000009A, with the non-zero part occupying the 8 least significant
    // bits of the 4 leak bytes. In contrast, if we wanted 2 leak bytes, we would compute
    // 0x10000 % 0x109, which yields 0x0041, again occupying the 8 least significant bits.
    // Here, compute the maximum number of leak bytes that type T can hold, assuming one
    // byte of T is already used to store the input byte (the dividend).
    const unsigned int n_leak_bytes = sizeof(T) - 1;

    // But make sure the leak space is enough to hold the remainder of division by x. Note
    // this verification implies the maximum acceptable degree of x is "n_leak_bytes * 8".
    if (x.degree() > static_cast<int>(n_leak_bytes) * 8)
        throw std::runtime_error("Failed to compute remainder LUT. Type T is too small.");

    std::array<T, 256> table;
    for (int i = 0; i < 256; i++) {
        T padded_in_byte = static_cast<T>(i) << (n_leak_bytes * 8);
        gf2_poly<T> y(padded_in_byte);
        table[i] = (y % x).get_poly();
    }
    return table;
}

/**
 * @brief Compute the remainder "y % x" of GF2 polynomials y and x using a LUT
 *
 * @tparam T Type whose bits represent the binary polynomial coefficients.
 * @param y Dividend GF(2) polynomial given by an array of bytes in network byte order
 * (big-endian), i.e., with the most significant byte at index 0.
 * @param y_size Size of the dividend polynomial y in bytes.
 * @param x Divisor GF(2) polynomial.
 * @param x_lut LUT generated by the `build_gf2_poly_rem_lut` function for polynomial x.
 * @return gf2_poly<T> Resulting remainder.
 */
template <typename T>
gf2_poly<T> gf2_poly_rem(u8_cptr_t y,
                         const int y_size,
                         const gf2_poly<T>& x,
                         const std::array<T, 256>& x_lut)
{
    static constexpr int n_leak_bytes = sizeof(T) - 1; // see build_gf2_poly_rem_lut
    static constexpr uint32_t bytes_after_msby = n_leak_bytes - 1;
    static constexpr uint32_t bits_after_msby = bytes_after_msby * 8;
    const T leak_mask = bitmask<T>(n_leak_bytes * 8);

    // Over the first "y_size - n_leak_bytes" bytes, iteratively look up the leak that the
    // input byte introduces into the next n_leak_bytes bytes.
    T leak = 0;
    for (int i = 0; i < y_size - n_leak_bytes; i++) {
        // Incorporate the preceding leak into the input byte. The leak spans over
        // n_leak_bytes of the type-T word, and the resulting most significant byte (MSBy)
        // determines the next leak. The other bytes (other than the MSBy) from the
        // preceding leak continue to leak (are carried forward) over the next bytes.
        uint8_t in_byte_plus_leak = y[i] ^ get_msby(leak, bits_after_msby);
        T leak_carried_forward = (leak_mask & (leak << 8));
        leak = leak_carried_forward ^ x_lut[in_byte_plus_leak];
    }

    // Convert the last n_leak_bytes of the input vector into a word of type T.
    //
    // In doing so, note that:
    // - We assume the order of bytes on the input u8_vector y is the order in which they
    //   arrive on serialization, i.e., y[0] arrives first and y[size-1] arrives last.
    //   Also, we could assume y carries a word in network byte order (big-endian), with
    //   y[0] representing the most significant byte and y[size-1] the least.
    // - The last n_leak_bytes are converted into a word of type T in network byte order,
    //   For instance, with T = uint32_t, an input vector {0x0A, 0x0B, 0x0C, 0x0D} would
    //   be converted into an unsigned int equal to 0x0A0B0C0D.
    // - The last n_leak_bytes are guaranteed to fit in T, otherwise
    //   build_gf2_poly_rem_lut would have thrown an exception.
    unsigned int n_last_bytes = std::min(n_leak_bytes, y_size);
    T y_last_bytes = from_u8_array<T>(y + y_size - n_last_bytes, n_last_bytes);

    // Incorporate the leak from the preceding bytes, if any
    y_last_bytes ^= leak;

    return gf2_poly<T>(y_last_bytes) % x;
}

/**
 * @overload
 * @param y Dividend GF(2) polynomial given by a vector of bytes in network byte order.
 * @param x Divisor GF(2) polynomial.
 * @param x_lut LUT generated by the `build_gf2_poly_rem_lut` function for polynomial x.
 */
template <typename T>
gf2_poly<T>
gf2_poly_rem(const u8_vector_t& y, const gf2_poly<T>& x, const std::array<T, 256>& x_lut)
{
    return gf2_poly_rem(y.data(), y.size(), x, x_lut);
}

} // namespace dvbs2rx
} // namespace gr

#endif // INCLUDED_DVBS2RX_GF_UTIL_H