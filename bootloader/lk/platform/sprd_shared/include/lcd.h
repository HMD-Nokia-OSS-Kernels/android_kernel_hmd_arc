/*
 *  <lcd.h> - <declaration of lcd>
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

#ifndef _LCD_H_
#define _LCD_H_
#include <lcd_console.h>
#include <logo_bin.h>
#include <bmp_layout.h>

extern char lcd_is_enabled;
extern int lcd_line_length;
extern struct vidinfo panel_info;

void lcd_ctrl_init(void *lcdbase);
void lcd_enable(void);
int lcd_getbpix(void);
void lcd_setcolreg(ushort regno, ushort red, ushort green, ushort blue);
void lcd_set_flush_dcache(int flush);
void set_panel_sleep_in(void);

struct bmp_image *gunzip_bmp(unsigned long addr, unsigned long *lenp,
			     void **alloc_addr);
int bmp_display(ulong addr, int x, int y);

typedef struct vidinfo {
	ushort	vl_col;		/* Number of columns (i.e. 160) */
	ushort	vl_row;		/* Number of rows (i.e. 100) */
	ushort	vl_rot;		/* Rotation of Display (0, 1, 2, 3) */
	u_char	vl_bpix;	/* Bits per pixel, 0 = 1 */
	ushort	*cmap;		/* Pointer to the colormap */
	void	*priv;		/* Pointer to driver-specific data */
} vidinfo_t;

typedef struct rgb24{
    u8 b;
    u8 g;
    u8 r;
} rgb24_t;

typedef struct argb32 {
    u8 b;
    u8 g;
    u8 r;
    u8 a;
} rgb32_t;

static ushort *configuration_get_cmap(void)
{
	return panel_info.cmap;
}

ushort *configuration_get_cmap(void);

extern vidinfo_t panel_info;

void lcd_puts(const char *s);
void lcd_putc(const char c);
int lcd_display_bitmap(ulong bmp_image, int x, int y);
void lcd_clear(void);

int lcd_get_pixel_width(void);
int lcd_get_screen_rows(void);
int lcd_get_pixel_height(void);
int lcd_getbgcolor(void);
int lcd_get_screen_columns(void);
void set_backlight(uint32_t value);
int lcd_getfgcolor(void);
void lcd_show_board_info(void);
void lcd_position_cursor(unsigned col, unsigned row);
void lcd_sync(void);
int lcd_get_size(int *line_length);
int drv_lcd_init(void);

/* Default to 8bpp if bit depth not specified */
#ifndef SPRD_LCD_BPP
#define SPRD_LCD_BPP			LCD_COLOR8
#endif

#define CONFIG_SYS_HIGH	0	/* Pins are active high			*/
#define CONFIG_SYS_LOW	1	/* Pins are active low			*/

#ifndef SPRD_LCD_DF
#define SPRD_LCD_DF			1
#endif

#if defined(CONFIG_LCD_INFO_BELOW_LOGO)
#define SPRD_LCD_INFO_Y		(BMP_LOGO_HEIGHT + VIDEO_FONT_HEIGHT)
#define SPRD_LCD_INFO_X		0
#elif defined(CONFIG_LCD_LOGO)
#define SPRD_LCD_INFO_Y		VIDEO_FONT_HEIGHT
#define SPRD_LCD_INFO_X		(BMP_LOGO_WIDTH + 4 * VIDEO_FONT_WIDTH)
#else
#define SPRD_LCD_INFO_Y		VIDEO_FONT_HEIGHT
#define SPRD_LCD_INFO_X		VIDEO_FONT_WIDTH
#endif

#define SPRD_LCD_COLOR2		1
#define SPRD_LCD_MONOCHROME	0
#define SPRD_LCD_COLOR8		3
#define SPRD_LCD_COLOR4		2
#define SPRD_LCD_COLOR32	5
#define SPRD_LCD_COLOR16	4

/* Calculate nr. of bits per pixel  and nr. of colors */
#define NBITS(bit_code)		(1 << (bit_code))
#define NCOLORS(bit_code)	(1 << NBITS(bit_code))

#if LCD_BPP == LCD_COLOR8
#define SPRD_CONSOLE_COLOR_BLACK	0
#define SPRD_CONSOLE_COLOR_RED		1
#define SPRD_CONSOLE_COLOR_GREEN	2
#define SPRD_CONSOLE_COLOR_YELLOW	3
#define SPRD_CONSOLE_COLOR_BLUE		4
#define SPRD_CONSOLE_COLOR_MAGENTA	5
#define SPRD_CONSOLE_COLOR_CYAN		6
#define SPRD_CONSOLE_COLOR_GREY		14
#define SPRD_CONSOLE_COLOR_WHITE	15		/* Must remain last / highest */
#elif LCD_BPP == LCD_COLOR32
#define SPRD_CONSOLE_COLOR_RED		0x00ff0000
#define SPRD_CONSOLE_COLOR_GREEN	0x0000ff00
#define SPRD_CONSOLE_COLOR_YELLOW	0x00ffff00
#define SPRD_CONSOLE_COLOR_BLUE		0x000000ff
#define SPRD_CONSOLE_COLOR_MAGENTA	0x00ff00ff
#define SPRD_CONSOLE_COLOR_CYAN		0x0000ffff
#define SPRD_CONSOLE_COLOR_GREY		0x00aaaaaa
#define SPRD_CONSOLE_COLOR_BLACK	0x00000000
#define SPRD_CONSOLE_COLOR_WHITE	0x00ffffff	/* Must remain last / highest */
#define NBYTES(bit_code)	(NBITS(bit_code) >> 3)
#else /* 16bpp color definitions */
#define SPRD_CONSOLE_COLOR_BLACK	0x0000
#define SPRD_CONSOLE_COLOR_WHITE	0xffff		/* Must remain last / highest */
#endif /* color definitions */

#define SPRD_CONSOLE_COLOR_BLACK2  	0
#define SPRD_CONSOLE_COLOR_WHITE2  	0xFFFFFFFF

#ifndef PAGE_SIZE
#define PAGE_SIZE	4096
#endif

#endif	/* _LCD_H_ */
