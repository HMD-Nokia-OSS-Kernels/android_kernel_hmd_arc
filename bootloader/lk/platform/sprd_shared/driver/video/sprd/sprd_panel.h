/*
 *  <sprd_panel.h> - <sprd panel>
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

#ifndef _INTERFACE_H_
#define _INTERFACE_H_

//==================================================================
#include <sprd_common.h>
//#include <../../arch/arm/include/asm/io.h>
#include <logo_bin.h>
#include <errno.h>
//#include <../../arch/arm/include/asm/arch/common.h>
//#include <../../arch/arm/include/asm/arch/sprd_reg.h>
#include <sprd_compat.h>
#include <linux/types.h>
#include <lcd.h>


#ifndef bool
typedef int bool;
#endif
#ifndef false
#define false 0
#endif
#ifndef true
#define true 1
#endif

enum {
	BL_TYPE_PWM = 0,
	BL_TYPE_MIPI,
	BL_TYPE_I2C,
};


#define pr_err(fmt, args...) do { dprintf(ALWAYS, "[sprdfb][%s] ", __func__); dprintf(CRITICAL, fmt, ##args); } while (0)
#define pr_info(fmt, args...) do { dprintf(INFO, "[sprdfb][%s] ", __func__); dprintf(INFO, fmt, ##args);} while (0)
#define pr_emerg(fmt, args...) do { dprintf(CRITICAL, "[sprdfb][%s]", __func__); dprintf(ALWAYS, fmt, ##args); } while (0)
#define pr_debug(fmt, args...) do { } while (0)

#define msleep(a)	udelay(a * 1000)
#define CMD_END		0
//==================================================================
enum {
	SPRD_MAINLCD_ID = 0,
	SPRD_SUBLCD_ID,
	SPRD_MAX_LCD_ID,
};

/* LCD supported FPS */
#define LCD_MAX_FPS 70
#define LCD_MIN_FPS 0

enum {
	SPRD_PANEL_TYPE_MCU = 0,
	SPRD_PANEL_TYPE_RGB,
	SPRD_PANEL_TYPE_MIPI,
	SPRD_PANEL_TYPE_LVDS,
	SPRD_PANEL_TYPE_LIMIT
};

enum {
	SPRD_POLARITY_POS = 0,
	SPRD_POLARITY_NEG,
	SPRD_POLARITY_LIMIT
};

enum {
	SPRD_RGB_BUS_TYPE_I2C = 0,
	SPRD_RGB_BUS_TYPE_SPI,
	SPRD_RGB_BUS_TYPE_LVDS,
	SPRD_RGB_BUG_TYPE_LIMIT
};

enum {
	SPRD_MIPI_MODE_CMD = 0,
	SPRD_MIPI_MODE_VIDEO,
	SPRD_MIPI_MODE_LIMIT
};

enum {
	PANEL_VIDEO_NON_BURST_SYNC_PULSES = 0,
	PANEL_VIDEO_NON_BURST_SYNC_EVENTS,
	PANEL_VIDEO_BURST_MODE
};

struct rgb_timing {
	uint16_t hfp;		/*unit: pixel */
	uint16_t hbp;
	uint16_t hsync;
	uint16_t vfp;		/*unit: line */
	uint16_t vbp;
	uint16_t vsync;
};

struct dsi_cmd_header {
	uint8_t data_type;
	uint8_t wait;
	uint8_t wc_l;
	uint8_t wc_h;
};

struct dsi_cmd_desc {
	uint8_t data_type;
	uint8_t wait;
	uint8_t wc_h;
	uint8_t wc_l;
	uint8_t payload[];
};

/* LCD abstraction */
struct panel_info {
	/* common parameters */
	const char *lcd_name;
	uint8_t type;
	uint8_t bpp;
//	uint8_t fps;
	uint16_t width;
	uint16_t height;

	/* DPI specific parameters */
	uint32_t pixel_clk;
	uint32_t vl_bpix;
	uint16_t h_sync_pol;
	uint16_t v_sync_pol;
	uint16_t de_pol;
	uint16_t te_pol;
	struct rgb_timing rgb_timing;

	/* MIPI DSI specific parameters */
	uint32_t phy_freq;
	uint8_t lane_num;
	uint8_t work_mode;	/*command_mode, video_mode */
	uint8_t burst_mode;	/*burst, non-burst */
	uint8_t bl_type;
	uint8_t bl_config_bit;
	bool nc_clk_en;
	uint32_t video_lp_config;

	/* platform specific parameters */
	bool bv3_en;
	bool is_oled;

	/* mipi clk division config parameters*/
	int dpi_clk_div;
	/* enable low power cmd transsmit in video mode */
	bool video_lp_cmd_enable;
	/* disable hporch enter in low power mode */
	bool hporch_lp_disable;

	bool dpi_clk_pixelpll;

	/*lcd bias power config*/
	bool need_config_bias;
	uint8_t lcd_i2c_bus_num;
	uint8_t lcd_i2c_slaver_addr;

	/*dsc config params*/
	bool dsc_en;
	bool dual_dsi_en;
	uint32_t slice_width;
	uint32_t slice_height;
	uint32_t output_bpc;

	/* cmd vrr params */
	bool cmd_dpi_mode;
	u32 actual_dpi_clk;

	/* UMB9230S specific parameters */
	bool umb9230s_en;
	uint32_t umb9230s_phy_freq;
	uint8_t umb9230s_work_mode;	/*command_mode, video_mode */
	bool umb9230s_dsc_en;
	uint32_t umb9230s_slice_width;
	uint32_t umb9230s_slice_height;
	uint32_t umb9230s_output_bpc;
};

struct panel_ops {
	int (*init)(void);
	int (*slave_init)(void);
	int (*read_id)(struct panel_info *info);
	int (*power)(int on);
	int (*set_brightness)(int level);
	int (*sleep_in)(void);
};

struct panel_cfg {
	uint32_t lcd_id;
	struct panel_driver *drv;
};

struct panel_driver {
	struct panel_info *info;
	struct panel_ops *ops;
};

struct panel_info *panel_info_attach(void);
int sprd_panel_probe(void);
extern struct panel_info *panel_device;

#endif
