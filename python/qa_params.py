#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2019-2021 Igor Freire.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

from gnuradio import gr_unittest
try:
    from dvbs2rx import params, dvbs2rx_python
except ImportError:
    import os
    import sys
    dirname, filename = os.path.split(os.path.abspath(__file__))
    sys.path.append(os.path.join(dirname, "bindings"))
    from dvbs2rx import params, dvbs2rx_python


class qa_params(gr_unittest.TestCase):
    def setUp(self):
        pass

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
                    params.validate(standard, frame_size, code, const))

        # Next, the code rates that supports normal FECFRAMEs only
        for const, code in [('qpsk', '9/10'), ('8psk', '9/10')]:
            self.assertTrue(params.validate(standard, 'normal', code, const))
            self.assertFalse(params.validate(standard, 'short', code, const))

    def test_dvbs2_modcod_translation(self):
        standard = 'DVB-S2'
        frame_size = 'normal'

        # First, try the MODCODs that work for normal and short FECFRAMEs
        modcods = [('qpsk', '1/4'), ('qpsk', '1/3'), ('qpsk', '2/5'),
                   ('qpsk', '1/2'), ('qpsk', '3/5'), ('qpsk', '2/3'),
                   ('qpsk', '3/4'), ('qpsk', '4/5'), ('qpsk', '5/6'),
                   ('qpsk', '8/9'), ('qpsk', '9/10'), ('8psk', '3/5'),
                   ('8psk', '2/3'), ('8psk', '3/4'), ('8psk', '5/6'),
                   ('8psk', '8/9'), ('8psk', '9/10')]
        for const, code in modcods:
            t_params = params.translate(standard, frame_size, code, const)
            self.assertIsInstance(t_params[0], dvbs2rx_python.dvb_standard_t)
            self.assertIsInstance(t_params[1], dvbs2rx_python.dvb_framesize_t)
            self.assertIsInstance(t_params[2], dvbs2rx_python.dvb_code_rate_t)
            self.assertIsInstance(t_params[3],
                                  dvbs2rx_python.dvb_constellation_t)


if __name__ == '__main__':
    gr_unittest.run(qa_params)
