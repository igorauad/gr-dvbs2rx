/* -*- c++ -*- */
/*
 * Copyright (c) 2023 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef INCLUDED_GR_BCH_H
#define INCLUDED_GR_BCH_H

#include "api.h"
#include "bch.h"
#include "bose_chaudhuri_hocquenghem_decoder.hh"
#include "galois_field.hh"
#include <bitset>
#include <memory>
#include <vector>

#define MAX_BCH_PARITY_BITS 192

namespace gr {
namespace dvbs2 {

typedef CODE::GaloisField<16, 0b10000000000101101, uint16_t> GF_NORMAL;
typedef CODE::GaloisField<15, 0b1000000000101101, uint16_t> GF_MEDIUM;
typedef CODE::GaloisField<14, 0b100000000101011, uint16_t> GF_SHORT;
typedef CODE::BoseChaudhuriHocquenghemDecoder<24, 1, 65343, GF_NORMAL> BCH_NORMAL_12;
typedef CODE::BoseChaudhuriHocquenghemDecoder<20, 1, 65375, GF_NORMAL> BCH_NORMAL_10;
typedef CODE::BoseChaudhuriHocquenghemDecoder<16, 1, 65407, GF_NORMAL> BCH_NORMAL_8;
typedef CODE::BoseChaudhuriHocquenghemDecoder<24, 1, 32587, GF_MEDIUM> BCH_MEDIUM_12;
typedef CODE::BoseChaudhuriHocquenghemDecoder<24, 1, 16215, GF_SHORT> BCH_SHORT_12;

/**
 * @brief Wrapper for GNU Radio's in-tree BCH Encoder
 *
 * Based on gr-dtv/lib/dvb/dvb_bch_bb_impl.cc from the GR sources.
 */
class DVBS2_GR_BCH_BENCH_API GrBchEncoder
{
private:
    int m_K;      // Message length in bits.
    int m_N;      // Codeword length in bits.
    int m_t;      // t-error correction capability
    int m_parity; // Parity bits
    std::bitset<MAX_BCH_PARITY_BITS> m_crc_table[256];
    std::bitset<MAX_BCH_PARITY_BITS> m_gen_poly;

    void compute_gen_poly(bool normal_fecframe);
    void compute_crc_table();

public:
    GrBchEncoder(int k, int n, int t);
    void encode(const std::vector<int>& ref_bits, std::vector<int>& enc_bits);
};

/**
 * @brief Wrapper for gr-dvbs2rx's original BCH Decoder
 *
 */
class DVBS2_GR_BCH_BENCH_API GrBchDecoder
{
private:
    int m_K; // Message length in bits.
    int m_N; // Codeword length in bits.
    int m_t; // Error correction capability.
    std::unique_ptr<GF_NORMAL> m_gf_normal;
    std::unique_ptr<GF_SHORT> m_gf_short;
    std::unique_ptr<BCH_NORMAL_12> m_dvbs2rx_decoder_n12;
    std::unique_ptr<BCH_NORMAL_10> m_dvbs2rx_decoder_n10;
    std::unique_ptr<BCH_NORMAL_8> m_dvbs2rx_decoder_n8;
    std::unique_ptr<BCH_SHORT_12> m_dvbs2rx_decoder_s12;
    std::array<uint8_t, 8192> m_packed_code;
    std::array<uint8_t, 24> m_packed_parity;


    /**
     * @brief Pack bits into bytes.
     *
     * @param in_bits Vector with the input bits.
     */
    void pack_bits(const std::vector<int>& in_bits);

    /**
     * @brief Unpack bits from bytes.
     *
     * @param dec_bits Vector to hold the resulting (unpacked) decoded bits.
     */
    void unpack_bits(std::vector<int>& dec_bits);

public:
    GrBchDecoder(int k, int n, int t);
    void decode(const std::vector<int>& in_bits, std::vector<int>& dec_bits);
};

/**
 * @brief Wrapper for the new BCH Codec implementation
 *
 */
class DVBS2_GR_BCH_BENCH_API NewBchCodec
{
private:
    gr::dvbs2rx::galois_field<uint32_t> m_gf;
    gr::dvbs2rx::bch_codec<uint32_t, gr::dvbs2rx::bitset256_t> m_bch;
    std::vector<uint8_t> m_packed_msg;
    std::vector<uint8_t> m_packed_codeword;

public:
    NewBchCodec(int N, int t);
    void encode(const std::vector<int>& ref_bits, std::vector<int>& enc_bits);
    void decode(const std::vector<int>& in_bits, std::vector<int>& dec_bits);
};

} // namespace dvbs2
} // namespace gr

#endif /* INCLUDED_GR_BCH_H */