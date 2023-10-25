#include "Decoder_BCH_DVBS2.hpp"
#include "Encoder_BCH_DVBS2.hpp"
#include "gr_bch.h"
#include <aff3ct.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <vector>

using namespace aff3ct;
namespace po = boost::program_options;

enum CodecImpl { AFF3CT_IMPL = 0, GR_DVBS2RX_IMPL = 1, NEW_IMPL = 2 };
std::map<int, std::string> codec_impl_map = { { AFF3CT_IMPL, "aff3ct" },
                                              { GR_DVBS2RX_IMPL, "gr-dvbs2rx" },
                                              { NEW_IMPL, "new" } };
std::string get_impl_options(const std::string& name)
{
    std::string options = name + " implementation: ";
    for (auto& kv : codec_impl_map) {
        options += kv.second + " (" + std::to_string(kv.first) + "), ";
    }
    return options;
}

void set_aff3ct_gen_poly(int N,
                         int t,
                         std::unique_ptr<tools::BCH_polynomial_generator<>>& p_gen_poly)
{
    std::vector<int> bch_prim_poly;
    if (N < 16200) { // for short FECFRAME
        bch_prim_poly = {
            1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1
        }; // g1(x) from Table 6b
    } else {
        bch_prim_poly = {
            1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
        }; // g1(x) from Table 6a
    }
    int N_p2_1 = tools::next_power_of_2(N) - 1;
    p_gen_poly.reset(new tools::BCH_polynomial_generator<int>(N_p2_1, t, bch_prim_poly));
}

struct BchEncoder {
private:
    int m_impl; // Decoder implementation
    std::unique_ptr<gr::dvbs2::NewBchCodec> m_new_encoder;
    std::unique_ptr<gr::dvbs2::GrBchEncoder> m_gr_encoder;
    std::unique_ptr<module::Encoder_BCH_DVBS2<>> m_aff3ct_encoder;
    std::unique_ptr<tools::BCH_polynomial_generator<>> m_aff3ct_gen_poly;

public:
    /**
     * @brief Construct a new BCH Encoder object.
     *
     * The GR implementation was copied from gr-dtv/lib/dvb/dvb_bch_bb_impl.cc.
     *
     * @param impl Decoder implementation: 0 for AFF3CT and 1 for GNU Radio (GR).
     * @param K Message length in bits.
     * @param N Codeword length in bits.
     * @param t Error correction capability.
     */
    BchEncoder(int impl, int K, int N, int t) : m_impl(impl)
    {
        if (m_impl == AFF3CT_IMPL) {
            set_aff3ct_gen_poly(N, t, m_aff3ct_gen_poly);
            m_aff3ct_encoder.reset(
                new module::Encoder_BCH_DVBS2<>(K, N, *m_aff3ct_gen_poly.get()));
        } else if (m_impl == GR_DVBS2RX_IMPL) {
            m_gr_encoder.reset(new gr::dvbs2::GrBchEncoder(K, N, t));
        } else if (m_impl == NEW_IMPL) {
            m_new_encoder.reset(new gr::dvbs2::NewBchCodec(N, t));
        }
    }

    void encode(const std::vector<int>& ref_bits, std::vector<int>& enc_bits)
    {
        if (m_impl == AFF3CT_IMPL) {
            m_aff3ct_encoder->encode(ref_bits, enc_bits);
        } else if (m_impl == GR_DVBS2RX_IMPL) {
            m_gr_encoder->encode(ref_bits, enc_bits);
        } else if (m_impl == NEW_IMPL) {
            m_new_encoder->encode(ref_bits, enc_bits);
        }
    }
};

struct BchDecoder {
private:
    int m_K;    // Message length in bits.
    int m_N;    // Codeword length in bits.
    int m_impl; // Decoder implementation
    std::unique_ptr<gr::dvbs2::NewBchCodec> m_new_encoder;
    std::unique_ptr<gr::dvbs2::GrBchDecoder> m_gr_decoder;
    std::unique_ptr<module::Decoder_BCH_DVBS2<>> m_aff3ct_std_decoder;
    std::unique_ptr<tools::BCH_polynomial_generator<>> m_aff3ct_gen_poly;
    std::vector<int> m_hard_dec;

public:
    /**
     * @brief Construct a new BCH Decoder object.
     *
     * @param impl Decoder implementation: 0 for AFF3CT and 1 for gr-dvbs2rx.
     * @param K Message length in bits.
     * @param N Codeword length in bits.
     * @param t Error correction capability.
     */
    BchDecoder(int impl, int K, int N, int t)
        : m_impl(impl), m_K(K), m_N(N), m_hard_dec(N)
    {
        if (m_impl == AFF3CT_IMPL) {
            set_aff3ct_gen_poly(N, t, m_aff3ct_gen_poly);
            m_aff3ct_std_decoder.reset(
                new module::Decoder_BCH_DVBS2<>(K, N, *m_aff3ct_gen_poly.get()));
        } else if (m_impl == GR_DVBS2RX_IMPL) {
            m_gr_decoder.reset(new gr::dvbs2::GrBchDecoder(K, N, t));
        } else if (m_impl == NEW_IMPL) {
            m_new_encoder.reset(new gr::dvbs2::NewBchCodec(N, t));
        }
    }

