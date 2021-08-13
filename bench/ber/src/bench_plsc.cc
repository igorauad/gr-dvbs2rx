#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <gnuradio/gr_complex.h>
#include <aff3ct.hpp>
#include <pl_signaling.h>
#include <boost/program_options.hpp>
using namespace aff3ct;
namespace po = boost::program_options;

struct params {
    int K = 7;          // number of information bits
    int N = 64;         // codeword size
    int fe = 100;       // number of frame errors
    int n_frames = 1e7; // max number of frames to simulate per ebn0
    int seed = 0;       // PRNG seed for the AWGN channel
    float ebn0_min;     // minimum SNR value
    float ebn0_max;     // maximum SNR value
    float ebn0_step;    // SNR step
    float R;            // code rate (R=K/N)

    params(float ebn0_min, float ebn0_max, float ebn0_step)
        : ebn0_min(ebn0_min), ebn0_max(ebn0_max), ebn0_step(ebn0_step)
    {
        R = (float)K / (float)N;
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
        std::cout << "#" << std::endl;
    };
};

struct AwgnChannel {
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

    explicit AwgnChannel(double esn0_db) : prgn(seed()) { set_esn0(esn0_db); }

    gr_complex noise()
    {
        return gr_complex(normal_dist_gen(prgn), normal_dist_gen(prgn));
    }
};

struct modules {
    std::unique_ptr<gr::dvbs2rx::plsc_encoder> encoder;
    std::unique_ptr<gr::dvbs2rx::plsc_decoder> decoder;
    std::unique_ptr<module::Monitor_BFER<>> monitor;
    std::unique_ptr<AwgnChannel> channel;
};

struct buffers {
    std::vector<int> ref_bits;         // reference/input bits
    std::vector<int> dec_bits;         // decoded bits
    std::vector<gr_complex> bpsk_syms; // PLSC BPSK symbols
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
    m.channel = std::unique_ptr<AwgnChannel>(new AwgnChannel(p.ebn0_min));
};

void init_buffers(const params& p, buffers& b)
{
    b.ref_bits = std::vector<int>(p.K);
    b.dec_bits = std::vector<int>(p.K);
    b.bpsk_syms = std::vector<gr_complex>(PLSC_LEN + 1);
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

void plsc_loopback_coherent(modules& m, buffers& b)
{
    // Pick a random PLSC
    uint8_t plsc = rand() % 128;

    // Encode and map to pi/2 BPSK symbols
    m.encoder->encode(b.bpsk_syms.data() + 1, plsc); // PLSC symbols only

    // Add noise over the PLSC symbols. Skip the first symbol, which represents
    // the last SOF symbol and is only used when detecting differentially.
    for (size_t i = 1; i < PLSC_LEN + 1; i++) {
        b.bpsk_syms[i] += m.channel->noise();
    }

    // Decode the noisy pi/2 BPSK symbols coherently
    m.decoder->decode(b.bpsk_syms.data());

    // Unpack the PLSC bits
    unpack_plsc_bits(plsc, m.decoder->dec_plsc, b);
}

void plsc_loopback_differential(modules& m, buffers& b)
{
    // Pick a random PLSC
    uint8_t plsc = rand() % 128;

    // Encode and map to pi/2 BPSK symbols
    b.bpsk_syms[0] = (-SQRT2_2 + SQRT2_2i);          // last SOF symbol
    m.encoder->encode(b.bpsk_syms.data() + 1, plsc); // PLSC symbols

    // Add noise over the 65 symbols (the last SOF symbol and the PLSC symbols)
    for (size_t i = 0; i < PLSC_LEN + 1; i++) {
        b.bpsk_syms[i] += m.channel->noise();
    }

    // Decode the noisy pi/2 BPSK symbols differentially
    m.decoder->decode(b.bpsk_syms.data(), false /* non-coherent */);

    // Unpack the PLSC bits
    unpack_plsc_bits(plsc, m.decoder->dec_plsc, b);
}

int parse_opts(int ac, char* av[], po::variables_map& vm)
{
    try {
        po::options_description desc("Program options");
        desc.add_options()("help,h", "produce help message")(
            "ebn0-min", po::value<float>()->default_value(0), "Etarting Eb/N0 in dB")(
            "ebn0-max", po::value<float>()->default_value(10), "Ending Eb/N0 in dB")(
            "ebn0-step", po::value<float>()->default_value(1), "Eb/N0 step in dB")(
            "differential",
            po::bool_switch(),
            "try differential detection instead of coherent");

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
    params p(args["ebn0-min"].as<float>(),
             args["ebn0-max"].as<float>(),
             args["ebn0-step"].as<float>());

    // Decide which PLSC loopback encoder-decoder wrapper to use (with coherent
    // or differential pi/2 BPSK de-mapping)
    void (*plsc_loopback)(modules & m, buffers & b);
    bool differential_demapping = args["differential"].as<bool>();
    if (differential_demapping) {
        std::cout << "# * pi/2 BPSK de-mapping: differential\n#" << std::endl;
        plsc_loopback = &plsc_loopback_differential;
    } else {
        std::cout << "# * pi/2 BPSK de-mapping: coherent\n#" << std::endl;
        plsc_loopback = &plsc_loopback_coherent;
    }

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

        u.noise->set_noise(sigma, ebn0, esn0);

        // update the Es/N0 generated by the AWGN channel
        m.channel->set_esn0(esn0);

        // display the performance (BER and FER) in real time (in a separate thread)
        u.terminal->start_temp_report();

        // run the simulation chain
        while (!m.monitor->fe_limit_achieved() && !m.monitor->frame_limit_achieved() &&
               !u.terminal->is_interrupt()) {
            plsc_loopback(m, b);
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