/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "debug_level.h"
#include "pi2_bpsk.h"
#include "pl_signaling.h"
#include <volk/volk.h>
#include <cstring>

namespace gr {
namespace dvbs2rx {

void pls_info_t::parse(uint8_t dec_plsc)
{
    plsc = dec_plsc;
    modcod = dec_plsc >> 2;
    short_fecframe = dec_plsc & 0x2;
    has_pilots = dec_plsc & 0x1;
    dummy_frame = modcod == 0;
    has_pilots &= !dummy_frame; // a dummy frame cannot have pilots

    /* Number of bits per constellation symbol and PLFRAME slots */
    if (modcod >= 1 && modcod <= 11) {
        n_mod = 2;
        n_slots = 360;
    } else if (modcod >= 12 && modcod <= 17) {
        n_mod = 3;
        n_slots = 240;
    } else if (modcod >= 18 && modcod <= 23) {
        n_mod = 4;
        n_slots = 180;
    } else if (modcod >= 24 && modcod <= 28) {
        n_mod = 5;
        n_slots = 144;
    } else {
        n_mod = 0;
        n_slots = 36; // dummy frame
    }

    /* For short FECFRAMEs, S is 4 times lower */
    if (short_fecframe && !dummy_frame)
        n_slots >>= 2;

    /* Number of pilot blocks */
    n_pilots = (has_pilots) ? ((n_slots - 1) >> 4) : 0;

    /* PLFRAME length (header + data + pilots) */
    plframe_len = ((n_slots + 1) * 90) + (36 * n_pilots);

    /* Payload length (data + pilots) */
    payload_len = plframe_len - 90;

    /* XFECFRAME length */
    xfecframe_len = n_slots * 90;
}

void pls_info_t::parse(uint8_t _modcod, bool _short_fecframe, bool _has_pilots)
{
    uint8_t _plsc = ((_modcod & 0x1F) << 2) | (_short_fecframe << 1) | _has_pilots;
    parse(_plsc);
}

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
    : pl_submodule("plsc_decoder", debug_level),
      d_reed_muller_decoder(&map_plsc_codeword_to_bpsk),
      d_soft_dec_buf(PLSC_LEN)
{
}

plsc_decoder::plsc_decoder(std::vector<uint8_t>&& expected_pls, int debug_level)
    : pl_submodule("plsc_decoder", debug_level),
      d_reed_muller_decoder(std::move(expected_pls), &map_plsc_codeword_to_bpsk),
      d_soft_dec_buf(PLSC_LEN)
{
}

void plsc_decoder::decode(const gr_complex* bpsk_in, bool coherent, bool soft)
{
    if (soft && coherent) {
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
        d_plsc = d_reed_muller_decoder.decode(d_soft_dec_buf.data());
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

        GR_LOG_DEBUG_LEVEL(3, "Descrambled PLSC: 0x{:016X}\n", rx_plsc);

        /* Decode the descrambled hard decisions */
        d_plsc = d_reed_muller_decoder.decode(rx_plsc);
    }

    // Parse the PLSC
    d_pls_info.parse(d_plsc);

    GR_LOG_DEBUG_LEVEL(1,
                       "MODCOD: {:2d}; Short FECFRAME: {:1d}; Pilots: {:1d}",
                       static_cast<unsigned>(d_pls_info.modcod),
                       d_pls_info.short_fecframe,
                       d_pls_info.has_pilots);
    GR_LOG_DEBUG_LEVEL(2,
                       "n_mod: {:1d}; S: {:3d}; PLFRAME length: {:d}",
                       static_cast<unsigned>(d_pls_info.n_mod),
                       d_pls_info.n_slots,
                       d_pls_info.plframe_len);
}

} // namespace dvbs2rx
} // namespace gr
