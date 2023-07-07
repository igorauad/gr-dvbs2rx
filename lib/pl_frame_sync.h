/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_PL_FRAME_SYNC_H
#define INCLUDED_DVBS2RX_PL_FRAME_SYNC_H

#include "cdeque.h"
#include "delay_line.h"
#include "pl_defs.h"
#include "pl_submodule.h"
#include <gnuradio/dvbs2rx/api.h>
#include <gnuradio/gr_complex.h>
#include <chrono>

/* correlator lengths, based on the number of differentials that we know in
 * advance (25 for SOF and only 32 for PLSC) */
#define SOF_CORR_LEN (SOF_LEN - 1)
#define PLSC_CORR_LEN (PLSC_LEN / 2)

namespace gr {
namespace dvbs2rx {


enum class frame_sync_state_t {
    searching, // searching for a cross-correlation peak
    found,     // found a peak but the lock is not confirmed yet
    locked     // lock confirmed
};

/**
 * \brief Frame Synchronizer
 *
 * Searches for the start of PLFRAMEs by computing and adding independent
 * cross-correlations between the SOF and the PLSC parts of the PLHEADER with
 * respect to their expected values known a priori. The cross-correlations are
 * based on the so-called *differential* metric given by `x[n]*conj(x[n+1])`,
 * namely based on the angle difference between consecutive symbols. This
 * non-coherent approach allows for frame synchronization despite the presence
 * of large frequency offsets. That's crucial because the fine frequency offset
 * estimation requires correct decoding of the PLSC, which, in turn, requires
 * the frame search provided by the frame synchronizer. Hence, the frame
 * synchronizer should act first, before the carrier recovery.
 *
 * Due to the interleaved Reed-Muller codeword construction (with a XOR given by
 * the 7th PLSC bit), each pair of consecutive PLSC bits is either of equal or
 * opposite bits. When the 7th PLSC bit is 0, all pairs of bits are equal, i.e.,
 * `b[2i+1] = b[2i]`. In this case, the corresponding pair of scrambled bits are
 * either equal to the original scrambler bits (when `b[2i] = b[2i+1] = 0`) or
 * their opposite (when `b[2i] = b[2i+1] = 1`). In either case, the complex
 * differential `x[2i]*conj(x[2i+1])` is the same due to the pi/2 BPSK mapping
 * (refer to the even-to-odd mapping rules in the body of function
 * `demap_bpsk_diff()` from `pi2_bpsk.cc`). Hence, when the 7th PLSC bit is 0,
 * the differential is determined by the scrambler sequence, not the actual PLSC
 * value that is unknown at this point.
 *
 * Next, consider the case when the 7th PLSC bit is 1 such that each pair of
 * PLSC bits is composed of opposite bits, i.e., `b[2i+1] = !b[2i]`. In this
 * case, if `b[2i] = 0` and `b[2i+1] = 1`, the pair of scrambled bits becomes
 * `(s[2i], !s[2i+1])`. Otherwise, if `b[2i] = 1` and `b[2i+1] = 0`, the pair of
 * scrambled bits becomes `(!s[2i], s[2i+1])`. In either case, the complex
 * differential is equal to the differential due to the scrambler sequence
 * alone, but shifted by 180 degrees (i.e., by `expj(j*pi)`, due to the pi/2
 * BPSK mapping rules (again, see `demap_bpsk_diff()`). Thus, if the
 * differentials due to the PLSC scrambler sequence are used as the correlator
 * taps, the cross-correlation still yields a peak when processing the PLSC. The
 * only difference is that the phase of the peak will be shifted by 180 degrees,
 * but the magnitude will be the same. In this case, the 180-degree shift can be
 * undone by taking the negative of the correlator peak.
 *
 * In the end, the PLSC correlator is implemented based on the scrambler
 * sequence alone (known a priori), and it is independent of the actual PLSC
 * embedded on each incoming PLHEADER. This correlator is composed of 32 taps
 * only, given that only the pairwise PLSC differentials are known a priori. In
 * contrast, the SOF correlator is based on all the 25 known SOF differentials,
 * given that the entire 26-symbol SOF sequence is known a priori.
 *
 * The two correlators (SOF and PLSC) are expected to peak when they observe the
 * SOF or PLSC in the input symbol sequence. The final timing metric is given by
 * the sum or difference of these correlators, whichever has the largest
 * magnitude. The sum (SOF + PLSC) peaks when the 7th PLSC bit is 0, and the
 * difference (SOF - PLSC) peaks when the 7th PLSC bit is 1. That is, the
 * difference metric essentially undoes the 180-degree shift on the PLSC
 * correlator peak that would arise when the 7th bit is 1.
 *
 * Furthermore, as stated before, the implementation is robust to frequency
 * offsets. The input symbol sequence can have any frequency offset, as long as
 * it doesn't change significantly in the course of the PLHEADER, which is
 * typically the case given that the PLHEADER is short enough (for typical
 * DVB-S2 baud rates). If the frequency offset is the same for symbols x[n] and
 * x[n+1], the differential metric includes a factor given by:
 *
 * ```
 * exp(j*2*pi*f0*n) * conj(exp(j*2*pi*f0*(n+1))) = exp(-j*2*pi*f0).
 * ```
 *
 * Moreover, if the frequency offset remains the same over the entire PLHEADER,
 * all differentials include this factor. Ultimately, the cross-correlation peak
 * is still observed, just with a different phase (shifted by `-2*pi*f0`). In
 * fact, the phase of the complex timing metric (sum or difference between the
 * correlator peaks) could be used to estimate the coarse frequency offset
 * affecting the PLHEADER. However, a better method is implemented on the
 * dedicated `freq_sync` class.
 *
 * Lastly, aside from the correlators, the implementation comprises a state
 * machine with three states: "searching", "found", and "locked". As soon as an
 * SOF is found, the state machine changes to the "found" state. At this point,
 * the caller should decode the corresponding PLSC and call method
 * `set_frame_len()` to inform the expected PLFRAME length following the
 * detected SOF. Then, if the next SOF comes exactly after the informed frame
 * length, the state machine changes into the "locked" state. From this point
 * on, the frame synchronizer will check the correlation peak (i.e., the
 * so-called "timing metric") at the expected index on every frame.
 *
 * Whenever the timing metric does not exceed a specific magnitude threshold,
 * the implementation will increment an internal count for unlocking. After a
 * chosen number of consecutive timing metric failures, this block will assume
 * the frame lock has been lost and transition back to the "searching" state. At
 * this point, it takes at least two more PLHEADERs to recover the lock, as the
 * state machine needs to go over the "found" and "locked" states again.
 */
class DVBS2RX_API frame_sync : public pl_submodule
{
private:
    /* Parameters */
    uint8_t d_unlock_thresh; /**< Number of frame detection failures before unlocking */

