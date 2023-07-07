/* -*- c++ -*- */
/*
 * Copyright (c) 2021 Igor Freire.
 *
 * This file is part of gr-dvbs2rx.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pi2_bpsk.h"
#include "pl_frame_sync.h"
#include "pl_signaling.h"
#include "qa_util.h"
#include "util.h"
#include <gnuradio/expj.h>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

namespace bdata = boost::unit_test::data;
namespace tt = boost::test_tools;

namespace gr {
namespace dvbs2rx {

/* Fixture */
struct F {
    F() : plheader(PLHEADER_LEN)
    {
        // Frame synchronizer object
        int debug_level = 0;
        uint8_t unlock_thresh = 1;
        p_frame_sync = new frame_sync(debug_level, unlock_thresh);

        // Noiseless PLHEADER for testing
        plsc_encoder plsc_mapper;
        gr_complex* ptr = plheader.data();
        map_bpsk(sof_big_endian, ptr, SOF_LEN);
        uint8_t modcod = 21;
        bool short_fecframe = true;
        bool has_pilots = false;
        pls_info.parse(modcod, short_fecframe, has_pilots);
        plsc_mapper.encode(ptr + SOF_LEN, modcod, short_fecframe, has_pilots);
    }
    ~F() { delete p_frame_sync; }

    pls_info_t pls_info;
    frame_sync* p_frame_sync;
    volk::vector<gr_complex> plheader;
};

BOOST_FIXTURE_TEST_CASE(test_sof_correlator_taps, F)
{
    // Pi/2 BPSK mapped SOF symbols
    volk::vector<gr_complex> mod_sof(SOF_LEN);
    map_bpsk(sof_big_endian, mod_sof.data(), SOF_LEN);

    // Expected SOF correlator taps:
    volk::vector<gr_complex> expected_taps(SOF_LEN - 1);
    for (int i = 0; i < SOF_LEN - 1; i++) {
        // The expected differential is "conj(mod_sof[i + 1]) * mod_sof[i]". The
        // cross-correlator taps should have the conjugate of that:
        gr_complex conj_diff = mod_sof[i + 1] * conj(mod_sof[i]);
        // Round the real and imaginary parts to avoid rounding errors:
        expected_taps[i] = { round(real(conj_diff)), round(imag(conj_diff)) };
    }
    // Fold the sequence
    std::reverse(expected_taps.begin(), expected_taps.end());

    // Actual SOF correlator taps
    const gr_complex* p_sof_taps = p_frame_sync->get_sof_corr_taps();

    // Check
    for (int i = 0; i < SOF_LEN - 1; i++)
        BOOST_CHECK_EQUAL(p_sof_taps[i], expected_taps[i]);
}

BOOST_FIXTURE_TEST_CASE(test_plsc_correlator_taps, F)
{
    // Map the PLSC scrambler sequence using pi/2 BPSK
    volk::vector<gr_complex> mod_plsc_scrambler(PLSC_LEN);
    map_bpsk(plsc_scrambler, mod_plsc_scrambler.data(), PLSC_LEN);

    // Expected PLSC correlator taps: while all the 25 differentials are known a
    // priori for the SOF, only the 32 consecutive pairs of differentials are
    // known for the PLSC.
    volk::vector<gr_complex> expected_taps(PLSC_LEN / 2);
    for (int i = 0; i < (PLSC_LEN / 2); i++) {
        // Conjugate of the expected pairwise differential:
        gr_complex conj_diff =
            mod_plsc_scrambler[(2 * i) + 1] * conj(mod_plsc_scrambler[2 * i]);
        // Round the real and imaginary parts to avoid rounding errors:
        expected_taps[i] = { round(real(conj_diff)), round(imag(conj_diff)) };
    }
    // Fold the sequence
    std::reverse(expected_taps.begin(), expected_taps.end());

    // Actual PLSC correlator taps
    const gr_complex* p_plsc_taps = p_frame_sync->get_plsc_corr_taps();

    // Check
    for (int i = 0; i < (PLSC_LEN / 2); i++)
        BOOST_CHECK_EQUAL(p_plsc_taps[i], expected_taps[i]);
}

