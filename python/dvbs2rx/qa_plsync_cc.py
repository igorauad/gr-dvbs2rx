#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (c) 2019-2021 Igor Freire.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

import random
from math import floor, pi, sqrt

import numpy as np
import pmt
from gnuradio import analog, blocks, digital, gr, gr_unittest

try:
    from gnuradio.dvbs2rx import plsync_cc, rotator_cc
except ImportError:
    from python.dvbs2rx import plsync_cc, rotator_cc

SQRT2_2 = sqrt(2) / 2
JSQRT2_2 = SQRT2_2 * 1j
EXPJ45 = (+SQRT2_2 + JSQRT2_2)
EXPJ135 = (-SQRT2_2 + JSQRT2_2)
EXPJ225 = (-SQRT2_2 - JSQRT2_2)
EXPJ315 = (+SQRT2_2 - JSQRT2_2)


def get_test_plframe(pilots):
    """Generate a PLFRAME for testing

    Returns one of two PLFRAME options: (modcod=21, short_frame=1, pilots=1),
    or (modcod=21, short_frame=1, pilots=0).

    Args:
        pilots (bool]): Whether the test frame should contain pilots.

    Returns:
        (tuple): Tuple containing the PLFRAME (tuple), MODCOD (int), short
            frame flag (bool), and the number of slots (int).

    """
    if (pilots):
        # This PLHEADER corresponds to:
        # {MODCOD: 21, Short FECFRAME: 1, Pilots: 1}
        # {n_mod: 4, S: 45, PLFRAME length: 4212}
        modcod = 21
        short_frame = True
        slots = 45
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
        pilot_blk_1 = generate_pilot_blk([
            3, 2, 2, 2, 1, 1, 2, 3, 2, 0, 1, 3, 1, 2, 0, 0, 3, 0, 0, 0, 1, 2,
            2, 3, 2, 2, 3, 3, 1, 2, 3, 2, 3, 1, 1, 0
        ])
        pilot_blk_2 = generate_pilot_blk([
            0, 3, 1, 3, 3, 3, 1, 2, 2, 1, 1, 2, 2, 2, 3, 1, 2, 1, 3, 3, 2, 1,
            3, 0, 3, 1, 3, 1, 0, 0, 2, 0, 3, 2, 2, 2
        ])
        plframe = plheader + tuple(np.zeros(16 * 90)) + pilot_blk_1 + \
            tuple(np.zeros(16 * 90)) + pilot_blk_2 + \
            tuple(np.zeros(13 * 90))

    else:
        # This PLHEADER corresponds to:
        # {MODCOD: 21, Short FECFRAME: 1, Pilots: 0}
        # {n_mod: 4, S: 45, PLFRAME length: 4140}
        modcod = 21
        short_frame = True
        slots = 45
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
        payload = tuple(np.zeros(4140 - 90))
        plframe = plheader + payload

    return plframe, modcod, short_frame, slots


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


def replicate_plframes(n, plframe):
    """Replicate a given PLFRAME n times

    Args:
        n : Number of PLFRAMEs to generate.
        plframe (tuple): Test PLFRAME.

    Returns:
        (tuple) IQ sequence with n PLFRAMEs and n+1 PLHEADERs.

    """
    assert (n > 0)
    seq = tuple()
    for _ in range(n):
        seq += plframe
    # Add an extra PLHEADER in the end due to how PLFRAMEs are found
    # between two consecutive SOFs.
    seq += plframe[:90]
    return seq


def calc_noise_std(snr_db):
    """Compute the complex AWGN standard deviation for a target SNR

    This standard deviation is equivalent to sqrt(N0), in the usual N0 notation
    denoting the complex noise variance.

    """
    noise_var = 1.0 / (10**(float(snr_db) / 10))
    return sqrt(noise_var)


def rnd_const_mapper_input(n_symbols, bits_per_sym):
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


