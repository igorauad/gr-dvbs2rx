# Usage

- [Usage](#usage)
  - [Input and Output Options](#input-and-output-options)
  - [Experimenting with Parameters](#experimenting-with-parameters)
  - [Graphical User Interface](#graphical-user-interface)
  - [Processing the MPEG Transport Stream](#processing-the-mpeg-transport-stream)
  - [Receiver Monitoring](#receiver-monitoring)
  - [Further Information](#further-information)


## Input and Output Options

The `dvbs2-tx` and `dvbs2-rx` applications are designed to interface with file descriptors, regular files, and SDR boards. By default, they read the input from stdin (file descriptor 0) and output to stdout (file descriptor 1). Hence, for testing purposes, the Tx output can be piped into the Rx input as follows:

### Example 1

```
cat example.ts | dvbs2-tx | dvbs2-rx > /dev/null
```

In this case, the Tx reads the MPEG transport stream (TS) file (`example.ts`) from its standard input (stdin) and transmits the corresponding IQ stream into the Rx app. Meanwhile, the Rx app outputs the decoded MPEG TS stream directly into its standard output (stdout), which is redirected to the null device.

If you don't have an MPEG TS file for testing, you can download an [example TS file](http://www.w6rz.net/thefuryclip.ts). Alternatively, you can use a transport stream generator. The `tsp` tool from the [TSDuck](https://tsduck.io) toolkit provides such a generator (see the [installation instructions](#tsduck-installation)), which can be used as follows:

### Example 2
```
tsp -I craft --pid 100 | dvbs2-tx | dvbs2-rx > /dev/null
```

In this case, it is the `craft` plugin from `tsp` that is generating a fake MPEG transport stream with packet ID (PID) 100 on the input to the `dvbs2-tx` app.

So far, both examples have adopted the file descriptor interface for input and output. However, the applications offer flexible source and sink options, which can be modified using the `--source` and `--sink` command-line switches. For example, you can specify source and sink files directly, as follows:

### Example 3
```
dvbs2-tx --source file --in-file example.ts | \
    dvbs2-rx --sink file --out-file /dev/null
```

When using file descriptors, it is also possible to configure which descriptors take the input and output. One problem in [Example 1](#example-1) is that the output from `dvbs2-rx` is entirely redirected to the null device, including the receiver logs. As an alternative, the MPEG TS output can be fed into a separate file descriptor instead of the stdout (descriptor 1), as follows:

### Example 4
```
cat example.ts | dvbs2-tx | dvbs2-rx --out-fd 3 3> /dev/null
```

In this example, only the decoded TS output is fed into descriptor 3, whereas the receiver logs continue to be printed to the console via the standard output. Hence, this command is equivalent to that of [Example 3](#example-2), since `--out-file` regulates the file for the TS output only, excluding the receiver logs.

Next, if your goal is to receive using an RTL-SDR interface, you can run a command like the following:

### Example 5
```
dvbs2-rx --source rtl --freq 1316.9e6 --sym-rate 1e6
```

Where:

- Option `--source` configures the application to read IQ samples from an RTL-SDR interface.
- Option `--freq` determines the RTL-SDR's tuner frequency.
- Option `--sym-rate` specifies the symbol rate (also known as baud rate) in symbols per second (or bauds).

Similarly, using a USRP device, you can receive with:

### Example 6
```
dvbs2-rx -f 1316.9e6 -s 1e6 --source usrp --usrp-args "serial=xyz"
```

where option `--usrp-args` specifies the [address identifier](https://files.ettus.com/manual/page_identification.html) of the target USRP device.

The same holds for the Tx application. A USRP device can be used for transmission as follows:

### Example 7
```
tsp -I craft --pid 100 | \
  dvbs2-tx --sink usrp --usrp-args "serial=xyz" --freq 1316.9e6 --sym-rate 1e6
```

See the help menu (`dvbs2-tx --help` and `dvbs2-rx --help`) for further USRP options like gain, antenna, clock/time source, and more.

## Experimenting with Parameters

Both the `dvbs2-tx` and `dvbs2-rx` applications provide a range of configurable DVB-S2 parameters. For instance, the previous Tx-Rx loopback example can be adapted to test a different MODCOD, FEC frame size, roll-off factor, symbol rate, and pilot configuration:

### Example 8
```
dvbs2-tx \
    --source file \
    --in-file example.ts \
    --modcod 8psk3/5 \
    --frame short \
    --rolloff 0.35 \
    --sym-rate 2000000 \
    --pilots | \
dvbs2-rx \
    --sink file \
    --out-file /dev/null \
    --modcod 8psk3/5 \
    --frame short \
    --rolloff 0.35 \
    --sym-rate 2000000
```

> NOTE: only the Tx application needs to enable pilot blocks explicitly. The Rx application detects the presence of pilots automatically. Nevertheless, you can also configure the Rx application with extra a priori information for better performance. For example, you can tell the Rx app that the received stream assuredly contains pilot symbols by appending option `--pilots on`.

The Tx application also supports noise and frequency offset simulation. For instance, you can repeat the above experiment on a 10 dB SNR scenario with a 100 kHz frequency offset by appending the following options to the `dvbs2-tx` command:

```
--snr 10 --freq-offset 1e5
```

## Graphical User Interface

A graphical user interface (GUI) is available on the receiver application. You can optionally enable it by running with the `--gui` option, as follows:

### Example 9
```
dvbs2-rx --source rtl --freq 1316.9e6 --sym-rate 1e6 --gui
```

Furthermore, you can run the GUI in dark mode with option `--gui-dark` or extend the content of the GUI by enabling the optional plots. See the help menu for the list of optional GUI metrics. Alternatively, enable all of them using option `--gui-all`.

Note, however, that the GUI increases the CPU utilization. Hence, if you are looking for a lightweight execution, consider running without the GUI.

## Processing the MPEG Transport Stream

The MPEG Transport Stream layer is beyond the scope of this project. The DVB-S2 Rx application only aims to output the MPEG TS stream for an external application instead of handling the MPEG stream itself. Likewise, the Tx application takes a fully-formatted TS stream on its input and does not modify the TS stream at all.

A recommended application to handle the MPEG TS layer is the `tsp` tool from the [TSDuck](https://tsduck.io) toolkit. The following example demonstrates how you can pipe the output from `dvbs2-rx` into `tsp`.

### Example 10
```
cat example.ts | dvbs2-tx | dvbs2-rx --out-fd 3 3>&1 1>&2 | \
  tsp -P mpe --pid 32 --udp-forward --local-address 127.0.0.1 -O drop
```

In this example, it is assumed that the incoming signal contains UDP packets encapsulated as follows:

```
DVB-S2 frames > MPEG TS packets > MPE frames > IP packets > UDP packets
```

The `tsp` application first extracts the [Multiprotocol Encapsulation (MPE)](https://en.wikipedia.org/wiki/Multiprotocol_Encapsulation) frames contained within the MPEG TS packets identified by PID (packet identifier) 32. Subsequently, the `mpe` plugin unpacks the UDP/IP packets within the MPE layer and forwards these packets towards the loopback interface with address `127.0.0.1`.

This example uses noteworthy descriptor redirection options, which ensure only the MPEG TS output is fed into `tsp` and not the receiver logs. At first, the TS output is fed into descriptor 3 through option `-out-fd 3`. However, descriptor three is later redirected to stdout through option `3>&1`. At the same time, the original stdout (containing the receiver logs) goes to stderr using `1>&2`. In the end, only the TS output is piped into `tsp`, and the logs are preserved on the console via stderr.

Another useful application from the TSDuck toolkit is the `tsdump` tool, which dumps all the incoming TS packets into the console in real-time. You can use it as follows:

### Example 11
```
cat example.ts | dvbs2-tx | dvbs2-rx --out-fd 3 3>&1 1>&2 | tsdump --no-pager --headers-only
```

Please refer to TSDuck's [user guide](https://tsduck.io/download/docs/tsduck.pdf) for further information.

## Receiver Monitoring

While the DVB-S2 receiver is running, it is often helpful to observe the underlying low-level performance metrics and statistics. For example, metrics such as the frequency offset estimated at the physical layer, the frame synchronization lock status, the average number of LDPC correction iterations, the MPEG TS packet counts, and so on. The receiver application can provide such metrics either through periodic logs printed to the console or on-demand via HTTP requests. In both cases, it returns the information in JSON format.

The example below illustrates the monitoring server launched for on-demand access via HTTP requests:

### Example 12
```
dvbs2-rx --source rtl --freq 1316.9e6 --sym-rate 1e6 --mon-server
```

In this case, you can probe the receiver at any time by sending a request to it via port 9004 (configurable through option `--mon-port`). For example, using the following command:

```
curl -s http://localhost:9004 | python3 -m json.tool
```

Alternatively, you may want to print the performance metrics continuously to the console. To do so, run with option `--log-stats`, as follows:

### Example 13
```
dvbs2-rx --source rtl --freq 1316.9e6 --sym-rate 1e6 --log-stats
```

## Further Information

### TSDuck Installation

You can install TSDuck directly from the [available binaries](https://tsduck.io/download/tsduck/) or from the [Blockstream Satellite](https://github.com/Blockstream/satellite) package repositories, as follows:

Ubuntu:
```
add-apt-repository ppa:blockstream/satellite
apt-get update
apt-get install tsduck
```

Debian:

```
add-apt-repository https://aptly.blockstream.com/satellite/debian/
apt-key adv --keyserver keyserver.ubuntu.com \
    --recv-keys 87D07253F69E4CD8629B0A21A94A007EC9D4458C
apt-get update
apt-get install tsduck
```

Raspbian:

```
add-apt-repository https://aptly.blockstream.com/satellite/raspbian/
apt-key adv --keyserver keyserver.ubuntu.com \
    --recv-keys 87D07253F69E4CD8629B0A21A94A007EC9D4458C
apt-get update
apt-get install tsduck
```

Fedora:
```
dnf copr enable blockstream/satellite
dnf install tsduck
```

---
Prev: [Installation](installation.md)