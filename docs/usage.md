# Usage

The `dvbs2-tx` and `dvbs2-rx` applications implement full DVB-S2 transmitter and receiver stacks. They consist of Python applications connecting the DVB-S2 blocks and offering various command-line options for different use cases. This section clarifies the main configurations and how to use these applications in general.

- [Usage](#usage)
  - [Application Interface](#application-interface)
  - [Input and Output Options](#input-and-output-options)
  - [Experimenting with Parameters](#experimenting-with-parameters)
  - [Graphical User Interface](#graphical-user-interface)
  - [Processing the MPEG Transport Stream](#processing-the-mpeg-transport-stream)
  - [Logging and Receiver Monitoring](#logging-and-receiver-monitoring)
  - [Recording and Playback](#recording-and-playback)
  - [Further Information](#further-information)


## Application Interface

The `dvbs2-tx` and `dvbs2-rx` applications are designed to interface with file descriptors, regular files, and SDR boards. By default, they read the input from stdin (file descriptor 0) and output to stdout (file descriptor 1). Hence, for testing purposes, the Tx output can be piped into the Rx input as follows:

### Example 1

```
cat example.ts | dvbs2-tx | dvbs2-rx
```

In this case, the Tx reads the MPEG transport stream (TS) file (`example.ts`) from its standard input (stdin) and transmits the corresponding IQ stream into the Rx app. Meanwhile, the Rx app outputs the decoded MPEG TS stream directly into its standard output (stdout).

If you don't have an MPEG TS file for testing, you can download an [example TS file](https://www.w6rz.net/thefuryclip.ts) by running the following:

```
wget https://www.w6rz.net/thefuryclip.ts -O example.ts
```

Alternatively, you can use a transport stream generator. The `tsp` tool from the [TSDuck](https://tsduck.io) toolkit provides such a generator (see the [installation instructions](#tsduck-installation)), which can be used as follows:

### Example 2
```
tsp -I craft --pid 100 | dvbs2-tx | dvbs2-rx
```

In this case, the `craft` plugin from `tsp` generates an MPEG transport stream with packet ID (PID) 100 and feeds it into the `dvbs2-tx` app for modulation and transmission. Then, the `dvbs2-rx` application demodulates the signal and outputs the decoded TS stream to stdout. More examples based on `tsp` are presented [later in this guide](#processing-the-mpeg-transport-stream).

The Rx configuration presented so far is not generally recommended because its output printed to stdout has both logs and TS data. This approach can cause problems when piping the output from `dvbs2-rx` into an application that processes the decoded TS stream. For this and other use cases, the Tx and Rx applications offer flexible configurations for the input and file descriptors, which only affect the TS input/output, not the logs. For example, the MPEG TS output can be fed into a separate file descriptor instead of the stdout (descriptor 1), while the logs are preserved on the stdout, as follows:

### Example 3
```
cat example.ts | dvbs2-tx | dvbs2-rx --log --out-fd 3 3> /dev/null
```

In this example, note the receiver logs are printed to the console, while the TS output is omitted due to being redirected to `/dev/null`. Also, this example uses option `--log` so that the receiver prints relevant metrics periodically.

## Input and Output Options

In addition to the default file descriptor (`fd`) interface for input and output, the Tx and Rx applications offer flexible source and sink options, which can be modified using the `--source` and `--sink` command-line switches. Currently, they have the following options:

| Application | Source                                             | Sink                                        |
| ----------- | -------------------------------------------------- | ------------------------------------------- |
| `dvbs2-tx`  | `fd`, `file`                                       | `fd`, `file`, `usrp`, `bladeRF`, `plutosdr` |
| `dvbs2-rx`  | `fd`, `file`, `rtl`, `usrp`, `bladeRF`, `plutosdr` | `fd`, `file`                                |

For example, the configuration from [Example 3](#example-3) can be reproduced using `file` source/sinks instead of `fd` source/sinks, as follows:

### Example 4
```
dvbs2-tx --source file --in-file example.ts | \
dvbs2-rx --log --sink file --out-file /dev/null
```

Alternatively, you can specify SDR interfaces as Tx sink or Rx sources. For example, to receive using an RTL-SDR interface, you can run a command like the following:

### Example 5

```
dvbs2-rx --source rtl --freq 1316.9e6 --sym-rate 1e6
```

Where:

- Option `--source` configures the application to read IQ samples from the RTL-SDR.
- Option `--freq` determines the RTL-SDR's tuner frequency.
- Option `--sym-rate` specifies the symbol rate (also known as baud rate) in symbols per second (or bauds).

Similarly, you can receive with a USRP by running a command like:

### Example 6

```
dvbs2-rx --source usrp --freq 1316.9e6 --sym-rate 1e6 --usrp-args "serial=xyz"
```

where option `--usrp-args` specifies the [address identifier](https://files.ettus.com/manual/page_identification.html) of the target USRP device.

And similar options are available on the Tx application. For instance, you can transmit using a USRP device as follows:

### Example 7

```
tsp -I craft --pid 100 | \
dvbs2-tx --sink usrp --freq 1316.9e6 --sym-rate 1e6 --usrp-args "serial=xyz"
```

See the help menu (`dvbs2-tx --help` or `dvbs2-rx --help`) for further USRP options like gain, antenna, clock/time source, and more.

Moreover, as indicated in the above table, it is also possible to transmit and receive with a bladeRF or PlutoSDR device, like so:

### Example 8

bladeRF transmission:
```
tsp -I craft --pid 100 | dvbs2-tx --sink bladeRF --freq 1316.9e6 --sym-rate 1e6
```

bladeRF reception:
```
dvbs2-rx --source bladeRF --freq 1316.9e6 --sym-rate 1e6
```

PlutoSDR transmission:
```
tsp -I craft --pid 100 | dvbs2-tx --sink plutosdr --freq 1316.9e6 --sym-rate 1e6
```

PlutoSDR reception:
```
dvbs2-rx --source plutosdr --freq 1316.9e6 --sym-rate 1e6
```

Again, the help menu shows further device-specific options for these devices, like gain, device identification/address, and more.

## Experimenting with Parameters

Both the `dvbs2-tx` and `dvbs2-rx` applications provide a range of configurable DVB-S2 parameters. For instance, the previous Tx-Rx loopback example can be adapted to test a different MODCOD, FEC frame size, roll-off factor, symbol rate, and pilot configuration:

### Example 9
```
dvbs2-tx \
    --source file \
    --in-file example.ts \
    --in-repeat \
    --modcod 8psk3/5 \
    --frame short \
    --rolloff 0.35 \
    --sym-rate 2000000 \
    --pilots | \
dvbs2-rx \
    --log \
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

A graphical user interface (GUI) is available on the transmitter and receiver applications. You can optionally enable it by running with the `--gui` option on either the Tx or Rx application. For instance, [Example 5](#example-5) can be altered to include the GUI as follows:

### Example 10

```
dvbs2-rx --source rtl --freq 1316.9e6 --sym-rate 1e6 --gui
```

Furthermore, you can run the GUI in dark mode with option `--gui-dark` or extend the content of the GUI by enabling the optional plots. See the help menu for the list of optional GUI metrics. Alternatively, enable all of them using option `--gui-all`.

Note, however, that the GUI increases the CPU utilization. Hence, if you are looking for a lightweight execution, consider running without the GUI.

## Processing the MPEG Transport Stream

The MPEG Transport Stream layer is beyond the scope of this project. The DVB-S2 Rx application only aims to output the MPEG TS stream for an external application instead of handling the MPEG stream itself. Likewise, the Tx application takes a fully-formatted TS stream on its input and does not modify the TS stream at all.

A recommended application to handle the MPEG TS layer is the `tsp` tool from the [TSDuck](https://tsduck.io) toolkit. The following examples demonstrate how you can pipe the output of `tsp` into `dvbs2-tx` or the output from `dvbs2-rx` into `tsp`.

### Example 11

The `craft` plugin used earlier is handy for generating a test TS stream. The example below uses it on the Tx side, while the `bitrate_monitor` plugin is used on the Rx side to measure the bitrate of the decoded MPEG TS stream:

```
tsp -I craft --pid 100 | \
dvbs2-tx --out-fd 3 3>&1 1>&2 | \
dvbs2-rx --out-fd 3 3>&1 1>&2 | \
tsp --realtime --buffer-size-mb 1 -P bitrate_monitor -p 1 -O drop
```

This example uses noteworthy descriptor redirection options on `dvbs2-tx` and `dvbs2-rx` to ensure they only feed their IQ or MPEG TS outputs into the next app, and not their logs. For instance, the `dvbs2-rx` application outputs the decoded TS stream into descriptor 3 through option `--out-fd 3`. At the same time, descriptor three is redirected to stdout using `3>&1`, while the original stdout (containing the receiver logs) goes to stderr due to `1>&2`. In the end, only the TS output is piped into `tsp`, and the logs are preserved on the console via stderr. The same idea applies to `dvbs2-tx`, and the same trick is adopted in the other `tsp` examples that follow.

### Example 12

A common application for DVB-S2 is sending UDP/IP traffic within a link-layer protocol like the [Multiprotocol Encapsulation (MPE)](https://en.wikipedia.org/wiki/Multiprotocol_Encapsulation) protocol. In this case, the UDP/IP packets go on the payload of MPE frames, and the MPE frames are carried by MPEG TS packets, which in turn are carried by the DVB-S2 frames. In other words, the encapsulation chain is as follows:

```
DVB-S2 frames > MPEG TS packets > MPE frames > IP packets > UDP packets
```

The `tsp` application can help with MPE transmission and extraction on the Rx side. On the Tx side, the following command generates a null TS stream constrained to a bitrate of 500 kbps and injects MPE-encapsulated UDP/IP packets with PID 32 into the stream. More specifically, the `mpeinject` plugin listens to UDP packets sent to port 9005 and injects those packets into the TS stream with PID 32.

```
tsp -I null -P regulate --bitrate 500000 -P mpeinject --pid 32 9005
```

On the Rx side, the following command extracts MPE frames with PID 32 from the decoded MPEG TS stream and redirects those packets to the localhost's port 9006:

```
tsp --realtime --buffer-size-mb 1 --max-flushed-packets 10 \
    -P mpe --pid 32 --udp-forward --local-address 127.0.0.1 --local-port 9006 -O drop
```

Then, connecting everything, the following command implements the entire chain from Tx to Rx:

```
tsp -I null -P regulate --bitrate 500000 -P mpeinject --pid 32 9005 | \
dvbs2-tx --out-fd 3 3>&1 1>&2 | \
dvbs2-rx --out-fd 3 3>&1 1>&2 | \
tsp --realtime --buffer-size-mb 1 --max-flushed-packets 10 \
    -P mpe --pid 32 --udp-forward --local-address 127.0.0.1 --local-port 9006 -O drop
```

To test, you can send an arbitrary UDP message to port 9005 and observe it arriving on port 9006. While the above command is running, open `tcpdump` in one terminal window:

```
tcpdump -i any -X port 9006
```

And send the test UDP message from another terminal window:

```
echo "Test message" > /dev/udp/127.0.0.1/9005
```

### Example 13

The `tsp` tool can also help with real-time video streaming. For example, the following command uses the `play` output plugin from `tsp` to play the decoded MPEG TS video stream in real time on VLC (provided that VLC is installed):

```
cat example.ts | dvbs2-tx --out-fd 3 3>&1 1>&2 | dvbs2-rx --out-fd 3 3>&1 1>&2 | tsp --realtime --buffer-size-mb 1 -O play
```

### Example 14

Lastly, another useful application from the TSDuck toolkit is the `tsdump` tool, which dumps all the incoming TS packets into the console in real time. You can use it as follows:

```
cat example.ts | dvbs2-tx --out-fd 3 3>&1 1>&2 | dvbs2-rx --out-fd 3 3>&1 1>&2 | tsdump --no-pager --headers-only
```

Please refer to TSDuck's [user guide](https://tsduck.io/download/docs/tsduck.pdf) for further information.

## Logging and Receiver Monitoring

While the DVB-S2 receiver is running, it is often helpful to observe the underlying low-level performance metrics and statistics. For example, metrics such as the frequency offset estimated at the physical layer, the frame synchronization lock status, the average number of LDPC correction iterations, the MPEG TS packet counts, and so on. The receiver application can provide such metrics either through periodic logs printed to the console or on-demand via HTTP requests.

### Example 15

The example below uses the `--mon-server` option to launch a monitoring server for on-demand access of metrics via HTTP requests:

```
dvbs2-rx --source rtl --freq 1316.9e6 --sym-rate 1e6 --mon-server
```

In this case, you can probe the receiver anytime by sending an HTTP request to port 9004 (configurable through option `--mon-port`). Then, the monitoring server returns the receiver metrics in JSON format. For example, using the following command:

```
curl -s http://localhost:9004 | python3 -m json.tool
```

### Example 16

Alternatively, you may want to print the performance metrics periodically to the console. To do so, run with option `--log`, as follows:

```
dvbs2-rx --source rtl --freq 1316.9e6 --sym-rate 1e6 --log
```

By default, this option prints a short set of metrics. However, you can switch to a more comprehensive JSON-formatted logging mode using option `--log-all`. Furthermore, you can control the logging periodicity with option `--log-period`.

## Recording and Playback

A dedicated application is available for recording DVB-S2 IQ received from an SDR device. The `dvbs2-rec` application can be used as follows:

```
dvbs2-rec
--source rtl \
--freq 1316.9e6 \
--sym-rate 1e6 \
--sps 2 \
--modcod qpsk3/5 \
--pilots on \
--author "Author Name" \
--description "Signal X from Satellite Y" \
--hardware "SDR X connected to LNB Y on antenna Z"
```

Among the above arguments, the following are meant to configure the SDR interface:

- The `--source` option specifies the SDR device, which can be `rtl`, `usrp`, `bladeRF`, or `plutosdr`.
- The `--freq` option specifies the tuner central frequency in Hz.
- The `--sym-rate` option specifies the symbol rate in symbols per second (bauds), while the `--sps` option specifies the number of samples per symbol. The two options combined determine the sample rate. Alternatively, you can set the sample rate directly using option `--samp-rate`.

The other arguments are saved as metadata only. With them, you can go back to the recording later and know the signal's origin and characteristics to play it back into the receiver application with the correct parameters. The above example defines the following parameters:

- The `--modcod` option specifies the CCM MODCOD carried by the signal.
- The `--pilots` option indicates the signal contains PL pilots. Alternatively, if you are unsure, you can omit this option and let the receiver application detect the pilots automatically. Also, if you know for sure the signal does not have pilots, you can set `--pilots off`.
- The `--author` option specifies the author of the recording.
- The `--description` option provides a short description of the recording.
- The `--hardware` option describes the hardware used to capture the recording.

On completion, `dvbs2-rec` saves the IQ file following the [Signal Metadata Format (SigMF)](https://github.com/sigmf/SigMF/blob/sigmf-v1.x/sigmf-spec.md) and using the proposed [SigMF Extension for DVB-S2](dvbs2.sigmf-ext.md). Hence, the application saves the IQ data into a file with extension `.sigmf-data` and the metadata into a file with extension `.sigmf-meta`. The metadata file is a JSON-formatted file that any SigMF-compliant application can read. For example, you can use the `iq-rec-cli` [utility tool](../util/README.md) available in the repository to manage the recordings produced by `dvbs2-rec` and play them back into the receiver application for testing and benchmarking purposes.

Aside from using `dvbs2-rec`, you can also use `dvbs2-tx` to produce a simulated IQ file since the Tx application can save output IQ samples into a file. For example, the following command saves a signal with 10 dB SNR and 10 kHz frequency offset into a file named `example.iq`:

```bash
tsp -I craft --pid 100 | \
dvbs2-tx --snr 10 --freq-offset 1e4 --sink file --out-file example.iq
```

However, note `dvbs2-tx` does not save the metadata. Also, `dvbs2-tx` can only produce simulated recordings, not real ones. In contrast, `dvbs2-rec` is meant to capture real signals from an SDR device while saving them in SigMF format for better cataloging and sharing.

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