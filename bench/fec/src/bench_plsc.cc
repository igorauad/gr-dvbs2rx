#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <gnuradio/expj.h>
#include <gnuradio/gr_complex.h>
#include <aff3ct.hpp>
#include <pl_signaling.h>
#include <boost/program_options.hpp>
using namespace aff3ct;
namespace po = boost::program_options;

struct params {
    int K = 7;       // number of information bits
    int N = 64;      // codeword size
    int seed = 0;    // PRNG seed for the AWGN channel
    int fe;          // number of frame errors
    int n_frames;    // max number of frames to simulate per ebn0
    float ebn0_min;  // minimum SNR value
    float ebn0_max;  // maximum SNR value
    float ebn0_step; // SNR step
    float foffset;   // Normalized symbol-spaced frequency offset
    bool coherent;   // Coherent pi/2 BPSK demapping
    bool soft_dec;   // Whether to use soft decoding
    float R;         // code rate (R=K/N)

    params(int fe,
           int n_frames,
           float ebn0_min,
           float ebn0_max,
           float ebn0_step,
           float foffset,
           bool coherent,
           bool soft)
        : fe(fe),
          n_frames(n_frames),
          ebn0_min(ebn0_min),
          ebn0_max(ebn0_max),
          ebn0_step(ebn0_step),
          foffset(foffset),
          coherent(coherent),
          soft_dec(soft)
    {
        R = (float)K / (float)(N); // Code rate
        // NOTE: the code rate is assumed equal to the spectral efficiency, as
        // the tools::ebn0_to_esn0 function assumes "EsN0 = EbN0 * R". With
        // 2-PAM mapping in baseband, the spectral efficiency of a block code is
        // actually 2K/N (see Section 6.3 on Forney's book), because there are K
        // bits for every N/2 pairs of real dimensions (note the spectral
        // efficiency is in units of bits/2D). In contrast, when considering
        // pi/2 BPSK in passband, we have K bits for every N complex
        // dimensions. Hence, the spectral efficiency is really K/N.
        std::cout << "# * Simulation parameters: " << std::endl;
        std::cout << "#    ** Frame errors   = " << fe << std::endl;
        std::cout << "#    ** Max frames     = " << n_frames << std::endl;
        std::cout << "#    ** Noise seed     = " << seed << std::endl;
        std::cout << "#    ** Info. bits (K) = " << K << std::endl;
        std::cout << "#    ** Frame size (N) = " << N << std::endl;
        std::cout << "#    ** Code rate  (R) = " << R << std::endl;
        std::cout << "#    ** SNR min   (dB) = " << ebn0_min << std::endl;
        std::cout << "#    ** SNR max   (dB) = " << ebn0_max << std::endl;
        std::cout << "#    ** SNR step  (dB) = " << ebn0_step << std::endl;
        std::cout << "#    ** Freq. offset   = " << foffset << std::endl;
        std::cout << "#    ** Coherent demap = " << coherent << std::endl;
        std::cout << "#    ** Soft decoding  = " << soft_dec << std::endl;
        std::cout << "#" << std::endl;
    };
};

struct AwgnChannel {
    float freq_offset = 0;   // normalized frequency offset
    std::random_device seed; // random seed generator
    std::mt19937 prgn;       // pseudo-random number engine
    std::normal_distribution<float> normal_dist_gen;

    void set_esn0(double esn0_db)
    {
        constexpr float es = 1; // asssume unitary Es
        double esn0 = pow(10, (esn0_db / 10));
        float n0 = es / esn0;
        // n0 is the variance of the complex AWGN noise. Since the noise is
        // zero-mean, its variance is equal to E[|noise|^2]. In other words,
        // E[|noise|^2]=N0. However, note the noise can be expressed as
        // "alpha*(norm_re + j*norm_im)", where norm_re and norm_im are
        // independent normal random variables, and alpha is a scaling factor
        // determining the standard deviation per dimension. Hence,
        //
        // E[|noise|^2] = (alpha^2)*(E[|noise_re|^2] + E[|noise_im|^2])
        //              = (alpha^2)*(2)
        //           N0 = 2 * alpha^2.
        //
        // Thus, in order to generate complex noise with variance N0, the
        // scaling factor should be "alpha = sqrt(N0/2)."
        float sdev_per_dim = sqrt(n0 / 2);
        normal_dist_gen = std::normal_distribution<float>(0, sdev_per_dim);
    }

    explicit AwgnChannel(double esn0_db, float foffset)
        : freq_offset(foffset), prgn(seed())
    {
        set_esn0(esn0_db);
    }

