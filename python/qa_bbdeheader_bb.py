#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright (c) 2019-2021 Blockstream
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
import struct

import numpy as np
from gnuradio import gr, gr_unittest
from gnuradio import blocks
from math import ceil, floor
try:
    from dvbs2rx import bbdeheader_bb, STANDARD_DVBS2, FECFRAME_NORMAL, C1_4
except ImportError:
    import os
    import sys
    dirname, filename = os.path.split(os.path.abspath(__file__))
    sys.path.append(os.path.join(dirname, "bindings"))
    from dvbs2rx import bbdeheader_bb, STANDARD_DVBS2, FECFRAME_NORMAL, C1_4

DVBS2_GEN_POLY = '111010101'  # x^8 + x^7 + x^6 + x^4 + x^2 +1
BBHEADER_NO_CRC_FMT = "!BBHHBH"  # BBHEADER format excluding the CRC field
BBHEADER_LEN = 10  # BBHEADER length in bytes
UPL_BYTES = 188  # User packet length in bytes
SYNC_BYTE = b'\x47'


def crc8(in_bytes, polynomial_bitstring=DVBS2_GEN_POLY):
    """Compute the CRC-8 of an input bytes sequence

    Args:
        in_bytes (bytes): Input sequence of bytes.
        polynomial_bitstring (str, optional): Generator polynomial as a bit
            string. Defaults to DVBS2_GEN_POLY.

    Returns:
        bytes: Resulting CRC-8 as a unit-length bytes object.
    """
    L = len(polynomial_bitstring)
    in_bits = np.unpackbits(np.frombuffer(in_bytes, dtype=np.uint8))
    gen_poly = np.array(list(map(int, polynomial_bitstring)), dtype=np.uint8)

    dividend = np.concatenate([in_bits, np.zeros(L - 1, dtype=np.uint8)])
    for i in range(len(in_bits)):
        if (dividend[i]):
            dividend[i:i + L] = np.bitwise_xor(dividend[i:i + L], gen_poly)

    remainder = dividend[len(in_bits):]
    return np.packbits(remainder).tobytes()


def gen_up():
    """Generate a random user packet (UP)

    Returns:
        bytes: Generated UP.
    """
    payload = np.random.bytes(UPL_BYTES - 4)
    return SYNC_BYTE + bytes(3) + payload


def gen_up_stream(n_ups):
    """Generate a stream of UPs

    Args:
        n_ups (int): Number of UPs to generate.

    Returns:
        bytes: Stream of UPs.
    """
    stream = bytearray()
    for i in range(n_ups):
        stream += gen_up()
    return bytes(stream)


def replace_sync_with_crc(original_stream):
    """Replace the sync byte on each UP with the CRC8 of the previous packet

    Args:
        original_stream (bytes): Original stream of UPs with the sync byte.

    Returns:
        (bytes): CRC8-modified UP stream.
    """
    stream = bytearray(original_stream)
    n_ups = int(len(stream) / UPL_BYTES)
    crc = None
    for i in range(n_ups):
        up_start = i * UPL_BYTES
        up_end = (i + 1) * UPL_BYTES
        if (crc is not None):
            stream[up_start] = int.from_bytes(crc, byteorder='big')
        crc = crc8(stream[up_start + 1:up_end])
    return bytes(stream)


def gen_bbheader(kbch, syncd, dfl=None):
    """Generate a BBHEADER

    Args:
        kbch (int): BCH message length.
        syncd (int): Distance in bits between the start of the DFL and the
            start of the first full UP.
        dfl (optional, int): DATAFIELD length in bits. When undefined, it is
            set to the maximum length equal to "kbch - 80".

    Returns:
        bytes: Generated BBHEADER.
    """
    ts_gs = 3  # MPEG-TS
    sis_mis = 1  # SIS
    ccm_acm = 1  # CCM
    issyi = 0  # not active
    npd = 0  # not active
    ro = 2  # rolloff=0.2
    matype_1 = ts_gs << 6 | sis_mis << 5 | ccm_acm << 4 | issyi << 3 \
        | npd << 2 | ro
    matype_2 = 0  # reserved if SIS/MIS=1 (i.e., in SIS mode)
    upl = UPL_BYTES * 8  # MPEG TS length in bits
    if (dfl is None):
        dfl = kbch - 80  # use the maximum DATAFIELD length

    # Generate the BBHEADER excluding the CRC8
    bbheader_no_crc = struct.pack(BBHEADER_NO_CRC_FMT, matype_1, matype_2, upl,
                                  dfl, int("47", 16), syncd)
    # Append the CRC8
    return bbheader_no_crc + crc8(bbheader_no_crc)


