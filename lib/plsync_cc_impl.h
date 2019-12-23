/* -*- c++ -*- */
/*
 * Copyright 2019-2021 Igor Freire.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_DVBS2RX_PLSYNC_CC_IMPL_H
#define INCLUDED_DVBS2RX_PLSYNC_CC_IMPL_H

#include "util.h"
#include <dvbs2rx/plsync_cc.h>

#define SOF_LEN 26
#define PLSC_LEN 64
#define PILOT_BLK_LEN 36
#define MAX_PILOT_BLKS 22
#define MAX_SLOTS 360
#define PLHEADER_LEN (SOF_LEN + PLSC_LEN)
#define SLOT_LEN 90
#define SLOTS_PER_PILOT_BLK 16
#define PILOT_BLK_INTERVAL (SLOTS_PER_PILOT_BLK * SLOT_LEN)
#define PILOT_BLK_PERIOD (PILOT_BLK_INTERVAL + PILOT_BLK_LEN)
#define MAX_PLFRAME_PAYLOAD (MAX_SLOTS * SLOT_LEN) + (MAX_PILOT_BLKS * PILOT_BLK_LEN)


/* correlator lengths, based on the number of differentials that we know in
 * advance (25 for SOF and only 32 for PLSC) */
#define SOF_CORR_LEN (SOF_LEN - 1)
#define PLSC_CORR_LEN 32

const int n_codewords = 128; // number of codewords for the 7-bit PLSC dataword
const uint64_t plsc_scrambler = 0x719d83c953422dfa; // PLSC scrambler sequence

const double fine_foffset_corr_range = 3.3e-4;
/* The normalized frequency offset that the fine frequency
 * estimator can "observe" is upper limited to:
 *
 * 1/(2*(1440 + 36)) = 3.3e-4
 *
 * When the frequency offset is higher than this, the phase changes by more than
 * 2*pi from pilot segment to pilot segment. Consequently, the fine estimation
 * approach does not work.
 */

namespace gr {
namespace dvbs2rx {

class frame_sync
{
private:
    /* Parameters */
    int debug_level; /** debug level */

    /* State */
    uint32_t sym_cnt;    /** symbol count */
    gr_complex last_in;  /** last symbol in */
    float timing_metric; /** most recent timing metric */

    bool locked;            /** whether frame timing is locked */
    bool locked_prev;       /** previous locked state */
    unsigned int frame_len; /** current PLFRAME length */

    buffer* d_plsc_delay_buf; /** buffer used as delay line */
    buffer* d_sof_buf;        /** SOF correlator buffer */
    buffer* d_plsc_e_buf;     /** Even PLSC correlator buffer  */
    buffer* d_plsc_o_buf;     /** Odd PLSC correlator buffer */
    buffer* d_plheader_buf;   /** buffer used to store PLHEADER syms */
    gr_complex* d_sof_taps;   /** SOF cross-correlation taps */
    gr_complex* d_plsc_taps;  /** PLSC cross-correlation taps */

    /* Timing metric threshold for inferring a start of frame.
     *
     * When unlocked, use a conservative threshold, as it is important
     * to avoid false positive SOF detection. In contrast, when locked,
     * we only want to periodically check whether the correlation is
     * sufficiently strong where it is expected to be (at the start of
     * the next frame). Since it is very important not to unlock
     * unncessarily, use a lower threshold for this. */
    const float threshold_u = 40; /** unlocked threshold */
    const float threshold_l = 25; /** locked threshold */

    /**
     * \brief Compute cross-correlation
     * \param buf (buffer*) Pointer to tapped-delay line buffer
     * \param taps (gr_complex *) Pointer to taps for cross-correlation
     * \param res (gr_complex *) Pointer where to store result
     * \return Void.
     */
    void correlate(buffer* buf, gr_complex* taps, gr_complex* res);

public:
    frame_sync(int debug_level);
    ~frame_sync();

    /**
     * \brief Step frame timing recovery loop
     * \param in (gr_complex &) Input symbol
     * \return bool whether current symbol is the first of a new frame
     */
    bool step(const gr_complex& in);

    float get_timing_metric() { return timing_metric; }
    bool get_locked() { return locked; }
    void set_frame_len(unsigned int len) { frame_len = len; }

    const gr_complex* get_plheader() { return d_plheader_buf->get_tail(); }
};

class freq_sync
{
private:
    /* Parameters */
    int debug_level;     /** debug level */
    unsigned int period; /** estimation periodicity in frames */

    /* Coarse frequency offset estimation state */
    double coarse_foffset; /** most recent freq. offset estimate */
    unsigned int i_frame;  /** frame counter */
    unsigned int N;        /** "preamble" length */
    unsigned int L;        /** used phase differentials (<= N) */
    bool coarse_corrected; /** residual offset is sufficiently low */

