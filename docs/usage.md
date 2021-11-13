# Usage

- Contents
  - [Input and Output Options](#input-and-output-options)
  - [Experimenting with Parameters](#experimenting-with-parameters)
  - [Graphical User Interface](#graphical-user-interface)
  - [Processing the MPEG Transport Stream](#processing-the-mpeg-transport-stream)


## Input and Output Options

The `dvbs2-tx` and `dvbs2-rx` applications are designed to interface with file
descriptors, regular files, and SDR boards. By default, they read the input from
stdin (file descriptor 0) and output to stdout (file descriptor 1). Hence, for
testing purposes, the Tx output can be piped into the Rx input as follows:

**Example 1:**
```
cat example.ts | dvbs2-tx | dvbs2-rx > /dev/null
```

Note the Tx reads the MPEG transport stream (TS) file (`example.ts`) from its
standard input (stdin) and transmits the corresponding IQ stream. Meanwhile, the
Rx reads the IQ stream fed into its stdin and outputs the decoded MPEG TS stream
directly into its standard output (stdout). Finally, the Rx output is redirected
to the null device.

> NOTE: If you don't have an MPEG TS file to test, you can download an [example
> TS file](http://www.w6rz.net/thefuryclip.ts)

Nevertheless, both the Tx and Rx applications offer flexible source and sink
options, which can be modified using the `--source` and `--sink` command-line
switches. For example, instead of using file descriptors, as in **Example 1**,
one can specify source and sink files directly, as follows:

**Example 2:**
```
dvbs2-tx --source file --in-file example.ts | \
    dvbs2-rx --sink file --out-file /dev/null
```

When using file descriptors, it is also possible to configure which descriptors
take the input and output. One problem in **Example 1**is that the output from
`dvbs2-rx` is entirely redirected to the null device, including the receiver
logs. As an alternative, the MPEG TS output can be fed into a separate file
descriptor instead of the regular stdout (descriptor 1), as follows:

**Example 3:**
```
cat example.ts | dvbs2-tx | dvbs2-rx --out-fd 3 3> /dev/null
```

In this example, only the decoded TS output is fed to descriptor 3, whereas the
receiver logs continue to be printed to the console via the standard output.
Hence, this command is equivalent to that of **Example 2**, since `--out-file`
regulates the file for the TS output only, excluding the receiver logs.

Next, if you are running an SDR interface, you can run a command like the
following:

**Example 4:**
```
dvbs2-rx --source rtl --freq 1316.9e6 --sym-rate 1e6
```

Where:

- Option `--source` configures the application to read IQ samples from an
  RTL-SDR interface.
- Option `--freq` determines the RTL-SDR tuner frequency.
- Option `--sym-rate` specifies the symbol rate (also known as baud rate) in
  symbols per second (or bauds).

Similarly, the Tx application can feed the transmit IQ stream into the Tx
interface of a USRP, as follows:

**Example 5:**
```
dvbs2-tx --sink usrp --out-usrp "serial=xyz" --freq 1316.9e6 --sym-rate 1e6
```

Where option `--out-usrp` specifies the identifier of the destination USRP.

## Experimenting with Parameters

Both the `dvbs2-tx` and `dvbs2-rx` applications provide a range of configurable
DVB-S2 parameters. For instance, the previous Tx-Rx loopback example can be
adapted to test a different MODCOD, FEC frame size, roll-off factor, symbol
rate, and pilot configuration:

**Example 6:**
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

> NOTE: only the Tx application needs to enable pilot blocks explicitly. The Rx
> application detects the presence of pilots automatically. Nevertheless, you
> can also configure the Rx application with extra a priori information for
> better performance. For example, you can tell the Rx app that the received
> stream assuredly contains pilot symbols by appending option `--pilots=on`.
> Similarly, if the input stream assuredly consists of a single input stream
> (SIS), you can inform the Rx application through option `--multistream=off`.

The Tx application also supports noise and frequency offset simulation. For
instance, you can repeat the above experiment on a 10 dB SNR scenario with a 100
kHz frequency offset by appending the following options to the `dvbs2-tx`
command:

```
--snr 10 --freq-offset 1e5
```

## Graphical User Interface

A graphical user interface (GUI) is available on the receiver application. You
can optionally enable it by running with the option `--gui`, as follows:

```
dvbs2-rx --source rtl --freq 1316.9e6 --sym-rate 1e6 --gui
```

Note, however, that the GUI increases the CPU utilization. Hence, if you are
looking for a lightweight execution, consider running with the GUI off.

## Processing the MPEG Transport Stream

The MPEG Transport Stream layer is beyond the scope of this project. For
example, the DVB-S2 Rx application only aims to output the MPEG TS stream for an
external application that accepts it via stdin, instead of handling the MPEG
stream itself. Likewise, the Tx application takes a fully-formatted TS stream on
its input and does not modify the TS stream at all.

A recommended application to handle the MPEG TS layer is the `tsp` tool from the
[TSDuck](https://tsduck.io) toolkit. The following example demonstrates how you
can pipe the `dvbs2-rx` into `tsp`.

**Example 7**:
```
cat example.ts | dvbs2-tx | dvbs2-rx --out-fd 3 3>&1 1>&2 | \
  tsp -P mpe --pid 32 --udp-forward --local-address 127.0.0.1 -O drop
```

In this example, it is assumed that the incoming signal contains UDP packets
encapsulated as follows:

```
DVB-S2 frames > MPEG TS packets > MPE frames > IP packets > UDP packets
```

The `tsp` application first extracts the [Multiprotocol Encapsulation
(MPE)](https://en.wikipedia.org/wiki/Multiprotocol_Encapsulation) frames
contained within the MPEG TS packets identified by PID (packet identifier) 32.
Subsequently, the `mpe` plugin unpacks the UDP/IP packets within the MPE layer
and forwards these packets towards the loopback interface with address
`127.0.0.1`.

Besides, this example uses noteworthy descriptor redirection options, which are
meant to ensure only the MPEG TS output is fed into `tsp` and not the receiver
logs. At first, the TS output is fed into descriptor 3 through option `-out-fd
3`. However, descriptor three is later redirected to stdout through option
`3>&1`. At the same time, the original stdout (where receiver logs are printed)
is redirected to stderr using `1>&2`. In the end, only the TS output is piped
into `tsp`, and the logs are preserved on the console (via stderr).

Lastly, another useful application from the TSDuck toolkit is the `tsdump` tool,
which dumps all the incoming TS packets into the console in real-time. You can
use it as follows:

**Example 8**:
```
cat example.ts | dvbs2-tx | dvbs2-rx --out-fd 3 3>&1 1>&2 | tsdump --no-pager --headers-only
```

Please refer to TSDuck's [user
guide](https://tsduck.io/download/docs/tsduck.pdf) for further information.

---
Prev: [Installation](installation.md)