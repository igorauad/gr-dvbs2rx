/* -*- c++ -*- */
/*
 * Copyright 2018,2021 Igor Freire, Ron Economos.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bbdeheader_bb_impl.h"
#include "debug_level.h"
#include <gnuradio/io_signature.h>
#include <gnuradio/logger.h>
#include <boost/format.hpp>

#define MPEG_TS_SYNC_BYTE 0x47
#define TRANSPORT_ERROR_INDICATOR 0x80

namespace gr {
namespace dvbs2rx {

bbdeheader_bb::sptr bbdeheader_bb::make(dvb_standard_t standard,
                                        dvb_framesize_t framesize,
                                        dvb_code_rate_t rate,
                                        int debug_level)
{
    return gnuradio::get_initial_sptr(
        new bbdeheader_bb_impl(standard, framesize, rate, debug_level));
}

/*
 * The private constructor
 */
bbdeheader_bb_impl::bbdeheader_bb_impl(dvb_standard_t standard,
                                       dvb_framesize_t framesize,
                                       dvb_code_rate_t rate,
                                       int debug_level)
    : gr::block("bbdeheader_bb",
                gr::io_signature::make(1, 1, sizeof(unsigned char)),
                gr::io_signature::make(1, 1, sizeof(unsigned char))),
      d_debug_level(debug_level),
      d_synched(false),
      d_partial_ts_bytes(0),
      d_packet_cnt(0),
      d_error_cnt(0),
      d_crc_poly(0b111010101), // x^8 + x^7 + x^6 + x^4 + x^2 + 1
      d_crc8_table(build_gf2_poly_rem_lut(d_crc_poly))
{
    unsigned int kbch;
    if (framesize == FECFRAME_NORMAL) {
        switch (rate) {
        case C1_4:
            kbch = 16008;
            break;
        case C1_3:
            kbch = 21408;
            break;
        case C2_5:
            kbch = 25728;
            break;
        case C1_2:
            kbch = 32208;
            break;
        case C3_5:
            kbch = 38688;
            break;
        case C2_3:
            kbch = 43040;
            break;
        case C3_4:
            kbch = 48408;
            break;
        case C4_5:
            kbch = 51648;
            break;
        case C5_6:
            kbch = 53840;
            break;
        case C8_9:
            kbch = 57472;
            break;
        case C9_10:
            kbch = 58192;
            break;
        case C2_9_VLSNR:
            kbch = 14208;
            break;
        case C13_45:
            kbch = 18528;
            break;
        case C9_20:
            kbch = 28968;
            break;
        case C90_180:
            kbch = 32208;
            break;
        case C96_180:
            kbch = 34368;
            break;
        case C11_20:
            kbch = 35448;
            break;
        case C100_180:
            kbch = 35808;
            break;
        case C104_180:
            kbch = 37248;
            break;
        case C26_45:
            kbch = 37248;
            break;
        case C18_30:
            kbch = 38688;
            break;
        case C28_45:
            kbch = 40128;
            break;
        case C23_36:
            kbch = 41208;
            break;
        case C116_180:
            kbch = 41568;
            break;
        case C20_30:
            kbch = 43008;
            break;
        case C124_180:
            kbch = 44448;
            break;
        case C25_36:
            kbch = 44808;
            break;
        case C128_180:
            kbch = 45888;
            break;
        case C13_18:
            kbch = 46608;
            break;
        case C132_180:
            kbch = 47328;
            break;
        case C22_30:
            kbch = 47328;
            break;
        case C135_180:
            kbch = 48408;
            break;
        case C140_180:
            kbch = 50208;
            break;
        case C7_9:
            kbch = 50208;
            break;
        case C154_180:
            kbch = 55248;
            break;
        default:
            kbch = 0;
            break;
        }
    } else if (framesize == FECFRAME_SHORT) {
        switch (rate) {
        case C1_4:
            kbch = 3072;
            break;
        case C1_3:
            kbch = 5232;
            break;
        case C2_5:
            kbch = 6312;
            break;
        case C1_2:
            kbch = 7032;
            break;
        case C3_5:
            kbch = 9552;
            break;
        case C2_3:
            kbch = 10632;
            break;
        case C3_4:
            kbch = 11712;
            break;
        case C4_5:
            kbch = 12432;
            break;
        case C5_6:
            kbch = 13152;
            break;
        case C8_9:
            kbch = 14232;
            break;
        case C11_45:
            kbch = 3792;
            break;
        case C4_15:
            kbch = 4152;
            break;
        case C14_45:
            kbch = 4872;
            break;
        case C7_15:
            kbch = 7392;
            break;
        case C8_15:
            kbch = 8472;
            break;
        case C26_45:
            kbch = 9192;
            break;
        case C32_45:
            kbch = 11352;
            break;
        case C1_5_VLSNR_SF2:
            kbch = 2512;
            break;
        case C11_45_VLSNR_SF2:
            kbch = 3792;
            break;
        case C1_5_VLSNR:
            kbch = 3072;
            break;
        case C4_15_VLSNR:
            kbch = 4152;
            break;
        case C1_3_VLSNR:
            kbch = 5232;
            break;
        default:
            kbch = 0;
            break;
        }
    } else {
        switch (rate) {
        case C1_5_MEDIUM:
            kbch = 5660;
            break;
        case C11_45_MEDIUM:
            kbch = 7740;
            break;
        case C1_3_MEDIUM:
            kbch = 10620;
            break;
        default:
            kbch = 0;
            break;
        }
    }
    d_kbch_bytes = kbch / 8;
    d_max_dfl = kbch - BB_HEADER_LENGTH_BITS;
    set_output_multiple(d_max_dfl / 8); // ensure full BBFRAMEs on the input
}

