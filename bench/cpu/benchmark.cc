#include "pi2_bpsk.h"
#include "pl_defs.h"
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

BENCHMARK_MAIN();
