/* -*- c++ -*- */
/*
 * Copyright 2021 2019-2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "symbol_sync_cc_impl.h"
#include <gnuradio/filter/firdes.h>
#include <gnuradio/io_signature.h>

namespace gr {
namespace dvbs2rx {

bool greater_than_or_equal(const uint64_t& value, const uint64_t& element)
{
    return element >= value;
}


gr_complex linear_interpolator::operator()(const gr_complex* in, int m_k, float mu) const
{
    assert(m_k >= 0);
    // Linear interpolation from Eq. 8.61
    return mu * in[m_k + 1] + (1 - mu) * in[m_k];
}

gr_complex
quadratic_interpolator::operator()(const gr_complex* in, int m_k, float mu) const
{
    assert((m_k - 2) >= 0);
    // Farrow coefficients from Table 8.4.1
    constexpr float coef2[4] = { .5, -.5, -.5, .5 };
    constexpr float coef1[4] = { -.5, 1.5, -.5, -.5 };
    // Inner sum v(l) from Eq. 8.76 for l ranging from 0 to 2
    gr_complex v2, v1, v0;
    for (int i = 0; i < 4; i++) {
        v2 += in[m_k + 1 - i] * coef2[i];
        v1 += in[m_k + 1 - i] * coef1[i];
    }
    v0 = in[m_k - 1];
    // Piecewise parabolic interpolation from Eq. 8.77
    return (((mu * v2) + v1) * mu) + v0;
}


gr_complex cubic_interpolator::operator()(const gr_complex* in, int m_k, float mu) const
{
    assert((m_k - 2) >= 0);
    // Farrow coefficients from Table 8.4.2
    constexpr float coef3[4] = { (1.0 / 6), -.5, .5, -(1.0 / 6) };
    constexpr float coef2[4] = { 0.0, .5, -1.0, .5 };
    constexpr float coef1[4] = { -(1.0 / 6), 1.0, -.5, -(1.0 / 3) };
    // Inner sum v(l) from Eq. 8.76 for l ranging from 0 to 3
    gr_complex v3, v2, v1, v0;
    for (int i = 0; i < 4; i++) {
        v3 += in[m_k + 1 - i] * coef3[i];
        v2 += in[m_k + 1 - i] * coef2[i];
        v1 += in[m_k + 1 - i] * coef1[i];
    }
    v0 = in[m_k - 1];
    // Cubic interpolation from Eq. 8.78
    return (((((mu * v3) + v2) * mu) + v1) * mu) + v0;
}

static int calc_rrc_subfilt_len(float sps, int rrc_delay, size_t n_subfilt)
{
    return std::ceil(((2 * n_subfilt * sps * rrc_delay) + 1) / n_subfilt);
}

polyphase_interpolator::polyphase_interpolator(float sps,
                                               float rolloff,
                                               int rrc_delay,
                                               size_t n_subfilt)
    : base_interpolator<double>(calc_rrc_subfilt_len(sps, rrc_delay, n_subfilt) - 1),
      d_n_subfilt(n_subfilt),
      d_subfilt_len(calc_rrc_subfilt_len(sps, rrc_delay, n_subfilt)),
      d_subfilt_delay((d_subfilt_len - 1) / 2)
{
    // Design an RRC filter with an oversampling factor of "n_subfilt * sps"
    float poly_sps = n_subfilt * sps;
    size_t n_poly_rrc_taps = (2 * poly_sps * rrc_delay) + 1;
    std::vector<float> rrc_taps = filter::firdes::root_raised_cosine(
        n_subfilt, poly_sps, 1.0, rolloff, n_poly_rrc_taps);
    assert(rrc_taps.size() == n_poly_rrc_taps);

    // Zero-pad the filter to a length that is an integer multiple of "n_subfilt"
    size_t n_zero_pad = n_subfilt - (n_poly_rrc_taps % n_subfilt);
    rrc_taps.resize(n_poly_rrc_taps + n_zero_pad, 0.0);
    assert(rrc_taps.size() % n_subfilt == 0);

    // Apply the polyphase decomposition. That is, split the original filter taps into
    // "n_subfilt" subfilters, each representing a phase-offset RRC filter designed for an
    // oversampling of "sps". The symbol timing recovery loop will pick the appropriate
    // subfilter on each strobe according to its symbol timing offset estimate.
    size_t subfilt_len = rrc_taps.size() / n_subfilt;
    for (size_t i = 0; i < n_subfilt; i++) {
        volk::vector<float> subfilt(subfilt_len);
        for (size_t j = 0; j < subfilt_len; j++) {
            subfilt[j] = rrc_taps[i + (j * n_subfilt)];
        }
        d_rrc_subfilters.emplace_back(std::move(subfilt));
    }

    // Flip all subfilters to facilitate the convolution computation
    for (size_t i = 0; i < n_subfilt; i++) {
        std::reverse(d_rrc_subfilters[i].begin(), d_rrc_subfilters[i].end());
    }

    // Sanity checks
    assert(d_rrc_subfilters.size() == d_n_subfilt);
    assert(std::all_of(d_rrc_subfilters.begin(),
                       d_rrc_subfilters.end(),
                       [&](const volk::vector<float>& subfilt) {
                           return subfilt.size() == d_subfilt_len;
                       }));
    assert(d_subfilt_len % 2 == 1); // odd length (even-symmetric around the peak)
}

gr_complex
polyphase_interpolator::operator()(const gr_complex* in, int m_k, double mu) const
{
    int idx_subfilt = (int)std::floor(d_n_subfilt * mu);
    const volk::vector<float>& subfilt = d_rrc_subfilters[idx_subfilt];
    assert((m_k + 2 - d_subfilt_len) >= 0);
    gr_complex result;
    volk_32fc_32f_dot_prod_32fc(
        &result, &in[m_k + 2 - d_subfilt_len], subfilt.data(), d_subfilt_len);
    return result;
}


symbol_sync_cc::sptr symbol_sync_cc::make(float sps,
                                          float loop_bw,
                                          float damping_factor,
                                          float rolloff,
                                          int rrc_delay,
                                          int n_subfilt,
                                          int interp_method)
{
    return gnuradio::make_block_sptr<symbol_sync_cc_impl>(
        sps,
        loop_bw,
        damping_factor,
        rolloff,
        rrc_delay,
        n_subfilt,
        static_cast<interp_method_t>(interp_method));
}

// NOTE: All equations references that follow refer to the book "Digital Communications: A
//  Discrete-Time Approach", by Michael Rice.

void symbol_sync_cc_impl::set_gted_gain(float rolloff)
{
    // Gardner Timing Error Detector (GTED) gain
    //
    // Use Eq. (8.47) while assuming K=1 (unitary channel gain due to an AGC), Eavg=1
    // (unitary average symbol energy), and tau_e/Ts = 1/L, where "L" is a hypothetical
    // oversampling factor used for the S-curve evaluation (not the same as d_sps). Note
    // the Eavg=1 assumption holds for DVB-S2 BPSK, QPSK, and 8PSK constellations,
    // according to the standard. It also typically holds for 16APSK and 32APSK. although
    // the standard admits another Eavg options for these constellations.
    float L = 1e3;
    float C = sin(M_PI * rolloff / 2) / (4 * M_PI * (1 - (rolloff * rolloff / 4)));
    float delta_x = 2.0 / L;                   // small interval around the origin
    float delta_y = 8 * C * sin(2 * M_PI / L); // corresponding S-curve (y-axis) change
    d_Kp = delta_y / delta_x;                  // the gain is the slope around the origin
}

void symbol_sync_cc_impl::set_pi_constants(float loop_bw, float damping_factor)
{
    assert(d_Kp != -1); // GTED gain must be initialized

    // Loop bandwidth
    //
    // Assume the loop bandwidth represents Bn*Ts, i.e., the noise bandwidth normalized by
    // the symbol rate (1/Ts). Then, convert "Bn*Ts" (multiplied by symbol period Ts) to
    // "Bn*T" (multiplied by the sampling period T). Since T = Ts/sps, it follows that:
    float Bn_T = loop_bw / d_sps;

    // Definition of theta_n (See Eq. C.60)
    float theta_n = Bn_T / (damping_factor + (1.0 / (4 * damping_factor)));

    // Eq. C.56:
    float Kp_K0_K1 = (4 * damping_factor * theta_n) /
                     (1 + 2 * damping_factor * theta_n + (theta_n * theta_n));
    float Kp_K0_K2 = (4 * (theta_n * theta_n)) /
                     (1 + 2 * damping_factor * theta_n + (theta_n * theta_n));

    // Counter gain (analogous to a DDS gain)
    float K0 = -1; // negative because the counter is a decrementing counter

    // Finally, compute the PI contants:
    d_K1 = Kp_K0_K1 / (d_Kp * K0);
    d_K2 = Kp_K0_K2 / (d_Kp * K0);
}

/*
 * The private constructor
 */
