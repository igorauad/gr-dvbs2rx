# Supported Operation Modes and Limitations

- The current implementation supports *constant coding and modulation* (CCM)
  mode only. It does not provide support for *adaptive or variable coding and
  modulation* (ACM/VCM) yet.

  - The only block that supports ACM/VCM is the PL Sync block implementing the
    low portion of the physical layer. However, the remaining blocks of the
    pipeline (e.g., BCH and LDPC FEC) are still missing ACM/VCM support.

- CCM mode can operate with a single input stream (SIS) or multiple input
  streams (MIS).

- Only QPSK and 8PSK constellations are currently supported. 16APSK and 32APSK
  constellations are not supported yet.

- The SIMD-accelerated BCH and LDPC implementations support the AVX2, SSE4.1,
  and NEON instruction sets. You can check whether these instruction sets are
  available in your machine by running:

  ```
  lscpu | grep -e avx2 -e sse4_1 -e neon
  ```

- SDR compatibility:
    - `dvbs2-tx` can currently transmit using USRP devices only.
    - `dvbs2-rx` can currently receive with RTL-SDR and USRP devices.
    - Please feel free to get in touch if you would like to use another SDR
      device/board. Adding support should be straightforward if the device
      already has GNU Radio support. Pull requests are also welcome.

- The Tx application takes an MPEG transport stream on the input, and the RX
  application likewise outputs an MPEG transport stream. None of them offer any
  mechanism to process the MPEG transport stream itself. Processing of MPEG TS
  is beyond the scope of this project.

  - A handy tool to use in conjunction with this project's transmitter/receiver
    is the `tsp` tool provided by the [TSDuck](https://tsduck.io) toolkit. Refer
    to more info in the [running section](docs/running.md).

---
Prev: [Index](../README.md)  -  Next: [Installation](installation.md)