BOOST_DATA_TEST_CASE_F(F, test_sof_detection, bdata::xrange(n_plsc_codewords), plsc)
{
    // Regenerate the PLHEADER for the chosen PLSC
    plsc_encoder plsc_mapper;
    pls_info.parse(plsc);
    plsc_mapper.encode(plheader.data() + SOF_LEN, plsc);

    // Process the first 89 symbols. The frame synchronizer should not be able
    // to find the SOF at this point.
    bool is_peak;
    for (int i = 0; i < PLHEADER_LEN - 1; i++) {
        is_peak = p_frame_sync->step(plheader[i]);
        BOOST_CHECK_EQUAL(is_peak, false);
        BOOST_CHECK_EQUAL(p_frame_sync->is_locked_or_almost(), false);
    }

    // Process the last PLHEADER symbol. At this point, the frame synchronizer
    // should find the cross-correlation peak and infer the SOF.
    is_peak = p_frame_sync->step(plheader[PLHEADER_LEN - 1]);
    BOOST_CHECK_EQUAL(is_peak, true);
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked_or_almost(), true);

    // Since the pi/2 BPSK symbols have unitary energy, the cross-correlation
    // peak at each correlator should be equal to the number of taps.
    BOOST_CHECK_CLOSE(
        p_frame_sync->get_timing_metric(), (SOF_CORR_LEN + PLSC_CORR_LEN), 1e-5);

    // The PLHEADER should be buffered internally and accessible when the SOF is
    // detected
    const gr_complex* buf_plheader = p_frame_sync->get_plheader();
    for (int i = 0; i < PLHEADER_LEN; i++)
        BOOST_CHECK_EQUAL(buf_plheader[i], plheader[i]);

    // Process one more symbol (e.g., the first payload symbol). The
    // almost-locked status should remain, but the symbol should not lead to a
    // cross-correlation peak.
    is_peak = p_frame_sync->step({ 0, 1.0 });
    BOOST_CHECK_EQUAL(is_peak, false);
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked_or_almost(), true);

    // The internally buffered PLHEADER is only accessible when the last
    // PLHEADER symbol is processed. As soon as the first payload symbol is
    // processed, the PLHEADER buffer is not guaranteed to be valid.
    buf_plheader = p_frame_sync->get_plheader();
    for (int i = 0; i < PLHEADER_LEN; i++)
        BOOST_TEST(buf_plheader[i] != plheader[i]);
}

BOOST_DATA_TEST_CASE_F(F,
                       test_sof_detection_under_freq_offset,
                       bdata::xrange(n_plsc_codewords) *
                           bdata::make({ -0.25, -0.13, 0.03, 0.19, 0.25 }),
                       plsc,
                       freq_offset)
{
    // Regenerate the PLHEADER for the chosen PLSC
    plsc_encoder plsc_mapper;
    pls_info.parse(plsc);
    plsc_mapper.encode(plheader.data() + SOF_LEN, plsc);

    // Add frequency and phase offsets to the test PLHEADER
    volk::vector<gr_complex> rotated(PLHEADER_LEN);
    float esn0_db = 1e2; // ignored unless channel.add_noise is called
    NoisyChannel channel(esn0_db, freq_offset);
    channel.set_random_phase();
    channel.rotate(rotated.data(), plheader.data(), PLHEADER_LEN);

    // Process the first 89 symbols. The frame synchronizer should not be able
    // to find the SOF at this point.
    bool is_peak;
    for (int i = 0; i < PLHEADER_LEN - 1; i++) {
        is_peak = p_frame_sync->step(rotated[i]);
        BOOST_CHECK_EQUAL(is_peak, false);
    }

    // Process the last PLHEADER symbol. At this point, the frame synchronizer
    // should find the cross-correlation peak and infer the SOF.
    is_peak = p_frame_sync->step(rotated[PLHEADER_LEN - 1]);
    BOOST_CHECK_EQUAL(is_peak, true);

    // Process one more symbol (e.g., the first payload symbol). It should not
    // lead to a cross-correlation peak.
    is_peak = p_frame_sync->step({ 0, 1.0 });
    BOOST_CHECK_EQUAL(is_peak, false);
}

