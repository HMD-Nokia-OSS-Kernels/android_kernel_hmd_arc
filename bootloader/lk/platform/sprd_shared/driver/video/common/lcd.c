/*
 *  <lcd.c> - <Common LCD routines>
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

/* #define DEBUG */
//#include <config.h>
#include <sprd_common.h>
//#include <command.h>
//#include <env_callback.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <sprd_compat.h>
#include <malloc.h>
//#include <stdio_dev.h>
#include <lcd.h>
//#include <mapmem.h>
//#include <watchdog.h>
#include <linux/unaligned/le_byteshift.h>
#include <splash.h>
#include <arch/sprd_cache.h>
//#include <asm/io.h>
#include <video_font.h>
#include <logo_bin.h>
#include "../sprd/sprd_panel.h"
#include "../sprd/sprd_dispc.h"
//#include "boot_parse.h"

extern void *fb_base;

#define RGB565_TO_INT16(rgb)  (rgb.blue >> 3 | rgb.green >> 2 << 5 | rgb.red >> 3 << 11)
#define PIXEL8_TO_INT16(index, table) 	(table[*index])
#define PIXEL16_TO_INT16(index, table)  (RGB565_TO_INT16(PIXEL8_TO_INT16(index, table)))
#define RGB16_TO_INT16(rgb)   (*rgb)
#define RGB24_TO_INT32(rgb)   (rgb->b | rgb->g << 8 | rgb->r << 16 | 0xff << 24)
#define RGB32_TO_INT32(rgb)   (rgb->b | rgb->g << 8 | rgb->r << 16 | rgb->a << 24)

