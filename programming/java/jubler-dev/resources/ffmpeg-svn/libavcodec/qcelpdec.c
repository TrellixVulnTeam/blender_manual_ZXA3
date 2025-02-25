/*
 * QCELP decoder
 * Copyright (c) 2007 Reynaldo H. Verdejo Pinochet
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file qcelpdec.c
 * QCELP decoder
 * @author Reynaldo H. Verdejo Pinochet
 * @remark FFmpeg merging spearheaded by Kenan Gillet
 * @remark Development mentored by Benjamin Larson
 */

#include <stddef.h>

#include "avcodec.h"
#include "internal.h"
#include "bitstream.h"

#include "qcelpdata.h"

#include "celp_math.h"
#include "celp_filters.h"

#undef NDEBUG
#include <assert.h>

typedef enum
{
    I_F_Q = -1,    /*!< insufficient frame quality */
    SILENCE,
    RATE_OCTAVE,
    RATE_QUARTER,
    RATE_HALF,
    RATE_FULL
} qcelp_packet_rate;

typedef struct
{
    GetBitContext     gb;
    qcelp_packet_rate bitrate;
    QCELPFrame        frame;    /*!< unpacked data frame */

    uint8_t  erasure_count;
    uint8_t  octave_count;      /*!< count the consecutive RATE_OCTAVE frames */
    float    prev_lspf[10];
    float    predictor_lspf[10];/*!< LSP predictor for RATE_OCTAVE and I_F_Q */
    float    pitch_synthesis_filter_mem[303];
    float    pitch_pre_filter_mem[303];
    float    rnd_fir_filter_mem[180];
    float    formant_mem[170];
    float    last_codebook_gain;
    int      prev_g1[2];
    int      prev_bitrate;
    float    pitch_gain[4];
    uint8_t  pitch_lag[4];
    uint16_t first16bits;
} QCELPContext;

/**
 * Reconstructs LPC coefficients from the line spectral pair frequencies.
 *
 * TIA/EIA/IS-733 2.4.3.3.5
 */
void ff_qcelp_lspf2lpc(const float *lspf, float *lpc);

static void weighted_vector_sumf(float *out, const float *in_a,
                                 const float *in_b, float weight_coeff_a,
                                 float weight_coeff_b, int length)
{
    int i;

    for(i=0; i<length; i++)
        out[i] = weight_coeff_a * in_a[i]
               + weight_coeff_b * in_b[i];
}

/**
 * Initialize the speech codec according to the specification.
 *
 * TIA/EIA/IS-733 2.4.9
 */
static av_cold int qcelp_decode_init(AVCodecContext *avctx)
{
    QCELPContext *q = avctx->priv_data;
    int i;

    avctx->sample_fmt = SAMPLE_FMT_FLT;

    for(i=0; i<10; i++)
        q->prev_lspf[i] = (i+1)/11.;

    return 0;
}

/**
 * Decodes the 10 quantized LSP frequencies from the LSPV/LSP
 * transmission codes of any bitrate and checks for badly received packets.
 *
 * @param q the context
 * @param lspf line spectral pair frequencies
 *
 * @return 0 on success, -1 if the packet is badly received
 *
 * TIA/EIA/IS-733 2.4.3.2.6.2-2, 2.4.8.7.3
 */