/*
 * Our virtual destructor.
 */
bbdeheader_bb_impl::~bbdeheader_bb_impl() {}

void bbdeheader_bb_impl::forecast(int noutput_items, gr_vector_int& ninput_items_required)
{
    unsigned int n_bbframes =
        std::ceil(static_cast<double>(noutput_items * 8) / d_max_dfl);
    ninput_items_required[0] = n_bbframes * d_kbch_bytes;
}

bool bbdeheader_bb_impl::parse_bbheader(u8_cptr_t in, BBHeader* h)
{
    // Integrity check
    if (!check_crc8(in, BB_HEADER_LENGTH_BYTES)) {
        GR_LOG_DEBUG_LEVEL(1, "Baseband header crc failed.");
        return false;
    }

    // MATYPE-1
    h->ts_gs = (*in >> 6) & 0x3;
    h->sis_mis = *in >> 5 & 0x1;
    h->ccm_acm = *in >> 4 & 0x1;
    h->issyi = *in >> 3 & 0x1;
    h->npd = *in >> 2 & 0x1;
    h->ro = *in++ & 0x3;
    // MATYPE-2
    h->isi = 0;
    if (h->sis_mis == 0) {
        h->isi = *in++;
    } else {
        in++;
    }
    // UPL
    h->upl = from_u8_array<uint16_t>(in, 2);
    in += 2;
    // DFL
    h->dfl = from_u8_array<uint16_t>(in, 2);
    in += 2;
    // SYNC
    h->sync = *in++;
    // SYNCD
    h->syncd = from_u8_array<uint16_t>(in, 2);

    // Validate the UPL, DFL and the SYNCD fields
    if (h->dfl > d_max_dfl) {
        d_logger->warn("Baseband header invalid (dfl > kbch - 80).");
        return false;
    }

    if (h->dfl % 8 != 0) {
        d_logger->warn("Baseband header invalid (dfl not a multiple of 8).");
        return false;
    }

    if (h->syncd > h->dfl) {
        d_logger->warn("Baseband header invalid (syncd > dfl).");
        return false;
    }

    if (h->upl != (TS_PACKET_LENGTH * 8)) {
        d_logger->warn("Baseband header unsupported (upl != 188 bytes).");
        return false;
    }

    if (h->syncd % 8 != 0) {
        d_logger->warn("Baseband header unsupported (syncd not byte-aligned).");
        return false;
    }

    return true;
}

bool bbdeheader_bb_impl::check_crc8(u8_cptr_t in, int size)
{
    const auto rem = gf2_poly_rem(in, size, d_crc_poly, d_crc8_table);
    return rem.get_poly() == 0;
}

