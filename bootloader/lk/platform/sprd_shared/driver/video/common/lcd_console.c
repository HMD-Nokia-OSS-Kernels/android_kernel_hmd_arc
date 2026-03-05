/*
 *  <lcd_console.c> - <LCD console routines>
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
//ZOVERLAY_TAG_HMD_ONEIMAGE
#include <sprd_common.h>
#include <serial_sprd.h>
#include <malloc.h>
#include <lcd.h>
//#include <lk/debug.h>
#include <video_font.h>		/* Get font data, width and height */
//#include "config.h"
#include <errno.h>
#include "../sprd/sprd_panel.h"
#include "../sprd/sprd_dispc.h"

static struct console_t cons;

static u16 *dst16, *src16;
static u32 *dst32, *src32;
static int bpix = 32;
static int fg_color, bg_color;
extern int panel_enabled;
static u32 *p_font_data[5];

#define VIDEO_FONT_BASE_WIDTH	8
#define VIDEO_FONT_BASE_HEIGHT	16

#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + \
 									sizeof(CONFIG_SYS_PROMPT) + 16)

#define min_t(type, x, y) ({			\
		type __min1 = (x);			\
		type __min2 = (y);			\
		__min1 < __min2 ? __min1: __min2; })

#define LCD_PUTC_XY0(dst, pixel) \
do { \
	int i, row; \
	uchar bits; \
	dst = (pixel *)pcons->fbbase + y * pcons->lcdsizex + x; \
	for (row = 0; row < VIDEO_FONT_HEIGHT; row++) { \
		bits = video_fontdata[c * VIDEO_FONT_HEIGHT + row]; \
		for (i = 0; i < VIDEO_FONT_WIDTH; ++i) { \
			*dst++ = (bits & 0x80) ? fg_color : bg_color; \
			bits <<= 1; \
		} \
		dst += (pcons->lcdsizex - VIDEO_FONT_WIDTH); \
	} \
} while(0)

#define CONSOLE_SETROW0(dst, pixel) \
do { \
	int i; \
	dst = (pixel *)pcons->fbbase + row * VIDEO_FONT_HEIGHT * pcons->lcdsizex; \
	for (i = 0; i < (VIDEO_FONT_HEIGHT * pcons->lcdsizex); i++) \
		*dst++ = clr; \
} while(0)

#define CONSOLE_MOVEROW0(dst, src, pixel) \
do { \
	int i; \
	dst = (pixel *)pcons->fbbase + rowdst * VIDEO_FONT_HEIGHT * pcons->lcdsizex; \
	src = (pixel *)pcons->fbbase + rowsrc * VIDEO_FONT_HEIGHT * pcons->lcdsizex; \
	for (i = 0; i < (VIDEO_FONT_HEIGHT * pcons->lcdsizex); i++) \
		*dst++ = *src++; \
} while(0)

#define LCD_PUTC_XY0_FORMAT(times, color, dst, pixel) \
do { \
	int i, row; \
	u32 bits; \
	dst = (pixel *)cons.fbbase + y * cons.lcdsizex + x; \
	for (row = 0; row < (VIDEO_FONT_BASE_HEIGHT * times); row++) { \
		bits = p_font_data[times][c * VIDEO_FONT_BASE_HEIGHT * times + row]; \
		for (i = 0; i < (VIDEO_FONT_BASE_WIDTH * times); ++i) { \
			*dst++ = (bits & (1 << (VIDEO_FONT_BASE_WIDTH * times - 1))) ? color : bg_color; \
			bits <<= 1; \
		} \
		dst += (cons.lcdsizex - (VIDEO_FONT_BASE_WIDTH * times)); \
	} \
} while(0)

void lcd_set_col(short col)
{
	cons.curr_col = col;
}

void lcd_set_row(short row)
{
	cons.curr_row = row;
}

void lcd_position_cursor(unsigned col, unsigned row)
{
	cons.curr_col = min_t(short, col, cons.cols - 1);
	cons.curr_row = min_t(short, row, cons.rows - 1);
}

int lcd_get_screen_rows(void)
{
	return cons.rows;
}

int lcd_get_screen_columns(void)
{
	return cons.cols;
}

