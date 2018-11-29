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

#ifndef LDPC_HH
#define LDPC_HH

#include <stdlib.h>
#include "exclusive_reduce.hh"

struct LDPCInterface
{
  virtual LDPCInterface *clone() = 0;
  virtual int code_len() = 0;
  virtual int data_len() = 0;
  virtual int links_total() = 0;
  virtual int links_max_cn() = 0;
  virtual int bit_deg() = 0;
  virtual int *acc_pos() = 0;
  virtual void first_bit() = 0;
  virtual void next_bit() = 0;
  virtual ~LDPCInterface() = default;
};

template <typename TABLE>
class LDPC : public LDPCInterface
{
  static const int M = TABLE::M;
  static const int N = TABLE::N;
  static const int K = TABLE::K;
  static const int R = N-K;
  static const int q = R/M;

  int acc_pos_[TABLE::DEG_MAX];
  const int *row_ptr;
  int bit_deg_;
  int grp_num;
  int grp_len;
  int grp_cnt;
  int row_cnt;

  void next_group()
  {
    if (grp_cnt >= grp_len) {
      grp_len = TABLE::LEN[grp_num];
      bit_deg_ = TABLE::DEG[grp_num];
      grp_cnt = 0;
      ++grp_num;
    }
    for (int i = 0; i < bit_deg_; ++i)
      acc_pos_[i] = row_ptr[i];
    row_ptr += bit_deg_;
    ++grp_cnt;
  }

public:

  LDPCInterface *clone()
  {
    return new LDPC<TABLE>();
  }

  int code_len()
  {
    return N;
  }

  int data_len()
  {
    return K;
  }

  int links_total()
  {
    return TABLE::LINKS_TOTAL;
  }

  int links_max_cn()
  {
    return TABLE::LINKS_MAX_CN;
  }

  int bit_deg()
  {
    return bit_deg_;
  }

  int *acc_pos()
  {
    return acc_pos_;
  }

  void next_bit()
  {
    if (++row_cnt < M) {
      for (int i = 0; i < bit_deg_; ++i)
        acc_pos_[i] += q;
      for (int i = 0; i < bit_deg_; ++i)
        acc_pos_[i] %= R;
    } else {
      next_group();
      row_cnt = 0;
    }
  }

  void first_bit()
  {
    grp_num = 0;
    grp_len = 0;
    grp_cnt = 0;
    row_cnt = 0;
    row_ptr = TABLE::POS;
    next_group();
  }
};

template <typename TYPE, typename ALG>
class LDPCDecoder
{
  void *aligned_buffer;
  TYPE *bnl, *bnv, *cnl, *cnv;
  uint8_t *cnc;
  LDPCInterface *ldpc;
  ALG alg;
  int N, K, R, CNL, LT;

  void bit_node_init(TYPE *data, TYPE *parity)
  {
    TYPE *bl = bnl;
    for (int i = 0; i < R-1; ++i) {
      bnv[i] = parity[i];
      *bl++ = parity[i];
      *bl++ = parity[i];
    }
    bnv[R-1] = parity[R-1];
    *bl++ = parity[R-1];
    ldpc->first_bit();
    for (int j = 0; j < K; ++j) {
      bnv[j+R] = data[j];
      int bit_deg = ldpc->bit_deg();
      for (int n = 0; n < bit_deg; ++n)
        *bl++ = data[j];
      ldpc->next_bit();
    }
  }

  void check_node_update()
  {
    TYPE *bl = bnl;
    cnv[0] = alg.sign(alg.one(), bnv[0]);
    cnl[0] = *bl++;
    cnc[0] = 1;
    for (int i = 1; i < R; ++i) {
      cnv[i] = alg.sign(alg.sign(alg.one(), bnv[i-1]), bnv[i]);
      cnl[CNL*i] = *bl++;
      cnl[CNL*i+1] = *bl++;
      cnc[i] = 2;
    }
    ldpc->first_bit();
    for (int j = 0; j < K; ++j) {
      int *acc_pos = ldpc->acc_pos();
      int bit_deg = ldpc->bit_deg();
      for (int n = 0; n < bit_deg; ++n) {
        int i = acc_pos[n];
        cnv[i] = alg.sign(cnv[i], bnv[j+R]);
        cnl[CNL*i+cnc[i]++] = *bl++;
      }
      ldpc->next_bit();
    }
    for (int i = 0; i < R; ++i)
      alg.finalp(cnl+CNL*i, cnc[i]);
  }

