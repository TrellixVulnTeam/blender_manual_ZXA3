/*
 * RV30/40 decoder common data
 * Copyright (c) 2007 Mike Melanson, Konstantin Shishkov
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
 * @file rv34.c
 * RV30/40 decoder common data
 */

#include "avcodec.h"
#include "dsputil.h"
#include "mpegvideo.h"
#include "golomb.h"
#include "rectangle.h"

#include "rv34vlc.h"
#include "rv34data.h"
#include "rv34.h"

//#define DEBUG

/** translation of RV30/40 macroblock types to lavc ones */
static const int rv34_mb_type_to_lavc[12] = {
    MB_TYPE_INTRA,
    MB_TYPE_INTRA16x16              | MB_TYPE_SEPARATE_DC,
    MB_TYPE_16x16   | MB_TYPE_L0,
    MB_TYPE_8x8     | MB_TYPE_L0,
    MB_TYPE_16x16   | MB_TYPE_L0,
    MB_TYPE_16x16   | MB_TYPE_L1,
    MB_TYPE_SKIP,
    MB_TYPE_DIRECT2 | MB_TYPE_16x16,
    MB_TYPE_16x8    | MB_TYPE_L0,
    MB_TYPE_8x16    | MB_TYPE_L0,
    MB_TYPE_16x16   | MB_TYPE_L0L1,
    MB_TYPE_16x16   | MB_TYPE_L0    | MB_TYPE_SEPARATE_DC
};


static RV34VLC intra_vlcs[NUM_INTRA_TABLES], inter_vlcs[NUM_INTER_TABLES];

/**
 * @defgroup vlc RV30/40 VLC generating functions
 * @{
 */

/**
 * Generate VLC from codeword lengths.
 * @param bits   codeword lengths (zeroes are accepted)
 * @param size   length of input data
 * @param insyms symbols for input codes (NULL for default ones)
 */
static void rv34_gen_vlc(const uint8_t *bits, int size, VLC *vlc, const uint8_t *insyms)
{
    int i;
    int counts[17] = {0}, codes[17];
    uint16_t cw[size], syms[size];
    uint8_t bits2[size];
    int maxbits = 0, realsize = 0;

    for(i = 0; i < size; i++){
        if(bits[i]){
            bits2[realsize] = bits[i];
            syms[realsize] = insyms ? insyms[i] : i;
            realsize++;
            maxbits = FFMAX(maxbits, bits[i]);
            counts[bits[i]]++;
        }
    }

    codes[0] = 0;
    for(i = 0; i < 16; i++)
        codes[i+1] = (codes[i] + counts[i]) << 1;
    for(i = 0; i < realsize; i++)
        cw[i] = codes[bits2[i]]++;

    init_vlc_sparse(vlc, FFMIN(maxbits, 9), realsize,
                    bits2, 1, 1,
                    cw,    2, 2,
                    syms,  2, 2, INIT_VLC_USE_STATIC);
}

/**
 * Initialize all tables.
 */
static av_cold void rv34_init_tables()
{
    int i, j, k;

    for(i = 0; i < NUM_INTRA_TABLES; i++){
        for(j = 0; j < 2; j++){
            rv34_gen_vlc(rv34_table_intra_cbppat   [i][j], CBPPAT_VLC_SIZE,   &intra_vlcs[i].cbppattern[j],     NULL);
            rv34_gen_vlc(rv34_table_intra_secondpat[i][j], OTHERBLK_VLC_SIZE, &intra_vlcs[i].second_pattern[j], NULL);
            rv34_gen_vlc(rv34_table_intra_thirdpat [i][j], OTHERBLK_VLC_SIZE, &intra_vlcs[i].third_pattern[j],  NULL);
            for(k = 0; k < 4; k++)
                rv34_gen_vlc(rv34_table_intra_cbp[i][j+k*2],  CBP_VLC_SIZE,   &intra_vlcs[i].cbp[j][k],         rv34_cbp_code);
        }
        for(j = 0; j < 4; j++)
            rv34_gen_vlc(rv34_table_intra_firstpat[i][j], FIRSTBLK_VLC_SIZE, &intra_vlcs[i].first_pattern[j], NULL);
        rv34_gen_vlc(rv34_intra_coeff[i], COEFF_VLC_SIZE, &intra_vlcs[i].coefficient, NULL);
    }

    for(i = 0; i < NUM_INTER_TABLES; i++){
        rv34_gen_vlc(rv34_inter_cbppat[i], CBPPAT_VLC_SIZE, &inter_vlcs[i].cbppattern[0], NULL);
        for(j = 0; j < 4; j++)
            rv34_gen_vlc(rv34_inter_cbp[i][j], CBP_VLC_SIZE, &inter_vlcs[i].cbp[0][j], rv34_cbp_code);
        for(j = 0; j < 2; j++){
            rv34_gen_vlc(rv34_table_inter_firstpat [i][j], FIRSTBLK_VLC_SIZE, &inter_vlcs[i].first_pattern[j],  NULL);
            rv34_gen_vlc(rv34_table_inter_secondpat[i][j], OTHERBLK_VLC_SIZE, &inter_vlcs[i].second_pattern[j], NULL);
            rv34_gen_vlc(rv34_table_inter_thirdpat [i][j], OTHERBLK_VLC_SIZE, &inter_vlcs[i].third_pattern[j],  NULL);
        }
        rv34_gen_vlc(rv34_inter_coeff[i], COEFF_VLC_SIZE, &inter_vlcs[i].coefficient, NULL);
    }
}

/** @} */ // vlc group


/**
 * @defgroup transform RV30/40 inverse transform functions
 * @{
 */

static av_always_inline void rv34_row_transform(int temp[16], DCTELEM *block)
{
    int i;

    for(i=0; i<4; i++){
        const int z0= 13*(block[i+8*0] +    block[i+8*2]);
        const int z1= 13*(block[i+8*0] -    block[i+8*2]);
        const int z2=  7* block[i+8*1] - 17*block[i+8*3];
        const int z3= 17* block[i+8*1] +  7*block[i+8*3];

        temp[4*i+0]= z0+z3;
        temp[4*i+1]= z1+z2;
        temp[4*i+2]= z1-z2;
        temp[4*i+3]= z0-z3;
    }
}

/**
 * Real Video 3.0/4.0 inverse transform
 * Code is almost the same as in SVQ3, only scaling is different.
 */
static void rv34_inv_transform(DCTELEM *block){
    int temp[16];
    int i;

    rv34_row_transform(temp, block);

    for(i=0; i<4; i++){
        const int z0= 13*(temp[4*0+i] +    temp[4*2+i]) + 0x200;
        const int z1= 13*(temp[4*0+i] -    temp[4*2+i]) + 0x200;
        const int z2=  7* temp[4*1+i] - 17*temp[4*3+i];
        const int z3= 17* temp[4*1+i] +  7*temp[4*3+i];

        block[i*8+0]= (z0 + z3)>>10;
        block[i*8+1]= (z1 + z2)>>10;
        block[i*8+2]= (z1 - z2)>>10;
        block[i*8+3]= (z0 - z3)>>10;
    }

}

/**
 * RealVideo 3.0/4.0 inverse transform for DC block
 *
 * Code is almost the same as rv34_inv_transform()
 * but final coefficients are multiplied by 1.5 and have no rounding.
 */
static void rv34_inv_transform_noround(DCTELEM *block){
    int temp[16];
    int i;

    rv34_row_transform(temp, block);

    for(i=0; i<4; i++){
        const int z0= 13*(temp[4*0+i] +    temp[4*2+i]);
        const int z1= 13*(temp[4*0+i] -    temp[4*2+i]);
        const int z2=  7* temp[4*1+i] - 17*temp[4*3+i];
        const int z3= 17* temp[4*1+i] +  7*temp[4*3+i];

        block[i*8+0]= ((z0 + z3)*3)>>11;
        block[i*8+1]= ((z1 + z2)*3)>>11;
        block[i*8+2]= ((z1 - z2)*3)>>11;
        block[i*8+3]= ((z0 - z3)*3)>>11;
    }

}

/** @} */ // transform


/**
 * @defgroup block RV30/40 4x4 block decoding functions
 * @{
 */

