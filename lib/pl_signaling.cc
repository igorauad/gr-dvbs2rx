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

/**
 * @brief Custom Euclidean-space image mapper for the RM encoder/decoder
 *
 * The Reed-Muller encoder's constructor accepts a function to customize the
 * mapping from the 64-bit binary codewords to their corresponding real-valued
 * Euclidean-space images. It is convenient to consider that the Euclidean-space
 * image used by the PLSC decoder is the real-valued BPSK sequence (of +-1
 * symbols) corresponding to the scrambled codeword, instead of the original
 * (unscrambled) codeword. That saves an extra descrambling step.
 *
 */
void map_plsc_codeword_to_bpsk(float* dest_ptr, uint64_t codeword)
{
    reed_muller::default_euclidean_map(dest_ptr, codeword ^ plsc_scrambler);
};

plsc_decoder::plsc_decoder(int debug_level)
    : d_debug_level(debug_level),
      d_reed_muller_decoder(&map_plsc_codeword_to_bpsk),
      d_soft_dec_buf(PLSC_LEN),
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
}

void plsc_decoder::decode(const gr_complex* bpsk_in, bool coherent, bool soft)
{
    if (soft) {
        // Soft decoding
        //
        // The Reed-Muller decoder assumes that the Euclidean-space image of
        // each codeword is the real vector that results from scrambling the
        // original codeword and mapping it to real using an ordinary 2-PAM/BPSK
        // mapping instead of pi/2 BPSK. See the "map_plsc_codeword_to_bpsk"
        // function defined above.
        //
        // Hence, the pi/2 BPSK sequence is first converted/derotated to obtain
        // the corresponding real-valued 2-PAM/BPSK sequence. Then, this real
        // BPSK sequence (called the vector of "soft decisions") is provided to
        // the soft Reed-Muller decoder.
        derotate_bpsk(bpsk_in + 1, d_soft_dec_buf.data(), PLSC_LEN);
        dec_plsc = d_reed_muller_decoder.decode(d_soft_dec_buf.data());
    } else {
        // Hard decoding
        uint64_t rx_scrambled_plsc;
        // Demap the pi/2 BPSK PLSC
        //
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
