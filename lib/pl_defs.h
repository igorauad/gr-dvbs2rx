/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
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
#define MAX_SLOTS 360
#define PLHEADER_LEN (SOF_LEN + PLSC_LEN)
#define SLOT_LEN 90
#define SLOTS_PER_PILOT_BLK 16
#define PILOT_BLK_INTERVAL (SLOTS_PER_PILOT_BLK * SLOT_LEN)
#define PILOT_BLK_PERIOD (PILOT_BLK_INTERVAL + PILOT_BLK_LEN)
#define MAX_PLFRAME_PAYLOAD (MAX_SLOTS * SLOT_LEN) + (MAX_PILOT_BLKS * PILOT_BLK_LEN)

// Math macros
#define SQRT2_2 0.7071067811865476
#define SQRT2_2i 0.7071067811865476j

namespace gr {
namespace dvbs2rx {

// number of codewords for the 7-bit PLSC dataword
const int n_plsc_codewords = 128;
// PLSC scrambler sequence
const uint64_t plsc_scrambler = 0x719d83c953422dfa;
// Start of Frame field in bit-level big endian format (MSB transmitted first)
constexpr uint64_t sof_big_endian = 0x18D2E82ll << 38;

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_PL_DEFS_H */
