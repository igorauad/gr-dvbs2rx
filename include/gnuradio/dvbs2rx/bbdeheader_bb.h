/* -*- c++ -*- */
/*
 * Copyright 2018 Ron Economos.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#ifndef INCLUDED_DVBS2RX_BBDEHEADER_BB_H
#define INCLUDED_DVBS2RX_BBDEHEADER_BB_H

#include <gnuradio/block.h>
#include <gnuradio/dvbs2rx/api.h>
#include <gnuradio/dvbs2rx/dvb_config.h>
#include <gnuradio/dvbs2rx/dvbt2_config.h>

namespace gr {
namespace dvbs2rx {

/*!
 * \brief <+description of block+>
 * \ingroup dvbs2rx
 *
 */
class DVBS2RX_API bbdeheader_bb : virtual public gr::block
{
public:
    typedef std::shared_ptr<bbdeheader_bb> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of dvbs2rx::bbdeheader_bb.
     *
     * To avoid accidental use of raw pointers, dvbs2rx::bbdeheader_bb's
     * constructor is in a private implementation
     * class. dvbs2rx::bbdeheader_bb::make is the public interface for
     * creating new instances.
     */
    static sptr make(dvb_standard_t standard,
                     dvb_framesize_t framesize,
                     dvb_code_rate_t rate,
                     int debug_level = 0);

    /*!
     * \brief Get count of MPEG TS packets extracted from BBFRAMEs.
     * \return uint64_t MPEG TS packet count.
     */
    virtual uint64_t get_packet_count() = 0;

    /*!
     * \brief Get count of corrupt MPEG TS packets extracted from BBFRAMEs.
     * \return uint64_t Corrupt packet count.
     */
    virtual uint64_t get_error_count() = 0;

    /*!
     * \brief Get count of processed BBFRAMEs.
     * \return uint64_t Number of BBFRAMEs processed so far.
     */
    virtual uint64_t get_bbframe_count() = 0;

    /*!
     * \brief Get count of BBFRAMEs dropped due to invalid BBHEADER.
     * \return uint64_t Number of BBFRAMEs dropped so far.
     */
    virtual uint64_t get_bbframe_drop_count() = 0;

    /*!
     * \brief Get count of gaps between BBFRAMEs.
     * \return uint64_t Number of gaps detected between BBFRAMEs.
     */
    virtual uint64_t get_bbframe_gap_count() = 0;
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_BBDEHEADER_BB_H */
