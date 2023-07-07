/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_PLSC_H
#define INCLUDED_DVBS2RX_PLSC_H

#include "pl_submodule.h"
#include "reed_muller.h"
#include <gnuradio/dvbs2rx/api.h>
#include <gnuradio/gr_complex.h>
#include <volk/volk_alloc.hh>
#include <cstring>

namespace gr {
namespace dvbs2rx {

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

struct DVBS2RX_API pls_info_t {
    uint8_t plsc = 0;            /**< Raw PLSC value */
    uint8_t modcod = 0;          /**< MODCOD of the decoded PLSC */
    bool short_fecframe = false; /**< Whether the FECFRAME size is short */
    bool has_pilots = false;     /**< Whether the PLFRAME has pilot blocks */
    bool dummy_frame = false;    /**< Whether the PLFRAME is a dummy frame */
    uint8_t n_mod = 0;           /**< Bits per constellation symbol */
    uint16_t n_slots = 0;        /**< Number of slots */
    uint16_t plframe_len = 0;    /**< PLFRAME length */
    uint16_t payload_len = 0;    /**< Payload length */
    uint16_t xfecframe_len = 0;  /**< XFECFRAME length */
    uint8_t n_pilots = 0;        /**< Number of pilot blocks */
    pls_info_t() = default;
    pls_info_t(uint8_t dec_plsc) { parse(dec_plsc); }
    void parse(uint8_t dec_plsc);
    void parse(uint8_t _modcod, bool _short_fecframe, bool _has_pilots);
    pls_info_t& operator=(const pls_info_t& other)
    {
        if (other.plsc != plsc) {
            memcpy(this, &other, sizeof(pls_info_t));
        }
        return *this;
    }
};

/**
 * \brief PLSC Decoder
 *
 * Decodes a sequence of 64 noisy pi/2 BPSK symbols into the corresponding 7-bit
 * PLS code. Implements the pi/2 BPSK demapping, the PLSC descrambling, and the
 * parsing of the PLSC information.
 */
class DVBS2RX_API plsc_decoder : public pl_submodule
{
private:
    reed_muller d_reed_muller_decoder;  /**< Reed-Muller decoder */
    volk::vector<float> d_soft_dec_buf; /**< Soft decisions buffer */
    pls_info_t d_pls_info;              /**< PL signaling information */

public:
    uint8_t d_plsc; /**< Last decoded PLSC */

    explicit plsc_decoder(int debug_level = 0);
    explicit plsc_decoder(std::vector<uint8_t>&& expected_pls, int debug_level = 0);
    ~plsc_decoder(){};

    /**
     * \brief Decode the incoming pi/2 BPSK symbols of the PLSC.
     * \param bpsk_in (const gr_complex *) Input pi/2 BPSK symbols, starting
     *                from the last SOF symbol and followed by the PLSC symbols
     *                (see note 1).
     * \param coherent (bool) Whether to use coherent BPSK demapping (the
     *                 default). When set to false, the decoding uses hard
     *                 decisions produced through differential demapping of the
     *                 pi/2 BPSK symbols, even if soft=true (see note 2).
     * \param soft (bool) Whether to decode the PLSC dataword using soft pi/2
     *             BPSK decisions instead of hard decisions.
     *
     * \note 1 - The last SOF symbol is required when coherent=false. In
     * contrast, when coherent=true, the implementation simply skips this
     * symbol. However, note "bpsk_in" must start at the last SOF symbol
     * regardless.
     *
     * \note 2 - The non-coherent (differential) demapping is only supported
     * with hard decisions because there is negligible performance difference
     * when differential demapping is used to produce soft decisions. On the
     * contrary, based on some experiments, it seems that differential demapping
     * with soft decisions would only be slower, and it would produce a similar
     * (if not worse) performance than differential demapping with hard
     * decisions. Ultimately, the supported parameter combinations are:
     * (coherent=true, soft=false), (coherent=true, soft=true), and
     * (coherent=false, soft=false). With (coherent=false, soft=true), the
     * implementation will silently fall back to differential demapping with
     * hard decisions (coherent=false, soft=false).
     */
    void decode(const gr_complex* bpsk_in, bool coherent = true, bool soft = true);

    /**
     * Read the last decoded PLS information
     *
     * \param out (pls_info_t*) Pointer to a pls_info_t structure where the
     * last decoded information will be written.
     */
    void get_info(pls_info_t* out) const { *out = d_pls_info; };
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_PLSC_H */