    /* NOTE: In principle, we could make N equal to the SOF length (26) and L =
     * N-1 (i.e. 25), in which case coarse frequency offset estimation would be
     * based on the SOF symbols only and would not require decoding of the
     * PLSC. However, this would waste all the other 64 known PLHEADER symbols,
     * which can improve coarse estimation performance substantially. So N in
     * the end will be set as 90 and L to 89. Nonetheless, the variables are
     * kept here for flexibility on experiments. */

    /* Fine frequency offset estimation state */
    double fine_foffset;

    /* Volk buffers */
    gr_complex* plheader_conj; /** conjugate of PLHEADER symbols */
    gr_complex* pilot_mod_rm;  /** modulation-removed received pilots */
    gr_complex* pilot_corr;    /** mod-removed autocorrelation */
    float* angle_corr;         /** autocorrelation angles */
    float* angle_diff;         /** angle differences */
    float* w_window_l;         /** weighting window when locked */
    float* w_window_u;         /** weighting window when unlocked */
    float* w_angle_diff;       /** weighted angle differences */
    float* w_angle_avg;        /** weighted angle average */
    gr_complex* unmod_pilots;  /** conjugate of un-modulated pilots */
    float* angle_pilot;        /** average angle of pilot segments */
    float* angle_diff_f;       /** diff of average pilot angles */
    gr_complex* pp_plheader;   /** derotated PLHEADER symbols */

public:
    freq_sync(unsigned int period, int debug_level, const uint64_t* codewords);
    ~freq_sync();

    /**
     * \brief Data-aided coarse frequency offset estimation
     *
     * The estimation completes after `period` frames have been
     * processed. Hence, one in every `period` calls a new estimate will
     * be produced.
     *
     * \param in (gr_complex *) Pointer to the start of frame
     * \param i_codeword (uint8_t) PLSC dataword (also the codeword idx)
     * \param locked (bool) Whether frame timing is locked
     * \return (bool) whether new estimate was computed.
     *
     * \note in order to compute the data-aided coarse frequency offset
     * estimation based on the entire PLHEADER, the PLSC dataword must
     * be correctly decoded. Hence, before frame time locking is
     * acquired, this function will estimate frequency offset based on
     * the SOF symbols only.
     */
    bool estimate_coarse(const gr_complex* in, uint8_t i_codeword, bool locked);

    /**
     * \brief Estimate the average phase of the PLHEADER
     * \param in (gr_complex *) Pointer to PLHEADER symbol array
     * \param i_codeword (uint8_t) PLSC dataword (also the codeword idx)
     * \return (float) The phase estimate within -pi to +pi.
     *
     * \note i_codeword indicates the true (expected) PLHEADER symbols,
     * so that the phase estimation can be fully data-aided.
     */
    float estimate_plheader_phase(const gr_complex* in, uint8_t i_codeword);

    /**
     * \brief Estimate the average phase of a pilot block
     * \param in (gr_complex *) Pointer to the pilot symbol array
     * \param i_blk (int) Index of this pilot block within the PLFRAME
     * \return (float) The phase estimate within -pi to +pi.
     *
     * \note The gr_complex array pointed `in` is expected to contain
     * the PLHEADER within its first 90 positions, then all 36-symbol
     * pilot blocks consecutively in the indexes that follow. The pilot
     * block index `i_blk` is used internally in order to fetch the
     * correct input pilots for phase estimation. The result will be
     * stored in an internal pilot angle buffer.
     */
    float estimate_pilot_phase(const gr_complex* in, int i_blk);

    /**
     * \brief Data-aided fine frequency offset estimation
     *
     * Should be executed solely if the frame has pilots
     *
     * \param pilots (gr_complex *) Pointer to the pilot symbol buffer
     * \param n_pilot_blks (uint8_t) Number of pilot blocks in the frame
     * \return Void.
     */
    void estimate_fine(const gr_complex* pilots, uint8_t n_pilot_blks);

    /**
     * \brief De-rotate PLHEADER symbols
     * \param in (const gr_complex *) Input rotated PLHEADER buffer
     * \return Void
     *
     * \note The de-rotate PLHEADER is saved internally and can be
     * accessed using the `get_plheader` function below.
     */
    void derotate_plheader(const gr_complex* in);

    double get_coarse_foffset() { return coarse_foffset; }
    double get_fine_foffset() { return fine_foffset; }
    bool is_coarse_corrected() { return coarse_corrected; }
    const gr_complex* get_plheader() { return pp_plheader; }
};

class plsc_decoder
{
private:
    /* Parameters */
    int debug_level; /** debug level */

    /* Constants */
    uint64_t codewords[n_codewords]; // all possible 64-bit codewords

