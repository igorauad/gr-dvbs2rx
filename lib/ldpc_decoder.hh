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

#include "exclusive_reduce.hh"

template <typename TYPE, int FACTOR>
struct SelfCorrectedMinSumAlgorithm
{
  static TYPE min(TYPE a, TYPE b)
  {
    return std::min(a, b);
  }

  static TYPE mul(TYPE a, TYPE b)
  {
    return a * b;
  }

  static void finalp(TYPE *links, int cnt)
  {
    TYPE blmags[cnt], mins[cnt];
    for (int i = 0; i < cnt; ++i)
      blmags[i] = std::abs(links[i]);
    CODE::exclusive_reduce(blmags, mins, cnt, min);

    TYPE blsigns[cnt], signs[cnt];
    for (int i = 0; i < cnt; ++i)
      blsigns[i] = links[i] < TYPE(0) ? TYPE(-1) : TYPE(1);
    CODE::exclusive_reduce(blsigns, signs, cnt, mul);

    for (int i = 0; i < cnt; ++i)
      links[i] = signs[i] * mins[i];
  }

  static TYPE add(TYPE a, TYPE b)
  {
    return a + b;
  }

  static TYPE update(TYPE a, TYPE b)
  {
    return (a == TYPE(0) || (a < TYPE(0)) == (b < TYPE(0))) ? b : TYPE(0);
  }
};

template <int FACTOR>
struct SelfCorrectedMinSumAlgorithm<int8_t, FACTOR>
{
  static int8_t add(int8_t a, int8_t b)
  {
    int x = int(a) + int(b);
    x = std::min<int>(std::max<int>(x, -128), 127);
    return x;
  }

  static uint8_t min(uint8_t a, uint8_t b)
  {
    return std::min(a, b);
  }

  static int8_t xor_(int8_t a, int8_t b)
  {
    return a ^ b;
  }

  static uint8_t abs(int8_t a)
  {
    return std::abs<int>(a);
  }

  static int8_t sign(int8_t a, int8_t b)
  {
    return b < 0 ? -a : b > 0 ? a : 0;
  }

  static void finalp(int8_t *links, int cnt)
  {
    uint8_t mags[cnt], mins[cnt];
    for (int i = 0; i < cnt; ++i)
      mags[i] = abs(links[i]);
    CODE::exclusive_reduce(mags, mins, cnt, min);
    for (int i = 0; i < cnt; ++i)
      mins[i] = std::min<uint8_t>(mins[i], 127);

    int8_t signs[cnt];
    CODE::exclusive_reduce(links, signs, cnt, xor_);
    for (int i = 0; i < cnt; ++i)
      signs[i] |= 127;

    for (int i = 0; i < cnt; ++i)
      links[i] = sign(mins[i], signs[i]);
  }

  static int8_t update(int8_t a, int8_t b)
  {
    return (a == 0 || ((a ^ b) & 128) == 0) ? b : 0;
  }
};

template <typename TYPE>
struct LDPCInterface
{
  virtual int code_len() = 0;
  virtual int data_len() = 0;
  virtual int decode(TYPE *, TYPE *, int) = 0;
  virtual ~LDPCInterface() = default;
};

template <typename TABLE, typename TYPE, int FACTOR>
class LDPC : public LDPCInterface<TYPE>
{
  typedef SelfCorrectedMinSumAlgorithm<TYPE, FACTOR> ALG;

  static const int M = TABLE::M;
  static const int N = TABLE::N;
  static const int K = TABLE::K;
  static const int R = N-K;
  static const int q = R/M;
  static const int CNL = TABLE::LINKS_MAX_CN;

  int acc_pos[TABLE::DEG_MAX];
  const int *row_ptr;
  int bit_deg;
  int grp_cnt;
  int grp_num;
  int grp_len;
  TYPE bnl[TABLE::LINKS_TOTAL];
  TYPE bnv[N];
  TYPE cnl[R * CNL];
  int8_t cnv[R];
  uint8_t cnc[R];
  ALG alg;

  int signum(TYPE v)
  {
    return (v > TYPE(0)) - (v < TYPE(0));
  }

  void next_group()
  {
    if (grp_cnt >= grp_len) {
      grp_len = TABLE::LEN[grp_num];
      bit_deg = TABLE::DEG[grp_num];
      grp_cnt = 0;
      ++grp_num;
    }
    for (int i = 0; i < bit_deg; ++i)
      acc_pos[i] = row_ptr[i];
    row_ptr += bit_deg;
    ++grp_cnt;
  }

  void next_bit()
  {
    for (int i = 0; i < bit_deg; ++i)
      acc_pos[i] += q;
    for (int i = 0; i < bit_deg; ++i)
      acc_pos[i] %= R;
  }

  void first_group()
  {
    grp_num = 0;
    grp_len = 0;
    grp_cnt = 0;
    row_ptr = TABLE::POS;
    next_group();
  }

