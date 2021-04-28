/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_PI2_BPSK_H
#define INCLUDED_DVBS2RX_PI2_BPSK_H

#include <gnuradio/gr_complex.h>
#include <dvbs2rx/api.h>

namespace gr {
namespace dvbs2rx {

/**
 * @brief Map N bits from the PLHEADER into pi/2 BPSK symbols.
 */
DVBS2RX_API void map_bpsk(uint64_t code, gr_complex* out, unsigned int N);

/**
 * @brief Coherently demap N pi/2 BPSK symbols from the PLHEADER into bits.
 * @param in (const gr_complex *) Incoming pi/2 BPSK symbols.
 * @param N (unsigned int) Number of pi/2 BPSK symbols to demap.
 * @return Demapped bits packed on a bit-level big-endian uint64_t.
 * @note N must be <= 64.
 */
DVBS2RX_API uint64_t demap_bpsk(const gr_complex* in, unsigned int N);

/**
 * @brief Differentially demap N pi/2 BPSK symbols from the PSLC into bits.
 *
 * The differential (non-coherent) demapping is an attractive alternative in the
 * presence of significant frequency offset. However, it is limited to work only
 * with PLSC symbols. It assumes the first input symbol pointed by argument *in
 * corresponds to the first PLSC symbol, namely the symbol following the last
 * SOF symbol. Do not use this function if the goal is to demap any arbitrary
 * pi/2 BPSK sequence, such as the SOF sequence. For that, use "demap_bpsk"
 * (i.e., the coherent demapping) instead.
 *
 * @param in (const gr_complex *) Pointer to the array of pi/2 BPSK symbols 
          composing the PSLC, starting from the first PLSC symbol.
 * @param N (unsigned int) Number of pi/2 BPSK symbols to demap.
 * @return Demapped bits packed on a bit-level big-endian uint64_t.
 * @note N must be <= 64.
 */
DVBS2RX_API uint64_t demap_bpsk_diff(const gr_complex* in, unsigned int N);

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_PI2_BPSK_H */
