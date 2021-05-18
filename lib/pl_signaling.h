/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_PLSC_H
#define INCLUDED_DVBS2RX_PLSC_H

#include "reed_muller.h"
#include <gnuradio/gr_complex.h>
#include <dvbs2rx/api.h>

namespace gr {
namespace dvbs2rx {

// PLSC scrambler sequence (see Section 5.5.2.4 of the standard)
const uint64_t plsc_scrambler = 0x719d83c953422dfa;

/**
 * \brief PLSC Encoder
 *
 * Encodes a 7-bit physical layer signalling (PLS) code into a sequence of 64
 * pi/2 BPSK symbols. Implements the PLSC scrambling and pi/2 BPSK mapping.
 */
class DVBS2RX_API plsc_encoder
{
private:
    reed_muller d_reed_muller_encoder;

public:
    /**
     * \brief Encode the PLSC info into the corresponding pi/2 BPSK symbols.
     * \param bpsk_out (gr_complex *) Pointer to output pi/2 BPSK symbols.
     * \param plsc 7-bit PLSC to be mapped into pi/2 BPSK symbols.
     */
    void encode(gr_complex* bpsk_out, const uint8_t plsc);

    /**
     * \brief Encode the PLSC info into the corresponding pi/2 BPSK symbols.
     * \param bpsk_out (gr_complex *) Pointer to output pi/2 BPSK symbols.
     * \param modcod 5-bit modulation and coding scheme.
     * \param short_fecframe False for normal FECFRAME (64800 bits), true for
     *                       short FECFRAME (16200 bits).
     * \param has_pilots Whether the FECFRAME has pilots.
     */
    void encode(gr_complex* bpsk_out,
                const uint8_t modcod,
                bool short_fecframe,
                bool has_pilots);
};


/**
 * \brief PLSC Decoder
 *
 * Decodes a sequence of 64 noisy pi/2 BPSK symbols into the corresponding 7-bit
 * PLS code. Implements the pi/2 BPSK demapping, the PLSC descrambling, and the
 * parsing of the PLSC information.
 */
class DVBS2RX_API plsc_decoder
{
private:
    int d_debug_level; /** debug level */
    reed_muller d_reed_muller_decoder;

public:
    /* State - made public to speed up access */
    uint8_t dec_plsc;     /** Last decoded PLSC */
    uint8_t modcod;       /** MODCOD of the decoded PLSC */
    bool short_fecframe;  /** Wether FECFRAME size is short */
    bool has_pilots;      /** Wether PLFRAME has pilot blocks */
    bool dummy_frame;     /** Whether PLFRAME is a dummy frame */
    uint8_t n_mod;        /** bits per const symbol */
    uint16_t S;           /** number of slots */
    uint16_t plframe_len; /** PLFRAME length */
    uint8_t n_pilots;     /* number of pilot blocks */

    explicit plsc_decoder(int debug_level = 0);
    ~plsc_decoder(){};

    /**
     * \brief Decode the incoming pi/2 BPSK symbols of the PLSC.
     * \param bpsk_in (const gr_complex *) Input pi/2 BPSK symbols of the PLSC.
     * \param coherent (bool) Whether to use coherent BPSK demapping.
     * \return Void.
     */
    void decode(const gr_complex* bpsk_in, bool coherent = true);
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_PLSC_H */