/**
 * Decode coded block pattern.
 */
static int rv34_decode_cbp(GetBitContext *gb, RV34VLC *vlc, int table)
{
    int pattern, code, cbp=0;
    int ones;
    static const int cbp_masks[3] = {0x100000, 0x010000, 0x110000};
    static const int shifts[4] = { 0, 2, 8, 10 };
    int *curshift = shifts;
    int i, t, mask;

    code = get_vlc2(gb, vlc->cbppattern[table].table, 9, 2);
    pattern = code & 0xF;
    code >>= 4;

    ones = rv34_count_ones[pattern];

    for(mask = 8; mask; mask >>= 1, curshift++){
        if(pattern & mask)
            cbp |= get_vlc2(gb, vlc->cbp[table][ones].table, vlc->cbp[table][ones].bits, 1) << curshift[0];
    }

    for(i = 0; i < 4; i++){
        t = modulo_three_table[code][i];
        if(t == 1)
            cbp |= cbp_masks[get_bits1(gb)] << i;
        if(t == 2)
            cbp |= cbp_masks[2] << i;
    }
    return cbp;
}

/**
 * Get one coefficient value from the bistream and store it.
 */
static inline void decode_coeff(DCTELEM *dst, int coef, int esc, GetBitContext *gb, VLC* vlc)
{
    if(coef){
        if(coef == esc){
            coef = get_vlc2(gb, vlc->table, 9, 2);
            if(coef > 23){
                coef -= 23;
                coef = 22 + ((1 << coef) | get_bits(gb, coef));
            }
            coef += esc;
        }
        if(get_bits1(gb))
            coef = -coef;
        *dst = coef;
    }
}

/**
 * Decode 2x2 subblock of coefficients.
 */
static inline void decode_subblock(DCTELEM *dst, int code, const int is_block2, GetBitContext *gb, VLC *vlc)
{
    int coeffs[4];

    coeffs[0] = modulo_three_table[code][0];
    coeffs[1] = modulo_three_table[code][1];
    coeffs[2] = modulo_three_table[code][2];
    coeffs[3] = modulo_three_table[code][3];
    decode_coeff(dst  , coeffs[0], 3, gb, vlc);
    if(is_block2){
        decode_coeff(dst+8, coeffs[1], 2, gb, vlc);
        decode_coeff(dst+1, coeffs[2], 2, gb, vlc);
    }else{
        decode_coeff(dst+1, coeffs[1], 2, gb, vlc);
        decode_coeff(dst+8, coeffs[2], 2, gb, vlc);
    }
    decode_coeff(dst+9, coeffs[3], 2, gb, vlc);
}

/**
 * Decode coefficients for 4x4 block.
 *
 * This is done by filling 2x2 subblocks with decoded coefficients
 * in this order (the same for subblocks and subblock coefficients):
 *  o--o
 *    /
 *   /
 *  o--o
 */

static inline void rv34_decode_block(DCTELEM *dst, GetBitContext *gb, RV34VLC *rvlc, int fc, int sc)
{
    int code, pattern;

    code = get_vlc2(gb, rvlc->first_pattern[fc].table, 9, 2);

    pattern = code & 0x7;

    code >>= 3;
    decode_subblock(dst, code, 0, gb, &rvlc->coefficient);

    if(pattern & 4){
        code = get_vlc2(gb, rvlc->second_pattern[sc].table, 9, 2);
        decode_subblock(dst + 2, code, 0, gb, &rvlc->coefficient);
    }
    if(pattern & 2){ // Looks like coefficients 1 and 2 are swapped for this block
        code = get_vlc2(gb, rvlc->second_pattern[sc].table, 9, 2);
        decode_subblock(dst + 8*2, code, 1, gb, &rvlc->coefficient);
    }
    if(pattern & 1){
        code = get_vlc2(gb, rvlc->third_pattern[sc].table, 9, 2);
        decode_subblock(dst + 8*2+2, code, 0, gb, &rvlc->coefficient);
    }

}

/**
 * Dequantize ordinary 4x4 block.
 * @todo optimize
 */
static inline void rv34_dequant4x4(DCTELEM *block, int Qdc, int Q)
{
    int i, j;

    block[0] = (block[0] * Qdc + 8) >> 4;
    for(i = 0; i < 4; i++)
        for(j = !i; j < 4; j++)
            block[j + i*8] = (block[j + i*8] * Q + 8) >> 4;
}

/**
 * Dequantize 4x4 block of DC values for 16x16 macroblock.
 * @todo optimize
 */
static inline void rv34_dequant4x4_16x16(DCTELEM *block, int Qdc, int Q)
{
    int i;

    for(i = 0; i < 3; i++)
         block[rv34_dezigzag[i]] = (block[rv34_dezigzag[i]] * Qdc + 8) >> 4;
    for(; i < 16; i++)
         block[rv34_dezigzag[i]] = (block[rv34_dezigzag[i]] * Q + 8) >> 4;
}
/** @} */ //block functions


/**
 * @defgroup bitstream RV30/40 bitstream parsing
 * @{
 */

/**
 * Decode starting slice position.
 * @todo Maybe replace with ff_h263_decode_mba() ?
 */
int ff_rv34_get_start_offset(GetBitContext *gb, int mb_size)
{
    int i;
    for(i = 0; i < 5; i++)
        if(rv34_mb_max_sizes[i] > mb_size)
            break;
    return rv34_mb_bits_sizes[i];
}

/**
 * Select VLC set for decoding from current quantizer, modifier and frame type.
 */
static inline RV34VLC* choose_vlc_set(int quant, int mod, int type)
{
    if(mod == 2 && quant < 19) quant += 10;
    else if(mod && quant < 26) quant += 5;
    return type ? &inter_vlcs[rv34_quant_to_vlc_set[1][av_clip(quant, 0, 30)]]
                : &intra_vlcs[rv34_quant_to_vlc_set[0][av_clip(quant, 0, 30)]];
}

/**
 * Decode quantizer difference and return modified quantizer.
 */
static inline int rv34_decode_dquant(GetBitContext *gb, int quant)
{
    if(get_bits1(gb))
        return rv34_dquant_tab[get_bits1(gb)][quant];
    else
        return get_bits(gb, 5);
}

/** @} */ //bitstream functions

/**
 * @defgroup mv motion vector related code (prediction, reconstruction, motion compensation)
 * @{
 */

/** macroblock partition width in 8x8 blocks */
static const uint8_t part_sizes_w[RV34_MB_TYPES] = { 2, 2, 2, 1, 2, 2, 2, 2, 2, 1, 2, 2 };

/** macroblock partition height in 8x8 blocks */
static const uint8_t part_sizes_h[RV34_MB_TYPES] = { 2, 2, 2, 1, 2, 2, 2, 2, 1, 2, 2, 2 };

/** availability index for subblocks */
static const uint8_t avail_indexes[4] = { 5, 6, 9, 10 };

/**
 * motion vector prediction
 *
 * Motion prediction performed for the block by using median prediction of
 * motion vectors from the left, top and right top blocks but in corner cases
 * some other vectors may be used instead.
 */
