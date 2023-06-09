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
#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace gr {
namespace dvbs2rx {

template <typename T>
class DVBS2RX_API gf2_poly;

template <typename T>
class DVBS2RX_API gf2m_poly;

/**
 * @brief Galois Field GF(2^m).
 *
 * @note See the reference implementation at
 * https://github.com/igorauad/bch/blob/master/gf.py.
 */
template <typename T>
class DVBS2RX_API galois_field
{
private:
    const uint8_t m_m;                   // dimension of the GF(2^m) field
    const uint32_t m_two_to_m_minus_one; // shortcut for (2^m - 1)
    std::vector<T> m_table;              // field elements
    std::map<T, uint32_t>
        m_inv_table; // LUT to map element alpha^i to its GF table index i+1

public:
    /**
     * @brief Construct a new Galois field object.
     *
     * @param prim_poly Primitive polynomial that generates the field.
     */
    galois_field(const gf2_poly<T>& prim_poly);

    /**
     * @brief Get the GF(2^m) element at a given index on the elements table.
     *
     * @param index Target index.
     * @return T GF(2^m) element.
     */
    T operator[](uint32_t index) const;

    /**
     * @brief Get the i-th power of the primitive element (alpha^i).
     *
     * @param i Exponent i of the target element alpha^i for i from 0 to "2**m - 2".
     * @return T Element alpha^i.
     * @note This function cannot obtain the zero (additive identity) element, given the
     * zero element cannot be expressed as a power of the primitive element. To access the
     * zero element, use the operator[] instead.
     */
    T get_alpha_i(uint32_t i) const;

    /**
     * @brief Get the exponent i of a given element beta = alpha^i.
     *
     * @param beta Element beta that is a power of the primitive element alpha.
     * @return T Exponent i.
     */
    uint32_t get_exponent(const T& beta) const;

    /**
     * @brief Multiply two elements from GF(2^m).
     *
     * @param a First multiplicand.
     * @param b Second multiplicand.
     * @return T Product a*b.
     */
    T multiply(const T& a, const T& b) const;

    /**
     * @brief Get the inverse beta^-1 from a GF(2^m) element beta.
     *
     * @param beta Element to invert.
     * @return T Inverse beta^-1.
     */
    T inverse(const T& beta) const;

    /**
     * @brief Divide two elements from GF(2^m).
     *
     * @param a Dividend.
     * @param b Divisor.
     * @return T Quotient a/b.
     */
    T divide(const T& a, const T& b) const;

    /**
     * @brief Get the conjugates of element beta.
     *
     * @param beta The element whose conjugates are to be computed.
     * @return std::set<T> The set of distinct conjugates alpha^(i^(2^l)) in GF(2^m)
     * associated with element beta=alpha^i.
     */
    std::set<T> get_conjugates(const T& beta) const;

    /**
     * @brief Compute the minimal polynomial associated with element beta.
     *
     * Computes the polynomial phi(x) over GF(2) of smallest degree having beta as root.
     * See the notes in the reference Python implementation.
     *
     * @param beta GF(2^m) element.
     * @return gf2_poly Minimal polynomial of beta as a polynomial over GF(2).
     */
    gf2_poly<T> get_min_poly(const T& beta) const;

    /**
     * @brief Get the dimension m of the GF(2^m) field.
     *
     * @return uint8_t Dimension m.
     */
    uint8_t get_m() const { return m_m; };
};

/**
 * @brief Polynomial over GF(2).
 *
 * A polynomial whose coefficients are elements from GF(2), i.e., binary.
 */
template <typename T>
class DVBS2RX_API gf2_poly
{
private:
    static constexpr int m_max_degree = (sizeof(T) * 8 - 1);

    T m_poly; // Polynomial coefficients
    // NOTE: the LSB (bit 0) has the zero-degree coefficient, bit 1 has the coefficient of
    // x, bit 2 of x^2, and so on. For instance, a polynomial "x^4 + x + 1" should be
    // stored as "10011"/
    int m_degree; // Polynomial degree
    // NOTE: by convention, the zero polynomial has degree -1.

public:
    /**
     * @brief Construct a new GF(2) polynomial object.
     *
     * @param coefs Binary coefficients.
     */
    gf2_poly(const T& coefs);

    /**
     * @brief GF(2) polynomial addition.
     *
     * @param x Polynomial to add.
     * @return gf2_poly Addition result.
     */
    gf2_poly<T> operator+(const gf2_poly<T>& x) const;

