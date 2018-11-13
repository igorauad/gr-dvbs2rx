/* -*- c++ -*- */
/* 
 * Copyright 2018 Ahmet Inan, Ron Economos.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*
 * Reduce N times while excluding ith input element
 */

#ifndef EXCLUSIVE_REDUCE_HH
#define EXCLUSIVE_REDUCE_HH

namespace CODE {

  template <typename TYPE, typename OPERATOR>
  void exclusive_reduce(const TYPE *in, TYPE *out, int N, OPERATOR op)
  {
    TYPE pre = in[0];
    for (int i = 1; i < N-1; ++i) {
      out[i] = pre;
      pre = op(pre, in[i]);
    }
    out[N-1] = pre;
    TYPE suf = in[N-1];
    for (int i = N-2; i > 0; --i) {
      out[i] = op(out[i], suf);
      suf = op(suf, in[i]);
    }
    out[0] = suf;
  }
}

#endif

