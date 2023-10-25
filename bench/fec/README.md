# FEC Benchmarking

This directory contains a set of benchmarking programs for the forward error correction (FEC) decoders used on the DVB-S2 receiver.

- [FEC Benchmarking](#fec-benchmarking)
  - [Dependencies](#dependencies)
  - [Reed-Muller PLSC Decoder](#reed-muller-plsc-decoder)
  - [BCH Decoder](#bch-decoder)

## Dependencies

The programs contained in this directory rely on the [AFF3CT infrastructure](https://aff3ct.readthedocs.io/en/latest/user/library/examples.html?highlight=start_temp_report#bootstrap) for simulation and generation of benchmarking reports. Hence, you need to install [AFF3CT library](https://github.com/aff3ct/my_project_with_aff3ct) before building and running these programs. Also, you need to compile gr-dvbs2rx with the `BENCHMARK_FEC` option enabled, as described in the [installation instructions](../../docs/installation.md#build-options).

You can build and install the AFF3CT library by running the following commands. For reference, see the [my_project_with_aff3ct](https://github.com/aff3ct/my_project_with_aff3ct) repository.

```bash
git clone --recursive https://github.com/aff3ct/aff3ct.git -b v3.0.1
cd aff3ct/
mkdir -p build && cd build/
cmake .. -G"Unix Makefiles" \
	  -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_CXX_FLAGS="-funroll-loops -march=native" \
	  -DAFF3CT_COMPILE_EXE="OFF" \
	  -DAFF3CT_COMPILE_STATIC_LIB="ON" \
	  -DAFF3CT_COMPILE_SHARED_LIB="ON"
make -j`nproc`
make install
```

Next, you can build gr-dvbs2rx with the `BENCHMARK_FEC` option enabled on the cmake step:

```bash
cmake -DCMAKE_BUILD_TYPE=Release -DBENCHMARK_FEC=ON -DNATIVE_OPTIMIZATIONS=ON ..
```

Note that, for benchmarking, it is usually preferable to have the `NATIVE_OPTIMIZATIONS` option enabled to evaluate the best performance achievable on the local CPU. For the same reason, make sure to build gr-dvbs2rx in Release mode.

## Reed-Muller PLSC Decoder

### Hard Decoding

```
bench/fec/bench_plsc --hard --fe 10000000
```

```
#----------------------------------------------------------
# PLSC decoding BER vs. SNR benchmark
#----------------------------------------------------------
#
# * Simulation parameters:
#    ** Frame errors   = 10000000
#    ** Max frames     = 10000000
#    ** Noise seed     = 0
#    ** Info. bits (K) = 7
#    ** Frame size (N) = 64
#    ** Code rate  (R) = 0.109375
#    ** SNR min   (dB) = 0
#    ** SNR max   (dB) = 10
#    ** SNR step  (dB) = 1
#    ** Freq. offset   = 0
#    ** Coherent demap = 1
#    ** Soft decoding  = 0
#
# ---------------------||------------------------------------------------------||---------------------
#  Signal Noise Ratio  ||   Bit Error Rate (BER) and Frame Error Rate (FER)    ||  Global throughput
#         (SNR)        ||                                                      ||  and elapsed time
# ---------------------||------------------------------------------------------||---------------------
# ----------|----------||----------|----------|----------|----------|----------||----------|----------
#     Es/N0 |    Eb/N0 ||      FRA |       BE |       FE |      BER |      FER ||  SIM_THR |    ET/RT
#      (dB) |     (dB) ||          |          |          |          |          ||   (Mb/s) | (hhmmss)
# ----------|----------||----------|----------|----------|----------|----------||----------|----------
      -9.61 |     0.00 || 10000000 | 12693153 |  3566319 | 1.81e-01 | 3.57e-01 ||    3.721 | 00h00'18
      -8.61 |     1.00 || 10000000 |  8635731 |  2425434 | 1.23e-01 | 2.43e-01 ||    3.696 | 00h00'18
      -7.61 |     2.00 || 10000000 |  5103463 |  1433107 | 7.29e-02 | 1.43e-01 ||    3.738 | 00h00'18
      -6.61 |     3.00 || 10000000 |  2501418 |   702080 | 3.57e-02 | 7.02e-02 ||    3.680 | 00h00'19
      -5.61 |     4.00 || 10000000 |   961217 |   269975 | 1.37e-02 | 2.70e-02 ||    3.738 | 00h00'18
      -4.61 |     5.00 || 10000000 |   269619 |    75585 | 3.85e-03 | 7.56e-03 ||    3.731 | 00h00'18
      -3.61 |     6.00 || 10000000 |    50230 |    14144 | 7.18e-04 | 1.41e-03 ||    3.723 | 00h00'18
      -2.61 |     7.00 || 10000000 |     5553 |     1582 | 7.93e-05 | 1.58e-04 ||    3.720 | 00h00'18
      -1.61 |     8.00 || 10000000 |      369 |      107 | 5.27e-06 | 1.07e-05 ||    3.745 | 00h00'18
      -0.61 |     9.00 || 10000000 |        0 |        0 | 1.43e-08 | 1.00e-07 ||    3.742 | 00h00'18
# End of the simulation
```

### Soft Decoding

```
bench/fec/bench_plsc --fe 10000000
```

```
#----------------------------------------------------------
# PLSC decoding BER vs. SNR benchmark
#----------------------------------------------------------
#
# * Simulation parameters:
#    ** Frame errors   = 10000000
#    ** Max frames     = 10000000
#    ** Noise seed     = 0
#    ** Info. bits (K) = 7
#    ** Frame size (N) = 64
#    ** Code rate  (R) = 0.109375
#    ** SNR min   (dB) = 0
#    ** SNR max   (dB) = 10
#    ** SNR step  (dB) = 1
#    ** Freq. offset   = 0
#    ** Coherent demap = 1
#    ** Soft decoding  = 1
#
# ---------------------||------------------------------------------------------||---------------------
#  Signal Noise Ratio  ||   Bit Error Rate (BER) and Frame Error Rate (FER)    ||  Global throughput
#         (SNR)        ||                                                      ||  and elapsed time
# ---------------------||------------------------------------------------------||---------------------
# ----------|----------||----------|----------|----------|----------|----------||----------|----------
#     Es/N0 |    Eb/N0 ||      FRA |       BE |       FE |      BER |      FER ||  SIM_THR |    ET/RT
#      (dB) |     (dB) ||          |          |          |          |          ||   (Mb/s) | (hhmmss)
# ----------|----------||----------|----------|----------|----------|----------||----------|----------
      -9.61 |     0.00 || 10000000 |  5104947 |  1438784 | 7.29e-02 | 1.44e-01 ||    2.572 | 00h00'27
      -8.61 |     1.00 || 10000000 |  2478274 |   699132 | 3.54e-02 | 6.99e-02 ||    2.570 | 00h00'27
      -7.61 |     2.00 || 10000000 |   931482 |   262584 | 1.33e-02 | 2.63e-02 ||    2.566 | 00h00'27
      -6.61 |     3.00 || 10000000 |   254488 |    71757 | 3.64e-03 | 7.18e-03 ||    2.579 | 00h00'27
      -5.61 |     4.00 || 10000000 |    45904 |    12928 | 6.56e-04 | 1.29e-03 ||    2.581 | 00h00'27
      -4.61 |     5.00 || 10000000 |     4647 |     1320 | 6.64e-05 | 1.32e-04 ||    2.577 | 00h00'27
      -3.61 |     6.00 || 10000000 |      240 |       70 | 3.43e-06 | 7.00e-06 ||    2.571 | 00h00'27
      -2.61 |     7.00 || 10000000 |       14 |        4 | 2.00e-07 | 4.00e-07 ||    2.575 | 00h00'27
      -1.61 |     8.00 || 10000000 |        0 |        0 | 1.43e-08 | 1.00e-07 ||    2.574 | 00h00'27
      -0.61 |     9.00 || 10000000 |        0 |        0 | 1.43e-08 | 1.00e-07 ||    2.576 | 00h00'27
```

## BCH Decoder

The BCH benchmarking program can compare the performance achieved with the new BCH decoder introduced into gr-dvbs2rx against the performances achieved with the old BCH decoder and the [Aff3ct BCH decoder](https://aff3ct.readthedocs.io/en/latest/user/simulation/parameters/codec/bch/decoder.html?highlight=BCH).

The program has options to determine the encoder and decoder implementations. See the following examples:

1. AFF3CT encoder feeding into the AFF3CT decoder:

```
bench/fec/bench_bch --enc 0 --dec 0
```

2. Old gr-dvbs2rx encoder feeding into the old gr-dvbs2rx decoder:

```
bench/fec/bench_bch --enc 1 --dec 1
```

3. New gr-dvbs2rx encoder feeding into the new gr-dvbs2rx decoder:

```
bench/fec/bench_bch --enc 2 --dec 2
```

To isolate the decoder performance, you may want to use the same encoder while varying the decoder implementation. For instance:

- `bench/fec/bench_bch --enc 0 --dec 0 --nframes 10000`
- `bench/fec/bench_bch --enc 0 --dec 1 --nframes 10000`
- `bench/fec/bench_bch --enc 0 --dec 2 --nframes 10000`

> Note: Make sure to compile gr-dvbs2rx in Release mode so that the best performance is achieved, as instructed [previously](#dependencies).

The following results were obtained with the above commands using `--enc 0` and varying the decoders on an Apple M2 Max CPU:

AFF3CT decoder:

```text
# ---------------------||------------------------------------------------------||---------------------
#  Signal Noise Ratio  ||   Bit Error Rate (BER) and Frame Error Rate (FER)    ||  Global throughput
#         (SNR)        ||                                                      ||  and elapsed time
# ---------------------||------------------------------------------------------||---------------------
# ----------|----------||----------|----------|----------|----------|----------||----------|----------
#     Es/N0 |    Eb/N0 ||      FRA |       BE |       FE |      BER |      FER ||  SIM_THR |    ET/RT
#      (dB) |     (dB) ||          |          |          |          |          ||   (Mb/s) | (hhmmss)
# ----------|----------||----------|----------|----------|----------|----------||----------|----------
      -0.08 |     0.00 ||      100 |    76626 |      100 | 8.02e-02 | 1.00e+00 ||    9.830 | 00h00'00
       0.92 |     1.00 ||      100 |    55202 |      100 | 5.78e-02 | 1.00e+00 ||    9.714 | 00h00'00
       1.92 |     2.00 ||      100 |    36867 |      100 | 3.86e-02 | 1.00e+00 ||    9.817 | 00h00'00
       2.92 |     3.00 ||      100 |    22921 |      100 | 2.40e-02 | 1.00e+00 ||    9.784 | 00h00'00
       3.92 |     4.00 ||      100 |    12624 |      100 | 1.32e-02 | 1.00e+00 ||    9.928 | 00h00'00
       4.92 |     5.00 ||      100 |     6070 |      100 | 6.35e-03 | 1.00e+00 ||    9.163 | 00h00'00
       5.92 |     6.00 ||      100 |     2469 |      100 | 2.58e-03 | 1.00e+00 ||    9.746 | 00h00'00
       6.92 |     7.00 ||     1320 |     1369 |      100 | 1.09e-04 | 7.58e-02 ||   10.272 | 00h00'01
       7.92 |     8.00 ||    10000 |        0 |        0 | 1.05e-08 | 1.00e-04 ||   10.730 | 00h00'08
       8.92 |     9.00 ||    10000 |        0 |        0 | 1.05e-08 | 1.00e-04 ||   11.338 | 00h00'08
```

Old gr-dvbs2rx decoder:

```text
# ---------------------||------------------------------------------------------||---------------------
#  Signal Noise Ratio  ||   Bit Error Rate (BER) and Frame Error Rate (FER)    ||  Global throughput
#         (SNR)        ||                                                      ||  and elapsed time
# ---------------------||------------------------------------------------------||---------------------
# ----------|----------||----------|----------|----------|----------|----------||----------|----------
#     Es/N0 |    Eb/N0 ||      FRA |       BE |       FE |      BER |      FER ||  SIM_THR |    ET/RT
#      (dB) |     (dB) ||          |          |          |          |          ||   (Mb/s) | (hhmmss)
# ----------|----------||----------|----------|----------|----------|----------||----------|----------
      -0.08 |     0.00 ||      100 |    76626 |      100 | 8.02e-02 | 1.00e+00 ||   11.429 | 00h00'00
       0.92 |     1.00 ||      100 |    55202 |      100 | 5.78e-02 | 1.00e+00 ||   11.449 | 00h00'00
       1.92 |     2.00 ||      100 |    36867 |      100 | 3.86e-02 | 1.00e+00 ||   11.472 | 00h00'00
       2.92 |     3.00 ||      100 |    22921 |      100 | 2.40e-02 | 1.00e+00 ||   11.461 | 00h00'00
       3.92 |     4.00 ||      100 |    12624 |      100 | 1.32e-02 | 1.00e+00 ||   11.458 | 00h00'00
       4.92 |     5.00 ||      100 |     6070 |      100 | 6.35e-03 | 1.00e+00 ||   11.329 | 00h00'00
       5.92 |     6.00 ||      100 |     2469 |      100 | 2.58e-03 | 1.00e+00 ||   11.269 | 00h00'00
       6.92 |     7.00 ||     1320 |     1369 |      100 | 1.09e-04 | 7.58e-02 ||   11.460 | 00h00'01
       7.92 |     8.00 ||    10000 |        0 |        0 | 1.05e-08 | 1.00e-04 ||   13.172 | 00h00'07
       8.92 |     9.00 ||    10000 |        0 |        0 | 1.05e-08 | 1.00e-04 ||   14.030 | 00h00'06
```

New gr-dvbs2rx decoder:

```text
# ---------------------||------------------------------------------------------||---------------------
#  Signal Noise Ratio  ||   Bit Error Rate (BER) and Frame Error Rate (FER)    ||  Global throughput
#         (SNR)        ||                                                      ||  and elapsed time
# ---------------------||------------------------------------------------------||---------------------
# ----------|----------||----------|----------|----------|----------|----------||----------|----------
#     Es/N0 |    Eb/N0 ||      FRA |       BE |       FE |      BER |      FER ||  SIM_THR |    ET/RT
#      (dB) |     (dB) ||          |          |          |          |          ||   (Mb/s) | (hhmmss)
# ----------|----------||----------|----------|----------|----------|----------||----------|----------
      -0.08 |     0.00 ||      100 |    76671 |      100 | 8.03e-02 | 1.00e+00 ||   14.463 | 00h00'00
       0.92 |     1.00 ||      100 |    55244 |      100 | 5.78e-02 | 1.00e+00 ||   14.274 | 00h00'00
       1.92 |     2.00 ||      100 |    36915 |      100 | 3.86e-02 | 1.00e+00 ||   14.381 | 00h00'00
       2.92 |     3.00 ||      100 |    22972 |      100 | 2.40e-02 | 1.00e+00 ||   14.240 | 00h00'00
       3.92 |     4.00 ||      100 |    12691 |      100 | 1.33e-02 | 1.00e+00 ||   13.860 | 00h00'00
       4.92 |     5.00 ||      100 |     6117 |      100 | 6.40e-03 | 1.00e+00 ||   14.135 | 00h00'00
       5.92 |     6.00 ||      100 |     2516 |      100 | 2.63e-03 | 1.00e+00 ||   14.351 | 00h00'00
       6.92 |     7.00 ||     1320 |     1422 |      100 | 1.13e-04 | 7.58e-02 ||   16.347 | 00h00'00
       7.92 |     8.00 ||    10000 |        0 |        0 | 1.05e-08 | 1.00e-04 ||   19.972 | 00h00'04
       8.92 |     9.00 ||    10000 |        0 |        0 | 1.05e-08 | 1.00e-04 ||   22.877 | 00h00'04
```