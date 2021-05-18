/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_REED_MULLER_H
#define INCLUDED_DVBS2RX_REED_MULLER_H

#include "pl_defs.h"
#include <dvbs2rx/api.h>

namespace gr {
namespace dvbs2rx {

/**
 * @brief Interleaved (64, 7, 32) Reed-Muller encoder/decoder.
 *
 * This class implements DVB-S2's Reed-Muller (RM) code used by the physical
 * layer signaling (PLS) encoding.
 */
class DVBS2RX_API reed_muller
{
private:
    uint64_t d_codeword_lut[n_plsc_codewords]; // LUT with all 64-bit codewords

public:
    reed_muller();

    /**
     * @brief Encode a given dataword (PLSC) into the corresponding codeword.
     * @param in_dataword 7-bit PLSC dataword.
     * @return 64-bit interleaved (64, 7, 32) RM codeword.
     */
    uint64_t encode(uint8_t in_dataword);

    /**
     * @brief Decode a received codeword into the corresponding dataword (PLSC).
     * @param in_codeword Received noisy 64-bit RM codeword to be decoded.
     * @return Decoded 7-bit dataword (PLSC).
     */
    uint8_t decode(uint64_t in_codeword);
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_REED_MULLER_H */