static inline void console_setrow0(struct console_t *pcons, u32 row, int clr)
{
	if (bpix > 16)
		CONSOLE_SETROW0(dst32, u32);
	else
		CONSOLE_SETROW0(dst16, u16);
}

static void lcd_putc_xy0(struct console_t *pcons, ushort x, ushort y, char c)
{
	if (bpix > 16)
		LCD_PUTC_XY0(dst32, u32);
	else
		LCD_PUTC_XY0(dst16, u16);
}

static inline void console_moverow0(struct console_t *pcons,
				    u32 rowdst, u32 rowsrc)
{
	if (bpix > 16)
		CONSOLE_MOVEROW0(dst32, src32, u32);
	else
		CONSOLE_MOVEROW0(dst16, src16, u16);
}

static inline void console_back(void)
{
	if (--cons.curr_col < 0) {
		cons.curr_col = cons.cols - 1;
		if (--cons.curr_row < 0)
			cons.curr_row = 0;
	}

	cons.fp_putc_xy(&cons,
			cons.curr_col * VIDEO_FONT_WIDTH,
			cons.curr_row * VIDEO_FONT_HEIGHT, ' ');
}

void console_calc_rowcol(struct console_t *pcons, u32 sizex, u32 sizey)
{
	pcons->cols = sizex / VIDEO_FONT_WIDTH;
#if defined(CONFIG_LCD_LOGO) && !defined(CONFIG_LCD_INFO_BELOW_LOGO)
	pcons->rows = (pcons->lcdsizey - BMP_LOGO_HEIGHT);
	pcons->rows /= VIDEO_FONT_HEIGHT;
#else
	pcons->rows = sizey / VIDEO_FONT_HEIGHT;
#endif
}

void lcd_init_console_rot(struct console_t *pcons)
{
	return;
}

static inline void console_newline(void)
{
	const int rows = CONFIG_CONSOLE_SCROLL_LINES;
	int bg_color = lcd_getbgcolor();
	int i;

	cons.curr_col = 0;

	/* Check if we need to scroll the terminal */
	if (++cons.curr_row >= cons.rows) {
		for (i = 0; i < cons.rows-rows; i++)
			cons.fp_console_moverow(&cons, i, i+rows);
		for (i = 0; i < rows; i++)
			cons.fp_console_setrow(&cons, cons.rows-i-1, bg_color);
		cons.curr_row -= rows;
	}
	lcd_sync();
}

void lcd_init_console(void *address, int vl_cols, int vl_rows, int vl_rot)
{
	bpix = lcd_getbpix();
	fg_color = lcd_getfgcolor();
	bg_color = lcd_getbgcolor();

	memset(&cons, 0, sizeof(cons));
	cons.fbbase = address;

	cons.lcdsizex = vl_cols;
	cons.lcdsizey = vl_rows;
	cons.lcdrot = vl_rot;

	cons.fp_putc_xy = &lcd_putc_xy0;
	cons.fp_console_moverow = &console_moverow0;
	cons.fp_console_setrow = &console_setrow0;
	console_calc_rowcol(&cons, cons.lcdsizex, cons.lcdsizey);

	lcd_init_console_rot(&cons);

	p_font_data[3] = video_fontdata_24;
	p_font_data[4] = video_fontdata_32;

	dprintf(INFO,"lcd_console: have %d/%d col/rws on scr %dx%d (%d deg rotated)\n",
	      cons.cols, cons.rows, cons.lcdsizex, cons.lcdsizey, vl_rot);
}

void lcd_puts(const char *s)
{
	if (!lcd_is_enabled) {
		sprd_serial_puts(s);

		return;
	}

	while (*s)
		lcd_putc(*s++);

	lcd_sync();
}

void lcd_putc(const char c)
{
	if (!lcd_is_enabled) {
		sprd_serial_putc(c);

		return;
	}

	switch (c) {
	case '\r':
		cons.curr_col = 0;
		return;
	case '\n':
		console_newline();

		return;
	case '\t':	/* Tab (8 chars alignment) */
		cons.curr_col +=  8;
		cons.curr_col &= ~7;

		if (cons.curr_col >= cons.cols)
			console_newline();

		return;
	case '\b':
		console_back();

		return;
	default:
		cons.fp_putc_xy(&cons,
				cons.curr_col * VIDEO_FONT_WIDTH,
				cons.curr_row * VIDEO_FONT_HEIGHT, c);
		if (++cons.curr_col >= cons.cols)
			console_newline();
	}
}