static void rv34_pred_mv(RV34DecContext *r, int block_type, int subblock_no, int dmv_no)
{
    MpegEncContext *s = &r->s;
    int mv_pos = s->mb_x * 2 + s->mb_y * 2 * s->b8_stride;
    int A[2] = {0}, B[2], C[2];
    int i, j;
    int mx, my;
    int avail_index = avail_indexes[subblock_no];
    int c_off = part_sizes_w[block_type];

    mv_pos += (subblock_no & 1) + (subblock_no >> 1)*s->b8_stride;
    if(subblock_no == 3)
        c_off = -1;

    if(r->avail_cache[avail_index - 1]){
        A[0] = s->current_picture_ptr->motion_val[0][mv_pos-1][0];
        A[1] = s->current_picture_ptr->motion_val[0][mv_pos-1][1];
    }
    if(r->avail_cache[avail_index - 4]){
        B[0] = s->current_picture_ptr->motion_val[0][mv_pos-s->b8_stride][0];
        B[1] = s->current_picture_ptr->motion_val[0][mv_pos-s->b8_stride][1];
    }else{
        B[0] = A[0];
        B[1] = A[1];
    }
    if(!r->avail_cache[avail_index - 4 + c_off]){
        if(r->avail_cache[avail_index - 4] && (r->avail_cache[avail_index - 1] || r->rv30)){
            C[0] = s->current_picture_ptr->motion_val[0][mv_pos-s->b8_stride-1][0];
            C[1] = s->current_picture_ptr->motion_val[0][mv_pos-s->b8_stride-1][1];
        }else{
            C[0] = A[0];
            C[1] = A[1];
        }
    }else{
        C[0] = s->current_picture_ptr->motion_val[0][mv_pos-s->b8_stride+c_off][0];
        C[1] = s->current_picture_ptr->motion_val[0][mv_pos-s->b8_stride+c_off][1];
    }
    mx = mid_pred(A[0], B[0], C[0]);
    my = mid_pred(A[1], B[1], C[1]);
    mx += r->dmv[dmv_no][0];
    my += r->dmv[dmv_no][1];
    for(j = 0; j < part_sizes_h[block_type]; j++){
        for(i = 0; i < part_sizes_w[block_type]; i++){
            s->current_picture_ptr->motion_val[0][mv_pos + i + j*s->b8_stride][0] = mx;
            s->current_picture_ptr->motion_val[0][mv_pos + i + j*s->b8_stride][1] = my;
        }
    }
}

#define GET_PTS_DIFF(a, b) ((a - b + 8192) & 0x1FFF)

/**
 * Calculate motion vector component that should be added for direct blocks.
 */
static int calc_add_mv(RV34DecContext *r, int dir, int val)
{
    int refdist = GET_PTS_DIFF(r->next_pts, r->last_pts);
    int dist = dir ? -GET_PTS_DIFF(r->next_pts, r->cur_pts) : GET_PTS_DIFF(r->cur_pts, r->last_pts);
    int mul;

    if(!refdist) return 0;
    mul = (dist << 14) / refdist;
    return (val * mul + 0x2000) >> 14;
}

/**
 * Predict motion vector for B-frame macroblock.
 */
static inline void rv34_pred_b_vector(int A[2], int B[2], int C[2],
                                      int A_avail, int B_avail, int C_avail,
                                      int *mx, int *my)
{
    if(A_avail + B_avail + C_avail != 3){
        *mx = A[0] + B[0] + C[0];
        *my = A[1] + B[1] + C[1];
        if(A_avail + B_avail + C_avail == 2){
            *mx /= 2;
            *my /= 2;
        }
    }else{
        *mx = mid_pred(A[0], B[0], C[0]);
        *my = mid_pred(A[1], B[1], C[1]);
    }
}

/**
 * motion vector prediction for B-frames
 */
static void rv34_pred_mv_b(RV34DecContext *r, int block_type, int dir)
{
    MpegEncContext *s = &r->s;
    int mb_pos = s->mb_x + s->mb_y * s->mb_stride;
    int mv_pos = s->mb_x * 2 + s->mb_y * 2 * s->b8_stride;
    int A[2], B[2], C[2];
    int has_A = 0, has_B = 0, has_C = 0;
    int mx, my;
    int i, j;
    Picture *cur_pic = s->current_picture_ptr;
    const int mask = dir ? MB_TYPE_L1 : MB_TYPE_L0;
    int type = cur_pic->mb_type[mb_pos];

    memset(A, 0, sizeof(A));
    memset(B, 0, sizeof(B));
    memset(C, 0, sizeof(C));
    if((r->avail_cache[5-1] & type) & mask){
        A[0] = cur_pic->motion_val[dir][mv_pos - 1][0];
        A[1] = cur_pic->motion_val[dir][mv_pos - 1][1];
        has_A = 1;
    }
    if((r->avail_cache[5-4] & type) & mask){
        B[0] = cur_pic->motion_val[dir][mv_pos - s->b8_stride][0];
        B[1] = cur_pic->motion_val[dir][mv_pos - s->b8_stride][1];
        has_B = 1;
    }
    if((r->avail_cache[5-2] & type) & mask){
        C[0] = cur_pic->motion_val[dir][mv_pos - s->b8_stride + 2][0];
        C[1] = cur_pic->motion_val[dir][mv_pos - s->b8_stride + 2][1];
        has_C = 1;
    }else if((s->mb_x+1) == s->mb_width && (r->avail_cache[5-5] & type) & mask){
        C[0] = cur_pic->motion_val[dir][mv_pos - s->b8_stride - 1][0];
        C[1] = cur_pic->motion_val[dir][mv_pos - s->b8_stride - 1][1];
        has_C = 1;
    }

    rv34_pred_b_vector(A, B, C, has_A, has_B, has_C, &mx, &my);

    mx += r->dmv[dir][0];
    my += r->dmv[dir][1];

    for(j = 0; j < 2; j++){
        for(i = 0; i < 2; i++){
            cur_pic->motion_val[dir][mv_pos + i + j*s->b8_stride][0] = mx;
            cur_pic->motion_val[dir][mv_pos + i + j*s->b8_stride][1] = my;
        }
    }
    if(block_type == RV34_MB_B_BACKWARD || block_type == RV34_MB_B_FORWARD)
        fill_rectangle(cur_pic->motion_val[!dir][mv_pos], 2, 2, s->b8_stride, 0, 4);
}

/**
 * motion vector prediction - RV3 version
 */
static void rv34_pred_mv_rv3(RV34DecContext *r, int block_type, int dir)
{
    MpegEncContext *s = &r->s;
    int mv_pos = s->mb_x * 2 + s->mb_y * 2 * s->b8_stride;
    int A[2] = {0}, B[2], C[2];
    int i, j;
    int mx, my;
    int avail_index = avail_indexes[0];

    if(r->avail_cache[avail_index - 1]){
        A[0] = s->current_picture_ptr->motion_val[0][mv_pos-1][0];
        A[1] = s->current_picture_ptr->motion_val[0][mv_pos-1][1];
    }
    if(r->avail_cache[avail_index - 4]){
        B[0] = s->current_picture_ptr->motion_val[0][mv_pos-s->b8_stride][0];
        B[1] = s->current_picture_ptr->motion_val[0][mv_pos-s->b8_stride][1];
    }else{
        B[0] = A[0];
        B[1] = A[1];
    }
    if(!r->avail_cache[avail_index - 4 + 2]){
        if(r->avail_cache[avail_index - 4] && (r->avail_cache[avail_index - 1])){
            C[0] = s->current_picture_ptr->motion_val[0][mv_pos-s->b8_stride-1][0];
            C[1] = s->current_picture_ptr->motion_val[0][mv_pos-s->b8_stride-1][1];
        }else{
            C[0] = A[0];
            C[1] = A[1];
        }
    }else{
        C[0] = s->current_picture_ptr->motion_val[0][mv_pos-s->b8_stride+2][0];
        C[1] = s->current_picture_ptr->motion_val[0][mv_pos-s->b8_stride+2][1];
    }
    mx = mid_pred(A[0], B[0], C[0]);
    my = mid_pred(A[1], B[1], C[1]);
    mx += r->dmv[0][0];
    my += r->dmv[0][1];
    for(j = 0; j < 2; j++){
        for(i = 0; i < 2; i++){
            s->current_picture_ptr->motion_val[0][mv_pos + i + j*s->b8_stride][0] = mx;
            s->current_picture_ptr->motion_val[0][mv_pos + i + j*s->b8_stride][1] = my;
        }
    }
    if(block_type == RV34_MB_B_BACKWARD || block_type == RV34_MB_B_FORWARD)
        fill_rectangle(s->current_picture_ptr->motion_val[!dir][mv_pos], 2, 2, s->b8_stride, 0, 4);
}

static const int chroma_coeffs[3] = { 0, 3, 5 };

/**
 * generic motion compensation function
 *
 * @param r decoder context
 * @param block_type type of the current block
 * @param xoff horizontal offset from the start of the current block
 * @param yoff vertical offset from the start of the current block
 * @param mv_off offset to the motion vector information
 * @param width width of the current partition in 8x8 blocks
 * @param height height of the current partition in 8x8 blocks
 */