int bbdeheader_bb_impl::general_work(int noutput_items,
                                     gr_vector_int& ninput_items,
                                     gr_vector_const_void_star& input_items,
                                     gr_vector_void_star& output_items)
{
    const unsigned char* in = (const unsigned char*)input_items[0];
    unsigned char* out = (unsigned char*)output_items[0];
    unsigned int produced = 0;
    unsigned int errors = 0;

    // Process as many BBFRAMES as possible, as long as these are available on the input
    // buffer and fit on the output buffer
    const unsigned int in_bbframes = ninput_items[0] / d_kbch_bytes;
    const unsigned int out_bbframes =
        std::ceil(static_cast<double>(noutput_items * 8) / d_max_dfl);
    const unsigned int n_bbframes = std::min(in_bbframes, out_bbframes);

    for (unsigned int i = 0; i < n_bbframes; i++) {
        // Parse and validate the BBHEADER
        const bool bbheader_valid = parse_bbheader(in, &d_bbheader);
        if (!bbheader_valid) {
            d_synched = false;
            in += d_kbch_bytes; // jump to the next BBFRAME
            continue;
        }

        GR_LOG_DEBUG_LEVEL(3,
                           "MATYPE: TS/GS={:b}; SIS/MIS={}; CCM/ACM={}; ISSYI={}; "
                           "NPD={}; RO={:b}; ISI={}",
                           d_bbheader.ts_gs,
                           d_bbheader.sis_mis,
                           d_bbheader.ccm_acm,
                           d_bbheader.issyi,
                           d_bbheader.npd,
                           d_bbheader.ro,
                           d_bbheader.isi);

        // Skip the BBHEADER
        in += BB_HEADER_LENGTH_BYTES;
        unsigned int df_remaining = d_bbheader.dfl / 8; // DATAFIELD bytes remaining

        // Skip the initial SYNCD bits of the DATAFIELD if re-synchronizing. Skip also the
        // first sync byte, as it contains the CRC8 of a lost or missed TS packet.
        if (!d_synched) {
            GR_LOG_DEBUG_LEVEL(1, "Baseband header resynchronizing.");
            in += (d_bbheader.syncd / 8) + 1;
            df_remaining -= (d_bbheader.syncd / 8) + 1;
            d_synched = true;
            d_partial_ts_bytes = 0; // Reset the count
        }

        // Process the TS packets available on the DATAFIELD
        while (df_remaining >= TS_PACKET_LENGTH) {
            u8_cptr_t packet;
            // Start by completing a partial TS packet from the previous BBFRAME (if any)
            if (d_partial_ts_bytes > 0) {
                unsigned int remaining = TS_PACKET_LENGTH - d_partial_ts_bytes;
                memcpy(d_partial_pkt + d_partial_ts_bytes, in, remaining);
                d_partial_ts_bytes = 0; // Reset the count
                in += remaining;
                df_remaining -= remaining;
                packet = d_partial_pkt;
            } else {
                packet = in;
                in += TS_PACKET_LENGTH;
                df_remaining -= TS_PACKET_LENGTH;
            }

            const bool crc_valid = check_crc8(packet, TS_PACKET_LENGTH);
            out[0] = MPEG_TS_SYNC_BYTE; // Restore the sync byte
            memcpy(out + 1, packet, TS_PACKET_LENGTH - 1);
            if (!crc_valid) {
                out[1] |= TRANSPORT_ERROR_INDICATOR;
                d_error_cnt++;
                errors++;
            }
            out += TS_PACKET_LENGTH;
            produced += TS_PACKET_LENGTH;
            d_packet_cnt++;
        }

        // If a partial TS packet remains on the DATAFIELD, store it
        if (df_remaining > 0) {
            d_partial_ts_bytes = df_remaining;
            memcpy(d_partial_pkt, in, df_remaining);
            in += df_remaining;
        }

        // Skip the DATAFIELD padding, if any
        in += (d_max_dfl - d_bbheader.dfl) / 8;
    }

    if (errors != 0) {
        GR_LOG_DEBUG_LEVEL(1,
                           "TS packet crc errors = {:d} (PER = {:g})",
                           errors,
                           ((double)d_error_cnt / d_packet_cnt));
    }

    consume_each(n_bbframes * d_kbch_bytes);
    return produced;
}

} /* namespace dvbs2rx */
} /* namespace gr */
