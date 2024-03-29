/*
 * Copyright 2021 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/***********************************************************************************/
/* This file is automatically generated using bindtool and can be manually edited  */
/* The following lines can be configured to regenerate this file during cmake      */
/* If manual edits are made, the following tags should be modified accordingly.    */
/* BINDTOOL_GEN_AUTOMATIC(0)                                                       */
/* BINDTOOL_USE_PYGCCXML(0)                                                        */
/* BINDTOOL_HEADER_FILE(symbol_sync_cc.h)                                          */
/* BINDTOOL_HEADER_FILE_HASH(9a1af62e0d1bde87191c0a0ee46e1169)                     */
/***********************************************************************************/

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <gnuradio/dvbs2rx/symbol_sync_cc.h>
// pydoc.h is automatically generated in the build directory
#include <symbol_sync_cc_pydoc.h>

void bind_symbol_sync_cc(py::module& m)
{

    using symbol_sync_cc = gr::dvbs2rx::symbol_sync_cc;

    py::class_<symbol_sync_cc,
               gr::block,
               gr::basic_block,
               std::shared_ptr<symbol_sync_cc>>(m, "symbol_sync_cc", D(symbol_sync_cc))

        .def(py::init(&symbol_sync_cc::make),
             py::arg("sps"),
             py::arg("loop_bw"),
             py::arg("damping_factor"),
             py::arg("rolloff"),
             py::arg("rrc_delay") = 5,
             py::arg("n_subfilt") = 128,
             py::arg("interp_method") = 0,
             D(symbol_sync_cc, make));
}