    void add_noise(gr_complex* in, unsigned int n_symbols)
    {
        for (size_t i = 0; i < n_symbols; i++) {
            in[i] += gr_complex(normal_dist_gen(prgn), normal_dist_gen(prgn));
        }
    }

    // Memoryless rotation: rotate the given symbols and do not keep track of
    // the phase between calls.
    void rotate(gr_complex* out, const gr_complex* in, unsigned int n_symbols)
    {
        const gr_complex phase_inc = gr_expj(2 * M_PI * freq_offset);
        gr_complex phase = gr_expj(0);
        volk_32fc_s32fc_x2_rotator_32fc(out, in, phase_inc, &phase, n_symbols);
    }
};

struct modules {
    std::unique_ptr<gr::dvbs2rx::plsc_encoder> encoder;
    std::unique_ptr<gr::dvbs2rx::plsc_decoder> decoder;
    std::unique_ptr<module::Monitor_BFER<>> monitor;
    std::unique_ptr<AwgnChannel> channel;
};

struct buffers {
    std::vector<int> ref_bits;             // reference/input bits
    std::vector<int> dec_bits;             // decoded bits
    volk::vector<gr_complex> tx_bpsk_syms; // Tx (clean) PLSC BPSK symbols
    volk::vector<gr_complex> rx_bpsk_syms; // Rx (noisy) PLSC BPSK symbols
};

struct utils {
    std::unique_ptr<tools::Sigma<>> noise; // a sigma noise type
    std::vector<std::unique_ptr<tools::Reporter>>
        reporters; // list of reporters displayed on the terminal
    std::unique_ptr<tools::Terminal_std>
        terminal; // manage the output text on the terminal
};

void init_modules(const params& p, modules& m)
{
    m.encoder = std::unique_ptr<gr::dvbs2rx::plsc_encoder>(new gr::dvbs2rx::plsc_encoder);
    m.decoder = std::unique_ptr<gr::dvbs2rx::plsc_decoder>(new gr::dvbs2rx::plsc_decoder);
    m.monitor = std::unique_ptr<module::Monitor_BFER<>>(
        new module::Monitor_BFER<>(p.K, p.fe, p.n_frames));
    m.channel = std::unique_ptr<AwgnChannel>(new AwgnChannel(p.ebn0_min, p.foffset));
};

void init_buffers(const params& p, buffers& b)
{
    b.ref_bits = std::vector<int>(p.K);
    b.dec_bits = std::vector<int>(p.K);
    b.tx_bpsk_syms = volk::vector<gr_complex>(PLSC_LEN + 1);
    b.rx_bpsk_syms = volk::vector<gr_complex>(PLSC_LEN + 1);
    // Note: the extra BPSK symbol allows for differential detection.
}

void init_utils(const modules& m, utils& u)
{
    // create a sigma noise type
    u.noise = std::unique_ptr<tools::Sigma<>>(new tools::Sigma<>());
    // report the noise values (Es/N0 and Eb/N0)
    u.reporters.push_back(
        std::unique_ptr<tools::Reporter>(new tools::Reporter_noise<>(*u.noise)));
    // report the bit/frame error rates
    u.reporters.push_back(
        std::unique_ptr<tools::Reporter>(new tools::Reporter_BFER<>(*m.monitor)));
    // report the simulation throughputs
    u.reporters.push_back(
        std::unique_ptr<tools::Reporter>(new tools::Reporter_throughput<>(*m.monitor)));
    // create a terminal to display the data collected from the reporters
    u.terminal =
        std::unique_ptr<tools::Terminal_std>(new tools::Terminal_std(u.reporters));
}

void unpack_plsc_bits(uint8_t plsc, uint8_t dec_plsc, buffers& b)
{
    // From a packed uint8 to a vector of ints
    for (size_t i = 0; i < 7; i++) {
        b.ref_bits[i] = (plsc >> i) & 1;
        b.dec_bits[i] = (dec_plsc >> i) & 1;
    }
}

