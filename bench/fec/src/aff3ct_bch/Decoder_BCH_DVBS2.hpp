#ifndef DECODER_BCH_DVBS2_HPP_
#define DECODER_BCH_DVBS2_HPP_

#include <aff3ct.hpp>

namespace aff3ct {
namespace module {
template <typename B = int, typename R = float>
class Decoder_BCH_DVBS2 : public Decoder_BCH_std<B, R>
{
public:
    Decoder_BCH_DVBS2(const int& K,
                      const int& N,
                      const tools::BCH_polynomial_generator<B>& GF);

    virtual ~Decoder_BCH_DVBS2() = default;

    virtual Decoder_BCH_DVBS2<B, R>* clone() const;

protected:
    virtual int _decode_hiho(const B* Y_N, int8_t* CWD, B* V_K, const size_t frame_id);
    virtual int _decode_hiho_cw(const B* Y_N, int8_t* CWD, B* V_N, const size_t frame_id);
    virtual int _decode_siho(const R* Y_N, int8_t* CWD, B* V_K, const size_t frame_id);
    virtual int _decode_siho_cw(const R* Y_N, int8_t* CWD, B* V_N, const size_t frame_id);
};
} // namespace module
} // namespace aff3ct

#endif // DECODER_BCH_DVBS2_HPP_
