/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pl_descrambler.h"

namespace gr {
namespace dvbs2rx {

pl_descrambler::pl_descrambler(int gold_code) : d_gold_code(gold_code)
{
    build_symbol_scrambler_table();
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

void pl_descrambler::build_symbol_scrambler_table()
{
    /* From gr-dtv's dvbs2_physical_cc_impl.cc */
    long x, y;
    int xa, xb, xc, ya, yb, yc;
    int zna, znb;

    // Initialisation
    x = 0x00001;
    y = 0x3FFFF;

    for (int n = 0; n < d_gold_code; n++) {
        xb = parity_chk(x, 0x0081);

        x >>= 1;
        if (xb) {
            x |= 0x20000;
        }
    }

    for (int i = 0; i < MAX_PLFRAME_PAYLOAD; i++) {
        xa = parity_chk(x, 0x8050);
        xb = parity_chk(x, 0x0081);
        xc = x & 1;

        x >>= 1;
        if (xb) {
            x |= 0x20000;
        }

        ya = parity_chk(y, 0x04A1);
        yb = parity_chk(y, 0xFF60);
        yc = y & 1;

        y >>= 1;
        if (ya) {
            y |= 0x20000;
        }

        zna = xc ^ yc;
        znb = xa ^ yb;
        d_Rn[i] = (znb << 1) + zna;
    }
}

void pl_descrambler::step(const gr_complex& in, gr_complex& out, int i) const
{
    // Undo the mapping given in Section 5.5.4 of the standard
    switch (d_Rn[i]) {
    case 0:
        out = in;
        break;
    case 1:
        out = gr_complex(in.imag(), -in.real());
        break;
    case 2:
        out = -in;
        break;
    case 3:
        out = gr_complex(-in.imag(), in.real());
        break;
    }
}

} // namespace dvbs2rx
} // namespace gr
