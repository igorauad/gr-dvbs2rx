/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_PL_DEFS_H
#define INCLUDED_DVBS2RX_PL_DEFS_H

#include <stdint.h>

#define SOF_LEN 26
#define PLSC_LEN 64
#define PILOT_BLK_LEN 36
#define MAX_PILOT_BLKS 22
#define MIN_SLOTS 36
#define MAX_SLOTS 360
#define PLHEADER_LEN (SOF_LEN + PLSC_LEN)
#define SLOT_LEN 90
#define SLOTS_PER_PILOT_BLK 16
#define PILOT_BLK_INTERVAL (SLOTS_PER_PILOT_BLK * SLOT_LEN)
#define PILOT_BLK_PERIOD (PILOT_BLK_INTERVAL + PILOT_BLK_LEN)
#define MIN_XFECFRAME_LEN (MIN_SLOTS * SLOT_LEN)
#define MAX_XFECFRAME_LEN (MAX_SLOTS * SLOT_LEN)
#define MIN_PLFRAME_PAYLOAD MIN_XFECFRAME_LEN
#define MAX_PLFRAME_PAYLOAD MAX_XFECFRAME_LEN + (MAX_PILOT_BLKS * PILOT_BLK_LEN)
#define MIN_PLFRAME_LEN (PLHEADER_LEN + MIN_PLFRAME_PAYLOAD)
#define MAX_PLFRAME_LEN (PLHEADER_LEN + MAX_PLFRAME_PAYLOAD)

// Math macros
#define SQRT2_2 0.7071067811865476

namespace gr {
namespace dvbs2rx {

// number of codewords for the 7-bit PLSC dataword
const unsigned int n_plsc_codewords = 128;
// Start of Frame field in bit-level big endian format (MSB transmitted first)
constexpr uint64_t sof_big_endian = 0x18D2E82ll << 38;
// PLSC scrambler sequence (see Section 5.5.2.4 of the standard)
const uint64_t plsc_scrambler = 0x719d83c953422dfa;

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_PL_DEFS_H */
