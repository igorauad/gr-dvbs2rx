#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright (c) 2019-2021 Igor Freire
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

import random
import unittest
from math import sqrt, pi, floor

import pmt
import numpy as np
from gnuradio import gr, gr_unittest
from gnuradio import blocks, digital, analog
try:
    from dvbs2rx import plsync_cc
except ImportError:
    import os
    import sys
    dirname, filename = os.path.split(os.path.abspath(__file__))
    sys.path.append(os.path.join(dirname, "bindings"))
    from dvbs2rx import plsync_cc

SQRT2_2 = sqrt(2) / 2
JSQRT2_2 = SQRT2_2 * 1j
EXPJ45 = (+SQRT2_2 + JSQRT2_2)
EXPJ135 = (-SQRT2_2 + JSQRT2_2)
EXPJ225 = (-SQRT2_2 - JSQRT2_2)
EXPJ315 = (+SQRT2_2 - JSQRT2_2)


def generate_pilot_blk(scrambler_seq):
    """Generate the 36-symbol pilot block for a given scrambler sequence"""
    assert (len(scrambler_seq) == 36)
    # Start with the unscrambled pilots
    p = EXPJ45 * np.ones(36)
    # Scramble using the supplied scrambler sequence
    for n in range(len(p)):
        Rn = scrambler_seq[n]
        if (Rn == 1):
            p[n] = complex(-p[n].imag, p[n].real)
        elif (Rn == 2):
            p[n] = -p[n]
        elif (Rn == 3):
            p[n] = complex(p[n].imag, -p[n].real)
    return tuple(p)


