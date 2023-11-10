/* -*- c++ -*- */
/*
 * Copyright 2023 Igor Freire.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "dvb_defines.h"
#include "pl_defs.h"
#include "xfecframe_demapper_cb_impl.h"
#include <gnuradio/io_signature.h>
#include <gnuradio/pdu.h>

namespace gr {
namespace dvbs2rx {


xfecframe_demapper_cb::sptr xfecframe_demapper_cb::make(dvb_framesize_t framesize,
                                                        dvb_code_rate_t rate,
                                                        dvb_constellation_t constellation)
{
    return gnuradio::make_block_sptr<xfecframe_demapper_cb_impl>(
        framesize, rate, constellation);
}


xfecframe_demapper_cb_impl::xfecframe_demapper_cb_impl(dvb_framesize_t framesize,
                                                       dvb_code_rate_t rate,
                                                       dvb_constellation_t constellation)
    : gr::block("xfecframe_demapper_cb",
                gr::io_signature::make(1, 1, sizeof(gr_complex)),
                gr::io_signature::make(1, 1, sizeof(int8_t))),
      d_constellation(constellation),
      d_waiting_first_llr(true),
      d_frame_cnt(0),
      d_idx_xfecframe_buffer(0)
{
    if (framesize == FECFRAME_NORMAL)
        d_fecframe_len = FRAME_SIZE_NORMAL;
    else if (framesize == FECFRAME_MEDIUM)
        d_fecframe_len = FRAME_SIZE_MEDIUM;
    else
        d_fecframe_len = FRAME_SIZE_SHORT;

    if (constellation == MOD_QPSK) {
        d_mod = new PhaseShiftKeying<4, gr_complex, int8_t>();
        d_qpsk = std::make_unique<QpskConstellation>();
    } else if (constellation == MOD_8PSK) {
        d_mod = new PhaseShiftKeying<8, gr_complex, int8_t>();
        unsigned int rows = d_fecframe_len / d_mod->bits();
        /* 210 */
        if (rate == C3_5) {
            d_rowaddr0 = rows * 2;
            d_rowaddr1 = rows;
            d_rowaddr2 = 0;
        }
        /* 102 */
        else if (rate == C25_36 || rate == C13_18 || rate == C7_15 || rate == C8_15 ||
                 rate == C26_45) {
            d_rowaddr0 = rows;
            d_rowaddr1 = 0;
            d_rowaddr2 = rows * 2;
        }
        /* 012 */
        else {
            d_rowaddr0 = 0;
            d_rowaddr1 = rows;
            d_rowaddr2 = rows * 2;
        }
    } else {
        throw std::runtime_error("Unsupported constellation");
    }

    d_xfecframe_len = d_fecframe_len / d_mod->bits();
    d_aux_8i_buffer.resize(d_fecframe_len);
    d_aux_8i_buffer_2.resize(d_fecframe_len);

    // Initialize the pool of XFECFRAME buffers used for post-decoder SNR estimation
    for (size_t i = 0; i < d_xfecframe_buffer_pool.size(); i++) {
        d_xfecframe_saved[i] = std::numeric_limits<uint64_t>::max();
        d_xfecframe_buffer_pool[i].resize(d_xfecframe_len);
    }

    // Frame-by-frame processing is convenient
    set_output_multiple(d_fecframe_len);
    set_relative_rate((double)d_mod->bits());

    // Port for post-decoder SNR estimation
    message_port_register_in(d_pdu_port_id);
    set_msg_handler(d_pdu_port_id, [this](pmt::pmt_t pdu) { this->handle_llr_pdu(pdu); });
}

xfecframe_demapper_cb_impl::~xfecframe_demapper_cb_impl() {}

void xfecframe_demapper_cb_impl::forecast(int noutput_items,
                                          gr_vector_int& ninput_items_required)
{
    ninput_items_required[0] = noutput_items / d_mod->bits();
}

