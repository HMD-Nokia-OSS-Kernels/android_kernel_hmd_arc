/*
# Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
# Licensed under the Unisoc General Software License, version 1.0 (the License);
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
# Software distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OF ANY KIND, either express or implied.
# See the Unisoc General Software License, version 1.0 for more details.
*/

#include <pwm.h>

#include "../sprd_shared/driver/video/sprd/sprd_dsi.h"
#include "../sprd_shared/driver/video/sprd/dsi/mipi_dsi_api.h"
#include "../sprd_shared/driver/video/sprd/sprd_panel.h"

#define DEFAULT_MAX_BL 255

void set_backlight(unsigned int brightness)
{
	struct panel_info *info;

	info = panel_info_attach();

	if (info->bl_type == BL_TYPE_PWM) {
		pwm_config(0, brightness, 255);
	} else if(info->bl_type == BL_TYPE_MIPI) {
		unsigned char set_bl_seq[] = {0x51, 0x00,0x00};
		struct sprd_dsi *dsi = &dsi_device;
		uint32_t current_brightness;

		if (info->bl_config_bit <= 8) {
			set_bl_seq[1] = brightness;
			mipi_dsi_gen_write(dsi, set_bl_seq, 2);
		} else {
			current_brightness = (BIT(info->bl_config_bit)) * brightness / DEFAULT_MAX_BL;
			set_bl_seq[1] = (current_brightness >> 8) & 0xff;
			set_bl_seq[2] = current_brightness & 0xff;
			mipi_dsi_gen_write(dsi, set_bl_seq, 3);
		}
	} else {
		debugf("undefined backlight type");
	}

	return;
}
