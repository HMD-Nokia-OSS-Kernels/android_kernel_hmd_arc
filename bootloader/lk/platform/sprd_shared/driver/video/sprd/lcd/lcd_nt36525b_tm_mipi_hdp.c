/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 */

#include "../sprd_panel.h"
#include "../sprd_dsi.h"
#include "../dsi/mipi_dsi_api.h"
#include "../sprd_dphy.h"
#include "gpio_plus.h"
#include <i2c.h>

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

#define NT36525B_TM_ID 0x04

static uint8_t init_data[] = {
	0x23, 0x00, 0x00, 0x02, 0xFF, 0x23,
	0x23, 0x00, 0x00, 0x02, 0xFB, 0x01,
	0x23, 0x00, 0x00, 0x02, 0x00, 0x80,
	0x23, 0x00, 0x00, 0x02, 0x07, 0x00,
	0x23, 0x00, 0x00, 0x02, 0x08, 0x01,
	0x23, 0x00, 0x00, 0x02, 0x09, 0x01,
	0x23, 0x00, 0x00, 0x02, 0xFF, 0xD0,
	0x23, 0x00, 0x00, 0x02, 0xFB, 0x01,
	0x23, 0x00, 0x00, 0x02, 0x05, 0x07,
	0x23, 0x00, 0x00, 0x02, 0x09, 0xF0,
	0x23, 0x00, 0x00, 0x02, 0x28, 0x70,
	0x23, 0x00, 0x00, 0x02, 0xFF, 0x10,
	0x23, 0x00, 0x00, 0x02, 0xFB, 0x01,
	0x23, 0x00, 0x00, 0x02, 0x53, 0x2C,
	0x23, 0x00, 0x00, 0x02, 0x55, 0x03,
	0x39, 0x00, 0x00, 0x03, 0x68, 0x03, 0x01,
	0x23, 0x00, 0x00, 0x02, 0xBA, 0x02,
	0x23, 0x78, 0x00, 0x02, 0x11, 0x00,
	0x23, 0x14, 0x00, 0x02, 0x29, 0x00,
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

static int nt36525b_tm_init(void)
{
	struct sprd_dsi *dsi = &dsi_device;
	struct sprd_dphy *dphy = &dphy_device;

	mipi_dsi_lp_cmd_enable(dsi, true);
	mipi_dsi_send_cmds(dsi, init_data);
	mipi_dsi_set_work_mode(dsi, SPRD_MIPI_MODE_VIDEO);
	mipi_dsi_state_reset(dsi);
	mipi_dphy_hs_clk_en(dphy, true);

	return 0;
}

static int nt36525b_tm_readid(struct panel_info *info)
{
	struct sprd_dsi *dsi = &dsi_device;
	uint8_t read_buf[1] = {0};
	int i, ret = 0;
	u8 bias_config[2][2] = {{0x00, 0x14}, {0x01, 0x14}};

	mipi_dsi_lp_cmd_enable(dsi, true);
	mipi_dsi_set_max_return_size(dsi, 1);
	mipi_dsi_dcs_read(dsi, NT36525B_TM_ID, &read_buf, 1);
	if(0x20 == read_buf[0]) {
		pr_info("nt36525b tm read id success!\n");

		if (info->need_config_bias) {
			for (i = 0; i < sizeof(bias_config)/sizeof(bias_config[0]); i++) {
				ret = i2c_send(info->lcd_i2c_bus_num, info->lcd_i2c_slaver_addr, (unsigned char *)bias_config[i], ARRAY_SIZE(bias_config[i]));
				if (ret < 0) {
					pr_err("config lcd i2c bias power failed\n");
					break;
				}
			}
		}

		return 0;
    }

	pr_err("nt36525b tm read id failed!, 0x%x\n",read_buf[0]);
	return -1;
}

static int nt36525b_tm_power(int on)
{
	if (on) {
#ifdef CONFIG_LCM_GPIO_AVDDEN
        sprd_gpio_request(CONFIG_LCM_GPIO_AVDDEN);
        sprd_gpio_direction_output(CONFIG_LCM_GPIO_AVDDEN, 1);
        mdelay(10);
#endif
#ifdef CONFIG_LCM_GPIO_AVEEEN
        sprd_gpio_request(CONFIG_LCM_GPIO_AVEEEN);
        sprd_gpio_direction_output(CONFIG_LCM_GPIO_AVEEEN, 1);
        mdelay(20);
#endif

		sprd_gpio_request(CONFIG_LCM_GPIO_RSTN);
		sprd_gpio_direction_output(CONFIG_LCM_GPIO_RSTN, 1);
		mdelay(10);
		sprd_gpio_direction_output(CONFIG_LCM_GPIO_RSTN, 0);
		mdelay(5);
		sprd_gpio_direction_output(CONFIG_LCM_GPIO_RSTN, 1);
		mdelay(5);
		sprd_gpio_direction_output(CONFIG_LCM_GPIO_RSTN, 0);
		mdelay(5);
		sprd_gpio_direction_output(CONFIG_LCM_GPIO_RSTN, 1);
		mdelay(20);
	} else {
		sprd_gpio_direction_output(CONFIG_LCM_GPIO_RSTN, 1);
		mdelay(5);
	}
	return 0;
}

static struct panel_ops nt36525b_tm_ops = {
	.init = nt36525b_tm_init,
	.read_id = nt36525b_tm_readid,
	.power = nt36525b_tm_power,
};

static struct panel_info nt36525b_tm_info = {
	/* common parameters */
	.lcd_name = "lcd_nt36525b_tm_mipi_hdp",
	.type = SPRD_PANEL_TYPE_MIPI,
	.bpp = 24,
//	.fps = 60,
	.width = 720,
	.height = 1600,

	/* DPI specific parameters */
	.pixel_clk = 96000000, /*Hz*/
	.rgb_timing = {
		.hfp = 64,
		.hbp = 64,
		.hsync = 8,
		.vfp = 10,
		.vbp = 254,
		.vsync = 2,
	},

	/* MIPI DSI specific parameters */
	.phy_freq = 898000,
	.lane_num = 3,
	.work_mode = SPRD_MIPI_MODE_VIDEO,
	.burst_mode = PANEL_VIDEO_BURST_MODE,
	.bl_type = BL_TYPE_MIPI,
	.bl_config_bit = 12,
	.need_config_bias = true,
	.lcd_i2c_bus_num = 4,
	.lcd_i2c_slaver_addr = 0x3e,
};

struct panel_driver nt36525b_tm_driver = {
	.info = &nt36525b_tm_info,
	.ops = &nt36525b_tm_ops,
};
