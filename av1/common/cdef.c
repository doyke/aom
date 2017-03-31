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

#include <assert.h>
#include <math.h>
#include <string.h>

#include "./aom_scale_rtcd.h"
#include "aom/aom_integer.h"
#include "av1/common/cdef.h"
#include "av1/common/od_dering.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/reconinter.h"

int sb_all_skip(const AV1_COMMON *const cm, int mi_row, int mi_col) {
  int r, c;
  int maxc, maxr;
  int skip = 1;
  maxc = cm->mi_cols - mi_col;
  maxr = cm->mi_rows - mi_row;
#if CONFIG_EXT_PARTITION
  if (maxr > cm->mib_size_log2) maxr = cm->mib_size_log2;
  if (maxc > cm->mib_size_log2) maxc = cm->mib_size_log2;
#else
  if (maxr > MAX_MIB_SIZE) maxr = MAX_MIB_SIZE;
  if (maxc > MAX_MIB_SIZE) maxc = MAX_MIB_SIZE;
#endif

  for (r = 0; r < maxr; r++) {
    for (c = 0; c < maxc; c++) {
      skip = skip &&
             cm->mi_grid_visible[(mi_row + r) * cm->mi_stride + mi_col + c]
                 ->mbmi.skip;
    }
  }
  return skip;
}

int sb_compute_dering_list(const AV1_COMMON *const cm, int mi_row, int mi_col,
                           dering_list *dlist) {
  int r, c;
  int maxc, maxr;
  MODE_INFO **grid;
  int count = 0;
  grid = cm->mi_grid_visible;
  maxc = cm->mi_cols - mi_col;
  maxr = cm->mi_rows - mi_row;
#if CONFIG_EXT_PARTITION
  if (maxr > cm->mib_size_log2) maxr = cm->mib_size_log2;
  if (maxc > cm->mib_size_log2) maxc = cm->mib_size_log2;
#else
  if (maxr > MAX_MIB_SIZE) maxr = MAX_MIB_SIZE;
  if (maxc > MAX_MIB_SIZE) maxc = MAX_MIB_SIZE;
#endif

  const int r_step = mi_size_high[BLOCK_8X8];
  const int c_step = mi_size_wide[BLOCK_8X8];

  for (r = 0; r < maxr; r += r_step) {
    MODE_INFO **grid_row;
    grid_row = &grid[(mi_row + r) * cm->mi_stride + mi_col];
    for (c = 0; c < maxc; c += c_step) {
      if (!grid_row[c]->mbmi.skip) {
        dlist[count].by = r;
        dlist[count].bx = c;
        count++;
      }
    }
  }
  return count;
}

/* TODO: Optimize this function for SSE. */
static void copy_sb8_16(UNUSED AV1_COMMON *cm, uint16_t *dst, int dstride,
                        const uint8_t *src, int src_voffset, int src_hoffset,
                        int sstride, int vsize, int hsize) {
  int r, c;
#if CONFIG_AOM_HIGHBITDEPTH
  if (cm->use_highbitdepth) {
    const uint16_t *base =
        &CONVERT_TO_SHORTPTR(src)[src_voffset * sstride + src_hoffset];
    for (r = 0; r < vsize; r++) {
      for (c = 0; c < hsize; c++) {
        dst[r * dstride + c] = base[r * sstride + c];
      }
    }
  } else {
#endif
    const uint8_t *base = &src[src_voffset * sstride + src_hoffset];
    for (r = 0; r < vsize; r++) {
      for (c = 0; c < hsize; c++) {
        dst[r * dstride + c] = base[r * sstride + c];
      }
    }
#if CONFIG_AOM_HIGHBITDEPTH
  }
#endif
}

