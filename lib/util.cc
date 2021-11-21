/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "util.h"

namespace gr {
namespace dvbs2rx {


void dump_real_vec(const float* vec, unsigned int N, const char* label)
{
    printf("- %10s: [", label);
    for (unsigned int i = 0; i < N - 1; i++)
        printf("%g, ", vec[i]);
    printf("%g]\n", vec[N - 1]);
}

void dump_real_vec(const volk::vector<float>& vec, unsigned int N, const char* label)
{
    if (vec.size() < N)
        throw std::runtime_error("Invalid vector size");
    dump_real_vec(vec.data(), N, label);
}

void dump_real_vec(const delay_line<float>& vec, const char* label)
{
    dump_real_vec(&vec.back(), vec.length(), label);
}

void dump_complex_vec(const gr_complex* vec, unsigned int N, const char* label)
{
    printf("- %10s: [", label);
    for (unsigned int i = 0; i < N - 1; i++)
        printf("(%+g %+gi), ", vec[i].real(), vec[i].imag());
    printf("(%+g %+gi)]\n", vec[N - 1].real(), vec[N - 1].imag());
}

void dump_complex_vec(const volk::vector<gr_complex>& vec,
                      unsigned int N,
                      const char* label)
{
    if (vec.size() < N)
        throw std::runtime_error("Invalid vector size");
    dump_complex_vec(vec.data(), N, label);
}

void dump_complex_vec(const delay_line<gr_complex>& vec, const char* label)
{
    dump_complex_vec(&vec.back(), vec.length(), label);
}

} // namespace dvbs2rx
} // namespace gr
