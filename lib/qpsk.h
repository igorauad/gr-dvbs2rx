/* -*- c++ -*- */
/*
 * Copyright (c) 2023 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_CONSTELLATIONS_H
#define INCLUDED_DVBS2RX_CONSTELLATIONS_H

#include "dvb_defines.h"
#include "pl_defs.h"
#include <gnuradio/gr_complex.h>
#include <volk/volk_alloc.hh>

namespace gr {
namespace dvbs2rx {

/**
 * @brief QPSK Constellation
 *
 * Implements vectorized QPSK operations.
 */
class QpskConstellation
{
private:
    bool d_volk_generic;                  /**< Whether the VOLK_GENERIC env var is set */
    volk::vector<int8_t> d_aux_8i_buffer; /**< Auxiliary int8 buffer */
    volk::vector<gr_complex> d_aux_32fc_buffer; /**< Auxiliary gr_complex buffer  */

    /**
     * @brief Estimate the linear SNR of input QPSK symbols.
     *
     * @param in_syms Buffer containing the input QPSK symbols.
     * @param ref_syms Buffer containing the reference QPSK constellation points.
     * @param n_syms Number of QPSK symbols available on the input buffer.
     * @return float Measured linear SNR.
     */
    float _estimate_snr(const gr_complex* in_syms,
                        const gr_complex* ref_syms,
                        unsigned int n_syms)
    {
        // Sum of the magnitude of the reference symbols
        gr_complex norm_sq_ref_fc = 0;
        volk_32fc_x2_conjugate_dot_prod_32fc(&norm_sq_ref_fc, ref_syms, ref_syms, n_syms);

        // Sum of the magnitude of the noise samples
        gr_complex* p_aux_buffer = d_aux_32fc_buffer.data();
        volk_32f_x2_subtract_32f(reinterpret_cast<float*>(p_aux_buffer),
                                 reinterpret_cast<const float*>(in_syms),
                                 reinterpret_cast<const float*>(ref_syms),
                                 n_syms * 2);
        gr_complex norm_sq_noise_fc = 0;
        volk_32fc_x2_conjugate_dot_prod_32fc(
            &norm_sq_noise_fc, p_aux_buffer, p_aux_buffer, n_syms);

        float norm_sq_ref_f = norm_sq_ref_fc.real();
        float norm_sq_noise_f = norm_sq_noise_fc.real();
        if (norm_sq_noise_f == 0)
            norm_sq_noise_f = 1e-12;

        return norm_sq_ref_f / norm_sq_noise_f;
    }

public:
    /**
     * @brief Construct a new Qpsk Constellation object.
     */
    QpskConstellation()
        : d_volk_generic(getenv("VOLK_GENERIC") != nullptr),
          d_aux_8i_buffer(FRAME_SIZE_NORMAL),
          d_aux_32fc_buffer(MAX_XFECFRAME_LEN)
    {
    }

    /**
     * @brief Destroy the Qpsk Constellation object.
     */
    ~QpskConstellation() {}

