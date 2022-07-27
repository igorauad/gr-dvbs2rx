/* -*- c++ -*- */
/*
 * Copyright (c) 2022 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_GF_H
#define INCLUDED_DVBS2RX_GF_H

#include <gnuradio/dvbs2rx/api.h>
#include <array>
#include <cstdint>
#include <set>
#include <vector>

namespace gr {
namespace dvbs2rx {

#define MAX_M 16
#define MAX_FIELD_ELEMENTS 1 << MAX_M

class DVBS2RX_API gf2_poly;
class DVBS2RX_API gf2m_poly;

/**
 * @brief Galois Field GF(2^m).
 *
 * @note See the reference implementation at
 * https://github.com/igorauad/bch/blob/master/gf.py.
 */
class DVBS2RX_API galois_field
{
private:
    uint8_t m_m;                   // dimension of the GF(2^m) field
    uint16_t m_prim_poly;          // 16-bit primitive polynomial excluding the MSB
    uint16_t m_two_to_m_minus_one; // shortcut for (2^m - 1)
    std::array<uint16_t, MAX_FIELD_ELEMENTS> m_table; // field elements
    std::array<uint16_t, MAX_FIELD_ELEMENTS>
        m_inv_table; // LUT to map element alpha^i to its GF table index i+1


public:
    /**
     * @brief Construct a new Galois field object.
     *
     * @param m GF(2^m) field dimension up to 16.
     * @param prim_poly Primitive polynomial with the lowest order term on the right (lsb)
     * and excluding the highest order term x^m. For instance, a polynomial "x^4 + x + 1"
     * should be given as "0011" instead of "10011".
     */
    galois_field(uint8_t m, uint16_t prim_poly);

    /**
     * @brief Get the GF(2^m) element at a given index on the elements table.
     *
     * @param index Target index.
     * @return uint16_t GF(2^m) element.
     */
    uint16_t operator[](int index) const;

    /**
     * @brief Get the i-th power of the primitive element (alpha^i).
     *
     * @param i Exponent i of the target element alpha^i for i from 0 to "2**m - 2".
     * @return uint16_t Element alpha^i.
     */
    uint16_t get_alpha_i(uint16_t i) const;

    /**
     * @brief Get the exponent i of a given element beta = alpha^i.
     *
     * @param beta Element beta that is a power of the primitive element alpha.
     * @return uint16_t Exponent i.
     */
    uint16_t get_exponent(uint16_t beta) const;

    /**
     * @brief Multiply two elements from GF(2^m).
     *
     * @param a First multiplicand.
     * @param b Second multiplicand.
     * @return uint16_t Product a*b.
     */
    uint16_t multiply(uint16_t a, uint16_t b) const;

    /**
     * @brief Get the inverse beta^-1 from a GF(2^m) element beta.
     *
     * @param beta Element to invert.
     * @return uint16_t Inverse beta^-1.
     */
    uint16_t inverse(uint16_t beta) const;

    /**
     * @brief Divide two elements from GF(2^m).
     *
     * @param a Dividend.
     * @param b Divisor.
     * @return uint16_t Quotient a/b.
     */
    uint16_t divide(uint16_t a, uint16_t b) const;

    /**
     * @brief Get the conjugates of element beta.
     *
     * @param beta The element whose conjugates are to be computed.
     * @return std::set<uint16_t> The set of distinct conjugates alpha^(i^(2^l)) in
     * GF(2^m) associated with element beta=alpha^i.
     */
    std::set<uint16_t> get_conjugates(uint16_t beta) const;

    /**
     * @brief Compute the minimal polynomial associated with element beta.
     *
     * Computes the polynomial phi(x) over GF(2) of smallest degree having beta as
     * root. See the notes in the reference Python implementation.
     *
     * @param beta GF(2^m) element.
     * @return gf2_poly Minimal polynomial of beta as a polynomial over GF(2).
     */
    gf2_poly get_min_poly(uint16_t beta) const;
};

/**
 * @brief Polynomial over GF(2).
 *
 * A polynomial whose coefficients are elements from GF(2), i.e., binary.
 */
class DVBS2RX_API gf2_poly
{
private:
    uint16_t m_poly; // Polynomial coefficients
    // NOTE: the LSB (bit 0) has the zero-degree coefficient, bit 1 has the coefficient of
    // x, bit 2 of x^2, and so on.
    int m_degree; // Polynomial degree
    // NOTE: by convention, the zero polynomial has degree -1.

