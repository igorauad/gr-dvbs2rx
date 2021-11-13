FROM igorfreire/gnuradio-oot-dev:3.9.2-ubuntu-focal
RUN apt-get install -y libusb-1.0-0-dev libosmosdr-dev libsndfile1-dev
RUN git clone https://github.com/osmocom/rtl-sdr.git && \
    cd rtl-sdr/ && mkdir build && cd build/ && \
    cmake -DINSTALL_UDEV_RULES=ON -DDETACH_KERNEL_DRIVER=ON .. && \
    cmake --build . && \
    cmake --install . && \
    cd ../../ && rm -r rtl-sdr/
RUN git clone https://github.com/osmocom/gr-osmosdr && \
    cd gr-osmosdr/ && \
    mkdir build && cd build && \
    cmake -DENABLE_DOXYGEN=OFF .. && \
    cmake --build . && \
    cmake --install . && \
    ldconfig && \
    cd ../../ && rm -r gr-osmosdr/
RUN add-apt-repository -y ppa:blockstream/satellite && \
    apt install -y tsduck
ADD . /src/gr-dvbs2rx/
RUN cd /src/gr-dvbs2rx/ && mkdir build && cd build && \
    cmake -DENABLE_DOXYGEN=OFF .. && \
    cmake --build . && \
    cmake --install . && \
    ldconfig && \
    cd ../../ && rm -r gr-dvbs2rx/