    /**
     * @brief Multiplication by a GF(2) scalar.
     *
     * @param x Binary scalar to multiply.
     * @return gf2_poly Multiplication result.
     */
    gf2_poly<T> operator*(bool x) const;

    /**
     * @brief Multiplication by another GF(2) polynomial.
     *
     * @param x Polynomial to multiply.
     * @return gf2_poly Multiplication result.
     */
    gf2_poly<T> operator*(const gf2_poly<T>& x) const;

    /**
     * @brief Remainder of division by another GF(2) polynomial.
     *
     * @param x Divisor polynomial.
     * @return gf2_poly Remainder result.
     */
    gf2_poly<T> operator%(const gf2_poly<T>& x) const;

    /**
     * @brief Equal comparator.
     *
     * @param x The other GF(2^m) polynomial.
     * @return bool Whether they are equal.
     */
    bool operator==(const gf2_poly<T>& x) const;

    /**
     * @brief Get the polynomial coefficients.
     *
     * @return T storage of polynomial coefficients.
     */
    const T& get_poly() const { return m_poly; }

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
template <typename T>
class DVBS2RX_API gf2m_poly
{
private:
    const galois_field<T>* m_gf; // Galois field
    std::vector<T> m_poly;       // Polynomial coefficients
    // NOTE: index 0 has the zero-degree coefficient, index 1 has the coefficient of x,
    // index 2 of x^2, and so on.
    int m_degree; // Polynomial degree

    /**
     * @brief Set the polynomial degree.
     */
    void set_degree();

public:
    /**
     * @brief Construct a new polynomial over GF(2^m).
     *
     * @param gf Reference Galois field.
     * @param coefs Polynomial coefficients.
     */
    gf2m_poly(const galois_field<T>* const gf, std::vector<T>&& coefs);

    /**
     * @brief Construct a polynomial over GF(2^m) from a polynomial over GF(2).
     *
     * @param gf Reference Galois field.
     * @param gf2_poly Reference polynomial over GF(2).
     */
    gf2m_poly(const galois_field<T>* const gf, const gf2_poly<T>& gf2_poly);

    /**
     * @brief GF(2^m) polynomial addition.
     *
     * @param x Polynomial to add.
     * @return gf2m_poly<T> Addition result.
     */
    gf2m_poly<T> operator+(const gf2m_poly<T>& x) const;

    /**
     * @brief Multiplication by a scalar.
     *
     * @param x Scalar to multiply.
     * @return gf2m_poly<T> Multiplication result.
     */
    gf2m_poly<T> operator*(T x) const;

    /**
     * @brief Multiplication by another GF(2^m) polynomial.
     *
     * @param x Polynomial to multiply.
     * @return gf2m_poly<T> Multiplication result.
     */
    gf2m_poly<T> operator*(const gf2m_poly<T>& x) const;

    /**
     * @brief Equal comparator.
     *
     * @param x The other GF(2^m) polynomial.
     * @return bool Whether they are equal.
     */
    bool operator==(const gf2m_poly<T>& x) const;

    /**
     * @brief Evaluate the polynomial for a given GF(2^m) element.
     *
     * Assuming the underlying polynomial is p(x), this function evaluates p(x) for a
     * given x from GF(2^m).
     *
     * @param x GF(2^m) value for which the polynomial should be evaluated.
     * @return T Evaluation result within GF(2^m).
     */
    T operator()(T x) const;

    /**
     * @brief Get the polynomial coefficients.
     *
     * @return const std::vector<T>& Reference to vector of polynomial coefficients.
     */
    const std::vector<T>& get_poly() const { return m_poly; }

    /**
     * @brief Get the polynomial degree.
     *
     * @return int Degree.
     * @note By convention, the zero polynomial has degree -1.
     */
    int degree() const { return m_degree; }

    /**
     * @brief Convert the polynomial to a GF(2) polynomial.
     *
     * Works when all coefficients of the local polynomial are either unit or zero such
     * that it can be reduced to a polynomial over GF(2). Otherwise, throws runtime error.
     *
     * @return gf2_poly<T> Polynomial over GF(2).
     */
    gf2_poly<T> to_gf2_poly() const;
};

// Type definitions
typedef gf2_poly<uint16_t> gf2_poly_u16;
typedef gf2_poly<uint32_t> gf2_poly_u32;
typedef gf2_poly<uint64_t> gf2_poly_u64;

} // namespace dvbs2rx
} // namespace gr

#endif // INCLUDED_DVBS2RX_GF_H