/*
 *  <sprd_spi_panel.h> - <sprd spi panel>
 *
 *  Copyright (C) 2019 Unisoc Communications Inc.
 *  History:
 *      <2023-07-11> <pony.wu@unisoc.com>
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

#ifndef _SPRDFB_PANEL_H_
#define _SPRDFB_PANEL_H_

#include <sprd_common.h>
#include <logo_bin.h>
#include <errno.h>
#include <sprd_compat.h>
#include <linux/types.h>
#include <lcd.h>

#define LCD_DelayMS(ms)  udelay(1000*(ms))


/* lcd directions */
#define LCD_DIRECT_NORMAL		0
#define LCD_DIRECT_ROT_90		1
#define LCD_DIRECT_ROT_180		2
#define LCD_DIRECT_ROT_270		3
#define LCD_DIRECT_MIR_H		4
#define LCD_DIRECT_MIR_V		5
#define LCD_DIRECT_MIR_HV		6


enum{
	SPRDFB_PANEL_TYPE_MCU = 0,
	SPRDFB_PANEL_TYPE_RGB,
	SPRDFB_PANEL_TYPE_MIPI,
	SPRDFB_PANEL_TYPE_LVDS,
	SPRDFB_PANEL_TYPE_SPI,
	SPRDFB_PANEL_TYPE_LIMIT
};

struct panel_spec;

typedef int32_t (*send_cmd_t)(uint32_t data);
typedef int32_t (*send_data_t)(uint32_t data);
typedef int32_t (*send_cmd_data_t)(uint32_t cmd, uint32_t data);
typedef uint32_t (*read_data_t)(void);

typedef void (*spi_send_cmd_t)(uint32_t cmd);
typedef void (*spi_send_data_t)(uint32_t data);
typedef void (*spi_read_t)(uint32_t *data, uint32_t len);
typedef void (*spi_clock_set_t)(unsigned int);

/* LCD operations */
struct panel_operations {
	int32_t (*panel_init)(struct panel_spec *self);
	int32_t (*panel_close)(struct panel_spec *self);
	int32_t (*panel_reset)(struct panel_spec *self);
	int32_t (*panel_refresh)(struct panel_spec *self,void *base);
	int32_t (*panel_enter_sleep)(struct panel_spec *self, uint8_t is_sleep);
	int32_t (*panel_set_contrast)(struct panel_spec *self, uint16_t contrast);
	int32_t (*panel_set_brightness)(struct panel_spec *self,
				uint16_t brightness);
	int32_t (*panel_set_window)(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom);
	int32_t (*panel_invalidate)(struct panel_spec *self);
	int32_t (*panel_invalidate_rect)(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom);
	int32_t (*panel_rotate_invalidate_rect)(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom,
				uint16_t angle);
	int32_t (*panel_set_direction)(struct panel_spec *self, uint16_t direction);
	uint32_t (*panel_readid)(struct panel_spec *self);
};

struct ops_spi{
	void (*spi_send_cmd)(uint32_t cmd);
	void (*spi_send_data)(uint32_t data);
	void (*spi_read)(uint32_t *data,uint32_t len);
	void (*spi_clock_set)(unsigned int);
};

struct spi_info{
	struct ops_spi *ops;
};

struct info_spi {
	uint16_t cmd_bus_mode;
	uint16_t bus_width;
	uint16_t bpp;
	uint16_t te_pol;
	uint16_t spi_mode; // 0,1,2,3
	uint16_t cd_gpio;
	uint32_t te_sync_delay;
	uint8_t bus_num;
	uint8_t spi_pol_mode;
	uint32_t speed;
	uint8_t cs;
	struct ops_spi *ops;
	const char *spi_panel_name;
};

/* LCD abstraction */
struct panel_spec {
	uint32_t cap;
	uint16_t width;
	uint16_t height;
	uint32_t fps;
	uint16_t type; /*mcu, rgb, mipi*/
	uint16_t direction;
	uint16_t is_need_reset;
	uint16_t is_need_dsc;
	uint16_t non_continue_clk_en;
	struct info_spi *spi;
	struct panel_operations *ops;
};

struct spi_panel_cfg {
	uint32_t lcd_id;
	struct panel_spec *panel;
};

const char *spi_panel_get_name(void);

extern void LCD_SetBackLightBrightness( unsigned long  value);
#endif

