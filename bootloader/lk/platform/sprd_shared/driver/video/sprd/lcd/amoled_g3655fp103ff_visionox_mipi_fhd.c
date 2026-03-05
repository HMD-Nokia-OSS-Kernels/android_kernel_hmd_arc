/*
 *  <amoled_g3655fp103ff_visionox_mipi_fhd.c> - <amoled g3655fp103ff visionox fhd>
 *
 *  Copyright (C) 2019 Unisoc Communications Inc.
 *  History:
 *      <2022-02-14> <pony.wu@unisoc.com>
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

#include "../sprd_panel.h"
#include "../sprd_dsi.h"
#include "../dsi/mipi_dsi_api.h"
#include "../sprd_dphy.h"
#include "gpio_plus.h"
#include <i2c.h>
#include <sprd_regulator.h>

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

static uint8_t init_data[] = {
	// CMD FOR DSC MODE
	0x15, 0x00, 0x00, 0x02, 0xfe, 0x40,
	0x15, 0x00, 0x00, 0x02, 0xbd, 0x00,//60fps
	//0x15, 0x00, 0x00, 0x02, 0xbd, 0x06,//90fps
	//0x15, 0x00, 0x00, 0x02, 0xbd, 0x07,//120fps
	0x15, 0x00, 0x00, 0x02, 0xfe, 0xa1,
	0x15, 0x00, 0x00, 0x02, 0xcd, 0x6b,
	0x15, 0x00, 0x00, 0x02, 0xce, 0xbb,
	0x15, 0x00, 0x00, 0x02, 0xfe, 0xd1,
	0x15, 0x00, 0x00, 0x02, 0xb4, 0x01,
	0x15, 0x00, 0x00, 0x02, 0xfe, 0x38,
	0x15, 0x00, 0x00, 0x02, 0x17, 0x0f,
	0x15, 0x00, 0x00, 0x02, 0x18, 0x0f,
	0x15, 0x00, 0x00, 0x02, 0xfe, 0x00,
	0x39, 0x00, 0x00, 0x03, 0x44, 0x04, 0xb0,
	0x15, 0x00, 0x00, 0x02, 0xfa, 0x01,
	0x15, 0x00, 0x00, 0x02, 0xc2, 0x08,
	0x15, 0x00, 0x00, 0x02, 0x35, 0x00,
	0x39, 0x00, 0x00, 0x03, 0x51, 0x00, 0x00,
	//----------------------LCD initial code End----------------------//
	//SLPOUT and DISPON
	0x05, 0x32, 0x00, 0x01, 0x11,
	0x05, 0x32, 0x00, 0x01, 0x29,
	CMD_END
};

static uint8_t sleep_in_data[] ={
	0x05, 0x32, 0x00, 0x01, 0x28,
	0x05, 0x32, 0x00, 0x01, 0x10,
	CMD_END
};

static int mipi_dsi_send_cmds(struct sprd_dsi *dsi, void *data)
{
	uint16_t len;
	struct dsi_cmd_desc *cmds = data;

	if ((cmds == NULL) || (dsi == NULL))
		return -1;

	for (; cmds->data_type != CMD_END;) {
		len = (cmds->wc_h << 8) | cmds->wc_l;
		mipi_dsi_dcs_write(dsi, cmds->payload, len);
		if (cmds->wait)
			msleep(cmds->wait);
		cmds = (struct dsi_cmd_desc *)(cmds->payload + len);
	}
	return 0;
}

static int g3655fp103ff_init(void)
{
	struct sprd_dsi *dsi = &dsi_device;
	struct sprd_dphy *dphy = &dphy_device;

	mipi_dsi_lp_cmd_enable(dsi, true);
	mipi_dsi_send_cmds(dsi, init_data);
	mipi_dsi_set_work_mode(dsi, SPRD_MIPI_MODE_CMD);
	mipi_dsi_lp_cmd_enable(dsi, true);
	mipi_dsi_state_reset(dsi);
	mipi_dphy_hs_clk_en(dphy, true);

	return 0;
}

static int g3655fp103ff_readid(struct panel_info *info)
{
	struct sprd_dsi *dsi = &dsi_device;
	uint8_t read_buf[4] = {0};
	int i, ret = 0;

	mipi_dsi_lp_cmd_enable(dsi, true);
	mipi_dsi_set_max_return_size(dsi, 4);
	mipi_dsi_dcs_read(dsi, 0x04, read_buf, 4);

	if(((0x00 == read_buf[0]) && (0x00 == read_buf[1]) && (0x06 == read_buf[2])) ||
	  ((0x00 == read_buf[0]) && (0x00 == read_buf[1]) && (0x07 == read_buf[2]))) {
		pr_info("g3655fp103ff read id success!\n");
		return 0;
	}

	pr_err("g3655fp103ff read id failed:0x%x,0x%x,0x%x\n",read_buf[0],read_buf[1],read_buf[2]);
	return -1;
}

static int g3655fp103ff_power(int on)
{
	if (on) {
		/* config RST */
		sprd_gpio_request(CONFIG_LCM_GPIO_RSTN);
		sprd_gpio_direction_output(CONFIG_LCM_GPIO_RSTN, 1);
		mdelay(20);
		sprd_gpio_direction_output(CONFIG_LCM_GPIO_RSTN, 0);
		mdelay(20);
		sprd_gpio_direction_output(CONFIG_LCM_GPIO_RSTN, 1);
		mdelay(50);
	} else {
		sprd_gpio_direction_output(CONFIG_LCM_GPIO_RSTN, 0);
		mdelay(20);
	}

	return 0;
}

static int g3655fp103ff_sleep_in(void)
{
	struct sprd_dsi *dsi = &dsi_device;

	mipi_dsi_lp_cmd_enable(dsi, true);
	mipi_dsi_send_cmds(dsi, sleep_in_data);
	pr_info("g3655fp103ff_sleep_in end\n");
	return 0;
}

static struct panel_ops g3655fp103ff_ops = {
	.init = g3655fp103ff_init,
	.read_id = g3655fp103ff_readid,
	.power = g3655fp103ff_power,
	.sleep_in = g3655fp103ff_sleep_in,
};

static struct panel_info g3655fp103ff_info = {
	/* common parameters */
	.lcd_name = "amoled_g3655fp103ff_visionox_mipi_fhd",
	.type = SPRD_PANEL_TYPE_MIPI,
	.bpp = 24,
//	.fps = 60,
	.width = 1080,
	.height = 2400,

	/* DPI specific parameters */
	.pixel_clk = 376000000, /*Hz*/
	.rgb_timing = {
		.hfp = 100,
		.hbp = 92,
		.hsync = 8,
		.vfp = 2460,
		.vbp = 12,
		.vsync = 20,
	},

	/* MIPI DSI specific parameters */
	.phy_freq = 1000000,
	.lane_num = 4,
	.work_mode = SPRD_MIPI_MODE_CMD,
	.burst_mode = PANEL_VIDEO_BURST_MODE,
	.nc_clk_en = false,
	.bl_type = BL_TYPE_MIPI,
	.bl_config_bit = 12,
	.need_config_bias = false,
	.dsc_en = 1,
	.cmd_dpi_mode = true,
	.actual_dpi_clk = 384000000,
	.dual_dsi_en = 0,
	.slice_width = 540,
	.slice_height = 20,
	.output_bpc = 8,
};

struct panel_driver g3655fp103ff_visionox_driver = {
	.info = &g3655fp103ff_info,
	.ops = &g3655fp103ff_ops,
};
