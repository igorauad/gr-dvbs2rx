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

namespace gr {
namespace dvbs2rx {

/**
 * @brief Fixed-size delay-line with contiguous storage of volk-aligned elements
 *
 * Creates a queue (or first-in first-out data structure) that stores data on a
 * fixed-length and contiguous range of indexes with proper alignment created by
 * volk_malloc. Similarly to an std::queue, this implementation always writes
 * new elements at the back (or tail) of the queue and pops elements from the
 * front (or head). On the other hand, unlike an std::queue, this implementation
 * always has L elements from tail to head, namely the queue has a fixed
 * length. Furthermore, unlike a regular std::queue, this implementation only
 * allows for writing of new elements, while not providing any function to pop
 * elements explicitly. Instead, the front element is popped automatically
 * whenever a new element is written at the back, given that the queue has a
 * fixed length. Effectively, this class implements a delay line, whose newest
 * and oldest elements lie on the tail and head indexes, respectively. It is
 * meant to be used on tapped delay line algorithms, such as digital filters.
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
 * was. Suppose the new number is 4, such that the hypothetical ring buffer now
 * holds [4, 1, 2, 3]. In this scenario, the actual array of interest is [1, 2,
 * 3, 4], but the circular buffer no longer holds these values sequentially on a
 * contiguous range of indexes. Hence, if we were to use such a ring buffer for
 * something like a volk dot product, it would be necessary to execute two
 * calls, one for the first part (e.g., [1, 2, 3]) and the other for the
 * remaining part ([4] in the example). That would reduce the efficiency of
 * processing a large batch of samples at once.
 *
 * This implementation avoids the problem by using a buffer whose actual length
 * is (n_reps * L), i.e. much longer than necessary, where "n_reps" is a chosen
 * number of repetitions of L-length segments. When the head pointer reaches the
 * end, the implementation copies the last L-1 values back to the beginning and
 * rewinds the head pointer back to index L-1, instead of index 0. As a result,
 * the range [i_tail, i_head) will process samples 0 to L-1. Consequently, the L
 * numbers of interest are always available on a contiguous range of indexes.
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
    volk::vector<T> buf;       /** buffer */
    unsigned int i_tail;       /** tail index (newest element) */
    const unsigned int L;      /** Delay line length */
    const unsigned int n_reps; /** L-length segment repetitions */

public:
    explicit delay_line(unsigned int len, unsigned int n_reps = 10)
        : buf(n_reps * len), i_tail(((n_reps - 1) * len)), L(len), n_reps(n_reps)
    {
    }

    /**
     * @brief Push new element into the ring buffer.
     * @param in New element.
     * @return Void.
     */
    void push(const T& in)
    {
        /* When the tail pointer needs to roll back, copy the L-1 delay line
         * element from the previous iteration back to the end of the buffer. */
        if (i_tail == 0) {
            i_tail = ((n_reps - 1) * L) + 1;
            std::copy(buf.begin(), buf.begin() + L - 1, buf.end() - L + 1);
        }
        buf[--i_tail] = in;
    }

    /**
     * @brief Access the last (i.e., newest) element at the back of the queue.
     * @return Reference to the last element.
     */
    const T& back() const { return buf[i_tail]; }

    /**
     * @brief Access the first (i.e., oldest) element at the front of the queue.
     * @return Reference to the first element.
     */
	const T& front() const { return buf[i_tail + L - 1]; }

    /**
     * @brief Get length L of the delay line.
     * @return Delay line length.
     */
    unsigned int length() const { return L; }
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_DELAY_LINE_H */
