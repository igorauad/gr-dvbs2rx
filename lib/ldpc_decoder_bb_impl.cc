/* -*- c++ -*- */
/*
 * Copyright 2018,2019,2023 Ahmet Inan, Ron Economos, Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cpu_features_macros.h"
#include "debug_level.h"
#include "fec_params.h"
#include "ldpc_decoder_bb_impl.h"
#include <gnuradio/io_signature.h>
#include <gnuradio/logger.h>
#include <gnuradio/pdu.h>
#include <boost/format.hpp>
#include <cmath>

#ifdef CPU_FEATURES_ARCH_ARM
#include "cpuinfo_arm.h"
using namespace cpu_features;
#endif

#ifdef CPU_FEATURES_ARCH_X86
#include "cpuinfo_x86.h"
using namespace cpu_features;
#endif

namespace ldpc_neon {
void ldpc_dec_init(LDPCInterface* it);
int ldpc_dec_decode(void* buffer, int8_t* code, int trials);
} // namespace ldpc_neon

namespace ldpc_avx2 {
void ldpc_dec_init(LDPCInterface* it);
int ldpc_dec_decode(void* buffer, int8_t* code, int trials);
} // namespace ldpc_avx2

namespace ldpc_sse41 {
void ldpc_dec_init(LDPCInterface* it);
int ldpc_dec_decode(void* buffer, int8_t* code, int trials);
} // namespace ldpc_sse41

namespace ldpc_generic {
void ldpc_dec_init(LDPCInterface* it);
int ldpc_dec_decode(void* buffer, int8_t* code, int trials);
} // namespace ldpc_generic

namespace gr {
namespace dvbs2rx {

ldpc_decoder_bb::sptr ldpc_decoder_bb::make(dvb_standard_t standard,
                                            dvb_framesize_t framesize,
                                            dvb_code_rate_t rate,
                                            dvb_constellation_t constellation,
                                            dvb_outputmode_t outputmode,
                                            dvb_infomode_t infomode,
                                            int max_trials,
                                            int debug_level)
{
    return gnuradio::get_initial_sptr(new ldpc_decoder_bb_impl(standard,
                                                               framesize,
                                                               rate,
                                                               constellation,
                                                               outputmode,
                                                               infomode,
                                                               max_trials,
                                                               debug_level));
}

/*
 * The private constructor
 */
