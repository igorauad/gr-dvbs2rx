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
 * counter, and a linear interpolator. Based on this loop, outputs a sequence of
 * interpolated symbols recovered from the oversampled input sample stream. Currently
 * supports any integer oversampling ratio greater than or equal to two.
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
     */
    static sptr make(float sps, float loop_bw, float damping_factor, float rolloff);
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_SYMBOL_SYNC_CC_H */
