/* -*- c++ -*- */
/*
 * Copyright 2018 Ahmet Inan, Ron Economos.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MODULATION_HH
#define MODULATION_HH

template <typename TYPE, typename CODE>
struct Modulation {
    typedef TYPE complex_type;
    typedef typename TYPE::value_type value_type;
    typedef CODE code_type;

    virtual int bits() = 0;
    virtual void hard(code_type*, complex_type) = 0;
    virtual void soft(code_type*, complex_type, value_type) = 0;
    virtual complex_type map(code_type*) = 0;
    virtual ~Modulation() = default;
};

#endif