static inline void rv34_mc(RV34DecContext *r, const int block_type,
                          const int xoff, const int yoff, int mv_off,
                          const int width, const int height, int dir,
                          const int thirdpel,
                          qpel_mc_func (*qpel_mc)[16],
                          h264_chroma_mc_func (*chroma_mc))
{
    MpegEncContext *s = &r->s;
    uint8_t *Y, *U, *V, *srcY, *srcU, *srcV;
    int dxy, mx, my, umx, umy, lx, ly, uvmx, uvmy, src_x, src_y, uvsrc_x, uvsrc_y;
    int mv_pos = s->mb_x * 2 + s->mb_y * 2 * s->b8_stride + mv_off;
    int is16x16 = 1;

    if(thirdpel){
        int chroma_mx, chroma_my;
        mx = (s->current_picture_ptr->motion_val[dir][mv_pos][0] + (3 << 24)) / 3 - (1 << 24);
        my = (s->current_picture_ptr->motion_val[dir][mv_pos][1] + (3 << 24)) / 3 - (1 << 24);
        lx = (s->current_picture_ptr->motion_val[dir][mv_pos][0] + (3 << 24)) % 3;
        ly = (s->current_picture_ptr->motion_val[dir][mv_pos][1] + (3 << 24)) % 3;
        chroma_mx = (s->current_picture_ptr->motion_val[dir][mv_pos][0] + 1) >> 1;
        chroma_my = (s->current_picture_ptr->motion_val[dir][mv_pos][1] + 1) >> 1;
        umx = (chroma_mx + (3 << 24)) / 3 - (1 << 24);
        umy = (chroma_my + (3 << 24)) / 3 - (1 << 24);
        uvmx = chroma_coeffs[(chroma_mx + (3 << 24)) % 3];
        uvmy = chroma_coeffs[(chroma_my + (3 << 24)) % 3];
    }else{
        int cx, cy;
        mx = s->current_picture_ptr->motion_val[dir][mv_pos][0] >> 2;
        my = s->current_picture_ptr->motion_val[dir][mv_pos][1] >> 2;
        lx = s->current_picture_ptr->motion_val[dir][mv_pos][0] & 3;
        ly = s->current_picture_ptr->motion_val[dir][mv_pos][1] & 3;
        cx = s->current_picture_ptr->motion_val[dir][mv_pos][0] / 2;
        cy = s->current_picture_ptr->motion_val[dir][mv_pos][1] / 2;
        umx = cx >> 2;
        umy = cy >> 2;
        uvmx = (cx & 3) << 1;
        uvmy = (cy & 3) << 1;
        //due to some flaw RV40 uses the same MC compensation routine for H2V2 and H3V3
        if(uvmx == 6 && uvmy == 6)
            uvmx = uvmy = 4;
    }
    dxy = ly*4 + lx;
    srcY = dir ? s->next_picture_ptr->data[0] : s->last_picture_ptr->data[0];
    srcU = dir ? s->next_picture_ptr->data[1] : s->last_picture_ptr->data[1];
    srcV = dir ? s->next_picture_ptr->data[2] : s->last_picture_ptr->data[2];
    src_x = s->mb_x * 16 + xoff + mx;
    src_y = s->mb_y * 16 + yoff + my;
    uvsrc_x = s->mb_x * 8 + (xoff >> 1) + umx;
    uvsrc_y = s->mb_y * 8 + (yoff >> 1) + umy;
    srcY += src_y * s->linesize + src_x;
    srcU += uvsrc_y * s->uvlinesize + uvsrc_x;
    srcV += uvsrc_y * s->uvlinesize + uvsrc_x;
    if(   (unsigned)(src_x - !!lx*2) > s->h_edge_pos - !!lx*2 - (width <<3) - 4
       || (unsigned)(src_y - !!ly*2) > s->v_edge_pos - !!ly*2 - (height<<3) - 4){
        uint8_t *uvbuf= s->edge_emu_buffer + 22 * s->linesize;

        srcY -= 2 + 2*s->linesize;
        ff_emulated_edge_mc(s->edge_emu_buffer, srcY, s->linesize, (width<<3)+6, (height<<3)+6,
                            src_x - 2, src_y - 2, s->h_edge_pos, s->v_edge_pos);
        srcY = s->edge_emu_buffer + 2 + 2*s->linesize;
        ff_emulated_edge_mc(uvbuf     , srcU, s->uvlinesize, (width<<2)+1, (height<<2)+1,
                            uvsrc_x, uvsrc_y, s->h_edge_pos >> 1, s->v_edge_pos >> 1);
        ff_emulated_edge_mc(uvbuf + 16, srcV, s->uvlinesize, (width<<2)+1, (height<<2)+1,
                            uvsrc_x, uvsrc_y, s->h_edge_pos >> 1, s->v_edge_pos >> 1);
        srcU = uvbuf;
        srcV = uvbuf + 16;
    }
    Y = s->dest[0] + xoff      + yoff     *s->linesize;
    U = s->dest[1] + (xoff>>1) + (yoff>>1)*s->uvlinesize;
    V = s->dest[2] + (xoff>>1) + (yoff>>1)*s->uvlinesize;

    if(block_type == RV34_MB_P_16x8){
        qpel_mc[1][dxy](Y, srcY, s->linesize);
        Y    += 8;
        srcY += 8;
    }else if(block_type == RV34_MB_P_8x16){
        qpel_mc[1][dxy](Y, srcY, s->linesize);
        Y    += 8 * s->linesize;
        srcY += 8 * s->linesize;
    }
    is16x16 = (block_type != RV34_MB_P_8x8) && (block_type != RV34_MB_P_16x8) && (block_type != RV34_MB_P_8x16);
    qpel_mc[!is16x16][dxy](Y, srcY, s->linesize);
    chroma_mc[2-width]   (U, srcU, s->uvlinesize, height*4, uvmx, uvmy);
    chroma_mc[2-width]   (V, srcV, s->uvlinesize, height*4, uvmx, uvmy);
}

static void rv34_mc_1mv(RV34DecContext *r, const int block_type,
                        const int xoff, const int yoff, int mv_off,
                        const int width, const int height, int dir)
{
    rv34_mc(r, block_type, xoff, yoff, mv_off, width, height, dir, r->rv30,
            r->rv30 ? r->s.dsp.put_rv30_tpel_pixels_tab
                    : r->s.dsp.put_rv40_qpel_pixels_tab,
            r->rv30 ? r->s.dsp.put_h264_chroma_pixels_tab
                    : r->s.dsp.put_rv40_chroma_pixels_tab);
}

static void rv34_mc_2mv(RV34DecContext *r, const int block_type)
{
    rv34_mc(r, block_type, 0, 0, 0, 2, 2, 0, r->rv30,
            r->rv30 ? r->s.dsp.put_rv30_tpel_pixels_tab
                    : r->s.dsp.put_rv40_qpel_pixels_tab,
            r->rv30 ? r->s.dsp.put_h264_chroma_pixels_tab
                    : r->s.dsp.put_rv40_chroma_pixels_tab);
    rv34_mc(r, block_type, 0, 0, 0, 2, 2, 1, r->rv30,
            r->rv30 ? r->s.dsp.avg_rv30_tpel_pixels_tab
                    : r->s.dsp.avg_rv40_qpel_pixels_tab,
            r->rv30 ? r->s.dsp.avg_h264_chroma_pixels_tab
                    : r->s.dsp.avg_rv40_chroma_pixels_tab);
}

static void rv34_mc_2mv_skip(RV34DecContext *r)
{
    int i, j;
    for(j = 0; j < 2; j++)
        for(i = 0; i < 2; i++){
             rv34_mc(r, RV34_MB_P_8x8, i*8, j*8, i+j*r->s.b8_stride, 1, 1, 0, r->rv30,
                    r->rv30 ? r->s.dsp.put_rv30_tpel_pixels_tab
                            : r->s.dsp.put_rv40_qpel_pixels_tab,
                    r->rv30 ? r->s.dsp.put_h264_chroma_pixels_tab
                            : r->s.dsp.put_rv40_chroma_pixels_tab);
             rv34_mc(r, RV34_MB_P_8x8, i*8, j*8, i+j*r->s.b8_stride, 1, 1, 1, r->rv30,
                    r->rv30 ? r->s.dsp.avg_rv30_tpel_pixels_tab
                            : r->s.dsp.avg_rv40_qpel_pixels_tab,
                    r->rv30 ? r->s.dsp.avg_h264_chroma_pixels_tab
                            : r->s.dsp.avg_rv40_chroma_pixels_tab);
        }
}

