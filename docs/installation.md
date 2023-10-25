# Installation

## From Source

Currently, the only available installation method is by compiling from source locally or within a Docker container environment. Other methods such as binary packages and [PyBOMBS](https://www.gnuradio.org/blog/2016-06-19-pybombs-the-what-the-how-and-the-why/) will be provided in the future.

To build from source, make sure to install the [build dependencies](#build-dependencies) first. After that, run:

```
git clone --recursive https://github.com/igorauad/gr-dvbs2rx.git
cd gr-dvbs2rx/
mkdir build
cd build
cmake ..
make
```

Next, test the built binaries by running:
```
ctest
```

Finally, install by executing:
```
sudo make install
sudo ldconfig
```
## Docker Image

A Dockerfile is available with a recipe for a Ubuntu-based Docker image featuring GNU Radio and gr-dvbs2rx. To build it, run:

```
docker build -t gr-dvbs2rx .
```

## Further Information

### Build Options

When compiling from source, the CMake build can be customized by the following options:

- `BENCHMARK_CPU`: when set to ON, builds an application for CPU benchmarking of selected functions. The application becomes available at `bench/cpu/bench_cpu` relative to the build directory. This feature requires the [libbenchmark library](https://github.com/google/benchmark).

- `BENCHMARK_FEC`: when set to ON, builds an application to assess the performance of various forward error correction (FEC) configurations. The current implementation focuses on the PLSC decoder and yields the `bench_plsc` application at the `bench/fec/` subdirectory of the build directory. This option requires the [AFF3CT library](http://aff3ct.github.io) (version 3.0.0 or later).

- `DEBUG_LOGS`: when set to OFF, disables the low-level logs available by default to debug the physical layer operation.

- `NATIVE_OPTIMIZATIONS`: when set to ON (default), compiles the dvbs2rx library using the `-march=native` flag to enable optimizations for the local CPU. Use this option to obtain improved CPU performance, as long as your goal is to run the project on the same machine used for compilation. Set this option to OFF if compiling binaries to run on other CPUs.

> Note the build options must be specified on the `cmake` step. For example, for an option named `MYOPTION`, append either `-DMYOPTION=ON` or `-DMYOPTION=OFF` to the `cmake ..` step.


### Build Dependencies

Fedora:

```
dnf install \
  cmake \
  doxygen \
  fftw-devel \
  gmp-devel \
  gnuradio-devel \
  graphviz \
  libsndfile-devel \
  pybind11-devel \
  python3-packaging \
  spdlog-devel
```

Ubuntu/Debian:

```
apt install \
  cmake \
  doxygen \
  gnuradio-dev \
  graphviz \
  libsndfile1-dev \
  libspdlog-dev \
  pkg-config \
  pybind11-dev \
  python3-packaging
```

---
Prev: [Supported Modes](support.md)  -  Next: [Usage](usage.md)