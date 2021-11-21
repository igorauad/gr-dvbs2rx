/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_UTIL_H
#define INCLUDED_DVBS2RX_UTIL_H

#include "delay_line.h"
#include <gnuradio/gr_complex.h>
#include <volk/volk_alloc.hh>

namespace gr {
namespace dvbs2rx {

void dump_real_vec(const float* vec, unsigned int N, const char* label);
void dump_real_vec(const volk::vector<float>& vec, unsigned int N, const char* label);
void dump_real_vec(const delay_line<float>& vec, const char* label);
void dump_complex_vec(const gr_complex* vec, unsigned int N, const char* label);
void dump_complex_vec(const volk::vector<gr_complex>& vec,
                      unsigned int N,
                      const char* label);
void dump_complex_vec(const delay_line<gr_complex>& vec, const char* label);

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_UTIL_H */