BOOST_FIXTURE_TEST_CASE(test_locking_unlocking, F)
{
    // Test a payload populated with a rotating sequence on the unit circle with
    // phase from 0 to 2*pi (exclusive). The actual rotation is unimportant. The
    // only goal is to make all payload symbols unique such that the payload
    // buffered by the frame synchronizer can be verified.
    volk::vector<gr_complex> payload_base(pls_info.payload_len, 1);
    volk::vector<gr_complex> payload(pls_info.payload_len);
    float esn0_db = 1e2; // ignored unless channel.add_noise is called
    float freq_offset = 1.0 / pls_info.payload_len;
    NoisyChannel channel(esn0_db, freq_offset);
    channel.rotate(payload.data(), payload_base.data(), pls_info.payload_len);

    // Process the first PLHEADER
    for (int i = 0; i < PLHEADER_LEN; i++)
        p_frame_sync->step(plheader[i]);

    // The frame synchronizer should have found one SOF. However, two SOFs are
    // required to lock, so it shouldn't be locked yet.
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked_or_almost(), true);
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked(), false);

    // At this point, the caller would decode the PLSC embedded on the PLHEADER
    // and obtain the frame length. Then, it would tell the frame synchronizer:
    p_frame_sync->set_frame_len(pls_info.plframe_len);

    // Process the payload
    for (int i = 0; i < pls_info.payload_len; i++) {
        bool is_peak = p_frame_sync->step(payload[i]);
        BOOST_CHECK_EQUAL(is_peak, false);
    }

    // The state should remain the same
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked_or_almost(), true);
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked(), false);

    // Finally, process the second PLHEADER coming exactly after the expected
    // PLFRAME length since the last SOF:
    for (int i = 0; i < PLHEADER_LEN; i++)
        p_frame_sync->step(plheader[i]);

    // Now it should be locked
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked_or_almost(), true);
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked(), true);

    // The full payload should be buffered internally
    const gr_complex* buf_payload = p_frame_sync->get_payload();
    for (int i = 0; i < pls_info.payload_len; i++)
        BOOST_CHECK_EQUAL(buf_payload[i], payload[i]);

    // Process another payload
    for (int i = 0; i < pls_info.payload_len; i++) {
        bool is_peak = p_frame_sync->step(payload[i]);
        BOOST_CHECK_EQUAL(is_peak, false);
    }

    // At this point, the frame synchronizer expects the third PLHEADER. If the
    // PLHEADER doesn't come, and if the unlock_thresh parameter is set to 1,
    // the frame synchronizer should unlock.
    volk::vector<gr_complex> non_plheader(PLHEADER_LEN, { 0, 1.0 });
    for (int i = 0; i < PLHEADER_LEN; i++)
        p_frame_sync->step(non_plheader[i]);

    BOOST_CHECK_EQUAL(p_frame_sync->is_locked_or_almost(), false);
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked(), false);
}

BOOST_FIXTURE_TEST_CASE(test_consecutive_sofs_after_wrong_frame_len, F)
{
    // Test an all-ones payload
    volk::vector<gr_complex> payload(pls_info.payload_len, 1);

    // Process the first PLHEADER
    for (int i = 0; i < PLHEADER_LEN; i++)
        p_frame_sync->step(plheader[i]);

    // The frame synchronizer should have found one SOF. However, two SOFs are
    // required to lock, so it shouldn't be locked yet. It should be on "found"
    // state at this point.
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked_or_almost(), true);
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked(), false);

    // Pretend the caller fails to decode the PLSC correctly and informs the
    // wrong frame length to the frame synchronizer.
    p_frame_sync->set_frame_len(100);

    // Process the payload
    for (int i = 0; i < pls_info.payload_len; i++) {
        bool is_peak = p_frame_sync->step(payload[i]);
        BOOST_CHECK_EQUAL(is_peak, false);
    }

    // The state should remain the same ("found").
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked_or_almost(), true);
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked(), false);

    // Finally, process a second PLHEADER:
    for (int i = 0; i < PLHEADER_LEN; i++)
        p_frame_sync->step(plheader[i]);

    // It shouldn't lock, as the PLHEADER comes at an unexpected index due to
    // the wrong frame length information. It should still be in "found" state,
    // given that another PLHEADER was just found.
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked_or_almost(), true);
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked(), false);
}

