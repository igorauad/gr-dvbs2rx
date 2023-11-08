/* -*- c++ -*- */
/*
 * Copyright 2023 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_FEC_PARAMS_H
#define INCLUDED_DVBS2RX_FEC_PARAMS_H

#include <gnuradio/dvbs2rx/dvb_config.h>
#include <cstdint>

namespace gr {
namespace dvbs2rx {

struct bch_info_t {
    uint32_t k; // BCH message length in bits
    uint32_t n; // BCH codeword length in bits
    uint32_t t; // BCH error correction capability in bits
};

struct ldpc_info_t {
    uint32_t k; // LDPC message length in bits
    uint32_t n; // LDPC codeword length in bits
};

struct fec_info_t {
    bch_info_t bch;
    ldpc_info_t ldpc;
};

void get_fec_info(dvb_standard_t standard,
                  dvb_framesize_t framesize,
                  dvb_code_rate_t rate,
                  fec_info_t& fec_info);

} // namespace dvbs2rx
} // namespace gr
#endif