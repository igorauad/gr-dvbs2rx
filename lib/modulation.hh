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

#ifndef MODULATION_HH
#define MODULATION_HH

template <typename TYPE, typename CODE>
struct Modulation {
  typedef TYPE complex_type;
  typedef typename TYPE::value_type value_type;
  typedef CODE code_type;

  virtual int
  bits() = 0;
  virtual void
  hard(code_type *, complex_type) = 0;
  virtual void
  soft(code_type *, complex_type, value_type) = 0;
  virtual complex_type
  map(code_type *) = 0;
  virtual ~Modulation() = default;
};

#endif
