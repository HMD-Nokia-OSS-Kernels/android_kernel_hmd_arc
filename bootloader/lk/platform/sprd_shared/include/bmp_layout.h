/*
 *  <bmp_layout.h> - <bmp layout>
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

#ifndef _BMP_H_
#define _BMP_H_

//#include <linux/compiler.h>

#define __packed	__attribute__((packed))

/* When accessing these fields, remember that they are stored in little
   endian format, so use linux macros, e.g. le32_to_cpu(width)          */

struct __packed bmp_header {
	/* Header */
	char signature[2];
	__u32	file_size;
	__u32	reserved;
	__u32	data_offset;
	/* InfoHeader */
	__u32	size;
	__u32	width;
	__u32	height;
	__u16	planes;
	__u16	bit_count;
	__u32	compression;
	__u32	image_size;
	__u32	x_pixels_per_m;
	__u32	y_pixels_per_m;
	__u32	colors_used;
	__u32	colors_important;
	/* ColorTable */
};

struct __packed bmp_color_table_entry {
	__u8	blue;
	__u8	green;
	__u8	red;
	__u8	reserved;
};

struct bmp_image {
	struct bmp_header header;
	/* We use a zero sized array just as a placeholder for variable
	   sized array */
	struct bmp_color_table_entry color_table[0];
};

/* Constants for the compression field */
#define BMP_BI_RGB	0
#define BMP_BI_RLE8	1
#define BMP_BI_RLE4	2

/* Data in the bmp_image is aligned to this length */
#define BMP_DATA_ALIGN	4

#endif							/* _BMP_H_ */
