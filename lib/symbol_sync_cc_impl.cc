/* -*- c++ -*- */
/*
 * Copyright 2021 2019-2021 Igor Freire.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "symbol_sync_cc_impl.h"
#include <gnuradio/io_signature.h>

namespace gr {
namespace dvbs2rx {

bool greater_than_or_equal(const uint64_t& value, const uint64_t& element)
{
    return element >= value;
}

symbol_sync_cc::sptr
symbol_sync_cc::make(float sps, float loop_bw, float damping_factor, float rolloff)
{
    return gnuradio::make_block_sptr<symbol_sync_cc_impl>(
        sps, loop_bw, damping_factor, rolloff);
}


/*
 * The private constructor
 */
symbol_sync_cc_impl::symbol_sync_cc_impl(float sps,
                                         float loop_bw,
                                         float damping_factor,
                                         float rolloff)
    : gr::block("symbol_sync_cc",
                gr::io_signature::make(1, 1, sizeof(gr_complex)),
                gr::io_signature::make(1, 1, sizeof(gr_complex))),
      d_sps(sps),
      d_midpoint(sps / 2),
      d_vi(0.0),
      d_nominal_step(1.0 / sps),
      d_cnt(1.0 - d_nominal_step), // modulo-1 counter (always ">= 0" and "< 1")
      d_mu(0),
      d_offset(2), // Start at "n=2" so that the first iteration can read index "n=1"
                   // required for the interpolant computation. Also, note "n=0" is the
                   // history index (last sample from the previous batch), so "n=1" is
                   // actually the first new input sample.
      d_init(false),
      d_last_xi(0)
{
    if ((ceilf(sps) != sps) || (floorf(sps) != sps) || (static_cast<int>(sps) % 2 != 0) ||
        (sps < 2.0)) {
        throw std::runtime_error("sps has to be an even integer >= 2");
    }

    // Define the loop constants. All equations references that follow refer to the book
    // "Digital Communications: A Discrete-Time Approach", by Michael Rice.

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
    float Kp = delta_y / delta_x;              // slope around the origin

    // Finally, compute the PI contants:
    d_K1 = Kp_K0_K1 / (Kp * K0);
    d_K2 = Kp_K0_K2 / (Kp * K0);

    // The interpolant is computed based on the n-th sample and the sample at index "n-1",
    // where "n-1" is the basepoint index. Make sure the basepoint index is always
    // available as history if necessary.
    set_history(2);

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
    ninput_items_required[0] = (d_sps * noutput_items) + history() - 1;
}