void av1_cdef_frame(YV12_BUFFER_CONFIG *frame, AV1_COMMON *cm,
                    MACROBLOCKD *xd) {
  int r, c;
  int sbr, sbc;
  int nhsb, nvsb;
  uint16_t src[OD_DERING_INBUF_SIZE];
  uint16_t *linebuf[3];
  uint16_t colbuf[3][OD_BSIZE_MAX + 2 * OD_FILT_VBORDER][OD_FILT_HBORDER];
  dering_list dlist[MAX_MIB_SIZE * MAX_MIB_SIZE];
  unsigned char *row_dering, *prev_row_dering, *curr_row_dering;
  int dering_count;
  int dir[OD_DERING_NBLOCKS][OD_DERING_NBLOCKS] = { { 0 } };
  int var[OD_DERING_NBLOCKS][OD_DERING_NBLOCKS] = { { 0 } };
  int stride;
  int bsize[3];
  int dec[3];
  int pli;
  int dering_left;
  int coeff_shift = AOMMAX(cm->bit_depth - 8, 0);
  int nplanes = 3;
  int chroma_dering =
      xd->plane[1].subsampling_x == xd->plane[1].subsampling_y &&
      xd->plane[2].subsampling_x == xd->plane[2].subsampling_y;
  nvsb = (cm->mi_rows + MAX_MIB_SIZE - 1) / MAX_MIB_SIZE;
  nhsb = (cm->mi_cols + MAX_MIB_SIZE - 1) / MAX_MIB_SIZE;
  av1_setup_dst_planes(xd->plane, frame, 0, 0);
  row_dering = aom_malloc(sizeof(*row_dering) * (nhsb + 2) * 2);
  memset(row_dering, 1, sizeof(*row_dering) * (nhsb + 2) * 2);
  prev_row_dering = row_dering + 1;
  curr_row_dering = prev_row_dering + nhsb + 2;
  for (pli = 0; pli < nplanes; pli++) {
    dec[pli] = xd->plane[pli].subsampling_x;
    bsize[pli] = OD_DERING_SIZE_LOG2 - dec[pli];
  }
  stride = (cm->mi_cols << bsize[0]) + 2 * OD_FILT_HBORDER;
  for (pli = 0; pli < nplanes; pli++) {
    linebuf[pli] = aom_malloc(sizeof(*linebuf) * OD_FILT_VBORDER * stride);
  }
  for (sbr = 0; sbr < nvsb; sbr++) {
    for (pli = 0; pli < nplanes; pli++) {
      for (r = 0; r < (MAX_MIB_SIZE << bsize[pli]) + 2 * OD_FILT_VBORDER; r++) {
        for (c = 0; c < OD_FILT_HBORDER; c++) {
          colbuf[pli][r][c] = OD_DERING_VERY_LARGE;
        }
      }
    }
    dering_left = 1;
    for (sbc = 0; sbc < nhsb; sbc++) {
      int level, clpf_strength;
      int uv_level, uv_clpf_strength;
      int nhb, nvb;
      int cstart = 0;
      if (cm->mi_grid_visible[MAX_MIB_SIZE * sbr * cm->mi_stride +
                              MAX_MIB_SIZE * sbc] == NULL) {
        dering_left = 0;
        continue;
      }
      if (!dering_left) cstart = -OD_FILT_HBORDER;
      nhb = AOMMIN(MAX_MIB_SIZE, cm->mi_cols - MAX_MIB_SIZE * sbc);
      nvb = AOMMIN(MAX_MIB_SIZE, cm->mi_rows - MAX_MIB_SIZE * sbr);
#if 1  // TODO(stemidts/jmvalin): Handle tile borders correctly
      int tile_top, tile_left, tile_bottom, tile_right;
      BOUNDARY_TYPE boundary_tl =
          cm->mi_grid_visible[MAX_MIB_SIZE * sbr * cm->mi_stride +
                              MAX_MIB_SIZE * sbc]
              ->mbmi.boundary_info;
      BOUNDARY_TYPE boundary_br =
          cm->mi_grid_visible[(MAX_MIB_SIZE * sbr + nvb - 1) * cm->mi_stride +
                              MAX_MIB_SIZE * sbc + nhb - 1]
              ->mbmi.boundary_info;
      tile_top = boundary_tl & TILE_ABOVE_BOUNDARY;
      tile_left = boundary_tl & TILE_LEFT_BOUNDARY;
      tile_bottom = boundary_br & TILE_BOTTOM_BOUNDARY;
      tile_right = boundary_br & TILE_RIGHT_BOUNDARY;
#endif
      level = cm->cdef_strengths[cm->mi_grid_visible[MAX_MIB_SIZE * sbr *
                                                         cm->mi_stride +
                                                     MAX_MIB_SIZE * sbc]
                                     ->mbmi.cdef_strength] /
              CLPF_STRENGTHS;
      clpf_strength =
          cm->cdef_strengths[cm->mi_grid_visible[MAX_MIB_SIZE * sbr *
                                                     cm->mi_stride +
                                                 MAX_MIB_SIZE * sbc]
                                 ->mbmi.cdef_strength] %
          CLPF_STRENGTHS;
      clpf_strength += clpf_strength == 3;
      uv_level = cm->cdef_uv_strengths[cm->mi_grid_visible[MAX_MIB_SIZE * sbr *
                                                               cm->mi_stride +
                                                           MAX_MIB_SIZE * sbc]
                                           ->mbmi.cdef_strength] /
                 CLPF_STRENGTHS;
      uv_clpf_strength =
          cm->cdef_uv_strengths[cm->mi_grid_visible[MAX_MIB_SIZE * sbr *
                                                        cm->mi_stride +
                                                    MAX_MIB_SIZE * sbc]
                                    ->mbmi.cdef_strength] %
          CLPF_STRENGTHS;
      uv_clpf_strength += uv_clpf_strength == 3;
      curr_row_dering[sbc] = 0;
      if ((level == 0 && clpf_strength == 0 && uv_level == 0 &&
           uv_clpf_strength == 0) ||
          (dering_count = sb_compute_dering_list(
               cm, sbr * MAX_MIB_SIZE, sbc * MAX_MIB_SIZE, dlist)) == 0) {
        dering_left = 0;
        continue;
      }

      curr_row_dering[sbc] = 1;
      for (pli = 0; pli < nplanes; pli++) {
        uint16_t dst[OD_BSIZE_MAX * OD_BSIZE_MAX];
        int coffset;
        int rend, cend;
        int clpf_damping = 3 - (pli != AOM_PLANE_Y) + (cm->base_qindex >> 6);

        if (pli) {
          if (chroma_dering)
            level = uv_level;
          else
            level = 0;
          clpf_strength = uv_clpf_strength;
        }
        if (sbc == nhsb - 1)
          cend = (nhb << bsize[pli]);
        else
          cend = (nhb << bsize[pli]) + OD_FILT_HBORDER;
        if (sbr == nvsb - 1)
          rend = (nvb << bsize[pli]);
        else
          rend = (nvb << bsize[pli]) + OD_FILT_VBORDER;
        coffset = sbc * MAX_MIB_SIZE << bsize[pli];
        if (sbc == nhsb - 1) {
          /* On the last superblock column, fill in the right border with
             OD_DERING_VERY_LARGE to avoid filtering with the outside. */
          for (r = 0; r < rend + OD_FILT_VBORDER; r++) {
            for (c = cend; c < (nhb << bsize[pli]) + OD_FILT_HBORDER; ++c) {
              src[r * OD_FILT_BSTRIDE + c + OD_FILT_HBORDER] =
                  OD_DERING_VERY_LARGE;
            }
          }
        }
        if (sbr == nvsb - 1) {
          /* On the last superblock row, fill in the bottom border with
             OD_DERING_VERY_LARGE to avoid filtering with the outside. */
          for (r = rend; r < rend + OD_FILT_VBORDER; r++) {
            for (c = 0; c < (nhb << bsize[pli]) + 2 * OD_FILT_HBORDER; c++) {
              src[(r + OD_FILT_VBORDER) * OD_FILT_BSTRIDE + c] =
                  OD_DERING_VERY_LARGE;
            }
          }
        }
        /* Copy in the pixels we need from the current superblock for
           deringing.*/
        copy_sb8_16(
            cm,
            &src[OD_FILT_VBORDER * OD_FILT_BSTRIDE + OD_FILT_HBORDER + cstart],
            OD_FILT_BSTRIDE, xd->plane[pli].dst.buf,
            (MAX_MIB_SIZE << bsize[pli]) * sbr, coffset + cstart,
            xd->plane[pli].dst.stride, rend, cend - cstart);
        if (!prev_row_dering[sbc]) {
          copy_sb8_16(cm, &src[OD_FILT_HBORDER], OD_FILT_BSTRIDE,
                      xd->plane[pli].dst.buf,
                      (MAX_MIB_SIZE << bsize[pli]) * sbr - OD_FILT_VBORDER,
                      coffset, xd->plane[pli].dst.stride, OD_FILT_VBORDER,
                      nhb << bsize[pli]);
        } else if (sbr > 0) {
          for (r = 0; r < OD_FILT_VBORDER; r++) {
            for (c = 0; c < nhb << bsize[pli]; c++) {
              src[r * OD_FILT_BSTRIDE + c + OD_FILT_HBORDER] =
                  linebuf[pli][r * stride + coffset + c];
            }
          }
        } else {
          for (r = 0; r < OD_FILT_VBORDER; r++) {
            for (c = 0; c < nhb << bsize[pli]; c++) {
              src[r * OD_FILT_BSTRIDE + c + OD_FILT_HBORDER] =
                  OD_DERING_VERY_LARGE;
            }
          }
        }
        if (!prev_row_dering[sbc - 1]) {
          copy_sb8_16(cm, src, OD_FILT_BSTRIDE, xd->plane[pli].dst.buf,
                      (MAX_MIB_SIZE << bsize[pli]) * sbr - OD_FILT_VBORDER,
                      coffset - OD_FILT_HBORDER, xd->plane[pli].dst.stride,
                      OD_FILT_VBORDER, OD_FILT_HBORDER);
        } else if (sbr > 0 && sbc > 0) {
          for (r = 0; r < OD_FILT_VBORDER; r++) {
            for (c = -OD_FILT_HBORDER; c < 0; c++) {
              src[r * OD_FILT_BSTRIDE + c + OD_FILT_HBORDER] =
                  linebuf[pli][r * stride + coffset + c];
            }
          }
        } else {
          for (r = 0; r < OD_FILT_VBORDER; r++) {
            for (c = -OD_FILT_HBORDER; c < 0; c++) {
              src[r * OD_FILT_BSTRIDE + c + OD_FILT_HBORDER] =
                  OD_DERING_VERY_LARGE;
            }
          }
        }
        if (!prev_row_dering[sbc + 1]) {
          copy_sb8_16(cm, &src[OD_FILT_HBORDER + (nhb << bsize[pli])],
                      OD_FILT_BSTRIDE, xd->plane[pli].dst.buf,
                      (MAX_MIB_SIZE << bsize[pli]) * sbr - OD_FILT_VBORDER,
                      coffset + (nhb << bsize[pli]), xd->plane[pli].dst.stride,
                      OD_FILT_VBORDER, OD_FILT_HBORDER);
        } else if (sbr > 0 && sbc < nhsb - 1) {
          for (r = 0; r < OD_FILT_VBORDER; r++) {
            for (c = nhb << bsize[pli];
                 c < (nhb << bsize[pli]) + OD_FILT_HBORDER; c++) {
              src[r * OD_FILT_BSTRIDE + c + OD_FILT_HBORDER] =
                  linebuf[pli][r * stride + coffset + c];
            }
          }
        } else {
          for (r = 0; r < OD_FILT_VBORDER; r++) {
            for (c = nhb << bsize[pli];
                 c < (nhb << bsize[pli]) + OD_FILT_HBORDER; c++) {
              src[r * OD_FILT_BSTRIDE + c + OD_FILT_HBORDER] =
                  OD_DERING_VERY_LARGE;
            }
          }
        }
        if (dering_left) {
          /* If we deringed the superblock on the left then we need to copy in
             saved pixels. */
          for (r = 0; r < rend + OD_FILT_VBORDER; r++) {
            for (c = 0; c < OD_FILT_HBORDER; c++) {
              src[r * OD_FILT_BSTRIDE + c] = colbuf[pli][r][c];
            }
          }
        }
        for (r = 0; r < rend + OD_FILT_VBORDER; r++) {
          for (c = 0; c < OD_FILT_HBORDER; c++) {
            /* Saving pixels in case we need to dering the superblock on the
               right. */
            colbuf[pli][r][c] =
                src[r * OD_FILT_BSTRIDE + c + (nhb << bsize[pli])];
          }
        }
        copy_sb8_16(cm, &linebuf[pli][coffset], stride, xd->plane[pli].dst.buf,
                    (MAX_MIB_SIZE << bsize[pli]) * (sbr + 1) - OD_FILT_VBORDER,
                    coffset, xd->plane[pli].dst.stride, OD_FILT_VBORDER,
                    (nhb << bsize[pli]));

        if (level == 0 && clpf_strength == 0) continue;
        if (tile_top) {
          for (r = 0; r < OD_FILT_VBORDER; r++) {
            for (c = 0; c < (nhb << bsize[pli]) + 2 * OD_FILT_HBORDER; c++) {
              src[r * OD_FILT_BSTRIDE + c] =
                  OD_DERING_VERY_LARGE;
            }
          }
        }
        if (tile_left) {
          for (r = 0; r < (nvb << bsize[pli]) + 2 * OD_FILT_VBORDER; r++) {
            for (c = 0; c < OD_FILT_HBORDER; c++) {
              src[r * OD_FILT_BSTRIDE + c] = OD_DERING_VERY_LARGE;
            }
          }
        }
        if (tile_bottom) {
          for (r = (nvb << bsize[pli]); r < (nvb << bsize[pli]) + OD_FILT_VBORDER; r++) {
            for (c = 0; c < (nhb << bsize[pli]) + 2 * OD_FILT_HBORDER; c++) {
              src[(r + OD_FILT_VBORDER) * OD_FILT_BSTRIDE + c] =
                  OD_DERING_VERY_LARGE;
            }
          }
        }
        if (tile_right) {
          for (r = 0; r < (nvb << bsize[pli]) + 2 * OD_FILT_VBORDER; r++) {
            for (c = (nhb << bsize[pli]); c < (nhb << bsize[pli]) + OD_FILT_HBORDER; ++c) {
              src[r * OD_FILT_BSTRIDE + c + OD_FILT_HBORDER] =
                  OD_DERING_VERY_LARGE;
            }
          }
        }
#if CONFIG_AOM_HIGHBITDEPTH
        if (cm->use_highbitdepth) {
          od_dering((uint8_t *)&CONVERT_TO_SHORTPTR(
                        xd->plane[pli]
                            .dst.buf)[xd->plane[pli].dst.stride *
                                          (MAX_MIB_SIZE * sbr << bsize[pli]) +
                                      (sbc * MAX_MIB_SIZE << bsize[pli])],
                    xd->plane[pli].dst.stride, dst,
                    &src[OD_FILT_VBORDER * OD_FILT_BSTRIDE + OD_FILT_HBORDER],
                    dec[pli], dir, NULL, var, pli, dlist, dering_count, level,
                    clpf_strength, clpf_damping, coeff_shift, 0, 1);
        } else {
#endif
          od_dering(
              &xd->plane[pli].dst.buf[xd->plane[pli].dst.stride *
                                          (MAX_MIB_SIZE * sbr << bsize[pli]) +
                                      (sbc * MAX_MIB_SIZE << bsize[pli])],
              xd->plane[pli].dst.stride, dst,
              &src[OD_FILT_VBORDER * OD_FILT_BSTRIDE + OD_FILT_HBORDER],
              dec[pli], dir, NULL, var, pli, dlist, dering_count, level,
              clpf_strength, clpf_damping, coeff_shift, 0, 0);

#if CONFIG_AOM_HIGHBITDEPTH
        }
#endif
      }
      dering_left = 1;
    }
    {
      unsigned char *tmp;
      tmp = prev_row_dering;
      prev_row_dering = curr_row_dering;
      curr_row_dering = tmp;
    }
  }
  aom_free(row_dering);
  for (pli = 0; pli < nplanes; pli++) {
    aom_free(linebuf[pli]);
  }
}
