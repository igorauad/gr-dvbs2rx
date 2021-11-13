/* -*- c++ -*- */
/*
 * Copyright 2018 Ron Economos.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bbdeheader_bb_impl.h"
#include <gnuradio/io_signature.h>
#include <stdio.h>

#define TRANSPORT_PACKET_LENGTH 188
#define TRANSPORT_ERROR_INDICATOR 0x80

namespace gr {
namespace dvbs2rx {

bbdeheader_bb::sptr bbdeheader_bb::make(dvb_standard_t standard,
                                        dvb_framesize_t framesize,
                                        dvb_code_rate_t rate)
{
    return gnuradio::get_initial_sptr(new bbdeheader_bb_impl(standard, framesize, rate));
}

/*
 * The private constructor
 */
bbdeheader_bb_impl::bbdeheader_bb_impl(dvb_standard_t standard,
                                       dvb_framesize_t framesize,
                                       dvb_code_rate_t rate)
    : gr::block("bbdeheader_bb",
                gr::io_signature::make(1, 1, sizeof(unsigned char)),
                gr::io_signature::make(1, 1, sizeof(unsigned char))),
      d_packet_cnt(0),
      d_error_cnt(0)
{
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
    max_dfl = kbch - BB_HEADER_LENGTH_BITS;
    build_crc8_table();
    dvb_standard = standard;
    count = 0;
    index = 0;
    synched = FALSE;
    spanning = FALSE;
    set_output_multiple((max_dfl / 8) * 2);
}

/*
 * Our virtual destructor.
 */
bbdeheader_bb_impl::~bbdeheader_bb_impl() {}

#define CRC_POLY 0xAB
// Reversed
#define CRC_POLYR 0xD5

void bbdeheader_bb_impl::build_crc8_table(void)
{
    int r, crc;

    for (int i = 0; i < 256; i++) {
        r = i;
        crc = 0;
        for (int j = 7; j >= 0; j--) {
            if ((r & (1 << j) ? 1 : 0) ^ ((crc & 0x80) ? 1 : 0)) {
                crc = (crc << 1) ^ CRC_POLYR;
            } else {
                crc <<= 1;
            }
        }
        crc_tab[i] = crc;
    }
}

/*
 * MSB is sent first
 *
 * The polynomial has been reversed
 */
unsigned int bbdeheader_bb_impl::check_crc8_bits(const unsigned char* in, int length)
{
    int crc = 0;
    int b;
    int i = 0;

    for (int n = 0; n < length; n++) {
        b = in[i++] ^ (crc & 0x01);
        crc >>= 1;
        if (b) {
            crc ^= CRC_POLY;
        }
    }

    return (crc);
}

void bbdeheader_bb_impl::forecast(int noutput_items, gr_vector_int& ninput_items_required)
{
    unsigned int n_bbframes = noutput_items / (max_dfl / 8);
    ninput_items_required[0] = n_bbframes * kbch;
}

