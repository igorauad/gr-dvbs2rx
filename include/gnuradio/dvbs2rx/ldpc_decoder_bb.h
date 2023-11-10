/* -*- c++ -*- */
/*
 * Copyright 2018 Ahmet Inan, Ron Economos.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef INCLUDED_DVBS2RX_LDPC_DECODER_BB_H
#define INCLUDED_DVBS2RX_LDPC_DECODER_BB_H

#include <gnuradio/block.h>
#include <gnuradio/dvbs2rx/api.h>
#include <gnuradio/dvbs2rx/dvb_config.h>

namespace gr {
namespace dvbs2rx {

/*!
 * \brief <+description of block+>
 * \ingroup dvbs2rx
 *
 */
class DVBS2RX_API ldpc_decoder_bb : virtual public gr::block
{
public:
    typedef std::shared_ptr<ldpc_decoder_bb> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of dvbs2rx::ldpc_decoder_bb.
     *
     * To avoid accidental use of raw pointers, dvbs2rx::ldpc_decoder_bb's constructor is
     * in a private implementation class. dvbs2rx::ldpc_decoder_bb::make is the public
     * interface for creating new instances.
     */
    static sptr make(dvb_standard_t standard,
                     dvb_framesize_t framesize,
                     dvb_code_rate_t rate,
                     dvb_constellation_t constellation,
                     dvb_outputmode_t outputmode,
                     dvb_infomode_t infomode,
                     int max_trials,
                     int debug_level = 0);

    /*!
     * \brief Get the average number of LDPC decoding iterations per frame.
     * \return unsigned int Average decoding interations.
     */
    virtual unsigned int get_average_trials() = 0;
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_LDPC_DECODER_BB_H */
