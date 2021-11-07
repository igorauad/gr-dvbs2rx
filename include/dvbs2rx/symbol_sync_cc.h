/* -*- c++ -*- */
/*
 * Copyright 2021 2019-2021 Igor Freire.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_SYMBOL_SYNC_CC_H
#define INCLUDED_DVBS2RX_SYMBOL_SYNC_CC_H

#include <gnuradio/block.h>
#include <dvbs2rx/api.h>

namespace gr {
namespace dvbs2rx {

/*!
 * \brief Symbol Synchronizer Loop
 * \ingroup dvbs2rx
 * \details
 *
 * Implements a symbol timing recovery loop composed of a non-data-aided Gardner timing
 * error detector (GTED), a proportional-plus-integral (PI) loop filter, a modulo-1
 * counter, and a polyphase root raised cosine (RRC) filter performing joint matched
 * filtering and symbol interpolation. The loop outputs a sequence of interpolated symbols
 * recovered from the oversampled input sample stream. It currently supports any integer
 * and even oversampling ratio greater than or equal to two.
 */
class DVBS2RX_API symbol_sync_cc : virtual public gr::block
{
public:
    typedef std::shared_ptr<symbol_sync_cc> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of dvbs2rx::symbol_sync_cc.
     *
     * To avoid accidental use of raw pointers, dvbs2rx::symbol_sync_cc's
     * constructor is in a private implementation
     * class. dvbs2rx::symbol_sync_cc::make is the public interface for
     * creating new instances.
     *
     * \param sps (float) Oversampling ratio.
     * \param loop_bw (float) Loop bandwidth.
     * \param damping_factor (float) Damping factor.
     * \param rolloff (float) Rolloff factor.
     * \param rrc_delay (int) Target root raised cosine (RRC) filter delay in symbol
     * periods.
     * \param n_subfilt (int) Number of subfilters in the polyphase realization of the RRC
     * filter used for joint interpolation and matched filtering.
     *
     * \note The number of subfilters `n_subfilt` does not impact the computational cost.
     * A single subfilter is used per strobe, so only the subfilter length matters for the
     * cost, not the number of subfilters. It is preferable to pick a sufficiently large
     * value for n_subfilt (such as 128) for better resolution on the output interpolants.
     */
    static sptr make(float sps,
                     float loop_bw,
                     float damping_factor,
                     float rolloff,
                     int rrc_delay,
                     int n_subfilt);
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_SYMBOL_SYNC_CC_H */
