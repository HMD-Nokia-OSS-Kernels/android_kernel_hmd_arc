/*
 *  <drm_fourcc.h> - <drm fourcc>
 *
 *  Copyright (C) 2019 Unisoc Communications Inc.
 *  History:
 *      <2021-08-09> <pony.wu@unisoc.com>
 *
 *      The above copyright notice shall be
 *      included in all copies or substantial portions of the Software.
 *
 *      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *      EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *      MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *      IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 *      CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 *      TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *      SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef SPRD_DRM_FOURCC_H
#define SPRD_DRM_FOURCC_H

#define sprd_drm_fourcc(a, b, c, d) ((__u32)(a) | ((__u32)(b) << 8) | \
				 ((__u32)(c) << 16) | ((__u32)(d) << 24))

#define SPRD_DRM_FORMAT_BIG_ENDIAN (1<<31) /* format is big endian instead of little endian */

/* color index */
/* [7:0] C */
#define SPRD_DRM_FORMAT_C8			sprd_drm_fourcc('C', '8', ' ', ' ')

/* 8 bpp Red */
/* [7:0] R */
#define SPRD_DRM_FORMAT_R8			sprd_drm_fourcc('R', '8', ' ', ' ')

/* 16 bpp RG */
/* [15:0] R:G 8:8 little endian */
#define SPRD_DRM_FORMAT_RG88		sprd_drm_fourcc('R', 'G', '8', '8')
#define SPRD_DRM_FORMAT_GR88		sprd_drm_fourcc('G', 'R', '8', '8')

/* 8 bpp RGB */
/* [7:0] R:G:B 3:3:2 */
#define SPRD_DRM_FORMAT_RGB332		sprd_drm_fourcc('R', 'G', 'B', '8')
#define SPRD_DRM_FORMAT_BGR233		sprd_drm_fourcc('B', 'G', 'R', '8')

/* 16 bpp RGB */
/* [15:0] x:R:G:B 4:4:4:4 little endian */
#define SPRD_DRM_FORMAT_XRGB4444	sprd_drm_fourcc('X', 'R', '1', '2')
#define SPRD_DRM_FORMAT_XBGR4444	sprd_drm_fourcc('X', 'B', '1', '2')
#define SPRD_DRM_FORMAT_RGBX4444	sprd_drm_fourcc('R', 'X', '1', '2')
#define SPRD_DRM_FORMAT_BGRX4444	sprd_drm_fourcc('B', 'X', '1', '2')

#define SPRD_DRM_FORMAT_ARGB4444	sprd_drm_fourcc('A', 'R', '1', '2')
#define SPRD_DRM_FORMAT_ABGR4444	sprd_drm_fourcc('A', 'B', '1', '2')
#define SPRD_DRM_FORMAT_RGBA4444	sprd_drm_fourcc('R', 'A', '1', '2')
#define SPRD_DRM_FORMAT_BGRA4444	sprd_drm_fourcc('B', 'A', '1', '2')

#define SPRD_DRM_FORMAT_XRGB1555	sprd_drm_fourcc('X', 'R', '1', '5')
#define SPRD_DRM_FORMAT_XBGR1555	sprd_drm_fourcc('X', 'B', '1', '5')
#define SPRD_DRM_FORMAT_RGBX5551	sprd_drm_fourcc('R', 'X', '1', '5')
#define SPRD_DRM_FORMAT_BGRX5551	sprd_drm_fourcc('B', 'X', '1', '5')

#define SPRD_DRM_FORMAT_ARGB1555	sprd_drm_fourcc('A', 'R', '1', '5')
#define SPRD_DRM_FORMAT_ABGR1555	sprd_drm_fourcc('A', 'B', '1', '5')
#define SPRD_DRM_FORMAT_RGBA5551	sprd_drm_fourcc('R', 'A', '1', '5')
#define SPRD_DRM_FORMAT_BGRA5551	sprd_drm_fourcc('B', 'A', '1', '5')

#define SPRD_DRM_FORMAT_RGB565		sprd_drm_fourcc('R', 'G', '1', '6')
#define SPRD_DRM_FORMAT_BGR565		sprd_drm_fourcc('B', 'G', '1', '6')

/* 24 bpp RGB */
#define SPRD_DRM_FORMAT_RGB888		sprd_drm_fourcc('R', 'G', '2', '4')
#define SPRD_DRM_FORMAT_BGR888		sprd_drm_fourcc('B', 'G', '2', '4')

/* 32 bpp RGB */
#define SPRD_DRM_FORMAT_XRGB8888	sprd_drm_fourcc('X', 'R', '2', '4')
#define SPRD_DRM_FORMAT_XBGR8888	sprd_drm_fourcc('X', 'B', '2', '4')
#define SPRD_DRM_FORMAT_RGBX8888	sprd_drm_fourcc('R', 'X', '2', '4')
#define SPRD_DRM_FORMAT_BGRX8888	sprd_drm_fourcc('B', 'X', '2', '4')

#define SPRD_DRM_FORMAT_ARGB8888	sprd_drm_fourcc('A', 'R', '2', '4')
#define SPRD_DRM_FORMAT_ABGR8888	sprd_drm_fourcc('A', 'B', '2', '4')
#define SPRD_DRM_FORMAT_RGBA8888	sprd_drm_fourcc('R', 'A', '2', '4')
#define SPRD_DRM_FORMAT_BGRA8888	sprd_drm_fourcc('B', 'A', '2', '4')

