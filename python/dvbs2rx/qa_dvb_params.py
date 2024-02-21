#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright (c) 2019-2021 Igor Freire.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
from itertools import product
from gnuradio import gr_unittest

try:
    from gnuradio import dvbs2rx
except ImportError:
    import os
    import sys

    dirname, filename = os.path.split(os.path.abspath(__file__))
    sys.path.append(os.path.join(dirname, "bindings"))
    try:
        from gnuradio import dvbs2rx
    except ImportError:
        from python import dvbs2rx


class qa_dvb_params(gr_unittest.TestCase):
    def setUp(self):
        pass

    def test_dvbs2_params(self):
        standard = "DVB-S2"
        frame_size = "normal"
        code = "1/4"
        const = "qpsk"
        rolloff = 0.2
        pilots = True

        t_params = dvbs2rx.dvb_params(
            standard, frame_size, code, const, rolloff, pilots
        )
        self.assertIsNotNone(t_params)

        self.assertIsInstance(t_params.standard, dvbs2rx.dvb_standard_t)
        self.assertEqual(t_params.standard, dvbs2rx.STANDARD_DVBS2)

        self.assertIsInstance(t_params.framesize, dvbs2rx.dvb_framesize_t)
        self.assertEqual(t_params.framesize, dvbs2rx.FECFRAME_NORMAL)

        self.assertIsInstance(t_params.rate, dvbs2rx.dvb_code_rate_t)
        self.assertEqual(t_params.rate, dvbs2rx.C1_4)

        self.assertIsInstance(t_params.constellation, dvbs2rx.dvb_constellation_t)
        self.assertEqual(t_params.constellation, dvbs2rx.MOD_QPSK)

        self.assertIsInstance(t_params.rolloff, dvbs2rx.dvbs2_rolloff_factor_t)
        self.assertEqual(t_params.rolloff, dvbs2rx.RO_0_20)

        self.assertIsInstance(t_params.pilots, dvbs2rx.dvbs2_pilots_t)
        self.assertEqual(t_params.pilots, dvbs2rx.PILOTS_ON)

        try:
            _ = dvbs2rx.dvb_params(standard, "large", code)
            self.fail('Invalid frame size ("large")')
        except Exception:
            pass

        try:
            _ = dvbs2rx.dvb_params(standard, frame_size, "0/0")
            self.fail('Invalid code rate ("0/0")')
        except Exception:
            pass

        try:
            _ = dvbs2rx.dvb_params(standard, frame_size, code, constellation="pam")
            self.fail('Invalid constellation ("pam")')
        except Exception:
            pass

        try:
            _ = dvbs2rx.dvb_params(standard, frame_size, code, rolloff=0.19)
            self.fail('Invalid rolloff ("0.19")')
        except Exception:
            pass

    def test_dvbs2_modcod(self):
        standard = "DVB-S2"

        # First, try the MODCODs that work for normal and short FECFRAMEs
        modcods = [
            ("qpsk", "1/4"),
            ("qpsk", "1/3"),
            ("qpsk", "2/5"),
            ("qpsk", "1/2"),
            ("qpsk", "3/5"),
            ("qpsk", "2/3"),
            ("qpsk", "3/4"),
            ("qpsk", "4/5"),
            ("qpsk", "5/6"),
            ("qpsk", "8/9"),
            ("8psk", "3/5"),
            ("8psk", "2/3"),
            ("8psk", "3/4"),
            ("8psk", "5/6"),
            ("8psk", "8/9"),
        ]
        for frame_size in ["normal", "short"]:
            for const, code in modcods:
                self.assertIsNotNone(
                    dvbs2rx.dvb_params(standard, frame_size, code, const)
                )

        # Next, the code rates that supports normal FECFRAMEs only
        for const, code in [("qpsk", "9/10"), ("8psk", "9/10")]:
            self.assertIsNotNone(dvbs2rx.dvb_params(standard, "normal", code, const))
            try:
                _ = dvbs2rx.dvb_params(standard, "short", code, const)
                self.fail(
                    "Unsupported MODCOD " + const + code + " for normal FECFRAMES"
                )
            except Exception:
                pass

        # Unsupported MODCOD combinations
        unsupported_8psk = product(["8psk"], ["1/4", "1/3", "2/5", "1/2", "4/5"])
        for const, code in unsupported_8psk:
            for frame_size in ["normal", "short"]:
                try:
                    _ = dvbs2rx.dvb_params(standard, "short", code, const)
                    self.fail("Unsupported MODCOD " + const + code)
                except Exception:
                    pass
