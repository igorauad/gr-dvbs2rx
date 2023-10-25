#ifndef ENCODER_BCH_DVBS2_HPP_
#define ENCODER_BCH_DVBS2_HPP_

#include <aff3ct.hpp>

namespace aff3ct {
namespace module {
template <typename B = int>
class Encoder_BCH_DVBS2 : public Encoder_BCH<B>
{
protected:
    std::vector<B> U_K_rev;

public:
    Encoder_BCH_DVBS2(const int& K,
                      const int& N,
                      const tools::BCH_polynomial_generator<B>& GF);

    virtual ~Encoder_BCH_DVBS2() = default;

    virtual Encoder_BCH_DVBS2<B>* clone() const;

protected:
    virtual void _encode(const B* U_K, B* X_N, const size_t frame_id);
};
} // namespace module
} // namespace aff3ct

#endif // ENCODER_BCH_DVBS2_HPP_
