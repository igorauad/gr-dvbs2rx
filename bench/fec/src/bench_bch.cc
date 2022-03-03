#include "bose_chaudhuri_hocquenghem_decoder.hh"
#include "galois_field.hh"
#include <aff3ct.hpp>
#include <boost/program_options.hpp>
#include <bitset>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace aff3ct;
namespace po = boost::program_options;

typedef CODE::GaloisField<16, 0b10000000000101101, uint16_t> GF_NORMAL;
typedef CODE::GaloisField<15, 0b1000000000101101, uint16_t> GF_MEDIUM;
typedef CODE::GaloisField<14, 0b100000000101011, uint16_t> GF_SHORT;
typedef CODE::BoseChaudhuriHocquenghemDecoder<24, 1, 65343, GF_NORMAL> BCH_NORMAL_12;
typedef CODE::BoseChaudhuriHocquenghemDecoder<20, 1, 65375, GF_NORMAL> BCH_NORMAL_10;
typedef CODE::BoseChaudhuriHocquenghemDecoder<16, 1, 65407, GF_NORMAL> BCH_NORMAL_8;
typedef CODE::BoseChaudhuriHocquenghemDecoder<24, 1, 32587, GF_MEDIUM> BCH_MEDIUM_12;
typedef CODE::BoseChaudhuriHocquenghemDecoder<24, 1, 16215, GF_SHORT> BCH_SHORT_12;

#define MAX_BCH_PARITY_BITS 192

template <typename T>
void bit_vector_to_bitset(const std::vector<T>& in_vec,
                          std::bitset<MAX_BCH_PARITY_BITS>& out_bitset,
                          unsigned int len)
{
    for (unsigned int i = 0; i < len; i++)
        out_bitset[i] = in_vec[i];
}

struct BchEncoder {
private:
    int m_K;      // Message length in bits.
    int m_N;      // Codeword length in bits.
    int m_t;      // t-error correction capability
    int m_impl;   // Decoder implementation
    int m_parity; // Parity bits
    std::unique_ptr<module::Encoder_BCH<>> m_aff3ct_encoder;
    std::bitset<MAX_BCH_PARITY_BITS> m_crc_table[256];
    std::bitset<MAX_BCH_PARITY_BITS> m_gen_poly;

    void multiply_poly(const std::vector<uint8_t>& in_a,
                       const std::vector<uint8_t>& in_b,
                       std::vector<uint8_t>& out)
    {
        int len_a = in_a.size();
        int len_b = in_b.size();
        out.clear();
        out.resize(len_a + len_b - 1);
        for (int i = 0; i < len_a; i++) {
            for (int j = 0; j < len_b; j++) {
                out[i + j] ^= in_a[i] & in_b[j];
            }
        }
    }

