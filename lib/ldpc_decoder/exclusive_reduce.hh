/* -*- c++ -*- */
/*
 * Copyright 2018 Ahmet Inan, Ron Economos.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * Reduce N times while excluding ith input element
 */

#ifndef EXCLUSIVE_REDUCE_HH
#define EXCLUSIVE_REDUCE_HH

namespace CODE {

template <typename TYPE, typename OPERATOR>
void exclusive_reduce(const TYPE* in, TYPE* out, int N, OPERATOR op)
{
    TYPE pre = in[0];
    for (int i = 1; i < N - 1; ++i) {
        out[i] = pre;
        pre = op(pre, in[i]);
    }
    out[N - 1] = pre;
    TYPE suf = in[N - 1];
    for (int i = N - 2; i > 0; --i) {
        out[i] = op(out[i], suf);
        suf = op(suf, in[i]);
    }
    out[0] = suf;
}
} // namespace CODE

#endif
