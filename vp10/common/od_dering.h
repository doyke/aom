/*Daala video codec
Copyright (c) 2003-2010 Daala project contributors.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.*/

#ifndef VP10_COMMON_OD_DERING_H_
#define VP10_COMMON_OD_DERING_H_

#include "vp10/common/enums.h"
#include "vpx/vpx_integer.h"
#include "vpx_dsp/vpx_dsp_common.h"
#include "vpx_ports/bitops.h"

/*Smallest blocks are 4x4*/
# define OD_LOG_BSIZE0 (2)
/*There are 5 block sizes total (4x4, 8x8, 16x16, 32x32 and 64x64).*/
# define OD_NBSIZES    (5)
/*The log of the maximum length of the side of a block.*/
# define OD_LOG_BSIZE_MAX (OD_LOG_BSIZE0 + OD_NBSIZES - 1)
/*The maximum length of the side of a block.*/
# define OD_BSIZE_MAX     (1 << OD_LOG_BSIZE_MAX)

typedef int od_coeff;
#define OD_COEFF_SHIFT (0)  // NB: differs from daala

#define OD_DIVU_SMALL(_x, _d) ((_x) / (_d))

#define OD_MINI VPXMIN
#define OD_CLAMPI(min, val, max) clamp((val), (min), (max))

#  define OD_ILOG_NZ(x) get_msb(x)
/*Note that __builtin_clz is not defined when x == 0, according to the gcc
   documentation (and that of the x86 BSR instruction that implements it), so
   we have to special-case it.
  We define a special version of the macro to use when x can be zero.*/
#  define OD_ILOG(x) ((x) ? OD_ILOG_NZ(x) : 0)

#define OD_DERINGSIZES (2)

#define OD_DERING_LEVELS (6)
extern const double OD_DERING_GAIN_TABLE[OD_DERING_LEVELS];

#define OD_DERING_NBLOCKS (OD_BSIZE_MAX/8)

#define OD_FILT_BORDER (3)
#define OD_FILT_BSTRIDE (OD_BSIZE_MAX + 2*OD_FILT_BORDER)

extern const int OD_DIRECTION_OFFSETS_TABLE[8][3];

typedef void (*od_filter_dering_direction_func)(int16_t *y, int ystride,
 const int16_t *in, int threshold, int dir);
typedef void (*od_filter_dering_orthogonal_func)(int16_t *y, int ystride,
 const int16_t *in, const int16_t *x, int xstride, int threshold, int dir);

void od_dering(int16_t *y, int ystride, const int16_t *x,
 int xstride, int ln, int sbx, int sby, int nhsb, int nvsb, int q, int xdec,
 int dir[OD_DERING_NBLOCKS][OD_DERING_NBLOCKS], int pli, unsigned char *bskip,
 int skip_stride, double gain);
void od_filter_dering_direction_c(int16_t *y, int ystride, const int16_t *in,
 int ln, int threshold, int dir);
void od_filter_dering_orthogonal_c(int16_t *y, int ystride, const int16_t *in,
 const int16_t *x, int xstride, int ln, int threshold, int dir);



extern const od_filter_dering_direction_func
 OD_DERING_DIRECTION_C[OD_DERINGSIZES];
extern const od_filter_dering_orthogonal_func
 OD_DERING_ORTHOGONAL_C[OD_DERINGSIZES];

void od_filter_dering_direction_4x4_c(int16_t *y, int ystride, const int16_t *in,
 int threshold, int dir);
void od_filter_dering_direction_8x8_c(int16_t *y, int ystride, const int16_t *in,
 int threshold, int dir);
void od_filter_dering_orthogonal_4x4_c(int16_t *y, int ystride, const int16_t *in,
 const int16_t *x, int xstride, int threshold, int dir);
void od_filter_dering_orthogonal_8x8_c(int16_t *y, int ystride, const int16_t *in,
 const int16_t *x, int xstride, int threshold, int dir);

#endif  // VP10_COMMON_OD_DERING_H_
