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

#include <lk/debug.h>
#include <target.h>
#include <adi_hal_internal.h>
#include <storage_init.h>
#include <ufs_storage_init.h>
#include <asm/arch/pinmap.h>
#include <eic.h>
#include <gpio.h>
#include <i2c.h>
#include <serial_sprd.h>
#include <sprd_adc.h>
#include <sprd_glb.h>
#include <asm/arch/common.h>
#include <asm/arch/sprd_reg.h>
#include <sprd_battery.h>
#ifdef CONFIG_SENSOR_HUB_LK
#include <sprd_sensor.h>
#include "sensor_board_info.h"
#endif
#ifdef SPRD_TRACE
#include <sprd_trace.h>
#endif
#include <chipram_env.h>

extern void board_keypad_init(void);
extern int regulator_init(void);
extern int boardid_init(void);
extern void misc_init(void);
extern void sprd_led_init(void);
extern void sprd_pmu_lowpower_init(void);
extern int sprd_get_pcbversion(void);
extern boot_device_t get_bootdevice(void);
extern void change_cdac_from_miscdata(void);
#ifdef CONFIG_USBPINMUX
extern void usb_uart_inf_config(void);
#endif

void wait_mm_wakeup(void)
{
	uint32_t cnt = 0;
	while(1) {
		if ((__raw_readl(REG_PMU_APB_PWR_STATUS_DBG_6) & BIT_PMU_APB_PD_CAMERA_STATE(~0)) == 0)
			cnt ++;
		else
			cnt=0;
		if (cnt == 5)
			return;
	};
}

void enable_global_clocks(void)
{
	wait_mm_wakeup();
	__raw_writel(BIT_AON_APB_MM_EB | BIT_AON_APB_AI_EB |
		     BIT_AON_APB_DPU_VSP_EB, REG_AON_APB_APB_EB0 + 0x1000);
	__raw_writel(BIT_CAMERASYS_GLB_CKG_EN, REG_CAMERASYS_GLB_MM_SYS_EN + 0x1000);
	__raw_writel(BIT_DPU_VSP_APB_CKG_EB, REG_DPU_VSP_APB_APB_EB + 0x1000);
	__raw_writel(BIT_AI_APB_DVFS_EB, REG_AI_APB_APB_EB + 0x1000);
}

static void thm_overheate_en(void)
{
	sci_glb_set(REG_AON_APB_OVERHEAT_CTRL,
		    BIT_AON_APB_THM0_OVERHEAT_ALARM_ADIE_EN |
		    BIT_AON_APB_THM1_OVERHEAT_ALARM_ADIE_EN |
		    BIT_AON_APB_THM2_OVERHEAT_ALARM_ADIE_EN );
}

static void usb_eye_pattern_set(void)
{
	u32 val;
	val = 0x067bd1c0; //the same as kernel set
	writel(val, REG_ANLG_PHY_G0L_ANALOG_USB20_USB20_TRIMMING);
}

static void battery_init(void)
{
	sprdchg_common_cfg();
	sprdchg_bq2560x_init();
	sprdbat_init();
	dprintf(INFO,"CHG init OK!\n");
}

void target_early_init(void) {
	sprd_serial_init();
#ifdef SPRD_TRACE
	trace_init();
#endif
}

void target_init(void) {
#ifndef CONFIG_FPGA
	boardid_init();
	/*config sensorhub*/
#ifdef CONFIG_SENSOR_HUB_UBOOT
	sprd_sensor_init();
#endif
	ADI_init();
	pin_init();
#ifdef CONFIG_USBPINMUX
	usb_uart_inf_config();
#endif
	sprd_i2c_init();
	/*FPGA forbiden*/
	regulator_init();
	pmic_adc_Init();
	/*FPGA forbiden*/
	sprd_gpio_init();
	misc_init();
	sprd_eic_init();
	sprd_led_init();
	/*FPGA forbiden*/
	sprd_pmu_lowpower_init();
	sci_adi_write(REG_ANA_UMP9622_TSX_CTRL14,
		      BIT_ANA_UMP9622_DCXO_26M_REF_BUF2_DIV_MODE_SEL,
		      BIT_ANA_UMP9622_DCXO_26M_REF_BUF2_DIV_MODE_SEL);
	enable_global_clocks();
	thm_overheate_en();
	usb_eye_pattern_set();
	if(sprd_get_pcbversion()) {
		sci_glb_set(REG_AON_APB_APB_EB2, BIT_AON_APB_AON_PWM1_EB);
		__raw_writel(0x10, 0x642e0328);//pwm1 pin func
		__raw_writel(0x82001, 0x642e0728);//pwm1 pin misc
		__raw_writel(0x0, 0x6492004c);//pwm1 clk sel 32k
		__raw_writel(0x7, 0x64034004);
		__raw_writel(0x4, 0x64034008);
		__raw_writel(0x1, 0x64034018);
	}
#else
	sci_glb_set(REG_AON_APB_APB_EB1, BIT_AON_APB_ANA_EB);
#endif

#if defined(CONFIG_BLK_DEV_BOOT)
	if (get_bootdevice() == BOOT_DEVICE_EMMC)
		board_mmc_initialize();
	else if (get_bootdevice() == BOOT_DEVICE_UFS)
		ufs_init();
#elif defined(CONFIG_EMMC_BOOT)
	board_mmc_initialize();
#else
	#error Macros(CONFIG_BLK_DEV_BOOT\CONFIG_EMMC_BOOT) need to be defined in this project
#endif
	board_keypad_init();
	change_cdac_from_miscdata();
	battery_init();
}

#ifdef CONFIG_SENSOR_HUB_LK
int sprd_sensor_bandlist(struct sensor_phypara **sensorlist)
{
	*sensorlist = sensor_board_phylist;
	return sizeof(sensor_board_phylist)/sizeof(sensor_board_phylist[0]);
}
#endif

