/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pi2_bpsk.h"
#include "plsc_decoder.h"
#include <volk/volk.h>

namespace gr {
namespace dvbs2rx {

plsc_decoder::plsc_decoder(int debug_level)
    : debug_level(debug_level),
      dec_plsc(0),
      modcod(0),
      short_fecframe(false),
      has_pilots(false),
      dummy_frame(false),
      n_mod(0),
      S(0),
      plframe_len(0),
      n_pilots(0)
{
    /* Generator matrix */
    uint32_t G[6] = { 0x55555555, 0x33333333, 0x0f0f0f0f,
                      0x00ff00ff, 0x0000ffff, 0xffffffff };
    /* Pre-compute codewords */
    /* TODO: add an optional MODCOD filter and compute codewords only for the
     * acceptable MODCODs when provided. This way, the PLSC decoding can
     * leverage on prior knowledge about the signal and become more robust */
    for (int index = 0; index < n_plsc_codewords; index++) {
        /* Each 32-bit codeword is a linear combination (modulo 2) of the rows
         * of G. Note The most significant bit of the MODCOD is multiplied with
         * the first row of G. */
        uint32_t code32 = 0;
        for (int row = 0; row < 6; row++) {
            if ((index >> (6 - row)) & 1)
                code32 ^= G[row];
        }

        /* Now form the interleaved 64-bit codeword */
        uint64_t code64 = 0;
        for (int bit = 31; bit >= 0; bit--) {
            int yi = (code32 >> bit) & 1;
            /* At odd indexes, the TYPE LSB is 1, hence the sequence must be (y1
             * !y1 y2 !y2 ... y32 !y32). Otherwise, the sequence is (y1 y1 y2 y2
             * ... y32 y32) */
            if (index & 1)
                code64 = (code64 << 2) | (yi << 1) | (yi ^ 1);
            else
                code64 = (code64 << 2) | (yi << 1) | yi;
        }

        codewords[index] = code64;
    }
}

void plsc_decoder::decode(const gr_complex* in, bool coherent)
{
    /* First demap the pi/2 BPSK PLSC */
    uint64_t rx_scrambled_plsc;
    if (coherent)
	    rx_scrambled_plsc = demap_bpsk(in, PLSC_LEN);
    else
        rx_scrambled_plsc = demap_bpsk_diff(in, PLSC_LEN);

    /* Descramble */
    uint64_t rx_plsc = rx_scrambled_plsc ^ plsc_scrambler;

    if (debug_level > 4)
        printf("%s: descrambled PLSC: 0x%016lX\n", __func__, rx_plsc);

    /* ML decoding : find codeword with lowest Hamming distance */
    uint64_t distance;
    uint64_t min_distance = 64;
    for (int i = 0; i < n_plsc_codewords; i++) {
        /* Hamming distance to the i-th possible codeword */
        volk_64u_popcnt(&distance, rx_plsc ^ codewords[i]);
        if (distance < min_distance) {
            min_distance = distance;
            dec_plsc = i;
        }
    }

    /* Parse the decoded PLSC */
    modcod = dec_plsc >> 2;
    short_fecframe = dec_plsc & 0x2;
    has_pilots = dec_plsc & 0x1;
    dummy_frame = modcod == 0;
    has_pilots &= !dummy_frame; // a dummy frame cannot have pilots

    /* Number of bits per constellation symbol and PLFRAME slots */
    if (modcod >= 1 && modcod <= 11) {
        n_mod = 2;
        S = 360;
    } else if (modcod >= 12 && modcod <= 17) {
        n_mod = 3;
        S = 240;
    } else if (modcod >= 18 && modcod <= 23) {
        n_mod = 4;
        S = 180;
    } else if (modcod >= 24 && modcod <= 28) {
        n_mod = 5;
        S = 144;
    } else {
        n_mod = 0;
        S = 36; // dummy frame
    }

    /* For short FECFRAMEs, S is 4 times lower */
    if (short_fecframe && !dummy_frame)
        S >>= 2;

    /* Number of pilot blocks */
    n_pilots = (has_pilots) ? ((S - 1) >> 4) : 0;

    /* PLFRAME length including header */
    plframe_len = ((S + 1) * 90) + (36 * n_pilots);

    if (debug_level > 0) {
        printf("Decoded PLSC: {MODCOD: %2u, Short FECFRAME: %1u, Pilots: %1u}\n",
               modcod,
               short_fecframe,
               has_pilots);

        if (debug_level > 1)
            printf("Decoded PLSC: {hammin dist: %2lu, n_mod: %1u, S: %3u, PLFRAME "
                   "length: %u}\n",
                   min_distance,
                   n_mod,
                   S,
                   plframe_len);
    }
}

} // namespace dvbs2rx
} // namespace gr
