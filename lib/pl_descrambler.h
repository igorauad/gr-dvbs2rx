/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_PL_DESCRAMBLER_H
#define INCLUDED_DVBS2RX_PL_DESCRAMBLER_H

#include "pl_defs.h"
#include <gnuradio/gr_complex.h>

namespace gr {
namespace dvbs2rx {

/**
 * \brief PL Descrambler
 *
 * Multiplies the PLFRAME IQ samples (excluding the PLHEADER) by a complex
 * randomization sequence defined by a chosen Gold code.
 */
class pl_descrambler
{
private:
    const int d_gold_code;         /** Gold code (scrambling code) */
    int d_Rn[MAX_PLFRAME_PAYLOAD]; /** Pre-computed int-valued sequence that
                                    * defines the scrambling symbol mapping
                                    * table (see Section 5.5.4 of the
                                    * standard) */
    int parity_chk(long a, long b) const;

    /**
     * \brief Pre-compute the scrambler table
     * \return Void
     */
    void build_symbol_scrambler_table();

public:
    pl_descrambler(int gold_code);
    ~pl_descrambler(){};

    /**
     * \brief De-scramble input symbol
     * \param in (const gr_complex&) Input symbol
     * \param out (gr_complex&) Output (descrambled) symbol
     * \param i (int) Symbol index
     *
     * \note Symbol index is relative to the start of the "payload", i.e., 0
     * refers to the first symbol past the PLHEADER.
     */
    void step(const gr_complex& in, gr_complex& out, int i) const;
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_PL_DESCRAMBLER_H */
