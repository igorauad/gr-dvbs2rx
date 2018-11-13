/* -*- c++ -*- */

#define DVBS2RX_API

%include "gnuradio.i"			// the common stuff

//load generated python docstrings
%include "dvbs2rx_swig_doc.i"

%{
#include "dvbs2rx/dvb_config.h"
#include "dvbs2rx/bbdescrambler_bb.h"
#include "dvbs2rx/bbdeheader_bb.h"
#include "dvbs2rx/ldpc_decoder_cb.h"
#include "dvbs2rx/bch_decoder_bb.h"
%}


%include "dvbs2rx/dvb_config.h"
%include "dvbs2rx/bbdescrambler_bb.h"
GR_SWIG_BLOCK_MAGIC2(dvbs2rx, bbdescrambler_bb);
%include "dvbs2rx/bbdeheader_bb.h"
GR_SWIG_BLOCK_MAGIC2(dvbs2rx, bbdeheader_bb);
%include "dvbs2rx/ldpc_decoder_cb.h"
GR_SWIG_BLOCK_MAGIC2(dvbs2rx, ldpc_decoder_cb);
%include "dvbs2rx/bch_decoder_bb.h"
GR_SWIG_BLOCK_MAGIC2(dvbs2rx, bch_decoder_bb);
