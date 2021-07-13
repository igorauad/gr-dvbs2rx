/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pl_descrambler.h"

namespace gr {
namespace dvbs2rx {

pl_descrambler::pl_descrambler(int gold_code)
    : d_gold_code(gold_code), d_payload_buf(MAX_PLFRAME_PAYLOAD)
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
    // Initialisation
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
        d_Rn[i] = (znb << 1) + zna;
    }
}

void pl_descrambler::descramble(const gr_complex* in, uint16_t payload_len)
{
    // Undo the mapping given in Section 5.5.4 of the standard
    for (int i = 0; i < payload_len; i++) {
        switch (d_Rn[i]) {
        case 0:
            d_payload_buf[i] = in[i]; // TODO remove (redudant)
            break;
        case 1:
            d_payload_buf[i] = gr_complex(in[i].imag(), -in[i].real());
            break;
        case 2:
            d_payload_buf[i] = -in[i];
            break;
        case 3:
            d_payload_buf[i] = gr_complex(-in[i].imag(), in[i].real());
            break;
        }
    }
}

} // namespace dvbs2rx
} // namespace gr