static int decode_lspf(QCELPContext *q, float *lspf)
{
    int i;
    float tmp_lspf, smooth, erasure_coeff;
    const float *predictors;

    if(q->bitrate == RATE_OCTAVE || q->bitrate == I_F_Q)
    {
        predictors = (q->prev_bitrate != RATE_OCTAVE &&
                       q->prev_bitrate != I_F_Q ?
                       q->prev_lspf : q->predictor_lspf);

        if(q->bitrate == RATE_OCTAVE)
        {
            q->octave_count++;

            for(i=0; i<10; i++)
            {
                q->predictor_lspf[i] =
                             lspf[i] = (q->frame.lspv[i] ?  QCELP_LSP_SPREAD_FACTOR
                                                         : -QCELP_LSP_SPREAD_FACTOR)
                                     + predictors[i] * QCELP_LSP_OCTAVE_PREDICTOR
                                     + (i + 1) * ((1 - QCELP_LSP_OCTAVE_PREDICTOR)/11);
            }
            smooth = (q->octave_count < 10 ? .875 : 0.1);
        }else
        {
            erasure_coeff = QCELP_LSP_OCTAVE_PREDICTOR;

            assert(q->bitrate == I_F_Q);

            if(q->erasure_count > 1)
                erasure_coeff *= (q->erasure_count < 4 ? 0.9 : 0.7);

            for(i=0; i<10; i++)
            {
                q->predictor_lspf[i] =
                             lspf[i] = (i + 1) * ( 1 - erasure_coeff)/11
                                     + erasure_coeff * predictors[i];
            }
            smooth = 0.125;
        }

        // Check the stability of the LSP frequencies.
        lspf[0] = FFMAX(lspf[0], QCELP_LSP_SPREAD_FACTOR);
        for(i=1; i<10; i++)
            lspf[i] = FFMAX(lspf[i], (lspf[i-1] + QCELP_LSP_SPREAD_FACTOR));

        lspf[9] = FFMIN(lspf[9], (1.0 - QCELP_LSP_SPREAD_FACTOR));
        for(i=9; i>0; i--)
            lspf[i-1] = FFMIN(lspf[i-1], (lspf[i] - QCELP_LSP_SPREAD_FACTOR));

        // Low-pass filter the LSP frequencies.
        weighted_vector_sumf(lspf, lspf, q->prev_lspf, smooth, 1.0-smooth, 10);
    }else
    {
        q->octave_count = 0;

        tmp_lspf = 0.;
        for(i=0; i<5 ; i++)
        {
            lspf[2*i+0] = tmp_lspf += qcelp_lspvq[i][q->frame.lspv[i]][0] * 0.0001;
            lspf[2*i+1] = tmp_lspf += qcelp_lspvq[i][q->frame.lspv[i]][1] * 0.0001;
        }

        // Check for badly received packets.
        if(q->bitrate == RATE_QUARTER)
        {
            if(lspf[9] <= .70 || lspf[9] >=  .97)
                return -1;
            for(i=3; i<10; i++)
                if(fabs(lspf[i] - lspf[i-2]) < .08)
                    return -1;
        }else
        {
            if(lspf[9] <= .66 || lspf[9] >= .985)
                return -1;
            for(i=4; i<10; i++)
                if (fabs(lspf[i] - lspf[i-4]) < .0931)
                    return -1;
        }
    }
    return 0;
}

/**
 * Converts codebook transmission codes to GAIN and INDEX.
 *
 * @param q the context
 * @param gain array holding the decoded gain
 *
 * TIA/EIA/IS-733 2.4.6.2
 */