int bbdeheader_bb_impl::general_work(int noutput_items,
                                     gr_vector_int& ninput_items,
                                     gr_vector_const_void_star& input_items,
                                     gr_vector_void_star& output_items)
{
    const unsigned char* in = (const unsigned char*)input_items[0];
    unsigned char* out = (unsigned char*)output_items[0];
    unsigned char* tei = out;
    unsigned int check, errors = 0;
    unsigned int mode;
    unsigned int produced = 0;
    unsigned char tmp;
    BBHeader* h = &m_format[0].bb_header;

    const unsigned int n_bbframes = noutput_items / (max_dfl / 8);
    for (unsigned int i = 0; i < n_bbframes; i++) {
        // Check the BBHEADER integrity
        check = check_crc8_bits(in, BB_HEADER_LENGTH_BITS);
        if (dvb_standard == STANDARD_DVBS2) {
            mode = INPUTMODE_NORMAL;
            if (check == 0) {
                check = TRUE;
            } else {
                check = FALSE;
            }
        } else {
            mode = INPUTMODE_NORMAL;
            if (check == 0) {
                check = TRUE;
            } else if (check == CRC_POLY) {
                check = TRUE;
                mode = INPUTMODE_HIEFF;
            } else {
                check = FALSE;
            }
        }

        if (check != TRUE) {
            synched = FALSE;
            printf("Baseband header crc failed.\n");
            in += kbch;
            continue;
        }

        // Parse the BBHEADER
        h->ts_gs = *in++ << 1;
        h->ts_gs |= *in++;
        h->sis_mis = *in++;
        h->ccm_acm = *in++;
        h->issyi = *in++;
        h->npd = *in++;
        h->ro = *in++ << 1;
        h->ro |= *in++;
        h->isi = 0;
        if (h->sis_mis == 0) {
            for (int n = 7; n >= 0; n--) {
                h->isi |= *in++ << n;
            }
        } else {
            in += 8;
        }
        h->upl = 0;
        for (int n = 15; n >= 0; n--) {
            h->upl |= *in++ << n;
        }
        h->dfl = 0;
        for (int n = 15; n >= 0; n--) {
            h->dfl |= *in++ << n;
        }
        df_remaining = h->dfl;
        h->sync = 0;
        for (int n = 7; n >= 0; n--) {
            h->sync |= *in++ << n;
        }
        h->syncd = 0;
        for (int n = 15; n >= 0; n--) {
            h->syncd |= *in++ << n;
        }
        in += 8; // Skip the last byte (CRC-8), processed in the beginning.

        // Validate the UPL, DFL and the SYNCD fields of the BBHEADER
        if (h->dfl > max_dfl) {
            synched = FALSE;
            printf("Baseband header invalid (dfl > kbch - 80).\n");
            in += max_dfl;
            continue;
        }

        if (h->dfl % 8 != 0) {
            synched = FALSE;
            printf("Baseband header invalid (dfl not a multiple of 8).\n");
            in += max_dfl;
            continue;
        }

        if (h->syncd > h->dfl) {
            synched = FALSE;
            printf("Baseband header invalid (syncd > dfl).\n");
            in += max_dfl;
            continue;
        }

        if (h->upl != (TRANSPORT_PACKET_LENGTH * 8)) {
            synched = FALSE;
            printf("Baseband header unsupported (upl != 188 bytes).\n");
            in += max_dfl;
            continue;
        }

        // Skip the initial SYNCD bits of the DATAFIELD if re-synchronizing
        if (synched == FALSE) {
            printf("Baseband header resynchronizing.\n");
            if (mode == INPUTMODE_NORMAL) {
                in += h->syncd + 8;
                df_remaining -= h->syncd + 8;
            } else {
                in += h->syncd;
                df_remaining -= h->syncd;
            }
            count = 0;
            synched = TRUE;
            index = 0;
            spanning = FALSE;
            distance = h->syncd;
        }

        // Process the DATAFIELD
        if (mode == INPUTMODE_NORMAL) {
            while (df_remaining) {
                if (count == 0) {
                    crc = 0;
                    if (index == TRANSPORT_PACKET_LENGTH) {
                        for (int j = 0; j < TRANSPORT_PACKET_LENGTH; j++) {
                            *out++ = packet[j];
                            produced++;
                        }
                        index = 0;
                        spanning = FALSE;
                    }
                    if (df_remaining < (TRANSPORT_PACKET_LENGTH - 1) * 8) {
                        index = 0;
                        packet[index++] = 0x47;
                        spanning = TRUE;
                    } else {
                        *out++ = 0x47;
                        produced++;
                        tei = out;
                    }
                    count++;
                    if (check == TRUE) {
                        if (distance != (unsigned int)h->syncd) {
                            synched = FALSE;
                        }
                        check = FALSE;
                    }
                } else if (count == TRANSPORT_PACKET_LENGTH) {
                    tmp = 0;
                    for (int n = 7; n >= 0; n--) {
                        tmp |= *in++ << n;
                    }
                    if (tmp != crc) {
                        errors++;
                        if (spanning) {
                            packet[1] |= TRANSPORT_ERROR_INDICATOR;
                        } else {
                            *tei |= TRANSPORT_ERROR_INDICATOR;
                        }
                        d_error_cnt++;
                    }
                    count = 0;
                    d_packet_cnt++;
                    df_remaining -= 8;
                    if (df_remaining == 0) {
                        distance = (TRANSPORT_PACKET_LENGTH - 1) * 8;
                    }
                }
                if (df_remaining >= 8 && count > 0) {
                    tmp = 0;
                    for (int n = 7; n >= 0; n--) {
                        tmp |= *in++ << n;
                        distance++;
                    }
                    crc = crc_tab[tmp ^ crc];
                    if (spanning == TRUE) {
                        packet[index++] = tmp;
                    } else {
                        *out++ = tmp;
                        produced++;
                    }
                    count++;
                    df_remaining -= 8;
                    if (df_remaining == 0) {
                        distance = 0;
                    }
                }
            }
            in += max_dfl - h->dfl; // Skip the DATAFIELD padding, if any
        } else {
            while (df_remaining) {
                if (count == 0) {
                    if (index == TRANSPORT_PACKET_LENGTH) {
                        for (int j = 0; j < TRANSPORT_PACKET_LENGTH; j++) {
                            *out++ = packet[j];
                            produced++;
                        }
                        index = 0;
                        spanning = FALSE;
                    }
                    if (df_remaining < (TRANSPORT_PACKET_LENGTH - 1) * 8) {
                        index = 0;
                        packet[index++] = 0x47;
                        spanning = TRUE;
                    } else {
                        *out++ = 0x47;
                        produced++;
                    }
                    count++;
                    if (check == TRUE) {
                        if (distance != (unsigned int)h->syncd) {
                            synched = FALSE;
                        }
                        check = FALSE;
                    }
                } else if (count == TRANSPORT_PACKET_LENGTH) {
                    count = 0;
                    if (df_remaining == 0) {
                        distance = 0;
                    }
                }
                if (df_remaining >= 8 && count > 0) {
                    tmp = 0;
                    for (int n = 7; n >= 0; n--) {
                        tmp |= *in++ << n;
                        distance++;
                    }
                    if (spanning == TRUE) {
                        packet[index++] = tmp;
                    } else {
                        *out++ = tmp;
                        produced++;
                    }
                    count++;
                    df_remaining -= 8;
                    if (df_remaining == 0) {
                        distance = 0;
                    }
                }
            }
            in += max_dfl - h->dfl; // Skip the DATAFIELD padding, if any
        }
    }

    if (errors != 0) {
        printf("TS packet crc errors = %d (PER = %g)\n",
               errors,
               (double)d_error_cnt / d_packet_cnt);
    }

    // Tell runtime system how many input items we consumed on
    // each input stream.
    consume_each(n_bbframes * kbch);

    // Tell runtime system how many output items we produced.
    return produced;
}

} /* namespace dvbs2rx */
} /* namespace gr */