int xfecframe_demapper_cb_impl::general_work(int noutput_items,
                                             gr_vector_int& ninput_items,
                                             gr_vector_const_void_star& input_items,
                                             gr_vector_void_star& output_items)
{
    gr::thread::scoped_lock l(d_mutex);
    auto in = static_cast<const gr_complex*>(input_items[0]);
    auto out = static_cast<int8_t*>(output_items[0]);
    const int n_mod = d_mod->bits();
    const int n_frames = noutput_items / d_fecframe_len;
    int8_t tmp[n_mod];
    int consumed = 0;
    static constexpr float Es = 1.0; // assume unitary symbol energy

    for (int i_frame = 0; i_frame < n_frames; i_frame++) {
        // Copy XFECFRAME to an internal buffer so that we can refine the SNR measurement
        // later once the LDPC decoder reports the decoded LLRs.
        d_xfecframe_saved[d_idx_xfecframe_buffer] = d_frame_cnt;
        memcpy(d_xfecframe_buffer_pool[d_idx_xfecframe_buffer].data(),
               in,
               d_xfecframe_len * sizeof(gr_complex));
        d_idx_xfecframe_buffer =
            (d_idx_xfecframe_buffer + 1) % d_xfecframe_buffer_pool.size();

        // Compute an initial SNR estimate if we are still waiting for the first batch of
        // post-decoder LLRs for SNR estimation refinement.
        float snr_lin;
        if (d_waiting_first_llr) {
            if (d_constellation == MOD_QPSK) {
                snr_lin = d_qpsk->estimate_snr(in, d_xfecframe_len);
            } else {
                float sp = 0;
                float np = 0;
                for (unsigned int j = 0; j < d_xfecframe_len; j++) {
                    d_mod->hard(tmp, in[j]);
                    gr_complex s = d_mod->map(tmp);
                    gr_complex e = in[j] - s;
                    sp += std::norm(s);
                    np += std::norm(e);
                }
                if (!(np > 0)) {
                    np = 1e-12;
                }
                snr_lin = sp / np;
            }
            d_snr = 10 * std::log10(snr_lin);
            d_N0 = Es / snr_lin;
            d_precision = 4.0 / d_N0;
        }

        // Soft constellation demapping
        if (d_constellation == MOD_QPSK) {
            d_qpsk->demap_soft(out, in, d_xfecframe_len, d_N0);
        } else {
            int8_t* p_soft_out = d_aux_8i_buffer.data();
            for (unsigned int j = 0; j < d_xfecframe_len; j++) {
                d_mod->soft(p_soft_out + (j * n_mod), in[j], d_precision);
            }
        }

        // Deinterleave
        if (d_constellation == MOD_8PSK) {
            int8_t *c1, *c2, *c3;
            c1 = &out[d_rowaddr0];
            c2 = &out[d_rowaddr1];
            c3 = &out[d_rowaddr2];
            // The block interleaver has n_mod columns and "fecframe_len / n_mod" rows.
            // The latter is equal to the xfecframe_len.
            int indexin = 0;
            int8_t* p_soft = d_aux_8i_buffer.data();
            for (unsigned int j = 0; j < d_xfecframe_len; j++) {
                c1[j] = p_soft[indexin++];
                c2[j] = p_soft[indexin++];
                c3[j] = p_soft[indexin++];
            }
        }

        in += d_xfecframe_len;
        out += d_fecframe_len;
        consumed += d_xfecframe_len;
        d_frame_cnt++;
    }

    consume_each(consumed);
    return noutput_items;
}

