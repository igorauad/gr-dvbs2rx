/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_CDEQUE_H
#define INCLUDED_DVBS2RX_CDEQUE_H

#include <volk/volk_alloc.hh>
#include <cstring>

/**
 * @brief Fixed-length double-ended queue with contiguous volk-aligned elements
 *
 * Creates a queue (or first-in first-out data structure) that stores data on a
 * fixed-length and contiguous range of indexes with proper alignment created by
 * volk_malloc. Similarly to an std::deque, this implementation supports
 * insertion at both the queue's back (or tail) and front (or head). On the
 * other hand, unlike an std::dequeue, this implementation always has L elements
 * from tail to head, namely the queue has a fixed length.
 *
 * To enforce the fixed length, this template class only allows for writing of
 * new elements, while not providing any function to pop elements
 * explicitly. The front element is popped automatically whenever a new element
 * is written at the back. Likewise, the back element is popped whenever a new
 * element is pushed into the head.
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
 * a new number comes in, and the buffer advances such that the previous tail
 * element (i.e., 0) is thrown away, the tail pointer advances to the next
 * number (i.e., 1), and the head pointer wraps around to where number 0 was
 * before. Suppose the new number is 4, such that the hypothetical ring buffer
 * now holds [4, 1, 2, 3]. In this scenario, the actual array of interest is [1,
 * 2, 3, 4], but the circular buffer no longer holds these values sequentially
 * on a contiguous range of indexes. Hence, if we were to use such a ring buffer
 * for something like a volk dot product with a fixed array of taps, it would be
 * necessary to execute two calls, one for the first part (e.g., [1, 2, 3]) and
 * the other for the remaining part ([4] in the example). That would reduce the
 * efficiency of processing a large batch of samples at once.
 *
 * This implementation avoids the problem by using a buffer whose actual length
 * is (n_reps * L), i.e. much longer than necessary, where "n_reps" is a chosen
 * number of repetitions of L-length segments. When the head pointer reaches the
 * end of the buffer, the implementation copies the last L-1 values back to the
 * beginning and rewinds the head pointer back to index L-1, instead of index
 * 0. As a result, the range [i_tail, i_head) will process samples 0 to
 * L-1. Consequently, the L numbers of interest are always available on a
 * contiguous range of indexes. The sample applies when moving the tail pointer
 * in the other direction.
 *
 * This double-ended queue allows for ring buffer movement in both clockwise and
 * counterclockwise directions. The buffer spins counterclockwise when writing
 * elements on its back (tail). For instance, if the tail index is 10 and we
 * push an element on this index, the buffer moves to the left
 * (counterclockwise) such that the next tail index becomes 9. As a result, the
 * previous write (on index 10) shifts to the second position when reading
 * samples from tail to head. In contrast, the buffer spins clockwise when
 * writing elements on its head. On a similar example, if the head index is 10
 * and we push a new element on this index, the buffer shifts to the right such
 * that the next head index becomes 11. Consequently, the previous write (on
 * index 10) shifts to the penultimate position when reading samples from tail
 * to head. In both cases, the buffer shifts in a direction that keeps the
 * previous write inside the buffer.
 *
 * The two movement directions can be used in different applications. For
 * example, by writing samples on the queue's back, one can implement a delay
 * line (see delay_line.h). That is, a buffer holding the last L samples, which
 * can be read on a contiguous array from tail to head starting with the most
 * recent sample and ending at the oldest sample. In contrast, by writing on the
 * queue's front, one can keep the last L samples in reversed order. That is,
 * the contiguous array from tail to head will hold the last L samples starting
 * from the oldest sample and ending at the most recent sample.
 *
 * Besides, note the adopted terminology is such that the tail pointer is
 * interchangeably called the back of the queue and the head pointer is
 * considered the front of the queue. The natural FIFO direction would be to
 * enter the queue at the back and leave it on its front. The reverse movement
 * is to enter the queue on its front and leave it on its back. In any case,
 * this class always reads the buffer contents from back to front (tail to
 * head). The beginning and end of the queue depends on the movement direction.
 *
 * Note this data structure is primarily meant to be used with Volk kernels,
 * which usually take the pointer to a contiguous volk-allocated data array. The
 * reason for using n_reps is to avoid doing the aforementioned copy too
 * often. The tradeoff is between copying often or allocating too much memory.
 */
namespace gr {
namespace dvbs2rx {

template <typename T>
class cdeque
{
private:
    volk::vector<T> buf;       /** Contiguous Volk buffer */
    unsigned int i_tail;       /** Tail index */
    const unsigned int L;      /** Queue length */
    const unsigned int n_reps; /** Number of L-length segment repetitions */

public:
    explicit cdeque(unsigned int len, unsigned int n_reps = 10)
        : buf(n_reps * len), i_tail(0), L(len), n_reps(n_reps)
    {
    }

    /**
     * @brief Push new element into the ring buffer's back (tail).
     *
     * Move the ring buffer counterclockwise before and then push the given
     * element on the new tail index.
     *
     * @param in New element.
     */
    void push_back(const T& in)
    {
        /* When the tail pointer needs to wrap around, copy the first L-1
         * elements back to the end of the buffer. */
        if (i_tail == 0) {
            i_tail = ((n_reps - 1) * L) + 1; // next index is that minus 1
            std::copy(buf.begin(), buf.begin() + L - 1, buf.end() - L + 1);
        }
        buf[--i_tail] = in;
    }

    /**
     * @brief Push new element into the ring buffer's front (head).
     *
     * Move the ring buffer clockwise before and then push the given element on
     * the new head index.
     *
     * @param in New element.
     */
    void push_front(const T& in)
    {
        /* When the tail pointer needs to wrap around, copy the last L-1
         * elements back to the beginning of the buffer. */
        if (i_tail == ((n_reps - 1) * L)) {
            i_tail = -1; // next index is that plus 1 (i.e., 0)
            std::copy(buf.end() - L + 1, buf.end(), buf.begin());
        }
        buf[++i_tail + L - 1] = in;
    }

    /**
     * @brief Access the element at the back of the queue.
     * @return Reference to the back element.
     */
    const T& back() const { return buf[i_tail]; }

    /**
     * @brief Access the element at the front of the queue.
     * @return Reference to the front element.
     */
    const T& front() const { return buf[i_tail + L - 1]; }

    /**
     * @brief Get length L of the queue
     * @return Queue length.
     */
    unsigned int length() const { return L; }
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_CDEQUE_H */
