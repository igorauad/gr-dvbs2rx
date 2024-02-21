#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright (c) 2019-2021 Igor Freire.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
try:
    from gnuradio.dvbs2rx import dvb_params
except ImportError:
    # When running tests before the module installation, assume the PYTHONPATH
    # includes the top-level build directory, which contains a "python/"
    # directory holding the built Python files to be installed.
    from python.dvbs2rx import dvb_params


def dvbs2_pls(constellation: str, code: str, frame_size: str, pilots: bool):
    """Get the physical layer signalling (PLS) for the given DVB-S2 parameters

    Args:
        constellation (str): DVB-S2 constellation.
        code (str): Code rate.
        frame_size (str): Frame size ("short" or "normal").
        pilots (bool): Whether pilots are included in the PLFRAMEs.

    Raises:
        ValueError: If the parameters are not valid for DVB-S2.

    Returns:
        int: PLS value as an integer from 0 to 127.
    """
    # Validation
    _ = dvb_params(
        "DVB-S2",
        frame_size.lower(),
        code,
        constellation=constellation.upper(),
        pilots=pilots,
    )

    dvbs2_modcods = {
        "qpsk1/4": 1,
        "qpsk1/3": 2,
        "qpsk2/5": 3,
        "qpsk1/2": 4,
        "qpsk3/5": 5,
        "qpsk2/3": 6,
        "qpsk3/4": 7,
        "qpsk4/5": 8,
        "qpsk5/6": 9,
        "qpsk8/9": 10,
        "qpsk9/10": 11,
        "8psk3/5": 12,
        "8psk2/3": 13,
        "8psk3/4": 14,
        "8psk5/6": 15,
        "8psk8/9": 16,
        "8psk9/10": 17,
        "16apsk2/3": 18,
        "16apsk3/4": 19,
        "16apsk4/5": 20,
        "16apsk5/6": 21,
        "16apsk8/9": 22,
        "16apsk9/10": 23,
        "32apsk3/4": 24,
        "32apsk4/5": 25,
        "32apsk5/6": 26,
        "32apsk8/9": 27,
        "32apsk9/10": 28,
    }

    modcod = dvbs2_modcods[constellation.lower() + code]
    short_frame = frame_size == "short"
    pls = (modcod << 2) | (short_frame << 1) | pilots

    if pls < 0 or pls >= 128:
        raise ValueError("Unexpected PLS value: {}".format(pls))

    return pls


def pls_filter(*pls_tuple):
    """Convert target PLSs into a u64 bitmask pair used by the PL sync block

    Args:
        pls_tuple (int): Tuple with the target PLSs.

    Raises:
        ValueError: If the input PLS is not in the valid DVB-S2 range.

    Returns:
        tuple: Tuple with the lower 64 bits and the upper 64 bits of the
        filter bitmask, respectively.
    """
    u64_filter_lo = 0
    u64_filter_hi = 0

    for pls in pls_tuple:
        if pls < 0 or pls >= 128:
            raise ValueError("Unexpected PLS value: {}".format(pls))
        if pls >= 64:
            u64_filter_hi |= 1 << (pls - 64)
        else:
            u64_filter_lo |= 1 << pls

    return (u64_filter_lo, u64_filter_hi)


def pl_info(constellation: str, code: str, frame_size: str, pilots: str):
    """Get the physical layer signalling (PLS) information

    Args:
        constellation (str): DVB-S2 constellation.
        code (str): Code rate.
        frame_size (str): Frame size ("short" or "normal").
        pilots (bool): Whether pilots are included in the PLFRAMEs.

    Raises:
        ValueError: If the parameters are not valid for DVB-S2.

    Returns:
        dict: Dictionary with the modulation efficiency in bits/sec/Hz, the
        number of slots, pilot blocks, PLFRAME length, and XFECFRAME length.
    """

    plsc = dvbs2_pls(constellation, code, frame_size, pilots)
    modcod = plsc >> 2
    short_fecframe = plsc & 0x2
    has_pilots = plsc & 0x1
    dummy_frame = modcod == 0
    has_pilots &= not dummy_frame  # a dummy frame cannot have pilots

    # Number of bits per constellation symbol and PLFRAME slots
    if modcod >= 1 and modcod <= 11:
        n_mod = 2
        n_slots = 360
    elif modcod >= 12 and modcod <= 17:
        n_mod = 3
        n_slots = 240
    elif modcod >= 18 and modcod <= 23:
        n_mod = 4
        n_slots = 180
    elif modcod >= 24 and modcod <= 28:
        n_mod = 5
        n_slots = 144
    else:
        n_mod = 0
        n_slots = 36  # dummy frame

    # For short FECFRAMEs, S is 4 times lower
    if short_fecframe and not dummy_frame:
        n_slots >>= 2

    # Number of pilot blocks
    n_pilots = ((n_slots - 1) >> 4) if has_pilots else 0

    # PLFRAME length (header + data + pilots)
    plframe_len = ((n_slots + 1) * 90) + (36 * n_pilots)

    # XFECFRAME length
    xfecframe_len = n_slots * 90

    return {
        "n_mod": n_mod,
        "n_slots": n_slots,
        "n_pilots": n_pilots,
        "plframe_len": plframe_len,
        "xfecframe_len": xfecframe_len,
    }
