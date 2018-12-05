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

#ifndef LAYERED_DECODER_HH
#define LAYERED_DECODER_HH

#include <stdlib.h>
#include "ldpc.hh"

template <typename TYPE, typename ALG>
class LDPCDecoder
{
  TYPE *bnl;
  uint16_t *pos;
  uint8_t *cnc;
  ALG alg;
  int N, K, R, CNL, LT;
  bool initialized;

  void reset()
  {
    for (int i = 0; i < LT; ++i)
      bnl[i] = alg.zero();
  }
  bool bad(TYPE *data, TYPE *parity, int blocks)
  {
    {
      int cnt = cnc[0];
      TYPE cnv = alg.sign(alg.one(), parity[0]);
      for (int j = 0; j < cnt; ++j)
        cnv = alg.sign(cnv, data[pos[j]]);
      if (alg.bad(cnv, blocks))
        return true;
    }
    for (int i = 1; i < R; ++i) {
      int cnt = cnc[i];
      TYPE cnv = alg.sign(alg.sign(alg.one(), parity[i-1]), parity[i]);
      for (int j = 0; j < cnt; ++j)
        cnv = alg.sign(cnv, data[pos[CNL*i+j]]);
      if (alg.bad(cnv, blocks))
        return true;
    }
    return false;
  }
  void update(TYPE *data, TYPE *parity)
  {
    TYPE *bl = bnl;
    {
      int cnt = cnc[0];
      int deg = cnt + 1;
      TYPE inp[deg], out[deg];
      for (int j = 0; j < cnt; ++j)
        inp[j] = out[j] = alg.sub(data[pos[j]], bl[j]);
      inp[cnt] = out[cnt] = alg.sub(parity[0], bl[cnt]);
      alg.finalp(out, deg);
      for (int j = 0; j < cnt; ++j)
        data[pos[j]] = alg.add(inp[j], out[j]);
      parity[0] = alg.add(inp[cnt], out[cnt]);
      for (int j = 0; j < deg; ++j)
        alg.update(bl++, out[j]);
    }
    for (int i = 1; i < R; ++i) {
      int cnt = cnc[i];
      int deg = cnt + 2;
      TYPE inp[deg], out[deg];
      for (int j = 0; j < cnt; ++j)
        inp[j] = out[j] = alg.sub(data[pos[CNL*i+j]], bl[j]);
      inp[cnt] = out[cnt] = alg.sub(parity[i-1], bl[cnt]);
      inp[cnt+1] = out[cnt+1] = alg.sub(parity[i], bl[cnt+1]);
      alg.finalp(out, deg);
      for (int j = 0; j < cnt; ++j)
        data[pos[CNL*i+j]] = alg.add(inp[j], out[j]);
      parity[i-1] = alg.add(inp[cnt], out[cnt]);
      parity[i] = alg.add(inp[cnt+1], out[cnt+1]);
      for (int j = 0; j < deg; ++j)
        alg.update(bl++, out[j]);
    }
  }
public:
  LDPCDecoder() : initialized(false)
  {
  }
  void init(LDPCInterface *it)
  {
    if (initialized) {
      free(bnl);
      delete[] cnc;
      delete[] pos;
    }
    initialized = true;
    LDPCInterface *ldpc = it->clone();
    N = ldpc->code_len();
    K = ldpc->data_len();
    R = N - K;
    CNL = ldpc->links_max_cn() - 2;
    pos = new uint16_t[R * CNL];
    cnc = new uint8_t[R];
    for (int i = 0; i < R; ++i)
      cnc[i] = 0;
    ldpc->first_bit();
    for (int j = 0; j < K; ++j) {
      int *acc_pos = ldpc->acc_pos();
      int bit_deg = ldpc->bit_deg();
      for (int n = 0; n < bit_deg; ++n) {
        int i = acc_pos[n];
        pos[CNL*i+cnc[i]++] = j;
      }
      ldpc->next_bit();
    }
    LT = ldpc->links_total();
    delete ldpc;
    bnl = reinterpret_cast<TYPE *>(aligned_alloc(sizeof(TYPE), sizeof(TYPE) * LT));
  }
  int operator()(TYPE *data, TYPE *parity, int trials = 25, int blocks = 1)
  {
    reset();
    while (bad(data, parity, blocks) && --trials >= 0)
      update(data, parity);
    return trials;
  }
  ~LDPCDecoder()
  {
    if (initialized) {
      free(bnl);
      delete[] cnc;
      delete[] pos;
    }
  }
};

#endif