    /**
     * \brief Coherent pi/2 BPSK demapping
     * \param in (const gr_complex *) Incoming PLHEADER BPSK symbols
     * \return (uin64_t) Demapped bits of the scrambled PLSC
     * \note Pointer *in must point to the start of the PLHEADER.
     */
    uint64_t demap_bpsk(const gr_complex* in);

    /**
     * \brief Differential pi/2 BPSK demapping
     * \param in (const gr_complex *) Incoming PLHEADER BPSK symbols
     * \return (uin64_t) Demapped bits of the scrambled PLSC
     * \note Pointer *in must point to the start of the PLHEADER.
     */
    uint64_t demap_bpsk_diff(const gr_complex* in);

public:
    /* State - made public to speed up access */
    uint8_t dec_plsc;     /** 7-bit decoded PLSC dataword */
    uint8_t modcod;       /** MODCOD of the decoded PLSC */
    bool short_fecframe;  /** Wether FECFRAME size is short */
    bool has_pilots;      /** Wether PLFRAME has pilot blocks */
    bool dummy_frame;     /** Whether PLFRAME is a dummy frame */
    uint8_t n_mod;        /** bits per const symbol */
    uint16_t S;           /** number of slots */
    uint16_t plframe_len; /** PLFRAME length */
    uint8_t n_pilots;     /* number of pilot blocks */

    plsc_decoder(int debug_level);
    ~plsc_decoder(){};

    /**
     * \brief Decode the incoming pi/2 BPSK symbols of the PLSC
     * \param in (const gr_complex *) Input pi/2 BPSK symbols
     * \param coherent (bool) Whether to use coherent BPSK demapping
     * \return Void.
     */
    void decode(const gr_complex* in, bool coherent);

    const uint64_t* get_codewords() { return codewords; }
};

class pl_descrambler
{
private:
    int gold_code;               /** Gold code (scrambling code) */
    int Rn[MAX_PLFRAME_PAYLOAD]; /** Pre-computed int-valued sequence
                                  * that defines the scrambling symbol
                                  * mapping table (c.f. Section 5.4.4 of
                                  * the standard) */
    int parity_chk(long a, long b);

    /**
     * \brief Pre-compute the scrambler table
     * \return Void
     */
    void build_symbol_scrambler_table();

public:
    pl_descrambler(int gold_code);
    ~pl_descrambler(){};

    /**
     * \brief De-scramble input symbol
     * \param in (const gr_complex&) Input symbol
     * \param out (gr_complex&) Output (descrambled) symbol
     * \param i (int) Symbol index
     *
     * \note Symbol index is relative to the start of the "payload",
     * i.e. is 0 on the first symbol past the PLHEADER.
     */
    void step(const gr_complex& in, gr_complex& out, int i);
};

class plsync_cc_impl : public plsync_cc
{
private:
    /* Parameters */
    int d_debug_level; /** debug level */
    const float d_sps; /** samples per symbol */
    /* NOTE: the PLSYNC block requires a symbol-spaced stream at its
     * input. Hence, sps does not refer to the input stream. Instead, it
     * refers to the oversampling ratio that is adopted in the receiver
     * flowgraph prior to the matched filter. This is so that this block
     * can control the external rotator phase properly */

    /* State */
    uint16_t d_i_sym;          /** Symbol index within PLFRAME */
    bool d_locked;             /** Whether frame timing is locked */
    float d_da_phase;          /** Last data-aided phase estimate */
    int d_tag_delay;           /** delay of rotator's new inc tag */
    double d_rot_freq[3] = {}; /** Upstream rotator's frequency (past,
                                * current and next) */

    const pmt::pmt_t d_port_id = pmt::mp("rotator_phase_inc");

    /* Objects */
    frame_sync* d_frame_sync;         /** frame synchronizer */
    freq_sync* d_freq_sync;           /** frequency synchronizer */
    plsc_decoder* d_plsc_decoder;     /** PLSC decoder */
    pl_descrambler* d_pl_descrambler; /** PL descrambler */

    /* Buffers */
    gr_complex* d_rx_pilots; /** Received pilot symbols */

public:
    plsync_cc_impl(int gold_code, int freq_est_period, float sps, int debug_level);
    ~plsync_cc_impl();

    // Where all the action really happens
    void forecast(int noutput_items, gr_vector_int& ninput_items_required);

    int general_work(int noutput_items,
                     gr_vector_int& ninput_items,
                     gr_vector_const_void_star& input_items,
                     gr_vector_void_star& output_items);

    float get_freq_offset() { return d_freq_sync->get_coarse_foffset(); }
};

} // namespace dvbs2rx
} // namespace gr

#endif /* INCLUDED_DVBS2RX_PLSYNC_CC_IMPL_H */
