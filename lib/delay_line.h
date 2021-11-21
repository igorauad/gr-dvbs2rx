/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_DELAY_LINE_H
#define INCLUDED_DVBS2RX_DELAY_LINE_H

#include "cdeque.h"
#include <cstring>

namespace gr {
namespace dvbs2rx {

/**
 * @brief Fixed-size delay-line with contiguous storage of volk-aligned elements
 *
 * Wrapper around the cdeque (contiguous double-ended queue) that implements a
 * delay line, whose newest and oldest elements lie on the tail and head
 * indexes, respectively. This implementation is meant to be used on tapped
 * delay line algorithms, such as digital filters.
 *
 * Unlike cdeque, the delay line always returns the most recent to oldest
 * samples when reading from tail to head. To ensure that, it only supports
 * writing of new elements on the queue's back.
 *
 **/
template <typename T>
class delay_line : public cdeque<T>
{
public:
    explicit delay_line(unsigned int len, unsigned int n_reps = 10)
        : cdeque<T>(len, n_reps)
    {
    }
    void push(const T& in) { this->push_back(in); }
    void push_front(const T& in) = delete; // pushing at the front is forbidden
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_DELAY_LINE_H */