void plsc_loopback(modules& m, buffers& b, const params& p)
{
    // Pick a random PLSC
    uint8_t plsc = rand() % 128;

    // Encode and map to pi/2 BPSK symbols
    b.tx_bpsk_syms[0] = { -SQRT2_2, SQRT2_2 };          // last SOF symbol
    m.encoder->encode(b.tx_bpsk_syms.data() + 1, plsc); // PLSC symbols

    // Add noise and rotate the 65 symbols (the last SOF symbol and the PLSC
    // symbols). The last SOF symbol is only used with differential pi/2 BPSK
    // demapping. Nevertheless, add noise and rotate this symbol regardless so
    // that both demapping approaches (coherent and differential) take about the
    // same amount of prep work outside the main function under test, which is
    // the decoder->decode function.
    m.channel->rotate(b.rx_bpsk_syms.data(), b.tx_bpsk_syms.data(), PLSC_LEN + 1);
    m.channel->add_noise(b.rx_bpsk_syms.data(), PLSC_LEN + 1);

    // Decode the noisy Rx pi/2 BPSK symbols
    m.decoder->decode(b.rx_bpsk_syms.data(), p.coherent, p.soft_dec);

    // Unpack the PLSC bits
    unpack_plsc_bits(plsc, m.decoder->d_plsc, b);
}

int parse_opts(int ac, char* av[], po::variables_map& vm)
{
    try {
        po::options_description desc("Program options");
        desc.add_options()("help,h", "produce help message")(
            "fe",
            po::value<int>()->default_value(1e2),
            "Max number of frame errors to simulate per Eb/N0.")(
            "nframes",
            po::value<int>()->default_value(1e7),
            "Max number of frames to simulate per Eb/N0.")(
            "ebn0-min", po::value<float>()->default_value(0), "Starting Eb/N0 in dB.")(
            "ebn0-max", po::value<float>()->default_value(10), "Ending Eb/N0 in dB.")(
            "ebn0-step", po::value<float>()->default_value(1), "Eb/N0 step in dB.")(
            "foffset",
            po::value<float>()->default_value(0),
            "Normalized frequency offset.")(
            "differential",
            po::bool_switch(),
            "Try differential pi/2 BPSK demapping instead of coherent.")(
            "hard",
            po::bool_switch(),
            "Try with hard pi/2 BPSK decisions instead of soft decisions.");

        po::store(po::parse_command_line(ac, av, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 0;
        }
    } catch (exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return -1;
    } catch (...) {
        std::cerr << "Exception of unknown type!\n";
    }

    return 1;
}

int main(int argc, char** argv)
{
    po::variables_map args;
    int opt_parser_res = parse_opts(argc, argv, args);
    if (opt_parser_res < 1)
        return opt_parser_res;

    std::cout << "#----------------------------------------------------------"
              << std::endl;
    std::cout << "# PLSC decoding BER vs. SNR benchmark" << std::endl;
    std::cout << "#----------------------------------------------------------"
              << std::endl;
    std::cout << "#" << std::endl;

    // create and initialize the parameters defined by the user
    params p(args["fe"].as<int>(),
             args["nframes"].as<int>(),
             args["ebn0-min"].as<float>(),
             args["ebn0-max"].as<float>(),
             args["ebn0-step"].as<float>(),
             args["foffset"].as<float>(),
             !args["differential"].as<bool>(),
             !args["hard"].as<bool>());

    modules m;
    init_modules(p, m); // create and initialize the modules
    buffers b;
    init_buffers(p, b); // create and initialize the buffers required by the modules
    utils u;
    init_utils(m, u); // create and initialize the utils

    // display the legend in the terminal
    u.terminal->legend();

    // loop over the various SNRs
    for (auto ebn0 = p.ebn0_min; ebn0 < p.ebn0_max; ebn0 += p.ebn0_step) {
        // compute the current sigma for the channel noise
        const auto esn0 = tools::ebn0_to_esn0(ebn0, p.R);
        const auto sigma = tools::esn0_to_sigma(esn0);

        u.noise->set_values(sigma, ebn0, esn0);

        // update the Es/N0 generated by the AWGN channel
        m.channel->set_esn0(esn0);

        // display the performance (BER and FER) in real time (in a separate thread)
        u.terminal->start_temp_report();

        // run the simulation chain
        while (!m.monitor->fe_limit_achieved() && !m.monitor->frame_limit_achieved() &&
               !u.terminal->is_interrupt()) {
            plsc_loopback(m, b, p);
            m.monitor->check_errors(b.dec_bits, b.ref_bits);
        }

        // display the performance (BER and FER) in the terminal
        u.terminal->final_report();

        // reset the monitor for the next SNR
        m.monitor->reset();
        u.terminal->reset();

        // if user pressed Ctrl+c twice, exit the SNRs loop
        if (u.terminal->is_over())
            break;
    }

    std::cout << "# End of the simulation" << std::endl;

    return 0;
}