class qa_plsync_cc(gr_unittest.TestCase):
    def setUp(self):
        self.tb = gr.top_block()

        # Parameters
        self.gold_code = 0
        self.debug_level = 0
        self.sps = 1.0  # oversampling
        self.freq_est_period = 1
        # NOTE: the oversampling parameter is only used for controlling an
        # external rotator, which is not something that is within the unit
        # tests. Hence, the actual sps value here doesn't matter.

        # Set the test PLHEADER
        self._set_test_plframe()

    def tearDown(self):
        self.tb = None

    def _set_test_plframe(self, pilots=False):
        """Define the PLFRAME to be used for testing

        The test PLHEADER is the sequence of 90 symbols obtained by mapping the
        scrambled PLSC using a pi/2 BPSK mapper.

        """
        if (pilots):
            # This PLHEADER corresponds to:
            # {MODCOD: 21, Short FECFRAME: 1, Pilots: 1}
            # {n_mod: 4, S: 45, PLFRAME length: 4212}
            slots = 45
            n_pilot_blks = floor(slots / 16)
            modcod = 21
            short_frame = True
            plheader = (EXPJ45, EXPJ315, EXPJ225, EXPJ135, EXPJ45, EXPJ135,
                        EXPJ225, EXPJ315, EXPJ45, EXPJ315, EXPJ45, EXPJ135,
                        EXPJ225, EXPJ135, EXPJ225, EXPJ315, EXPJ225, EXPJ135,
                        EXPJ225, EXPJ135, EXPJ45, EXPJ135, EXPJ45, EXPJ135,
                        EXPJ225, EXPJ135, EXPJ225, EXPJ315, EXPJ225, EXPJ135,
                        EXPJ225, EXPJ135, EXPJ45, EXPJ135, EXPJ225, EXPJ315,
                        EXPJ225, EXPJ315, EXPJ225, EXPJ135, EXPJ225, EXPJ315,
                        EXPJ45, EXPJ135, EXPJ45, EXPJ315, EXPJ225, EXPJ135,
                        EXPJ225, EXPJ135, EXPJ225, EXPJ135, EXPJ225, EXPJ135,
                        EXPJ225, EXPJ315, EXPJ225, EXPJ315, EXPJ45, EXPJ135,
                        EXPJ225, EXPJ315, EXPJ45, EXPJ315, EXPJ45, EXPJ315,
                        EXPJ225, EXPJ315, EXPJ45, EXPJ315, EXPJ225, EXPJ135,
                        EXPJ225, EXPJ315, EXPJ45, EXPJ315, EXPJ45, EXPJ135,
                        EXPJ225, EXPJ135, EXPJ225, EXPJ315, EXPJ45, EXPJ315,
                        EXPJ225, EXPJ135, EXPJ45, EXPJ135, EXPJ225, EXPJ315)
            frame_len = ((slots + 1) * 90) + (n_pilot_blks * 36)
            pilot_blk_1 = generate_pilot_blk([
                3, 2, 2, 2, 1, 1, 2, 3, 2, 0, 1, 3, 1, 2, 0, 0, 3, 0, 0, 0, 1,
                2, 2, 3, 2, 2, 3, 3, 1, 2, 3, 2, 3, 1, 1, 0
            ])
            pilot_blk_2 = generate_pilot_blk([
                0, 3, 1, 3, 3, 3, 1, 2, 2, 1, 1, 2, 2, 2, 3, 1, 2, 1, 3, 3, 2,
                1, 3, 0, 3, 1, 3, 1, 0, 0, 2, 0, 3, 2, 2, 2
            ])
            plframe = plheader + tuple(np.zeros(16 * 90)) + pilot_blk_1 + \
                tuple(np.zeros(16 * 90)) + pilot_blk_2 + \
                tuple(np.zeros(13 * 90))

        else:
            # This PLHEADER corresponds to:
            # {MODCOD: 21, Short FECFRAME: 1, Pilots: 0}
            # {n_mod: 4, S: 45, PLFRAME length: 4140}
            slots = 45
            n_pilot_blks = 0
            modcod = 21
            short_frame = True
            plheader = (EXPJ45, EXPJ315, EXPJ225, EXPJ135, EXPJ45, EXPJ135,
                        EXPJ225, EXPJ315, EXPJ45, EXPJ315, EXPJ45, EXPJ135,
                        EXPJ225, EXPJ135, EXPJ225, EXPJ315, EXPJ225, EXPJ135,
                        EXPJ225, EXPJ135, EXPJ45, EXPJ135, EXPJ45, EXPJ135,
                        EXPJ225, EXPJ135, EXPJ225, EXPJ135, EXPJ225, EXPJ315,
                        EXPJ225, EXPJ315, EXPJ45, EXPJ315, EXPJ225, EXPJ135,
                        EXPJ225, EXPJ135, EXPJ225, EXPJ315, EXPJ225, EXPJ135,
                        EXPJ45, EXPJ315, EXPJ45, EXPJ135, EXPJ225, EXPJ315,
                        EXPJ225, EXPJ315, EXPJ225, EXPJ315, EXPJ225, EXPJ315,
                        EXPJ225, EXPJ135, EXPJ225, EXPJ135, EXPJ45, EXPJ315,
                        EXPJ225, EXPJ135, EXPJ45, EXPJ135, EXPJ45, EXPJ135,
                        EXPJ225, EXPJ135, EXPJ45, EXPJ135, EXPJ225, EXPJ315,
                        EXPJ225, EXPJ135, EXPJ45, EXPJ135, EXPJ45, EXPJ315,
                        EXPJ225, EXPJ315, EXPJ225, EXPJ135, EXPJ45, EXPJ135,
                        EXPJ225, EXPJ315, EXPJ45, EXPJ315, EXPJ225, EXPJ135)
            frame_len = ((slots + 1) * 90)
            payload = tuple(np.zeros(frame_len - 90))
            plframe = plheader + payload

        xfecframe_len = slots * 90

        # Save some attributes
        self.mod_plheader = plheader
        self.frame_len = frame_len
        self.xfecframe_len = xfecframe_len
        self.modcod = modcod
        self.short_frame = short_frame
        self.plframe = plframe

    def _noise_std(self, snr_db):
        """Compute the complex AWGN standard deviation for a target SNR

        This standard deviation is equivalent to sqrt(N0), in the usual N0
        notation denoting the complex noise variance.

        """
        noise_var = 1.0 / (10**(float(snr_db) / 10))
        noise_std = sqrt(noise_var)
        return noise_std

    def _rnd_const_mapper_input(self, n_symbols, bits_per_sym):
        """Generate random symbols (int numbers) to a constellation mapper input

        Args:
            n_symbols    : Desired number of symbols in the output
            bits_per_sym : Number of bits carried by each const symbol

        Returns:
            Random input symbols

        """
        rndm = random.Random()
        n_bits = int(bits_per_sym * n_symbols)
        in_vec = tuple([rndm.randint(0, 1) for i in range(0, n_bits)])
        return in_vec

    def _gen_plframes(self, n):
        """Generate n blank PLFRAMEs

        PLHEADER is legit, but data symbols are simply zero-valued

        Args:
            n : Number of PLFRAMEs to generate

        """
        assert (n > 0)

        # Repeat the test PLFRAME n times
        seq = self.plframe
        i_rep = 0
        while (i_rep < n - 1):
            seq += self.plframe
            i_rep += 1

        # Add an extra PLHEADER in the end due to how PLFRAMEs are found
        # between two consecutive SOFs. Ultimately, the sequence will have n
        # full PLFRAMES and n+1 PLHEADERS.
        seq += self.mod_plheader

        return seq

    def _assert_tags(self,
                     tags,
                     key,
                     n_expected,
                     val_expected,
                     offset_expected,
                     val_equality=True,
                     places=1):
        """Assert that tag(s) are located and valued as expected

        Args:
            tags            : vector of tags obtained with a Tag Debug block
            key             : key of the tag of interest
            n_expected      : number of tags that were expected
            val_expected    : values expected for each tag
            offset_expected : symbol index where we expect to see the tag
            val_equality    : whether the expected value should be checked for
                              equality. Otherwise, expect the tag value to be
                              greater than the corresponding `val_expected`.
            places          : decimal places when checking equality

        """
        assert (type(val_expected) is list)
        assert (type(offset_expected) is list)
        assert(n_expected == len(val_expected)), \
            "Please provide all {} expected values".format(n_expected)
        assert(n_expected == len(offset_expected)), \
            "Please provide all {} expected offsets".format(n_expected)

        # Filter tags by key
        filt_tags = list()
        for tag in tags:
            if (pmt.eq(tag.key, pmt.intern(key))):
                filt_tags.append(tag)

        # Confirm that the number of tags found matches expectation
        self.assertEqual(len(filt_tags), n_expected)

        for i, tag in enumerate(filt_tags):
            if (pmt.is_pair(tag.value)):
                tag_val = (pmt.to_long(pmt.car(tag.value)),
                           pmt.to_bool(pmt.cdr(tag.value)))
            else:
                # Check the value of the tag
                tag_val = pmt.to_double(tag.value)

            if (type(val_expected[i]) is tuple):
                for j, val in enumerate(val_expected[i]):
                    if (val_equality):
                        self.assertAlmostEqual(tag_val[j], val, places=places)
                    else:
                        self.assertTrue(tag_val[j] > val)
            else:
                if (val_equality):
                    self.assertAlmostEqual(tag_val,
                                           val_expected[i],
                                           places=places)
                else:
                    self.assertTrue(tag_val > val_expected[i])

            # Check the location (symbol index/offset) of the tag
            self.assertEqual(tag.offset, offset_expected[i])

    # @unittest.skip
    def test_detect_ideal_plheader(self):
        """Test detection of ideal/noiseless modulated PLHEADER

        PLHEADER is the ideal pi/2 BPSK-modulated header with PLSC scrambled,
        in the absence of noise and any other impairment.

        """

        mod_syms = self._gen_plframes(2)  # at least two PLFRAMEs for locking

        # Flowgraph
        src = blocks.vector_source_c(mod_syms)
        plsync = plsync_cc(self.gold_code, self.freq_est_period, self.sps,
                           self.debug_level)
        snk = blocks.tag_debug(gr.sizeof_gr_complex, "XFECFRAME")
        snk.set_save_all(True)
        self.tb.connect(src, plsync, snk)
        self.tb.run()

        # The block shall add a tag on the first data symbol after each
        # PLHEADER, i.e., at the start of each output XFECFRAME. Each tag must
        # contain the correct PLSC info.
        tags = snk.current_tags()
        self._assert_tags(tags, "XFECFRAME", 2,
                          [(self.modcod, self.short_frame),
                           (self.modcod, self.short_frame)],
                          [0, self.xfecframe_len])

    # @unittest.skip
    def test_detect_noisy_plheader_open_loop(self):
        """Test detection of a noisy modulated PLHEADER in open loop

        Test detection of a PLHEADER with phase rotation due to freq. offset,
        AWGN, and a time offset. Test the open-loop approach, where the PL Sync
        block does not rely on an external rotator for frequency correction.

        """

        # Parameters
        snr_db = 5.0  # SNR in dB
        fe = 0.2  # normalized frequency offset

        # Constants
        phase_inc = 2 * pi * fe
        noise_std = self._noise_std(snr_db)

        # Random time offset: prepend zeros to the symbol sequence
        mod_syms = tuple(np.zeros(np.random.randint(1000)))
        mod_syms += self._gen_plframes(2)  # at least 2 PLFRAMEs for locking

        # Flowgraph
        src = blocks.vector_source_c(mod_syms)
        rot = blocks.rotator_cc(phase_inc)
        noise = analog.noise_source_c(analog.GR_GAUSSIAN, noise_std, 0)
        nadder = blocks.add_cc()
        plsync = plsync_cc(self.gold_code, self.freq_est_period, self.sps,
                           self.debug_level)
        snk = blocks.tag_debug(gr.sizeof_gr_complex, "XFECFRAME")
        snk.set_save_all(True)
        self.tb.connect(src, rot)
        self.tb.connect(rot, (nadder, 0))
        self.tb.connect(noise, (nadder, 1))
        self.tb.connect(nadder, plsync, snk)
        self.tb.run()

        # Check the tag at the start of each output XFECFRAME. The zeros (i.e.,
        # time offset) preceding the first PLFRAME should not change anything.
        tags = snk.current_tags()
        self._assert_tags(tags, "XFECFRAME", 2,
                          [(self.modcod, self.short_frame),
                           (self.modcod, self.short_frame)],
                          [0, self.xfecframe_len])

    # @unittest.skip
    def test_detect_noisy_plheader_closed_loop(self):
        """Test detection of a noisy modulated PLHEADER

        Test detection of a PLHEADER with phase rotation due to freq. offset,
        AWGN, and a time offset. Test the closed-loop approach, where the PL
        Sync block relies on an external rotator for frequency correction.

        """

        # Parameters
        snr_db = 5.0  # SNR in dB
        fe = 0.2  # normalized frequency offset

        # Constants
        phase_inc = 2 * pi * fe
        noise_std = self._noise_std(snr_db)

        # Overwrite the test PLHEADER - add pilot symbols to test the
        # pilot-based fine frequency offset estimation
        self._set_test_plframe(pilots=True)

        # Random time offset: prepend zeros to the symbol sequence
        mod_syms = tuple(np.zeros(np.random.randint(1000)))
        mod_syms += self._gen_plframes(2)  # at least 2 PLFRAMEs for locking

        # Flowgraph including the freq. offset correction derotator
        src = blocks.vector_source_c(mod_syms)
        channel_rot = blocks.rotator_cc(phase_inc)
        freq_corr_rot = blocks.rotator_cc(0, True)
        freq_corr_rot.set_max_output_buffer(1024)
        # Restrict the rotator output buffer to reduce the chances of missing
        # the frequency corrections published via message port.
        noise = analog.noise_source_c(analog.GR_GAUSSIAN, noise_std, 0)
        nadder = blocks.add_cc()
        plsync = plsync_cc(self.gold_code, self.freq_est_period, self.sps,
                           self.debug_level)
        snk = blocks.tag_debug(gr.sizeof_gr_complex, "XFECFRAME")
        snk.set_save_all(True)
        self.tb.msg_connect((plsync, 'rotator_phase_inc'),
                            (freq_corr_rot, 'cmd'))
        self.tb.connect(src, channel_rot)
        self.tb.connect(channel_rot, (nadder, 0))
        self.tb.connect(noise, (nadder, 1))
        self.tb.connect(nadder, freq_corr_rot, plsync, snk)
        self.tb.run()

        # Check the tag at the start of each output XFECFRAME. The zeros (i.e.,
        # time offset) preceding the first PLFRAME should not change anything.
        tags = snk.current_tags()
        self._assert_tags(tags, "XFECFRAME", 2,
                          [(self.modcod, self.short_frame),
                           (self.modcod, self.short_frame)],
                          [0, self.xfecframe_len])

    # @unittest.skip
    def test_non_plheader_qpsk(self):
        """Test random sequence of noisy QPSK symbols

        This sequence does not contain the PLHEADER and, hence, it should not
        lead to a false positive frame detection.

        """

        # Parameters
        snr_db = 10.0
        frame_len = 10000

        # Constants
        bits_per_sym = 2
        noise_std = self._noise_std(snr_db)
        qpsk_const = (EXPJ225, EXPJ315, EXPJ135, EXPJ45)

        # Random input symbols
        in_vec = self._rnd_const_mapper_input(frame_len, bits_per_sym)

        # Flowgraph
        src = blocks.vector_source_b(in_vec)
        pack = blocks.repack_bits_bb(1, bits_per_sym, "", False,
                                     gr.GR_MSB_FIRST)
        cmap = digital.chunks_to_symbols_bc(qpsk_const)
        noise = analog.noise_source_c(analog.GR_GAUSSIAN, noise_std, 0)
        nadder = blocks.add_cc()
        plsync = plsync_cc(self.gold_code, self.freq_est_period, self.sps,
                           self.debug_level)
        snk = blocks.tag_debug(gr.sizeof_gr_complex, "SOF")

        # Connect source into flowgraph
        self.tb.connect(src, pack, cmap)
        self.tb.connect(cmap, (nadder, 0))
        self.tb.connect(noise, (nadder, 1))
        self.tb.connect(nadder, plsync, snk)
        self.tb.run()

        # Check if there was any false positive
        tags = snk.current_tags()
        self.assertTrue(len(tags) == 0)

    # @unittest.skip
    def test_freq_est_noiseless_rot_stream_open_loop(self):
        """Test frequency offset estimation over noiseless but rotated stream

        Use the open-loop approach, where the PL Sync works standalone without
        an external rotator block.

        Since the PLFRAME stream is noiseless and only rotated, use the default
        period=1 for the coarse frequency offset estimation.

        """

        # Parameters
        fe = np.random.uniform(-.25, .25)  # normalized frequency offset

        # Constants
        phase_inc = 2 * pi * fe
        frames = self._gen_plframes(1)  # one frame is enough to estimate

        # Flowgraph
        src = blocks.vector_source_c(frames)
        rot = blocks.rotator_cc(phase_inc)
        plsync = plsync_cc(self.gold_code, self.freq_est_period, self.sps,
                           self.debug_level)
        snk = blocks.null_sink(gr.sizeof_gr_complex)
        self.tb.connect(src, rot, plsync, snk)
        self.tb.run()

        # Expect an accurate estimate for a noiseless stream
        self.assertAlmostEqual(plsync.get_freq_offset(), fe)

    # @unittest.skip
    def test_freq_est_noisy_rot_stream_open_loop(self):
        """Test frequency offset estimation over noisy and rotated stream

        This time, introduce AWGN at various SNR levels. To overcome the noise,
        estimate the coarse frequency offset over a non-unitary and
        sufficiently large period to achieve better averaging.

        Also, continue to use the open-loop approach, where the PL Sync works
        standalone without an external rotator block. Note that, in the
        open-loop configuration, the fine frequency offset estimation in most
        cases won't be active, as the coarse frequency offset estimate never
        falls into a lower range without the actual closed-loop corrections
        (unless the actual offset is already low). Hence, the results are not
        expected to be very accurate, especially under low SNR levels.

        """

        # Paramet3rs
        n_frames = 200
        freq_est_period = 100

        # Generate the PLFRAME stream and prepend a random time offset
        frames = tuple(np.zeros(np.random.randint(1000)))
        frames += self._gen_plframes(n_frames)

        # Iterate over SNR levels
        for snr_db in reversed(range(-1, 10)):
            print("Try SNR: %.1f dB" % (snr_db))
            # Generate a new uniformly-distributed freq. offset realization
            fe = np.random.uniform(-.25, .25)
            phase_inc = 2 * pi * fe

            # Update the noise amplitude for the target SNR
            noise_std = self._noise_std(snr_db)

            # Re-generate the flowgraph every time to clear the state and the
            # tags held within the Tag Debug block
            src = blocks.vector_source_c(frames)
            rot = blocks.rotator_cc(phase_inc)
            noise = analog.noise_source_c(analog.GR_GAUSSIAN, noise_std, 0)
            nadder = blocks.add_cc()
            plsync = plsync_cc(self.gold_code, freq_est_period, self.sps,
                               self.debug_level)
            snk = blocks.null_sink(gr.sizeof_gr_complex)
            self.tb.connect(src, rot)
            self.tb.connect(rot, (nadder, 0))
            self.tb.connect(noise, (nadder, 1))
            self.tb.connect(nadder, plsync, snk)
            self.tb.run()

            # Expect a rough estimate, especially due to the low SNR levels
            self.assertAlmostEqual(plsync.get_freq_offset(), fe, places=2)

    # @unittest.skip
    def test_freq_est_noiseless_rot_stream_closed_loop(self):
        """Test closed-loop freq. estimation over a noiseless but rotated stream

        Test PLFRAMEs disturbed by frequency and phase offset only, without any
        AWG noise. Test the closed-loop correction, where the PL Sync block
        coordinates an external rotator block. Assume the PLFRAMEs contain
        pilot symbols so that the pilot-mode fine offset estimation is used by
        the PL Sync block when the residual frequency offset (after
        corrections) falls within the fine estimation range.

        """

        # Parameters
        fe = np.random.uniform(-.25, .25)  # normalized frequency offset

        # Constants
        phase_inc = 2 * pi * fe

        # Overwrite the default pilotless test PLHEADER to one with pilots.
        self._set_test_plframe(pilots=True)

        # Generate the PLFRAMEs and prepend a random time offset
        mod_syms = tuple(np.zeros(np.random.randint(1000)))
        mod_syms += self._gen_plframes(2)
        # Two frames are enough to produce one fine estimate. The first frame
        # leads to a coarse frequency offset estimate and a corresponding
        # correction scheduled to the start of the succeeding frame. On the
        # second frame, the coarse corrected state is achieved. Hence, when
        # processing the second payload, the fine estimate is produced.

        # Flowgraph including the freq. offset correction derotator
        src = blocks.vector_source_c(mod_syms)
        channel_rot = blocks.rotator_cc(phase_inc)
        freq_corr_rot = blocks.rotator_cc(0, True)
        freq_corr_rot.set_max_output_buffer(1024)
        # Restrict the rotator output buffer to reduce the chances of missing
        # the frequency corrections published via message port.
        plsync = plsync_cc(self.gold_code, self.freq_est_period, self.sps,
                           self.debug_level)
        snk = blocks.null_sink(gr.sizeof_gr_complex)
        self.tb.msg_connect((plsync, 'rotator_phase_inc'),
                            (freq_corr_rot, 'cmd'))
        self.tb.connect(src, channel_rot, freq_corr_rot, plsync, snk)
        self.tb.run()

        # The PL Sync block returns the cumulative frequency offset, not the
        # residual frequency offset. Hence, the result should converge to fe.
        self.assertAlmostEqual(plsync.get_freq_offset(), fe)

    # @unittest.skip
    def test_freq_est_noisy_rot_stream_closed_loop(self):
        """Test closed-loop freq. estimation over a noisy but rotated stream

        Similar to test_freq_est_noisy_rot_stream_open_loop, but in closed
        loop, i.e., with an external rotator block for frequency correction.

        """

        # Paramet3rs
        n_frames = 200
        freq_est_period = 100

        # Overwrite the default pilotless test PLHEADER to one with pilots.
        self._set_test_plframe(pilots=True)

        # Generate the PLFRAME stream and prepend a random time offset
        frames = tuple(np.zeros(np.random.randint(1000)))
        frames += self._gen_plframes(n_frames)

        # Iterate over SNR levels
        for snr_db in [-1]:  #reversed(range(-1, 10)):
            print("Try SNR: %.1f dB" % (snr_db))
            # Generate a new uniformly-distributed freq. offset realization
            fe = np.random.uniform(-.25, .25)
            phase_inc = 2 * pi * fe

            # Update the noise amplitude for the target SNR
            noise_std = self._noise_std(snr_db)

            # Re-generate the flowgraph every time to clear the state and the
            # tags held within the Tag Debug block
            src = blocks.vector_source_c(frames)
            channel_rot = blocks.rotator_cc(phase_inc)
            freq_corr_rot = blocks.rotator_cc(0, True)
            freq_corr_rot.set_max_output_buffer(1024)
            # Restrict the rotator output buffer to reduce the chances of missing
            # the frequency corrections published via message port.
            noise = analog.noise_source_c(analog.GR_GAUSSIAN, noise_std, 0)
            nadder = blocks.add_cc()
            plsync = plsync_cc(self.gold_code, freq_est_period, self.sps,
                               self.debug_level)
            snk = blocks.null_sink(gr.sizeof_gr_complex)
            self.tb.msg_connect((plsync, 'rotator_phase_inc'),
                                (freq_corr_rot, 'cmd'))
            self.tb.connect(src, channel_rot)
            self.tb.connect(channel_rot, (nadder, 0))
            self.tb.connect(noise, (nadder, 1))
            self.tb.connect(nadder, freq_corr_rot, plsync, snk)
            self.tb.run()

            # Expect a rough estimate, especially due to the low SNR levels
            self.assertAlmostEqual(plsync.get_freq_offset(), fe, places=2)


if __name__ == '__main__':
    gr_unittest.run(qa_plsync_cc)
