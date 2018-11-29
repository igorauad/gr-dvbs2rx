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

#ifndef ALGORITHMS_HH
#define ALGORITHMS_HH

#include "generic.hh"
#include "exclusive_reduce.hh"

template <typename VALUE, int WIDTH>
struct SelfCorrectedUpdate<SIMD<VALUE, WIDTH>>
{
  typedef SIMD<VALUE, WIDTH> TYPE;
  static TYPE update(TYPE a, TYPE b)
  {
    return vreinterpret<TYPE>(vand(vmask(b), vorr(vceqz(a), veor(vcgtz(a), vcltz(b)))));
  }
};

template <typename VALUE, int WIDTH, typename UPDATE>
struct MinSumAlgorithm<SIMD<VALUE, WIDTH>, UPDATE>
{
  typedef SIMD<VALUE, WIDTH> TYPE;
  static TYPE one()
  {
    return vdup<TYPE>(1);
  }
  static TYPE min(TYPE a, TYPE b)
  {
    return vmin(a, b);
  }
  static TYPE sign(TYPE a, TYPE b)
  {
    return vsign(a, b);
  }
  static void finalp(TYPE *links, int cnt)
  {
    TYPE mags[cnt], mins[cnt];
    for (int i = 0; i < cnt; ++i)
      mags[i] = vabs(links[i]);
    CODE::exclusive_reduce(mags, mins, cnt, min);

    TYPE signs[cnt];
    CODE::exclusive_reduce(links, signs, cnt, sign);

    for (int i = 0; i < cnt; ++i)
      links[i] = sign(mins[i], signs[i]);
  }
  static TYPE add(TYPE a, TYPE b)
  {
    return vadd(a, b);
  }
  static bool bad(TYPE v, int blocks)
  {
    auto tmp = vcgtz(v);
    for (int i = 0; i < blocks; ++i)
      if (!tmp.v[i])
        return true;
    return false;
  }
  static TYPE update(TYPE a, TYPE b)
  {
    return UPDATE::update(a, b);
  }
};

template <int WIDTH, typename UPDATE>
struct MinSumAlgorithm<SIMD<int8_t, WIDTH>, UPDATE>
{
  typedef int8_t VALUE;
  typedef SIMD<VALUE, WIDTH> TYPE;
  static TYPE one()
  {
    return vdup<TYPE>(1);
  }
  static TYPE min(TYPE a, TYPE b)
  {
    return vmin(a, b);
  }
  static TYPE sign(TYPE a, TYPE b)
  {
    return vsign(a, b);
  }
  static TYPE eor(TYPE a, TYPE b)
  {
    return vreinterpret<TYPE>(veor(vmask(a), vmask(b)));
  }
  static void finalp(TYPE *links, int cnt)
  {
    TYPE mags[cnt], mins[cnt];
    for (int i = 0; i < cnt; ++i)
      mags[i] = vqabs(links[i]);
    CODE::exclusive_reduce(mags, mins, cnt, min);

    TYPE signs[cnt];
    CODE::exclusive_reduce(links, signs, cnt, eor);
    for (int i = 0; i < cnt; ++i)
      signs[i] = vreinterpret<TYPE>(vorr(vmask(signs[i]), vmask(vdup<TYPE>(127))));

    for (int i = 0; i < cnt; ++i)
      links[i] = sign(mins[i], signs[i]);
  }
  static TYPE add(TYPE a, TYPE b)
  {
    return vqadd(a, b);
  }
  static bool bad(TYPE v, int blocks)
  {
    auto tmp = vcgtz(v);
    for (int i = 0; i < blocks; ++i)
      if (!tmp.v[i])
        return true;
    return false;
  }
  static TYPE update(TYPE a, TYPE b)
  {
    return UPDATE::update(a, b);
  }
};