#define BMP_TO_FB(dst, bits0, from, bits1, format, ...) \
do { \
	int i, j; \
	dst = (bits0 *)fb; \
	from = (bits1 *)bmap; \
	for (i = 0; i < height; ++i) { \
		for (j = 0; j < width; j++) { \
			*dst = format(from, ##__VA_ARGS__); \
			from++; \
			dst++; \
		} \
		from += bmp_width; \
		dst -= fb_width; \
	} \
} while(0)

#ifdef CONFIG_SC2703_LCD_POWERON
extern void lcd_use_sc2703l_to_power_on(void);
#endif

static void lcd_logo(void);
static int  lcd_init(void *lcdbase);
static void lcd_setbgcolor(int color);
static void lcd_setfgcolor(int color);

static int lcd_color_bg;
static int lcd_color_fg;
char lcd_is_enabled = 0;
int lcd_line_length;
static char lcd_flush_dcache;
static void *lcd_base;			/* Start of framebuffer memory	*/

static uint16_t colormap[256];
vidinfo_t panel_info = {
	.cmap = colormap,
	.vl_bpix = 8,
};

/* Flush LCD activity to the caches */
void lcd_sync(void)
{
	/*
	 * flush_dcache_range() is declared in common.h but it seems that some
	 * architectures do not actually implement it. Is there a way to find
	 * out whether it exists? For now, ARM is safe.
	 */
	int line_length;

	lcd_get_size(&line_length);
	if (lcd_flush_dcache)
		flush_dcache_range(lcd_base,
			(lcd_base + line_length * panel_info.vl_row));
}

void lcd_set_flush_dcache(int flush)
{
	lcd_flush_dcache = (flush != 0);
}

// static void lcd_stub_putc(struct stdio_dev *dev, const char c)
// {
// 	lcd_putc(c);
// }
//
// static void lcd_stub_puts(struct stdio_dev *dev, const char *s)
// {
// 	lcd_puts(s);
// }

/*
 * With most lcd drivers the line length is set up
 * by calculating it from panel_info parameters. Some
 * drivers need to calculate the line length differently,
 * so make the function weak to allow overriding it.
 */
int lcd_get_size(int *line_length)
{
	int bpp = panel_info.vl_bpix >> 3;

	/* DPU support ARGB8888(4Byte) and RGB565(2Byte) */
	*line_length = (bpp > 2 ? 4 : 2) * panel_info.vl_col;
	return panel_info.vl_col * panel_info.vl_row * bpp;
}

int drv_lcd_init(void)
{
	//struct stdio_dev lcddev;
	int rc;

	/* if lcd is enabled, no need to init it again*/
	if (lcd_is_enabled) {
		splash_get_bpix(LOGO_PART);
 		return 0;
	}

#ifdef CONFIG_SC2703_LCD_POWERON
	lcd_use_sc2703l_to_power_on();
#endif

	//lcd_base = map_sysmem(gd->fb_base, 0);
	lcd_base = fb_base;

	lcd_init(lcd_base);

	/* Device initialization */
	//memset(&lcddev, 0, sizeof(lcddev));

	//strcpy(lcddev.name, "lcd");
	//lcddev.ext   = 0;			/* No extensions */
	//lcddev.flags = DEV_FLAGS_OUTPUT;	/* Output only */
	//lcddev.putc  = lcd_stub_putc;		/* 'putc' function */
	//lcddev.puts  = lcd_stub_puts;		/* 'puts' function */

	//rc = stdio_register(&lcddev);

	//return (rc == 0) ? 1 : rc;
	return 0;
}

void lcd_clear(void)
{
	u32 bg_color = 0x0;
	char *s;
	ulong addr;
	static int do_splash = 1;
	u32 *ppix = lcd_base;
	u32 i;

#ifndef CONFIG_SYS_WHITE_ON_BLACK
	lcd_setfgcolor(SPRD_CONSOLE_COLOR_BLACK2);
	lcd_setbgcolor(SPRD_CONSOLE_COLOR_WHITE2);
	bg_color = SPRD_CONSOLE_COLOR_WHITE2;
#else
	lcd_setfgcolor(SPRD_CONSOLE_COLOR_WHITE2);
	lcd_setbgcolor(SPRD_CONSOLE_COLOR_BLACK2);
	bg_color = SPRD_CONSOLE_COLOR_BLACK2;
#endif	/* CONFIG_SYS_WHITE_ON_BLACK */

	if (panel_info.vl_bpix <= 16)
		memset((char *)lcd_base, bg_color, lcd_line_length * panel_info.vl_row);
	else {
		for (i = 0; i < panel_info.vl_col * panel_info.vl_row; i++)
			*ppix++ = bg_color;
 	}

	/* setup text-console */
	pr_info("[LCD] setting up console...\n");

	lcd_init_console(lcd_base,
			 panel_info.vl_col,
			 panel_info.vl_row,
			 panel_info.vl_rot);

}

static int lcd_init(void *lcdbase)
{
	//lcdbase will be probed from dts in lcd_ctrl_init, the value here is not correct
	lcd_ctrl_init(lcdbase);
	/* Get logo bits/pixel, initialize panel_info.vl_bpix */
	splash_get_bpix(LOGO_PART);

	/*
	 * lcd_ctrl_init() of some drivers (i.e. bcm2835 on rpi) ignores
	 * the 'lcdbase' argument and uses custom lcd base address
	 * by setting up gd->fb_base. Check for this condition and fixup
	 * 'lcd_base' address.
	 */
	//if (map_to_sysmem(lcdbase) != gd->fb_base)
	lcd_base = fb_base;

	pr_info("[LCD] Using LCD frambuffer at %p\n", fb_base);

	lcd_get_size(&lcd_line_length);
	lcd_is_enabled = 1;
	lcd_clear();

	/* Initialize the console */
	lcd_set_col(0);
#ifdef CONFIG_LCD_INFO_BELOW_LOGO
	lcd_set_row(7 + BMP_LOGO_HEIGHT / VIDEO_FONT_HEIGHT);
#else
	lcd_set_row(1);	/* leave 1 blank line below logo */
#endif

	return 0;
}

static void lcd_setfgcolor(int color)
{
	lcd_color_fg = color;
}

int lcd_getfgcolor(void)
{
	return lcd_color_fg;
}

static void lcd_setbgcolor(int color)
{
	lcd_color_bg = color;
}

int lcd_getbgcolor(void)
{
	return lcd_color_bg;
}

int lcd_getbpix(void)
{
	return panel_info.vl_bpix;
}

#ifdef CONFIG_LCD_BMP_RLE8
#define BMP_RLE8_EOL		0
#define BMP_RLE8_ESCAPE		0
#define BMP_RLE8_DELTA		2
#define BMP_RLE8_EOBMP		1

static void draw_unencoded_bitmap(ushort **fbp, char *bmap, ushort *cmap,
				  int map_cnt)
{
	while (map_cnt > 0) {
		*(*fbp)++ = cmap[*bmap++];
		map_cnt--;
	}
}

static void draw_encoded_bitmap(ushort **fbp, ushort c, int cnt)
{
	ushort *fbpp = *fbp;
	int cnt_8copy = cnt >> 3;

	cnt -= cnt_8copy << 3;
	while (cnt_8copy > 0) {
		*fbpp++ = c;
		*fbpp++ = c;
		*fbpp++ = c;
		*fbpp++ = c;
		*fbpp++ = c;
		*fbpp++ = c;
		*fbpp++ = c;
		*fbpp++ = c;
		cnt_8copy--;
	}
	while (cnt > 0) {
		*fbpp++ = c;
		cnt--;
	}
	*fbp = fbpp;
}

/*
 * Do not call this function directly, must be called from lcd_display_bitmap.
 */
static void lcd_display_rle8_bitmap(struct bmp_image *bmp, ushort *cmap,
				    char *fb, int x_off, int y_off)
{
	char *bitmap;
	ulong width, height;
	ulong cnt, runlen;
	int x, y;
	int decode = 1;

	width = get_unaligned_le32(&bmp->header.width);
	height = get_unaligned_le32(&bmp->header.height);
	bitmap = (char *)bmp + get_unaligned_le32(&bmp->header.data_offset);

	x = 0;
	y = height - 1;

	while (decode) {
		if (bitmap[0] == BMP_RLE8_ESCAPE) {
			switch (bitmap[1]) {
			case BMP_RLE8_EOBMP:
				/* end of bitmap */
				decode = 0;
				break;
			case BMP_RLE8_DELTA:
				/* delta run */
				x += bitmap[2];
				y -= bitmap[3];
				/* 16bpix, 2-byte per pixel, x should *2 */
				fb = (char *) (lcd_base + (y + y_off - 1)
					* lcd_line_length + (x + x_off) * 2);
				bitmap += 4;
				break;
			case BMP_RLE8_EOL:
				/* end of line */
				bitmap += 2;
				x = 0;
				y--;
				/* 16bpix, 2-byte per pixel, width should *2 */
				fb -= (width * 2 + lcd_line_length);
				break;
			default:
				/* unencoded run */
				runlen = bitmap[1];
				bitmap += 2;
				if (y < height) {
					if (x < width) {
						if (x + runlen > width)
							cnt = width - x;
						else
							cnt = runlen;
						draw_unencoded_bitmap(
							(ushort **)&fb,
							bitmap, cmap, cnt);
					}
					x += runlen;
				}
				bitmap += runlen;
				if (runlen & 1)
					bitmap++;
			}
		} else {
			/* encoded run */
			if (y < height) {
				runlen = bitmap[0];
				if (x < width) {
					/* aggregate the same code */
					while (bitmap[0] == 0xff &&
					       bitmap[2] != BMP_RLE8_ESCAPE &&
					       bitmap[1] == bitmap[3]) {
						runlen += bitmap[2];
						bitmap += 2;
					}
					if (x + runlen > width)
						cnt = width - x;
					else
						cnt = runlen;
					draw_encoded_bitmap((ushort **)&fb,
						cmap[bitmap[1]], cnt);
				}
				x += runlen;
			}
			bitmap += 2;
		}
	}
}
#endif

void fb_put_byte(char **fb, char **from)
{
	*(*fb)++ = *(*from)++;
}

void fb_put_word(char **fb, char **from)
{
	*(*fb)++ = *(*from)++;
	*(*fb)++ = *(*from)++;
}

void lcd_set_cmap(struct bmp_image *bmp, unsigned colors)
{
	int i;
	struct bmp_color_table_entry cte;
	ushort *cmap = configuration_get_cmap();

	for (i = 0; i < colors; ++i) {
		cte = bmp->color_table[i];
		*cmap = (((cte.red)   << 8) & 0xf800) |
			(((cte.green) << 3) & 0x07e0) |
			(((cte.blue)  >> 3) & 0x001f);
#if defined(CONFIG_MPC823)
		cmap--;
#else
		cmap++;
#endif
	}
}

int lcd_display_bitmap(ulong bmp_image, int x, int y)
{
	u8 bmp_bpix;
	u16 width, height, bmp_width, fb_width, hdr_size;
	u32 colors;
	u8 *fb, *bmap, *bmap8;
	u16 *fb16, *bmap16, *cmap_base = NULL;
	u32 *fb32;
	rgb24_t *bmap24;
	rgb32_t *bmap32;
	struct bmp_color_table_entry *palette;
	struct bmp_image *bmp = (struct bmp_image *)bmp_image;

	if (!bmp || !(bmp->header.signature[0] == 'B' &&
		bmp->header.signature[1] == 'M')) {
		pr_info("Error: no valid bmp image at %lx\n", bmp_image);

		return 1;
	}

	palette = bmp->color_table;

	hdr_size = get_unaligned_le16(&bmp->header.size);
	bmp_bpix = get_unaligned_le16(&bmp->header.bit_count);
	height = get_unaligned_le32(&bmp->header.height);
	width = get_unaligned_le32(&bmp->header.width);

	pr_info("hdr_size=%d, bmp_bpix=%d\n", hdr_size, bmp_bpix);

	/*
	 * We support displaying 8bpp BMPs on 16bpp LCDs
	 * and displaying 24bpp BMPs on 32bpp LCDs
	 * */
	if (bmp_bpix != 8 && bmp_bpix != 16 && bmp_bpix != 24 && bmp_bpix != 32) {
		pr_info ("Error: BMP %d bit/pixel mode not support!\n", bmp_bpix);

		return 1;
	}

	colors = 1 << bmp_bpix;
	pr_info("Display-bmp: %d x %d  with %d colors, bpix: %d\n",
	     width, height, colors, bmp_bpix);

	if (bmp_bpix == 8)
		lcd_set_cmap(bmp, colors);

	if ((x + width) > panel_info.vl_col)
		width = panel_info.vl_col - x;
	if ((y + height) > panel_info.vl_row)
		height = panel_info.vl_row - y;

	bmap = (char *)bmp + get_unaligned_le32(&bmp->header.data_offset);
	fb   = (char *)(lcd_base + (y + height - 1) * lcd_line_length +
			x * (lcd_line_length / panel_info.vl_col));

	bmp_width = (width & 0x3 ? (width & ~0x3) + 4 : width) - width;
	fb_width = lcd_line_length / (bmp_bpix > 16 ? 4 : 2) + width;

	switch (bmp_bpix) {
	case 1:
	case 8:
		cmap_base = configuration_get_cmap();
#ifdef CONFIG_LCD_BMP_RLE8
		u32 compression = get_unaligned_le32(&bmp->header.compression);
		pr_info("compressed %d %d\n", compression, BMP_BI_RLE8);
		if (compression == BMP_BI_RLE8) {
			lcd_display_rle8_bitmap(bmp, cmap_base, fb, x, y);
			break;
		}
#endif
		if (cmap_base)
			BMP_TO_FB(fb16, u16, bmap8, u8, PIXEL8_TO_INT16, cmap_base);
		else
			BMP_TO_FB(fb16, u16, bmap8, u8, PIXEL16_TO_INT16, palette);
		break;
	case 16:
		BMP_TO_FB(fb16, u16, bmap16, u16, RGB16_TO_INT16);
		break;
	case 24:
		BMP_TO_FB(fb32, u32, bmap24, rgb24_t, RGB24_TO_INT32);
		break;
	case 32:
		BMP_TO_FB(fb32, u32, bmap32, rgb32_t, RGB32_TO_INT32);
		break;
	default:
		break;
	};

	lcd_sync();
	return 0;
}

int lcd_get_pixel_width(void)
{
	return panel_info.vl_col;
}

int lcd_get_pixel_height(void)
{
	return panel_info.vl_row;
}
