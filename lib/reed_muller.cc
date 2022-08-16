/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "reed_muller.h"
#include <volk/volk.h>
#include <algorithm>
#include <numeric>

namespace gr {
namespace dvbs2rx {

/**
 * \brief Interleave the bits from the given 32-bit words a and b.
 * \return 64-bit word with bits "a31,b31,a30,b30,...,a0,b0".
 */
uint64_t bit_interleave(uint32_t a, uint32_t b)
{
    uint64_t res = 0;
    for (uint32_t i = 0; i < 32; i++) {
        res |= ((uint64_t)a & (1lu << i)) << (i + 1);
        res |= ((uint64_t)b & (1lu << i)) << i;
    }
    return res;
}

reed_muller::reed_muller(euclidean_map_func_ptr p_custom_map)
    : d_euclidean_img_lut(n_plsc_codewords * PLSC_LEN), d_dot_prod_buf(n_plsc_codewords)
{
    d_enabled_codewords.resize(n_plsc_codewords);
    std::iota(d_enabled_codewords.begin(),
              d_enabled_codewords.end(),
              0); // all codewords allowed
    init(p_custom_map);
}

reed_muller::reed_muller(std::vector<uint8_t>&& enabled_codewords,
                         euclidean_map_func_ptr p_custom_map)
    : d_enabled_codewords(std::move(enabled_codewords)),
      d_euclidean_img_lut(n_plsc_codewords * PLSC_LEN),
      d_dot_prod_buf(n_plsc_codewords)
{
    auto it_max =
        std::max_element(d_enabled_codewords.begin(), d_enabled_codewords.end());
    if (*it_max >= n_plsc_codewords) {
        throw std::runtime_error("Codeword indexes must be within [0, 128)");
    }

    init(p_custom_map);
}

void reed_muller::init(euclidean_map_func_ptr p_custom_map)
{
    /* Generator matrix (see Figure 13b on the standard) */
    uint32_t G[6] = { 0x55555555, 0x33333333, 0x0f0f0f0f,
                      0x00ff00ff, 0x0000ffff, 0xffffffff };

    /* Prepare a look-up table (LUT) with the interleaved (64, 7, 32)
     * Reed-Muller codewords used by the physical layer signaling code (PLSC).
     *
     * On the outer loop, compute all possible 32-bit codewords of the (32, 6,
     * 16) Reed-Muller code, namely the codewords of the RM(1,5) code in RM(r,m)
     * notation. Note this leads to 2^6=64 possible 32-bit codewords. Then,
     * expand each of these codewords into two 64-bit interleaved (64, 7, 32)
     * Reed-Muller codewords (or RM(1,6)) with the construction described in
     * Section 5.5.2.4 of the standard. */
    for (uint8_t i = 0; i < 64; i++) {
        /* Each 32-bit RM(1,5) codeword is a linear combination (modulo 2) of
         * the rows of G. Note the MSB of the PLSC (denoted as b1 in the
         * standard) multiplies G[0], b2 multiplies G[1], and so on, until b6
         * multiplies G[5]. Meanwhile, the LSB (denoted as b7 in the standard)
         * is not used for RM(1,5) encoding. Instead, it is reserved for usage
         * in the interleaving scheme implemented next. */
        uint32_t code32 = 0;
        for (int row = 0; row < 6; row++) {
            // Assume i is the 6-bit dataword with PLSC bits b1,...,b6
            if (i & (0x20 >> row))
                code32 ^= G[row]; // modulo-2 (binary field) addition
        }

        /* Now form the interleaved 64-bit codewords
         *
         * When the PLSC's LSB (denoted as b7 in the standard) is 1, the
         * interleaved RM(1,6) codeword becomes (y1 !y1 y2 !y2 ... y32 !y32),
         * where y1..y32 represents the 32-bit RM(1,5) codeword. In contrast,
         * when b7=0, the interleaved RM(1,6) codeword becomes (y1 y1 y2 y2
         * ... y32 y32). Here, we consider that b7=1 on odd indexes of the LUT
         * and b7=0 on even indexes. */
        d_codeword_lut[2 * i] = bit_interleave(code32, code32);
        d_codeword_lut[2 * i + 1] = bit_interleave(code32, ~code32);
    }

    /* Prepare a LUT with the Euclidean-space images (real vectors) of all
     * possible codewords. If a custom Euclidean-space mapping function is
     * provided by argument, used that. Otherwise, use the default mapping based
     * on ordinary 2-PAM. Ultimately, this LUT is used by the soft decoder. */
    euclidean_map = (p_custom_map) ? p_custom_map : &default_euclidean_map;
    for (uint8_t i = 0; i < n_plsc_codewords; i++) {
        float* dest_ptr = d_euclidean_img_lut.data() + (i * 64);
        euclidean_map(dest_ptr, d_codeword_lut[i]);
    }
}

void reed_muller::default_euclidean_map(float* dptr, uint64_t codeword)
{
    // Ordinary 2-PAM mapping:
    for (uint8_t i = 0; i < 64; i++) {
        bool bit = (codeword >> (63 - i)) & 1;
        dptr[i] = 1 - (2 * bit);
    }
}

uint64_t reed_muller::encode(uint8_t in_dataword) { return d_codeword_lut[in_dataword]; }

uint8_t reed_muller::decode(uint64_t hard_dec)
{
    uint8_t out_dataword = 0;

    /* ML decoding: find the codeword with the lowest Hamming distance relative
     * to the received/input codeword. The index corresponding to the minimum
     * distance is already the decoded dataword due to the LUT arrangement. */
    uint64_t distance;
    uint64_t min_distance = 65;
    for (uint8_t i : d_enabled_codewords) {
        /* Hamming distance to the i-th possible codeword */
        volk_64u_popcnt(&distance, hard_dec ^ d_codeword_lut[i]);
        /* Recall that the **Hamming distance** between x and y is equivalent to
         * the **Hamming weight** (or population count) of "x - y", which in
         * turn is equivalent to the weight of "x + y" in a binary field (with
         * bitwise mod-2 addition), i.e., equivalent to "weight(x ^ y)". */
        if (distance < min_distance) {
            min_distance = distance;
            out_dataword = i;
        }
    }
    return out_dataword;
}

uint8_t reed_muller::decode(const float* soft_dec)
{
    // The soft decoding, also known as (maximum inner-product decoding), is
    // based on the minimum distance between the input symbols (here, referred
    // to as "soft decisions") and all possible Euclidean-space images.
    //
    // If r is a received complex sequence and s(x) is a complex Euclidean-space
    // image corresponding to codeword x, then the minimum distance decoder
    // seeks the x that minimizes ||r - s(x)||^2. By expressing the norm as an
    // inner product, we get:
    //
    // ||r - s(x)||^2 = <r - s(x), r - s(x)>
    //                = <r, r> + <r, -s(x)> + <-s(x), r> + <s(x), s(x)>
    //
    // Using the conjugate symmetry property of the complex inner product, it
    // follows that:
    //
    //                = <r,r> + <r, -s(x)> + conj(<r, -s(x)>) + <s(x),s(x)>
    //                = <r,r> + 2*real(<r, -s(x)>) + <s(x),s(x)>
    //                = ||r||^2 + 2*real(<r, -s(x)>) + ||s(x)||^2
    // ||r - s(x)||^2 = ||r||^2 - 2*real(<r, s(x)>) + ||s(x)||^2
    //
    // This expression is further reduced by two major assumptions:
    //
    // Assumption 1: ||s(x)||^2 is the same for all x.
    //
    // When all Euclidean-space images s(x) have the same magnitude, as assumed
    // here, the ||s(x)||^2 term can be neglected when pursuing the minimum
    // distance. Similarly, the term ||r||^2 is the same regardless of the
    // tested codeword x. This means that, ultimately, to minimize the norm, we
    // can pursue the codeword x that maximizes the real-part of the inner
    // product <r, s(x)>. Hence, this decoder is called a maximum inner-product
    // decoder. See Section 6.5.1 on Forney's book.
    //
    // Assumption 2: s(x) is a real vector for all x.
    //
    // A further simplification becomes possible by considering that s(x) is a
    // real vector instead of a complex vector, as considered here. To start,
    // note that the complex inner product can be expressed as:
    //
    // <r, s(x)> = sum_k(r_k * conj(s_k(x))),
    //
    // where the sum_k() operator denotes the summation over k, r_k is the k-th
    // element of the complex vector r and s_k(x) is the k-th element of
    // s(x). When s(x) is a real vector, this expression becomes equivalent to:
    //
    // <r, s(x)> = sum_k(real(r_k) * s_k(x)) + j*sum_k(imag(r_k) * s_k(x))
    //
    // Since we want the real part of the inner product, it follows that:
    //
    // real(<r, s(x)>) = sum_k(real(r_k) * s_k(x)),
    //
    // which requires real multiplications only.
    //
    // In the end, the codeword that minimizes ||r - s(x)||^2 can be obtained by
    // searching for the codeword that maximizes the real inner product
    // between the real part of the input symbols (even if they are originally
    // complex) and the real Euclidean-space s(x) of each codeword x, provided
    // that the above two assumptions hold.
    for (uint8_t i : d_enabled_codewords) {
        const float* p_euclidean_img = d_euclidean_img_lut.data() + (i * 64);
        volk_32f_x2_dot_prod_32f(&d_dot_prod_buf[i], soft_dec, p_euclidean_img, 64);
    }
    uint32_t argmax;
    volk_32f_index_max_32u(&argmax, d_dot_prod_buf.data(), n_plsc_codewords);
    return (uint8_t)argmax;
}

} // namespace dvbs2rx
} // namespace gr
