#include "pi2_bpsk.h"
#include "pl_defs.h"
#include "pl_descrambler.h"
#include <benchmark/benchmark.h>

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
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i)
    };

    for (auto _ : state) {
        gr::dvbs2rx::demap_bpsk(bpsk.data(), PLSC_LEN);
    }
}
BENCHMARK(BM_demap_bpsk);

static void BM_demap_bpsk_diff(benchmark::State& state)
{
    std::vector<gr_complex> bpsk = {
        (-SQRT2_2 + SQRT2_2i), // last SOF symbol
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i)
    };

    for (auto _ : state) {
        gr::dvbs2rx::demap_bpsk_diff(bpsk.data(), PLSC_LEN);
    }
}
BENCHMARK(BM_demap_bpsk_diff);

static void BM_derotate_bpsk(benchmark::State& state)
{
    std::vector<gr_complex> pi2_bpsk = {
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),
        (-SQRT2_2 - SQRT2_2i), (SQRT2_2 - SQRT2_2i),  (-SQRT2_2 - SQRT2_2i),
        (SQRT2_2 - SQRT2_2i)
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
    constexpr gr_complex qpsk_lut[4] = { (+SQRT2_2 + SQRT2_2i),
                                         (-SQRT2_2 + SQRT2_2i),
                                         (-SQRT2_2 - SQRT2_2i),
                                         (+SQRT2_2 - SQRT2_2i) };
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

BENCHMARK_MAIN();