    /* State */
    uint32_t d_sym_cnt;         /**< Symbol count since the last SOF */
    gr_complex d_last_in;       /**< Last input complex symbol */
    float d_timing_metric;      /**< Most recent timing metric */
    uint32_t d_sof_interval;    /**< Interval between the last two SOFs */
    frame_sync_state_t d_state; /**< Frame timing recovery state */
    uint32_t d_frame_len;       /**< Current PLFRAME length */
    uint8_t d_unlock_cnt;       /**< Count of consecutive frame detection failures */
    std::chrono::system_clock::time_point d_lock_time; /**< Frame lock timestamp */

    delay_line<gr_complex> d_plsc_delay_buf; /**< Buffer used as delay line */
    delay_line<gr_complex> d_sof_buf;        /**< SOF correlator buffer */
    delay_line<gr_complex> d_plsc_e_buf;     /**< Even PLSC correlator buffer  */
    delay_line<gr_complex> d_plsc_o_buf;     /**< Odd PLSC correlator buffer */
    cdeque<gr_complex> d_plheader_buf;       /**< Buffer to store the PLHEADER symbols */
    volk::vector<gr_complex> d_payload_buf;  /**< Buffer to store the PLFRAME payload */
    volk::vector<gr_complex> d_sof_taps;     /**< SOF cross-correlation taps */
    volk::vector<gr_complex> d_plsc_taps;    /**< PLSC cross-correlation taps */

    /* Timing metric threshold for inferring a start of frame.
     *
     * When unlocked, use a conservative threshold, as it is important
     * to avoid false positive SOF detection. In contrast, when locked,
     * we only want to periodically check whether the correlation is
     * sufficiently strong where it is expected to be (at the start of
     * the next frame). Since it is very important not to unlock
     * unnecessarily, use a lower threshold for this task. */
    const float threshold_u = 30; /** unlocked threshold */
                                  /* TODO: make this a top-level parameter */
    const float threshold_l = 25; /** locked threshold */