// ning.wei@hmd++ for fastboot ui display sync from solo begin
void console_setfgcolor(int color)
{
    fg_color = color;
}
void console_setbgcolor(int color)
{
    bg_color = color;
}
// ning.wei@hmd++ for fastboot ui display sync from solo end

void lcd_printf(const char *fmt, ...)
{
	va_list args;
	char *buf = NULL;
	struct panel_info *info;
	struct sprd_dispc *dispc;
	buf = malloc(CONFIG_SYS_PBSIZE);
	if (buf == NULL) {
		errorf("no enough heap for lcd_printf\n");
		return;
	}
	memset(buf, 0, CONFIG_SYS_PBSIZE);

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	lcd_puts(buf);

	if (panel_enabled) {
		info = panel_info_attach();
		dispc = &dispc_device;
		if ((!info) || (!dispc)) {
			errorf("lcd_printf info or dispc is NULL pointer\n");
			free(buf);
			buf = NULL;
			return;
		}
		if (info->work_mode == SPRD_MIPI_MODE_CMD)
			sprd_dispc_run(dispc);
	}

	free(buf);
	buf = NULL;
}

void lcd_putc_format(u8 times, u32 color, const char c)
{
	int x = cons.curr_col * VIDEO_FONT_BASE_WIDTH * times;
	int y = cons.curr_row * VIDEO_FONT_BASE_HEIGHT * times;

	if (!lcd_is_enabled) {
		sprd_serial_putc(c);

		return;
	}

	switch (c) {
	case '\r':
		cons.curr_col = 0;
		return;
	case '\n':
		console_newline();

		return;
	case '\t':	/* Tab (8 chars alignment) */
		cons.curr_col +=  8;
		cons.curr_col &= ~7;

		if (cons.curr_col >= cons.cols)
			console_newline();

		return;
	case '\b':
		console_back();

		return;
	default:
		if (bpix > 16)
			LCD_PUTC_XY0_FORMAT(times, color, dst32, u32);
		else
			LCD_PUTC_XY0_FORMAT(times, color, dst16, u16);

		if (++cons.curr_col >= cons.cols)
			console_newline();
	}
}

void lcd_puts_format(u8 times, u32 color, const char *s)
{
	if(times !=3 && times !=4) {
		errorf("input times is illegal set times to default\n");
		times = 3;
	}

	if (!lcd_is_enabled) {
		sprd_serial_puts(s);

		return;
	}

	while (*s)
		lcd_putc_format(times, color, *s++);

	lcd_sync();
}

/*
 * print string with a customise font
 * @font_info: the info of strings
 * @fmt: the string you want to print
 */
void lcd_printf_xy(struct sprd_font_info *font_info, const char *fmt, ...)
{
	va_list args;
	char *buf = NULL;
	struct panel_info *info;
	struct sprd_dispc *dispc;

	buf = malloc(CONFIG_SYS_PBSIZE);
	if (buf == NULL) {
		errorf("no enough heap for lcd_printf_format\n");
		return;
	}
	memset(buf, 0, CONFIG_SYS_PBSIZE);

	lcd_set_row(font_info->st_x);
	lcd_set_col(font_info->st_y);
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	lcd_puts_format(font_info->size / VIDEO_FONT_BASE_WIDTH, font_info->color, buf);
	if (panel_enabled) {
		info = panel_info_attach();
		dispc = &dispc_device;
		if ((!info) || (!dispc)) {
			errorf("lcd_printf info or dispc is NULL pointer\n");
			free(buf);
			buf = NULL;
			return;
		}
		if (info->work_mode == SPRD_MIPI_MODE_CMD)
			sprd_dispc_run(dispc);
	}
	lcd_set_row(0);
	lcd_set_col(0);

	free(buf);
	buf = NULL;
}
//start add by hyinfeng for HMD NYX-190 show warning messages
#ifdef CONFIG_SYS_WHITE_ON_BLACK
void console_setFGColor(int color)
{
	fg_color = color;
}
#endif
//end add by hyifeng for HMD NYX-190 show warning messages