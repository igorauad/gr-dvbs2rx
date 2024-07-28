/* -*- c++ -*- */
/*
 * Copyright 2018 Ron Economos.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_BBDEHEADER_BB_IMPL_H
#define INCLUDED_DVBS2RX_BBDEHEADER_BB_IMPL_H

#include "dvb_defines.h"
#include "gf_util.h"
#include <gnuradio/dvbs2rx/bbdeheader_bb.h>

namespace gr {
namespace dvbs2rx {

#define TS_PACKET_LENGTH 188

typedef struct {
    int ts_gs;
    int sis_mis;
    int ccm_acm;
    int issyi;
    int npd;
    int ro;
    int isi;
    unsigned int upl;
    unsigned int dfl;
    int sync;
    unsigned int syncd;
} BBHeader;

class bbdeheader_bb_impl : public bbdeheader_bb
{
private:
    const int d_debug_level;         /**< Debug level*/
    unsigned int d_kbch_bytes;       /**< BBFRAME length in bytes */
    unsigned int d_max_dfl;          /**< Maximum DATAFIELD length in bits */
    bool d_synched;                  /**< Synchronized to the start of TS packets */
    unsigned int d_partial_ts_bytes; /**< Byte count of the partial TS packet
                                        extracted at the end of the previous BBFRAME */
    unsigned char d_partial_pkt[TS_PACKET_LENGTH]; /**< Partial TS packet storage */
    BBHeader d_bbheader;                           /**< Parsed BBHEADER */
    uint64_t d_packet_cnt;                         /**< Count of received packets  */
    uint64_t d_error_cnt;                   /**< Count of packets with bit errors */
    uint64_t d_bbframe_cnt;                 /**< Count of processed BBFRAMEs */
    uint64_t d_bbframe_drop_cnt;            /**< Count of dropped BBFRAMEs */
    uint64_t d_bbframe_gap_cnt;             /**< Count of gaps between BBFRAMEs */
    gf2_poly<uint16_t> d_crc_poly;          /**< CRC-8 generator polynomial */
    std::array<uint16_t, 256> d_crc8_table; /**< CRC-8 remainder look-up table */

    /**
     * @brief Parse and validate an incoming BBHEADER
     *
     * @param in Input bytes carrying the BBHEADER.
     * @param h Output parsed BBHEADER.
     * @return true When the BBHEADER is valid.
     * @return false When the BBHEADER is invalid.
     */
    bool parse_bbheader(u8_cptr_t in, BBHeader* h);

    /**
     * @brief Check the CRC-8 of a sequence of bytes
     *
     * @param in Input bytes to check.
     * @param size Number of bytes to check.
     * @return true When the CRC-8 is valid.
     * @return false When the CRC-8 is invalid.
     */
    bool check_crc8(u8_cptr_t in, int size);

public:
    bbdeheader_bb_impl(dvb_standard_t standard,
                       dvb_framesize_t framesize,
                       dvb_code_rate_t rate,
                       int debug_level);
    ~bbdeheader_bb_impl();

    void forecast(int noutput_items, gr_vector_int& ninput_items_required);

    int general_work(int noutput_items,
                     gr_vector_int& ninput_items,
                     gr_vector_const_void_star& input_items,
                     gr_vector_void_star& output_items);

    uint64_t get_packet_count() { return d_packet_cnt; }
    uint64_t get_error_count() { return d_error_cnt; }
    uint64_t get_bbframe_count() { return d_bbframe_cnt; }
    uint64_t get_bbframe_drop_count() { return d_bbframe_drop_cnt; }
    uint64_t get_bbframe_gap_count() { return d_bbframe_gap_cnt; }
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_BBDEHEADER_BB_IMPL_H */
