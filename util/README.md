# Utility Tools and Scripts

## IQ Recordings CLI

The IQ Recordings CLI (`iq-rec-cli`) is a command line tool that can help manage and test the IQ recordings generated with the `dvbs2-rec` application.

First, you can use it to list the available recordings:

```bash
./iq-rec-cli ls
```

By default, the CLI assumes the recordings are in the `data/iq/` directory relative to the root of the repository. If that is not the case, you can switch the directory using option `--data-dir`.

Next, you can select one of the recordings and test the receiver on it. The CLI will run `dvbs2-rx` reading IQ samples from the file and will measure the number of decoded bytes, the processing duration, and the corresponding throughput. You can run the test as follows:

```bash
./iq-rec-cli rx REC_NAME
```

Note `REC_NAME` is the name of the recording as displayed in the first column returned by the list (`ls`) command.

The above command can also measure the CPU utilization of each thread spawned by the receiver application. To do so, run it with the `--cpu` option. Also, if you would like to plot the per-thread CPU utilization over time, use the `--plot-cpu` switch, as follows:

```bash
./iq-rec-cli rx REC_NAME --cpu --plot-cpu
```

Finally, the CLI can benchmark implementation performances using a selected recording. In this case, it compares the performance of `dvbs2-rx` to the one achieved with the `leandvb` application, which must be installed separately. The benchmark can be launched as follows:

```bash
./iq-rec-cli bench REC_NAME
```

Like the `rx` command, the `bench` command supports the `--cpu` and `--plot-cpu` options to measure and plot the CPU utilization of the receiver threads.
