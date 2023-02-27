# Supported Operation Modes and Limitations

- The current implementation supports *constant coding and modulation* (CCM) only. It does not support *adaptive or variable coding and modulation* (ACM/VCM) yet.

  - The only block that supports ACM/VCM is the physical layer (PL) synchronization block implementing the low portion of the PL. However, the remaining blocks of the pipeline (e.g., BCH and LDPC FEC) still lack ACM/VCM support.

- CCM mode can operate with a single input stream (SIS) or multiple input streams (MIS).

- Only QPSK and 8PSK constellations are currently supported. 16APSK and 32APSK constellations are not supported yet.

- The SIMD-accelerated LDPC implementation supports the AVX2, SSE4.1, and NEON instruction sets. You can check whether these instruction sets are available in your machine by running:

  ```
  lscpu | grep -e avx2 -e sse4_1 -e neon
  ```

- SDR compatibility:
  | Application | USRP               | bladeRF            | ADALM-PLUTO (PlutoSDR) | RTL-SDR            |
  | ----------- | ------------------ | ------------------ | ---------------------- | ------------------ |
  | `dvbs2-tx`  | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark:     |                    |
  | `dvbs2-rx`  | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark:     | :heavy_check_mark: |

> Please feel free to get in touch if you would like to use another SDR device/board. Adding support should be straightforward if the device already has GNU Radio support. Pull requests are also welcome.

> NOTE: The current implementations for the bladeRF and PlutoSDR are subject to experimental validation.

- The Tx application takes an MPEG transport stream on the input, and the RX application likewise outputs a transport stream. None of them offer any mechanism to process the MPEG transport stream itself. Processing of MPEG TS is beyond the scope of this project.

  - A handy tool to use in conjunction with this project's transmitter/receiver is the `tsp` tool provided by the [TSDuck](https://tsduck.io) toolkit. Refer to more info in the [usage section](usage.md).

- The current receiver implementation supports signal-to-noise ratios (SNRs) as low as 2 dB in pilot mode. Lower SNRs are yet to be supported.
  - The current performance bottleneck lies in the PL synchronization algorithms.
  - For example, the following configuration works (see the [usage guide](usage.md)):

```
dvbs2-tx \
  --source file \
  --in-file data/example.ts \
  --in-repeat \
  --modcod qpsk1/4 \
  --pilots \
  --snr 2 \
  --freq-offset 1e5 | \
dvbs2-rx \
  --sink file \
  --out-file /dev/null \
  --modcod qpsk1/4 \
  --pilots on
```

- The PL synchronization block currently has limited support for PLFRAMES without pilot symbols. The full support for pilotless operation is still on the to-do list.

- Aside from simulation, the current implementation has been tested primarily with the [Blockstream Satellite](https://blockstream.github.io/satellite/) system. More specifically, it has been tested using the hardware recommended for Blockstream Satellite receivers and the system's DVB-S2 configuration summarized below. Other hardware parts and DVB-S2 parameters are yet to be tested with live carriers.
  - SDR interface: RTL-SDR with a temperature-controlled crystal oscillator (TCXO) (see the [recommendation](https://blockstream.github.io/satellite/doc/hardware.html#software-defined-radio-sdr-setup)).
  - Low-noise block downconverter (LNB): phase-locked loop (PLL) design with +-300 kHz local oscillator stability or better (see the [recommendation](https://blockstream.github.io/satellite/doc/hardware.html#lnb)).
  - DVB-S2 signal configured with:
    - QPSK3/5 MODCOD.
    - CCM/SIS mode.
    - Pilot symbols enabled.
    - 1 Mbaud.
    - 0.2 roll-off factor.

---
Prev: [Index](../README.md)  -  Next: [Installation](installation.md)