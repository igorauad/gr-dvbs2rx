/* -*- c++ -*- */
/*
 * Copyright 2019-2021 Igor Freire.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_UTIL_H
#define INCLUDED_DVBS2RX_UTIL_H

#include <gnuradio/gr_complex.h>

#define N_REPS 10

namespace gr {
namespace dvbs2rx {

class buffer
{
private:
    gr_complex* buf;     /** buffer pointer */
    unsigned int i_head; /** head index */
    unsigned int i_tail; /** tail index */
    unsigned int L;      /** length */

    const unsigned int n_reps = N_REPS;

public:
    buffer(unsigned int len);
    ~buffer();
    void push(const gr_complex& in);
    gr_complex* get_tail() { return buf + i_tail; }
    unsigned int get_length() { return L; }
    void dump(const char* label);
};

void dump_real_vec(const float* vec, unsigned int N, const char* label);
void dump_complex_vec(const gr_complex* vec, unsigned int N, const char* label);

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_UTIL_H */