ldpc_decoder_bb_impl::ldpc_decoder_bb_impl(dvb_standard_t standard,
                                           dvb_framesize_t framesize,
                                           dvb_code_rate_t rate,
                                           dvb_constellation_t constellation,
                                           dvb_outputmode_t outputmode,
                                           dvb_infomode_t infomode,
                                           int max_trials,
                                           int debug_level)
    : gr::block("ldpc_decoder_bb",
                gr::io_signature::make(1, 1, sizeof(int8_t)),
                gr::io_signature::make(1, 1, sizeof(unsigned char))),
      d_debug_level(debug_level),
      d_output_mode(outputmode),
      d_frame_cnt(0),
      d_batch_cnt(0),
      d_total_trials(0),
      d_max_trials(max_trials)
{
    fec_info_t fec_info;
    get_fec_info(standard, framesize, rate, fec_info);
    d_kldpc = fec_info.ldpc.k;
    d_nldpc = fec_info.ldpc.n;
    d_kldpc_bytes = d_kldpc / 8;
    d_nldpc_bytes = d_nldpc / 8;

    if (framesize == FECFRAME_NORMAL) {
        switch (rate) {
        case C1_4:
            d_ldpc = new LDPC<DVB_S2_TABLE_B1>();
            break;
        case C1_3:
            d_ldpc = new LDPC<DVB_S2_TABLE_B2>();
            break;
        case C2_5:
            d_ldpc = new LDPC<DVB_S2_TABLE_B3>();
            break;
        case C1_2:
            d_ldpc = new LDPC<DVB_S2_TABLE_B4>();
            break;
        case C3_5:
            d_ldpc = new LDPC<DVB_S2_TABLE_B5>();
            break;
        case C2_3:
            if (standard == STANDARD_DVBS2) {
                d_ldpc = new LDPC<DVB_S2_TABLE_B6>();
            } else {
                d_ldpc = new LDPC<DVB_T2_TABLE_A3>();
            }
            break;
        case C3_4:
            d_ldpc = new LDPC<DVB_S2_TABLE_B7>();
            break;
        case C4_5:
            d_ldpc = new LDPC<DVB_S2_TABLE_B8>();
            break;
        case C5_6:
            d_ldpc = new LDPC<DVB_S2_TABLE_B9>();
            break;
        case C8_9:
            d_ldpc = new LDPC<DVB_S2_TABLE_B10>();
            break;
        case C9_10:
            d_ldpc = new LDPC<DVB_S2_TABLE_B11>();
            break;
        case C2_9_VLSNR:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B1>();
            break;
        case C13_45:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B2>();
            break;
        case C9_20:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B3>();
            break;
        case C90_180:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B11>();
            break;
        case C96_180:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B12>();
            break;
        case C11_20:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B4>();
            break;
        case C100_180:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B13>();
            break;
        case C104_180:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B14>();
            break;
        case C26_45:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B5>();
            break;
        case C18_30:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B22>();
            break;
        case C28_45:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B6>();
            break;
        case C23_36:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B7>();
            break;
        case C116_180:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B15>();
            break;
        case C20_30:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B23>();
            break;
        case C124_180:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B16>();
            break;
        case C25_36:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B8>();
            break;
        case C128_180:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B17>();
            break;
        case C13_18:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B9>();
            break;
        case C132_180:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B18>();
            break;
        case C22_30:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B24>();
            break;
        case C135_180:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B19>();
            break;
        case C140_180:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B20>();
            break;
        case C7_9:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B10>();
            break;
        case C154_180:
            d_ldpc = new LDPC<DVB_S2X_TABLE_B21>();
            break;
        default:
            break;
        }
    } else if (framesize == FECFRAME_SHORT) {
        switch (rate) {
        case C1_4:
            d_ldpc = new LDPC<DVB_S2_TABLE_C1>();
            break;
        case C1_3:
            d_ldpc = new LDPC<DVB_S2_TABLE_C2>();
            break;
        case C2_5:
            d_ldpc = new LDPC<DVB_S2_TABLE_C3>();
            break;
        case C1_2:
            d_ldpc = new LDPC<DVB_S2_TABLE_C4>();
            break;
        case C3_5:
            if (standard == STANDARD_DVBS2) {
                d_ldpc = new LDPC<DVB_S2_TABLE_C5>();
            } else {
                d_ldpc = new LDPC<DVB_T2_TABLE_B3>();
            }
            break;
        case C2_3:
            d_ldpc = new LDPC<DVB_S2_TABLE_C6>();
            break;
        case C3_4:
            d_ldpc = new LDPC<DVB_S2_TABLE_C7>();
            break;
        case C4_5:
            d_ldpc = new LDPC<DVB_S2_TABLE_C8>();
            break;
        case C5_6:
            d_ldpc = new LDPC<DVB_S2_TABLE_C9>();
            break;
        case C8_9:
            d_ldpc = new LDPC<DVB_S2_TABLE_C10>();
            break;
        case C11_45:
            d_ldpc = new LDPC<DVB_S2X_TABLE_C1>();
            break;
        case C4_15:
            d_ldpc = new LDPC<DVB_S2X_TABLE_C2>();
            break;
        case C14_45:
            d_ldpc = new LDPC<DVB_S2X_TABLE_C3>();
            break;
        case C7_15:
            d_ldpc = new LDPC<DVB_S2X_TABLE_C4>();
            break;
        case C8_15:
            d_ldpc = new LDPC<DVB_S2X_TABLE_C5>();
            break;
        case C26_45:
            d_ldpc = new LDPC<DVB_S2X_TABLE_C6>();
            break;
        case C32_45:
            d_ldpc = new LDPC<DVB_S2X_TABLE_C7>();
            break;
        case C1_5_VLSNR_SF2:
            d_ldpc = new LDPC<DVB_S2_TABLE_C1>();
            break;
        case C11_45_VLSNR_SF2:
            d_ldpc = new LDPC<DVB_S2X_TABLE_C1>();
            break;
        case C1_5_VLSNR:
            d_ldpc = new LDPC<DVB_S2_TABLE_C1>();
            break;
        case C4_15_VLSNR:
            d_ldpc = new LDPC<DVB_S2X_TABLE_C2>();
            break;
        case C1_3_VLSNR:
            d_ldpc = new LDPC<DVB_S2_TABLE_C2>();
            break;
        default:
            break;
        }
    } else {
        switch (rate) {
        case C1_5_MEDIUM:
            d_ldpc = new LDPC<DVB_S2X_TABLE_C8>();
            break;
        case C11_45_MEDIUM:
            d_ldpc = new LDPC<DVB_S2X_TABLE_C9>();
            break;
        case C1_3_MEDIUM:
            d_ldpc = new LDPC<DVB_S2X_TABLE_C10>();
            break;
        default:
            break;
        }
    }

    decode = nullptr;
    std::string impl = "generic";
#ifdef CPU_FEATURES_ARCH_ANY_ARM
    d_simd_size = 16;
#ifdef CPU_FEATURES_ARCH_AARCH64
    const bool has_neon = true; // always available on aarch64
#else
    const ArmFeatures features = GetArmInfo().features;
    const bool has_neon = features.neon;
#endif
    if (has_neon) {
        ldpc_neon::ldpc_dec_init(d_ldpc);
        decode = &ldpc_neon::ldpc_dec_decode;
        impl = "neon";
    } else {
        ldpc_generic::ldpc_dec_init(d_ldpc);
        decode = &ldpc_generic::ldpc_dec_decode;
    }
#else
#ifdef CPU_FEATURES_ARCH_X86
    const X86Features features = GetX86Info().features;
    d_simd_size = features.avx2 ? 32 : 16;
    if (features.avx2) {
        ldpc_avx2::ldpc_dec_init(d_ldpc);
        decode = &ldpc_avx2::ldpc_dec_decode;
        impl = "avx2";
    } else if (features.sse4_1) {
        ldpc_sse41::ldpc_dec_init(d_ldpc);
        decode = &ldpc_sse41::ldpc_dec_decode;
        impl = "sse4_1";
    } else {
        ldpc_generic::ldpc_dec_init(d_ldpc);
        decode = &ldpc_generic::ldpc_dec_decode;
    }
#else
    // Not ARM, nor x86. Use generic implementation.
    d_simd_size = 16;
    ldpc_generic::ldpc_dec_init(d_ldpc);
    decode = &ldpc_generic::ldpc_dec_decode;
#endif
#endif
    assert(decode != nullptr);
    d_debug_logger->debug("LDPC decoder implementation: {:s}", impl);
    d_soft = new int8_t[d_ldpc->code_len() * d_simd_size];
    d_aligned_buffer = aligned_alloc(d_simd_size, d_simd_size * d_ldpc->code_len());
    if (outputmode == OM_MESSAGE) {
        set_output_multiple(d_kldpc_bytes * d_simd_size);
        set_relative_rate((double)d_kldpc_bytes / d_nldpc);
    } else {
        set_output_multiple(d_nldpc_bytes * d_simd_size);
        set_relative_rate((double)d_nldpc_bytes / d_nldpc);
    }

    // Settings for LLR PDU port
    d_pdu_meta = pmt::make_dict();
    d_pdu_meta =
        pmt::dict_add(d_pdu_meta, pmt::mp("simd_size"), pmt::from_long(d_simd_size));
    d_pdu_meta = pmt::dict_add(d_pdu_meta, pmt::mp("frame_cnt"), pmt::from_uint64(0));
    message_port_register_out(d_pdu_port_id);
}