    /**
     * @brief Map input bits to QPSK symbols.
     *
     * Supports mapping with the standard (normal) convention and an inverted convention
     * that is useful for the slicing implementation.
     *
     * Standard convention:
     *  b1b0 ->    Real    + j*Imaginary
     *    00 -> +sqrt(2)/2 + j*sqrt(2)/2
     *    01 -> +sqrt(2)/2 - j*sqrt(2)/2
     *    10 -> -sqrt(2)/2 + j*sqrt(2)/2
     *    11 -> -sqrt(2)/2 - j*sqrt(2)/2
     *
     * Note: the MSB b1 is tied to the real part and the LSB b0 to the imaginary part. The
     * real part is positive for b1=0 and negative for b1=1. Likewise, the imaginary part
     * is positive for b0=0 and negative for b0=1.
     *
     * Inverted convention:
     *  b1b0 ->    Real    + j*Imaginary
     *    00 -> -sqrt(2)/2 - j*sqrt(2)/2
     *    01 -> -sqrt(2)/2 + j*sqrt(2)/2
     *    10 -> +sqrt(2)/2 - j*sqrt(2)/2
     *    11 -> +sqrt(2)/2 + j*sqrt(2)/2
     *
     * The difference in the inverted convention is that bit=1 is mapped to a positive
     * value (+sqrt(2)/2) and bit=0 to a negative value (-sqrt(2)/2) instead of the other
     * way around.
     *
     * @param out_buf Buffer where the mapped symbols should be stored.
     * @param in_bits Buffer containing the input bits on unpacked int8 values.
     * @param n_bits Number of bits to map.
     * @param inv_convention Whether to use the inverted mapping convention.
     */
    void map(gr_complex* out_buf,
             const int8_t* in_bits,
             unsigned int n_bits,
             bool inv_convention = false)
    {
        if (n_bits % 2 != 0)
            throw std::invalid_argument("Number of bits must be even");

        // Standard mapping: multiply the input bits by -sqrt(2) and add sqrt(2)/2.
        // Inverted mapping: multiply the input bits by +sqrt(2) and subtract sqrt(2)/2.
        float div_scalar = inv_convention ? SQRT2_2 : -SQRT2_2;
        float* out_buf_32f = reinterpret_cast<float*>(out_buf);
        volk_8i_s32f_convert_32f(out_buf_32f, in_bits, div_scalar, n_bits);

        // The volk_32f_s32f_add_32f kernel has a bug in which its generic implementation
        // can never be found with volk version <= 3.0.0. Given the QA tests run with
        // VOLK_GENERIC=1 (added by the GR_ADD_CPP_TEST macro), this bug leads to an
        // infinite loop below when volk_32f_s32f_add_32f spins forever searching for the
        // generic implementation. A fix was provided in gnuradio/volk#641, but it won't
        // take effect until a new version following 3.0.0 is released. Hence, for
        // affected versions, check if VOLK_GENERIC is set and use an alternative
        // implementation if so.
        float offset = inv_convention ? -SQRT2_2 : SQRT2_2;
#if VOLK_VERSION <= 30000
        if (d_volk_generic) {
            for (unsigned int i = 0; i < n_bits; i++)
                out_buf_32f[i] += offset;
        } else {
            volk_32f_s32f_add_32f(out_buf_32f, out_buf_32f, offset, n_bits);
        }
#else
        volk_32f_s32f_add_32f(out_buf_32f, out_buf_32f, offset, n_bits);
#endif
    }

    /**
     * @overload
     * @param out_buf Buffer where the mapped symbols should be stored.
     * @param in_bits Vector containing the input bits on unpacked int8 values.
     * @param inv_convention Whether to use the inverted mapping convention.
     */
    void map(gr_complex* out_buf,
             const volk::vector<int8_t>& in_bits,
             bool inv_convention = false)
    {
        map(out_buf, in_bits.data(), in_bits.size(), inv_convention);
    }

    /**
     * @brief Slice noisy input QPSK symbols to the closest constellation points.
     *
     * @param out_buf Buffer where the sliced symbols should be stored.
     * @param in_buf Buffer containing the input QPSK symbols.
     * @param n_syms Number of symbols to slice.
     */
    void slice(gr_complex* out_buf, const gr_complex* in_buf, unsigned int n_syms)
    {
        // The volk_32f_binary_slicer_8i kernel facilitates decoding but returns inverted
        // results relative to the convention adopted in the DVB-S2 standard. It returns
        // bit=1 for non-negative values and bit=0 for negative values. Nevertheless, we
        // can use it as-is as long as we stick to the same inverted convention when
        // remapping the hard-decoded bits back to QPSK constellation symbols.
        volk_32f_binary_slicer_8i(
            d_aux_8i_buffer.data(), reinterpret_cast<const float*>(in_buf), n_syms * 2);
        map(out_buf, d_aux_8i_buffer.data(), n_syms * 2, /*inv_convention=*/true);
    }

    /**
     * @overload
     * @param out_buf Buffer where the sliced symbols should be stored.
     * @param in_syms Vector containing the input QPSK symbols.
     */
    void slice(gr_complex* out_buf, const volk::vector<gr_complex>& in_syms)
    {
        slice(out_buf, in_syms.data(), in_syms.size());
    }

