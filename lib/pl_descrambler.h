/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_PL_DESCRAMBLER_H
#define INCLUDED_DVBS2RX_PL_DESCRAMBLER_H

#include "pl_defs.h"
#include <gnuradio/dvbs2rx/api.h>
#include <gnuradio/gr_complex.h>
#include <volk/volk_alloc.hh>

namespace gr {
namespace dvbs2rx {

/**
 * \brief PL Descrambler
 *
 * Multiplies the scrambled payload of a PLFRAME by the conjugate of the complex
 * randomization sequence used on the Tx side for PL scrambling. This multiplication
 * effectively undoes the scrambling, and the resulting descrambled payload is stored
 * internally for later access through the `get_payload()` method. This processes depends
 * only on the Gold code defining the complex scrambling sequence, which must be provided
 * to the constructor.
 */
class DVBS2RX_API pl_descrambler
{
private:
    const int d_gold_code;                       /**< Gold code (scrambling code) */
    volk::vector<gr_complex> d_descrambling_seq; /**< Complex descrambling sequence */
    volk::vector<gr_complex> d_payload_buf;      /**< Descrambled payload buffer */
    int parity_chk(long a, long b) const;

    /**
     * \brief Pre-compute the complex descrambling sequence
     */
    void compute_descrambling_sequence();

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
     * \param in (const gr_complex*) Pointer to the target scrambled PLFRAME payload
     *                               buffer.
     * \param payload_len (uint16_t) Payload length.
     * \note The payload length must be equal to the PLFRAME length minus 90
     * (the PLHEADER length). This means that pilots are part of the payload,
     * since the pilot symbols must be descrambled.
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
