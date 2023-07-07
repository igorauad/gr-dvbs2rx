#include "pi2_bpsk.h"
#include "pl_defs.h"
#include "pl_descrambler.h"
#include "symbol_sync_cc_impl.h"
#include <benchmark/benchmark.h>
#include <volk/volk_alloc.hh>

static void BM_map_bpsk(benchmark::State& state)
{
    std::vector<gr_complex> bpsk(PLSC_LEN);
    for (auto _ : state) {
        gr::dvbs2rx::map_bpsk(0xFFFFFFFFFFFFFFFF, bpsk.data(), PLSC_LEN);
    }
}
BENCHMARK(BM_map_bpsk);

static void BM_demap_bpsk(benchmark::State& state)
{
    std::vector<gr_complex> bpsk = {
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 }
    };

    for (auto _ : state) {
        gr::dvbs2rx::demap_bpsk(bpsk.data(), PLSC_LEN);
    }
}
BENCHMARK(BM_demap_bpsk);

static void BM_demap_bpsk_diff(benchmark::State& state)
{
    std::vector<gr_complex> bpsk = {
        { -SQRT2_2, SQRT2_2 }, // last SOF symbol
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 }
    };

    for (auto _ : state) {
        gr::dvbs2rx::demap_bpsk_diff(bpsk.data(), PLSC_LEN);
    }
}
BENCHMARK(BM_demap_bpsk_diff);

static void BM_derotate_bpsk(benchmark::State& state)
{
    std::vector<gr_complex> pi2_bpsk = {
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },
        { -SQRT2_2, -SQRT2_2 }, { SQRT2_2, -SQRT2_2 },  { -SQRT2_2, -SQRT2_2 },
        { SQRT2_2, -SQRT2_2 }
    };
    std::vector<float> bpsk(64);

    for (auto _ : state) {
        gr::dvbs2rx::derotate_bpsk(pi2_bpsk.data(), bpsk.data(), PLSC_LEN);
    }
}
BENCHMARK(BM_derotate_bpsk);

static void BM_pl_descrambler(benchmark::State& state)
{
    // Vector with arbitrary QPSK symbols and max length
    int nsyms = MAX_PLFRAME_PAYLOAD;
    std::vector<gr_complex> qpsk_syms(nsyms);
    constexpr gr_complex qpsk_lut[4] = { { SQRT2_2, SQRT2_2 },
                                         { -SQRT2_2, SQRT2_2 },
                                         { -SQRT2_2, -SQRT2_2 },
                                         { SQRT2_2, -SQRT2_2 } };
    for (int i = 0; i < nsyms; i++) {
        qpsk_syms[i] = qpsk_lut[i % 4];
    }
    // Descrambler
    int gold_code = 0;
    gr::dvbs2rx::pl_descrambler pl_descrambler(gold_code);

    for (auto _ : state) {
        pl_descrambler.descramble(qpsk_syms.data(), nsyms);
    }
}
BENCHMARK(BM_pl_descrambler);

static void BM_symbol_sync(benchmark::State& state)
{
    float sps = 2.0;
    float loop_bw = 0.01;
    float damping_factor = 1.0;
    float rolloff = 0.2;
    int rrc_delay = 5;
    int n_subfilt = 128;
    int interp_method = state.range(0);

    int ninput_items = 1025;
    int noutput_items = 512;
    volk::vector<gr_complex> in_buf(ninput_items);
    volk::vector<gr_complex> out_buf(noutput_items);

    gr::dvbs2rx::symbol_sync_cc_impl symbol_sync(
        sps, loop_bw, damping_factor, rolloff, rrc_delay, n_subfilt, interp_method);

    // Run the loop once before the benchmarking loop so that the initialization routine
    // (with an std::vector resize call) does not disturb the benchmarking results
    symbol_sync.loop(in_buf.data(), out_buf.data(), ninput_items, noutput_items);

    for (auto _ : state) {
        symbol_sync.loop(in_buf.data(), out_buf.data(), ninput_items, noutput_items);
    }
}
BENCHMARK(BM_symbol_sync)->DenseRange(0, 3);

BENCHMARK_MAIN();
