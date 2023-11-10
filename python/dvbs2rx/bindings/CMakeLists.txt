# Copyright 2020 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

GR_PYTHON_CHECK_MODULE_RAW(
    "pygccxml"
    "import pygccxml"
    PYGCCXML_FOUND
    )

include(GrPybind)

########################################################################
# Python Bindings
########################################################################

list(APPEND dvbs2rx_python_files
    bbdeheader_bb_python.cc
    bbdescrambler_bb_python.cc
    bch_decoder_bb_python.cc
    dvb_config_python.cc
    dvbs2_config_python.cc
    dvbt2_config_python.cc
    ldpc_decoder_bb_python.cc
    plsync_cc_python.cc
    rotator_cc_python.cc
    symbol_sync_cc_python.cc
    xfecframe_demapper_cb_python.cc
    python_bindings.cc)

GR_PYBIND_MAKE_OOT(dvbs2rx
   ../../..
   gr::dvbs2rx
   "${dvbs2rx_python_files}")

install(TARGETS dvbs2rx_python DESTINATION ${GR_PYTHON_DIR}/gnuradio/dvbs2rx COMPONENT pythonapi)
