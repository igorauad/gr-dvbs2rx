/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_PL_SUBMODULE_H
#define INCLUDED_DVBS2RX_PL_SUBMODULE_H

#include <gnuradio/logger.h>

namespace gr {
namespace dvbs2rx {

/**
 * PL Block Submodule
 *
 * Provides logging support for the physical layer (PL) submodules that do not inherit
 * directly from gr::basic_block.
 */
class pl_submodule
{
protected:
    int d_debug_level;             //! Debug level
    gr::logger_ptr d_logger;       //! Default logger
    gr::logger_ptr d_debug_logger; //! Verbose logger

public:
    pl_submodule(const std::string name, int debug_level) : d_debug_level(debug_level)
    {
        gr::configure_default_loggers(d_logger, d_debug_logger, name);
    }
};

} // namespace dvbs2rx
} // namespace gr

#endif