symbol_sync_cc_impl::symbol_sync_cc_impl(float sps,
                                         float loop_bw,
                                         float damping_factor,
                                         float rolloff,
                                         int rrc_delay,
                                         int n_subfilt,
                                         interp_method_t interp_method)
    : gr::block("symbol_sync_cc",
                gr::io_signature::make(1, 1, sizeof(gr_complex)),
                gr::io_signature::make(1, 1, sizeof(gr_complex))),
      d_sps(sps),
      d_midpoint(sps / 2),
      d_K1(-1), // assume -1 means uninitialized for K1, K2, and Kp
      d_K2(-1),
      d_Kp(-1),
      d_vi(0.0),
      d_nominal_step(1.0 / sps),
      d_cnt(1.0 - d_nominal_step), // modulo-1 counter (always ">= 0" and "< 1")
      d_mu(0),
      d_jump(d_sps),
      d_init(false),
      d_last_xi(0),
      d_interp_method(interp_method),
      d_poly_interp(sps, rolloff, rrc_delay, n_subfilt)
{
    if ((ceilf(sps) != sps) || (floorf(sps) != sps) || (static_cast<int>(sps) % 2 != 0) ||
        (sps < 2.0)) {
        throw std::runtime_error("sps has to be an even integer >= 2");
    }

    // Define the loop constants.
    set_gted_gain(rolloff);
    set_pi_constants(loop_bw, damping_factor);

    // The k-th interpolant is computed based on the n-th sample and some preceding
    // samples, including the k-th basepoint index "n-1". Make sure these samples are
    // available as input history if necessary. Also, since the GTED considers the
    // zero-crossing interpolant between the current and previous output symbols, make
    // sure the zero-crossing sample located "d_midpoint" indexes before the basepoint
    // index is also within the input buffer's history.
    switch (interp_method) {
    case interp_method_t::POLYPHASE:
        d_history = d_poly_interp.history() + d_midpoint;
        break;
    case interp_method_t::LINEAR:
        d_history = d_lin_interp.history() + d_midpoint;
        break;
    case interp_method_t::QUADRATIC:
        d_history = d_qua_interp.history() + d_midpoint;
        break;
    case interp_method_t::CUBIC:
        d_history = d_cub_interp.history() + d_midpoint;
        break;
    default:
        throw std::runtime_error("Invalid interpolation method (choose from 0 to 3)");
    }
    set_history(d_history + 1); // GR basic block's history is actually history + 1

    // The work function has to move tags from arbitrary sample instants to output
    // symbols/interpolants. Handle this propagation internally instead of letting the
    // scheduler attempt to do it.
    set_tag_propagation_policy(TPP_DONT);

    // Approximate output rate / input rate
    set_inverse_relative_rate(d_sps);
}

