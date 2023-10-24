#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (c) 2019-2021 Igor Freire.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
import numpy as np
from gnuradio import blocks, gr, gr_unittest
from gnuradio.filter import firdes
from scipy.signal import convolve, upfirdn

try:
    from gnuradio.dvbs2rx import symbol_sync_cc
except ImportError:
    from python.dvbs2rx import symbol_sync_cc


def randQpskSyms(nsyms):
    """Generate random unit-energy QPSK symbols"""
    const = np.array([(-1 + 1j), (-1 - 1j), (1 + 1j), (1 - 1j)]) / np.sqrt(2)
    data = np.random.randint(4, size=nsyms)
    return [const[i] for i in data]


class qa_plsync_cc(gr_unittest.TestCase):

    def _set_up_flowgraph(self,
                          in_stream,
                          sps=2,
                          loop_bw=0.01,
                          damping=1.0,
                          rolloff=0.2,
                          rrc_delay=5,
                          n_subfilt=128,
                          tag_period=None):
        """Set up the flowgraph

        Vector Source -> Symbol Sync -> Vector Sink

        Optionally inserts a "stream-to-tagged-stream" block between the vector
        source and the symbol sync when a tag period is defined.

        Args:
            in_stream (list): Stream to feed into the symbol synchronizer.
            sps (int): Samples per symbol (oversampling ratio).
            loop_bw (float): Loop bandwidth.
            damping (float): Loop damping factor.
            rolloff (float): Raised cosine rolloff factor.
            tag_offset (int): Tag period in samples. Disabled when None.

        """
        interp_method = 1  # test with the linear interpolator for simplicity
        # TODO: test all other interpolation methods
        src = blocks.vector_source_c(tuple(in_stream))
        symbol_sync = symbol_sync_cc(sps, loop_bw, damping, rolloff, rrc_delay,
                                     n_subfilt, interp_method)
        self.sink = blocks.vector_sink_c()

        if (tag_period is not None):
            tagger = blocks.stream_to_tagged_stream(gr.sizeof_gr_complex, 1,
                                                    tag_period, "tag_key")
            self.tag_sink = blocks.tag_debug(gr.sizeof_gr_complex, "tag_key")
            self.tag_sink.set_save_all(True)
            self.tb.connect(src, tagger, symbol_sync, self.sink)
            self.tb.connect(symbol_sync, self.tag_sink)
        else:
            self.tb.connect(src, symbol_sync, self.sink)

    def setUp(self):
        self.tb = gr.top_block()

    def tearDown(self):
        self.tb = None

    def test_open_loop(self):
        """Test open-loop mode

        By setting the damping factor to 0, the PI loop constants K1 and K2
        become zero, so the symbol synchronizer operates in open loop. In this
        case, it should behave just like a downsampler, keeping one sample for
        every sps input samples.

        """
        sps = 2
        x_syms = list(map(complex, [1.0, -1.0, 3.0, -2.0, 2.0, -2.5, 1.5]))

        # Upsampling
        x_up = np.zeros(len(x_syms) * sps, dtype=complex)
        for i in range(len(x_syms)):
            x_up[2 * i] = x_syms[i]

        self._set_up_flowgraph(x_up, sps, damping=0)
        self.tb.run()
        # All symbols should be retained, except the first, since the symbol
        # synchronizer needs at least two symbols to start.
        self.assertListEqual(x_syms[1:], self.sink.data())

    def test_closed_loop_convergence(self):
        """Test whether the loop converges after enough symbols"""
        sps = 2
        rolloff = 0.2
        delay = 10
        nsyms = int(1e3)

        # RC pulse shaping filter (Tx filter convolved with matched filter)
        ntaps = sps * delay + 1
        rrc_taps = firdes.root_raised_cosine(1, sps, 1, rolloff, ntaps)
        rc_taps = convolve(rrc_taps, rrc_taps)
        rc_taps = rc_taps / max(rc_taps)

        # Random QPSK symbol sequence
        x_syms = randQpskSyms(nsyms)

        # Matched filter output
        mf_out = upfirdn(rc_taps, x_syms, sps)

        # Test
        self._set_up_flowgraph(mf_out,
                               sps,
                               loop_bw=0.001,
                               damping=1.0,
                               rolloff=rolloff)
        self.tb.run()
        # The output symbols should be all close to 0.707 after the loop
        # conveges. Check the output after the head/transitory of the
        # convolution and before its tail.
        output = self.sink.data()[delay * sps:-delay * sps]
        for x in output:
            self.assertAlmostEqual(abs(x.real), 1 / np.sqrt(2), delta=0.02)
            self.assertAlmostEqual(abs(x.imag), 1 / np.sqrt(2), delta=0.02)

    def test_reference_implementation(self):
        """Test fidelity relative to the reference MATLAB implementation

        Test the loop using input and output test vectors generated in MATLAB
        using function genTestVector() from the "igorauad/symbol_timing_sync"
        Github repository. More specifically, test against test vectors
        generated by the following calls:

        - genTestVector(50, 2, 0.2, 10, 'GTED', 1, 0.01, 1);

        Where nSymbols=50, sps=2, rolloff=0.2, rcDelay=10, tedChoice='GTED'
        (Gardner TED), interpChoice=1 (linear interpolator), loopBw = 0.01, and
        dampingFactor=1.

        - genTestVector(50, 4, 0.35, 12, 'GTED', 1, 0.005, 0.707);

        Where nSymbols=50, sps=4, rolloff=0.35, rcDelay=12, tedChoice='GTED'
        (Gardner TED), interpChoice=1 (linear interpolator), loopBw = 0.005,
        and dampingFactor=0.707.

        """
        test_in = [(0.707107 - 0.707107j), (1.085764 - 1.050358j),
                   (0.707107 - 0.707107j), (-0.259122 + 0.197291j),
                   (-0.707107 + 0.707107j), (-0.102595 + 0.204492j),
                   (0.707107 - 0.707107j), (0.856821 - 1.053693j),
                   (0.707107 - 0.707107j), (0.802733 - 0.059292j),
                   (0.707107 + 0.707107j), (-0.003331 + 1.191152j),
                   (-0.707107 + 0.707107j), (-0.792778 - 0.413892j),
                   (-0.707107 - 0.707107j), (-0.864388 + 0.189681j),
                   (-0.707107 + 0.707107j), (0.109304 - 0.057207j),
                   (0.707107 - 0.707107j), (0.242218 - 0.058883j),
                   (-0.707107 + 0.707107j), (-1.043606 + 0.192832j),
                   (-0.707107 - 0.707107j), (-0.478239 - 0.413689j),
                   (-0.707107 + 0.707107j), (-0.954121 + 1.187821j),
                   (-0.707107 + 0.707107j), (0.010256 - 0.048774j),
                   (0.707107 - 0.707107j), (0.927439 - 1.078381j),
                   (0.707107 - 0.707107j), (0.535148 + 0.258703j),
                   (0.707107 + 0.707107j), (0.920732 + 0.094976j),
                   (0.707107 - 0.707107j), (0.020271 - 0.849456j),
                   (-0.707107 - 0.707107j), (-0.974392 - 0.809537j),
                   (-0.707107 - 0.707107j), (-0.439481 + 0.010015j),
                   (-0.707107 + 0.707107j), (-1.105032 + 0.785913j),
                   (-0.707107 + 0.707107j), (0.337813 + 0.885267j),
                   (0.707107 + 0.707107j), (-0.079229 - 0.156402j),
                   (-0.707107 - 0.707107j), (-0.134354 - 0.153454j),
                   (0.707107 + 0.707107j), (0.409975 + 0.888981j),
                   (-0.707107 + 0.707107j), (-1.229742 + 0.768995j),
                   (-0.707107 + 0.707107j), (0.157978 + 0.046187j),
                   (0.707107 - 0.707107j), (0.842389 - 0.881985j),
                   (0.707107 - 0.707107j), (0.577620 - 0.703915j),
                   (0.707107 - 0.707107j), (0.910882 - 0.207984j),
                   (0.707107 + 0.707107j), (0.000766 + 1.217090j),
                   (-0.707107 + 0.707107j), (-0.928971 - 0.328350j),
                   (-0.707107 - 0.707107j), (-0.517463 - 0.055451j),
                   (-0.707107 + 0.707107j), (-0.984648 + 0.823436j),
                   (-0.707107 + 0.707107j), (0.128289 + 0.829737j),
                   (0.707107 + 0.707107j), (0.681463 - 0.055451j),
                   (0.707107 - 0.707107j), (0.998016 - 0.338770j),
                   (0.707107 + 0.707107j), (-0.314741 + 1.240309j),
                   (-0.707107 + 0.707107j), (0.134759 - 0.253810j),
                   (0.707107 - 0.707107j), (-0.023072 - 0.625933j),
                   (-0.707107 - 0.707107j), (-0.077478 - 1.004407j),
                   (0.707107 - 0.707107j), (0.206764 + 0.272471j),
                   (-0.707107 + 0.707107j), (-0.437516 - 0.019997j),
                   (0.707107 - 0.707107j), (1.230677 - 0.200429j),
                   (0.707107 + 0.707107j), (-0.134688 + 0.512611j),
                   (-0.707107 - 0.707107j), (-0.912272 - 1.405505j),
                   (-0.707107 - 0.707107j), (-0.071130 + 0.484458j),
                   (0.707107 + 0.707107j), (1.089220 - 0.141840j),
                   (0.707107 - 0.707107j), (-0.158429 - 0.134444j),
                   (-0.707107 + 0.707107j), (-0.504679 + 0.658752j)]
        expected_out = [(0.707107 - 0.707107j), (-0.707107 + 0.707107j),
                        (0.681275 - 0.678024j), (0.708437 - 0.710186j),
                        (0.708243 + 0.698004j), (-0.702091 + 0.710557j),
                        (-0.707777 - 0.704814j), (-0.713004 + 0.687705j),
                        (0.677721 - 0.675160j), (-0.654095 + 0.664333j),
                        (-0.717689 - 0.678805j), (-0.702377 + 0.683944j),
                        (-0.700735 + 0.700393j), (0.708881 - 0.710097j),
                        (0.706574 - 0.704117j), (0.707870 + 0.704920j),
                        (0.710283 - 0.695180j), (-0.701361 - 0.708231j),
                        (-0.708886 - 0.707789j), (-0.705219 + 0.702190j),
                        (-0.710040 + 0.707688j), (0.704186 + 0.708516j),
                        (-0.687090 - 0.689550j), (0.669063 + 0.668199j),
                        (-0.682533 + 0.711108j), (-0.702993 + 0.703964j),
                        (0.707623 - 0.707774j), (0.707716 - 0.707895j),
                        (0.706515 - 0.707092j), (0.708028 + 0.702970j),
                        (-0.709456 + 0.696142j), (-0.705091 - 0.700181j),
                        (-0.704743 + 0.697602j), (-0.709404 + 0.708070j),
                        (0.701898 + 0.708210j), (0.706650 - 0.695511j),
                        (0.713244 + 0.685044j), (-0.704628 + 0.704278j),
                        (0.695317 - 0.697769j), (-0.675314 - 0.703334j),
                        (0.670826 - 0.720855j), (-0.670440 + 0.689668j),
                        (0.659403 - 0.678471j), (0.712694 + 0.697422j),
                        (-0.707198 - 0.707418j), (-0.689662 - 0.674422j),
                        (0.718887 + 0.680935j), (0.706682 - 0.706826j),
                        (-0.702286 + 0.699713j)]
        self._set_up_flowgraph(test_in,
                               sps=2,
                               loop_bw=0.01,
                               damping=1.0,
                               rolloff=0.2)
        self.tb.run()
        observed_out = self.sink.data()
        self.assertComplexTuplesAlmostEqual(observed_out,
                                            expected_out,
                                            places=5)

        test_in = [(0.707107 - 0.707107j), (0.910527 - 0.923097j),
                   (1.018227 - 1.045818j), (0.961219 - 0.989085j),
                   (0.707107 - 0.707107j), (0.291171 - 0.242172j),
                   (-0.178570 + 0.266991j), (-0.554434 + 0.632870j),
                   (-0.707107 + 0.707107j), (-0.580848 + 0.460234j),
                   (-0.218083 + 0.005721j), (0.260536 - 0.450659j),
                   (0.707107 - 0.707107j), (1.001820 - 0.654404j),
                   (1.087793 - 0.312618j), (0.972784 + 0.195647j),
                   (0.707107 + 0.707107j), (0.354434 + 1.076597j),
                   (-0.027769 + 1.210807j), (-0.394331 + 1.077470j),
                   (-0.707107 + 0.707107j), (-0.927677 + 0.193532j),
                   (-1.017250 - 0.315707j), (-0.945443 - 0.656052j),
                   (-0.707107 - 0.707107j), (-0.337430 - 0.452853j),
                   (0.085597 - 0.002083j), (0.461765 + 0.450111j),
                   (0.707107 + 0.707107j), (0.793370 + 0.656829j),
                   (0.762985 + 0.314700j), (0.706339 - 0.196229j),
                   (0.707107 - 0.707107j), (0.786866 - 1.068961j),
                   (0.884193 - 1.193092j), (0.887954 - 1.059160j),
                   (0.707107 - 0.707107j), (0.335462 - 0.226355j),
                   (-0.129064 + 0.256441j), (-0.525768 + 0.603365j),
                   (-0.707107 + 0.707107j), (-0.605307 + 0.534979j),
                   (-0.255822 + 0.149618j), (0.228536 - 0.314095j),
                   (0.707107 - 0.707107j), (1.055169 - 0.929245j),
                   (1.186617 - 0.961988j), (1.064016 - 0.859096j),
                   (0.707107 - 0.707107j), (0.201779 - 0.584314j),
                   (-0.307196 - 0.538742j), (-0.652006 - 0.585055j),
                   (-0.707107 - 0.707107j), (-0.452779 - 0.857916j),
                   (0.000727 - 0.960891j), (0.454240 - 0.929714j),
                   (0.707107 - 0.707107j), (0.647329 - 0.307278j),
                   (0.295907 + 0.166502j), (-0.214352 + 0.553877j),
                   (-0.707107 + 0.707107j), (-1.037092 + 0.563485j),
                   (-1.134065 + 0.179200j), (-1.004813 - 0.299397j),
                   (-0.707107 - 0.707107j), (-0.317920 - 0.933703j),
                   (0.086935 - 0.962299j), (0.444048 - 0.855062j),
                   (0.707107 - 0.707107j), (0.851399 - 0.598091j),
                   (0.878785 - 0.567425j), (0.815774 - 0.613265j),
                   (0.707107 - 0.707107j), (0.605117 - 0.807728j),
                   (0.557174 - 0.869298j), (0.592189 - 0.847156j),
                   (0.707107 - 0.707107j), (0.857611 - 0.438966j),
                   (0.962911 - 0.069190j), (0.931990 + 0.340293j),
                   (0.707107 + 0.707107j), (0.305289 + 0.953784j),
                   (-0.168760 + 1.033493j), (-0.555049 + 0.940894j),
                   (-0.707107 + 0.707107j), (-0.562890 + 0.381607j),
                   (-0.177596 + 0.012544j), (0.301797 - 0.362035j),
                   (0.707107 - 0.707107j), (0.925806 - 0.975383j),
                   (0.944531 - 1.102202j), (0.836211 - 1.021608j),
                   (0.707107 - 0.707107j), (0.636361 - 0.214245j),
                   (0.640557 + 0.309763j), (0.682040 + 0.668648j),
                   (0.707107 + 0.707107j), (0.689425 + 0.402197j),
                   (0.649466 - 0.102111j), (0.640744 - 0.551056j),
                   (0.707107 - 0.707107j), (0.838264 - 0.482534j),
                   (0.955468 + 0.004962j), (0.941118 + 0.490200j),
                   (0.707107 + 0.707107j), (0.264386 + 0.537719j),
                   (-0.253641 + 0.077263j), (-0.637439 - 0.424998j),
                   (-0.707107 - 0.707107j), (-0.420884 - 0.632573j),
                   (0.082577 - 0.247938j), (0.541263 + 0.266538j),
                   (0.707107 + 0.707107j), (0.485905 + 0.946739j),
                   (-0.002856 + 0.973780j), (-0.489977 + 0.861201j),
                   (-0.707107 + 0.707107j), (-0.537404 + 0.587920j),
                   (-0.077183 + 0.544988j), (0.424493 + 0.589409j),
                   (0.707107 + 0.707107j), (0.635253 + 0.854012j),
                   (0.252728 + 0.955721j), (-0.262695 + 0.925637j),
                   (-0.707107 + 0.707107j), (-0.950091 + 0.315538j),
                   (-0.976690 - 0.148603j), (-0.861021 - 0.534864j),
                   (-0.707107 - 0.707107j), (-0.596537 - 0.602481j),
                   (-0.566042 - 0.254836j), (-0.612291 + 0.227635j),
                   (-0.707107 + 0.707107j), (-0.809505 + 1.057836j),
                   (-0.871565 + 1.191079j), (-0.847813 + 1.067462j),
                   (-0.707107 + 0.707107j), (-0.444408 + 0.197631j),
                   (-0.084881 - 0.312739j), (0.321115 - 0.655485j),
                   (0.707107 - 0.707107j), (0.999328 - 0.451238j),
                   (1.124907 + 0.001034j), (1.030079 + 0.452632j),
                   (0.707107 + 0.707107j), (0.220936 + 0.654834j),
                   (-0.288863 + 0.312278j), (-0.645054 - 0.197561j),
                   (-0.707107 - 0.707107j), (-0.446682 - 1.069015j),
                   (0.020337 - 1.193798j), (0.477177 - 1.060028j),
                   (0.707107 - 0.707107j), (0.600887 - 0.225182j),
                   (0.208363 + 0.257964j), (-0.294860 + 0.604203j),
                   (-0.707107 + 0.707107j), (-0.905963 + 0.535085j),
                   (-0.897891 + 0.150689j), (-0.789929 - 0.312651j),
                   (-0.707107 - 0.707107j), (-0.712013 - 0.931662j),
                   (-0.774833 - 0.966100j), (-0.803313 - 0.862315j),
                   (-0.707107 - 0.707107j), (-0.452059 - 0.581223j),
                   (-0.074530 - 0.535361j), (0.342085 - 0.583749j),
                   (0.707107 - 0.707107j), (0.951819 - 0.855382j),
                   (1.037512 - 0.954038j), (0.952075 - 0.922109j),
                   (0.707107 - 0.707107j), (0.341908 - 0.321964j),
                   (-0.073232 + 0.139823j), (-0.449166 + 0.530340j),
                   (-0.707107 + 0.707107j), (-0.813515 + 0.598291j),
                   (-0.797354 + 0.237935j), (-0.735337 - 0.249981j),
                   (-0.707107 - 0.707107j), (-0.746137 - 1.005335j),
                   (-0.816135 - 1.087793j), (-0.831077 - 0.969268j),
                   (-0.707107 - 0.707107j), (-0.416685 - 0.364989j),
                   (-0.011241 + 0.007916j), (0.400431 + 0.376888j),
                   (0.707107 + 0.707107j), (0.850185 + 0.951770j),
                   (0.846864 + 1.055997j), (0.771488 + 0.976236j),
                   (0.707107 + 0.707107j), (0.698384 + 0.298701j),
                   (0.731878 - 0.149174j), (0.753469 - 0.516455j),
                   (0.707107 - 0.707107j), (0.570747 - 0.687252j),
                   (0.369543 - 0.497396j), (0.160117 - 0.232907j)]
        expected_out = [(0.707107 - 0.707107j), (-0.707107 + 0.707107j),
                        (0.703615 - 0.705102j), (0.708125 + 0.705146j),
                        (-0.707456 + 0.706294j), (-0.706708 - 0.706833j),
                        (0.706016 + 0.705964j), (0.707102 - 0.704105j),
                        (0.707174 - 0.707238j), (-0.707030 + 0.707063j),
                        (0.703664 - 0.704280j), (0.707120 - 0.707113j),
                        (-0.707100 - 0.707092j), (0.705711 - 0.708336j),
                        (-0.704377 + 0.706258j), (-0.708070 - 0.705787j),
                        (0.707070 - 0.707128j), (0.707290 - 0.706949j),
                        (0.706912 - 0.707344j), (0.707491 + 0.706481j),
                        (-0.707031 + 0.707223j), (0.705700 - 0.705909j),
                        (0.707121 - 0.707140j), (0.707103 + 0.707101j),
                        (0.706736 - 0.706236j), (0.708828 + 0.705511j),
                        (-0.706588 - 0.705004j), (0.705369 + 0.702490j),
                        (-0.705479 + 0.708262j), (0.704960 + 0.706213j),
                        (-0.704345 + 0.708465j), (-0.707385 - 0.706795j),
                        (-0.706684 + 0.704971j), (-0.707101 + 0.707095j),
                        (0.707104 - 0.707106j), (0.708401 + 0.706087j),
                        (-0.706856 - 0.705051j), (0.706279 - 0.708378j),
                        (-0.705459 + 0.706695j), (-0.707508 - 0.705198j),
                        (-0.707320 - 0.707450j), (0.706287 - 0.706830j),
                        (0.707334 - 0.707306j), (-0.706862 + 0.706939j),
                        (-0.707239 - 0.704967j), (-0.707169 - 0.707239j),
                        (0.706943 + 0.706931j), (0.707137 + 0.707232j),
                        (0.707129 - 0.707017j)]
        self._set_up_flowgraph(test_in,
                               sps=4,
                               loop_bw=0.005,
                               damping=0.707,
                               rolloff=0.35)
        self.tb.run()
        observed_out = self.sink.data()
        self.assertComplexTuplesAlmostEqual(observed_out,
                                            expected_out,
                                            places=5)

    def test_tag_propagation(self):
        """Test whether tags are properly placed at the nearest interpolant"""
        sps = 2
        nsyms = 30

        # Upsampled sequence of random unit-energy QPSK symbols
        x_syms = randQpskSyms(nsyms)
        x_up = upfirdn([1], x_syms, sps)

        # Open-loop flowgraph (with damping=0) where tags are placed after
        # every two symbols (i.e., 2*sps=4 samples) on the upsampled sequence
        # fed to the synchronizer
        self._set_up_flowgraph(x_up, sps, damping=0, tag_period=2 * sps)
        self.tb.run()

        # Output interpolants
        out_syms = self.sink.data()

        # The synchronizer should preserve the tags after every two
        # symbols/interpolants. The only exception is the first tag. Since the
        # synchronizer skips the first sps samples on startup, the first output
        # interpolant is equal to x_syms[1] (the second input symbol), not
        # x_syms[0]. Nevertheless, since the input tags always propagate
        # through the closest output interpolant, even though the symbol at
        # x_up[0] (or x_syms[0]) is not produced on the output, its tag still
        # propagates out through the first output interpolant out_syms[0]
        # (representing x_syms[1]). In other words, we expect a tag at index 0.
        # Next, the second output interpolant is equal to x_syms[2] (the third
        # input symbol), which is located on the upsampled input sequence at
        # x_up[4], i.e., the second tagged index on x_up. Hence, the second tag
        # comes already in out_syms[1] (after a single symbol interval). After
        # that, all tags should come spaced by two symbol intervals.
        self.assertAlmostEqual(out_syms[0], x_syms[1])
        self.assertAlmostEqual(out_syms[1], x_syms[2])
        expected_offsets = np.arange(1, (nsyms - 1), 2)
        expected_offsets = np.concatenate(([0], expected_offsets))
        observed_tags = self.tag_sink.current_tags()
        for i, expected_offset in enumerate(expected_offsets):
            self.assertEqual(observed_tags[i].offset, expected_offset)


if __name__ == '__main__':
    gr_unittest.run(qa_plsync_cc)