    void decode(const std::vector<float>& llr_vec, std::vector<int>& dec_bits)
    {
        // Convert the LLR vector into hard decisions. Assume the BCH decoder would take
        // hard decisions output by the LDPC decoder even though there is no LDPC block in
        // this setup. Importantly, make the same assumption for all implementations.
        tools::hard_decide(llr_vec.data(), m_hard_dec.data(), m_N);

        if (m_impl == AFF3CT_IMPL) {
            m_aff3ct_std_decoder->decode_hiho(m_hard_dec, dec_bits);
        } else if (m_impl == GR_DVBS2RX_IMPL) {
            m_gr_decoder->decode(m_hard_dec, dec_bits);
        } else if (m_impl == NEW_IMPL) {
            m_new_encoder->decode(m_hard_dec, dec_bits);
        }
    }
};

struct params {
    int N;            // codeword size
    int K;            // number of information bits
    int t;            // t-error correction capability
    int fe;           // target frame errors
    int max_n_frames; // max frames to simulate per ebn0
    float ebn0_min;   // minimum SNR value
    float ebn0_max;   // maximum SNR value
    float ebn0_step;  // SNR step
    int enc_impl;     // Encoder implementation
    int dec_impl;     // Decoder implementation
    float R;          // code rate (R=K/N)
    int seed = 0;     // PRNG seed for the AWGN channel

    params(int N,
           int K,
           int t,
           int fe,
           int max_n_frames,
           float ebn0_min,
           float ebn0_max,
           float ebn0_step,
           int enc_impl,
           int dec_impl)
        : N(N),
          K(K),
          t(t),
          fe(fe),
          max_n_frames(max_n_frames),
          ebn0_min(ebn0_min),
          ebn0_max(ebn0_max),
          ebn0_step(ebn0_step),
          enc_impl(enc_impl),
          dec_impl(dec_impl),
          R((float)K / (float)N)
    {
        if (codec_impl_map.find(enc_impl) == codec_impl_map.end())
            throw std::runtime_error("Unsupported encoder implementation");

        if (codec_impl_map.find(dec_impl) == codec_impl_map.end())
            throw std::runtime_error("Unsupported decoder implementation");

        std::cout << "# * Parameters: " << std::endl;
        std::cout << "#    ** Frame errors   = " << fe << std::endl;
        std::cout << "#    ** Max frames     = " << max_n_frames << std::endl;
        std::cout << "#    ** Noise seed     = " << seed << std::endl;
        std::cout << "#    ** Info. bits (K) = " << K << std::endl;
        std::cout << "#    ** Frame size (N) = " << N << std::endl;
        std::cout << "#    ** Err. Corr. (t) = " << t << std::endl;
        std::cout << "#    ** Code rate  (R) = " << R << std::endl;
        std::cout << "#    ** SNR min   (dB) = " << ebn0_min << std::endl;
        std::cout << "#    ** SNR max   (dB) = " << ebn0_max << std::endl;
        std::cout << "#    ** SNR step  (dB) = " << ebn0_step << std::endl;
        std::cout << "#" << std::endl;
    }
};

struct modules {
    std::unique_ptr<module::Source_random<>> source;
    std::unique_ptr<BchEncoder> encoder;
    std::unique_ptr<module::Modem_BPSK<>> modem;
    std::unique_ptr<module::Channel_AWGN_LLR<>> channel;
    std::unique_ptr<BchDecoder> decoder;
    std::unique_ptr<module::Monitor_BFER<>> monitor;
};

struct buffers {
    std::vector<int> ref_bits;
    std::vector<int> enc_bits;
    std::vector<float> symbols;
    std::vector<float> sigma;
    std::vector<float> noisy_symbols;
    std::vector<float> LLRs;
    std::vector<int> dec_bits;
};

struct utils {
    std::unique_ptr<tools::Sigma<>> noise; // a sigma noise type
    std::vector<std::unique_ptr<tools::Reporter>>
        reporters; // list of reporters dispayed in the terminal
    std::unique_ptr<tools::Terminal_std>
        terminal; // manage the output text in the terminal
};

void init_modules(const params& p, modules& m);
void init_buffers(const params& p, buffers& b);
void init_utils(const modules& m, utils& u);

