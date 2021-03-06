/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP10_ENCODER_RDOPT_H_
#define VP10_ENCODER_RDOPT_H_

#include "vp10/common/blockd.h"

#include "vp10/encoder/block.h"
#include "vp10/encoder/context_tree.h"

#ifdef __cplusplus
extern "C" {
#endif

struct TileInfo;
struct VP10_COMP;
struct macroblock;
struct RD_COST;

void vp10_rd_pick_intra_mode_sb(struct VP10_COMP *cpi, struct macroblock *x,
                                struct RD_COST *rd_cost, BLOCK_SIZE bsize,
                                PICK_MODE_CONTEXT *ctx, int64_t best_rd);

unsigned int vp10_get_sby_perpixel_variance(VP10_COMP *cpi,
                                            const struct buf_2d *ref,
                                            BLOCK_SIZE bs);
#if CONFIG_VPX_HIGHBITDEPTH
unsigned int vp10_high_get_sby_perpixel_variance(VP10_COMP *cpi,
                                                 const struct buf_2d *ref,
                                                 BLOCK_SIZE bs, int bd);
#endif

void vp10_rd_pick_inter_mode_sb(struct VP10_COMP *cpi,
                                struct TileDataEnc *tile_data,
                                struct macroblock *x, int mi_row, int mi_col,
                                struct RD_COST *rd_cost, BLOCK_SIZE bsize,
                                PICK_MODE_CONTEXT *ctx, int64_t best_rd_so_far);

void vp10_rd_pick_inter_mode_sb_seg_skip(
    struct VP10_COMP *cpi, struct TileDataEnc *tile_data, struct macroblock *x,
    struct RD_COST *rd_cost, BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
    int64_t best_rd_so_far);

int vp10_internal_image_edge(struct VP10_COMP *cpi);
int vp10_active_h_edge(struct VP10_COMP *cpi, int mi_row, int mi_step);
int vp10_active_v_edge(struct VP10_COMP *cpi, int mi_col, int mi_step);
int vp10_active_edge_sb(struct VP10_COMP *cpi, int mi_row, int mi_col);

void vp10_rd_pick_inter_mode_sub8x8(struct VP10_COMP *cpi,
                                    struct TileDataEnc *tile_data,
                                    struct macroblock *x, int mi_row,
                                    int mi_col, struct RD_COST *rd_cost,
                                    BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                                    int64_t best_rd_so_far);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_ENCODER_RDOPT_H_