static void decode_gain_and_index(QCELPContext  *q,
                                  float *gain) {
    int   i, subframes_count, g1[16];
    float slope;

    if(q->bitrate >= RATE_QUARTER)
    {
        switch(q->bitrate)
        {
            case RATE_FULL: subframes_count = 16; break;
            case RATE_HALF: subframes_count = 4;  break;
            default:        subframes_count = 5;
        }
        for(i=0; i<subframes_count; i++)
        {
            g1[i] = 4 * q->frame.cbgain[i];
            if(q->bitrate == RATE_FULL && !((i+1) & 3))
            {
                g1[i] += av_clip((g1[i-1] + g1[i-2] + g1[i-3]) / 3 - 6, 0, 32);
            }

            gain[i] = qcelp_g12ga[g1[i]];

            if(q->frame.cbsign[i])
            {
                gain[i] = -gain[i];
                q->frame.cindex[i] = (q->frame.cindex[i]-89) & 127;
            }
        }

        q->prev_g1[0] = g1[i-2];
        q->prev_g1[1] = g1[i-1];
        q->last_codebook_gain = qcelp_g12ga[g1[i-1]];

        if(q->bitrate == RATE_QUARTER)
        {
            // Provide smoothing of the unvoiced excitation energy.
            gain[7] =     gain[4];
            gain[6] = 0.4*gain[3] + 0.6*gain[4];
            gain[5] =     gain[3];
            gain[4] = 0.8*gain[2] + 0.2*gain[3];
            gain[3] = 0.2*gain[1] + 0.8*gain[2];
            gain[2] =     gain[1];
            gain[1] = 0.6*gain[0] + 0.4*gain[1];
        }
    }else
    {
        if(q->bitrate == RATE_OCTAVE)
        {
            g1[0] = 2 * q->frame.cbgain[0]
                  + av_clip((q->prev_g1[0] + q->prev_g1[1]) / 2 - 5, 0, 54);
            subframes_count = 8;
        }else
        {
            assert(q->bitrate == I_F_Q);

            g1[0] = q->prev_g1[1];
            switch(q->erasure_count)
            {
                case 1 : break;
                case 2 : g1[0] -= 1; break;
                case 3 : g1[0] -= 2; break;
                default: g1[0] -= 6;
            }
            if(g1[0] < 0)
                g1[0] = 0;
            subframes_count = 4;
        }
        // This interpolation is done to produce smoother background noise.
        slope = 0.5*(qcelp_g12ga[g1[0]] - q->last_codebook_gain) / subframes_count;
        for(i=1; i<=subframes_count; i++)
            gain[i-1] = q->last_codebook_gain + slope * i;

        q->last_codebook_gain = gain[i-2];
        q->prev_g1[0] = q->prev_g1[1];
        q->prev_g1[1] = g1[0];
    }
}

/**
 * If the received packet is Rate 1/4 a further sanity check is made of the
 * codebook gain.
 *
 * @param cbgain the unpacked cbgain array
 * @return -1 if the sanity check fails, 0 otherwise
 *
 * TIA/EIA/IS-733 2.4.8.7.3
 */
static int codebook_sanity_check_for_rate_quarter(const uint8_t *cbgain)
{
    int i, diff, prev_diff=0;

    for(i=1; i<5; i++)
    {
        diff = cbgain[i] - cbgain[i-1];
        if(FFABS(diff) > 10)
            return -1;
        else if(FFABS(diff - prev_diff) > 12)
            return -1;
        prev_diff = diff;
    }
    return 0;
}

/**
 * Computes the scaled codebook vector Cdn From INDEX and GAIN
 * for all rates.
 *
 * The specification lacks some information here.
 *
 * TIA/EIA/IS-733 has an omission on the codebook index determination
 * formula for RATE_FULL and RATE_HALF frames at section 2.4.8.1.1. It says
 * you have to subtract the decoded index parameter from the given scaled
 * codebook vector index 'n' to get the desired circular codebook index, but
 * it does not mention that you have to clamp 'n' to [0-9] in order to get
 * RI-compliant results.
 *
 * The reason for this mistake seems to be the fact they forgot to mention you
 * have to do these calculations per codebook subframe and adjust given
 * equation values accordingly.
 *
 * @param q the context
 * @param gain array holding the 4 pitch subframe gain values
 * @param cdn_vector array for the generated scaled codebook vector
 */
