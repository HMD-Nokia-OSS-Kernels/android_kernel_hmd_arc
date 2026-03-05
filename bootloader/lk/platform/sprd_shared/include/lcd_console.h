/*
 *  <lcd_console.h> - <lcd console>
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
#include <logo_bin.h>

/* By default we scroll by a single line */
#ifndef CONFIG_CONSOLE_SCROLL_LINES
#define CONFIG_CONSOLE_SCROLL_LINES 1
#endif

struct console_t {
	short curr_col, curr_row;
	short cols, rows;
	void *fbbase;
	u32 lcdsizex, lcdsizey, lcdrot;
	void (*fp_putc_xy)(struct console_t *pcons, ushort x, ushort y, char c);
	void (*fp_console_moverow)(struct console_t *pcons,
				   u32 rowdst, u32 rowsrc);
	void (*fp_console_setrow)(struct console_t *pcons, u32 row, int clr);
};

void lcd_init_console(void *address, int vl_cols, int vl_rows, int vl_rot);
void console_calc_rowcol(struct console_t *pcons, u32 sizex, u32 sizey);
void lcd_set_row(short row);
void lcd_set_col(short col);
int lcd_get_screen_rows(void);
void lcd_position_cursor(unsigned col, unsigned row);
void lcd_putc(const char c);
int lcd_get_screen_columns(void);
void lcd_printf(const char *fmt, ...);
void lcd_puts(const char *s);
void lcd_printf_xy(struct sprd_font_info *font_info, const char *fmt, ...);
//start add by hyinfeng for HMD NYX-190 show warning messages
#ifndef CONFIG_SYS_WHITE_ON_BLACK
#define CONFIG_SYS_WHITE_ON_BLACK
void console_setFGColor(int color);
#else
void console_setFGColor(int color);
#endif
//end add by hyinfeng for HMD NYX-190 show warning messages
