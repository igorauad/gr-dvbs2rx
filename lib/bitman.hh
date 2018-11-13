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

#ifndef BITMAN_HH
#define BITMAN_HH

namespace CODE {

  void
  xor_be_bit(uint8_t *buf, int pos, bool val)
  {
    buf[pos/8] ^= val<<(7-pos%8);
  }

  void
  xor_le_bit(uint8_t *buf, int pos, bool val)
  {
    buf[pos/8] ^= val<<(pos%8);
  }

  void
  set_be_bit(uint8_t *buf, int pos, bool val)
  {
    buf[pos/8] = (~(1<<(7-pos%8))&buf[pos/8])|(val<<(7-pos%8));
  }

  void
  set_le_bit(uint8_t *buf, int pos, bool val)
  {
    buf[pos/8] = (~(1<<(pos%8))&buf[pos/8])|(val<<(pos%8));
  }

  bool
  get_be_bit(uint8_t *buf, int pos)
  {
    return (buf[pos/8]>>(7-pos%8))&1;
  }

  bool
  get_le_bit(uint8_t *buf, int pos)
  {
    return (buf[pos/8]>>(pos%8))&1;
  }
}

#endif

