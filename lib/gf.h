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
#include <unordered_map>
#include <bitset>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

namespace gr {
namespace dvbs2rx {

// Non-int types for storing GF(2) coefficients or GF(2^m) elements:
typedef std::bitset<256> bitset256_t;

template <typename T>
class DVBS2RX_API gf2_poly;

template <typename T>
class DVBS2RX_API gf2m_poly;

/**
 * @brief Test if bit is set
 *
 * @param x Bit register.
 * @param i_bit Target bit index.
 * @return true if bit is 1 and false otherwise.
 */
template <typename T>
inline bool is_bit_set(const T& x, int i_bit)
{
    return x & (static_cast<T>(1) << i_bit);
}

/**
 * @overload
 * @note Template specialization for T = bitset256_t.
 */
template <>
inline bool is_bit_set(const bitset256_t& x, int i_bit)
{
    return x.test(i_bit);
}

/**
 * @brief Galois Field GF(2^m).
 *
 * @tparam T Base type for the field elements.
 * @note See the reference implementation at
 * https://github.com/igorauad/bch/blob/master/gf.py.
 */
template <typename T>
class DVBS2RX_API galois_field
{
private:
    const uint8_t m_m;                           // dimension of the GF(2^m) field
    const uint32_t m_two_to_m_minus_one;         // shortcut for (2^m - 1)
    std::vector<T> m_table;                      // field elements
    std::vector<T> m_table_nonzero;              // non-zero field elements
    std::unordered_map<T, uint32_t> m_exp_table; // map element alpha^i to its exponent i

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
    T operator[](uint32_t index) const { return m_table[index]; }

    /**
     * @brief Get the i-th power of the primitive element (alpha^i).
     *
     * @param i Exponent i of the target element alpha^i for i from 0 to "2**m - 2".
     * @return T Element alpha^i.
     * @note This function cannot obtain the zero (additive identity) element, given the
     * zero element cannot be expressed as a power of the primitive element. To access the
     * zero element, use the operator[] instead.
     */
    T get_alpha_i(uint32_t i) const { return m_table_nonzero[i % m_two_to_m_minus_one]; }

    /**
     * @brief Get the exponent i of a given element beta = alpha^i.
     *
     * @param beta Element beta that is a power of the primitive element alpha.
     * @return T Exponent i.
     * @note This function cannot obtain the exponent of the zero (additive identity)
     * element, given the zero element cannot be expressed as a power of the primitive
     * element alpha. A runtime error exception is raised if beta is the zero element.
     */
    uint32_t get_exponent(const T& beta) const { return m_exp_table.at(beta); }

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
     * @brief Get the inverse from a GF(2^m) element alpha^i given by its exponent i.
     *
     * @param i Exponent i of the element beta = alpha^i to be inverted.
     * @return T Inverse beta^-1.
     */
    T inverse_by_exp(uint32_t i) const;

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
 * @brief Get the maximum degree a GF2 polynomial can have with type T.
 *
 * @return int Maximum degree.
 */
template <typename T>
inline constexpr size_t get_max_gf2_poly_degree()
{
    return (sizeof(T) * 8 - 1);
}

/**
 * @overload for T = bitset256_t.
 */
template <>
inline constexpr size_t get_max_gf2_poly_degree<bitset256_t>()
{
    return bitset256_t().size() - 1;
}

/**
 * @brief Polynomial over GF(2).
 *
 * A polynomial whose coefficients are elements from GF(2), i.e., binary.
 *
 * @tparam T Type whose bits represent the binary polynomial coefficients.
 */
template <typename T>
class DVBS2RX_API gf2_poly
{
private:
    static constexpr int m_max_degree = get_max_gf2_poly_degree<T>();

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
    gf2_poly<T> operator+(const gf2_poly<T>& x) const { return m_poly ^ x.get_poly(); }

    /**
     * @brief Multiplication by a GF(2) scalar.
     *
     * @param x Binary scalar to multiply.
     * @return gf2_poly Multiplication result.
     */
    gf2_poly<T> operator*(bool x) const { return x ? m_poly : 0; }

    /**
     * @brief Multiplication by another GF(2) polynomial.
     *
     * @param x Polynomial to multiply.
     * @return gf2_poly Multiplication result.
     */
    gf2_poly<T> operator*(const gf2_poly<T>& x) const;

