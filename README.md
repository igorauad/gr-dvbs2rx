# GNU Radio DVB-S2 Receiver

## Overview

`gr-dvbs2rx` is a GNU Radio [out-of-tree (OOT)
module](https://wiki.gnuradio.org/index.php/OutOfTreeModules) with full DVB-S2
transmitter and receiver implementations for software-defined radio (SDR).

For the receiver side, this project includes a shared library with all the
essential signal processing blocks to process a stream of input IQ samples and
recover an MPEG transport stream (TS) on the output. The processing blocks
include, for instance, the physical layer (PL) synchronization stages (frame
timing, symbol timing, carrier frequency, and phase recovery), forward error
correction (FEC) blocks, and baseband frame (BBFRAME) processing. These blocks
are wired together within the `dvbs2-rx` application and in the example GNU
Radio flowgraphs.

Additionally, the project provides the `dvbs2-tx` transmitter application. While
this OOT module focuses on the Rx signal processing blocks, the Tx blocks are
taken directly from the
[`gr-dtv`](https://github.com/gnuradio/gnuradio/tree/master/gr-dtv) in-tree GNU
Radio module for digital TV. Ultimately, similar to the Rx application, the
`dvbs2-tx` application wires the Tx processing blocks together while offering a
flexible user interface.

Both the Tx and Rx applications are highly customizable through command-line
options. The options are further detailed within the [usage
section](docs/usage.md). They include, for instance, customization of the
MODCOD, frame size, PL pilots, rolloff factor, and much more.

This repository is a fork of drmpeg's
[gr-dvbs2rx](http://github.com/drmpeg/gr-dvbs2rx) project, including the fast
SIMD-accelerated BCH and LDPC implementations from
[aicodix/code](https://github.com/aicodix/code/) and
[xdsopl/LDPC](https://github.com/xdsopl/LDPC), respectively. In particular, this
project extends `drmpeg/gr-dvbs2rx` primarily by adding the physical layer
blocks required for a fully-fledged receiver and by adding the flexible
`dvbs2-tx` and `dvbs2-rx` applications.

This project is licensed under GPLv3, and it was developed primarily for
non-commercial research, experimentation, and educational purposes. Please feel
free to contribute or get in touch to request features or report any problems.

## User Guide

- [Supported operation modes and limitations.](docs/support.md)
- [Installation instructions.](docs/installation.md)
- [Usage instructions.](docs/usage.md)

## Authors

- [Ahmet Inan](https://github.com/xdsopl)
- [Igor Freire](https://github.com/igorauad)
- [Ron Economos](https://github.com/drmpeg/) - w6rz@comcast.net