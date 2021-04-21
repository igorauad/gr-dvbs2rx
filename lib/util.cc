/* -*- c++ -*- */
/*
 * Copyright 2019-2021 Igor Freire.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "util.h"
#include <volk/volk.h>
#include <cstring>

namespace gr {
namespace dvbs2rx {

buffer::buffer(unsigned int len) : i_head(-1), i_tail(0), L(len)
{
    buf = (gr_complex*)volk_malloc(n_reps * L * sizeof(gr_complex), volk_get_alignment());
    memset(buf, 0, n_reps * L * sizeof(gr_complex));
}

buffer::~buffer() { volk_free(buf); }

void buffer::push(const gr_complex& in)
{
    /* Advance head and wrap around if necessary
     *
     * The key feature of the buffer class is that it can always provide a
     * pointer after which L indexes are valid. We can then pass this pointer to
     * volk and don't need to e.g. call the volk kernel twice for two pieces of
     * a buffer.
     *
     * We could accomplish this by a "circular buffer", but we would need to
     * shift the buffer for every new sample pushed into it. Instead, the way we
     * accomplish this is by using a buffer whose actual length is (n_reps * L),
     * i.e. much longer than necessary. We always advance the head pointer and,
     * when i_head reaches the end, we copy the last L-1 values of the buffer
     * back to its beginning. We then rewind i_head back to index L-1 such that
     * range [i_tail, i_head) will processes samples 0 to L-1.
     *
     * The reason for using n_reps is to avoid doing the referred copy too
     * often. The tradeoff is between copying often or allocating too much
     * memory.
     **/
    i_head++;

    if (i_head == (n_reps * L)) {
        i_head = L - 1;
        i_tail = 0;
        /* Copy the last L-1 values of the buffer back to the
         * beginning.
         *
         * NOTE: the reason why we have a buffer with size
         * n_reps*L is to avoid doing so very often. */
        memcpy(&buf[0], &buf[(n_reps - 1) * L + 1], (L - 1) * sizeof(gr_complex));
    }

    /* Put new value onto head */
    buf[i_head] = in;

    /* Transitory: don't advance tail until we are over it */
    if (i_head < L)
        return;

    i_tail++;
}

void buffer::dump(const char* label)
{
    printf("Head: %u\tTail: %u\n", i_head, i_tail);
    dump_complex_vec(buf, L * n_reps, label);
}

void dump_real_vec(const float* vec, unsigned int N, const char* label)
{
    printf("- %10s: [", label);
    for (unsigned int i = 0; i < N - 1; i++)
        printf("%.2f, ", vec[i]);
    printf("%.2f]\n", vec[N - 1]);
}

void dump_real_vec(const volk::vector<float>& vec, unsigned int N, const char* label)
{
    if (vec.size() < N)
        throw std::runtime_error("Invalid vector size");
    dump_real_vec(vec.data(), N, label);
}

void dump_complex_vec(const gr_complex* vec, unsigned int N, const char* label)
{
    printf("- %10s: [", label);
    for (unsigned int i = 0; i < N - 1; i++)
        printf("(%+.2f %+.2fi), ", vec[i].real(), vec[i].imag());
    printf("(%+.2f %+.2fi)]\n", vec[N - 1].real(), vec[N - 1].imag());
}

void dump_complex_vec(const volk::vector<gr_complex>& vec,
                      unsigned int N,
                      const char* label)
{
    if (vec.size() < N)
        throw std::runtime_error("Invalid vector size");
    dump_complex_vec(vec.data(), N, label);
}

} // namespace dvbs2rx
} // namespace gr
