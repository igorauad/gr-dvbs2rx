/* -*- c++ -*- */
/* 
 * Copyright 2018 Ron Economos.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */


#ifndef INCLUDED_DVBS2RX_BBDEHEADER_BB_H
#define INCLUDED_DVBS2RX_BBDEHEADER_BB_H

#include <dvbs2rx/api.h>
#include <gnuradio/block.h>
#include <dvbs2rx/dvb_config.h>
#include <dvbs2rx/dvbt2_config.h>

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
      static sptr make(dvb_standard_t standard, dvb_framesize_t framesize, dvb_code_rate_t rate);
    };

  } // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_BBDEHEADER_BB_H */