  void bit_node_init(TYPE *data, TYPE *parity)
  {
    for (int i = 0; i < R; ++i)
      bnv[i] = parity[i];
    for (int i = 0; i < K; ++i)
      bnv[i+R] = data[i];

    TYPE *bl = bnl;
    for (int i = 0; i < R-1; ++i) {
      *bl++ = parity[i];
      *bl++ = parity[i];
    }
    *bl++ = parity[R-1];
    first_group();
    for (int j = 0; j < K; j += M) {
      for (int m = 0; m < M; ++m) {
        for (int n = 0; n < bit_deg; ++n)
          *bl++ = data[j+m];
        next_bit();
      }
      next_group();
    }
  }

  void check_node_update()
  {
    cnv[0] = signum(bnv[0]);
    for (int i = 1; i < R; ++i)
      cnv[i] = signum(bnv[i-1]) * signum(bnv[i]);

    TYPE *bl = bnl;
    cnl[0] = *bl++;
    cnc[0] = 1;
    for (int i = 1; i < R; ++i) {
      cnl[CNL*i] = *bl++;
      cnl[CNL*i+1] = *bl++;
      cnc[i] = 2;
    }
    first_group();
    for (int j = 0; j < K; j += M) {
      for (int m = 0; m < M; ++m) {
        for (int n = 0; n < bit_deg; ++n) {
          int i = acc_pos[n];
          cnv[i] *= signum(bnv[j+m+R]);
          cnl[CNL*i+cnc[i]++] = *bl++;
        }
        next_bit();
      }
      next_group();
    }
    for (int i = 0; i < R; ++i)
      alg.finalp(cnl+CNL*i, cnc[i]);
    if (0) {
      TYPE min = 0, max = 0;
      for (int i = 0; i < R * CNL; ++i) {
        min = std::min(min, cnl[i]);
        max = std::max(max, cnl[i]);
      }
      std::cerr << "cnl: min = " << +min << " max = " << +max << std::endl;
    }
  }

  void bit_node_update(TYPE *data, TYPE *parity)
  {
    bnv[0] = alg.add(parity[0], alg.add(cnl[0], cnl[CNL]));
    for (int i = 1; i < R-1; ++i)
      bnv[i] = alg.add(parity[i], alg.add(cnl[CNL*i+1], cnl[CNL*(i+1)]));
    bnv[R-1] = alg.add(parity[R-1], cnl[CNL*(R-1)+1]);

    TYPE *bl = bnl;
    cnc[0] = 1;
    for (int i = 1; i < R; ++i)
      cnc[i] = 2;
    *bl = alg.update(*bl, alg.add(parity[0], cnl[CNL])); ++bl;
    *bl = alg.update(*bl, alg.add(parity[0], cnl[0])); ++bl;
    for (int i = 1; i < R-1; ++i) {
      *bl = alg.update(*bl, alg.add(parity[i], cnl[CNL*(i+1)])); ++bl;
      *bl = alg.update(*bl, alg.add(parity[i], cnl[CNL*i+1])); ++bl;
    }
    *bl = alg.update(*bl, parity[R-1]); ++bl;
    first_group();
    for (int j = 0; j < K; j += M) {
      for (int m = 0; m < M; ++m) {
        TYPE inp[bit_deg];
        for (int n = 0; n < bit_deg; ++n) {
          int i = acc_pos[n];
          inp[n] = cnl[CNL*i+cnc[i]++];
        }
        TYPE out[bit_deg];
        CODE::exclusive_reduce(inp, out, bit_deg, alg.add);
        bnv[j+m+R] = alg.add(data[j+m], alg.add(out[0], inp[0]));
        for (int n = 0; n < bit_deg; ++n, ++bl)
          *bl = alg.update(*bl, alg.add(data[j+m], out[n]));
        next_bit();
      }
      next_group();
    }
    if (0) {
      TYPE min = 0, max = 0;
      for (int i = 0; i < TABLE::LINKS_TOTAL; ++i) {
        min = std::min(min, bnv[i]);
        max = std::max(max, bnv[i]);
      }
      std::cerr << "bnl: min = " << +min << " max = " << +max << std::endl;
    }
    if (0) {
      TYPE min = 0, max = 0;
      for (int i = 0; i < N; ++i) {
        min = std::min(min, bnv[i]);
        max = std::max(max, bnv[i]);
      }
      std::cerr << "bnv: min = " << +min << " max = " << +max << std::endl;
    }
    if (0) {
      static int count;
      std::cout << count++;
      for (int i = 0; i < N; ++i)
        std::cout << " " << +bnv[i];
      std::cout << std::endl;
    }
  }

  bool hard_decision()
  {
    for (int i = 0; i < R; ++i)
      if (cnv[i] <= 0)
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

  int code_len()
  {
    return N;
  }

  int data_len()
  {
    return K;
  }

  int decode(TYPE *data, TYPE *parity, int trials = 50)
  {
    bit_node_init(data, parity);
    check_node_update();
    if (!hard_decision()) {
      return trials;
    }
    --trials;
    bit_node_update(data, parity);
    check_node_update();
    while (hard_decision() && --trials >= 0) {
      bit_node_update(data, parity);
      check_node_update();
    }
    update_user(data, parity);
    return trials;
  }
};

#endif

