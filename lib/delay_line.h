/* -*- c++ -*- */
/*
 * Copyright (c) 2019-2021 Igor Freire
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_DELAY_LINE_H
#define INCLUDED_DVBS2RX_DELAY_LINE_H

#include <volk/volk_alloc.hh>
#include <cstring>

#define N_REPS 10

namespace gr {
namespace dvbs2rx {

/**
 * @brief Fixed-size delay-line with contiguous storage of volk-aligned elements
 *
 * Creates a queue (or first-in first-out data structure) that stores data on a
 * fixed-length and contiguous range of indexes with proper alignment created by
 * volk_malloc. Similarly to an std::queue, this implementation always writes
 * new elements at the back of the queue and pops from the front. Nevertheless,
 * unlike an std::queue, this implementation always has L elements from the tail
 * to head pointers, as the queue has a fixed length. Furthermore, unlike a
 * regular std::queue, this implementation only allows for writing of new
 * elements, while not providing any function to pop elements. Instead, the
 * front element is popped automatically whenever a new element is written at
 * the back. Effectively, this class consists of a delay line, which is meant to
 * be used on tapped delay line algorithms (e.g., digital filters).
 *
 * The key feature of this class is that it ensures the elements from tail to
 * head (or back to front) are always on a contiguous memory block with
 * sequential indexes. Hence, one can safely pass this buffer's tail pointer to
 * a function that processes up to L elements from a C-style array. For
 * instance, a function "sum(float* x)" that sums up to L elements from a
 * float[L] array at address x.
 *
 * The implementation is based on a ring buffer, with a fixed-size volk-aligned
 * array allocated on the heap by the constructor. However, unlike a regular
 * ring buffer, this implementation ensures the data is on a contiguous and
 * sequential range of indexes even when the head and tail pointers wrap around.
 *
 * For example, consider a four-element circular buffer holding the sequence [0,
 * 1, 2, 3], where 0 is at the tail index and 3 is at the head. Then, suppose
 * that, at first, we want to call a volk kernel based on this array. Later on,
 * a new number comes, and the buffer advances such that the previous tail
 * element (i.e., 0) is thrown away, the tail pointer advances to the next
 * number (i.e., 1), and the head pointer wraps around to where number 0
 * was. Suppose the new number is 4, such that the buffer holds [4, 1, 2,
 * 3]. Now, the actual array of interest is [1, 2, 3, 4], but the circular
 * buffer no longer holds this sequence on a contiguous range of indexes. In
 * this case, if we were to use such a ring buffer, it would be necessary to
 * split any linear volk kernel into two calls, one for the first part (e.g.,
 * [1, 2, 3]) and the other for the remaining part ([4] in the example). That
 * would reduce the efficiency of processing a large batch of samples at once.
 *
 * This implementation avoids the problem by using a buffer whose actual length
 * is (n_reps * L), i.e. much longer than necessary, where "n_reps" is a chosen
 * number of repetitions of L-length segments. When this buffer's head pointer
 * reaches the end, the implementation copies the last L-1 values back to the
 * beginning and rewinds the head pointer back to index L-1, instead of index
 * 0. As a result, that range [i_tail, i_head) will process samples 0 to L-1. As
 * a result, the L numbers of interest are always available on a contiguous
 * range of indexes.
 *
 * This data structure is primarily meant to be used with Volk kernels, which
 * usually take the pointer to a contiguous volk-allocated data array. The
 * reason for using n_reps is to avoid doing the aforementioned copy too
 * often. The tradeoff is between copying often or allocating too much memory.
 **/
template <typename T>
class delay_line
{
private:
    T* buf;                    /** buffer pointer */
    unsigned int i_head;       /** head index */
    unsigned int i_tail;       /** tail index */
    const unsigned int L;      /** length */
    const unsigned int n_reps; /** L-length segment repetitions */

public:
    explicit delay_line(unsigned int len, unsigned int n_reps = N_REPS)
        : i_head(-1), i_tail(0), L(len), n_reps(n_reps)
    {
        buf = (T*)volk_malloc(n_reps * L * sizeof(T), volk_get_alignment());
        memset(buf, 0, n_reps * L * sizeof(T));
    }

    ~delay_line() { volk_free(buf); }

    /**
     * @brief Push new element into the ring buffer.
     * @param in New element.
     * @return Void.
     */
    void push(const T& in)
    {
        i_head++;

        if (i_head == (n_reps * L)) {
            i_head = L - 1;
            i_tail = 0;

            /* Copy the last L-1 values of the buffer back to the
             * beginning.
             *
             * NOTE: the reason why we have a buffer with size
             * n_reps*L is to avoid doing so very often. */
            memcpy(&buf[0], &buf[(n_reps - 1) * L + 1], (L - 1) * sizeof(T));
        }

        /* Put new value into the head index */
        buf[i_head] = in;

        /* Initial transitory only: don't advance the tail until it's over */
        if (i_head < L)
            return;

        i_tail++;
    }

    /**
     * @brief Get pointer to the tail element
     * @return Tail pointer
     */
    T* get_tail() { return buf + i_tail; }

    /**
     * @brief Get length of the underlaying tapped-delay line
     * @return Length
     */
    unsigned int get_length() { return L; }
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_DELAY_LINE_H */
