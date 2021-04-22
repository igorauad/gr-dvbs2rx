/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_PLSC_DECODER_H
#define INCLUDED_DVBS2RX_PLSC_DECODER_H

#include "pl_defs.h"
#include <gnuradio/gr_complex.h>

namespace gr {
namespace dvbs2rx {

class plsc_decoder
{
private:
    /* Parameters */
    int debug_level; /** debug level */

    /* Constants */
    uint64_t codewords[n_plsc_codewords]; // all possible 64-bit codewords

    /**
     * \brief Coherent pi/2 BPSK demapping
     * \param in (const gr_complex *) Incoming PLHEADER BPSK symbols
     * \return (uin64_t) Demapped bits of the scrambled PLSC
     * \note Pointer *in must point to the start of the PLHEADER.
     */
    uint64_t demap_bpsk(const gr_complex* in);

    /**
     * \brief Differential pi/2 BPSK demapping
     * \param in (const gr_complex *) Incoming PLHEADER BPSK symbols
     * \return (uin64_t) Demapped bits of the scrambled PLSC
     * \note Pointer *in must point to the start of the PLHEADER.
     */
    uint64_t demap_bpsk_diff(const gr_complex* in);

public:
    /* State - made public to speed up access */
    uint8_t dec_plsc;     /** 7-bit decoded PLSC dataword */
    uint8_t modcod;       /** MODCOD of the decoded PLSC */
    bool short_fecframe;  /** Wether FECFRAME size is short */
    bool has_pilots;      /** Wether PLFRAME has pilot blocks */
    bool dummy_frame;     /** Whether PLFRAME is a dummy frame */
    uint8_t n_mod;        /** bits per const symbol */
    uint16_t S;           /** number of slots */
    uint16_t plframe_len; /** PLFRAME length */
    uint8_t n_pilots;     /* number of pilot blocks */

    plsc_decoder(int debug_level);
    ~plsc_decoder(){};

    /**
     * \brief Decode the incoming pi/2 BPSK symbols of the PLSC
     * \param in (const gr_complex *) Input pi/2 BPSK symbols
     * \param coherent (bool) Whether to use coherent BPSK demapping
     * \return Void.
     */
    void decode(const gr_complex* in, bool coherent);

    const uint64_t* get_codewords() { return codewords; }
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_PLSC_DECODER_H */
