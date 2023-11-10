/* -*- c++ -*- */
/*
 * Copyright 2023 Igor Freire.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_XFECFRAME_DEMAPPER_CB_H
#define INCLUDED_DVBS2RX_XFECFRAME_DEMAPPER_CB_H

#include <gnuradio/block.h>
#include <gnuradio/dvbs2rx/api.h>
#include <gnuradio/dvbs2rx/dvb_config.h>

namespace gr {
namespace dvbs2rx {

/*!
 * \brief XFECFRAME Constellation Demapper.
 * \ingroup dvbs2rx
 *
 */
class DVBS2RX_API xfecframe_demapper_cb : virtual public gr::block
{
public:
    typedef std::shared_ptr<xfecframe_demapper_cb> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of dvbs2rx::xfecframe_demapper_cb.
     *
     * To avoid accidental use of raw pointers, dvbs2rx::xfecframe_demapper_cb's
     * constructor is in a private implementation class.
     * dvbs2rx::xfecframe_demapper_cb::make is the public interface for creating new
     * instances.
     */
    static sptr make(dvb_framesize_t framesize,
                     dvb_code_rate_t rate,
                     dvb_constellation_t constellation);

    /*!
     * \brief Get the measured SNR.
     * \return float Measured SNR.
     */
    virtual float get_snr() = 0;
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_XFECFRAME_DEMAPPER_CB_H */
