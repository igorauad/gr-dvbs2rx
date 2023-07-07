/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_PI2_BPSK_H
#define INCLUDED_DVBS2RX_PI2_BPSK_H

#include <gnuradio/dvbs2rx/api.h>
#include <gnuradio/gr_complex.h>
#include <cstdint>

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
 * the input pi/2 BPSK symbols are noisy, the resulting real-valued BPSK symbols
 * are naturally noisy too and deviate from the nominal +-1 values.
 *
 * This derotation effectively produces the "soft decisions" corresponding to
 * the received pi/2 BPSK symbols. Taking an even index for the demonstration,
 * note the constellation symbols are either exp(jpi/4) or exp(-j3pi/4). Hence,
 * the log-likelihood ratio between the received complex symbol r representing
 * bit=0 versus bit=1 is as follows:
 *
 * LLR(r) =  (-||r - exp(jpi/4)||^2 + ||r - exp(-j3pi/4)||^2) / N0
 *
 * Now, let's say r can be expressed as " exp(jpi/4) * r' ", where r' is the
 * derotated versions of r, namely " r' = r*exp(-jpi/4) ". In this case, it
 * follows that:
 *
 * LLR(r) =  (-||exp(jpi/4)*(r' - 1)||^2 + ||exp(jpi/4)(r' + 1)||^2) / N0,
 *
 * given that exp(jpi/4) = -exp(-j3pi/4).
 *
 * Since ||exp(jpi/4)||^2 = 1, it can be factored out of the Euclidean norm
 * terms, which yields:
 *
 * LLR(r) =  (-||r' - 1||^2 + ||r' + 1||^2) / N0.
 *
 * Besides, note that:
 *
 * ||r' - 1||^2 = ||r'||^2 -2*real(<r', 1>) + ||1||^2
 * ||r' + 1||^2 = ||r'||^2 +2*real(<r', 1>) + ||1||^2
 *
 * Hence, the LLR can be expressed as:
 *
 * LLR(r) = 4 * real(<r', 1>) / N0,
 *        = 4 * real(r') / N0.
 *
 * In other words, the LLR is given by the real part of the de-rotated symbol
 * r', scaled by 4/N0. Furthermore, the scaling factor 4/N0 is only useful for a
 * maximum a posteriori (MAP) decoder, when the constellation symbols are not
 * equiprobable. In contrast, for maximum likelihood (ML) decoding, when the
 * symbols are equiprobable, the scaling factor can be ignored, as the threshold
 * for decision is zero (i.e., bit=0 when LLR(r) > 0 and bit=1 otherwise). Thus,
 * ultimately, the "soft decisions" are given by real(r'), which is equal to:
 *
 * - real(r * exp(-jpi/4)), on even indexes;
 * - real(r * exp(-j3pi/4)), on odd indexes.
 *
 * Note the term soft decision is loosely taken here as a sufficient statistic
 * indicating the likelihood of a particular bit, which can be used by a
 * maximum-likelihood decoder. The particular bit-by-bit soft decision that this
 * function produces is one proportional to the LLR, but not exactly equal to
 * the LLR (as it does not need to be). Furthermore, using the terminology from
 * textbooks such as Forney's (Section 6.5.2), the returned value real(r') can
 * be used to derive both the hard decision "sign(real(r'))" and the reliability
 * weight associated with this decision given by "abs(real(r'))".
 *
 * In the end, what matters the most is the soft decision format expected by the
 * coding scheme's decoder. Here, we assume the decoder expects real(r'), as
 * this function is meant to feed soft decisions into the Reed-Muller decoder
 * (see the implementation of reed_muller::decode on reed_muller.cc). Hence,
 * this function first derotates each complex input symbol r[k] to produce
 * r'[k]. Then, it returns real(r'[k]) for all k on the output buffer.
 *
 * @param in (const gr_complex *) Input complex pi/2 BPSK symbols.
 * @param out (float *) Output real-valued BPSK symbols.
 * @param N (unsigned int) Number of pi/2 BPSK symbols to derotate.
 */
DVBS2RX_API void derotate_bpsk(const gr_complex* in, float* out, unsigned int N);

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_PI2_BPSK_H */