static void compute_svector(QCELPContext *q, const float *gain,
                            float *cdn_vector)
{
    int      i, j, k;
    uint16_t cbseed, cindex;
    float    *rnd, tmp_gain, fir_filter_value;

    switch(q->bitrate)
    {
        case RATE_FULL:
            for(i=0; i<16; i++)
            {
                tmp_gain = gain[i] * QCELP_RATE_FULL_CODEBOOK_RATIO;
                cindex = -q->frame.cindex[i];
                for(j=0; j<10; j++)
                    *cdn_vector++ = tmp_gain * qcelp_rate_full_codebook[cindex++ & 127];
            }
        break;
        case RATE_HALF:
            for(i=0; i<4; i++)
            {
                tmp_gain = gain[i] * QCELP_RATE_HALF_CODEBOOK_RATIO;
                cindex = -q->frame.cindex[i];
                for (j = 0; j < 40; j++)
                *cdn_vector++ = tmp_gain * qcelp_rate_half_codebook[cindex++ & 127];
            }
        break;
        case RATE_QUARTER:
            cbseed = (0x0003 & q->frame.lspv[4])<<14 |
                     (0x003F & q->frame.lspv[3])<< 8 |
                     (0x0060 & q->frame.lspv[2])<< 1 |
                     (0x0007 & q->frame.lspv[1])<< 3 |
                     (0x0038 & q->frame.lspv[0])>> 3 ;
            rnd = q->rnd_fir_filter_mem + 20;
            for(i=0; i<8; i++)
            {
                tmp_gain = gain[i] * (QCELP_SQRT1887 / 32768.0);
                for(k=0; k<20; k++)
                {
                    cbseed = 521 * cbseed + 259;
                    *rnd = (int16_t)cbseed;

                    // FIR filter
                    fir_filter_value = 0.0;
                    for(j=0; j<10; j++)
                        fir_filter_value += qcelp_rnd_fir_coefs[j ]
                                          * (rnd[-j ] + rnd[-20+j]);

                    fir_filter_value += qcelp_rnd_fir_coefs[10] * rnd[-10];
                    *cdn_vector++ = tmp_gain * fir_filter_value;
                    rnd++;
                }
            }
            memcpy(q->rnd_fir_filter_mem, q->rnd_fir_filter_mem + 160, 20 * sizeof(float));
        break;
        case RATE_OCTAVE:
            cbseed = q->first16bits;
            for(i=0; i<8; i++)
            {
                tmp_gain = gain[i] * (QCELP_SQRT1887 / 32768.0);
                for(j=0; j<20; j++)
                {
                    cbseed = 521 * cbseed + 259;
                    *cdn_vector++ = tmp_gain * (int16_t)cbseed;
                }
            }
        break;
        case I_F_Q:
            cbseed = -44; // random codebook index
            for(i=0; i<4; i++)
            {
                tmp_gain = gain[i] * QCELP_RATE_FULL_CODEBOOK_RATIO;
                for(j=0; j<40; j++)
                    *cdn_vector++ = tmp_gain * qcelp_rate_full_codebook[cbseed++ & 127];
            }
        break;
    }
}

/**
 * Apply generic gain control.
 *
 * @param v_out output vector
 * @param v_in gain-controlled vector
 * @param v_ref vector to control gain of
 *
 * FIXME: If v_ref is a zero vector, it energy is zero
 *        and the behavior of the gain control is
 *        undefined in the specs.
 *
 * TIA/EIA/IS-733 2.4.8.3-2/3/4/5, 2.4.8.6
 */
static void apply_gain_ctrl(float *v_out, const float *v_ref,
                            const float *v_in)
{
    int   i, j, len;
    float scalefactor;

    for(i=0, j=0; i<4; i++)
    {
        scalefactor = ff_dot_productf(v_in + j, v_in + j, 40);
        if(scalefactor)
            scalefactor = sqrt(ff_dot_productf(v_ref + j, v_ref + j, 40)
                        / scalefactor);
        else
            ff_log_missing_feature(NULL, "Zero energy for gain control", 1);
        for(len=j+40; j<len; j++)
            v_out[j] = scalefactor * v_in[j];
    }
}

/**
 * Apply filter in pitch-subframe steps.
 *
 * @param memory buffer for the previous state of the filter
 *        - must be able to contain 303 elements
 *        - the 143 first elements are from the previous state
 *        - the next 160 are for output
 * @param v_in input filter vector
 * @param gain per-subframe gain array, each element is between 0.0 and 2.0
 * @param lag per-subframe lag array, each element is
 *        - between 16 and 143 if its corresponding pfrac is 0,
 *        - between 16 and 139 otherwise
 * @param pfrac per-subframe boolean array, 1 if the lag is fractional, 0
 *        otherwise
 *
 * @return filter output vector
 */
