from textwrap import fill

try:
    import gnuradio.dvbs2rx as dvbs2rx  # noqa: F401
except ImportError:
    # When running tests before the module installation, assume the PYTHONPATH
    # includes the top-level build directory, which contains a "python/"
    # directory holding the built Python files to be installed.
    import python.dvbs2rx as dvbs2rx  # noqa: F401

from . import defs


def _adjust_case(standard, frame_size, constellation):
    """Adjust string paramters to the adopted upper/lower case convention"""
    if standard is not None:
        standard = standard.upper()
    if frame_size is not None:
        frame_size = frame_size.lower()
    if constellation is not None:
        constellation = constellation.upper()
    return standard, frame_size, constellation


def validate(standard,
             frame_size,
             code,
             constellation=None,
             rolloff=None,
             pilots=None,
             vl_snr=False):
    """Validate DVB-S2/S2X/T2 parameters

    Args:
        standard: DVB standard (DVB-S2, DVB-S2X, DVB-T2).
        frame_size: Frame size (normal, medium, short).
        code: LDPC code rate identifier (e.g., 1/4, 1/3, etc.).
        constellation: Constellation (QPSK, 8PSK, 16QAM, etc.).
        rolloff: Roll-off factor.
        pilots: Whether physical layer pilots are enabled.
        vl_snr: DVB-S2X very-low (VL) SNR mode.

    Note:
        This function assumes the constellation, rolloff, and pilots parameters
        can be empty, while the other parameters are always defined.

    Returns:
        Boolean indicating whether the set of parameters is valid.

    """
    standard, frame_size, constellation = _adjust_case(standard, frame_size,
                                                       constellation)

    if (standard not in defs.standards):
        print("Invalid DVB standard \"{}\" - choose from {}".format(
            frame_size, defs.standards.keys()))
        return False

    if (frame_size not in defs.frame_sizes):
        print(
            fill("Invalid frame size \"{}\" - choose from {}".format(
                frame_size, list(defs.frame_sizes.keys()))))
        return False

    if (constellation is not None):
        filtered_constellations = [
            key for key, val in defs.constellations.items()
            if standard in val['standard']
        ]
        if (constellation not in filtered_constellations):
            print("\"{}\" not a supported {} constellation".format(
                constellation, standard))
            print(fill("Choose from: {}".format(filtered_constellations)))
            return False

    filtered_codes = [
        key for key, val in defs.ldpc_codes.items()
        if (standard in val['standard'] and frame_size in val['frame'])
    ]
    # TODO: check also if the code is supported for the chosen constellation

    if (code not in filtered_codes):
        print("Code rate \"{}\" not supported in {} with {} frame size".format(
            code, standard, frame_size))
        print(fill("Choose from: {}".format(filtered_codes)))
        return False

    if (constellation is not None):
        modcod_str = constellation.lower() + code
        if (standard == "DVB-S2" and modcod_str not in defs.dvbs2_modcods):
            print("Invalid DVB-S2 MODCOD {}".format(modcod_str))
            return False

    if (vl_snr and standard != "DVB-S2X"):
        print("VL-SNR mode is only supported by the DVB-S2X standard")
        return False

    if (vl_snr and ('vl-snr' not in defs.ldpc_codes[code] or
                    (not defs.ldpc_codes[code]['vl-snr']))):
        print("Code rate {} does not support VL-SNR DVB-S2X mode".format(code))
        return False

    if (rolloff is not None):
        filtered_rolloffs = [
            key for key, val in defs.rolloffs.items()
            if standard in val['standard']
        ]
        if (rolloff not in filtered_rolloffs):
            print("{} is not a supported {} roll-off factor".format(
                rolloff, standard))
            return False

    if (pilots is not None):
        if (not isinstance(pilots, bool)):
            print("The \"pilots\" flag must be a Boolean")
            return False

    return True