    void compute_gen_poly(bool normal_fecframe)
    {
        // Normal FECFRAME minimal polynomials (Table 6a)
        const std::vector<std::vector<uint8_t>> normal_min_poly = {
            { 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, // g1(x)
            { 1, 1, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1 }, // g2(x)
            { 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1 }, // g3(x)
            { 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1 }, // g4(x)
            { 1, 1, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1 }, // g5(x)
            { 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1 }, // g6(x)
            { 1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1 }, // g7(x)
            { 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 1 }, // g8(x)
            { 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1 }, // g9(x)
            { 1, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1 }, // g10(x)
            { 1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1 }, // g11(x)
            { 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1 }  // g12(x)
        };

        // Short FECFRAME minimal polynomials (Table 6b)
        const std::vector<std::vector<uint8_t>> short_min_poly = {
            { 1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, // g1(x)
            { 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1 }, // g2(x)
            { 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1 }, // g3(x)
            { 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1 }, // g4(x)
            { 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1 }, // g5(x)
            { 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1 }, // g6(x)
            { 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1 }, // g7(x)
            { 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1 }, // g8(x)
            { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1 }, // g9(x)
            { 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1 }, // g10(x)
            { 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1 }, // g11(x)
            { 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 0, 1, 1 }  // g12(x)
        };

        const std::vector<std::vector<uint8_t>> min_poly =
            normal_fecframe ? normal_min_poly : short_min_poly;

        // The generator polynomial is the product of the first t minimal polynomials
        std::vector<uint8_t> gen_poly_vec = { 1 };
        for (unsigned int i = 0; i < m_t; i++) {
            std::vector<uint8_t> prev_gen_poly_vec = gen_poly_vec;
            multiply_poly(min_poly[i], prev_gen_poly_vec, gen_poly_vec);
        }
        assert(gen_poly_vec.size() == m_parity + 1);

        // Convert polynomial vector to bitset
        bit_vector_to_bitset(gen_poly_vec, m_gen_poly, m_parity);
    }

    void compute_crc_table(void)
    {
        // See http://www.sunshine2k.de/articles/coding/crc/understanding_crc.html
        for (int dividend = 0; dividend < 256; dividend++) {
            std::bitset<MAX_BCH_PARITY_BITS> shift_reg(dividend);
            shift_reg <<= m_parity - 8; // put dividend byte on the register's MSB
            for (unsigned char bit = 0; bit < 8; bit++) {
                if (shift_reg[m_parity - 1]) {
                    shift_reg <<= 1;
                    shift_reg ^= m_gen_poly;
                } else {
                    shift_reg <<= 1;
                }
            }
            m_crc_table[dividend] = shift_reg;
        }
    }

    void gr_encode(const std::vector<int>& ref_bits, std::vector<int>& enc_bits)
    {
        const int* in = ref_bits.data();
        int* out = enc_bits.data();
        std::bitset<MAX_BCH_PARITY_BITS> crc; // parity bits
        // NOTE: call the parity bits the CRC for brevity. The computations of cyclic
        // redundancy check (CRC) and parity bits of a cyclic code are equivalent.

        // Systematic bits
        memcpy(out, in, sizeof(int) * m_K);
        out += m_K;

        // Parity bits
        for (int i_byte = 0; i_byte < (int)m_K / 8; i_byte++) { // byte-by-byte
            // Pack the next 8 bits to form the next input (message) byte
            unsigned char in_byte = 0;
            for (int i_bit = 7; i_bit >= 0; i_bit--) {
                in_byte |= *in++ << i_bit;
            }

            // The CRC "register" holds the remainder from the previous input message
            // byte. This remainder has length m_parity bits and, therefore, leaks into
            // the m_parity bits following it in the remainder computation. The first byte
            // within these m_parity bits (the most significant byte or MSB) aligns with
            // the next input message byte (in_byte) that we are about to process. Hence,
            // get the MSB in the CRC "register" and XOR it with the the next input
            // message byte. The result is the byte whose remainder is to be computed
            // through a table look-up, namely the dividend (refer to Section 5 in
            // http://www.sunshine2k.de/articles/coding/crc/understanding_crc.html).
            unsigned char msb_crc = 0;
            for (int n = 1; n <= 8; n++) {
                msb_crc |= crc[m_parity - n] << (8 - n);
            }
            unsigned char dividend = (msb_crc ^ in_byte);

            // The first byte from the previous remainder (i.e., msb_crc) is already
            // included in the dividend. On the other hand, the remaining (m_parity - 8)
            // bits from the previous remainder are not. These must be added (mod-2) back
            // in the end. So, next, look-up the remainder from the dividend byte alone
            // and add back the (m_parity - 8) lower bits from the previous remainder:
            crc = (crc << 8) ^ m_crc_table[dividend];
        }

        // Serialize the parity bits to the output
        for (int n = m_parity - 1; n >= 0; n--) {
            *out++ = crc[n];
        }
    }

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
     * @param normal_fecframe Whether to use normal FECFRAMEs.
     * @param gen_poly Generator polynomial.
     */
    BchEncoder(int impl,
               int K,
               int N,
               int t,
               bool normal_fecframe,
               const tools::BCH_polynomial_generator<B>& gen_poly)
        : m_impl(impl),
          m_K(K),
          m_N(N),
          m_t(t),
          m_parity(N - K),
          m_aff3ct_encoder(new module::Encoder_BCH<>(K, N, gen_poly))
    {
        compute_gen_poly(normal_fecframe);
        compute_crc_table();
    }

    void encode(const std::vector<int>& ref_bits, std::vector<int>& enc_bits)
    {
        if (m_impl == 0) {
            m_aff3ct_encoder->encode(ref_bits, enc_bits);
        } else {
            gr_encode(ref_bits, enc_bits);
        }
    }
};

struct BchDecoder {
private:
    int m_K;    // Message length in bits.
    int m_N;    // Codeword length in bits.
    int m_impl; // Decoder implementation
    std::unique_ptr<module::Decoder_BCH_std<>> m_aff3ct_std_decoder;
    std::unique_ptr<GF_NORMAL> m_gf_normal;
    std::unique_ptr<GF_SHORT> m_gf_short;
    std::unique_ptr<BCH_SHORT_12> m_dvbs2rx_decoder_s12;
    std::array<uint8_t, 8192> m_packed_code;
    std::array<uint8_t, 24> m_packed_parity;

    /**
     * @brief Compute the hard decisions and pack them into bytes.
     *
     * Each LLR is converted to a hard decision by comparison to 0. The decision mapping
     * is bit=1 for llr < 0 and bit=0 for llr >= 0.
     *
     * @param llr_vec Vector with log likelihood ratios (LLRs).
     */
    void slice_and_pack(const std::vector<float>& llr_vec)
    {
        assert(llr_vec.size() == m_N);
        for (unsigned int j = 0; j < m_K; j++) {
            CODE::set_be_bit(m_packed_code.data(), j, llr_vec[j] < 0);
        }
        for (unsigned int j = 0; j < m_N - m_K; j++) {
            CODE::set_be_bit(m_packed_parity.data(), j, llr_vec[j + m_K] < 0);
        }
    }

    /**
     * @brief Unpack the BCH decoder result.
     *
     * @param dec_bits Resulting (unpacked) vector of decoded bits.
     */
    void unpack(std::vector<int>& dec_bits)
    {
        assert(dec_bits.size() == m_K);
        for (unsigned int j = 0; j < m_K; j++) {
            dec_bits[j] = CODE::get_be_bit(m_packed_code.data(), j);
        }
    }

public:
    /**
     * @brief Construct a new BCH Decoder object.
     *
     * @param impl Decoder implementation: 0 for AFF3CT and 1 for gr-dvbs2rx.
     * @param K Message length in bits.
     * @param N Codeword length in bits.
     * @param gen_poly Generator polynomial.
     */
    BchDecoder(int impl, int K, int N, const tools::BCH_polynomial_generator<B>& gen_poly)
        : m_impl(impl),
          m_K(K),
          m_N(N),
          m_aff3ct_std_decoder(new module::Decoder_BCH_std<>(K, N, gen_poly)),
          m_gf_normal(new GF_NORMAL()),
          m_gf_short(new GF_SHORT()),
          m_dvbs2rx_decoder_s12(new BCH_SHORT_12())
    {
        if (impl == 1) {
            for (int i = 0; i < m_packed_code.size(); i++) {
                m_packed_code[i] = 0;
            }
            for (int i = 0; i < m_packed_parity.size(); i++) {
                m_packed_parity[i] = 0;
            }
        }
    };

    void decode(const std::vector<float>& llr_vec, std::vector<int>& dec_bits)
    {
        if (m_impl == 0) {
            m_aff3ct_std_decoder->decode_siho(llr_vec, dec_bits);
        } else if (m_impl == 1) {
            slice_and_pack(llr_vec);
            (*m_dvbs2rx_decoder_s12)(
                m_packed_code.data(), m_packed_parity.data(), 0, 0, m_K);
            unpack(dec_bits);
        }
    }
};

struct params {
    int K = 9552;                 // number of information bits
    int N = 9720;                 // codeword size
    int t = 12;                   // t-error correction capability
    bool normal_fecframe = false; // whether to use normal FECFRAME
    int fe = 100;                 // target frame errors
    int max_n_frames = 10000;     // max frames to simulate per ebn0
    int seed = 0;                 // PRNG seed for the AWGN channel
    float ebn0_min = 0.00f;       // minimum SNR value
    float ebn0_max = 10.01f;      // maximum SNR value
    float ebn0_step = 1.00f;      // SNR step
    float R;                      // code rate (R=K/N)
    int impl = 0;                 // Decoder implementation

    params(int fe,
           int max_n_frames,
           float ebn0_min,
           float ebn0_max,
           float ebn0_step,
           int impl)
        : fe(fe),
          max_n_frames(max_n_frames),
          ebn0_min(ebn0_min),
          ebn0_max(ebn0_max),
          ebn0_step(ebn0_step),
          R((float)K / (float)N),
          impl(impl)
    {
        if (impl > 1)
            throw std::runtime_error("Unsupported implementation");

        std::cout << "# * Parameters: " << std::endl;
        std::cout << "#    ** Frame errors   = " << fe << std::endl;
        std::cout << "#    ** Max frames     = " << max_n_frames << std::endl;
        std::cout << "#    ** Noise seed     = " << seed << std::endl;
        std::cout << "#    ** Info. bits (K) = " << K << std::endl;
        std::cout << "#    ** Frame size (N) = " << N << std::endl;
        std::cout << "#    ** Code rate  (R) = " << R << std::endl;
        std::cout << "#    ** SNR min   (dB) = " << ebn0_min << std::endl;
        std::cout << "#    ** SNR max   (dB) = " << ebn0_max << std::endl;
        std::cout << "#    ** SNR step  (dB) = " << ebn0_step << std::endl;
        std::cout << "#" << std::endl;
    }
};

struct modules {
    std::unique_ptr<tools::BCH_polynomial_generator<>> gen_poly;
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
            "fe",
            po::value<int>()->default_value(1e2),
            "Max number of frame errors to simulate per Eb/N0.")(
            "nframes",
            po::value<int>()->default_value(1e7),
            "Max number of frames to simulate per Eb/N0.")(
            "ebn0-min", po::value<float>()->default_value(0), "Starting Eb/N0 in dB.")(
            "ebn0-max", po::value<float>()->default_value(10), "Ending Eb/N0 in dB.")(
            "ebn0-step", po::value<float>()->default_value(1), "Eb/N0 step in dB.")(
            "impl",
            po::value<int>()->default_value(0),
            "Decoder Implementation (0 for AFF3CT and 1 for GR)");

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

    params p(args["fe"].as<int>(),
             args["nframes"].as<int>(),
             args["ebn0-min"].as<float>(),
             args["ebn0-max"].as<float>(),
             args["ebn0-step"].as<float>(),
             args["impl"].as<int>());
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
    std::vector<int> bch_prim_poly;
    if (p.N < 16200) { // for short FECFRAME
        bch_prim_poly = {
            1, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1
        }; // g1(x) from Table 6b
    } else {
        bch_prim_poly = {
            1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
        }; // g1(x) from Table 6a
    }
    int N_p2_1 = tools::next_power_of_2(p.N) - 1;
    m.gen_poly.reset(new tools::BCH_polynomial_generator<>(N_p2_1, p.t, bch_prim_poly));
    m.source.reset(new module::Source_random<>(p.K));
    m.encoder.reset(
        new BchEncoder(p.impl, p.K, p.N, p.t, p.normal_fecframe, *m.gen_poly.get()));
    m.modem.reset(new module::Modem_BPSK<>(p.N));
    m.channel.reset(new module::Channel_AWGN_LLR<>(p.N));
    m.decoder.reset(new BchDecoder(p.impl, p.K, p.N, *m.gen_poly.get()));
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