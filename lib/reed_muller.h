/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_REED_MULLER_H
#define INCLUDED_DVBS2RX_REED_MULLER_H

#include "pl_defs.h"
#include <gnuradio/dvbs2rx/api.h>
#include <volk/volk_alloc.hh>

namespace gr {
namespace dvbs2rx {

// Pointer to an Euclidean-space mapping function:
typedef void (*euclidean_map_func_ptr)(float* dptr, uint64_t codeword);

/**
 * @brief Interleaved (64, 7, 32) Reed-Muller encoder/decoder.
 *
 * This class implements DVB-S2's Reed-Muller (RM) code used by the physical
 * layer signaling (PLS) encoding.
 */
class DVBS2RX_API reed_muller
{
private:
    // Vector with all possible codeword indexes. When the constructor does not specify
    // the enabled codewords, this vector holds the sequence from 0 to 127. Otherwise,
    // when it's known a priori that only a subset of the codewords can be present in the
    // incoming signal, this vector can be reduced to a subset of the codewords.
    std::vector<uint8_t> d_enabled_codewords;
    // LUT with all the 64-bit codewords:
    uint64_t d_codeword_lut[n_plsc_codewords];
    // LUT with the Euclidean-space image of the codewords (real vectors):
    volk::vector<float> d_euclidean_img_lut;
    // Buffer used by the maximum inner product soft decoder:
    volk::vector<float> d_dot_prod_buf;

    /**
     * @brief Initialize the codeword and Euclidean-space image LUTs
     *
     * @param p_custom_map Custom Euclidean space mapping function defined on the
     * constructor.
     */
    void init(euclidean_map_func_ptr p_custom_map = nullptr);

public:
    // Function used to map binary codewords into the corresponding real vector:
    euclidean_map_func_ptr euclidean_map;

    /**
     * @brief Construct the Reed-Muller encoder/decoder.
     * @param p_custom_map Pointer to a custom mapping function used to map the
     *        binary the codewords into real-valued Euclidean-space images. If
     *        not defined, method "default_euclidean_map" is used.
     */
    explicit reed_muller(euclidean_map_func_ptr p_custom_map = nullptr);

    /**
     * @brief Construct the Reed-Muller encoder/decoder for a codeword subset.
     * @param enabled_codewords Temporary vector with the indexes within [0, 128)
     * corresponding to subset of codewords that may appear (according to a priori
     * knowledge) on the incoming signal.
     * @param p_custom_map Pointer to a custom mapping function used to map the
     *        binary the codewords into real-valued Euclidean-space images. If
     *        not defined, method "default_euclidean_map" is used.
     */
    explicit reed_muller(std::vector<uint8_t>&& enabled_codewords,
                         euclidean_map_func_ptr p_custom_map = nullptr);

    /**
     * @brief Map codeword to a real vector using 2-PAM.
     * @param dptr Destination pointer for the 64 real 2-PAM mapped symbols.
     * @param codeword 64-bit (64, 7, 32) Reed-Muller codeword to be mapped.
     * @note This is the default Euclidean-space mapping if another custom
     *       mapping is not provided through the constructor.
     */
    static void default_euclidean_map(float* dptr, uint64_t codeword);

    /**
     * @brief Encode a given dataword (PLSC) into the corresponding codeword.
     * @param in_dataword 7-bit PLSC dataword.
     * @return 64-bit interleaved (64, 7, 32) RM codeword.
     */
    uint64_t encode(uint8_t in_dataword);

    /**
     * @brief Decode a binary hard decision into the corresponding dataword.
     * @param hard_dec Received 64-bit hard decision to be decoded.
     * @return Decoded 7-bit dataword.
     */
    uint8_t decode(uint64_t hard_dec);

    /**
     * @brief Decode a real soft decision vector into the corresponding dataword.
     * @param soft_dec Received 64-element soft decision real vector to be decoded.
     * @return Decoded 7-bit dataword.
     */
    uint8_t decode(const float* soft_dec);
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_REED_MULLER_H */
