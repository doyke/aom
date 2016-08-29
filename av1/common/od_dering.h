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

#if !defined(_dering_H)
#define _dering_H (1)

#include "odintrin.h"

#if defined(DAALA_ODINTRIN)
#include "filter.h"
typedef int16_t od_dering_in;
#endif

#define OD_DERINGSIZES (2)

#define OD_DERING_NBLOCKS (OD_BSIZE_MAX / 8)

#define OD_FILT_BORDER (3)
#define OD_FILT_BSTRIDE (OD_BSIZE_MAX + 2 * OD_FILT_BORDER)
#define OD_DERING_INBUF_SIZE \
  ((OD_BSIZE_MAX + 2 * OD_FILT_BORDER) * (OD_BSIZE_MAX + 2 * OD_FILT_BORDER))

extern const int OD_DIRECTION_OFFSETS_TABLE[8][3];

#define dering_type uint8_t

typedef void (*od_filter_dering_direction_func)(int16_t *y, int ystride,
                                                const int16_t *in,
                                                int threshold, int dir);
typedef void (*od_filter_dering_orthogonal_func)(int16_t *y, int ystride,
                                                 const int16_t *in,
                                                 const dering_type *x,
                                                 int xstride, int threshold,
                                                 int dir);

struct od_dering_opt_vtbl {
  od_filter_dering_direction_func filter_dering_direction[OD_DERINGSIZES];
  od_filter_dering_orthogonal_func filter_dering_orthogonal[OD_DERINGSIZES];
};
typedef struct od_dering_opt_vtbl od_dering_opt_vtbl;

void od_inbuf_copy(od_dering_in *inbuf, const uint8_t *x, int xstride,
    int bsize, int nvb, int nhb, int sby, int sbx, int nvsb, int nhsb);

void od_dering(const od_dering_opt_vtbl *vtbl, int16_t *y, int ystride,
               od_dering_in *inbuf, dering_type *x, int xstride, int nvb, int nhb, int sbx,
               int sby, int nhsb, int nvsb, int xdec,
               int dir[OD_DERING_NBLOCKS][OD_DERING_NBLOCKS], int pli,
               int *bskip, int skip_stride, int threshold,
               int coeff_shift);
void od_filter_dering_direction_c(int16_t *y, int ystride, const int16_t *in,
                                  int ln, int threshold, int dir);
void od_filter_dering_orthogonal_c(int16_t *y, int ystride, const int16_t *in,
                                   const dering_type *x, int xstride, int ln,
                                   int threshold, int dir);

extern const od_dering_opt_vtbl OD_DERING_VTBL_C;

void od_filter_dering_direction_4x4_c(int16_t *y, int ystride,
                                      const int16_t *in, int threshold,
                                      int dir);
void od_filter_dering_direction_8x8_c(int16_t *y, int ystride,
                                      const int16_t *in, int threshold,
                                      int dir);
void od_filter_dering_orthogonal_4x4_c(int16_t *y, int ystride,
                                       const int16_t *in, const dering_type *x,
                                       int xstride, int threshold, int dir);
void od_filter_dering_orthogonal_8x8_c(int16_t *y, int ystride,
                                       const int16_t *in, const dering_type *x,
                                       int xstride, int threshold, int dir);

#endif
