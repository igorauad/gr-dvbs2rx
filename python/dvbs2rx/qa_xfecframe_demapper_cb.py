#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2023 Igor Freire.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

from gnuradio import gr, gr_unittest

try:
    from gnuradio.dvbs2rx import (C3_5, FECFRAME_NORMAL, MOD_QPSK,
                                  xfecframe_demapper_cb)
except ImportError:
    from python.dvbs2rx import (C3_5, FECFRAME_NORMAL, MOD_QPSK,
                                xfecframe_demapper_cb)


class qa_xfecframe_demapper_cb(gr_unittest.TestCase):

    def setUp(self):
        self.tb = gr.top_block()

    def tearDown(self):
        self.tb = None

    def test_instance(self):
        instance = xfecframe_demapper_cb(FECFRAME_NORMAL, C3_5, MOD_QPSK)
        assert (instance is not None)


if __name__ == '__main__':
    gr_unittest.run(qa_xfecframe_demapper_cb)
