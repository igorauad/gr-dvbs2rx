/* -*- c++ -*- */
/*
 * Copyright 2021 2019-2021 Igor Freire.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_SYMBOL_SYNC_CC_H
#define INCLUDED_DVBS2RX_SYMBOL_SYNC_CC_H

#include <gnuradio/block.h>
#include <gnuradio/dvbs2rx/api.h>

namespace gr {
namespace dvbs2rx {

/*!
 * \brief Symbol Synchronizer Loop
 * \ingroup dvbs2rx
 * \details
 *
 * Implements symbol timing recovery using a feedback loop composed of a non-data-aided
 * Gardner timing error detector (GTED), a proportional-plus-integral (PI) loop filter, a
 * modulo-1 counter, and a configurable interpolator. The loop takes an oversampled sample
 * stream on its input and outputs a sequence of interpolated symbols.
 *
 * By default, the loop uses a polyphase interpolator, namely a polyphase root
 * raised-cosine (RRC) filter capable of joint matched filtering and symbol interpolation.
 * Thus, there is no need to precede this block with a dedicated matched filter. Instead,
 * the symbol synchronizer plays the role of both the matched filter and the synchronizer
 * itself. In contrast, when using any other interpolation scheme (linear, quadratic, or
 * cubic), this block must be preceded by a dedicated matched filter block.
 *
 * Note the current implementation only supports integer and even oversampling ratios
 * greater than or equal to two. Odd or fractional oversampling ratio are for future work.
 */
class DVBS2RX_API symbol_sync_cc : virtual public gr::block
{
public:
    typedef std::shared_ptr<symbol_sync_cc> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of dvbs2rx::symbol_sync_cc.
     *
     * \param sps (float) Oversampling ratio.
     * \param loop_bw (float) Loop bandwidth.
     * \param damping_factor (float) Damping factor.
     * \param rolloff (float) Rolloff factor.
     * \param rrc_delay (int) Target root raised cosine (RRC) filter delay in symbol
     * periods when using a polyphase interpolator (with interp_method=0). Ignored if
     * using another interpolation method.
     * \param n_subfilt (int) Number of subfilters in the polyphase realization of the RRC
     * filter used for joint matched filtering and polyphase interpolation when
     * interp_method=0. Ignored if using another interpolation method.
     * \param interp_method (int) Interpolation method: polyphase (0),
     * linear (1), quadratic (2), or cubic (3).
     *
     * \note The number of subfilters `n_subfilt` used with the polyphase interpolator
     * does not impact on the computational cost. A single subfilter is used per strobe,
     * so only the subfilter length controlled by paramter `rrc_delay` matters for the CPU
     * usage, not the number of subfilters. In fact, it is preferable to pick a
     * sufficiently large value for n_subfilt (such as 128) for better resolution on the
     * output interpolants. In contrast, to keep the computational cost at a minimum, it
     * is generally preferable to use a relatively low RRC delay value.
     *
     * \note The polyphase interpolation method is generally more efficient because its
     * interpolation is obtained by the same computations used for matched filtering. In
     * contrast, the other interpolation schemes require both a dedicated matched filter
     * block (preceding the symbol synchronizer) and the interpolation itself.
     */
    static sptr make(float sps,
                     float loop_bw,
                     float damping_factor,
                     float rolloff,
                     int rrc_delay = 5,
                     int n_subfilt = 128,
                     int interp_method = 0);
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_SYMBOL_SYNC_CC_H */
