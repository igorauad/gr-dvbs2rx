# gr-dvbs2rx

[![Formatting](https://github.com/igorauad/gr-dvbs2rx/actions/workflows/formatting.yml/badge.svg)](https://github.com/igorauad/gr-dvbs2rx/actions/workflows/formatting.yml)
[![Tests](https://github.com/igorauad/gr-dvbs2rx/actions/workflows/test.yml/badge.svg)](https://github.com/igorauad/gr-dvbs2rx/actions/workflows/test.yml)

## Overview

`gr-dvbs2rx` is a GNU Radio [out-of-tree (OOT) module](https://wiki.gnuradio.org/index.php/OutOfTreeModules) with full DVB-S2 transmitter and receiver implementations for software-defined radio (SDR).

For the receiver side, this project includes a shared library with the essential signal processing blocks to process input IQ samples and recover an MPEG transport stream (TS) on the output. The processing blocks include:
- The physical layer (PL) synchronization stages (frame timing, symbol timing, carrier frequency, and phase recovery).
- Forward error correction (FEC) blocks.
- Baseband frame (BBFRAME) processing.

These blocks are wired together in the `dvbs2-rx` application and within the example GNU Radio flowgraphs.

Additionally, the project provides the `dvbs2-tx` transmitter application. While this OOT module focuses on the Rx signal processing blocks, the Tx blocks are taken directly from the [`gr-dtv`](https://github.com/gnuradio/gnuradio/tree/master/gr-dtv) in-tree GNU Radio module for digital TV. Like the Rx application, the `dvbs2-tx` application wires the Tx processing blocks together while offering a flexible user interface.

The Tx and Rx applications are highly customizable through the command-line options detailed in the [usage section](docs/usage.md). For instance, they include customization of the input/output interface and parameters like MODCOD, frame size, PL pilots, and roll-off factor. Also, they support using SDR devices (RTL-SDR, USRP, PlutoSDR, and BladeRF) as input/output interfaces.

This repository is a fork of drmpeg's [gr-dvbs2rx](http://github.com/drmpeg/gr-dvbs2rx) original project, including the SIMD-accelerated LDPC decoder from [xdsopl/LDPC](https://github.com/xdsopl/LDPC). Among a variety of improvements, this fork adds:
- The physical layer blocks required for a fully-fledged receiver.
- The flexible `dvbs2-tx` and `dvbs2-rx` command-line applications.
- A new BCH decoder implementation that is approximately 2.5x faster than the original (introduced in version 1.3.0)

## License

The project is licensed under GPLv3 and was developed primarily for non-commercial research, experimentation, and education. Please feel free to contribute or get in touch to request features or report any problems.

## User Guide

- [Supported operation modes and limitations.](docs/support.md)
- [Installation instructions.](docs/installation.md)
- [Usage instructions.](docs/usage.md)

## Main Authors

- [Ahmet Inan](https://github.com/xdsopl)
- [Igor Freire](https://github.com/igorauad)
- [Ron Economos](https://github.com/drmpeg/)

## Further Information

- [Igor Freire's talk at GRCon22: "gr-dvbs2rx: An Overview of the project state and path forward."](https://www.youtube.com/watch?v=qcqvfElQUVk&t)
