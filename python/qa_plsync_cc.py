#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2019-2021 Igor Freire.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

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

from math import sqrt, pi
import random
import pmt
import numpy as np


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

        # Modulated PLHEADER with scrambled PLSC
        self.mod_plheader = (
            (+0.7071 + 0.7071j), (+0.7071 - 0.7071j), (-0.7071 - 0.7071j),
            (-0.7071 + 0.7071j), (+0.7071 + 0.7071j), (-0.7071 + 0.7071j),
            (-0.7071 - 0.7071j), (+0.7071 - 0.7071j), (+0.7071 + 0.7071j),
            (+0.7071 - 0.7071j), (+0.7071 + 0.7071j), (-0.7071 + 0.7071j),
            (-0.7071 - 0.7071j), (-0.7071 + 0.7071j), (-0.7071 - 0.7071j),
            (+0.7071 - 0.7071j), (-0.7071 - 0.7071j), (-0.7071 + 0.7071j),
            (-0.7071 - 0.7071j), (-0.7071 + 0.7071j), (+0.7071 + 0.7071j),
            (-0.7071 + 0.7071j), (+0.7071 + 0.7071j), (-0.7071 + 0.7071j),
            (-0.7071 - 0.7071j), (-0.7071 + 0.7071j), (-0.7071 - 0.7071j),
            (-0.7071 + 0.7071j), (-0.7071 - 0.7071j), (+0.7071 - 0.7071j),
            (-0.7071 - 0.7071j), (+0.7071 - 0.7071j), (+0.7071 + 0.7071j),
            (+0.7071 - 0.7071j), (-0.7071 - 0.7071j), (-0.7071 + 0.7071j),
            (-0.7071 - 0.7071j), (-0.7071 + 0.7071j), (-0.7071 - 0.7071j),
            (+0.7071 - 0.7071j), (-0.7071 - 0.7071j), (-0.7071 + 0.7071j),
            (+0.7071 + 0.7071j), (+0.7071 - 0.7071j), (+0.7071 + 0.7071j),
            (-0.7071 + 0.7071j), (-0.7071 - 0.7071j), (+0.7071 - 0.7071j),
            (-0.7071 - 0.7071j), (+0.7071 - 0.7071j), (-0.7071 - 0.7071j),
            (+0.7071 - 0.7071j), (-0.7071 - 0.7071j), (+0.7071 - 0.7071j),
            (-0.7071 - 0.7071j), (-0.7071 + 0.7071j), (-0.7071 - 0.7071j),
            (-0.7071 + 0.7071j), (+0.7071 + 0.7071j), (+0.7071 - 0.7071j),
            (-0.7071 - 0.7071j), (-0.7071 + 0.7071j), (+0.7071 + 0.7071j),
            (-0.7071 + 0.7071j), (+0.7071 + 0.7071j), (-0.7071 + 0.7071j),
            (-0.7071 - 0.7071j), (-0.7071 + 0.7071j), (+0.7071 + 0.7071j),
            (-0.7071 + 0.7071j), (-0.7071 - 0.7071j), (+0.7071 - 0.7071j),
            (-0.7071 - 0.7071j), (-0.7071 + 0.7071j), (+0.7071 + 0.7071j),
            (-0.7071 + 0.7071j), (+0.7071 + 0.7071j), (+0.7071 - 0.7071j),
            (-0.7071 - 0.7071j), (+0.7071 - 0.7071j), (-0.7071 - 0.7071j),
            (-0.7071 + 0.7071j), (+0.7071 + 0.7071j), (-0.7071 + 0.7071j),
            (-0.7071 - 0.7071j), (+0.7071 - 0.7071j), (+0.7071 + 0.7071j),
            (+0.7071 - 0.7071j), (-0.7071 - 0.7071j), (-0.7071 + 0.7071j))
        # This PLHEADER corresponds to:
        # {MODCOD: 21, Short FECFRAME: 1, Pilots: 0}
        # {n_mod: 4, S: 45, PLFRAME length: 4140}
        self.frame_len = 4140
        self.modcod = 21
        self.short_frame = True

        # QPSK const symbols
        self.qpsk_const = ((-0.7071 - 0.7071j), (+0.7071 - 0.7071j),
                           (-0.7071 + 0.7071j), (+0.7071 + 0.7071j))

    def tearDown(self):
        self.tb = None

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

        zeros = tuple(np.zeros(self.frame_len - 90))
        frame = self.mod_plheader + zeros
        seq = frame

        i_rep = 0
        while (i_rep < n - 1):
            seq += frame
            i_rep += 1

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

        # If a frame start was detected, the block added a tag on the first
        # data symbol after the PLHEADER. In this case, we expect two tags, one
        # for each frame. They will be on the same symbol, because the first
        # frame does not lead to any data output (frame sync is not locked
        # yet), so its tag is accumulated into the first symbol of the second
        # frame.
        #
        # It is required that the tag lies exactly on the first output data
        # symbol. Also, it must contain the correct PLSC info.
        tags = snk.current_tags()
        self._assert_tags(tags, "XFECFRAME", 2,
                          [(self.modcod, self.short_frame),
                           (self.modcod, self.short_frame)], [0, 0])

    # @unittest.skip
    def test_detect_noisy_plheader(self):
        """Test detection of a noisy modulated PLHEADER

        Now the modulated PLHEADER has both phase rotation (due to
        freq. offset) and is under AWGN.

        """

        # Paramers
        snr_db = 3.0  # SNR in dB
        fe = 0.2  # normalized frequency offset

        # Constants
        phase_inc = 2 * pi * fe

        # AWGN standard dev
        noise_var = 1.0 / (10**(float(snr_db) / 10))
        noise_std = sqrt(noise_var)

        # Shift the PLHEADER a little for testing
        mod_syms = self._gen_plframes(2)  # at least 2 PLFRAMEs for locking
        zeros = (0, 0, 0, 0, 0)
        mod_syms = zeros + mod_syms

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

        # Check the same conditions as in the previous test. The zeros
        # preceding the frame symbols should not change anything, as the PL
        # Sync block shall only output data symbols when frame-locked.
        tags = snk.current_tags()
        self._assert_tags(tags, "XFECFRAME", 2,
                          [(self.modcod, self.short_frame),
                           (self.modcod, self.short_frame)], [0, 0])

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

        # Random input symbols
        in_vec = self._rnd_const_mapper_input(frame_len, bits_per_sym)

        # Random noise
        noise_var = 1.0 / (10**(float(snr_db) / 10))
        noise_std = sqrt(noise_var)

        # Flowgraph
        src = blocks.vector_source_b(in_vec)
        pack = blocks.repack_bits_bb(1, bits_per_sym, "", False,
                                     gr.GR_MSB_FIRST)
        cmap = digital.chunks_to_symbols_bc(self.qpsk_const)
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
    def test_freq_est_noiseless_rot_plheader(self):
        """Test frequency offset estimation on noiseless, but rotated PLHEADER

        """

        # Parameters
        fe = 0.2  # normalized frequency offset

        # Constants
        phase_inc = 2 * pi * fe
        frames = self._gen_plframes(2)  # two frames is enough for locking

        # Flowgraph
        src = blocks.vector_source_c(frames)
        rot = blocks.rotator_cc(phase_inc)
        plsync = plsync_cc(self.gold_code, self.freq_est_period, self.sps,
                           self.debug_level)
        snk = blocks.null_sink(gr.sizeof_gr_complex)
        self.tb.connect(src, rot, plsync, snk)
        self.tb.run()

        # Check results
        #
        # NOTE: the Tag debug block does not suit very well for frequency
        # offset estimation tests. The issue is that the Tag Debug block resets
        # its internal list of tags on every call to its work function. Since
        # frequency offset estimations are only produced after frame lock, a
        # large number of symbols must be processed, which likely means
        # multiple calls to Tag Debug' work function. Instead of checking tags,
        # the most recent frequency offset estimation is read directly from the
        # plsync block and compared to the target.
        self.assertAlmostEqual(plsync.get_freq_offset(), fe, places=3)

    # @unittest.skip
    def test_freq_est_noiseless_rot_plheader_rep(self):
        """Test frequency offset estimation baed on several PLHEADER repetitions

        The algorithm tries to combine multiple PLHEADERs in order to improve
        the estimation. This test evaluates how performance improves over time
        for a rotated PLHEADER under AWGN.

        """

        # Paramers
        n_frames = 30
        freq_est_period = 10  # estimation period in frames
        snr_db = 3  # SNR in dB

        # Constants
        fe = random.uniform(0, 0.25)  # normalized frequency offset
        phase_inc = 2 * pi * fe
        frames = self._gen_plframes(n_frames)

        # Noise amplitude for the target SNR
        noise_var = 1.0 / (10**(float(snr_db) / 10))
        noise_std = sqrt(noise_var)

        # Flowgraph
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

        self.assertAlmostEqual(plsync.get_freq_offset(), fe, places=3)

    # @unittest.skip
    def test_freq_est_noisy_rot_plheader(self):
        """Test frequency offset estimation on noisy rotated PLHEADER

        """

        # Paramers
        n_frames = 200
        freq_est_period = 20

        # Constants
        frames = self._gen_plframes(n_frames)

        # Iterate over SNR levels
        for snr_db in reversed(range(-1, 10)):
            print("Try SNR: %.1f dB" % (snr_db))
            # Generate a new uniformly-distributed freq. offset realization
            fe = random.uniform(0, 0.25)
            phase_inc = 2 * pi * fe

            # Update noise amplitude for the target SNR
            noise_var = 1.0 / (10**(float(snr_db) / 10))
            noise_std = sqrt(noise_var)

            # Need to re-generate the flowgraph every time in order to clear
            # the tags within the Tag Debug block
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

            self.assertAlmostEqual(plsync.get_freq_offset(), fe, places=2)


if __name__ == '__main__':
    gr_unittest.run(qa_plsync_cc)
