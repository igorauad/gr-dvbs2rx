/*
 * Copyright 2022 Free Software Foundation, Inc.
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
/* BINDTOOL_HEADER_FILE(rotator_cc.h)                                        */
/* BINDTOOL_HEADER_FILE_HASH(7ca109eeae131119952f36ef2ba0860d)                     */
/***********************************************************************************/

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <gnuradio/dvbs2rx/rotator_cc.h>
// pydoc.h is automatically generated in the build directory
#include <rotator_cc_pydoc.h>

void bind_rotator_cc(py::module& m)
{

    using rotator_cc = ::gr::dvbs2rx::rotator_cc;

    py::class_<rotator_cc,
               gr::sync_block,
               gr::block,
               gr::basic_block,
               std::shared_ptr<rotator_cc>>(m, "rotator_cc", D(rotator_cc))

        .def(py::init(&rotator_cc::make),
             py::arg("phase_inc") = 0.,
             py::arg("tag_inc_updates") = false,
             D(rotator_cc, make))

        .def("set_phase_inc",
             &rotator_cc::set_phase_inc,
             py::arg("phase_inc"),
             D(rotator_cc, set_phase_inc));
}