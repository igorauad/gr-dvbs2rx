/* -*- c++ -*- */
/*
 * Copyright 2024 AsriFox.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_DVB_PARAMS_H
#define INCLUDED_DVBS2RX_DVB_PARAMS_H

#include <gnuradio/dvbs2rx/api.h>
#include <gnuradio/dvbs2rx/dvb_config.h>
#include <gnuradio/dvbs2rx/dvbs2_config.h>
#include <gnuradio/dvbs2rx/dvbt2_config.h>
#include <string>

namespace gr {
namespace dvbs2rx {

/*!
 * \brief Translation of DVB-S2/S2X/T2 parameters from strings to enum definitions.
 * \ingroup dvbs2rx
 */
class DVBS2RX_API dvb_params
{
public:
    /*!
     * \brief Translate parameters from strings to enum definitions.
     * \param standard (string) DVB standard (DVB-S2, DVB-S2X, DVB-T2).
     * \param frame_size (string) Frame size (normal, medium, short).
     * \param code_rate (string) LDPC code rate identifier (e.g., 1/4, 1/3, etc.).
     * \param constellation (string) Constellation (QPSK, 8PSK, 16QAM, etc.).
     * \param rolloff (float) Roll-off factor.
     * \param pilots (bool) Whether physical layer pilots are enabled.
     * \param vl_snr (bool) DVB-S2X very-low (VL) SNR mode.
     */
    static dvb_params make(std::string standard,
                           std::string frame_size,
                           std::string code_rate,
                           std::string constellation,
                           float rolloff,
                           bool pilots,
                           bool vl_snr = false);

    dvb_params(dvb_standard_t,
               dvb_framesize_t,
               dvb_code_rate_t,
               dvb_constellation_t,
               dvbs2_rolloff_factor_t,
               dvbs2_pilots_t);
    ~dvb_params();

    const dvb_standard_t standard;
    const dvb_framesize_t framesize;
    const dvb_code_rate_t rate;
    const dvb_constellation_t constellation;
    const dvbs2_rolloff_factor_t rolloff;
    const dvbs2_pilots_t pilots;
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_DVB_PARAMS_H */
