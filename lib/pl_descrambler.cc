/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pl_descrambler.h"

namespace gr {
namespace dvbs2rx {

pl_descrambler::pl_descrambler(int gold_code)
    : d_gold_code(gold_code),
      d_descrambling_seq(MAX_PLFRAME_PAYLOAD),
      d_payload_buf(MAX_PLFRAME_PAYLOAD)
{
    compute_descrambling_sequence();
}

int pl_descrambler::parity_chk(long a, long b) const
{
    /* From gr-dtv's dvbs2_physical_cc_impl.cc */
    int c = 0;
    a = a & b;
    for (int i = 0; i < 18; i++) {
        if (a & (1L << i)) {
            c++;
        }
    }
    return c & 1;
}

void pl_descrambler::compute_descrambling_sequence()
{
    // The goal of the complex descrambling sequence is to undo the randomization
    // described in Section 5.5.4 of the standard. The original scrambling sequence
    // depends only on the Gold code. Hence, the descrambling sequence also follows the
    // same property. This means that, given the Gold code remains constant throughout the
    // existence of this object, we can compute the descrambling sequence in advance.
    //
    // The i-th value of the scrambling sequence applies to the i-th payload symbol,
    // counting from the first symbol after the PLHEADER. This i-th scrambling value is
    // given by "exp(j*Rn[i]*π/2)", which depends on Rn(i), a number within [0,3]. Hence,
    // the original scrambling is obtained by multiplying each payload symbol by one of
    // the four possibilities below:
    //
    //   - exp(j*0) = 1
    //   - exp(j*π/2) = j1
    //   - exp(j*π) = -1
    //   - exp(j*3*π/2) = -j1
    //
    // The descrambling is achieved by multiplying the input symbols by the complex
    // conjugate of the scrambling factors, which take the following possible values:
    constexpr gr_complex descrambling_lut[4] = {
        { 1.0, 0 }, { 0, -1.0 }, { -1.0, 0 }, { 0, 1.0 }
    };

    // In the sequel, compute Rn[i] over MAX_PLFRAME_PAYLOAD. Reuse the implementation
    // from gr-dtv's dvbs2_physical_cc_impl.cc.
    long x = 0x00001;
    long y = 0x3FFFF;

    for (int n = 0; n < d_gold_code; n++) {
        int xb = parity_chk(x, 0x0081);

        x >>= 1;
        if (xb) {
            x |= 0x20000;
        }
    }

    for (int i = 0; i < MAX_PLFRAME_PAYLOAD; i++) {
        int xa = parity_chk(x, 0x8050);
        int xb = parity_chk(x, 0x0081);
        int xc = x & 1;

        x >>= 1;
        if (xb) {
            x |= 0x20000;
        }

        int ya = parity_chk(y, 0x04A1);
        int yb = parity_chk(y, 0xFF60);
        int yc = y & 1;

        y >>= 1;
        if (ya) {
            y |= 0x20000;
        }

        int zna = xc ^ yc;
        int znb = xa ^ yb;
        int Rn = (znb << 1) + zna;
        d_descrambling_seq[i] = descrambling_lut[Rn];
    }
}

void pl_descrambler::descramble(const gr_complex* in, uint16_t payload_len)
{
    volk_32fc_x2_multiply_32fc(
        d_payload_buf.data(), in, d_descrambling_seq.data(), payload_len);
}

} // namespace dvbs2rx
} // namespace gr