class qa_plsync_cc(gr_unittest.TestCase):

    def setUp(self):
        self.tb = gr.top_block()
        self.gold_code = 0
        self.debug_level = 0
        self.sps = 1.0  # oversampling ratio
        self.freq_est_period = 1

    def tearDown(self):
        self.tb = None

    def _run_flowgraph(self,
                       nframes,
                       freq_offset=0,
                       noise_std=0,
                       rnd_offset=False,
                       closed_loop=False,
                       pilots=False,
                       debug_tags=True):
        """Set up and run the flowgraph to completion

        Args:
            nframes (int): Number of PLFRAMEs to generate as input.
            freq_offset (float): Frequency offset disturbing the input symbols.
            noise_std (float): Standard deviation of the complex AWGN.
            rnd_offset (bool): Whether to simulate a time offset by prepeding a
                random-length all-zeros interval to the input stream.
            closed_loop (bool): Whether to use the closed-loop architecture,
                where the PL Sync block controls an external rotator block.
            pilots (bool): Whether to use a test PLFRAME containing pilots.
            debug_tags (bool): Wether to debug the XFECFRAME tags.

        Returns:
            (list) List of tags collected by the Tag Debug block if
            debug_tags=True. None otherwise.

        """
        # --- PL Sync parameters ---
        acm_vcm = True
        multistream = True
        pls_filter_lo = 0xFFFFFFFFFFFFFFFF
        pls_filter_hi = 0xFFFFFFFFFFFFFFFF

        # --- Input ---
        self._set_test_plframe(pilots)
        in_syms = replicate_plframes(nframes, self.plframe)
        if (rnd_offset):
            delay = 2 * np.random.randint(1000)
            in_syms = tuple(np.zeros(delay)) + in_syms
            # NOTE: the delay has to be an even number, otherwise the scheduler
            # may not process all the test samples.

        # Append some trailing zeros to ensure the flowgraph runner processes
        # all the PLFRAME IQ samples. That is, if the runner leaves some
        # samples unprocessed, make sure they are zero-padding samples.
        in_syms += tuple(np.zeros(100))

        # --- Blocks ---
        src = blocks.vector_source_c(in_syms)
        phase_rot = 2 * pi * freq_offset
        chan_rot = rotator_cc(phase_rot)  # channel rotation
        freq_corr_rot = rotator_cc(0, True)  # frequency correction
        # Restrict the rotator output buffer to reduce the chances of missing
        # the frequency corrections published via message port.
        freq_corr_rot.set_max_output_buffer(1024)
        noise = analog.noise_source_c(analog.GR_GAUSSIAN, noise_std, 0)
        nadder = blocks.add_cc()
        self.plsync = plsync = plsync_cc(self.gold_code, self.freq_est_period,
                                         self.sps, self.debug_level, acm_vcm,
                                         multistream, pls_filter_lo,
                                         pls_filter_hi)
        if debug_tags:
            snk = blocks.tag_debug(gr.sizeof_gr_complex, "XFECFRAME")
            snk.set_save_all(True)
        else:
            snk = blocks.null_sink(gr.sizeof_gr_complex)

        # Scale the combination of signal+noise such that it achieves unit RMS.
        # This scaling represents a static/converged (ideal) AGC. It is
        # required because the PL Sync block assumes the input has unit RMS.
        rms = sqrt(noise_std**2 + 1)  # assuming the original signal RMS=1
        gain = 1 / rms
        static_agc = blocks.multiply_const_cc(gain)

        # --- Connections ---
        self.tb.connect(src, chan_rot)
        self.tb.connect(chan_rot, (nadder, 0))
        self.tb.connect(noise, (nadder, 1))
        if (closed_loop):
            self.tb.msg_connect((plsync, 'rotator_phase_inc'),
                                (freq_corr_rot, 'cmd'))
            self.tb.connect(nadder, static_agc, freq_corr_rot, plsync, snk)
        else:
            self.tb.connect(nadder, static_agc, plsync, snk)

        # --- Run ---
        self.tb.run()

        if (debug_tags):
            return snk.current_tags()

    def _set_test_plframe(self, pilots):
        """Define the PLFRAME to be used for testing"""
        plframe, modcod, short_frame, slots = get_test_plframe(pilots)
        n_pilot_blks = floor(slots / 16) if pilots else 0
        self.frame_len = ((slots + 1) * 90) + (n_pilot_blks * 36)
        self.xfecframe_len = slots * 90
        self.modcod = modcod
        self.short_frame = short_frame
        self.plframe = plframe

    def _assert_tags(self,
                     tags,
                     key,
                     n_expected,
                     val_expected,
                     offset_expected,
                     val_equality=True,
                     places=7):
        """Assert that tag(s) are located and valued as expected

        Args:
            tags (list): Vector of tags obtained from the Tag Debug block..
            key (str): Key of the tag of interest.
            n_expected (int): Number of tags that were expected.
            val_expected (list): Values expected for each tag.
            offset_expected (list): Symbol index where the tags are expected.
            val_equality (bool): Whether the expected value should be checked
                for equality. Otherwise, expect the tag value to be greater
                than the corresponding `val_expected`.
            places (int): decimal places when checking equality

        """
        assert (type(val_expected) is list)
        assert (type(offset_expected) is list)
        assert (n_expected == len(val_expected))
        assert (n_expected == len(offset_expected))

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
                tag_val = (pmt.to_double(tag.value), )

            if (not isinstance(val_expected[i], tuple)):
                val_expected[i] = (val_expected[i], )

            for j, val in enumerate(val_expected[i]):
                if (val_equality):
                    self.assertAlmostEqual(tag_val[j], val, places=places)
                else:
                    self.assertTrue(tag_val[j] > val)

            # Check the location (symbol index/offset) of the tag
            self.assertEqual(tag.offset, offset_expected[i])

    def test_detect_ideal_plheader(self):
        """Test detection of ideal/noiseless modulated PLHEADERs

        The PL Sync block shall tag the first data symbol after each PLHEADER,
        i.e., the start of each output XFECFRAME. Each tag must contain the
        correct PLSC information.

        """
        nframes = 2  # at least two PLFRAMEs for locking
        tags = self._run_flowgraph(nframes)
        self._assert_tags(tags, "XFECFRAME", 2,
                          [(self.modcod, self.short_frame),
                           (self.modcod, self.short_frame)],
                          [0, self.xfecframe_len])

    def test_detect_noisy_plheader_open_loop(self):
        """Test detection of a noisy modulated PLHEADER in open loop

        In this case, the PLHEADER is disturbed by frequency offset, AWGN, and
        a time offset (delay). Test the open-loop approach, where the PL Sync
        block does not rely on an external rotator for frequency correction.

        The PL Sync block should detect the PLHEADERs and tag the start of the
        corresponding XFECFRAMEs. The random time offset should not change the
        tag location, as the corresponding zero samples representing the time
        offset do not reach the output (only the actual XFECFRAMEs are output).

        """
        snr_db = 5.0  # SNR in dB
        fe = 0.2  # normalized frequency offset
        nframes = 2  # at least two PLFRAMEs for locking
        tags = self._run_flowgraph(nframes,
                                   freq_offset=fe,
                                   noise_std=calc_noise_std(snr_db),
                                   rnd_offset=True)
        self._assert_tags(tags, "XFECFRAME", 2,
                          [(self.modcod, self.short_frame),
                           (self.modcod, self.short_frame)],
                          [0, self.xfecframe_len])

    def test_detect_noisy_plheader_closed_loop(self):
        """Test detection of a noisy modulated PLHEADER in closed loop

        Same as the preceding test, but in closed loop.

        """
        snr_db = 5.0  # SNR in dB
        fe = 0.2  # normalized frequency offset
        nframes = 2  # at least two PLFRAMEs for locking
        tags = self._run_flowgraph(nframes,
                                   freq_offset=fe,
                                   noise_std=calc_noise_std(snr_db),
                                   rnd_offset=True,
                                   closed_loop=True)
        self._assert_tags(tags, "XFECFRAME", 2,
                          [(self.modcod, self.short_frame),
                           (self.modcod, self.short_frame)],
                          [0, self.xfecframe_len])

    def test_non_plheader_qpsk(self):
        """Test random sequence of noisy QPSK symbols

        This sequence does not contain the PLHEADER and, therefore, it should
        not lead to a false-positive frame detection.

        """
        # Parameters
        snr_db = 0.0
        frame_len = 10000
        agc_rate = 1e-2
        agc_gain = 1
        agc_ref = 1
        acm_vcm = True
        multistream = True
        pls_filter_lo = 0xFFFFFFFFFFFFFFFF
        pls_filter_hi = 0xFFFFFFFFFFFFFFFF

        # Constants
        bits_per_sym = 2
        noise_std = calc_noise_std(snr_db)
        qpsk_const = (EXPJ225, EXPJ315, EXPJ135, EXPJ45)

        # Random input symbols
        in_vec = rnd_const_mapper_input(frame_len, bits_per_sym)

        # Flowgraph
        src = blocks.vector_source_b(in_vec)
        pack = blocks.repack_bits_bb(1, bits_per_sym, "", False,
                                     gr.GR_MSB_FIRST)
        cmap = digital.chunks_to_symbols_bc(qpsk_const)
        noise = analog.noise_source_c(analog.GR_GAUSSIAN, noise_std, 0)
        nadder = blocks.add_cc()
        agc = analog.agc_cc(agc_rate, agc_ref, agc_gain)
        agc.set_max_gain(65536)
        plsync = plsync_cc(self.gold_code, self.freq_est_period, self.sps,
                           self.debug_level, acm_vcm, multistream,
                           pls_filter_lo, pls_filter_hi)
        snk = blocks.tag_debug(gr.sizeof_gr_complex, "SOF")

        # Connect source into flowgraph
        self.tb.connect(src, pack, cmap)
        self.tb.connect(cmap, (nadder, 0))
        self.tb.connect(noise, (nadder, 1))
        self.tb.connect(nadder, agc, plsync, snk)
        self.tb.run()

        # Check if there was any false positive
        tags = snk.current_tags()
        self.assertTrue(len(tags) == 0)

    def test_freq_est_noiseless_rot_stream_open_loop(self):
        """Test frequency offset estimation over noiseless but rotated stream

        Use the open-loop approach, where the PL Sync works standalone without
        an external rotator block.

        Since the PLFRAME stream is noiseless and only rotated, use the default
        period=1 for the coarse frequency offset estimation.

        """
        fe = np.random.uniform(-.25, .25)  # normalized frequency offset
        nframes = 1  # one frame is enough to estimate
        self._run_flowgraph(nframes, freq_offset=fe, debug_tags=False)
        self.assertAlmostEqual(self.plsync.get_freq_offset(), fe)

    def test_freq_est_noiseless_rot_stream_closed_loop_pilotless_mode(self):
        """Test closed-loop freq. estimation over a noiseless+rotated stream

        Same as the previous test, but in closed-loop.

        """
        fe = np.random.uniform(-.25, .25)  # normalized frequency offset
        nframes = 3
        # Three frames are enough to produce one fine frequency offset
        # estimate. The second PLHEADER is when the frame synchronizer locks,
        # so it sends the frequency correction to the rotator. On the third
        # PLHEADER, the correction is already applied, so the frequency
        # synchronizer should enter the coarse-corrected state. Finally, the
        # fine estimate is obtained after processing the third payload.
        self._run_flowgraph(nframes,
                            freq_offset=fe,
                            closed_loop=True,
                            debug_tags=False)
        self.assertAlmostEqual(self.plsync.get_freq_offset(), fe)

    def test_freq_est_noiseless_rot_stream_closed_loop_pilot_mode(self):
        """Test closed-loop freq. estimation over a noiseless+rotated stream

        Same as the previous test, but now with PLFRAMEs containing pilot
        symbols so that the pilot-mode fine frequency offset estimation can be
        used by the PL Sync block.

        """
        fe = np.random.uniform(-.25, .25)  # normalized frequency offset
        nframes = 3  # see the explanation in the previous test
        self._run_flowgraph(nframes,
                            freq_offset=fe,
                            closed_loop=True,
                            pilots=True,
                            debug_tags=False)
        self.assertAlmostEqual(self.plsync.get_freq_offset(), fe)

    def test_freq_est_noisy_rot_stream_closed_loop(self):
        """Test closed-loop freq. estimation over a noisy but rotated stream

        This time, introduce AWGN at various SNR levels. To overcome the noise,
        estimate the coarse frequency offset over a sufficiently large period
        to achieve better averaging. Also, use the closed-loop architecture,
        and PLFRAMEs containing pilot symbols, such that fine frequency offset
        estimations can be used to boost the performance.

        """
        nframes = 200
        self.freq_est_period = 30
        for snr_db in reversed(range(0, 10)):
            print("Try SNR: %.1f dB" % (snr_db))
            fe = np.random.uniform(-.25, .25)  # random frequency offset
            self._run_flowgraph(nframes,
                                freq_offset=fe,
                                noise_std=calc_noise_std(snr_db),
                                closed_loop=True,
                                pilots=True,
                                debug_tags=False)
            # Expect a rough estimate, especially due to the low SNR levels
            self.assertAlmostEqual(self.plsync.get_freq_offset(), fe, places=2)


if __name__ == '__main__':
    gr_unittest.run(qa_plsync_cc)