#define SPRD_DRM_FORMAT_XRGB2101010	sprd_drm_fourcc('X', 'R', '3', '0')
#define SPRD_DRM_FORMAT_XBGR2101010	sprd_drm_fourcc('X', 'B', '3', '0')
#define SPRD_DRM_FORMAT_RGBX1010102	sprd_drm_fourcc('R', 'X', '3', '0')
#define SPRD_DRM_FORMAT_BGRX1010102	sprd_drm_fourcc('B', 'X', '3', '0')

#define SPRD_DRM_FORMAT_ARGB2101010	sprd_drm_fourcc('A', 'R', '3', '0')
#define SPRD_DRM_FORMAT_ABGR2101010	sprd_drm_fourcc('A', 'B', '3', '0')
#define SPRD_DRM_FORMAT_RGBA1010102	sprd_drm_fourcc('R', 'A', '3', '0')
#define SPRD_DRM_FORMAT_BGRA1010102	sprd_drm_fourcc('B', 'A', '3', '0')

/* packed YCbCr */
#define SPRD_DRM_FORMAT_YUYV		sprd_drm_fourcc('Y', 'U', 'Y', 'V')
#define SPRD_DRM_FORMAT_YVYU		sprd_drm_fourcc('Y', 'V', 'Y', 'U')
#define SPRD_DRM_FORMAT_UYVY		sprd_drm_fourcc('U', 'Y', 'V', 'Y')
#define SPRD_DRM_FORMAT_VYUY		sprd_drm_fourcc('V', 'Y', 'U', 'Y')

#define SPRD_DRM_FORMAT_AYUV		sprd_drm_fourcc('A', 'Y', 'U', 'V')

/*
 * 2 plane YCbCr
 * index 0 = Y plane, [7:0] Y
 * index 1 = Cr:Cb plane, [15:0] Cr:Cb little endian
 * or
 * index 1 = Cb:Cr plane, [15:0] Cb:Cr little endian
 */
#define SPRD_DRM_FORMAT_NV12		sprd_drm_fourcc('N', 'V', '1', '2')
#define SPRD_DRM_FORMAT_NV21		sprd_drm_fourcc('N', 'V', '2', '1')
#define SPRD_DRM_FORMAT_NV16		sprd_drm_fourcc('N', 'V', '1', '6')
#define SPRD_DRM_FORMAT_NV61		sprd_drm_fourcc('N', 'V', '6', '1')
#define SPRD_DRM_FORMAT_NV24		sprd_drm_fourcc('N', 'V', '2', '4')
#define SPRD_DRM_FORMAT_NV42		sprd_drm_fourcc('N', 'V', '4', '2')

/*
 * 3 plane YCbCr
 * index 0: Y plane, [7:0] Y
 * index 1: Cb plane, [7:0] Cb
 * index 2: Cr plane, [7:0] Cr
 * or
 * index 1: Cr plane, [7:0] Cr
 * index 2: Cb plane, [7:0] Cb
 */
#define SPRD_DRM_FORMAT_YUV410		sprd_drm_fourcc('Y', 'U', 'V', '9')
#define SPRD_DRM_FORMAT_YVU410		sprd_drm_fourcc('Y', 'V', 'U', '9')
#define SPRD_DRM_FORMAT_YUV411		sprd_drm_fourcc('Y', 'U', '1', '1')
#define SPRD_DRM_FORMAT_YVU411		sprd_drm_fourcc('Y', 'V', '1', '1')
#define SPRD_DRM_FORMAT_YUV420		sprd_drm_fourcc('Y', 'U', '1', '2')
#define SPRD_DRM_FORMAT_YVU420		sprd_drm_fourcc('Y', 'V', '1', '2')
#define SPRD_DRM_FORMAT_YUV422		sprd_drm_fourcc('Y', 'U', '1', '6')
#define SPRD_DRM_FORMAT_YVU422		sprd_drm_fourcc('Y', 'V', '1', '6')
#define SPRD_DRM_FORMAT_YUV444		sprd_drm_fourcc('Y', 'U', '2', '4')
#define SPRD_DRM_FORMAT_YVU444		sprd_drm_fourcc('Y', 'V', '2', '4')

/* Vendor Ids: */
#define SPRD_DRM_FORMAT_MOD_NONE           0
#define SPRD_DRM_FORMAT_MOD_VENDOR_INTEL   0x01
#define SPRD_DRM_FORMAT_MOD_VENDOR_AMD     0x02
#define SPRD_DRM_FORMAT_MOD_VENDOR_NV      0x03
#define SPRD_DRM_FORMAT_MOD_VENDOR_SAMSUNG 0x04
#define SPRD_DRM_FORMAT_MOD_VENDOR_QCOM    0x05
/* add more to the end as needed */

#define fourcc_mod_code(vendor, val) \
	((((__u64)SPRD_DRM_FORMAT_MOD_VENDOR_## vendor) << 56) | (val & 0x00ffffffffffffffULL))

#define I915_FORMAT_MOD_X_TILED	fourcc_mod_code(INTEL, 1)

#define I915_FORMAT_MOD_Y_TILED	fourcc_mod_code(INTEL, 2)

#define I915_FORMAT_MOD_Yf_TILED fourcc_mod_code(INTEL, 3)

#define SPRD_DRM_FORMAT_MOD_SAMSUNG_64_32_TILE	fourcc_mod_code(SAMSUNG, 1)

#endif /* SPRD_DRM_FOURCC_H */