BOOST_FIXTURE_TEST_CASE(test_sof_after_wrong_frame_len_while_locked, F)
{
    // Test all-ones payloads
    volk::vector<gr_complex> payload(pls_info.payload_len, 1);

    // Get to locked state
    for (int i = 0; i < PLHEADER_LEN; i++) // 1st PLHEADER
        p_frame_sync->step(plheader[i]);
    p_frame_sync->set_frame_len(pls_info.plframe_len);

    for (int i = 0; i < pls_info.payload_len; i++) { // 1st Payload
        bool is_peak = p_frame_sync->step(payload[i]);
        BOOST_CHECK_EQUAL(is_peak, false);
    }

    for (int i = 0; i < PLHEADER_LEN; i++) // 2nd PLHEADER
        p_frame_sync->step(plheader[i]);

    // It should be locked at this point
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked_or_almost(), true);
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked(), true);

    // Now, pretend the caller has failed to decode the second PLSC correctly
    // and informed the wrong PLFRAME length for the second frame.
    p_frame_sync->set_frame_len(100);

    // Process the second payload
    for (int i = 0; i < pls_info.payload_len; i++) {
        bool is_peak = p_frame_sync->step(payload[i]);
        BOOST_CHECK_EQUAL(is_peak, false);
    }

    // While processing the second payload, the frame synchronizer should find
    // that the timing metric does not peak after the informed frame length. At
    // this point, it should transition back to the "searching" state, given
    // that the `unlock_thresh` parameter is set to 1.
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked_or_almost(), false);
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked(), false);
}

void add_noise_to_plheader(volk::vector<gr_complex>& noisy_plheader, float esn0_db)
{
    const float freq_offset = 0;
    NoisyChannel channel(esn0_db, freq_offset);
    channel.add_noise(noisy_plheader.data(), PLHEADER_LEN);

    // Scale the noisy symbols for unit energy, just like an AGC would
    gr_complex sq_norm;
    volk_32fc_x2_conjugate_dot_prod_32fc(
        &sq_norm, noisy_plheader.data(), noisy_plheader.data(), PLHEADER_LEN);
    float rms = sqrt(abs(sq_norm) / PLHEADER_LEN);
    for (gr_complex& x : noisy_plheader) {
        x = x / rms;
    }
}

BOOST_FIXTURE_TEST_CASE(test_non_unit_unlock_count_threshold, F)
{
    // Recreate the frame synchronizer object with a non-unitary unlock threshold
    delete p_frame_sync;
    int debug_level = 0;
    uint8_t unlock_thresh = 2;
    p_frame_sync = new frame_sync(debug_level, unlock_thresh);

    // Test all-ones payloads
    volk::vector<gr_complex> payload(pls_info.payload_len, 1);

    // Get to locked state
    for (int i = 0; i < PLHEADER_LEN; i++) // 1st PLHEADER
        p_frame_sync->step(plheader[i]);
    p_frame_sync->set_frame_len(pls_info.plframe_len);

    for (int i = 0; i < pls_info.payload_len; i++) { // 1st Payload
        bool is_peak = p_frame_sync->step(payload[i]);
        BOOST_CHECK_EQUAL(is_peak, false);
    }

    for (int i = 0; i < PLHEADER_LEN; i++) // 2nd PLHEADER
        p_frame_sync->step(plheader[i]);

    // It should be locked at this point
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked_or_almost(), true);
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked(), true);

    for (int i = 0; i < pls_info.payload_len; i++) { // 2nd Payload
        bool is_peak = p_frame_sync->step(payload[i]);
        BOOST_CHECK_EQUAL(is_peak, false);
    }

    // When processing the third PLHEADER, add a significant amount of noise
    volk::vector<gr_complex> noisy_plheader1(plheader);
    float esn0_db = -10;
    add_noise_to_plheader(noisy_plheader1, esn0_db);

    for (int i = 0; i < PLHEADER_LEN; i++) // 3rd PLHEADER
        p_frame_sync->step(noisy_plheader1[i]);

    // The timing metric should fail at this point, but the frame synchronizer
    // should remain locked due to unlock_thresh=2.
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked_or_almost(), true);
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked(), true);

    for (int i = 0; i < pls_info.payload_len; i++) { // 3rd Payload
        bool is_peak = p_frame_sync->step(payload[i]);
        BOOST_CHECK_EQUAL(is_peak, false);
    }

    // Process one more very noisy PLHEADER
    volk::vector<gr_complex> noisy_plheader2(plheader);
    add_noise_to_plheader(noisy_plheader2, esn0_db);

    for (int i = 0; i < PLHEADER_LEN; i++) // 4th PLHEADER
        p_frame_sync->step(noisy_plheader2[i]);

    // Again, the timing metric should fail. However, since this is the second
    // consecutive failure, and unlock_thresh=2, the synchronizer should unlock.
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked_or_almost(), false);
    BOOST_CHECK_EQUAL(p_frame_sync->is_locked(), false);
}

} // namespace dvbs2rx
} // namespace gr
