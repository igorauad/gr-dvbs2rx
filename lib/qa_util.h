/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_QA_UTIL_H
#define INCLUDED_DVBS2RX_QA_UTIL_H

#include <gnuradio/expj.h>
#include <gnuradio/gr_complex.h>
#include <random>

namespace gr {
namespace dvbs2rx {

class NoisyChannel
{
private:
    gr_complex phase_inc;    /**< Phase increment used by the rotator */
    gr_complex phasor;       /**< Phasor updated by the rotator */
    std::random_device seed; /**< Random seed generator */
    std::mt19937 prgn;       /**< Pseudo-random number engine */
    std::normal_distribution<float> normal_dist_gen; /**< Normal distribution */

    /**
     * \brief Update the Es/N0 used by the noise generator.
     * \param esn0_db (float) Target Es/N0 in dB.
     */
    void set_esn0(float esn0_db)
    {
        constexpr float es = 1; // asssume unitary Es
        float esn0 = pow(10, (esn0_db / 10));
        float n0 = es / esn0;
        // n0 is the variance of the complex AWGN noise. Since the noise is
        // zero-mean, its variance is equal to E[|noise|^2]. In other words,
        // E[|noise|^2]=N0. However, note the noise can be expressed as
        // "alpha*(norm_re + j*norm_im)", where norm_re and norm_im are
        // independent normal random variables, and alpha is a scaling factor
        // determining the standard deviation per dimension. Hence,
        //
        // E[|noise|^2] = (alpha^2)*(E[|noise_re|^2] + E[|noise_im|^2])
        //              = (alpha^2)*(2)
        //           N0 = 2 * alpha^2.
        //
        // Thus, in order to generate complex noise with variance N0, the
        // scaling factor should be "alpha = sqrt(N0/2)."
        float sdev_per_dim = sqrt(n0 / 2);
        normal_dist_gen = std::normal_distribution<float>(0, sdev_per_dim);
    }

public:
    /**
     * \brief Construct a noisy channel object.
     * \param esn0_db (float) Target Es/N0 in dB.
     * \param freq_offset (float) Target normalized frequency offset.
     * \param phase_offset (float) Target initial phase offset.
     */
    explicit NoisyChannel(float esn0_db, float freq_offset, float phase_offset = 0)
        : phase_inc(gr_expj(2 * M_PI * freq_offset)),
          phasor(gr_expj(phase_offset)),
          prgn(seed())
    {
        set_esn0(esn0_db);
    }

    /**
     * \brief Set a random phase uniformly distributed from [-pi, pi).
     */
    void set_random_phase()
    {
        std::uniform_real_distribution<> uniform_dist_gen(-M_PI, M_PI);
        phasor = gr_expj(uniform_dist_gen(prgn));
    }

    /**
     * \brief Add AWG noise to a vector of complex symbols.
     * \param in (gr_complex* in) Pointer to input symbol buffer.
     * \param n_symbols (int) Number of symbols to process.
     */
    void add_noise(gr_complex* in, unsigned int n_symbols)
    {
        for (size_t i = 0; i < n_symbols; i++) {
            in[i] += gr_complex(normal_dist_gen(prgn), normal_dist_gen(prgn));
        }
    }

    /**
     * \brief Add frequency and phase offset to a vector of complex symbols.
     * \param in (gr_complex* out) Pointer to the output buffer.
     * \param in (const gr_complex* in) Pointer to the input buffer.
     * \param n_symbols (int) Number of symbols to process.
     * \note The phase obtained after rotation is kept internally such that, in
     * the next call, it starts from where it stopped.
     */
    void rotate(gr_complex* out, const gr_complex* in, unsigned int n_symbols)
    {
        volk_32fc_s32fc_x2_rotator_32fc(out, in, phase_inc, &phasor, n_symbols);
    }
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_QA_UTIL_H */