int parse_opts(int ac, char* av[], po::variables_map& vm)
{
    try {
        po::options_description desc("Program options");
        desc.add_options()("help,h", "produce help message")(
            "n", po::value<int>()->default_value(9720), "Codeword length.")(
            "k", po::value<int>()->default_value(9552), "Message length.")(
            "t", po::value<int>()->default_value(12), "Error correction capability.")(
            "fe",
            po::value<int>()->default_value(1e2),
            "Max number of frame errors to simulate per Eb/N0.")(
            "nframes",
            po::value<int>()->default_value(1e7),
            "Max number of frames to simulate per Eb/N0.")(
            "ebn0-min", po::value<float>()->default_value(0), "Starting Eb/N0 in dB.")(
            "ebn0-max", po::value<float>()->default_value(10), "Ending Eb/N0 in dB.")(
            "ebn0-step", po::value<float>()->default_value(1), "Eb/N0 step in dB.")(
            "enc",
            po::value<int>()->default_value(0),
            get_impl_options("Encoder").c_str())("dec",
                                                 po::value<int>()->default_value(0),
                                                 get_impl_options("Decoder").c_str());

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

    params p(args["n"].as<int>(),
             args["k"].as<int>(),
             args["t"].as<int>(),
             args["fe"].as<int>(),
             args["nframes"].as<int>(),
             args["ebn0-min"].as<float>(),
             args["ebn0-max"].as<float>(),
             args["ebn0-step"].as<float>(),
             args["enc"].as<int>(),
             args["dec"].as<int>());
    modules m;
    buffers b;
    utils u;
    init_modules(p, m); // create and initialize the modules
    init_buffers(p, b); // create and initialize the buffers required by the modules
    init_utils(m, u);   // create and initialize the utils

    // display the legend in the terminal
    u.terminal->legend();

    // loop over the various SNRs
    for (auto ebn0 = p.ebn0_min; ebn0 < p.ebn0_max; ebn0 += p.ebn0_step) {
        // compute the current sigma for the channel noise
        const auto esn0 = tools::ebn0_to_esn0(ebn0, p.R);
        std::fill(b.sigma.begin(),
                  b.sigma.end(),
                  tools::esn0_to_sigma(esn0)); // do we need a vector for this?

        u.noise->set_values(b.sigma[0], ebn0, esn0);

        // display the performance (BER and FER) in real time (in a separate thread)
        u.terminal->start_temp_report();

        // run the simulation chain
        while (!m.monitor->fe_limit_achieved() && !m.monitor->frame_limit_achieved() &&
               !u.terminal->is_interrupt()) {
            m.source->generate(b.ref_bits);
            m.encoder->encode(b.ref_bits, b.enc_bits);
            m.modem->modulate(b.enc_bits, b.symbols);
            m.channel->add_noise(b.sigma, b.symbols, b.noisy_symbols);
            m.modem->demodulate(b.sigma, b.noisy_symbols, b.LLRs);
            m.decoder->decode(b.LLRs, b.dec_bits);
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

void init_modules(const params& p, modules& m)
{
    m.source.reset(new module::Source_random<>(p.K));
    m.encoder.reset(new BchEncoder(p.enc_impl, p.K, p.N, p.t));
    m.modem.reset(new module::Modem_BPSK<>(p.N));
    m.channel.reset(new module::Channel_AWGN_LLR<>(p.N));
    m.decoder.reset(new BchDecoder(p.dec_impl, p.K, p.N, p.t));
    m.monitor.reset(new module::Monitor_BFER<>(p.K, p.fe, p.max_n_frames));
    m.channel->set_seed(p.seed);

    // TODO double check
    // m.encoder->set_n_frames(1);
    // m.decoder->set_n_frames(1);
};

void init_buffers(const params& p, buffers& b)
{
    b.ref_bits = std::vector<int>(p.K);
    b.enc_bits = std::vector<int>(p.N);
    b.symbols = std::vector<float>(p.N);
    b.sigma = std::vector<float>(1);
    b.noisy_symbols = std::vector<float>(p.N);
    b.LLRs = std::vector<float>(p.N);
    b.dec_bits = std::vector<int>(p.K);
}

void init_utils(const modules& m, utils& u)
{
    // create a sigma noise type
    u.noise.reset(new tools::Sigma<>());
    // report the noise values (Es/N0 and Eb/N0)
    u.reporters.push_back(
        std::unique_ptr<tools::Reporter>(new tools::Reporter_noise<>(*u.noise)));
    // report the bit/frame error rates
    u.reporters.push_back(
        std::unique_ptr<tools::Reporter>(new tools::Reporter_BFER<>(*m.monitor)));
    // report the simulation throughputs
    u.reporters.push_back(
        std::unique_ptr<tools::Reporter>(new tools::Reporter_throughput<>(*m.monitor)));
    // create a terminal that will display the collected data from the reporters
    u.terminal.reset(new tools::Terminal_std(u.reporters));
}