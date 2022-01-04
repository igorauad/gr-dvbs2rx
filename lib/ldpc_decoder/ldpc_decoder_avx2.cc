/* -*- c++ -*- */
/*
 * Copyright 2022 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "algorithms.hh"
#include "layered_decoder.hh"

#define FACTOR 2

namespace ldpc_avx2 {

typedef SIMD<int8_t, 32> simd_type;
typedef NormalUpdate<simd_type> update_type;
typedef OffsetMinSumAlgorithm<simd_type, update_type, FACTOR> algorithm_type;

LDPCDecoder<simd_type, algorithm_type> LdpcDecoder;

void ldpc_dec_init(LDPCInterface* it) { LdpcDecoder.init(it); }

int ldpc_dec_decode(void* buffer, int8_t* code, int trials)
{
    return LdpcDecoder(buffer, code, trials);
}

} // namespace ldpc_avx2