    /**
     * @brief Equal comparator.
     *
     * @param x The other GF(2^m) polynomial.
     * @return bool Whether they are equal.
     */
    bool operator==(const gf2_poly<T>& x) const { return m_poly == x.get_poly(); }

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

    /**
     * @brief Test if the polynomial is the zero polynomial.
     *
     * @return true if the polynomial is zero and false otherwise.
     */
    bool is_zero() const { return m_degree == -1; }
};

/**
 * @brief Compute the remainder of the division between two GF(2) polynomials.
 *
 * Computes the remainder of polynomial over GF(2) a(x) divided by another polynomial over
 * GF(2) b(x), i.e., computes a(x) % b(x). The result has degree up to the degree of b(x)
 * minus one. Hence, the result necessarily fits within the type used to store b(x).
 *
 * @tparam Ta Type of the dividend polynomial.
 * @tparam Tb Type of the divisor polynomial.
 * @param a Dividend polynomial.
 * @param b Divisor polynomial.
 * @return gf2_poly<Tb> Remainder result.
 * @throws std::runtime_error if b(x) is a zero polynomial.
 * @note The implementation requires the divisor type Tb to be larger than or equal to the
 * dividend type Ta. Otherwise, a static assertion is raised.
 */
template <typename Ta, typename Tb>
inline gf2_poly<Tb> operator%(const gf2_poly<Ta> a, const gf2_poly<Tb> b)
{
    check_rem_types(a, b); // ensure the types are
    if (b.degree() == -1)  // zero divisor
        throw std::runtime_error("Remainder of division by a zero polynomial");
    if (a.degree() == -1) // zero dividend
        return gf2_poly<Tb>(0);
    if (a.degree() < b.degree()) // remainder is the dividend polynomial a(x) itself
        return gf2_poly<Tb>(a.get_poly());

    int b_degree = b.degree();
    const Tb b_coefs = b.get_poly();
    Tb remainder = a.get_poly(); // here type Tb must be large enough to store a(x)
    for (int i = a.degree(); i >= b_degree; i--) {
        if (is_bit_set(remainder, i))
            remainder ^= b_coefs << (i - b_degree);
    }
    return remainder;
}

template <typename Ta, typename Tb>
inline void check_rem_types(const gf2_poly<Ta> a, const gf2_poly<Tb> b)
{
    static_assert(sizeof(Tb) >= sizeof(Ta),
                  "Divisor type must be larger or equal than dividend type");
}

template <typename Tb>
inline void check_rem_types(const gf2_poly<bitset256_t> a, const gf2_poly<Tb> b)
{
    static_assert(sizeof(Tb) * 8 >= bitset256_t().size(),
                  "Divisor type must be larger or equal than dividend type");
}

template <typename Ta>
inline void check_rem_types(const gf2_poly<Ta> a, const gf2_poly<bitset256_t> b)
{
    static_assert(bitset256_t().size() >= sizeof(Ta) * 8,
                  "Divisor type must be larger or equal than dividend type");
}

inline void check_rem_types(const gf2_poly<bitset256_t> a, const gf2_poly<bitset256_t> b)
{
    static_assert(true); // the two types are the same
}

/**
 * @brief Polynomial over GF(2^m).
 *
 * A polynomial whose coefficients are elements from a GF(2^m) extension field.
 *
 * @tparam T Type used to represent each polynomial coefficient as a GF(2^m) element.
 */
template <typename T>
class DVBS2RX_API gf2m_poly
{
private:
    const galois_field<T>* m_gf; // Galois field
    std::vector<T> m_poly;       // Polynomial coefficients
    // NOTE: index 0 has the zero-degree coefficient, index 1 has the coefficient of x,
    // index 2 of x^2, and so on.
    std::vector<uint32_t> m_nonzero_coef_idx; // Indexes (degrees) of the non-zero
                                              // polynomial coefficients
    std::vector<uint32_t> m_nonzero_coef_exp; // Exponents of the non-zero polynomial
                                              // coefficients
    size_t m_n_nonzero_coef;                  // Number of non-zero coefficients
    // Example: a polynomial "alpha^4 x^3 + alpha^2 x + 1" would have the following data
    // in the vectors: m_poly = [1, alpha^2, 0, alpha^4], m_nonzero_coef_idx = [0, 1, 3],
    // m_nonzero_coef_exp = [0, 2, 4].
    int m_degree; // Polynomial degree

