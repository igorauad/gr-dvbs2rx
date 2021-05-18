/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "reed_muller.h"
#include <volk/volk.h>

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

reed_muller::reed_muller()
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

        /* Now form the interleaved 64-bit codeword
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
}

uint64_t reed_muller::encode(uint8_t in_dataword) { return d_codeword_lut[in_dataword]; }

uint8_t reed_muller::decode(uint64_t in_codeword)
{
    uint8_t out_dataword = 0;

    /* ML decoding: find the codeword with the lowest Hamming distance relative
     * to the received/input codeword. The index corresponding to the minimum
     * distance is already the decoded dataword due to the LUT arrangement. */
    uint64_t distance;
    uint64_t min_distance = 64;
    for (int i = 0; i < n_plsc_codewords; i++) {
        /* Hamming distance to the i-th possible codeword */
        volk_64u_popcnt(&distance, in_codeword ^ d_codeword_lut[i]);
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

} // namespace dvbs2rx
} // namespace gr
