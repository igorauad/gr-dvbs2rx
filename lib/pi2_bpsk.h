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
 * The differential (non-coherent) demapping is an interesting alternative in
 * the presence of significant frequency offset. However, it is designed to work
 * with the PLSC symbols and may not work with an arbitrary pi/2 BPSK
 * sequence. It assumes the first pi/2 BPSK symbol pointed by argument *in lies
 * at an odd index and corresponds to the last SOF symbol. Correspondingly, it
 * assumes the second symbol in the input array represents an even PLHEADER
 * index and encodes the first PLSC symbol. Do not use this function to demap
 * any arbitrary pi/2 BPSK sequence. For that, use "demap_bpsk" (i.e., the
 * coherent demapping) instead.
 *
 * @param in (const gr_complex *) Pointer to the array of pi/2 BPSK symbols,
 *        starting from the last SOF symbol and followed by the PLSC symbols.
 * @param N (unsigned int) Number of pi/2 BPSK symbols to demap.
 *
 * @return Demapped bits corresponding to symbols in[1] to in[N] (i.e., the N
 *         symbols past the first input symbol in[0]). The result is packed on a
 *         bit-level big-endian uint64_t.
 *
 * @note N must be <= 64, and the complex input array must hold at least N+1
 * pi/2 BPSK symbols, starting from an odd-indexed symbol.
 */
DVBS2RX_API uint64_t demap_bpsk_diff(const gr_complex* in, unsigned int N);


/**
 * @brief Derotate N complex-valued pi/2 BPSK into regular real BPSK symbols
 *
 * Converts a sequence of complex-valued pi/2 BPSK symbols with values
 * originating from the +-0.707 +-j*0.707 constellation points into the
 * corresponding sequence of ordinary real-valued BPSK symbols around +-1. If
 * the input pi/2 BPSK symbols are noisy, naturally the resulting real-valued
 * BPSK symbols are noisy too and deviate from the nominal +-1 values.
 *
 * @param in (const gr_complex *) Input complex pi/2 BPSK symbols.
 * @param out (float *) Output real-valued BPSK symbols.
 * @param N (unsigned int) Number of pi/2 BPSK symbols to derotate.
 * @return Void.
 */
DVBS2RX_API void derotate_bpsk(const gr_complex* in, float* out, unsigned int N);

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_PI2_BPSK_H */