/** number of motion vectors in each macroblock type */
static const int num_mvs[RV34_MB_TYPES] = { 0, 0, 1, 4, 1, 1, 0, 0, 2, 2, 2, 1 };

/**
 * Decode motion vector differences
 * and perform motion vector reconstruction and motion compensation.
 */
static int rv34_decode_mv(RV34DecContext *r, int block_type)
{
    MpegEncContext *s = &r->s;
    GetBitContext *gb = &s->gb;
    int i, j, k, l;
    int mv_pos = s->mb_x * 2 + s->mb_y * 2 * s->b8_stride;
    int next_bt;

    memset(r->dmv, 0, sizeof(r->dmv));
    for(i = 0; i < num_mvs[block_type]; i++){
        r->dmv[i][0] = svq3_get_se_golomb(gb);
        r->dmv[i][1] = svq3_get_se_golomb(gb);
    }
    switch(block_type){
    case RV34_MB_TYPE_INTRA:
    case RV34_MB_TYPE_INTRA16x16:
        fill_rectangle(s->current_picture_ptr->motion_val[0][s->mb_x * 2 + s->mb_y * 2 * s->b8_stride], 2, 2, s->b8_stride, 0, 4);
        return 0;
    case RV34_MB_SKIP:
        if(s->pict_type == FF_P_TYPE){
            fill_rectangle(s->current_picture_ptr->motion_val[0][s->mb_x * 2 + s->mb_y * 2 * s->b8_stride], 2, 2, s->b8_stride, 0, 4);
            rv34_mc_1mv (r, block_type, 0, 0, 0, 2, 2, 0);
            break;
        }
    case RV34_MB_B_DIRECT:
        //surprisingly, it uses motion scheme from next reference frame
        next_bt = s->next_picture_ptr->mb_type[s->mb_x + s->mb_y * s->mb_stride];
        for(j = 0; j < 2; j++)
            for(i = 0; i < 2; i++)
                for(k = 0; k < 2; k++)
                    for(l = 0; l < 2; l++)
                        s->current_picture_ptr->motion_val[l][mv_pos + i + j*s->b8_stride][k] = calc_add_mv(r, l, s->next_picture_ptr->motion_val[0][mv_pos + i + j*s->b8_stride][k]);
        if(IS_16X16(next_bt)) //we can use whole macroblock MC
            rv34_mc_2mv(r, block_type);
        else
            rv34_mc_2mv_skip(r);
        fill_rectangle(s->current_picture_ptr->motion_val[0][s->mb_x * 2 + s->mb_y * 2 * s->b8_stride], 2, 2, s->b8_stride, 0, 4);
        break;
    case RV34_MB_P_16x16:
    case RV34_MB_P_MIX16x16:
        rv34_pred_mv(r, block_type, 0, 0);
        rv34_mc_1mv (r, block_type, 0, 0, 0, 2, 2, 0);
        break;
    case RV34_MB_B_FORWARD:
    case RV34_MB_B_BACKWARD:
        r->dmv[1][0] = r->dmv[0][0];
        r->dmv[1][1] = r->dmv[0][1];
        if(r->rv30)
            rv34_pred_mv_rv3(r, block_type, block_type == RV34_MB_B_BACKWARD);
        else
            rv34_pred_mv_b  (r, block_type, block_type == RV34_MB_B_BACKWARD);
        rv34_mc_1mv     (r, block_type, 0, 0, 0, 2, 2, block_type == RV34_MB_B_BACKWARD);
        break;
    case RV34_MB_P_16x8:
    case RV34_MB_P_8x16:
        rv34_pred_mv(r, block_type, 0, 0);
        rv34_pred_mv(r, block_type, 1 + (block_type == RV34_MB_P_16x8), 1);
        if(block_type == RV34_MB_P_16x8){
            rv34_mc_1mv(r, block_type, 0, 0, 0,            2, 1, 0);
            rv34_mc_1mv(r, block_type, 0, 8, s->b8_stride, 2, 1, 0);
        }
        if(block_type == RV34_MB_P_8x16){
            rv34_mc_1mv(r, block_type, 0, 0, 0, 1, 2, 0);
            rv34_mc_1mv(r, block_type, 8, 0, 1, 1, 2, 0);
        }
        break;
    case RV34_MB_B_BIDIR:
        rv34_pred_mv_b  (r, block_type, 0);
        rv34_pred_mv_b  (r, block_type, 1);
        rv34_mc_2mv     (r, block_type);
        break;
    case RV34_MB_P_8x8:
        for(i=0;i< 4;i++){
            rv34_pred_mv(r, block_type, i, i);
            rv34_mc_1mv (r, block_type, (i&1)<<3, (i&2)<<2, (i&1)+(i>>1)*s->b8_stride, 1, 1, 0);
        }
        break;
    }

    return 0;
}
/** @} */ // mv group

/**
 * @defgroup recons Macroblock reconstruction functions
 * @{
 */
/** mapping of RV30/40 intra prediction types to standard H.264 types */
static const int ittrans[9] = {
 DC_PRED, VERT_PRED, HOR_PRED, DIAG_DOWN_RIGHT_PRED, DIAG_DOWN_LEFT_PRED,
 VERT_RIGHT_PRED, VERT_LEFT_PRED, HOR_UP_PRED, HOR_DOWN_PRED,
};

/** mapping of RV30/40 intra 16x16 prediction types to standard H.264 types */
static const int ittrans16[4] = {
 DC_PRED8x8, VERT_PRED8x8, HOR_PRED8x8, PLANE_PRED8x8,
};

/**
 * Perform 4x4 intra prediction.
 */
static void rv34_pred_4x4_block(RV34DecContext *r, uint8_t *dst, int stride, int itype, int up, int left, int down, int right)
{
    uint8_t *prev = dst - stride + 4;
    uint32_t topleft;

    if(!up && !left)
        itype = DC_128_PRED;
    else if(!up){
        if(itype == VERT_PRED) itype = HOR_PRED;
        if(itype == DC_PRED)   itype = LEFT_DC_PRED;
    }else if(!left){
        if(itype == HOR_PRED)  itype = VERT_PRED;
        if(itype == DC_PRED)   itype = TOP_DC_PRED;
        if(itype == DIAG_DOWN_LEFT_PRED) itype = DIAG_DOWN_LEFT_PRED_RV40_NODOWN;
    }
    if(!down){
        if(itype == DIAG_DOWN_LEFT_PRED) itype = DIAG_DOWN_LEFT_PRED_RV40_NODOWN;
        if(itype == HOR_UP_PRED) itype = HOR_UP_PRED_RV40_NODOWN;
        if(itype == VERT_LEFT_PRED) itype = VERT_LEFT_PRED_RV40_NODOWN;
    }
    if(!right && up){
        topleft = dst[-stride + 3] * 0x01010101;
        prev = &topleft;
    }
    r->h.pred4x4[itype](dst, prev, stride);
}

/** add_pixels_clamped for 4x4 block */
static void rv34_add_4x4_block(uint8_t *dst, int stride, DCTELEM block[64], int off)
{
    int x, y;
    for(y = 0; y < 4; y++)
        for(x = 0; x < 4; x++)
            dst[x + y*stride] = av_clip_uint8(dst[x + y*stride] + block[off + x+y*8]);
}

static inline int adjust_pred16(int itype, int up, int left)
{
    if(!up && !left)
        itype = DC_128_PRED8x8;
    else if(!up){
        if(itype == PLANE_PRED8x8)itype = HOR_PRED8x8;
        if(itype == VERT_PRED8x8) itype = HOR_PRED8x8;
        if(itype == DC_PRED8x8)   itype = LEFT_DC_PRED8x8;
    }else if(!left){
        if(itype == PLANE_PRED8x8)itype = VERT_PRED8x8;
        if(itype == HOR_PRED8x8)  itype = VERT_PRED8x8;
        if(itype == DC_PRED8x8)   itype = TOP_DC_PRED8x8;
    }
    return itype;
}