static const float *do_pitchfilter(float memory[303], const float v_in[160],
                                   const float gain[4], const uint8_t *lag,
                                   const uint8_t pfrac[4])
{
    int         i, j;
    float       *v_lag, *v_out;
    const float *v_len;

    v_out = memory + 143; // Output vector starts at memory[143].

    for(i=0; i<4; i++)
    {
        if(gain[i])
        {
            v_lag = memory + 143 + 40 * i - lag[i];
            for(v_len=v_in+40; v_in<v_len; v_in++)
            {
                if(pfrac[i]) // If it is a fractional lag...
                {
                    for(j=0, *v_out=0.; j<4; j++)
                        *v_out += qcelp_hammsinc_table[j] * (v_lag[j-4] + v_lag[3-j]);
                }else
                    *v_out = *v_lag;

                *v_out = *v_in + gain[i] * *v_out;

                v_lag++;
                v_out++;
            }
        }else
        {
            memcpy(v_out, v_in, 40 * sizeof(float));
            v_in  += 40;
            v_out += 40;
        }
    }

    memmove(memory, memory + 160, 143 * sizeof(float));
    return memory + 143;
}

/**
 * Apply pitch synthesis filter and pitch prefilter to the scaled codebook vector.
 * TIA/EIA/IS-733 2.4.5.2
 *
 * @param q the context
 * @param cdn_vector the scaled codebook vector
 */
static void apply_pitch_filters(QCELPContext *q, float *cdn_vector)
{
    int         i;
    const float *v_synthesis_filtered, *v_pre_filtered;

    if(q->bitrate >= RATE_HALF ||
       (q->bitrate == I_F_Q && (q->prev_bitrate >= RATE_HALF)))
    {

        if(q->bitrate >= RATE_HALF)
        {

            // Compute gain & lag for the whole frame.
            for(i=0; i<4; i++)
            {
                q->pitch_gain[i] = q->frame.plag[i] ? (q->frame.pgain[i] + 1) * 0.25 : 0.0;

                q->pitch_lag[i] = q->frame.plag[i] + 16;
            }
        }else
        {
            float max_pitch_gain = q->erasure_count < 3 ? 0.9 - 0.3 * (q->erasure_count - 1) : 0.0;
            for(i=0; i<4; i++)
                q->pitch_gain[i] = FFMIN(q->pitch_gain[i], max_pitch_gain);

            memset(q->frame.pfrac, 0, sizeof(q->frame.pfrac));
        }

        // pitch synthesis filter
        v_synthesis_filtered = do_pitchfilter(q->pitch_synthesis_filter_mem,
                                              cdn_vector, q->pitch_gain,
                                              q->pitch_lag, q->frame.pfrac);

        // pitch prefilter update
        for(i=0; i<4; i++)
            q->pitch_gain[i] = 0.5 * FFMIN(q->pitch_gain[i], 1.0);

        v_pre_filtered = do_pitchfilter(q->pitch_pre_filter_mem,
                                        v_synthesis_filtered,
                                        q->pitch_gain, q->pitch_lag,
                                        q->frame.pfrac);

        apply_gain_ctrl(cdn_vector, v_synthesis_filtered, v_pre_filtered);
    }else
    {
        memcpy(q->pitch_synthesis_filter_mem, cdn_vector + 17,
               143 * sizeof(float));
        memcpy(q->pitch_pre_filter_mem, cdn_vector + 17, 143 * sizeof(float));
        memset(q->pitch_gain, 0, sizeof(q->pitch_gain));
        memset(q->pitch_lag,  0, sizeof(q->pitch_lag));
    }
}

