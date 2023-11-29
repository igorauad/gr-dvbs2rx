# Changelog

- [Changelog](#changelog)
  - [1.4.0](#131)
  - [1.3.0](#130)

## 1.4.0

Release Date: 2023-11-29

### Added

- Standalone block for XFECFRAME demapping.
- Vectorized implementation of QPSK demapping and deinterleaving operations.
- Count of detected SOFs and BBFRAMEs (processed and dropped) monitored by dvbs2-rx.
- Optional debug logging of the BBHEADER's UPL, DFL, SYNC, and SYNCD fields.
- Optimized computation of BCH error-location numbers for first-order and second-order error-location polynomials.

### Changed

- BCH syndrome computation using the remainder of a single division by the generator polynomial instead of t divisions by minimal polynomials.
- Bias used on IQ format conversion from cf32 to cu8 and vice versa.

### Fixed

- LDPC decoder's relative input/output rate.
- SIGINT/SIGTERM handlers of the dvbs2-tx, dvbs2-rx, and dvbs2-rec applications.

## 1.3.0

Release Date: 2023-11-01

### Added

- New BCH decoder implementation performing at least 2.5x faster than the previous one.
- BCH decoder benchmarking application for comparison between the original gr-dvbs2rx BCH decoder (based on [aicodix/code](https://github.com/aicodix/code/)), the AFF3CT [DVB-S2 BCH decoder](https://github.com/aff3ct/dvbs2), and the new BCH decoder implementation.
- `dvbs2-rec` application for recording IQ samples from a DVB-S2 signal and storing them in Signal Metadata Format (SigMF).
- Proposal of [SigMF extension for DVB-S2](docs/dvbs2.sigmf-ext.md).

### Changed

- Replaced the unpacked bit stream interface of the BBFRAME processing (descrambler and deheader) and LDPC decoder blocks with a bit-packed stream interface for better performance.

### Fixed

- Compilation on AppleClang.
- Runtime library version parsing on macOS.