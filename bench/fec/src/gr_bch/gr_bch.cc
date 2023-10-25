/* -*- c++ -*- */
/*
 * Copyright (c) 2023 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gr_bch.h"
#include <cassert>
#include <cstring>

namespace gr {
namespace dvbs2 {

template <typename T>
void bit_vector_to_bitset(const std::vector<T>& in_vec,
                          std::bitset<MAX_BCH_PARITY_BITS>& out_bitset,
                          unsigned int len)
{
    for (unsigned int i = 0; i < len; i++)
        out_bitset[i] = in_vec[i];
}

void multiply_poly(const std::vector<uint8_t>& in_a,
                   const std::vector<uint8_t>& in_b,
                   std::vector<uint8_t>& out)
{
    int len_a = in_a.size();
    int len_b = in_b.size();
    out.clear();
    out.resize(len_a + len_b - 1);
    for (int i = 0; i < len_a; i++) {
        for (int j = 0; j < len_b; j++) {
            out[i + j] ^= in_a[i] & in_b[j];
        }
    }
}

void unpacked_to_packed(const std::vector<int>& in_bits, std::vector<uint8_t>& out_bytes)
{
    if (in_bits.size() % 8 != 0)
        throw std::runtime_error("Input bits must be a multiple of 8");
    if (in_bits.size() / 8 != out_bytes.size())
        throw std::runtime_error("Input and output sizes do not match");
    memset(out_bytes.data(), 0, out_bytes.size());
    for (unsigned int j = 0; j < in_bits.size(); j++) {
        out_bytes[j / 8] |= in_bits[j] << (7 - (j % 8));
    }
}

void packed_to_unpacked(const std::vector<uint8_t>& in_bytes, std::vector<int>& out_bits)
{
    if (in_bytes.size() * 8 != out_bits.size())
        throw std::runtime_error("Input and output sizes do not match");
    for (unsigned int j = 0; j < out_bits.size(); j++) {
        out_bits[j] = (in_bytes[j / 8] >> (7 - (j % 8))) & 1;
    }
}

GrBchEncoder::GrBchEncoder(int k, int n, int t) : m_K(k), m_N(n), m_t(t), m_parity(n - k)
{
    const bool normal_fecframe = (n >= 16200);
    compute_gen_poly(normal_fecframe);
    compute_crc_table();
}

void GrBchEncoder::compute_gen_poly(bool normal_fecframe)
{
    // Normal FECFRAME minimal polynomials (Table 6a)
    const std::vector<std::vector<uint8_t>> normal_min_poly = {
        { 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, // g1(x)
        { 1, 1, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 }, // g2(x)
        { 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1 }, // g3(x)
        { 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1 }, // g4(x)
        { 1, 1, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1 }, // g5(x)
        { 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1 }, // g6(x)
        { 1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1 }, // g7(x)
        { 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 1 }, // g8(x)
        { 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1 }, // g9(x)
        { 1, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1 }, // g10(x)
        { 1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1 }, // g11(x)
        { 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1 }  // g12(x)
    };

    // Short FECFRAME minimal polynomials (Table 6b)
    const std::vector<std::vector<uint8_t>> short_min_poly = {
        { 1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, // g1(x)
        { 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1 }, // g2(x)
        { 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1 }, // g3(x)
        { 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1 }, // g4(x)
        { 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1 }, // g5(x)
        { 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1 }, // g6(x)
        { 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1 }, // g7(x)
        { 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1 }, // g8(x)
        { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1 }, // g9(x)
        { 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1 }, // g10(x)
        { 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1 }, // g11(x)
        { 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 0, 1, 1 }  // g12(x)
    };

    const std::vector<std::vector<uint8_t>> min_poly =
        normal_fecframe ? normal_min_poly : short_min_poly;

    // The generator polynomial is the product of the first t minimal polynomials
    std::vector<uint8_t> gen_poly_vec = { 1 };
    for (unsigned int i = 0; i < m_t; i++) {
        std::vector<uint8_t> prev_gen_poly_vec = gen_poly_vec;
        multiply_poly(min_poly[i], prev_gen_poly_vec, gen_poly_vec);
    }
    assert(gen_poly_vec.size() == m_parity + 1);

    // Convert polynomial vector to bitset
    bit_vector_to_bitset(gen_poly_vec, m_gen_poly, m_parity);
}

void GrBchEncoder::compute_crc_table()
{
    // See http://www.sunshine2k.de/articles/coding/crc/understanding_crc.html
    for (int dividend = 0; dividend < 256; dividend++) {
        std::bitset<MAX_BCH_PARITY_BITS> shift_reg(dividend);
        shift_reg <<= m_parity - 8; // put dividend byte on the register's MSB
        for (unsigned char bit = 0; bit < 8; bit++) {
            if (shift_reg[m_parity - 1]) {
                shift_reg <<= 1;
                shift_reg ^= m_gen_poly;
            } else {
                shift_reg <<= 1;
            }
        }
        m_crc_table[dividend] = shift_reg;
    }
}

void GrBchEncoder::encode(const std::vector<int>& ref_bits, std::vector<int>& enc_bits)
{
    const int* in = ref_bits.data();
    int* out = enc_bits.data();
    std::bitset<MAX_BCH_PARITY_BITS> crc; // parity bits
    // NOTE: call the parity bits the CRC for brevity. The computations of cyclic
    // redundancy check (CRC) and parity bits of a cyclic code are equivalent.

    // Systematic bits
    memcpy(out, in, sizeof(int) * m_K);
    out += m_K;

    // Parity bits
    for (int i_byte = 0; i_byte < (int)m_K / 8; i_byte++) { // byte-by-byte
        // Pack the next 8 bits to form the next input (message) byte
        unsigned char in_byte = 0;
        for (int i_bit = 7; i_bit >= 0; i_bit--) {
            in_byte |= *in++ << i_bit;
        }

        // The CRC "register" holds the remainder from the previous input message
        // byte. This remainder has length m_parity bits and, therefore, leaks into
        // the m_parity bits following it in the remainder computation. The first byte
        // within these m_parity bits (the most significant byte or MSB) aligns with
        // the next input message byte (in_byte) that we are about to process. Hence,
        // get the MSB in the CRC "register" and XOR it with the the next input
        // message byte. The result is the byte whose remainder is to be computed
        // through a table look-up, namely the dividend (refer to Section 5 in
        // http://www.sunshine2k.de/articles/coding/crc/understanding_crc.html).
        unsigned char msb_crc = 0;
        for (int n = 1; n <= 8; n++) {
            msb_crc |= crc[m_parity - n] << (8 - n);
        }
        unsigned char dividend = (msb_crc ^ in_byte);

        // The first byte from the previous remainder (i.e., msb_crc) is already
        // included in the dividend. On the other hand, the remaining (m_parity - 8)
        // bits from the previous remainder are not. These must be added (mod-2) back
        // in the end. So, next, look-up the remainder from the dividend byte alone
        // and add back the (m_parity - 8) lower bits from the previous remainder:
        crc = (crc << 8) ^ m_crc_table[dividend];
    }

    // Serialize the parity bits to the output
    for (int n = m_parity - 1; n >= 0; n--) {
        *out++ = crc[n];
    }
}

GrBchDecoder::GrBchDecoder(int k, int n, int t)
    : m_K(k),
      m_N(n),
      m_t(t),
      m_gf_normal(new GF_NORMAL()),
      m_gf_short(new GF_SHORT()),
      m_dvbs2rx_decoder_n12(new BCH_NORMAL_12()),
      m_dvbs2rx_decoder_n10(new BCH_NORMAL_10()),
      m_dvbs2rx_decoder_n8(new BCH_NORMAL_8()),
      m_dvbs2rx_decoder_s12(new BCH_SHORT_12())
{
    for (int i = 0; i < m_packed_code.size(); i++) {
        m_packed_code[i] = 0;
    }
    for (int i = 0; i < m_packed_parity.size(); i++) {
        m_packed_parity[i] = 0;
    }
}

void GrBchDecoder::pack_bits(const std::vector<int>& in_bits)
{
    assert(in_bits.size() == m_N);
    for (unsigned int j = 0; j < m_K; j++) {
        CODE::set_be_bit(m_packed_code.data(), j, in_bits[j]);
    }
    for (unsigned int j = 0; j < m_N - m_K; j++) {
        CODE::set_be_bit(m_packed_parity.data(), j, in_bits[j + m_K]);
    }
}

void GrBchDecoder::unpack_bits(std::vector<int>& dec_bits)
{
    assert(dec_bits.size() == m_K);
    for (unsigned int j = 0; j < m_K; j++) {
        dec_bits[j] = CODE::get_be_bit(m_packed_code.data(), j);
    }
}

void GrBchDecoder::decode(const std::vector<int>& in_bits, std::vector<int>& dec_bits)
{
    pack_bits(in_bits);
    if (m_N >= 16200) {
        if (m_t == 12) {
            (*m_dvbs2rx_decoder_n12)(
                m_packed_code.data(), m_packed_parity.data(), 0, 0, m_K);
        } else if (m_t == 10) {
            (*m_dvbs2rx_decoder_n10)(
                m_packed_code.data(), m_packed_parity.data(), 0, 0, m_K);
        } else {
            (*m_dvbs2rx_decoder_n8)(
                m_packed_code.data(), m_packed_parity.data(), 0, 0, m_K);
        }
    } else {
        (*m_dvbs2rx_decoder_s12)(m_packed_code.data(), m_packed_parity.data(), 0, 0, m_K);
    }
    unpack_bits(dec_bits);
}


NewBchCodec::NewBchCodec(int N, int t)
    : m_gf(N >= 16200 ? 0b10000000000101101 : 0b100000000101011),
      m_bch(&m_gf, t, N),
      m_packed_msg(m_bch.get_k() / 8),
      m_packed_codeword(m_bch.get_n() / 8)
{
}

void NewBchCodec::encode(const std::vector<int>& ref_bits, std::vector<int>& enc_bits)
{
    if (ref_bits.size() != m_bch.get_k())
        throw std::runtime_error("Input size does not match");
    if (enc_bits.size() != m_bch.get_n())
        throw std::runtime_error("Output size does not match");
    unpacked_to_packed(ref_bits, m_packed_msg);
    m_bch.encode(m_packed_msg.data(), m_packed_codeword.data());
    packed_to_unpacked(m_packed_codeword, enc_bits);
}

void NewBchCodec::decode(const std::vector<int>& in_bits, std::vector<int>& dec_bits)
{
    if (in_bits.size() != m_bch.get_n())
        throw std::runtime_error("Input size does not match");
    if (dec_bits.size() != m_bch.get_k())
        throw std::runtime_error("Output size does not match");
    unpacked_to_packed(in_bits, m_packed_codeword);
    m_bch.decode(m_packed_codeword.data(), m_packed_msg.data());
    packed_to_unpacked(m_packed_msg, dec_bits);
}

} // namespace dvbs2
} // namespace gr