/**
 * Interpolates LSP frequencies and computes LPC coefficients
 * for a given bitrate & pitch subframe.
 *
 * TIA/EIA/IS-733 2.4.3.3.4
 *
 * @param q the context
 * @param curr_lspf LSP frequencies vector of the current frame
 * @param lpc float vector for the resulting LPC
 * @param subframe_num frame number in decoded stream
 */
void interpolate_lpc(QCELPContext *q, const float *curr_lspf, float *lpc,
                     const int subframe_num)
{
    float interpolated_lspf[10];
    float weight;

    if(q->bitrate >= RATE_QUARTER)
        weight = 0.25 * (subframe_num + 1);
    else if(q->bitrate == RATE_OCTAVE && !subframe_num)
        weight = 0.625;
    else
        weight = 1.0;

    if(weight != 1.0)
    {
        weighted_vector_sumf(interpolated_lspf, curr_lspf, q->prev_lspf,
                             weight, 1.0 - weight, 10);
        ff_qcelp_lspf2lpc(interpolated_lspf, lpc);
    }else if(q->bitrate >= RATE_QUARTER ||
             (q->bitrate == I_F_Q && !subframe_num))
        ff_qcelp_lspf2lpc(curr_lspf, lpc);
}

static qcelp_packet_rate buf_size2bitrate(const int buf_size)
{
    switch(buf_size)
    {
        case 35: return RATE_FULL;
        case 17: return RATE_HALF;
        case  8: return RATE_QUARTER;
        case  4: return RATE_OCTAVE;
        case  1: return SILENCE;
    }

    return I_F_Q;
}

/**
 * Determine the bitrate from the frame size and/or the first byte of the frame.
 *
 * @param avctx the AV codec context
 * @param buf_size length of the buffer
 * @param buf the bufffer
 *
 * @return the bitrate on success,
 *         I_F_Q  if the bitrate cannot be satisfactorily determined
 *
 * TIA/EIA/IS-733 2.4.8.7.1
 */
static int determine_bitrate(AVCodecContext *avctx, const int buf_size,
                             const uint8_t **buf)
{
    qcelp_packet_rate bitrate;

    if((bitrate = buf_size2bitrate(buf_size)) >= 0)
    {
        if(bitrate > **buf)
        {
            av_log(avctx, AV_LOG_WARNING,
                   "Claimed bitrate and buffer size mismatch.\n");
            bitrate = **buf;
        }else if(bitrate < **buf)
        {
            av_log(avctx, AV_LOG_ERROR,
                   "Buffer is too small for the claimed bitrate.\n");
            return I_F_Q;
        }
        (*buf)++;
    }else if((bitrate = buf_size2bitrate(buf_size + 1)) >= 0)
    {
        av_log(avctx, AV_LOG_WARNING,
               "Bitrate byte is missing, guessing the bitrate from packet size.\n");
    }else
        return I_F_Q;

    if(bitrate == SILENCE)
    {
        // FIXME: the decoder should not handle SILENCE frames as I_F_Q frames
        ff_log_missing_feature(avctx, "Blank frame", 1);
        bitrate = I_F_Q;
    }
    return bitrate;
}

static void warn_insufficient_frame_quality(AVCodecContext *avctx,
                                            const char *message)
{
    av_log(avctx, AV_LOG_WARNING, "Frame #%d, IFQ: %s\n", avctx->frame_number,
           message);
}

