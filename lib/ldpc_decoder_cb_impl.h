/* -*- c++ -*- */
/* 
 * Copyright 2018 Ahmet Inan, Ron Economos.
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

#ifndef INCLUDED_DVBS2RX_LDPC_DECODER_CB_IMPL_H
#define INCLUDED_DVBS2RX_LDPC_DECODER_CB_IMPL_H

#include <dvbs2rx/ldpc_decoder_cb.h>
#include "dvb_defines.h"
#include "psk.hh"
#include "qam.hh"
#include "ldpc_decoder.hh"
#include "dvb_s2_tables.hh"
#include "dvb_s2x_tables.hh"
#include "dvb_t2_tables.hh"

namespace gr {
  namespace dvbs2rx {

    class ldpc_decoder_cb_impl : public ldpc_decoder_cb
    {
     private:
      unsigned int frame_size;
      unsigned int signal_constellation;
      unsigned int code_rate;
      unsigned int nbch;
      unsigned int q_val;
      unsigned int dvb_standard;
      unsigned int output_mode;
      unsigned int frame;
      unsigned int rowaddr0;
      unsigned int rowaddr1;
      unsigned int rowaddr2;
      unsigned int rowaddr3;
      unsigned int rowaddr4;
      unsigned int rowaddr5;
      unsigned int rowaddr6;
      unsigned int rowaddr7;
      LDPCInterface<int8_t> *ldpc;
      Modulation<gr_complex, int8_t> *mod;

      const static int twist16n[8];
      const static int twist64n[12];
      const static int twist256n[16];

      const static int twist16s[8];
      const static int twist64s[12];
      const static int twist256s[8];

      const static int mux16[8];
      const static int mux64[12];
      const static int mux256[16];

      const static int mux16_35[8];
      const static int mux16_13[8];
      const static int mux16_25[8];
      const static int mux64_35[12];
      const static int mux64_13[12];
      const static int mux64_25[12];
      const static int mux256_35[16];
      const static int mux256_23[16];

      const static int mux256s[8];
      const static int mux256s_13[8];
      const static int mux256s_25[8];

     public:
      ldpc_decoder_cb_impl(dvb_standard_t standard, dvb_framesize_t framesize, dvb_code_rate_t rate, dvb_constellation_t constellation, dvb_outputmode_t outputmode);
      ~ldpc_decoder_cb_impl();

      void forecast (int noutput_items, gr_vector_int &ninput_items_required);

      int general_work(int noutput_items,
           gr_vector_int &ninput_items,
           gr_vector_const_void_star &input_items,
           gr_vector_void_star &output_items);
    };

  } // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_LDPC_DECODER_CB_IMPL_H */

