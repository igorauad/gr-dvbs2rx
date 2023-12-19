#include "pi2_bpsk.h"
#include "pl_defs.h"
#include "pl_descrambler.h"
#include "psk.hh"
#include "qpsk.h"
#include "symbol_sync_cc_impl.h"
#include <benchmark/benchmark.h>
#include <volk/volk_alloc.hh>

void fill_qpsk_syms(volk::vector<gr_complex>& syms)
{
    volk::vector<gr_complex> template_symbols = {
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
    for (const gr_complex& sym : template_symbols) {
        syms.push_back(sym);
    }
}
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
    volk::vector<gr_complex> bpsk;
    fill_qpsk_syms(bpsk);

    for (auto _ : state) {
        gr::dvbs2rx::demap_bpsk(bpsk.data(), PLSC_LEN);
    }
}
BENCHMARK(BM_demap_bpsk);

static void BM_demap_bpsk_diff(benchmark::State& state)
{
    volk::vector<gr_complex> bpsk = {
        { -SQRT2_2, SQRT2_2 }, // last SOF symbol
    };
    fill_qpsk_syms(bpsk);

    for (auto _ : state) {
        gr::dvbs2rx::demap_bpsk_diff(bpsk.data(), PLSC_LEN);
    }
}
BENCHMARK(BM_demap_bpsk_diff);

static void BM_derotate_bpsk(benchmark::State& state)
{
    volk::vector<gr_complex> pi2_bpsk;
    volk::vector<float> bpsk(64);
    fill_qpsk_syms(pi2_bpsk);
    for (auto _ : state) {
        gr::dvbs2rx::derotate_bpsk(pi2_bpsk.data(), bpsk.data(), PLSC_LEN);
    }
}
BENCHMARK(BM_derotate_bpsk);

static void BM_qpsk_demap_soft_new(benchmark::State& state)
{
    gr::dvbs2rx::QpskConstellation qpsk;
    volk::vector<gr_complex> in_syms;
    fill_qpsk_syms(in_syms);
    volk::vector<int8_t> out_llr(in_syms.size() * 2);

    for (auto _ : state) {
        qpsk.demap_soft(out_llr.data(), in_syms, /*N0=*/1.0);
    }
}
BENCHMARK(BM_qpsk_demap_soft_new);

static void BM_qpsk_demap_soft_old(benchmark::State& state)
{
    Modulation<gr_complex, int8_t>* mod = new PhaseShiftKeying<4, gr_complex, int8_t>();
    volk::vector<gr_complex> in_syms;
    fill_qpsk_syms(in_syms);
    volk::vector<int8_t> out_llr(in_syms.size() * 2);
    int8_t* soft = out_llr.data();

    for (auto _ : state) {
        for (int j = 0; j < in_syms.size(); j++) {
            mod->soft(soft + (j * 2), in_syms[j], /*precision=*/1.0);
        }
    }
}
BENCHMARK(BM_qpsk_demap_soft_old);

static void BM_qpsk_post_dec_snr_est_new(benchmark::State& state)
{
    gr::dvbs2rx::QpskConstellation qpsk;
    volk::vector<gr_complex> in_syms;
    fill_qpsk_syms(in_syms);
    volk::vector<int8_t> ref_llrs(2 * in_syms.size());
    qpsk.demap_soft(ref_llrs.data(), in_syms, /*scalar=*/1.0);

    for (auto _ : state) {
        float snr_lin = qpsk.estimate_snr(in_syms, ref_llrs);
    }
}
BENCHMARK(BM_qpsk_post_dec_snr_est_new);

static void BM_qpsk_post_dec_snr_est_old(benchmark::State& state)
{
    Modulation<gr_complex, int8_t>* mod = new PhaseShiftKeying<4, gr_complex, int8_t>();
    gr::dvbs2rx::QpskConstellation qpsk;
    volk::vector<gr_complex> in_syms;
    fill_qpsk_syms(in_syms);
    size_t n_syms = in_syms.size();
    size_t n_bits = in_syms.size() * 2;
    volk::vector<int8_t> ref_llrs(n_bits);
    qpsk.demap_soft(ref_llrs.data(), in_syms, /*scalar=*/1.0);
    volk::vector<int8_t> hard_dec(n_bits);

    for (auto _ : state) {
        for (int j = 0; j < n_bits; j++) {
            hard_dec[j] = ref_llrs[j] < 0 ? -1 : 1;
        }
        float sp = 0;
        float np = 0;
        for (int j = 0; j < n_syms; j++) {
            gr_complex s = mod->map(&hard_dec[j * 2]);
            gr_complex e = in_syms[j] - s;
            sp += std::norm(s);
            np += std::norm(e);
        }
        if (!(np > 0)) {
            np = 1e-12;
        }
        float snr_lin = sp / np;
    }
}
BENCHMARK(BM_qpsk_post_dec_snr_est_old);

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
    gr::dvbs2rx::interp_method_t interp_method =
        static_cast<gr::dvbs2rx::interp_method_t>(state.range(0));

    int ninput_items = 1025;
    int noutput_items = 512;
    volk::vector<gr_complex> in_buf(ninput_items);
    volk::vector<gr_complex> out_buf(noutput_items);

    gr::dvbs2rx::symbol_sync_cc_impl symbol_sync(
        sps, loop_bw, damping_factor, rolloff, rrc_delay, n_subfilt, interp_method);

    // Run the loop once before the benchmarking loop so that the initialization
    // routine (with an std::vector resize call) does not disturb the benchmarking
    // results
    symbol_sync.loop(in_buf.data(), out_buf.data(), ninput_items, noutput_items);

    for (auto _ : state) {
        symbol_sync.loop(in_buf.data(), out_buf.data(), ninput_items, noutput_items);
    }
}
BENCHMARK(BM_symbol_sync)->DenseRange(0, 3);

BENCHMARK_MAIN();
