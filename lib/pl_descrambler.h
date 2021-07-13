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
#include <volk/volk_alloc.hh>

namespace gr {
namespace dvbs2rx {

/**
 * \brief PL Descrambler
 *
 * Multiplies the PLFRAME IQ samples (excluding the PLHEADER) by a complex
 * randomization sequence defined by a chosen Gold code.
 *
 * The complex randomization sequence is implemented as a complex symbol mapping
 * table, as specified in Section 5.5.4 of the standard. The table has four
 * mapping possibilities. The rule that applies to the n-th symbol in the
 * payload depends on the value of the integer-valued variable "Rn". The Rn
 * value, in turn, is pre-computed by this class's constructor for all possible
 * payload indexes, starting from n=0 (first payload symbol following the
 * PLHEADER) up to the maximum PLFRAME payload length.
 */
class pl_descrambler
{
private:
    const int d_gold_code;                  /**< Gold code (scrambling code) */
    int d_Rn[MAX_PLFRAME_PAYLOAD];          /**< Pre-computed Rn sequence */
    volk::vector<gr_complex> d_payload_buf; /**< Descrambled payload buffer */
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
     * \brief Descramble a PLFRAME payload.
     *
     * Descrambles a given PLFRAME payload and stores the descrambled result on
     * the internal descrambled payload buffer, which can be accessed through
     * method `get_payload()`.
     *
     * \param in (const gr_complex*) Pointer to the target PLFRAME payload
     *                               buffer.
     * \param payload_len (uint16_t) Payload length.
     * \note The payload length must be equal to the PLFRAME length minus 90
     * (the PLHEADER length). This means that pilots are part of the payload,
     * since the pilot symbols must be descrambled.
     * \return Void.
     */
    void descramble(const gr_complex* in, uint16_t payload_len);

    /**
     * \brief Get the descrambled payload.
     * \return Pointer to the descrambled payload buffer.
     */
    const gr_complex* get_payload() { return d_payload_buf.data(); }
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_PL_DESCRAMBLER_H */
