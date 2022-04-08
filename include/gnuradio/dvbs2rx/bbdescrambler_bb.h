/* -*- c++ -*- */
/*
 * Copyright 2018 Ron Economos.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_BBDESCRAMBLER_BB_H
#define INCLUDED_DVBS2RX_BBDESCRAMBLER_BB_H

#include <gnuradio/dvbs2rx/api.h>
#include <gnuradio/dvbs2rx/dvb_config.h>
#include <gnuradio/sync_block.h>

namespace gr {
namespace dvbs2rx {

/*!
 * \brief <+description of block+>
 * \ingroup dvbs2rx
 *
 */
class DVBS2RX_API bbdescrambler_bb : virtual public gr::sync_block
{
public:
    typedef std::shared_ptr<bbdescrambler_bb> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of dvbs2rx::bbdescrambler_bb.
     *
     * To avoid accidental use of raw pointers, dvbs2rx::bbdescrambler_bb's
     * constructor is in a private implementation
     * class. dvbs2rx::bbdescrambler_bb::make is the public interface for
     * creating new instances.
     */
    static sptr
    make(dvb_standard_t standard, dvb_framesize_t framesize, dvb_code_rate_t rate);
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_BBDESCRAMBLER_BB_H */