    /**
     * @brief Set the polynomial degree.
     */
    void set_degree();

    /**
     * @brief Fill the indexes and exponents of the non-zero polynomial coefficients.
     *
     * Each coefficient can be represented as a GF(2^m) element alpha^j, where j is the
     * exponent and alpha is the primitive element. To speed up computations, this
     * function caches the exponents associated with the non-zero coefficients on a
     * dedicated vector. Also, it caches the degrees of the terms associated with such
     * non-zero coefficients.
     */
    void set_coef_exponents();

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
    template <typename Y>
    gf2m_poly(const galois_field<T>* const gf, const gf2_poly<Y>& gf2_poly) : m_gf(gf)
    {
        const Y& coefs = gf2_poly.get_poly();
        for (int i = 0; i <= gf2_poly.degree(); i++) {
            if (is_bit_set(coefs, i)) {
                m_poly.push_back((*gf)[1]);
            } else {
                m_poly.push_back((*gf)[0]);
            }
        }
        set_degree();
        set_coef_exponents();
    };

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
    bool operator==(const gf2m_poly<T>& x) const { return m_poly == x.get_poly(); }

    /**
     * @brief Access a polynomial coefficient.
     *
     * @param index Index of the target coefficient.
     * @return T Polynomial coefficient.
     */
    T operator[](uint32_t index) const { return m_poly[index]; }

    /**
     * @brief Evaluate the polynomial for a given GF(2^m) element.
     *
     * Assuming the underlying polynomial is p(x), this function evaluates p(x) for a
     * given x from GF(2^m).
     *
     * @param x GF(2^m) value for which the polynomial should be evaluated.
     * @return T Evaluation result within GF(2^m).
     */
    T eval(T x) const;

    /**
     * @brief Evaluate the polynomial for an element alpha^i given by its exponent i.
     *
     * Assuming the underlying polynomial is p(x), this function evaluates p(x) for a
     * given x from GF(2^m). The difference to the operator() is that this function takes
     * x by its exponent i. If x = alpha^i, this function takes i as input instead of x.
     *
     * @param i Exponent of the GF(2^m) value for which the polynomial should be
     * evaluated.
     * @return T Evaluation result within GF(2^m).
     * @note Since the zero element (additive identity) cannot be represented as a power
     * of the primitive element, there is no exponent i for the zero element. Hence, this
     * function can only evaluate the polynomial for non-zero elements.
     */
    T eval_by_exp(uint32_t i) const;

    /**
     * @brief Search roots by evaluating the polynomial for elements in a given range.
     *
     * Evaluates the polynomial for all alpha^i with i varying in [i_start, i_end] and
     * returns the elements for which the polynomial evaluates to zero (the roots) given
     * by their exponents. For instance, if alpha^2 and alpha^3 are roots of the
     * polynomial, this function returns the vector [2, 3].
     *
     * The implementation does not simply rely on the eval() or eval_by_exp() functions.
     * Instead, it is optimized by leveraging the fact that the evaluation is for a
     * contiguous range of exponents. Hence, when searching for polynomial roots in
     * GF(2^m), it is preferable to use this function instead of manually calling the
     * eval() or eval_by_exp() functions.
     *
     * @param i_start Exponent of element alpha^i_start at the start of the range.
     * @param i_end Exponent of element alpha^i_end at the end of the range.
     * @param max_roots Maximum number of roots to be returned. When defined, the search
     * is stopped earlier as soon as this number of roots is found.
     * @return std::vector<uint32_t> Vector with the exponents associated with the GF(2^m)
     * roots found in the range.
     */
    std::vector<uint32_t> search_roots_in_exp_range(
        uint32_t i_start,
        uint32_t i_end,
        uint32_t max_roots = std::numeric_limits<uint32_t>::max()) const;

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
typedef gf2_poly<bitset256_t> gf2_poly_b256;

} // namespace dvbs2rx
} // namespace gr

#endif // INCLUDED_DVBS2RX_GF_H