def gen_bbframe_stream(kbch, n_frames, up_stream):
    """Generate stream of unscrambled BBFRAMEs

    Args:
        kbch (int): BCH input (uncoded) message length in bits.
        n_frames (int): Number of BBFRAMEs to generate.
        up_stream (bytes): Stream of UPs to fill in the DATAFIELDs.

    Returns:
        bytes: Generated stream of BBFRAMEs.
    """

    dfl_bytes = int((kbch - 80) / 8)
    assert (len(up_stream) >= n_frames * dfl_bytes)

    # CRC-8 Encoder: replace the sync field of each UP with the CRC-8 of the
    # preceding packet.
    modified_up_stream = replace_sync_with_crc(up_stream)

    # BBFRAME stream:
    stream = bytearray()
    syncd = 0
    offset = 0
    for i in range(n_frames):
        # Fill the BBHEADER
        stream += gen_bbheader(kbch, syncd)
        # Fill the payload with UPs
        stream += modified_up_stream[offset:(offset + dfl_bytes)]
        # The last UP may not be complete:
        partial_up_len = (offset + dfl_bytes) % UPL_BYTES
        # The remaining part of the UP will come in the next BBFRAME
        remaining_up_len = UPL_BYTES - partial_up_len
        # Update the corresponding syncd info (in bits, not bytes)
        syncd = remaining_up_len * 8
        # Go to the next DATAFIELD
        offset += dfl_bytes
    return stream


class qa_bbdeheader_bb(gr_unittest.TestCase):
    def setUp(self):
        self.tb = gr.top_block()

    def _set_stream_len_params(self, kbch, n_bbframes):
        """Set up some common stream length parameters

        Args:
            kbch (int): BCH uncoded message (dataword) length.
            n_bbframes (int): Number of BBFRAMEs generated on the test.
        """
        assert (n_bbframes % 2 == 0), \
            "n_bbframes must be even to match bbdeheader output multiple"

        # Bytes per DATAFIELD
        self.dfl_bytes = dfl_bytes = int((kbch - 80) / 8)

        # Total number of full or partial UPs within n_bbframes.
        self.n_ups = int(ceil(n_bbframes * dfl_bytes / UPL_BYTES))

        # Total number of full UPs within n_bbframes.
        self.n_full_ups = int(floor(n_bbframes * dfl_bytes / UPL_BYTES))

    def _set_up_flowgraph(self, in_stream):
        """Set up the flowgraph

        Vector Source -> Packed-to-Unpacked -> BBDEHEADER -> Vector Sink

        Args:
            in_stream (bytes): Input stream to feed into the vector source.
        """
        src = blocks.vector_source_b(tuple(in_stream))
        unpack = blocks.packed_to_unpacked_bb(1, gr.GR_MSB_FIRST)
        bbdeheader = bbdeheader_bb(STANDARD_DVBS2, FECFRAME_NORMAL, C1_4)
        self.sink = blocks.vector_sink_b()
        self.tb.connect(src, unpack, bbdeheader, self.sink)

    def _assert_up_stream(self, up_stream, n_discarded_bbframes=0):
        """Assert the output contains the UPs from the non-discarded BBFRAMEs

        Args:
            up_stream (bytes): Original UP bytes sequence spanning the BBFRAME
                stream generated on the test case.
            n_discarded_bbframes (int, optional): Number of BBFRAMES discarded
                in the beginning of the BBFRAME stream. Defaults to 0.
        """
        # The expected output starts at the first full UP available after the
        # discarded BBFRAMEs.
        n_discarded_ups = int(
            ceil(n_discarded_bbframes * self.dfl_bytes / UPL_BYTES))
        i_start = n_discarded_ups * UPL_BYTES
        # It ends at the last full UP available on the last BBFRAME.
        i_end = self.n_full_ups * UPL_BYTES
        # Check
        expected_out = list(up_stream[i_start:i_end])
        observed_out = self.sink.data()
        self.assertListEqual(expected_out, observed_out)

    def test_successful_deframing(self):
        """Test successful deframing of consecutive BBFRAMEs
        """
        # Parameters
        kbch = 16008  # QPSK 1/4 with normal fecframe
        n_bbframes = 10  # Number of BBFRAMEs to generate
        self._set_stream_len_params(kbch, n_bbframes)

        # Generate the stream of UPs and the corresponding stream of BBFRAMEs
        up_stream = gen_up_stream(self.n_ups)
        bbframe_stream = gen_bbframe_stream(kbch, n_bbframes, up_stream)

        # Run the flowgraph and check results
        self._set_up_flowgraph(bbframe_stream)
        self.tb.run()
        self._assert_up_stream(up_stream)

    def test_bbheader_crc_error(self):
        """Test processing of a BBFRAME with an invalid BBHEADER CRC
        """
        # Parameters
        kbch = 16008  # QPSK 1/4 with normal fecframe
        n_bbframes = 2  # Number of BBFRAMEs to generate
        self._set_stream_len_params(kbch, n_bbframes)

        # Generate the stream of UPs and the corresponding stream of BBFRAMEs
        up_stream = gen_up_stream(self.n_ups)
        bbframe_stream = gen_bbframe_stream(kbch, n_bbframes, up_stream)

        # Corrupt the CRC checksum of the first BBHEADER
        corrupt_stream = bytearray(bbframe_stream)
        corrupt_stream[9] ^= 255

        # Run the flowgraph
        self._set_up_flowgraph(corrupt_stream)
        self.tb.run()

        # The first BBFRAME (with the CRC error) should be discarded
        self._assert_up_stream(up_stream, n_discarded_bbframes=1)


if __name__ == '__main__':
    gr_unittest.run(qa_bbdeheader_bb)
