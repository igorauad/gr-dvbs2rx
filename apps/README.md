# Applications

The main application provided by this OOT module is the DVB-S2 receiver, named
`dvbs2-rx`. Additionally, this module provides a configurable DVB-S2 Transmitter
to facilitate testing and experimentation.

For example, you can launch both the Tx and Rx applications while piping the Tx
output into the Rx input, as follows:

```
dvbs2-tx --source file --in-file example.ts | \
    dvbs2-rx --sink file --out-file /dev/null
```

In this case, the Tx transmits the MPEG transport stream (TS) read from file
`example.ts`. Meanwhile, the Rx outputs the decoded MPEG TS stream directly into
the null device.

> NOTE: If you don't have an MPEG TS file to test, you can download the [example
> file](http://www.w6rz.net/thefuryclip.ts) mentioned on the main README page.

Both the Tx and Rx applications offer flexible source and sink options,
described next.

## Run Options

The `dvbs2-tx` and `dvbs2-rx` applications are designed to interface with file
descriptors, regular files, and SDR boards. By default, they read the input from
stdin (file descriptor 0) and output to stdout (file descriptor 1). However, the
sink and source interfaces can be modified using the `--source` and `--sink`
command-line options.

For example, the loopback setup (Tx connected to Rx) explained earlier could be
alternatively obtained with the default stdin/stoud interface as follows:

```
cat example.ts | dvbs2-tx | dvbs2-rx --out-fd 3 3> /dev/null
```

In this case, the only extra trick concerns the redirection to `/dev/null`. With
the previous command (using ` --sink file --out-file /dev/null`), only the
decoded MPEG TS stream was redirected to the null device, while the Rx logs were
preserved on the console. Here, the same is achieved by redirecting the decoded
TS to descriptor 3 and then hooking descriptor 3 into the null device.


Alternatively, if you are running an SDR interface, you can run the receiver
application only. For instance, using an RTL-SDR interface,  you can run a
command like the following:

```
dvbs2-rx --source rtl --freq 1316.9e6 --sym-rate 1e6 --gui
```

Where:

- Option `--source` configures the application to read IQ samples from the
  RTL-SDR interface.
- Option `--freq` determines the RTL-SDR tuner frequency.
- Option `--sym-rate` specifies the symbol rate (also known as baud rate) in
  symbols per second (or bauds).
- Option `--gui` opens a graphical user interface (GUI), if so desired.

Similarly, the Tx application can feed the Tx interface of a USRP, as follows:

```
dvbs2-tx --sink usrp --out-usrp "serial=xyz" --freq 1316.9e6 --sym-rate 1e6
```

Where option `--out-usrp` specifies the identifier of the destination USRP.

## Experimenting with Parameters

Both the `dvbs2-tx` and `dvbs2-rx` applications provide a range of configurable
DVB-S2 parameters. For instance, the previous Tx-Rx loopback example can be
adapted to test a different MODCOD, frame, roloff factor, symbol rate, and pilot
configuration:

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
    --gui \
    --modcod 8psk3/5 \
    --frame short \
    --rolloff 0.35 \
    --sym-rate 2000000
```

> NOTE: only the Tx application needs to enable pilot blocks explicitly. The Rx
> application detects the presence of pilots automatically.

The Tx application also supports noise and frequency offset simulation. For
instance, you can repeat the above experiment on a 10 dB SNR scenario with a 100
kHz frequency offset by appending the following options to the `dvbs2-tx`
command:

```
--snr 10 \
--freq-offset 1e5 \
```

## Processing the MPEG Transport Stream

The MPEG Transport Stream layer is beyond the scope of this project. Instead of
handling this layer, the DVB-S2 Rx application only aims to output the MPEG TS
stream for an external application that accepts it via stdin. For example, an
application that fits well in this context is the `tsp` tool provided by the
[TSDuck](https://tsduck.io) toolkit.

The following command demonstrates how the `dvbs2-rx` output can be piped into
another application. In this case, the application is the `tsdump` tool, also
from TSDuck, which dumps all the incoming TS packets into the console in
real-time.

```
cat example.ts | dvbs2-tx | dvbs2-rx --out-fd 3 3>&1 1>&2 | tsdump --no-pager --headers-only
```