  void bit_node_update(TYPE *data, TYPE *parity)
  {
    TYPE *bl = bnl;
    bnv[0] = alg.add(parity[0], alg.add(cnl[0], cnl[CNL]));
    *bl = alg.update(*bl, alg.add(parity[0], cnl[CNL])); ++bl;
    *bl = alg.update(*bl, alg.add(parity[0], cnl[0])); ++bl;
    cnc[0] = 1;
    for (int i = 1; i < R-1; ++i) {
      bnv[i] = alg.add(parity[i], alg.add(cnl[CNL*i+1], cnl[CNL*(i+1)]));
      *bl = alg.update(*bl, alg.add(parity[i], cnl[CNL*(i+1)])); ++bl;
      *bl = alg.update(*bl, alg.add(parity[i], cnl[CNL*i+1])); ++bl;
      cnc[i] = 2;
    }
    bnv[R-1] = alg.add(parity[R-1], cnl[CNL*(R-1)+1]);
    *bl = alg.update(*bl, parity[R-1]); ++bl;
    cnc[R-1] = 2;
    ldpc->first_bit();
    for (int j = 0; j < K; ++j) {
      int *acc_pos = ldpc->acc_pos();
      int bit_deg = ldpc->bit_deg();
      TYPE inp[bit_deg];
      for (int n = 0; n < bit_deg; ++n) {
        int i = acc_pos[n];
        inp[n] = cnl[CNL*i+cnc[i]++];
      }
      TYPE out[bit_deg];
      CODE::exclusive_reduce(inp, out, bit_deg, alg.add);
      bnv[j+R] = alg.add(data[j], alg.add(out[0], inp[0]));
      for (int n = 0; n < bit_deg; ++n, ++bl)
        *bl = alg.update(*bl, alg.add(data[j], out[n]));
      ldpc->next_bit();
    }
  }

  bool hard_decision(int blocks)
  {
    for (int i = 0; i < R; ++i)
      if (alg.bad(cnv[i], blocks))
        return true;
    return false;
  }

  void update_user(TYPE *data, TYPE *parity)
  {
    for (int i = 0; i < R; ++i)
      parity[i] = bnv[i];
    for (int i = 0; i < K; ++i)
      data[i] = bnv[i+R];
  }

public:

  LDPCDecoder(LDPCInterface *it)
  {
    ldpc = it->clone();
    N = ldpc->code_len();
    K = ldpc->data_len();
    R = N - K;
    CNL = ldpc->links_max_cn();
    LT = ldpc->links_total();
    int num = LT + N + R * CNL + R;
    aligned_buffer = aligned_alloc(sizeof(TYPE), sizeof(TYPE) * num);
    TYPE *ptr = reinterpret_cast<TYPE *>(aligned_buffer);
    bnl = ptr; ptr += LT;
    bnv = ptr; ptr += N;
    cnl = ptr; ptr += R * CNL;
    cnv = ptr; ptr += R;
    cnc = new uint8_t[R];
  }

  int operator()(TYPE *data, TYPE *parity, int trials = 50, int blocks = 1)
  {
    bit_node_init(data, parity);
    check_node_update();
    if (!hard_decision(blocks))
      return trials;
    --trials;
    bit_node_update(data, parity);
    check_node_update();
    while (hard_decision(blocks) && --trials >= 0) {
      bit_node_update(data, parity);
      check_node_update();
    }
    update_user(data, parity);
    return trials;
  }

  ~LDPCDecoder()
  {
    free(aligned_buffer);
    delete[] cnc;
    delete ldpc;
  }
};

#endif