    static constexpr int m_max_degree = 15; // uint16_t storage can support up to x^15

    /**
     * @brief Set the polynomial degree.
     */
    void set_degree();

public:
    /**
     * @brief Construct a new GF(2) polynomial object.
     *
     * @param coefs Binary coefficients.
     */
    gf2_poly(uint16_t coefs);

    /**
     * @brief Construct a GF(2) polynomial from a GF(2^m) polynomial.
     *
     * @param poly Polynomial over GF(2^m) whose coefficients are either unit or zero such
     * that it can be reduced to a polynomial over GF(2).
     * @note Throws runtime error if the given polynomial over GF(2^m) is not binary.
     */
    gf2_poly(const gf2m_poly& poly);

    /**
     * @brief GF(2) polynomial addition.
     *
     * @param x Polynomial to add.
     * @return gf2_poly Addition result.
     */
    gf2_poly operator+(const gf2_poly& x) const;

    /**
     * @brief Multiplication by a GF(2) scalar.
     *
     * @param x Binary scalar to multiply.
     * @return gf2_poly Multiplication result.
     */
    gf2_poly operator*(bool x) const;

    /**
     * @brief Multiplication by another GF(2) polynomial.
     *
     * @param x Polynomial to multiply.
     * @return gf2_poly Multiplication result.
     */
    gf2_poly operator*(const gf2_poly& x) const;

    /**
     * @brief Equal comparator.
     *
     * @param x The other GF(2^m) polynomial.
     * @return bool Whether they are equal.
     */
    bool operator==(const gf2_poly& x) const;

    /**
     * @brief Get the polynomial coefficients.
     *
     * @return uint16_t 16-bit storage of polynomial coefficients.
     */
    uint16_t get_poly() const { return m_poly; }

    /**
     * @brief Get the polynomial degree.
     *
     * @return int Degree.
     * @note By convention, the zero polynomial has degree -1.
     */
    int degree() const { return m_degree; }
};

/**
 * @brief Polynomial over GF(2^m).
 *
 * A polynomial whose coefficients are elements from a GF(2^m) extension field.
 */
class DVBS2RX_API gf2m_poly
{
private:
    const galois_field* m_gf;     // Galois field
    std::vector<uint16_t> m_poly; // Polynomial coefficients
    // NOTE: index 0 has the zero-degree coefficient, index 1 has the coefficient of x,
    // index 2 of x^2, and so on.
    int m_degree; // Polynomial degree

public:
    /**
     * @brief Construct a new polynomial over GF(2^m).
     *
     * @param gf Reference Galois field.
     * @param coefs Polynomial coefficients.
     */
    gf2m_poly(const galois_field* const gf, std::vector<uint16_t>&& coefs);

    /**
     * @brief GF(2^m) polynomial addition.
     *
     * @param x Polynomial to add.
     * @return gf2m_poly Addition result.
     */
    gf2m_poly operator+(const gf2m_poly& x) const;

    /**
     * @brief Multiplication by a scalar.
     *
     * @param x Scalar to multiply.
     * @return gf2m_poly Multiplication result.
     */
    gf2m_poly operator*(uint16_t x) const;

    /**
     * @brief Multiplication by another GF(2^m) polynomial.
     *
     * @param x Polynomial to multiply.
     * @return gf2m_poly Multiplication result.
     */
    gf2m_poly operator*(const gf2m_poly& x) const;

    /**
     * @brief Equal comparator.
     *
     * @param x The other GF(2^m) polynomial.
     * @return bool Whether they are equal.
     */
    bool operator==(const gf2m_poly& x) const;

    /**
     * @brief Get the polynomial coefficients.
     *
     * @return const std::vector<uint16_t>& Reference to vector of polynomial
     * coefficients.
     */
    const std::vector<uint16_t>& get_poly() const { return m_poly; }

    /**
     * @brief Get the polynomial degree.
     *
     * @return int Degree.
     * @note By convention, the zero polynomial has degree -1.
     */
    int degree() const { return m_degree; }
};

} // namespace dvbs2rx
} // namespace gr

#endif // INCLUDED_DVBS2RX_GF_H