static void rv34_output_macroblock(RV34DecContext *r, int8_t *intra_types, int cbp, int is16)
{
    MpegEncContext *s = &r->s;
    DSPContext *dsp = &s->dsp;
    int i, j;
    uint8_t *Y, *U, *V;
    int itype;
    int avail[6*8] = {0};
    int idx;

    // Set neighbour information.
    if(r->avail_cache[0])
        avail[0] = 1;
    if(r->avail_cache[1])
        avail[1] = avail[2] = 1;
    if(r->avail_cache[2])
        avail[3] = avail[4] = 1;
    if(r->avail_cache[3])
        avail[5] = 1;
    if(r->avail_cache[4])
        avail[8] = avail[16] = 1;
    if(r->avail_cache[8])
        avail[24] = avail[32] = 1;

    Y = s->dest[0];
    U = s->dest[1];
    V = s->dest[2];
    if(!is16){
        for(j = 0; j < 4; j++){
            idx = 9 + j*8;
            for(i = 0; i < 4; i++, cbp >>= 1, Y += 4, idx++){
                rv34_pred_4x4_block(r, Y, s->linesize, ittrans[intra_types[i]], avail[idx-8], avail[idx-1], avail[idx+7], avail[idx-7]);
                avail[idx] = 1;
                if(cbp & 1)
                    rv34_add_4x4_block(Y, s->linesize, s->block[(i>>1)+(j&2)], (i&1)*4+(j&1)*32);
            }
            Y += s->linesize * 4 - 4*4;
            intra_types += s->b4_stride;
        }
        intra_types -= s->b4_stride * 4;
        fill_rectangle(r->avail_cache + 5, 2, 2, 4, 0, 4);
        for(j = 0; j < 2; j++){
            idx = 5 + j*4;
            for(i = 0; i < 2; i++, cbp >>= 1, idx++){
                rv34_pred_4x4_block(r, U + i*4 + j*4*s->uvlinesize, s->uvlinesize, ittrans[intra_types[i*2+j*2*s->b4_stride]], r->avail_cache[idx-4], r->avail_cache[idx-1], !i && !j, r->avail_cache[idx-3]);
                rv34_pred_4x4_block(r, V + i*4 + j*4*s->uvlinesize, s->uvlinesize, ittrans[intra_types[i*2+j*2*s->b4_stride]], r->avail_cache[idx-4], r->avail_cache[idx-1], !i && !j, r->avail_cache[idx-3]);
                r->avail_cache[idx] = 1;
                if(cbp & 0x01)
                    rv34_add_4x4_block(U + i*4 + j*4*s->uvlinesize, s->uvlinesize, s->block[4], i*4+j*32);
                if(cbp & 0x10)
                    rv34_add_4x4_block(V + i*4 + j*4*s->uvlinesize, s->uvlinesize, s->block[5], i*4+j*32);
            }
        }
    }else{
        itype = ittrans16[intra_types[0]];
        itype = adjust_pred16(itype, r->avail_cache[5-4], r->avail_cache[5-1]);
        r->h.pred16x16[itype](Y, s->linesize);
        dsp->add_pixels_clamped(s->block[0], Y,     s->linesize);
        dsp->add_pixels_clamped(s->block[1], Y + 8, s->linesize);
        Y += s->linesize * 8;
        dsp->add_pixels_clamped(s->block[2], Y,     s->linesize);
        dsp->add_pixels_clamped(s->block[3], Y + 8, s->linesize);

        itype = ittrans16[intra_types[0]];
        if(itype == PLANE_PRED8x8) itype = DC_PRED8x8;
        itype = adjust_pred16(itype, r->avail_cache[5-4], r->avail_cache[5-1]);
        r->h.pred8x8[itype](U, s->uvlinesize);
        dsp->add_pixels_clamped(s->block[4], U, s->uvlinesize);
        r->h.pred8x8[itype](V, s->uvlinesize);
        dsp->add_pixels_clamped(s->block[5], V, s->uvlinesize);
    }
}

/** @} */ // recons group

/**
 * @addtogroup bitstream
 * Decode macroblock header and return CBP in case of success, -1 otherwise.
 */
static int rv34_decode_mb_header(RV34DecContext *r, int8_t *intra_types)
{
    MpegEncContext *s = &r->s;
    GetBitContext *gb = &s->gb;
    int mb_pos = s->mb_x + s->mb_y * s->mb_stride;
    int i, t;

    if(!r->si.type){
        r->is16 = get_bits1(gb);
        if(!r->is16 && !r->rv30){
            if(!get_bits1(gb))
                av_log(s->avctx, AV_LOG_ERROR, "Need DQUANT\n");
        }
        s->current_picture_ptr->mb_type[mb_pos] = r->is16 ? MB_TYPE_INTRA16x16 : MB_TYPE_INTRA;
        r->block_type = r->is16 ? RV34_MB_TYPE_INTRA16x16 : RV34_MB_TYPE_INTRA;
    }else{
        r->block_type = r->decode_mb_info(r);
        if(r->block_type == -1)
            return -1;
        s->current_picture_ptr->mb_type[mb_pos] = rv34_mb_type_to_lavc[r->block_type];
        r->mb_type[mb_pos] = r->block_type;
        if(r->block_type == RV34_MB_SKIP){
            if(s->pict_type == FF_P_TYPE)
                r->mb_type[mb_pos] = RV34_MB_P_16x16;
            if(s->pict_type == FF_B_TYPE)
                r->mb_type[mb_pos] = RV34_MB_B_DIRECT;
        }
        r->is16 = !!IS_INTRA16x16(s->current_picture_ptr->mb_type[mb_pos]);
        rv34_decode_mv(r, r->block_type);
        if(r->block_type == RV34_MB_SKIP){
            fill_rectangle(intra_types, 4, 4, s->b4_stride, 0, sizeof(intra_types[0]));
            return 0;
        }
        r->chroma_vlc = 1;
        r->luma_vlc   = 0;
    }
    if(IS_INTRA(s->current_picture_ptr->mb_type[mb_pos])){
        if(r->is16){
            t = get_bits(gb, 2);
            fill_rectangle(intra_types, 4, 4, s->b4_stride, t, sizeof(intra_types[0]));
            r->luma_vlc   = 2;
        }else{
            if(r->decode_intra_types(r, gb, intra_types) < 0)
                return -1;
            r->luma_vlc   = 1;
        }
        r->chroma_vlc = 0;
        r->cur_vlcs = choose_vlc_set(r->si.quant, r->si.vlc_set, 0);
    }else{
        for(i = 0; i < 16; i++)
            intra_types[(i & 3) + (i>>2) * s->b4_stride] = 0;
        r->cur_vlcs = choose_vlc_set(r->si.quant, r->si.vlc_set, 1);
        if(r->mb_type[mb_pos] == RV34_MB_P_MIX16x16){
            r->is16 = 1;
            r->chroma_vlc = 1;
            r->luma_vlc   = 2;
            r->cur_vlcs = choose_vlc_set(r->si.quant, r->si.vlc_set, 0);
        }
    }

    return rv34_decode_cbp(gb, r->cur_vlcs, r->is16);
}

/**
 * @addtogroup recons
 * @{
 */
/**
 * mask for retrieving all bits in coded block pattern
 * corresponding to one 8x8 block
 */
#define LUMA_CBP_BLOCK_MASK 0x33

#define U_CBP_MASK 0x0F0000
#define V_CBP_MASK 0xF00000


static void rv34_apply_differences(RV34DecContext *r, int cbp)
{
    static const int shifts[4] = { 0, 2, 8, 10 };
    MpegEncContext *s = &r->s;
    int i;

    for(i = 0; i < 4; i++)
        if((cbp & (LUMA_CBP_BLOCK_MASK << shifts[i])) || r->block_type == RV34_MB_P_MIX16x16)
            s->dsp.add_pixels_clamped(s->block[i], s->dest[0] + (i & 1)*8 + (i&2)*4*s->linesize, s->linesize);
    if(cbp & U_CBP_MASK)
        s->dsp.add_pixels_clamped(s->block[4], s->dest[1], s->uvlinesize);
    if(cbp & V_CBP_MASK)
        s->dsp.add_pixels_clamped(s->block[5], s->dest[2], s->uvlinesize);
}

