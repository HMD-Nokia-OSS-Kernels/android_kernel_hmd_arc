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

#include <sprd_common.h>
#include <asm/arch/hardware.h>
#include <asm/arch/sprd_reg.h>
#include <lcd.h>
#include <logo_bin.h>
#include <lk/reg.h>
#include <lk/board.h>

#include "../sprd_shared/driver/video/sprd/sprd_dsi.h"
#include "../sprd_shared/driver/video/sprd/dsi/mipi_dsi_api.h"
#include "../sprd_shared/driver/video/sprd/sprd_panel.h"

#define PWM_INDEX  1

#define DEFAULT_MAX_BL 255

/*r3p0*/

#define PWM_PRESCALE	(0x0000)
#define PWM_MOD		(0x0004)
#define PWM_DUTY		(0x0008)
#define PWM_DIV			(0x000c)
#define PWM_PAT_LOW	(0x0010)
#define PWM_PAT_HIGH	(0x0014)
#define PWM_ENABLE 	(0x0018)
#define PWM_VERSION	(0x001c)

#define PWM2_SCALE		0x0
#define PWM_MOD_MAX 0xff
#define PWM_REG_MSK 0xffff



static inline uint32_t pwm_read(int index, uint32_t reg)
{
	return __raw_readl(CTL_BASE_PWM + index * 0x20 + reg);
}

static void pwm_write(int index, uint32_t value, uint32_t reg)
{
	__raw_writel(value, CTL_BASE_PWM + index * 0x20 + reg);
}

static void __raw_bits_or(unsigned int v, unsigned int a)
{
	__raw_writel((__raw_readl(a) | v), a);
}

void set_backlight(uint32_t brightness)
{
	struct panel_info *info;
	int index = PWM_INDEX;

	if (0 == panel_enabled) {
		debugf("lcd is not enabled, backlight is off and skip config backlight\n");
		return;
	}

	info = panel_info_attach();

	if (info->bl_type == BL_TYPE_PWM) {
		__raw_bits_or((0x1 << 0), REG_AON_CLK_CORE_CGM_PWM0_CFG + index * 4);//ext_26m select
		__raw_bits_or(((1 << PWM_INDEX) << 4), REG_AON_APB_APB_EB0); //PWMx EN


		if (0 == brightness) {
			pwm_write(index, 0, PWM_ENABLE);
			debugf("sprd backlight power off. pwm_index=%d  brightness=%d\n", index, brightness);
		} else {
#ifdef ZCFG_UBOOT_BACKLIGHT_DELAY_TIME
			mdelay(ZCFG_UBOOT_BACKLIGHT_DELAY_TIME);
#endif
			pwm_write(index, PWM2_SCALE, PWM_PRESCALE);
			pwm_write(index, PWM_MOD_MAX, PWM_MOD);
			pwm_write(index,  brightness, PWM_DUTY);

			pwm_write(index, PWM_REG_MSK, PWM_PAT_LOW);
			pwm_write(index, PWM_REG_MSK, PWM_PAT_HIGH);
			pwm_write(index, 1, PWM_ENABLE);
			debugf("sprd backlight power on. pwm_index=%d  brightness=%d\n", index, brightness);
		}
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
