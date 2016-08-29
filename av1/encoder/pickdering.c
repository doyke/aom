/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <string.h>
#include <math.h>

#include "./aom_scale_rtcd.h"
#include "av1/common/dering.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/reconinter.h"
#include "av1/encoder/encoder.h"
#include "aom/aom_integer.h"

static double compute_dist(int16_t *x, int xstride, int16_t *y, int ystride,
                           int nhb, int nvb, int coeff_shift) {
  int i, j;
  double sum;
  sum = 0;
  for (i = 0; i < nvb << 3; i++) {
    for (j = 0; j < nhb << 3; j++) {
      double tmp;
      tmp = x[i * xstride + j] - y[i * ystride + j];
      sum += tmp * tmp;
    }
  }
  return sum / (double)(1 << 2 * coeff_shift);
}

int av1_dering_search(YV12_BUFFER_CONFIG *frame, const YV12_BUFFER_CONFIG *ref,
                      AV1_COMMON *cm, MACROBLOCKD *xd) {
  int r, c;
  int sbr, sbc;
  int nhsb, nvsb;
  uint8_t *src;
  int16_t *ref_coeff;
  int *bskip;
  int dir[OD_DERING_NBLOCKS][OD_DERING_NBLOCKS] = { { 0 } };
  int stride;
  int bsize[3];
  int dec[3];
  int pli;
  int level;
  int best_level;
  int coeff_shift = AOMMAX(cm->bit_depth - 8, 0);
  src = aom_malloc(sizeof(*src) * cm->mi_rows * cm->mi_cols * 64);
  ref_coeff = aom_malloc(sizeof(*ref_coeff) * cm->mi_rows * cm->mi_cols * 64);
  bskip = aom_malloc(sizeof(*bskip) * cm->mi_rows * cm->mi_cols);
  av1_setup_dst_planes(xd->plane, frame, 0, 0);
  for (pli = 0; pli < 3; pli++) {
    dec[pli] = xd->plane[pli].subsampling_x;
    bsize[pli] = 8 >> dec[pli];
  }
  stride = bsize[0] * cm->mi_cols;
  for (r = 0; r < bsize[0] * cm->mi_rows; ++r) {
    for (c = 0; c < bsize[0] * cm->mi_cols; ++c) {
#if CONFIG_AOM_HIGHBITDEPTH
      if (cm->use_highbitdepth) {
        src[r * stride + c] = CONVERT_TO_SHORTPTR(
            xd->plane[0].dst.buf)[r * xd->plane[0].dst.stride + c];
        ref_coeff[r * stride + c] =
            CONVERT_TO_SHORTPTR(ref->y_buffer)[r * ref->y_stride + c];
      } else {
#endif
        src[r * stride + c] =
            xd->plane[0].dst.buf[r * xd->plane[0].dst.stride + c];
        ref_coeff[r * stride + c] = ref->y_buffer[r * ref->y_stride + c];
#if CONFIG_AOM_HIGHBITDEPTH
      }
#endif
    }
  }
  for (r = 0; r < cm->mi_rows; ++r) {
    for (c = 0; c < cm->mi_cols; ++c) {
      const MB_MODE_INFO *mbmi =
          &cm->mi_grid_visible[r * cm->mi_stride + c]->mbmi;
      bskip[r * cm->mi_cols + c] = mbmi->skip;
    }
  }
  nvsb = (cm->mi_rows + MAX_MIB_SIZE - 1) / MAX_MIB_SIZE;
  nhsb = (cm->mi_cols + MAX_MIB_SIZE - 1) / MAX_MIB_SIZE;
  /* Pick a base threshold based on the quantizer. The threshold will then be
     adjusted on a 64x64 basis. We use a threshold of the form T = a*Q^b,
     where a and b are derived empirically trying to optimize rate-distortion
     at different quantizer settings. */
  best_level = (int)floor(
      .5 + .45 * pow(av1_ac_quant(cm->base_qindex, 0, cm->bit_depth), 0.6));
  for (sbr = 0; sbr < nvsb; sbr++) {
    for (sbc = 0; sbc < nhsb; sbc++) {
      int nvb, nhb;
      int gi;
      int best_gi;
      int32_t best_mse = INT32_MAX;
      int16_t dst[MAX_MIB_SIZE * MAX_MIB_SIZE * 8 * 8];
      nhb = AOMMIN(MAX_MIB_SIZE, cm->mi_cols - MAX_MIB_SIZE * sbc);
      nvb = AOMMIN(MAX_MIB_SIZE, cm->mi_rows - MAX_MIB_SIZE * sbr);
      if (sb_all_skip(cm, sbr * MAX_MIB_SIZE, sbc * MAX_MIB_SIZE)) continue;
      best_gi = 0;
      for (gi = 0; gi < DERING_REFINEMENT_LEVELS; gi++) {
        int cur_mse;
        int threshold;
        od_dering_in inbuf[OD_DERING_INBUF_SIZE];
        level = compute_level_from_index(best_level, gi);
        threshold = level << coeff_shift;
        od_inbuf_copy(inbuf,
            &src[sbr * stride * bsize[0] * MAX_MIB_SIZE +
                                   sbc * bsize[0] * MAX_MIB_SIZE],
                              cm->mi_cols * bsize[0],
                              3, nvb, nhb, sbr, sbc, nvsb, nhsb);
        od_dering(&OD_DERING_VTBL_C, dst, MAX_MIB_SIZE * bsize[0],
                  inbuf, nhb, nvb, sbc, sbr, nhsb, nvsb, 0,
                  dir, 0,
                  &bskip[MAX_MIB_SIZE * sbr * cm->mi_cols + MAX_MIB_SIZE * sbc],
                  cm->mi_cols, threshold, coeff_shift);
        cur_mse = (int)compute_dist(
            dst, MAX_MIB_SIZE * bsize[0],
            &ref_coeff[sbr * stride * bsize[0] * MAX_MIB_SIZE +
                       sbc * bsize[0] * MAX_MIB_SIZE],
            stride, nhb, nvb, coeff_shift);
        if (cur_mse < best_mse) {
          best_gi = gi;
          best_mse = cur_mse;
        }
      }
      cm->mi_grid_visible[MAX_MIB_SIZE * sbr * cm->mi_stride +
                          MAX_MIB_SIZE * sbc]
          ->mbmi.dering_gain = best_gi;
    }
  }
  aom_free(src);
  aom_free(ref_coeff);
  aom_free(bskip);
  return best_level;
}