static int is_mv_diff_gt_3(int16_t (*motion_val)[2], int step)
{
    int d;
    d = motion_val[0][0] - motion_val[-step][0];
    if(d < -3 || d > 3)
        return 1;
    d = motion_val[0][1] - motion_val[-step][1];
    if(d < -3 || d > 3)
        return 1;
    return 0;
}

static int rv34_set_deblock_coef(RV34DecContext *r)
{
    MpegEncContext *s = &r->s;
    int hmvmask = 0, vmvmask = 0, i, j;
    int midx = s->mb_x * 2 + s->mb_y * 2 * s->b8_stride;
    int16_t (*motion_val)[2] = s->current_picture_ptr->motion_val[0][midx];
    for(j = 0; j < 16; j += 8){
        for(i = 0; i < 2; i++){
            if(is_mv_diff_gt_3(motion_val + i, 1))
                vmvmask |= 0x11 << (j + i*2);
            if((j || s->mb_y) && is_mv_diff_gt_3(motion_val + i, s->b8_stride))
                hmvmask |= 0x03 << (j + i*2);
        }
        motion_val += s->b8_stride;
    }
    if(s->first_slice_line)
        hmvmask &= ~0x000F;
    if(!s->mb_x)
        vmvmask &= ~0x1111;
    return hmvmask | vmvmask; //XXX: should be stored separately for RV3
}

static int rv34_decode_macroblock(RV34DecContext *r, int8_t *intra_types)
{
    MpegEncContext *s = &r->s;
    GetBitContext *gb = &s->gb;
    int cbp, cbp2;
    int i, blknum, blkoff;
    DCTELEM block16[64];
    int luma_dc_quant;
    int dist;
    int mb_pos = s->mb_x + s->mb_y * s->mb_stride;

    // Calculate which neighbours are available. Maybe it's worth optimizing too.
    memset(r->avail_cache, 0, sizeof(r->avail_cache));
    fill_rectangle(r->avail_cache + 5, 2, 2, 4, 1, 4);
    dist = (s->mb_x - s->resync_mb_x) + (s->mb_y - s->resync_mb_y) * s->mb_width;
    if(s->mb_x && dist)
        r->avail_cache[4] =
        r->avail_cache[8] = s->current_picture_ptr->mb_type[mb_pos - 1];
    if(dist >= s->mb_width)
        r->avail_cache[1] =
        r->avail_cache[2] = s->current_picture_ptr->mb_type[mb_pos - s->mb_stride];
    if(((s->mb_x+1) < s->mb_width) && dist >= s->mb_width - 1)
        r->avail_cache[3] = s->current_picture_ptr->mb_type[mb_pos - s->mb_stride + 1];
    if(s->mb_x && dist > s->mb_width)
        r->avail_cache[0] = s->current_picture_ptr->mb_type[mb_pos - s->mb_stride - 1];

    s->qscale = r->si.quant;
    cbp = cbp2 = rv34_decode_mb_header(r, intra_types);
    r->cbp_luma  [s->mb_x + s->mb_y * s->mb_stride] = cbp;
    r->cbp_chroma[s->mb_x + s->mb_y * s->mb_stride] = cbp >> 16;
    if(s->pict_type == FF_I_TYPE)
        r->deblock_coefs[mb_pos] = 0;
    else
        r->deblock_coefs[mb_pos] = rv34_set_deblock_coef(r);
    s->current_picture_ptr->qscale_table[s->mb_x + s->mb_y * s->mb_stride] = s->qscale;

    if(cbp == -1)
        return -1;

    luma_dc_quant = r->block_type == RV34_MB_P_MIX16x16 ? r->luma_dc_quant_p[s->qscale] : r->luma_dc_quant_i[s->qscale];
    if(r->is16){
        memset(block16, 0, sizeof(block16));
        rv34_decode_block(block16, gb, r->cur_vlcs, 3, 0);
        rv34_dequant4x4_16x16(block16, rv34_qscale_tab[luma_dc_quant],rv34_qscale_tab[s->qscale]);
        rv34_inv_transform_noround(block16);
    }

    for(i = 0; i < 16; i++, cbp >>= 1){
        if(!r->is16 && !(cbp & 1)) continue;
        blknum = ((i & 2) >> 1) + ((i & 8) >> 2);
        blkoff = ((i & 1) << 2) + ((i & 4) << 3);
        if(cbp & 1)
            rv34_decode_block(s->block[blknum] + blkoff, gb, r->cur_vlcs, r->luma_vlc, 0);
        rv34_dequant4x4(s->block[blknum] + blkoff, rv34_qscale_tab[s->qscale],rv34_qscale_tab[s->qscale]);
        if(r->is16) //FIXME: optimize
            s->block[blknum][blkoff] = block16[(i & 3) | ((i & 0xC) << 1)];
        rv34_inv_transform(s->block[blknum] + blkoff);
    }
    if(r->block_type == RV34_MB_P_MIX16x16)
        r->cur_vlcs = choose_vlc_set(r->si.quant, r->si.vlc_set, 1);
    for(; i < 24; i++, cbp >>= 1){
        if(!(cbp & 1)) continue;
        blknum = ((i & 4) >> 2) + 4;
        blkoff = ((i & 1) << 2) + ((i & 2) << 4);
        rv34_decode_block(s->block[blknum] + blkoff, gb, r->cur_vlcs, r->chroma_vlc, 1);
        rv34_dequant4x4(s->block[blknum] + blkoff, rv34_qscale_tab[rv34_chroma_quant[1][s->qscale]],rv34_qscale_tab[rv34_chroma_quant[0][s->qscale]]);
        rv34_inv_transform(s->block[blknum] + blkoff);
    }
    if(IS_INTRA(s->current_picture_ptr->mb_type[s->mb_x + s->mb_y*s->mb_stride]))
        rv34_output_macroblock(r, intra_types, cbp2, r->is16);
    else
        rv34_apply_differences(r, cbp2);

    return 0;
}

static int check_slice_end(RV34DecContext *r, MpegEncContext *s)
{
    int bits;
    if(s->mb_y >= s->mb_height)
        return 1;
    if(!s->mb_num_left)
        return 1;
    if(r->s.mb_skip_run > 1)
        return 0;
    bits = r->bits - get_bits_count(&s->gb);
    if(bits < 0 || (bits < 8 && !show_bits(&s->gb, bits)))
        return 1;
    return 0;
}

static inline int slice_compare(SliceInfo *si1, SliceInfo *si2)
{
    return si1->type   != si2->type  ||
           si1->start  >= si2->start ||
           si1->width  != si2->width ||
           si1->height != si2->height||
           si1->pts    != si2->pts;
}

