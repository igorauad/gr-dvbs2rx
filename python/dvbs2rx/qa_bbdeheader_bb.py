#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (c) 2019-2021 Igor Freire.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
import struct
from math import ceil, floor

import numpy as np
from gnuradio import blocks, gr, gr_unittest

try:
    from gnuradio.dvbs2rx import (C1_4, FECFRAME_NORMAL, STANDARD_DVBS2,
                                  bbdeheader_bb)
except ImportError:
    from python.dvbs2rx import (C1_4, FECFRAME_NORMAL, STANDARD_DVBS2,
                                bbdeheader_bb)

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


def gen_bbframe_stream(kbch, n_frames, up_stream, syncd=0):
    """Generate stream of unscrambled BBFRAMEs

    Args:
        kbch (int): BCH input (uncoded) message length in bits.
        n_frames (int): Number of BBFRAMEs to generate.
        up_stream (bytes): Stream of UPs to fill in the DATAFIELDs.
        syncd (int): Starting SYNCD value.

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

        Vector Source -> BBDEHEADER -> Vector Sink

        Args:
            in_stream (bytes): Input stream to feed into the vector source.
        """
        src = blocks.vector_source_b(tuple(in_stream))
        bbdeheader = bbdeheader_bb(STANDARD_DVBS2, FECFRAME_NORMAL, C1_4)
        self.sink = blocks.vector_sink_b()
        self.tb.connect(src, bbdeheader, self.sink)

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

    def test_undetected_dfl_corruption(self):
        """Test undetectable errors on the DFL field of the BBHEADER
        """
        # Parameters
        kbch = 16008  # QPSK 1/4 with normal fecframe
        n_bbframes = 2  # Number of BBFRAMEs to generate
        self._set_stream_len_params(kbch, n_bbframes)

        # Generate the stream of UPs and the corresponding stream of BBFRAMEs
        up_stream = gen_up_stream(self.n_ups)
        bbframe_stream = gen_bbframe_stream(kbch, n_bbframes, up_stream)

        # Corrupt the DFL field of the first BBHEADER with an error that is a
        # multiple of the CRC generator polynomial (i.e., undetectable), and
        # which results in a DFL exceeding "kbch - 80"
        gen_poly = int(DVBS2_GEN_POLY, 2)
        error = gen_poly.to_bytes(2, byteorder='big')
        dfl_field_offset = 4  # in bytes from the start of the BBHEADER
        corrupt_stream = bytearray(bbframe_stream)
        for j in range(len(error)):
            corrupt_stream[dfl_field_offset + j] ^= error[j]
        resulting_dfl = int.from_bytes(
            corrupt_stream[dfl_field_offset:dfl_field_offset + 2],
            byteorder='big')
        assert (resulting_dfl > kbch - 80)

        # Run the flowgraph
        self._set_up_flowgraph(corrupt_stream)
        self.tb.run()

        # The first BBFRAME (with the corrupt DFL) should be discarded
        self._assert_up_stream(up_stream, n_discarded_bbframes=1)

        # Now, corrupt the DFL field of the first BBHEADER with an undetectable
        # error yielding a DFL that is not a multiple of 8
        error = (gen_poly << 2).to_bytes(2, byteorder='big')
        dfl_field_offset = 4  # in bytes from the start of the BBHEADER
        corrupt_stream = bytearray(bbframe_stream)
        for j in range(len(error)):
            corrupt_stream[dfl_field_offset + j] ^= error[j]
        resulting_dfl = int.from_bytes(
            corrupt_stream[dfl_field_offset:dfl_field_offset + 2],
            byteorder='big')
        assert (resulting_dfl % 8 != 0)

        # Run the flowgraph
        self._set_up_flowgraph(corrupt_stream)
        self.tb.run()

        # Again, the first BBFRAME (with the corrupt DFL) should be discarded
        self._assert_up_stream(up_stream, n_discarded_bbframes=1)

    def test_undetected_syncd_corruption(self):
        """Test undetectable error on the SYNCD field of the BBHEADER
        """
        # Parameters
        kbch = 16008  # QPSK 1/4 with normal fecframe
        n_bbframes = 2  # Number of BBFRAMEs to generate
        self._set_stream_len_params(kbch, n_bbframes)

        # Generate the stream of UPs and the corresponding stream of BBFRAMEs
        up_stream = gen_up_stream(self.n_ups)
        bbframe_stream = gen_bbframe_stream(kbch, n_bbframes, up_stream)

        # Corrupt the SYNCD field of the first BBHEADER with an error that is a
        # multiple of the CRC generator polynomial (i.e., undetectable), and
        # which sets the SYNCD to an invalid value exceeding the DFL.
        gen_poly = int(DVBS2_GEN_POLY, 2)
        error = (gen_poly << 7).to_bytes(2, byteorder='big')
        syncd_field_offset = 7  # in bytes from the start of the BBHEADER
        corrupt_stream = bytearray(bbframe_stream)
        for j in range(len(error)):
            corrupt_stream[syncd_field_offset + j] ^= error[j]
        resulting_syncd = int.from_bytes(
            corrupt_stream[syncd_field_offset:syncd_field_offset + 2],
            byteorder='big')
        assert (resulting_syncd > (kbch - 80))

        # Run the flowgraph
        self._set_up_flowgraph(corrupt_stream)
        self.tb.run()

        # The first BBFRAME (with the corrupt SYNCD) should be discarded
        self._assert_up_stream(up_stream, n_discarded_bbframes=1)

    def test_padded_dfl(self):
        """Test processing of a zero-padded DATAFIELD
        """
        # Parameters
        kbch = 16008  # QPSK 1/4 with normal fecframe
        n_bbframes = 4  # Number of BBFRAMEs to generate
        self._set_stream_len_params(kbch, n_bbframes)

        # Configure the DATAFIELD length to hold an integer number of UPs and
        # no partial UP. In this case, the DATAFIELD length is not equal to
        # "kbch - 80", and the DATAFIELD is zero-padded in the end.
        max_dfl_bytes = int((kbch - 80) / 8)
        n_ups_per_df = int(floor(max_dfl_bytes / UPL_BYTES))
        dfl_bytes = n_ups_per_df * UPL_BYTES
        pad_bytes = max_dfl_bytes - dfl_bytes
        assert (pad_bytes > 0)
        assert ((dfl_bytes + pad_bytes) * 8 == kbch - 80)

        # Generate the stream of UPs
        n_ups = n_bbframes * n_ups_per_df
        up_stream = gen_up_stream(n_ups)

        # Generate the CRC-8 encoded UP stream
        modified_up_stream = replace_sync_with_crc(up_stream)

        # Generate the BBFRAME stream with zero-padded datafields.
        #
        # NOTE: since the DATAFIELD is constrained to contain an integer number
        # of UPs, each DATAFIELD starts aligned with a UP. Hence, the SYNCD is
        # always zero in this case.
        bbframe_stream = bytearray()
        syncd = 0
        offset = 0
        for i in range(n_bbframes):
            # Fill the BBHEADER
            bbframe_stream += gen_bbheader(kbch, syncd, dfl=dfl_bytes * 8)
            # Fill the payload with UPs
            bbframe_stream += modified_up_stream[offset:(offset + dfl_bytes)]
            # Zero pad so that BBFRAME+DATAFIELD+PADDING has a length of kbch
            bbframe_stream += bytes(pad_bytes)
            # Go to the next DATAFIELD
            offset += dfl_bytes

        # Run the flowgraph
        self._set_up_flowgraph(bbframe_stream)
        self.tb.run()

        # All UPs but the last should be processed despite the zero-padding.
        # The last UP cannot be processed because its CRC only comes in the
        # next BBFRAME.
        expected_out = list(up_stream[:-188])
        observed_out = self.sink.data()
        self.assertListEqual(expected_out, observed_out)

    def test_non_byte_aligned_syncd(self):
        """Test processing of a BBFRAME carrying a byte-misaligned SYNCD"""
        # Parameters
        kbch = 16008  # QPSK 1/4 with normal fecframe
        n_bbframes = 10  # Number of BBFRAMEs to generate
        self._set_stream_len_params(kbch, n_bbframes)

        # Generate the stream of UPs
        up_stream = gen_up_stream(self.n_ups)

        # Start the first BBFRAME with an unsupported non-byte-aligned SYNCD
        # value, namely a SYNCD that is not a multiple of 8.
        bbframe_stream = gen_bbframe_stream(kbch,
                                            n_bbframes,
                                            up_stream,
                                            syncd=5)

        # Run the flowgraph
        self._set_up_flowgraph(bbframe_stream)
        self.tb.run()

        # The first BBFRAME (with the unsupported SYNCD) should be discarded
        self._assert_up_stream(up_stream, n_discarded_bbframes=1)

    def test_non_consecutive_bbframes(self):
        """Test processing of non-consecutive BBFRAMEs"""
        # Parameters
        kbch = 16008  # QPSK 1/4 with normal fecframe
        n_bbframes = 10  # Number of BBFRAMEs to generate
        self._set_stream_len_params(kbch, n_bbframes)

        # Generate the stream of UPs and the corresponding stream of BBFRAMEs
        up_stream = gen_up_stream(self.n_ups)
        bbframe_stream = gen_bbframe_stream(kbch, n_bbframes, up_stream)

        # Confirm the length
        bbframe_bytes = kbch // 8
        assert len(bbframe_stream) == n_bbframes * bbframe_bytes

        # Drop a BBFRAME in the middle of the stream
        i_drop = 5  # which BBFRAME to drop
        bbframe_stream = bbframe_stream[:i_drop * bbframe_bytes] + \
            bbframe_stream[(i_drop + 1) * bbframe_bytes:]
        assert len(bbframe_stream) == (n_bbframes - 1) * bbframe_bytes

        # Expected UPs (excluding those dropped in the middle)
        n_ups_pre_drop = int(floor(i_drop * self.dfl_bytes / UPL_BYTES))
        n_partial_ts_bytes_remaining = (i_drop * bbframe_bytes) - \
            (n_ups_pre_drop * UPL_BYTES)
        n_bytes_dropped = bbframe_bytes + n_partial_ts_bytes_remaining
        n_ups_dropped = int(ceil(n_bytes_dropped / UPL_BYTES))
        n_ups_post_drop = self.n_full_ups - n_ups_dropped - n_ups_pre_drop
        ups_pre_drop = up_stream[:n_ups_pre_drop * UPL_BYTES]
        i_start_post_drop = (n_ups_pre_drop + n_ups_dropped) * UPL_BYTES
        i_end_post_drop = i_start_post_drop + n_ups_post_drop * UPL_BYTES
        ups_post_drop = up_stream[i_start_post_drop:i_end_post_drop]
        expected_out = list(ups_pre_drop + ups_post_drop)

        # Run the flowgraph and check results
        self._set_up_flowgraph(bbframe_stream)
        self.tb.run()
        observed_out = self.sink.data()
        self.assertListEqual(expected_out, observed_out)


if __name__ == '__main__':
    gr_unittest.run(qa_bbdeheader_bb)