def translate(standard,
              frame_size,
              code,
              constellation=None,
              rolloff=None,
              pilots=None,
              vl_snr=False):
    """Translate DVB-S2/S2X/T2 parameters to the pybind11 C++ definitions

    Args:
        standard: DVB standard (DVB-S2, DVB-S2X, DVB-T2).
        frame_size: Frame size (normal, medium, short).
        code: LDPC code rate identifier (e.g., 1/4, 1/3, etc.).
        constellation: Constellation (QPSK, 8PSK, 16QAM, etc.).
        rolloff: Roll-off factor.
        pilots: Whether physical layer pilots are enabled.
        vl_snr: DVB-S2X very-low (VL) SNR mode.

    Note:
        Most blocks take the standard, frame size, and LDPC code as
        parameters. In contrast, the constellation, roll-off, and pilots
        parameters are only taken by a few blocks. Such optional parameters are
        only translated and included in the output when the corresponding
        input argument is provided.

    Returns:
        Tuple with the corresponding pybind11 C++ definitions.

    """
    standard, frame_size, constellation = _adjust_case(standard, frame_size,
                                                       constellation)
    valid = validate(standard, frame_size, code, constellation, rolloff,
                     pilots)
    if (not valid):
        raise ValueError("Invalid {} parameters".format(standard))

    t_standard = eval("dvbs2rx." + defs.standards[standard])
    t_frame_size = eval("dvbs2rx." + defs.frame_sizes[frame_size])

    # The LDPC code rate definition may depend on the chosen frame size and
    # VL-SNR mode
    code_def = defs.ldpc_codes[code]['def']
    if (isinstance(code_def, list)):
        frame_sizes = defs.ldpc_codes[code]['frame']
        vl_snr_modes = defs.ldpc_codes[code]['vl-snr']

        found = False
        for idx, d in enumerate(code_def):
            if (frame_size == frame_sizes[idx]
                    and vl_snr == vl_snr_modes[idx]):
                found = True
                break
        assert (found)  # It should be found here (_validate guarantees that)
        t_code = eval("dvbs2rx." + defs.ldpc_codes[code]['def'][idx])
    else:
        t_code = eval("dvbs2rx." + defs.ldpc_codes[code]['def'])

    # Resulting tuple with definitions
    res = (t_standard, t_frame_size, t_code)

    if constellation is not None:
        t_constellation = eval("dvbs2rx." +
                               defs.constellations[constellation]['def'])
        res += (t_constellation, )

    if rolloff is not None:
        t_rolloff = eval("dvbs2rx." + defs.rolloffs[rolloff]['def'])
        res += (t_rolloff, )

    if pilots is not None:
        t_pilots = eval("dvbs2rx." + defs.pilots[bool(pilots)])
        res += (t_pilots, )

    return res


def dvbs2_pls(constellation, code, frame_size, pilots):
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
    valid = validate('DVB-S2',
                     frame_size.lower(),
                     code,
                     constellation=constellation.upper(),
                     rolloff=None,
                     pilots=pilots,
                     vl_snr=False)

    if (not valid):
        raise ValueError("Invalid DVB-S2 parameters")

    modcod = defs.dvbs2_modcods[constellation.lower() + code]
    short_frame = frame_size == 'short'
    pls = (modcod << 2) | (short_frame << 1) | pilots

    if (pls < 0 or pls >= 128):
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
        if (pls < 0 or pls >= 128):
            raise ValueError("Unexpected PLS value: {}".format(pls))
        if (pls >= 64):
            u64_filter_hi |= 1 << (pls - 64)
        else:
            u64_filter_lo |= 1 << pls

    return (u64_filter_lo, u64_filter_hi)


def pl_info(constellation, code, frame_size, pilots):
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
    if (modcod >= 1 and modcod <= 11):
        n_mod = 2
        n_slots = 360
    elif (modcod >= 12 and modcod <= 17):
        n_mod = 3
        n_slots = 240
    elif (modcod >= 18 and modcod <= 23):
        n_mod = 4
        n_slots = 180
    elif (modcod >= 24 and modcod <= 28):
        n_mod = 5
        n_slots = 144
    else:
        n_mod = 0
        n_slots = 36  # dummy frame

    # For short FECFRAMEs, S is 4 times lower
    if (short_fecframe and not dummy_frame):
        n_slots >>= 2

    # Number of pilot blocks
    n_pilots = ((n_slots - 1) >> 4) if has_pilots else 0

    # PLFRAME length (header + data + pilots)
    plframe_len = ((n_slots + 1) * 90) + (36 * n_pilots)

    # XFECFRAME length
    xfecframe_len = n_slots * 90

    return {
        'n_mod': n_mod,
        'n_slots': n_slots,
        'n_pilots': n_pilots,
        'plframe_len': plframe_len,
        'xfecframe_len': xfecframe_len
    }