static int qcelp_decode_frame(AVCodecContext *avctx, void *data, int *data_size,
                              const uint8_t *buf, int buf_size)
{
    QCELPContext *q = avctx->priv_data;
    float *outbuffer = data;
    int   i;
    float quantized_lspf[10], lpc[10];
    float gain[16];
    float *formant_mem;

    if((q->bitrate = determine_bitrate(avctx, buf_size, &buf)) == I_F_Q)
    {
        warn_insufficient_frame_quality(avctx, "bitrate cannot be determined.");
        goto erasure;
    }

    if(q->bitrate == RATE_OCTAVE &&
       (q->first16bits = AV_RB16(buf)) == 0xFFFF)
    {
        warn_insufficient_frame_quality(avctx, "Bitrate is 1/8 and first 16 bits are on.");
        goto erasure;
    }

    if(q->bitrate > SILENCE)
    {
        const QCELPBitmap *bitmaps     = qcelp_unpacking_bitmaps_per_rate[q->bitrate];
        const QCELPBitmap *bitmaps_end = qcelp_unpacking_bitmaps_per_rate[q->bitrate]
                                       + qcelp_unpacking_bitmaps_lengths[q->bitrate];
        uint8_t           *unpacked_data = (uint8_t *)&q->frame;

        init_get_bits(&q->gb, buf, 8*buf_size);

        memset(&q->frame, 0, sizeof(QCELPFrame));

        for(; bitmaps < bitmaps_end; bitmaps++)
            unpacked_data[bitmaps->index] |= get_bits(&q->gb, bitmaps->bitlen) << bitmaps->bitpos;

        // Check for erasures/blanks on rates 1, 1/4 and 1/8.
        if(q->frame.reserved)
        {
            warn_insufficient_frame_quality(avctx, "Wrong data in reserved frame area.");
            goto erasure;
        }
        if(q->bitrate == RATE_QUARTER &&
           codebook_sanity_check_for_rate_quarter(q->frame.cbgain))
        {
            warn_insufficient_frame_quality(avctx, "Codebook gain sanity check failed.");
            goto erasure;
        }

        if(q->bitrate >= RATE_HALF)
        {
            for(i=0; i<4; i++)
            {
                if(q->frame.pfrac[i] && q->frame.plag[i] >= 124)
                {
                    warn_insufficient_frame_quality(avctx, "Cannot initialize pitch filter.");
                    goto erasure;
                }
            }
        }
    }

    decode_gain_and_index(q, gain);
    compute_svector(q, gain, outbuffer);

    if(decode_lspf(q, quantized_lspf) < 0)
    {
        warn_insufficient_frame_quality(avctx, "Badly received packets in frame.");
        goto erasure;
    }


    apply_pitch_filters(q, outbuffer);

    if(q->bitrate == I_F_Q)
    {
erasure:
        q->bitrate = I_F_Q;
        q->erasure_count++;
        decode_gain_and_index(q, gain);
        compute_svector(q, gain, outbuffer);
        decode_lspf(q, quantized_lspf);
        apply_pitch_filters(q, outbuffer);
    }else
        q->erasure_count = 0;

    formant_mem = q->formant_mem + 10;
    for(i=0; i<4; i++)
    {
        interpolate_lpc(q, quantized_lspf, lpc, i);
        ff_celp_lp_synthesis_filterf(formant_mem, lpc, outbuffer + i * 40, 40,
                                     10);
        formant_mem += 40;
    }
    memcpy(q->formant_mem, q->formant_mem + 160, 10 * sizeof(float));

    // FIXME: postfilter and final gain control should be here.
    // TIA/EIA/IS-733 2.4.8.6

    formant_mem = q->formant_mem + 10;
    for(i=0; i<160; i++)
        *outbuffer++ = av_clipf(*formant_mem++, QCELP_CLIP_LOWER_BOUND,
                                QCELP_CLIP_UPPER_BOUND);

    memcpy(q->prev_lspf, quantized_lspf, sizeof(q->prev_lspf));
    q->prev_bitrate = q->bitrate;

    *data_size = 160 * sizeof(*outbuffer);

    return *data_size;
}

AVCodec qcelp_decoder =
{
    .name   = "qcelp",
    .type   = CODEC_TYPE_AUDIO,
    .id     = CODEC_ID_QCELP,
    .init   = qcelp_decode_init,
    .decode = qcelp_decode_frame,
    .priv_data_size = sizeof(QCELPContext),
    .long_name = NULL_IF_CONFIG_SMALL("QCELP / PureVoice"),
};