static int rv34_decode_slice(RV34DecContext *r, int end, uint8_t* buf, int buf_size)
{
    MpegEncContext *s = &r->s;
    GetBitContext *gb = &s->gb;
    int mb_pos;
    int res;

    init_get_bits(&r->s.gb, buf, buf_size*8);
    res = r->parse_slice_header(r, gb, &r->si);
    if(res < 0){
        av_log(s->avctx, AV_LOG_ERROR, "Incorrect or unknown slice header\n");
        return -1;
    }

    if ((s->mb_x == 0 && s->mb_y == 0) || s->current_picture_ptr==NULL) {
        if(s->width != r->si.width || s->height != r->si.height){
            av_log(s->avctx, AV_LOG_DEBUG, "Changing dimensions to %dx%d\n", r->si.width,r->si.height);
            MPV_common_end(s);
            s->width  = r->si.width;
            s->height = r->si.height;
            if(MPV_common_init(s) < 0)
                return -1;
            r->intra_types_hist = av_realloc(r->intra_types_hist, s->b4_stride * 4 * 2 * sizeof(*r->intra_types_hist));
            r->intra_types = r->intra_types_hist + s->b4_stride * 4;
            r->mb_type = av_realloc(r->mb_type, r->s.mb_stride * r->s.mb_height * sizeof(*r->mb_type));
            r->cbp_luma   = av_realloc(r->cbp_luma,   r->s.mb_stride * r->s.mb_height * sizeof(*r->cbp_luma));
            r->cbp_chroma = av_realloc(r->cbp_chroma, r->s.mb_stride * r->s.mb_height * sizeof(*r->cbp_chroma));
            r->deblock_coefs = av_realloc(r->deblock_coefs, r->s.mb_stride * r->s.mb_height * sizeof(*r->deblock_coefs));
        }
        s->pict_type = r->si.type ? r->si.type : FF_I_TYPE;
        if(MPV_frame_start(s, s->avctx) < 0)
            return -1;
        ff_er_frame_start(s);
        r->cur_pts = r->si.pts;
        if(s->pict_type != FF_B_TYPE){
            r->last_pts = r->next_pts;
            r->next_pts = r->cur_pts;
        }
        s->mb_x = s->mb_y = 0;
    }

    r->si.end = end;
    s->qscale = r->si.quant;
    r->bits = buf_size*8;
    s->mb_num_left = r->si.end - r->si.start;
    r->s.mb_skip_run = 0;

    mb_pos = s->mb_x + s->mb_y * s->mb_width;
    if(r->si.start != mb_pos){
        av_log(s->avctx, AV_LOG_ERROR, "Slice indicates MB offset %d, got %d\n", r->si.start, mb_pos);
        s->mb_x = r->si.start % s->mb_width;
        s->mb_y = r->si.start / s->mb_width;
    }
    memset(r->intra_types_hist, -1, s->b4_stride * 4 * 2 * sizeof(*r->intra_types_hist));
    s->first_slice_line = 1;
    s->resync_mb_x= s->mb_x;
    s->resync_mb_y= s->mb_y;

    ff_init_block_index(s);
    while(!check_slice_end(r, s)) {
        ff_update_block_index(s);
        s->dsp.clear_blocks(s->block[0]);

        if(rv34_decode_macroblock(r, r->intra_types + s->mb_x * 4 + 1) < 0){
            ff_er_add_slice(s, s->resync_mb_x, s->resync_mb_y, s->mb_x-1, s->mb_y, AC_ERROR|DC_ERROR|MV_ERROR);
            return -1;
        }
        if (++s->mb_x == s->mb_width) {
            s->mb_x = 0;
            s->mb_y++;
            ff_init_block_index(s);

            memmove(r->intra_types_hist, r->intra_types, s->b4_stride * 4 * sizeof(*r->intra_types_hist));
            memset(r->intra_types, -1, s->b4_stride * 4 * sizeof(*r->intra_types_hist));

            if(r->loop_filter && s->mb_y >= 2)
                r->loop_filter(r, s->mb_y - 2);
        }
        if(s->mb_x == s->resync_mb_x)
            s->first_slice_line=0;
        s->mb_num_left--;
    }
    ff_er_add_slice(s, s->resync_mb_x, s->resync_mb_y, s->mb_x-1, s->mb_y, AC_END|DC_END|MV_END);

    return s->mb_y == s->mb_height;
}

/** @} */ // recons group end

/**
 * Initialize decoder.
 */
av_cold int ff_rv34_decode_init(AVCodecContext *avctx)
{
    RV34DecContext *r = avctx->priv_data;
    MpegEncContext *s = &r->s;

    MPV_decode_defaults(s);
    s->avctx= avctx;
    s->out_format = FMT_H263;
    s->codec_id= avctx->codec_id;

    s->width = avctx->width;
    s->height = avctx->height;

    r->s.avctx = avctx;
    avctx->flags |= CODEC_FLAG_EMU_EDGE;
    r->s.flags |= CODEC_FLAG_EMU_EDGE;
    avctx->pix_fmt = PIX_FMT_YUV420P;
    avctx->has_b_frames = 1;
    s->low_delay = 0;

    if (MPV_common_init(s) < 0)
        return -1;

    ff_h264_pred_init(&r->h, CODEC_ID_RV40);

    r->intra_types_hist = av_malloc(s->b4_stride * 4 * 2 * sizeof(*r->intra_types_hist));
    r->intra_types = r->intra_types_hist + s->b4_stride * 4;

    r->mb_type = av_mallocz(r->s.mb_stride * r->s.mb_height * sizeof(*r->mb_type));

    r->cbp_luma   = av_malloc(r->s.mb_stride * r->s.mb_height * sizeof(*r->cbp_luma));
    r->cbp_chroma = av_malloc(r->s.mb_stride * r->s.mb_height * sizeof(*r->cbp_chroma));
    r->deblock_coefs = av_malloc(r->s.mb_stride * r->s.mb_height * sizeof(*r->deblock_coefs));

    if(!intra_vlcs[0].cbppattern[0].bits)
        rv34_init_tables();

    return 0;
}

static int get_slice_offset(AVCodecContext *avctx, uint8_t *buf, int n)
{
    if(avctx->slice_count) return avctx->slice_offset[n];
    else                   return AV_RL32(buf + n*8 - 4) == 1 ? AV_RL32(buf + n*8) :  AV_RB32(buf + n*8);
}

int ff_rv34_decode_frame(AVCodecContext *avctx,
                            void *data, int *data_size,
                            const uint8_t *buf, int buf_size)
{
    RV34DecContext *r = avctx->priv_data;
    MpegEncContext *s = &r->s;
    AVFrame *pict = data;
    SliceInfo si;
    int i;
    int slice_count;
    uint8_t *slices_hdr = NULL;
    int last = 0;

    /* no supplementary picture */
    if (buf_size == 0) {
        /* special case for last picture */
        if (s->low_delay==0 && s->next_picture_ptr) {
            *pict= *(AVFrame*)s->next_picture_ptr;
            s->next_picture_ptr= NULL;

            *data_size = sizeof(AVFrame);
        }
        return 0;
    }

    if(!avctx->slice_count){
        slice_count = (*buf++) + 1;
        slices_hdr = buf + 4;
        buf += 8 * slice_count;
    }else
        slice_count = avctx->slice_count;

    for(i=0; i<slice_count; i++){
        int offset= get_slice_offset(avctx, slices_hdr, i);
        int size;
        if(i+1 == slice_count)
            size= buf_size - offset;
        else
            size= get_slice_offset(avctx, slices_hdr, i+1) - offset;

        if(offset > buf_size){
            av_log(avctx, AV_LOG_ERROR, "Slice offset is greater than frame size\n");
            break;
        }

        r->si.end = s->mb_width * s->mb_height;
        if(i+1 < slice_count){
            init_get_bits(&s->gb, buf+get_slice_offset(avctx, slices_hdr, i+1), (buf_size-get_slice_offset(avctx, slices_hdr, i+1))*8);
            if(r->parse_slice_header(r, &r->s.gb, &si) < 0){
                if(i+2 < slice_count)
                    size = get_slice_offset(avctx, slices_hdr, i+2) - offset;
                else
                    size = buf_size - offset;
            }else
                r->si.end = si.start;
        }
        if(!i && si.type == FF_B_TYPE && (!s->last_picture_ptr || !s->last_picture_ptr->data[0]))
            return -1;
        last = rv34_decode_slice(r, r->si.end, buf + offset, size);
        s->mb_num_left = r->s.mb_x + r->s.mb_y*r->s.mb_width - r->si.start;
        if(last)
            break;
    }

    if(last){
        if(r->loop_filter)
            r->loop_filter(r, s->mb_height - 1);
        ff_er_frame_end(s);
        MPV_frame_end(s);
        if (s->pict_type == FF_B_TYPE || s->low_delay) {
            *pict= *(AVFrame*)s->current_picture_ptr;
        } else if (s->last_picture_ptr != NULL) {
            *pict= *(AVFrame*)s->last_picture_ptr;
        }

        if(s->last_picture_ptr || s->low_delay){
            *data_size = sizeof(AVFrame);
            ff_print_debug_info(s, pict);
        }
        s->current_picture_ptr= NULL; //so we can detect if frame_end wasnt called (find some nicer solution...)
    }
    return buf_size;
}

av_cold int ff_rv34_decode_end(AVCodecContext *avctx)
{
    RV34DecContext *r = avctx->priv_data;

    MPV_common_end(&r->s);

    av_freep(&r->intra_types_hist);
    r->intra_types = NULL;
    av_freep(&r->mb_type);
    av_freep(&r->cbp_luma);
    av_freep(&r->cbp_chroma);
    av_freep(&r->deblock_coefs);

    return 0;
}
