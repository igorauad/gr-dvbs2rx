#include <cmath>
#include <iostream>
#include <vector>

#include "Decoder_BCH_DVBS2.hpp"

using namespace aff3ct;
using namespace aff3ct::module;

template <typename B, typename R>
Decoder_BCH_DVBS2<B, R>::Decoder_BCH_DVBS2(
    const int& K, const int& N, const tools::BCH_polynomial_generator<B>& GF_poly)
    : Decoder_BCH_std<B, R>(K, N, GF_poly)
{
    const std::string name = "Decoder_BCH_DVBS2";
    this->set_name(name);
}

template <typename B, typename R>
Decoder_BCH_DVBS2<B, R>* Decoder_BCH_DVBS2<B, R>::clone() const
{
    auto m = new Decoder_BCH_DVBS2(*this);
    m->deep_copy(*this);
    return m;
}

template <typename B, typename R>
int Decoder_BCH_DVBS2<B, R>::_decode_hiho(const B* Y_N,
                                          int8_t* CWD,
                                          B* V_K,
                                          const size_t frame_id)
{
    std::reverse_copy(Y_N, Y_N + this->N, this->YH_N.begin());

    auto status = this->_decode(this->YH_N.data(), frame_id);

    std::reverse_copy(
        this->YH_N.data() + this->N - this->K, this->YH_N.data() + this->N, V_K);

    CWD[0] = !status;
    return status;
}

template <typename B, typename R>
int Decoder_BCH_DVBS2<B, R>::_decode_hiho_cw(const B* Y_N,
                                             int8_t* CWD,
                                             B* V_N,
                                             const size_t frame_id)
{
    throw tools::unimplemented_error(__FILE__, __LINE__, __func__);
}

template <typename B, typename R>
int Decoder_BCH_DVBS2<B, R>::_decode_siho(const R* Y_N,
                                          int8_t* CWD,
                                          B* V_K,
                                          const size_t frame_id)
{
    throw tools::unimplemented_error(__FILE__, __LINE__, __func__);
}

template <typename B, typename R>
int Decoder_BCH_DVBS2<B, R>::_decode_siho_cw(const R* Y_N,
                                             int8_t* CWD,
                                             B* V_N,
                                             const size_t frame_id)
{
    throw tools::unimplemented_error(__FILE__, __LINE__, __func__);
}


// ====================================================================================
// explicit template instantiation
#include "Tools/types.h"
#ifdef AFF3CT_MULTI_PREC
template class aff3ct::module::Decoder_BCH_DVBS2<B_8, Q_8>;
template class aff3ct::module::Decoder_BCH_DVBS2<B_16, Q_16>;
template class aff3ct::module::Decoder_BCH_DVBS2<B_32, Q_32>;
template class aff3ct::module::Decoder_BCH_DVBS2<B_64, Q_64>;
#else
template class aff3ct::module::Decoder_BCH_DVBS2<B, Q>;
#endif
// ====================================================================================
// explicit template instantiation
