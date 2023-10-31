/* -*- c++ -*- */
/*
 * Copyright (c) 2022 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_BITSET256_H
#define INCLUDED_DVBS2RX_BITSET256_H

#include <gnuradio/dvbs2rx/api.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <ios>
#include <ostream>
#include <stdexcept>

namespace gr {
namespace dvbs2rx {

class DVBS2RX_API bitset256
{
private:
    constexpr static std::size_t _size = 256;
    constexpr static std::size_t _n_words = 4;
    std::array<uint64_t, _n_words> _data;

public:
    bitset256() { memset(_data.data(), 0, sizeof(_data)); }

    bitset256(uint64_t val)
    {
        _data[0] = val;
        memset(&_data[1], 0, (_n_words - 1) * sizeof(uint64_t));
    }

    bitset256(const bitset256& other)
    {
        memcpy(_data.data(), other.data(), _n_words * sizeof(uint64_t));
    }

    const uint64_t* data() const { return _data.data(); }

    uint64_t get_word(std::size_t i) const { return _data[i]; }

    void set_word(std::size_t i, uint64_t value) { _data[i] = value; }

    void set(std::size_t pos)
    {
        std::size_t i_word = pos / 64;
        std::size_t i_bit = pos % 64;
        _data[i_word] |= (1ULL << i_bit);
    }

    bool test(std::size_t pos) const
    {
        std::size_t i_word = pos / 64;
        std::size_t i_bit = pos % 64;
        return (_data[i_word] & (1ULL << i_bit)) != 0;
    }

    bool operator[](std::size_t pos) const { return test(pos); }

    bitset256 operator<<(std::size_t pos) const noexcept
    {
        if (pos == 0)
            return *this; // no shift

        bitset256 result;
        if (pos >= 256)
            return result; // full shift

        std::size_t i_word = pos / 64;
        std::size_t i_bit = pos % 64;
        for (std::size_t i = 0; i < _n_words; i++) {
            if (i < i_word) {
                result.set_word(i, 0);
            } else if (i_bit > 0 && i > i_word) {
                result.set_word(i,
                                (_data[i - i_word] << i_bit) ^
                                    (_data[i - i_word - 1] >> (64 - i_bit)));
            } else {
                result.set_word(i, _data[i - i_word] << i_bit);
            }
        }
        return result;
    }

    bitset256& operator^=(const bitset256& other) noexcept
    {
        for (std::size_t i = 0; i < 4; i++)
            _data[i] ^= other.get_word(i);
        return *this;
    }

    bitset256& operator|=(const bitset256& other) noexcept
    {
        for (std::size_t i = 0; i < 4; i++)
            _data[i] |= other.get_word(i);
        return *this;
    }

    bool operator==(const bitset256& rhs) const noexcept
    {
        for (std::size_t i = 0; i < 4; i++)
            if (_data[i] != rhs.get_word(i))
                return false;
        return true;
    }

    uint64_t to_ulong() const
    {
        if (_data[1] != 0 || _data[2] != 0 || _data[3] != 0)
            throw std::overflow_error("bitset256::to_ulong");
        return _data[0];
    }

    uint8_t get_byte(std::size_t i_byte) const
    {
        std::size_t i_word = i_byte / 8;
        std::size_t i_bit = (i_byte % 8) * 8;
        return (_data[i_word] >> i_bit) & 0xFF;
    }
};


inline bitset256 operator^(const bitset256& lhs, const bitset256& rhs) noexcept
{
    bitset256 result;
    for (std::size_t i = 0; i < 4; i++)
        result.set_word(i, lhs.get_word(i) ^ rhs.get_word(i));
    return result;
}


inline bitset256 operator&(const bitset256& lhs, const bitset256& rhs) noexcept
{
    bitset256 result;
    for (std::size_t i = 0; i < 4; i++)
        result.set_word(i, lhs.get_word(i) & rhs.get_word(i));
    return result;
}

inline std::ostream& operator<<(std::ostream& os, const bitset256& x)
{
    os << std::hex;
    for (int i = 3; i >= 0; i--)
        os << x.get_word(i);
    os << std::dec;
    return os;
}

typedef bitset256 bitset256_t;

} // namespace dvbs2rx
} // namespace gr

#endif