template <typename VALUE, int WIDTH, typename UPDATE, int FACTOR>
struct MinSumCAlgorithm<SIMD<VALUE, WIDTH>, UPDATE, FACTOR>
{
  typedef SIMD<VALUE, WIDTH> TYPE;
  static TYPE one()
  {
    return vdup<TYPE>(1);
  }
  static TYPE sign(TYPE a, TYPE b)
  {
    return vsign(a, b);
  }
  static TYPE correction_factor(TYPE a, TYPE b)
  {
    TYPE apb = vabs(vadd(a, b));
    TYPE apb2 = vadd(apb, apb);
    TYPE amb = vabs(vsub(a, b));
    TYPE amb2 = vadd(amb, amb);
    TYPE factor2 = vdup<TYPE>(FACTOR * 2);
    auto pc = vmask(vdup<TYPE>(VALUE(FACTOR) / VALUE(2)));
    auto nc = vmask(vdup<TYPE>(-VALUE(FACTOR) / VALUE(2)));
    pc = vand(pc, vand(vcgt(factor2, apb), vcgt(amb, apb2)));
    nc = vand(nc, vand(vcgt(factor2, amb), vcgt(apb, amb2)));
    return vreinterpret<TYPE>(vorr(pc, nc));
  }
  static TYPE minc(TYPE a, TYPE b)
  {
    TYPE m = vmin(vabs(a), vabs(b));
    TYPE x = vsign(vsign(m, a), b);
    x = vadd(x, correction_factor(a, b));
    return x;
  }
  static void finalp(TYPE *links, int cnt)
  {
    TYPE tmp[cnt];
    CODE::exclusive_reduce(links, tmp, cnt, minc);
    for (int i = 0; i < cnt; ++i)
      links[i] = tmp[i];
  }
  static TYPE add(TYPE a, TYPE b)
  {
    return vadd(a, b);
  }
  static bool bad(TYPE v, int blocks)
  {
    auto tmp = vcgtz(v);
    for (int i = 0; i < blocks; ++i)
      if (!tmp.v[i])
        return true;
    return false;
  }
  static TYPE update(TYPE a, TYPE b)
  {
    return UPDATE::update(a, b);
  }
};

template <int WIDTH, typename UPDATE, int FACTOR>
struct MinSumCAlgorithm<SIMD<int8_t, WIDTH>, UPDATE, FACTOR>
{
  typedef int8_t VALUE;
  typedef SIMD<VALUE, WIDTH> TYPE;
  static TYPE one()
  {
    return vdup<TYPE>(1);
  }
  static TYPE sign(TYPE a, TYPE b)
  {
    return vsign(a, b);
  }
  static TYPE correction_factor(TYPE a, TYPE b)
  {
    TYPE apb = vqabs(vqadd(a, b));
    TYPE apb2 = vqadd(apb, apb);
    TYPE amb = vqabs(vqsub(a, b));
    TYPE amb2 = vqadd(amb, amb);
    TYPE factor2 = vdup<TYPE>(FACTOR * 2);
    auto pc = vmask(vdup<TYPE>(VALUE(FACTOR) / VALUE(2)));
    auto nc = vmask(vdup<TYPE>(-VALUE(FACTOR) / VALUE(2)));
    pc = vand(pc, vand(vcgt(factor2, apb), vcgt(amb, apb2)));
    nc = vand(nc, vand(vcgt(factor2, amb), vcgt(apb, amb2)));
    return vreinterpret<TYPE>(vorr(pc, nc));
  }
  static TYPE minc(TYPE a, TYPE b)
  {
    TYPE m = vmin(vqabs(a), vqabs(b));
    TYPE x = vsign(vsign(m, a), b);
    x = vqadd(x, correction_factor(a, b));
    return x;
  }
  static void finalp(TYPE *links, int cnt)
  {
    TYPE tmp[cnt];
    CODE::exclusive_reduce(links, tmp, cnt, minc);
    for (int i = 0; i < cnt; ++i)
      links[i] = tmp[i];
  }
  static TYPE add(TYPE a, TYPE b)
  {
    return vqadd(a, b);
  }
  static bool bad(TYPE v, int blocks)
  {
    auto tmp = vcgtz(v);
    for (int i = 0; i < blocks; ++i)
      if (!tmp.v[i])
        return true;
    return false;
  }
  static TYPE update(TYPE a, TYPE b)
  {
    return UPDATE::update(a, b);
  }
};

#endif
