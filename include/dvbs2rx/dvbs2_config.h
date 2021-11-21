/* -*- c++ -*- */
/*
 * Copyright 2018 Ron Economos.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_DVBS2_CONFIG_H
#define INCLUDED_DVBS2RX_DVBS2_CONFIG_H

namespace gr {
namespace dvbs2rx {
enum dvbs2_rolloff_factor_t {
    RO_0_35 = 0,
    RO_0_25,
    RO_0_20,
    RO_RESERVED,
    RO_0_15,
    RO_0_10,
    RO_0_05,
};

enum dvbs2_pilots_t {
    PILOTS_OFF = 0,
    PILOTS_ON,
};

enum dvbs2_interpolation_t {
    INTERPOLATION_OFF = 0,
    INTERPOLATION_ON,
};

} // namespace dvbs2rx
} // namespace gr

typedef gr::dvbs2rx::dvbs2_rolloff_factor_t dvbs2_rolloff_factor_t;
typedef gr::dvbs2rx::dvbs2_pilots_t dvbs2_pilots_t;
typedef gr::dvbs2rx::dvbs2_interpolation_t dvbs2_interpolation_t;

#endif /* INCLUDED_DVBS2RX_DVBS2_CONFIG_H */
