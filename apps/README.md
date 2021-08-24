# Applications

The main application provided by this OOT module is the DVB-S2 receiver, named
`dvbs2-rx`. Additionally, this module provides a configurable DVB-S2 Transmitter
to facilitate testing and experimentation.

For example, you can launch both the Tx and Rx applications while piping the Tx
output into the Rx input, as follows:

```
cat example.ts | dvbs2-tx | dvbs2-rx --gui --out-fd 3 1>&2 3> /dev/null
```

Where:

- `example.ts` is an MPEG TS file to be transmitted. You can download the
  [example file](http://www.w6rz.net/thefuryclip.ts) mentioned on the main
  README page.
- The Rx application outputs the received MPEG Transport Stream into file
  descriptor 3, which is redirected to `/dev/null`. Meanwhile, the original
  stdout logs printed by the Rx application (including the SNR and frame count)
  are redirected to stderr and still printed on the console.
- The Rx application is launching the graphical user interface (GUI).

Alternatively, if you are running an SDR interface, you can run the receiver
application only. For instance, using an RTL-SDR interface,  you can run a
command like the following:

```
dvbs2-rx --source rtl --freq 1316.9e6 --sym-rate 1e6 --modcod qpsk3/5
```

Where:

- Option `--source` configures the application to read IQ samples from the
  RTL-SDR interface.
- Option `--freq` determines the RTL-SDR tuner frequency.
- Option `--sym-rate` specifies the symbol rate (also known as baud rate) in
  symbols per second (or bauds).

## Experimenting with Parameters

Both the `dvbs2-tx` and `dvbs2-rx` applications provide a range of configurable
parameters. For instance, the previous Tx-Rx loopback example can be adapted to
test a different MODCOD and frame configuration:

```
cat example.ts | \
  dvbs2-tx --modcod 8psk3/5 --frame short | \
  dvbs2-rx --modcod 8psk3/5 --frame short --gui --out-fd 3 1>&2 3> /dev/null
```

## Processing the MPEG Transport Stream

The MPEG Transport Stream layer is beyond the scope of this project. So, instead
of handling this layer, the DVB-S2 Rx application only aims to output the MPEG
TS stream for an external application that accepts it via stdin. For example,
one application that fits well in this context is the `tsp` application provided
by the [TSDuck](https://tsduck.io) toolkit.

The following command demonstrates how the `dvbs2-rx` output can be piped into
another application. In this case, the application is the `tsdump` tool, also
from TSDuck, which dumps all the incoming TS packets into the console in real
time.

```
cat example.ts | dvbs2-tx | dvbs2-rx --out-fd 3 3>&1 1>&2 | tsdump --no-pager --headers-only
```