std::pair<int, int> symbol_sync_cc_impl::loop(const gr_complex* in,
                                              gr_complex* out,
                                              int ninput_items,
                                              int noutput_items)
{
    if (noutput_items > static_cast<int>(d_strobe_idx.size()))
        d_strobe_idx.resize(noutput_items);

    int n = d_offset; // Input (sample-spaced) index
    int k = 0;        // Output (symbol-spaced) index (or interpolant index)

    // On startup, initialize the first interpolant and start the loop from the second
    // strobe/interpolant onwards so that the TED can access "d_last_xi". By doing so,
    // this implementation matches relative to the reference MATLAB implementation tested
    // in QA (see the "test_reference_implementation" test case). Other simpler
    // alternatives like setting d_last_xi=0 would get rid of the conditional below but
    // would lead to a mismatch relative to the reference implementation.
    if (!d_init) {
        if (ninput_items < d_offset) {
            return std::make_pair(0, 0);
        }
        d_last_xi = in[d_offset - 1]; // n=d_offset, but d_mu=0 on startup, so the linear
                                      // interpolator takes "in[d_offset - 1] = in[n -1]".
        d_init = true;
        n += d_sps;
    }


    int jump = 0;
    while ((n + jump) < ninput_items && k < noutput_items) {
        // This loop jumps from strobe to strobe, so every iteration processes a strobe
        // index and produces an interpolated symbol in the output. Index n is always a
        // post-underflow index, and the basepoint index is the preceding index.
        n += jump;
        int m_k = n - 1; // basepoint index

        // Output interpolant (using the linear interpolator from Eq. 8.61)
        out[k] = d_mu * in[n] + (1 - d_mu) * in[m_k];

        // Strobe index (where in the input stream the strobe was asserted)
        //
        // NOTE: this information is used later when deciding where to place incoming
        // tags. Since the incoming tag offsets are oblivious to this block's history
        // (e.g., tag offset 0 is really the first input sample), make sure to store the
        // strobe indexes after substracting the history. Also, consider the strobe index
        // to be the index after the basepoint index. If we wanted to be more accurate, we
        // could use the basepoint index m_k if "d_mu < 0.5" and m_k + 1 otherwise.
        // However, it's better to avoid any unnecessary computations in this loop.
        d_strobe_idx[k] = n - 1; // same as "m_k + 1 - (history() - 1)"

        // Zero-crossing interpolant
        gr_complex x_zc = d_mu * in[n - d_midpoint] + (1 - d_mu) * in[m_k - d_midpoint];

        // Error detected by the Gardner TED (purely non-data-aided)
        float e = x_zc.real() * (d_last_xi.real() - out[k].real()) +
                  x_zc.imag() * (d_last_xi.imag() - out[k].imag());
        d_last_xi = out[k++];

        // Loop filter
        float vp = d_K1 * e;      // Proportional
        d_vi += (d_K2 * e);       // Integral
        float pi_out = vp + d_vi; // PI Output

        // NOTE: the PI output is "vp + vi" on a strobe index (when e != 0), and simply
        // "vi" on the other indexes (when e = 0). Hence, the counter step briefly changes
        // to "(1/L + vp + vi)" on a strobe index and then changes back to "(1/L + vi)" on
        // the remaining indexes. Both counter steps must be taken into account when
        // calculating how many iterations until the counter underflows again.
        float W1 = d_nominal_step + pi_out;
        float W2 = d_nominal_step + d_vi;
        assert(W1 > 0);
        assert(W2 > 0);
        // NOTE: W1 and W2 can become negative when the loop bandwidth is too wide.

        // Iterations to underflow the modulo-1 counter
        //
        // As noted above, the counter decrements by W1 on the strobe iteration and by W2
        // on the remaining iterations.
        jump = std::floor((d_cnt - W1) / W2) + 2;
        assert(jump > 0);

        if (jump > 1) {
            // Counter value on the next basepoint index (before the next underflow)
            float cnt_basepoint = d_cnt - W1 - ((jump - 2) * W2);
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
    }

    // Next time, start after jumping indexes. For example, if the last sample here is
    // "n=4095" and "jump=2", start at index n=2 on the next batch. The rationale is that
    // n=2 is the second sample after the buffer history of one sample (saved at n=0).
    d_offset = jump;
    assert(d_offset >= 0);

    return std::make_pair(n, k);
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

    // Propagate tags
    const unsigned int input_port = 0;
    const unsigned int output_port = 0;
    const uint64_t n_read = nitems_read(input_port);
    const uint64_t n_written = nitems_written(output_port);
    std::vector<tag_t> tags;
    get_tags_in_range(tags, input_port, n_read, n_read + n);
    for (auto& tag : tags) {
        // Find the first strobe index past or equal to the original tag offset
        const auto last = d_strobe_idx.begin() + k;
        const auto it = std::upper_bound(
            d_strobe_idx.begin(), last, tag.offset - n_read, greater_than_or_equal);
        if (it != last) {
            tag.offset = n_written + (it - d_strobe_idx.begin());
            add_item_tag(output_port, tag);
        }
    }

    // Tell runtime how many input items we consumed.
    consume_each(n);
    // NOTE: if we stop at, say, n=7, it means we consumed n+1=8 samples. However, the
    // first sample n=0 is the history, so the total of consumed samples is only n.

    // Tell runtime system how many output items we produced.
    return k;
}

} /* namespace dvbs2rx */
} /* namespace gr */
