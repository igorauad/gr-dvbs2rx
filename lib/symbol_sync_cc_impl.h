/* -*- c++ -*- */
/*
 * Copyright 2021 2019-2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_SYMBOL_SYNC_CC_IMPL_H
#define INCLUDED_DVBS2RX_SYMBOL_SYNC_CC_IMPL_H

#include <gnuradio/dvbs2rx/symbol_sync_cc.h>
#include <gnuradio/gr_complex.h>
#include <volk/volk_alloc.hh>

namespace gr {
namespace dvbs2rx {

enum class interp_method_t : int {
    POLYPHASE = 0,
    LINEAR = 1,
    QUADRATIC = 2,
    CUBIC = 3,
};

template <typename T>
struct base_interpolator {
    base_interpolator(unsigned history) : d_history(history){};

    /**
     * @brief Compute the complex interpolant.
     *
     * @param in Input IQ sample buffer.
     * @param m_k Basepoint index.
     * @param mu Fractional timing offset estimate.
     * @return gr_complex Output interpolant.
     */
    virtual gr_complex operator()(const gr_complex* in, int m_k, T mu) const = 0;

    /**
     * @brief Get the interpolator history requirement.
     *
     * @return unsigned Historic (past) samples required to compute an interpolant.
     */
    unsigned history() const { return d_history; };

private:
    unsigned d_history;
};

constexpr unsigned hist_linear_interp = 1;    // accesses m_k = n - 1
constexpr unsigned hist_quadratic_interp = 3; // accesses m_k - 2 = n - 3
constexpr unsigned hist_cubic_interp = 3;     // accesses m_k - 2 = n - 3

struct linear_interpolator : public base_interpolator<float> {
    linear_interpolator() : base_interpolator(hist_linear_interp){};
    gr_complex operator()(const gr_complex* in, int m_k, float mu) const;
};

struct quadratic_interpolator : public base_interpolator<float> {
    quadratic_interpolator() : base_interpolator(hist_quadratic_interp){};
    gr_complex operator()(const gr_complex* in, int m_k, float mu) const;
};

struct cubic_interpolator : public base_interpolator<float> {
    cubic_interpolator() : base_interpolator(hist_cubic_interp){};
    gr_complex operator()(const gr_complex* in, int m_k, float mu) const;
};

struct polyphase_interpolator : public base_interpolator<double> {
    polyphase_interpolator(float sps, float rolloff, int rrc_delay, size_t n_subfilt);
    gr_complex operator()(const gr_complex* in, int m_k, double mu) const;
    // NOTE: on the polyphase interpolator, represent mu by a double (instead of float) to
    // avoid mu=1.0 that can result from numerical errors. While the other interpolators
    // can handle mu=1.0 (although the effects are TBC), the polyphase would certainly
    // segfault with mu=1.0, as that would lead to an out-of-range subfilter index.
    size_t get_subfilt_delay() const { return d_subfilt_delay; }

private:
    std::vector<volk::vector<float>> d_rrc_subfilters; /** Vector of RRC subfilters */
    size_t d_n_subfilt;     /** Number of subfilters in the polyphase RRC filter bank */
    size_t d_subfilt_len;   /** Number of taps in each RRC subfilter */
    size_t d_subfilt_delay; /** RRC subfilter delay */
};

class DVBS2RX_API symbol_sync_cc_impl : public symbol_sync_cc
{
private:
    int d_sps;             /**< Samples per symbol (oversampling ratio) */
    int d_midpoint;        /**< Midpoint index between interpolants */
    unsigned d_history;    /**< History of samples in the input buffer */
    float d_K1;            /**< PI filter's proportional constant */
    float d_K2;            /**< PI filter's integrator constant */
    float d_Kp;            /**< Gardner TED gain */
    double d_vi;           /**< Last integrator value */
    double d_nominal_step; /**< Nominal mod-1 counter step (equal to "1/d_sps") */
    double d_cnt;          /**< Modulo-1 counter */
    double d_mu;           /**< Fractional symbol timing offset estimate */
    int d_jump;            /**< Samples to jump until the next strobe */
    bool d_init;           /**< Whether the loop is initialized (after the first work) */
    gr_complex d_last_xi;  /**< Last output interpolant */
    std::vector<int> d_strobe_idx;     /**< Indexes of the output interpolants */
    std::vector<tag_t> d_pending_tags; /**< Pending tags from the previous work */

    // Interpolators
    //
    // NOTE: The synchronizer uses a single interpolator defined by the interp_method
    // parameter. However, this class includes all interpolators as members so that these
    // can be passed by reference to the templated loop function defined below. This
    // approach allows the compiler to know exactly which interpolator functor is used for
    // each templated loop instantiation. Consequently, the compiler can inline the
    // interpolation calls, which is so much more important than saving memory here.
    interp_method_t d_interp_method;
    linear_interpolator d_lin_interp;
    quadratic_interpolator d_qua_interp;
    cubic_interpolator d_cub_interp;
    polyphase_interpolator d_poly_interp;

    void set_gted_gain(float rolloff);
    void set_pi_constants(float loop_bw, float damping_factor);


public:
    symbol_sync_cc_impl(float sps,
                        float loop_bw,
                        float damping_factor,
                        float rolloff,
                        int rrc_delay,
                        int n_subfilt,
                        interp_method_t interp_method);
    ~symbol_sync_cc_impl();

    void forecast(int noutput_items, gr_vector_int& ninput_items_required);

    int general_work(int noutput_items,
                     gr_vector_int& ninput_items,
                     gr_vector_const_void_star& input_items,
                     gr_vector_void_star& output_items);

    template <typename Interpolator>
    std::pair<int, int> loop(const gr_complex* in,
                             gr_complex* out,
                             int ninput_items,
                             int noutput_items,
                             const Interpolator& interp);

    std::pair<int, int>
    loop(const gr_complex* in, gr_complex* out, int ninput_items, int noutput_items);
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_SYMBOL_SYNC_CC_IMPL_H */