void xfecframe_demapper_cb_impl::handle_llr_pdu(pmt::pmt_t pdu)
{
    static constexpr float Es = 1.0; // again, assume unitary symbol energy
    gr::thread::scoped_lock l(d_mutex);

    if (!(pmt::is_pdu(pdu))) {
        d_logger->error("PMT is not a PDU. Dropping...");
        return;
    }

    pmt::pmt_t meta = pmt::car(pdu);
    pmt::pmt_t v_data = pmt::cdr(pdu);

    // PDU metadata
    if (!pmt::is_dict(meta)) {
        d_logger->error("PDU metadata is not a dict. Dropping...");
        return;
    }
    static const pmt::pmt_t simd_size_key = pmt::intern("simd_size");
    static const pmt::pmt_t frame_key = pmt::intern("frame_cnt");
    if (!pmt::dict_has_key(meta, simd_size_key)) {
        d_logger->error("PDU metadata has no simd_size key. Dropping...");
        return;
    }
    if (!pmt::dict_has_key(meta, frame_key)) {
        d_logger->error("PDU metadata has no frame_cnt key. Dropping...");
        return;
    }
    int simd_size = pmt::to_long(pmt::dict_ref(meta, simd_size_key, pmt::PMT_NIL));
    uint64_t starting_frame_cnt =
        pmt::to_uint64(pmt::dict_ref(meta, frame_key, pmt::PMT_NIL));

    // PDU data
    if (pmt::length(v_data) == 0) {
        d_logger->error("PDU has no data. Dropping...");
        return;
    }

    size_t v_itemsize = pmt::uniform_vector_itemsize(v_data);
    if (v_itemsize != sizeof(int8_t)) {
        d_logger->error("PDU has incorrect itemsize ({:d} != {:d}). Dropping...",
                        v_itemsize,
                        sizeof(int8_t));
        return;
    }

    size_t n_llr = 0;
    const int8_t* p_pdu_data =
        static_cast<const int8_t*>(pmt::uniform_vector_elements(v_data, n_llr));
    if (n_llr != simd_size * d_fecframe_len) {
        d_logger->error(
            "PDU does not have the expected number of LLRs (n_llr = {:d}). Dropping...",
            n_llr);
        return;
    }

    size_t n_frames = n_llr / d_fecframe_len;
    size_t n_processed_frames = 0;
    float snr_lin_accum = 0;
    for (size_t i_frame = 0; i_frame < n_frames; i_frame++) {
        // Find the corresponding XFECFRAME buffer
        size_t buffer_idx;
        bool buffer_found = false;
        uint64_t frame_num = starting_frame_cnt + i_frame;
        for (size_t i = 0; i < d_xfecframe_saved.size(); i++) {
            if (d_xfecframe_saved[i] == frame_num) {
                buffer_idx = i;
                buffer_found = true;
                break;
            }
        }

        if (!buffer_found) {
            d_logger->error(
                "Buffer not found for XFECFRAME {:d} (frame_cnt={:d}). Skipping...",
                frame_num,
                d_frame_cnt);
            continue;
        }

        // Refine the SNR estimate using the given LLR vector
        const int8_t* p_llr = p_pdu_data + (i_frame * d_fecframe_len);
        const gr_complex* p_xfecframe = d_xfecframe_buffer_pool[buffer_idx].data();
        if (d_constellation == MOD_QPSK) {
            snr_lin_accum += d_qpsk->estimate_snr(p_xfecframe, p_llr, d_xfecframe_len);
        } else if (d_constellation == MOD_8PSK) {
            int8_t *c1, *c2, *c3;
            // Map the soft LDPC-decoded output to +-1 and use those to remap into the
            // corresponding constellation symbols. Then, refine the SNR estimate.
            int8_t* tempu = d_aux_8i_buffer.data();
            int8_t* tempv = d_aux_8i_buffer_2.data();
            for (unsigned int j = 0; j < d_fecframe_len; j++) {
                tempu[j] = p_llr[j] < 0 ? -1 : 1;
            }
            c1 = &tempu[d_rowaddr0];
            c2 = &tempu[d_rowaddr1];
            c3 = &tempu[d_rowaddr2];

            // The block interleaver has n_mod columns and "fecframe_len / n_mod" rows.
            // The latter is equal to the xfecframe_len.
            int indexout = 0;
            for (unsigned int j = 0; j < d_xfecframe_len; j++) {
                tempv[indexout++] = c1[j];
                tempv[indexout++] = c2[j];
                tempv[indexout++] = c3[j];
            }

            float sp = 0;
            float np = 0;
            const int n_mod = d_mod->bits();

            for (unsigned int j = 0; j < d_xfecframe_len; j++) {
                gr_complex s = d_mod->map(&tempv[(j * n_mod)]);
                gr_complex e = p_xfecframe[j] - s;
                sp += std::norm(s);
                np += std::norm(e);
            }
            if (!(np > 0)) {
                np = 1e-12;
            }
            snr_lin_accum += sp / np;
        }
        n_processed_frames++;
    }
    float avg_snr_lin = snr_lin_accum / n_processed_frames;
    d_snr = 10 * std::log10(avg_snr_lin);
    d_N0 = Es / avg_snr_lin;
    d_precision = 4.0 / d_N0;
    if (n_processed_frames > 0)
        d_waiting_first_llr = false;
}

} /* namespace dvbs2rx */
} /* namespace gr */