/*
 * Our virtual destructor.
 */
symbol_sync_cc_impl::~symbol_sync_cc_impl() {}

void symbol_sync_cc_impl::forecast(int noutput_items,
                                   gr_vector_int& ninput_items_required)
{
    ninput_items_required[0] = (d_sps * noutput_items) + d_history;
}

template <typename Interpolator>
std::pair<int, int> symbol_sync_cc_impl::loop(const gr_complex* in,
                                              gr_complex* out,
                                              int ninput_items,
                                              int noutput_items,
                                              const Interpolator& interp)
{
    if (noutput_items > static_cast<int>(d_strobe_idx.size()))
        d_strobe_idx.resize(noutput_items);

    // Starting input index
    //
    // Each loop iteration advances to the next strobe by jumping indexes according to the
    // jump value held at "d_jump", which persists across loop calls. At this point,
    // "d_jump" holds the jump required from the last strobe of the previous batch to the
    // first strobe of the current batch. For example, if the last sample processed in the
    // previous batch was "n=4095" and "jump=2", ordinarily, neglecting the block history,
    // this call would need to start at index "n = jump - 1" (i.e., "n=1", the second
    // sample index). However, since the input buffer holds the sample history, the second
    // new input sample is not at index "jump - 1", but at "jump - 1 + d_history". For
    // instance, with a history of one sample (i.e., d_history=1), in[0] holds the history
    // sample, and the second new input sample is at in[2]. Hence, initialize n to
    // "d_history - 1" and let the loop add the jump to obtain "n = jump - 1 + d_history".
    int n = d_history - 1; // Input (sample-spaced) index
    int k = 0;             // Output (symbol-spaced) index (or interpolant index)

    // On startup, initialize the first interpolant and start the loop from the second
    // strobe/interpolant onwards so that the TED can access "d_last_xi". By doing so,
    // this implementation matches relative to the reference MATLAB implementation
    // verified on QA tests (see the "test_reference_implementation" test case). Other
    // simpler alternatives like setting d_last_xi=0 would get rid of the conditional
    // below but would lead to a mismatch relative to the reference implementation.
    if (!d_init) {
        if (ninput_items < (int)d_history) {
            return std::make_pair(0, 0);
        }
        // Assume the loop starts at "n=d_history + 1" on startup so that the first
        // iteration can read the preceding indexes for interpolation. Also, note d_mu=0
        // on startup, so the linear interpolator produces "in[d_history]" as its first
        // interpolant, which is the first new input sample of the first input batch.
        d_last_xi = in[d_history];
        d_init = true;
        n += 2; // assume the loop starts at "n=d_history + 1"
    }

    while ((n + d_jump) < ninput_items && k < noutput_items) {
        // This loop jumps from strobe to strobe, so every iteration processes a strobe
        // index and produces an interpolated symbol in the output. Index n is always a
        // post-underflow index, and the basepoint index is the preceding index.
        n += d_jump;
        int m_k = n - 1;       // basepoint index
        d_strobe_idx[k] = m_k; // save the strobe index to use later when placing tags
        // NOTE: define the strobe index as the basepoint index, following the definition
        // on Michael Rice's book. If we wanted to define the strobe index as the closest
        // sample index relative to the output interpolant, we could set it equal to the
        // basepoint index m_k whenever "d_mu < 0.5" and m_k + 1 otherwise. However, it's
        // better to avoid any unnecessary computations in this loop.

        // Output interpolant
        out[k] = interp(in, m_k, d_mu);

        // Zero-crossing interpolant
        gr_complex x_zc = interp(in, m_k - d_midpoint, d_mu);

        // Error detected by the Gardner TED (purely non-data-aided)
        float e = x_zc.real() * (d_last_xi.real() - out[k].real()) +
                  x_zc.imag() * (d_last_xi.imag() - out[k].imag());
        d_last_xi = out[k++];

        // Loop filter
        double vp = d_K1 * e;      // Proportional
        d_vi += (d_K2 * e);        // Integral
        double pi_out = vp + d_vi; // PI Output

        // NOTE: the PI output is "vp + vi" on a strobe index (when a new interpolant is
        // computed and the TED error is evaluated), and simply "vi" on the other indexes
        // (when e = 0). Hence, the counter step briefly changes to "(1/L + vp + vi)" on a
        // strobe index and then changes back to "(1/L + vi)" on the remaining indexes.
        // Both counter steps must be taken into account when calculating how many
        // iterations until the counter underflows again.
        double W1 = d_nominal_step + pi_out;
        double W2 = d_nominal_step + d_vi;
        assert(W1 > 0);
        assert(W2 > 0);
        // NOTE: W1 and W2 can become negative when the loop bandwidth is too wide.

        // Iterations to underflow the modulo-1 counter
        //
        // As noted above, the counter decrements by W1 on the strobe iteration and by W2
        // on the remaining iterations.
        d_jump = std::floor((d_cnt - W1) / W2) + 2;
        assert(d_jump > 0);

        if (d_jump > 1) {
            // Counter value on the next basepoint index (before the next underflow)
            double cnt_basepoint = d_cnt - W1 - ((d_jump - 2) * W2);
            assert(cnt_basepoint >= 0);

            // Update the fractional symbol timing offset estimate using Eq. (8.89).
            d_mu = cnt_basepoint / W2;

            // Counter value after the underflow (and the corresponding mod-1 wrap-around)
            d_cnt = cnt_basepoint - W2 + 1;
        } else {
            // Same as above, but assuming the counter underflows with a single step W1,
            // in which case the basepoint count is simply d_cnt.
            d_mu = d_cnt / W1;
            d_cnt = d_cnt - W1 + 1;
        }
        // d_mu is the ratio between the mod-1 counter value at the basepoint index and
        // the counter step (W1 or W2) that leads to underflow in the next cycle. Hence,
        // the denominator is always greater than the numerator, otherwise the counter
        // would not underflow. However, due to numerical errors, d_mu may end up being
        // equal to 1.0. To avoid that as much as possible, we use double for the mod-1
        // counter arithmetic (W1, W2, d_cnt, cnt_basepoint, and d_mu) instead of float.
        assert(d_mu >= 0 && d_mu < 1.0);
    }

    return std::make_pair(n, k);
}

