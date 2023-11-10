/*
 * Copyright 2020 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include <pybind11/pybind11.h>

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>

namespace py = pybind11;

// Headers for binding functions
/**************************************
 * The following comment block is used for
 * gr_modtool to insert function prototypes
 * Please do not delete
 **************************************/
// BINDING_FUNCTION_PROTOTYPES(
void bind_bbdeheader_bb(py::module& m);
void bind_bbdescrambler_bb(py::module& m);
void bind_bch_decoder_bb(py::module& m);
void bind_dvb_config(py::module& m);
void bind_dvbs2_config(py::module& m);
void bind_dvbt2_config(py::module& m);
void bind_ldpc_decoder_bb(py::module& m);
void bind_plsync_cc(py::module& m);
void bind_rotator_cc(py::module& m);
void bind_symbol_sync_cc(py::module& m);
void bind_xfecframe_demapper_cb(py::module& m);
// ) END BINDING_FUNCTION_PROTOTYPES


// We need this hack because import_array() returns NULL
// for newer Python versions.
// This function is also necessary because it ensures access to the C API
// and removes a warning.
void* init_numpy()
{
    import_array();
    return NULL;
}

PYBIND11_MODULE(dvbs2rx_python, m)
{
    // Initialize the numpy C API
    // (otherwise we will see segmentation faults)
    init_numpy();

    // Allow access to base block methods
    py::module::import("gnuradio.gr");

    /**************************************
     * The following comment block is used for
     * gr_modtool to insert binding function calls
     * Please do not delete
     **************************************/
    // BINDING_FUNCTION_CALLS(
    bind_bbdeheader_bb(m);
    bind_bbdescrambler_bb(m);
    bind_bch_decoder_bb(m);
    bind_dvb_config(m);
    bind_dvbs2_config(m);
    bind_dvbt2_config(m);
    bind_ldpc_decoder_bb(m);
    bind_plsync_cc(m);
    bind_rotator_cc(m);
    bind_symbol_sync_cc(m);
    bind_xfecframe_demapper_cb(m);
    // ) END BINDING_FUNCTION_CALLS
}