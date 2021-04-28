/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pi2_bpsk.h"
#include "pl_defs.h"
#include <gnuradio/expj.h>
#include <gnuradio/math.h>

namespace gr {
namespace dvbs2rx {

void map_bpsk(uint64_t code, gr_complex* out, unsigned int N)
{
    /* The standard pi/2 BPSK mapping, with even/odd mappings swapped to comply
     * with the C-style index starting from 0 instead of starting from 1 (as
     * assumed in the standard). */
    constexpr gr_complex pi2_bpsk_map[2][2] = {
        // even index here (odd index mapping from the standard)
        {
            (SQRT2_2 + SQRT2_2i), // bit 0
            (-SQRT2_2 - SQRT2_2i) // bit 1
        },
        // odd index here (even index mapping from the standard)
        {
            (-SQRT2_2 + SQRT2_2i), // bit 0
            (SQRT2_2 - SQRT2_2i)   // bit 1
        }
    };

    if (N > 64) {
        throw std::runtime_error("N has to be <= 64");
    }

    for (unsigned int j = 0; j < N; j++) { // for each input code bit
        out[j] = pi2_bpsk_map[j & 1][code >> (63 - j) & 1];
    }
}

uint64_t demap_bpsk(const gr_complex* in, unsigned int N)
{
    /*
     * If we rotate the standard symbol mapping by -pi/4 on even indexes and
     * -3pi/4 on odd indexes, the mapping becomes:
     *
     * +1 if bit = 0
     * -1 if bit = 1
     *
     * Thus, the decision rule simplifies to checking the real part of the
     * rotated pi/2 BPSK symbol.
     */
    constexpr gr_complex rot[2] = {
        (SQRT2_2 - SQRT2_2i), // rotation factor for even indexes
        (-SQRT2_2 - SQRT2_2i) // rotation factor for odd indexes
    };

    if (N > 64) {
        throw std::runtime_error("N has to be <= 64");
    }

    uint64_t code = 0;
    for (unsigned int j = 0; j < N; j++) {
        gr_complex rot_sym = in[j] * rot[j & 1];
        uint64_t bit = (rot_sym.real() < 0);
        code |= (bit << (63 - j));
    }

    return code;
}

uint64_t demap_bpsk_diff(const gr_complex* in, unsigned int N)
{
    /*
     * The i-th differential pi/2 BPSK demapping is based on the product
     * "conj(in[i]) * in[i-1]", namely the phase rotation from symbol i-1 to
     * symbol i. The result depends on the evenness/oddness of the index
     * transition. In the sequel, we assume the local indexing convention
     * (starting from 0), as opposed to the convention adopted by the DVB-S2
     * standard (starting from 1).
     *
     * Due to the nature of the pi/2 BPSK mapping, the phase change between
     * consecutive symbols is always +-pi/2. Correspondingly, the product
     * "conj(in[i]) * in[i-1]" always results in either +j or -j. The key for
     * differentially demapping is to formulate a table of possible results for
     * even-to-odd and odd-to-even index transitions and demap the i-th symbol
     * based on that.
     *
     * To do so, it is instructive to express the mapping on "pi2_bpsk_map" (see
     * the "map_bpsk" function) in complex exponential notation:
     *
     *     Local even index (odd index on the standard's convention):
     *
     *     {
     *         expj(pi/4),  // bit 0
     *         expj(-3pi/4) // bit 1
     *     }
     *
     *     Local odd index (even index on the standard's convention):
     *
     *     {
     *         expj(3pi/4), // bit 0
     *         expj(-pi/4)  // bit 1
     *     }
     *
     * Next, we investigate the possible conjugate products for pairs of bits
     * representing symbol i-1 and symbol i, in that order. On an even-to-odd
     * transition (e.g., from i-1=0 to i=1), the results are as follows:
     *
     *     00 -> expj(pi/4) * conj(expj(3pi/4))   = expj(-pi/2)  = -j
     *     01 -> expj(pi/4) * conj(expj(-pi/4))   = expj(pi/2)   = +j
     *     10 -> expj(-3pi/4) * conj(expj(3pi/4)) = expj(-3pi/2) = +j
     *     11 -> expj(-3pi/4) * conj(expj(-pi/4)) = expj(-pi/2)  = -j
     *
     * Similarly, on an odd-to-even transition (e.g., from i-1=1 to i=2), the
     * results are as follows:
     *
     *     00 -> expj(3pi/4) * conj(expj(pi/4))   = expj(pi/2)  = +j
     *     01 -> expj(3pi/4) * conj(expj(-3pi/4)) = expj(3pi/2) = -j
     *     10 -> expj(-pi/4) * conj(expj(pi/4))   = expj(-pi/2) = -j
     *     11 -> expj(-pi/4) * conj(expj(-3pi/4)) = expj(pi/2)  = +j
     *
     * With that, we can formulate the decision rule. We always need to check:
     * 1) the current index's evenness/oddness; 2) the previous bit; 3) whether
     * the current differential is +j or -j.
     *
     * On an even-to-odd transition, we have the following rules:
     *
     *     - If imag(conj(in[i]) * in[i-1]) > 0, bit[i] = !bit[i-1].
     *     - If imag(conj(in[i]) * in[i-1]) < 0, bit[i] = bit[i-1].
     *
     * In contrast, on an odd-to-even transition, we have the opposite:
     *
     *     - If imag(conj(in[i]) * in[i-1]) > 0, bit[i] = bit[i-1].
     *     - If imag(conj(in[i]) * in[i-1]) < 0, bit[i] = !bit[i-1].
     *
     * Ultimately, the decision process can be implemented as follows:
     *
     *    - Start with bit[i] = bit[i-1];
     *    - Flip the bit if imag(conj(in[i]) * in[i-1]) < 0.
     *    - Flip the bit if i is odd (i.e., on an even-to-odd transition).
     *
     * Importantly, note that we can only decode the exact bits starting from
     * the pointer *in when we know the symbol and bit preceding this
     * symbol. This works when decoding the PLSC, since we know the last bit of
     * the SOF, which is 0. Besides, note the last SOF bit is at index
     * 25. Hence, the first transition is an odd-to-even transition when
     * decoding the PLSC, from bit 25 (last SOF bit) to bit 26 (first PLSC bit).
     */
    if (N > 64) {
        throw std::runtime_error("N has to be <= 64");
    }

    gr_complex diff;
    uint64_t bit = 0; // last SOF bit is 0
    uint64_t code = 0;

    // The first index is at the SOF-to-PLSC transition, an odd-to-even
    // transition whose previous bit and symbol are known to be bit[i-1]=0 and
    // in[i=1]=(-SQRT2_2 + SQRT2_2i), respectively:
    unsigned int j = 0;
    diff = conj(in[j]) * gr_complex(-SQRT2_2 + SQRT2_2i);
    bit = diff.imag() < 0;
    code |= (bit << 63);

    // Subsequent indexes follow the general decision rule stated above:
    for (j = 1; j < N; j++) {
        diff = conj(in[j]) * in[j - 1];
        bit = bit ^ (diff.imag() < 0) ^ (j & 1);
        code |= (bit << (63 - j));
    }

    return code;
}


} // namespace dvbs2rx
} // namespace gr
