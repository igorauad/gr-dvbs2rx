# Installation

Currently, the only available installation method is by compiling from source
locally or within a Docker container environment. Other methods such as binary
packages and
[PyBOMBS](https://www.gnuradio.org/blog/2016-06-19-pybombs-the-what-the-how-and-the-why/)
will be provided in the future.

To build from source, run:
```
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

A Dockerfile is available with a recipe for a Ubuntu-based Docker image
featuring GNU Radio and gr-dvbs2rx. To build it, run:

```
docker build -t gr-dvbs2rx .
```

---
Prev: [Supported Modes](support.md)  -  Next: [Usage](usage.md)