std::pair<int, int> symbol_sync_cc_impl::loop(const gr_complex* in,
                                              gr_complex* out,
                                              int ninput_items,
                                              int noutput_items)
{
    switch (d_interp_method) {
    case interp_method_t::POLYPHASE:
        return loop(in, out, ninput_items, noutput_items, d_poly_interp);
        break;
    case interp_method_t::LINEAR:
        return loop(in, out, ninput_items, noutput_items, d_lin_interp);
        break;
    case interp_method_t::QUADRATIC:
        return loop(in, out, ninput_items, noutput_items, d_qua_interp);
        break;
    case interp_method_t::CUBIC:
        return loop(in, out, ninput_items, noutput_items, d_cub_interp);
        break;
    default:
        throw std::runtime_error("Invalid interpolation method (choose from 0 to 3)");
    }
}

int symbol_sync_cc_impl::general_work(int noutput_items,
                                      gr_vector_int& ninput_items,
                                      gr_vector_const_void_star& input_items,
                                      gr_vector_void_star& output_items)
{
    const gr_complex* in = reinterpret_cast<const gr_complex*>(input_items[0]);
    gr_complex* out = reinterpret_cast<gr_complex*>(output_items[0]);

    // Call the main loop
    int n, k;
    std::tie(n, k) = loop(in, out, ninput_items[0], noutput_items);
    if (n == 0)
        return 0;

    // Consumed input samples
    const int n_consumed = n + 1 - d_history;
    // NOTE: if we stop at, say, n=7, it means we consumed n+1=8 samples. However, with
    // d_history=1, the first sample n=0 is the history from the previous input buffer
    // batch, so the total of consumed samples is only n.

    // Propagate tags
    const unsigned int input_port = 0;
    const unsigned int output_port = 0;
    const uint64_t n_read = nitems_read(input_port);
    const uint64_t n_written = nitems_written(output_port);
    std::vector<tag_t> tags;
    get_tags_in_range(tags, input_port, n_read, n_read + n_consumed);
    tags.insert(tags.begin(), d_pending_tags.begin(), d_pending_tags.end());
    if (!d_pending_tags.empty())
        d_pending_tags.clear();

    // The incoming tag offsets are oblivious to this block's history. For instance,
    // tag offset 0 refers to the first new input sample. In contrast, the strobe
    // indexes saved on vector "d_strobe_idx" are offset by the input buffer history.
    // Hence, account for the buffer history on the target strobe index.
    //
    // When using the polyphase interpolator, consider also the subfilter delay. The
    // interpolator processes samples "n - N + 1" to "n" (inclusive), where N is the
    // subfilter length. However, the RRC subfilter has a peak in its central point when
    // mu < 0.5 and at the center point minus one (a shorter delay) for mu > 0.5. Thus,
    // the output interpolant is more strongly influenced by either sample "n - D", where
    // "D" is the subfilter delay (from d_subfilt_delay), or sample "n - D + 1". In terms
    // of the basepoint index, the interpolant is more strongly influenced by the sample
    // at "m_k + 1 - D" for mu < 0.5, and "m_k + 2 - D" for mu > 0.5. Again, as for the
    // other interpolation methods, assume the case of mu < 0.5 for simplicity.
    unsigned int strobe_offset = (d_interp_method == interp_method_t::POLYPHASE)
                                     ? (d_history + d_poly_interp.get_subfilt_delay() - 1)
                                     : d_history;
    for (auto& tag : tags) {
        uint64_t target_strobe_idx = tag.offset - n_read + strobe_offset;
        // Find the first strobe index past or equal the target
        const auto last = d_strobe_idx.begin() + k;
        const auto it = std::upper_bound(
            d_strobe_idx.begin(), last, target_strobe_idx, greater_than_or_equal);

        if (it != last) {
            tag.offset = n_written + (it - d_strobe_idx.begin());
            add_item_tag(output_port, tag);
        } else {
            // The tag does not have a strobe in this work. Save it for the next call.
            d_pending_tags.push_back(tag);
        }
    }

    // Tell runtime how many input items we consumed.
    consume_each(n_consumed);

    // Tell runtime system how many output items we produced.
    return k;
}

} /* namespace dvbs2rx */
} /* namespace gr */