    /**
     * @brief Soft-demap noisy input QPSK symbols into quantized LLRs.
     *
     * As explained in the mapping function, for each pair of bits b1b0, the MSB b1 is
     * tied to the real part and the LSB b0 to the imaginary part. Hence, the theoretical
     * LLR values for each bit are:
     *
     * LLR(b1) = 2 * sqrt(2) * Re(x) / N0
     * LLR(b0) = 2 * sqrt(2) * Im(x) / N0
     *
     * @param out_buf Buffer where the output quantized LLRs should be stored.
     * @param in_buf Buffer containing the input QPSK symbols.
     * @param N0 Noise energy per complex dimension extracted from the estimated Es/N0.
     * @param n_syms Number of QPSK symbols to soft-decode.
     */
    void
    demap_soft(int8_t* out_buf, const gr_complex* in_buf, unsigned int n_syms, float N0)
    {
        float scalar = 2 * M_SQRT2 / N0;
        volk_32f_s32f_convert_8i(
            out_buf, reinterpret_cast<const float*>(in_buf), scalar, n_syms * 2);
    }

    /**
     * @overload
     * @param out_buf Buffer where the output quantized LLRs should be stored.
     * @param in_syms Vector containing the input QPSK symbols.
     * @param N0 Noise energy per complex dimension extracted from the estimated Es/N0.
     */
    void demap_soft(int8_t* out_buf, const volk::vector<gr_complex>& in_syms, float N0)
    {
        demap_soft(out_buf, in_syms.data(), in_syms.size(), N0);
    }

    /**
     * @brief Estimate the linear SNR of input QPSK symbols.
     *
     * Slices the input symbols with hard-demapping and uses the resulting sliced symbols
     * as the reference (ideal constellation points) for the measurement.
     *
     * @param in_syms Buffer containing the input QPSK symbols.
     * @param n_syms Number of QPSK symbols available on the input buffer.
     * @return float Estimated linear SNR.
     * @note Use the `estimate_snr()` overloaded method with the ref_llrs arguments to
     * estimate the post-decoder SNR when decoded LLRs are available to obtain more
     * accurate reference constellation points.
     */
    float estimate_snr(const gr_complex* in_syms, unsigned int n_syms)
    {
        slice(d_aux_32fc_buffer.data(), in_syms, n_syms);
        return _estimate_snr(in_syms, d_aux_32fc_buffer.data(), n_syms);
    }

    /**
     * @overload
     * @param in_syms Vector containing the input QPSK symbols.
     */
    float estimate_snr(const volk::vector<gr_complex>& in_syms)
    {
        return estimate_snr(in_syms.data(), in_syms.size());
    }

    /**
     * @brief Estimate the linear SNR based on input QPSK symbols and reference LLRs.
     *
     * Uses the input reference LLRs (e.g., out of the LDPC decoder) to obtain the
     * reference constellation points. Then, measures the error between the input QPSK
     * symbols and the reference constellation points to estimate the linear SNR.
     *
     * @param in_syms Buffer containing the input QPSK symbols.
     * @param ref_llrs Buffer containing the reference LLRs.
     * @param n_syms Number of QPSK symbols available on the input buffer.
     * @return float Estimated linear SNR.
     */
    float
    estimate_snr(const gr_complex* in_syms, const int8_t* ref_llrs, unsigned int n_syms)
    {
        // Remap the LLRs back to reference QPSK constellation symbols. Unfortunately,
        // there is no binary slicer volk kernel for 8i type, so we convert the 8i LLRs to
        // 32f values, then slice these by reusing the slice() method. Note this entails
        // two conversions unnecessarily, from 8i to 32f and back.
        //
        // TODO Implement a volk kernel to slice 8i LLRs directly.
        gr_complex* p_ref_syms = d_aux_32fc_buffer.data();
        volk_8i_s32f_convert_32f(
            reinterpret_cast<float*>(p_ref_syms), ref_llrs, 1.0, n_syms * 2);
        slice(p_ref_syms, p_ref_syms, n_syms);
        return _estimate_snr(in_syms, p_ref_syms, n_syms);
    }

    /**
     * @overload
     * @param in_syms Vector containing the input QPSK symbols.
     * @param ref_llrs Vector containing the reference LLRs.
     */
    float estimate_snr(const volk::vector<gr_complex>& in_syms,
                       const volk::vector<int8_t>& ref_llrs)
    {
        if (in_syms.size() * 2 != ref_llrs.size())
            throw std::invalid_argument("Input symbols and LLRs must have matching size");
        return estimate_snr(in_syms.data(), ref_llrs.data(), in_syms.size());
    }
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_CONSTELLATIONS_H */