    /**
     * \brief Cross-correlation between a delay line buffer and a given vector.
     * \param d_line Reference to a delay line buffer with the newest sample at
     *               index 0 and the oldest sample at the last index.
     * \param taps Reference to a volk vector with taps for cross-correlation.
     * \param res Pointer to result.
     * \note The tap vector should consist of the folded version of the target
     * sequence (SOF or PLSC scrambler differentials).
     */
    void correlate(delay_line<gr_complex>& d_line,
                   volk::vector<gr_complex>& taps,
                   gr_complex* res);

public:
    /**
     * @brief Construct a new frame sync object
     *
     * @param debug_level (int) Target debugging log level (0 disables logs).
     * @param unlock_thresh (uint8_t) Number of consecutive frame detection
     * failures before unlocking. A failure occurs when the timing metric does
     * not exceed the expected magnitude threshold. By default, 3 failures will
     * lead to unlocking.
     *
     * @note The number of consecutive timing metric failures before unlocking
     * must be tuned to avoid unlocking prematurely under high noise, when the
     * timing metric deviates significantly from the nominal peak of 57 for
     * unit-energy symbols (57 due to the 26+32=57 correlator taps). On the
     * other hand, this threshold parameter should not be very high to avoid too
     * much delay in unlocking. For example, if a PLSC decoding error occurs and
     * a wrong PLFRAME length is informed to the frame synchronizer, the timing
     * metric observed after the wrong frame length will most certainly fail to
     * exceed the threshold. In this scenario, all subsequent `unlock_thresh`
     * frames will likely fail, as the frame synchronizer will search for their
     * PLHEADERs in wrong indexes. Hence, in this example, it is better to
     * unlock reasonably fast than to wait further.
     */
    frame_sync(int debug_level, uint8_t unlock_thresh = 3);

    /**
     * \brief Process the next input symbol.
     * \param in (gr_complex &) Input symbol.
     * \return (bool) Whether the input symbol consists of the last PLHEADER
     * symbol, where the timing metric is expected to peak.
     * \note This function should return true for the last PLHEADER symbol only.
     * For all other symbols, it should return false.
     */
    bool step(const gr_complex& in);

    /**
     * \brief Set the current PLFRAME length.
     *
     * This information is used to predict when the next SOF should be
     * observed. If a timing metric peak is indeed observed at the next expected
     * SOF index, the synchronizer achieves frame lock.
     *
     * \param len (uint32_t) Current PLFRAME length.
     */
    void set_frame_len(uint32_t len);

    /**
     * \brief Check whether frame lock has been achieved
     * \return (bool) True if locked.
     */
    bool is_locked() const { return d_state == frame_sync_state_t::locked; }

    /**
     * \brief Check whether frame lock has been achieved or a SOF has been found.
     * \return (bool) True if locked or if at least a SOF has been found.
     */
    bool is_locked_or_almost() const { return d_state != frame_sync_state_t::searching; }

    /**
     * @brief Get the symbol count on the internal payload buffer
     *
     * @return uint32_t Current number of payload symbols buffered internally if locked.
     */
    uint32_t get_sym_count() const
    {
        return std::min(d_sym_cnt, (uint32_t)MAX_PLFRAME_PAYLOAD);
    }

    /**
     * \brief Get the interval between the last two detected SOFs.
     * \return (uint32_t) Interval in symbol periods.
     */
    uint32_t get_sof_interval() const { return d_sof_interval; }

    /**
     * \brief Get the PLHEADER buffered internally.
     * \return (const gr_complex*) Pointer to the internal PLHEADER buffer.
     */
    const gr_complex* get_plheader() const { return &d_plheader_buf.back(); }

    /**
     * \brief Get the PLFRAME payload (data + pilots) buffered internally.
     *
     * The payload observed between consecutive SOFs is buffered internally. If
     * a SOF is missed such that the last two observed SOFs are spaced by more
     * than the maximum payload length, only up to MAX_PLFRAME_PAYLOAD symbols
     * are buffered internally.
     *
     * \return (const gr_complex*) Pointer to the internal payload buffer.
     */
    const gr_complex* get_payload() const { return d_payload_buf.data(); }

    /**
     * \brief Get the SOF correlator taps.
     * \return (const gr_complex*) Pointer to the SOF correlator taps.
     */
    const gr_complex* get_sof_corr_taps() const { return d_sof_taps.data(); }

    /**
     * \brief Get the PLSC correlator taps.
     * \return (const gr_complex*) Pointer to the PLSC correlator taps.
     */
    const gr_complex* get_plsc_corr_taps() const { return d_plsc_taps.data(); }

    /**
     * \brief Get the last evaluated timing metric.
     *
     * Once locked, the timing metric updates only once per frame. Before that,
     * it updates after every input symbol.
     *
     * \return (float) Last evaluated timing metric.
     */
    float get_timing_metric() const { return d_timing_metric; }

    /**
     * @brief Get the frame lock timestamp
     *
     * @return std::chrono::system_clock::time_point Timestamp in UTC time corresponding
     * to when the frame synchronizer locked the frame timing. Valid only when locked.
     */
    std::chrono::system_clock::time_point get_lock_time() { return d_lock_time; }
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_PL_FRAME_SYNC_H */
