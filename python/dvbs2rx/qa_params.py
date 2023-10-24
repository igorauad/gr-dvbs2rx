#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (c) 2019-2021 Igor Freire.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
import itertools

from gnuradio import gr_unittest

try:
    import gnuradio.dvbs2rx as dvbs2rx
except ImportError:
    import python.dvbs2rx as dvbs2rx


class qa_params(gr_unittest.TestCase):

    def setUp(self):
        pass

    def test_dvbs2_params_validation(self):
        standard = 'DVB-S2'
        frame_size = 'normal'
        code = '1/4'
        const = 'qpsk'
        rolloff = 0.2
        pilots = True

        # Validate parameters starting from the minimum set of required
        # parameters and, then, including the optional parameters.
        self.assertTrue(dvbs2rx.params.validate(standard, frame_size, code))
        self.assertTrue(
            dvbs2rx.params.validate(standard, frame_size, code, const))
        self.assertTrue(
            dvbs2rx.params.validate(standard, frame_size, code, const,
                                    rolloff))
        self.assertTrue(
            dvbs2rx.params.validate(standard, frame_size, code, const, rolloff,
                                    pilots))

        # Invalid frame size ("large")
        self.assertFalse(dvbs2rx.params.validate(standard, 'large', code))

        # Invalid code rate ("0/0")
        self.assertFalse(dvbs2rx.params.validate(standard, frame_size, '0/0'))

        # Invalid constellation ("pam")
        self.assertFalse(
            dvbs2rx.params.validate(standard,
                                    frame_size,
                                    code,
                                    constellation="pam"))

        # Invalid rolloff ("0.19")
        self.assertFalse(
            dvbs2rx.params.validate(standard,
                                    frame_size,
                                    code,
                                    const,
                                    rolloff=0.19))

        # Invalid pilots ("1" instead of "True")
        self.assertFalse(
            dvbs2rx.params.validate(standard, frame_size, code, pilots=1))

    def test_dvbs2_modcod_validation(self):
        standard = 'DVB-S2'

        # First, try the MODCODs that work for normal and short FECFRAMEs
        modcods = [('qpsk', '1/4'), ('qpsk', '1/3'), ('qpsk', '2/5'),
                   ('qpsk', '1/2'), ('qpsk', '3/5'), ('qpsk', '2/3'),
                   ('qpsk', '3/4'), ('qpsk', '4/5'), ('qpsk', '5/6'),
                   ('qpsk', '8/9'), ('8psk', '3/5'), ('8psk', '2/3'),
                   ('8psk', '3/4'), ('8psk', '5/6'), ('8psk', '8/9')]
        for frame_size in ['normal', 'short']:
            for const, code in modcods:
                self.assertTrue(
                    dvbs2rx.params.validate(standard, frame_size, code, const))

        # Next, the code rates that supports normal FECFRAMEs only
        for const, code in [('qpsk', '9/10'), ('8psk', '9/10')]:
            self.assertTrue(
                dvbs2rx.params.validate(standard, 'normal', code, const))
            self.assertFalse(
                dvbs2rx.params.validate(standard, 'short', code, const))

        # Unsupported MODCOD combinations
        unsupported_8psk = itertools.product(
            ['8psk'], ['1/4', '1/3', '2/5', '1/2', '4/5'])
        for const, code in unsupported_8psk:
            for frame_size in ['normal', 'short']:
                self.assertFalse(
                    dvbs2rx.params.validate(standard, frame_size, code, const))

    def test_dvbs2_modcod_translation(self):
        standard = 'DVB-S2'
        frame_size = 'normal'
        modcods = [('qpsk', '1/4'), ('qpsk', '1/3'), ('qpsk', '2/5'),
                   ('qpsk', '1/2'), ('qpsk', '3/5'), ('qpsk', '2/3'),
                   ('qpsk', '3/4'), ('qpsk', '4/5'), ('qpsk', '5/6'),
                   ('qpsk', '8/9'), ('qpsk', '9/10'), ('8psk', '3/5'),
                   ('8psk', '2/3'), ('8psk', '3/4'), ('8psk', '5/6'),
                   ('8psk', '8/9'), ('8psk', '9/10')]
        for const, code in modcods:
            t_params = dvbs2rx.params.translate(standard, frame_size, code,
                                                const)
            self.assertIsInstance(t_params[0], dvbs2rx.dvb_standard_t)
            self.assertIsInstance(t_params[1], dvbs2rx.dvb_framesize_t)
            self.assertIsInstance(t_params[2], dvbs2rx.dvb_code_rate_t)
            self.assertIsInstance(t_params[3], dvbs2rx.dvb_constellation_t)

    def test_optional_params_translation(self):
        standard = 'DVB-S2'
        frame_size = 'normal'
        code = '1/4'
        const = 'qpsk'
        rolloff = 0.2
        pilots = True
        t_params = dvbs2rx.params.translate(standard, frame_size, code, const,
                                            rolloff, pilots)
        self.assertIsInstance(t_params[0], dvbs2rx.dvb_standard_t)
        self.assertIsInstance(t_params[1], dvbs2rx.dvb_framesize_t)
        self.assertIsInstance(t_params[2], dvbs2rx.dvb_code_rate_t)
        self.assertIsInstance(t_params[3], dvbs2rx.dvb_constellation_t)
        self.assertIsInstance(t_params[4], dvbs2rx.dvbs2_rolloff_factor_t)
        self.assertIsInstance(t_params[5], dvbs2rx.dvbs2_pilots_t)

    def test_pls_parsing(self):
        self.assertEqual(
            dvbs2rx.params.dvbs2_pls("QPSK", "1/4", "normal", False), 1 << 2)
        self.assertEqual(
            dvbs2rx.params.dvbs2_pls("8PSK", "3/5", "normal", False), 12 << 2)
        self.assertEqual(
            dvbs2rx.params.dvbs2_pls("QPSK", "3/5", "normal", False), 5 << 2)
        self.assertEqual(
            dvbs2rx.params.dvbs2_pls("QPSK", "3/5", "normal", True),
            (5 << 2) + 1)
        self.assertEqual(
            dvbs2rx.params.dvbs2_pls("QPSK", "3/5", "short", False),
            (5 << 2) + 2)
        self.assertEqual(
            dvbs2rx.params.dvbs2_pls("QPSK", "3/5", "short", True),
            (5 << 2) + 3)

    def test_pls_filter(self):
        # Single PLS, lower u64 bits only
        self.assertEqual(dvbs2rx.params.pls_filter(0), (1, 0))
        self.assertEqual(dvbs2rx.params.pls_filter(1), (2, 0))
        self.assertEqual(dvbs2rx.params.pls_filter(2), (1 << 2, 0))
        self.assertEqual(dvbs2rx.params.pls_filter(63), (1 << 63, 0))
        # Single PLS, upper u64 bits only
        self.assertEqual(dvbs2rx.params.pls_filter(64), (0, 1))
        self.assertEqual(dvbs2rx.params.pls_filter(65), (0, 2))
        self.assertEqual(dvbs2rx.params.pls_filter(66), (0, 1 << 2))
        self.assertEqual(dvbs2rx.params.pls_filter(127), (0, 1 << 63))
        # Multiple PLSs
        self.assertEqual(dvbs2rx.params.pls_filter(0, 1, 2, 3), (15, 0))
        self.assertEqual(dvbs2rx.params.pls_filter(64, 65, 66, 67), (0, 15))
        with self.assertRaises(ValueError):
            dvbs2rx.params.pls_filter(128)

    def test_pl_info(self):
        info = dvbs2rx.params.pl_info("QPSK", "1/4", "normal", True)
        self.assertEqual(info['n_mod'], 2)
        self.assertEqual(info['n_slots'], 360)
        self.assertEqual(info['n_pilots'], 22)
        self.assertEqual(info['plframe_len'], 90 + (22 * 36) + (360 * 90))
        self.assertEqual(info['xfecframe_len'], 360 * 90)

        info = dvbs2rx.params.pl_info("QPSK", "1/4", "normal", False)
        self.assertEqual(info['n_mod'], 2)
        self.assertEqual(info['n_slots'], 360)
        self.assertEqual(info['n_pilots'], 0)
        self.assertEqual(info['plframe_len'], 90 + (360 * 90))
        self.assertEqual(info['xfecframe_len'], 360 * 90)

        info = dvbs2rx.params.pl_info("QPSK", "1/4", "short", False)
        self.assertEqual(info['n_mod'], 2)
        self.assertEqual(info['n_slots'], 90)
        self.assertEqual(info['n_pilots'], 0)
        self.assertEqual(info['plframe_len'], 90 + (90 * 90))
        self.assertEqual(info['xfecframe_len'], 90 * 90)

        info = dvbs2rx.params.pl_info("8PSK", "8/9", "short", True)
        self.assertEqual(info['n_mod'], 3)
        self.assertEqual(info['n_slots'], 60)
        self.assertEqual(info['n_pilots'], 3)
        self.assertEqual(info['plframe_len'], 90 + (60 * 90) + (3 * 36))
        self.assertEqual(info['xfecframe_len'], 60 * 90)

        info = dvbs2rx.params.pl_info("8PSK", "8/9", "short", False)
        self.assertEqual(info['n_mod'], 3)
        self.assertEqual(info['n_slots'], 60)
        self.assertEqual(info['n_pilots'], 0)
        self.assertEqual(info['plframe_len'], 90 + (60 * 90))
        self.assertEqual(info['xfecframe_len'], 60 * 90)


if __name__ == '__main__':
    gr_unittest.run(qa_params)