/*
 * Our virtual destructor.
 */
ldpc_decoder_bb_impl::~ldpc_decoder_bb_impl()
{
    free(d_aligned_buffer);
    delete[] d_soft;
    delete d_ldpc;
}

void ldpc_decoder_bb_impl::forecast(int noutput_items,
                                    gr_vector_int& ninput_items_required)
{
    if (d_output_mode == OM_MESSAGE) {
        unsigned int n_frames = noutput_items / d_kldpc_bytes;
        ninput_items_required[0] = n_frames * d_nldpc;
    } else {
        ninput_items_required[0] = 8 * noutput_items;
    }
}

const int DEFAULT_TRIALS = 25;
#define FACTOR 2 // same factor used on the decoder implementation

int ldpc_decoder_bb_impl::general_work(int noutput_items,
                                       gr_vector_int& ninput_items,
                                       gr_vector_const_void_star& input_items,
                                       gr_vector_void_star& output_items)
{
    const int8_t* in = (const int8_t*)input_items[0];
    unsigned char* out = (unsigned char*)output_items[0];
    const int CODE_LEN = d_ldpc->code_len();
    const int trials = (d_max_trials == 0) ? DEFAULT_TRIALS : d_max_trials;
    int consumed = 0;
    int output_size = d_output_mode ? d_kldpc_bytes : d_nldpc_bytes;

    for (int i = 0; i < noutput_items; i += output_size * d_simd_size) {
        memcpy(d_soft, in + consumed, d_nldpc * d_simd_size * sizeof(int8_t));

        // LDPC Decoding
        int count = decode(d_aligned_buffer, d_soft, trials);
        if (count < 0) {
            d_total_trials += trials;
            GR_LOG_DEBUG_LEVEL(
                1, "frame = {:d}, trials = {:d} (max)", d_frame_cnt, trials);
        } else {
            d_total_trials += (trials - count);
            GR_LOG_DEBUG_LEVEL(
                1, "frame = {:d}, trials = {:d}", d_frame_cnt, (trials - count));
        }

        // Send decoded LLRs so that the XFECFRAME demapper can refine its SNR estimate
        d_pdu_meta = pmt::dict_add(
            d_pdu_meta, pmt::mp("frame_cnt"), pmt::from_uint64(d_frame_cnt));
        message_port_pub(
            d_pdu_port_id,
            pmt::cons(d_pdu_meta,
                      pdu::make_pdu_vector(types::byte_t,
                                           reinterpret_cast<const uint8_t*>(d_soft),
                                           d_nldpc * d_simd_size)));

        // Output bit-packed bytes with the hard decisions and with the MSB first
        for (int blk = 0; blk < d_simd_size; blk++) {
            for (int j = 0; j < output_size; j++) {
                *out = 0;
                for (int k = 0; k < 8; k++) {
                    if (d_soft[(j * 8) + k + (blk * CODE_LEN)] < 0) {
                        *out |= 1 << (7 - k);
                    }
                }
                out++;
            }
        }

        consumed += d_nldpc * d_simd_size;
        d_frame_cnt += d_simd_size;
        d_batch_cnt++;
    }

    // Tell runtime system how many input items we consumed on
    // each input stream.
    consume_each(consumed);

    // Tell runtime system how many output items we produced.
    return noutput_items;
}

} /* namespace dvbs2rx */
} /* namespace gr */
