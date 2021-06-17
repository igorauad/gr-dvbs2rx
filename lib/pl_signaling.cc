/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pi2_bpsk.h"
#include "pl_signaling.h"
#include <volk/volk.h>

namespace gr {
namespace dvbs2rx {

void plsc_encoder::encode(gr_complex* bpsk_out, const uint8_t plsc)
{
    uint64_t codeword = d_reed_muller_encoder.encode(plsc);
    map_bpsk(codeword ^ plsc_scrambler, bpsk_out, PLSC_LEN);
}

void plsc_encoder::encode(gr_complex* bpsk_out,
                          const uint8_t modcod,
                          bool short_fecframe,
                          bool has_pilots)
{
    uint8_t plsc = ((modcod & 0x1F) << 2) | (short_fecframe << 1) | has_pilots;
    encode(bpsk_out, plsc);
}

plsc_decoder::plsc_decoder(int debug_level)
    : d_debug_level(debug_level),
      d_plsc_bpsk_lut(PLSC_LEN * n_plsc_codewords),
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
    // Generate a LUT with all possible sequences of pi/2 BPSK symbols
    // representing the scrambled PLSC:
    plsc_encoder plsc_mapper;
    for (uint8_t plsc = 0; plsc < n_plsc_codewords; plsc++) { // codewords
        gr_complex* ptr = d_plsc_bpsk_lut.data() + (plsc * PLSC_LEN);
        plsc_mapper.encode(ptr, plsc);
    }
}

void plsc_decoder::decode(const gr_complex* bpsk_in, bool coherent, bool soft)
{
    /* First demap the pi/2 BPSK PLSC */
    if (soft) {
        // Soft decoding (maximum inner-product decoding)
        //
        // This approach is based on the minimum distance between the arriving
        // pi/2 BPSK symbols and the known possible sequences of pi/2 BPSK
        // symbols out of each possible codeword (after scrambling).
        //
        // If r is the received complex sequence and s(x) is the complex pi/2
        // BPSK sequence corresponding to codeword x, then the minimum distance
        // decoder seeks the x that minimizes ||r - s(x)||^2. By expressing the
        // norm as an inner product, we get:
        //
        // ||r - s(x)||^2 = <r - s(x), r - s(x)>
        //                = <r, r> + <r, -s(x)> + <-s(x), r> + <s(x), s(x)>
        //
        // Using the conjugate symmetry property of the complex inner product,
        // this reduces to:
        //
        //                = <r,r> + <r, -s(x)> + conj(<r, -s(x)>) + <s(x),s(x)>
        //                = <r,r> + 2*real(<r, -s(x)>) + <s(x),s(x)>
        //                = ||r||^2 + 2*real(<r, -s(x)>) + ||s(x)||^2
        // ||r - s(x)||^2 = ||r||^2 - 2*real(<r, s(x)>) + ||s(x)||^2
        //
        // Since all BPSK symbols have the same magnitude, the ||s(x)||^2 term
        // can be neglected when pursuing the minimum distance. Similarly, the
        // term ||r||^2 is the same for all possible codewords x. Ultimately, we
        // can pursue the codeword x that maximizes the real-part of the inner
        // product <r, s(x)>. Hence, this decoder is called a maximum
        // inner-product decoder. See Section 6.5.1 on Forney's book.
        //
        // From another viewpoint, real(<r, s(x)>) is the log likelihood ratio
        // LLR(r), aside from a missing scaling factor of 4/N0, see Section
        // 8.3.4 on Gallager's book. The factor of 4/N0 is irrelevant when the
        // codewords are equiprobable, so real(<r, s(x)>) is a sufficient
        // statistic. The maximum likelihood decoder pursues the codeword x that
        // maximizes this scaled LLR given by real(<r, s(x)>).
        gr_complex dot_prod;
        float max_re_dot_prod = 0;
        for (uint8_t i = 0; i < n_plsc_codewords; i++) { // codewords
            const gr_complex* ptr = d_plsc_bpsk_lut.data() + (i * PLSC_LEN);
            volk_32fc_x2_conjugate_dot_prod_32fc(&dot_prod, bpsk_in + 1, ptr, PLSC_LEN);
            if (real(dot_prod) > max_re_dot_prod) {
                max_re_dot_prod = real(dot_prod);
                dec_plsc = i;
            }
        }
    } else {
        // Hard decoding
        uint64_t rx_scrambled_plsc;
        // Assume bpsk_in points to a contiguous complex array starting at the
        // last SOF symbol and followed by the PLSC symbols. Use the last SOF
        // symbol for differential demapping and skip it otherwise.
        if (coherent) {
            rx_scrambled_plsc = demap_bpsk(bpsk_in + 1, PLSC_LEN);
        } else {
            rx_scrambled_plsc = demap_bpsk_diff(bpsk_in, PLSC_LEN);
        }

        /* Descramble */
        uint64_t rx_plsc = rx_scrambled_plsc ^ plsc_scrambler;

        if (d_debug_level > 4)
            printf("%s: descrambled PLSC: 0x%016lX\n", __func__, rx_plsc);

        /* Decode and parse the decoded PLSC */
        dec_plsc = d_reed_muller_decoder.decode(rx_plsc);
    }
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

    if (d_debug_level > 0) {
        printf("Decoded PLSC: {MODCOD: %2u, Short FECFRAME: %1u, Pilots: %1u}\n",
               modcod,
               short_fecframe,
               has_pilots);
    }
    if (d_debug_level > 1) {
        printf("Decoded PLSC: {n_mod: %1u, S: %3u, PLFRAME length: %u}\n",
               n_mod,
               S,
               plframe_len);
    }
}

} // namespace dvbs2rx
} // namespace gr
