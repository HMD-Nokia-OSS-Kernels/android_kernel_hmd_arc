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
#include <asm/arch/pinmap.h>
#include <eic.h>
#include <gpio.h>
#include <i2c.h>
#include <serial_sprd.h>
#include <sprd_adc.h>
#include <asm/arch/common.h>
#include <asm/arch/sprd_reg.h>
#include <sprd_battery.h>
#ifdef CONFIG_SENSOR_HUB_LK
#include <sprd_sensor.h>
#include "sensor_board_info.h"
#endif
#include <sprd_chg_helper.h>
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
extern void usb_uart_inf_config(void);

void wait_mm_wakeup(void)
{
	uint32_t cnt = 0;
	while(1) {
		if ((__raw_readl(REG_PMU_APB_PWR_STATUS_DBG_6) & BIT_PMU_APB_PD_MM_STATE(~0)) == 0) {
			cnt ++;
		} else {
			cnt=0;
		}

		if(cnt == 5) {
			return;
		}
	};
}

void enable_global_clocks(void)
{
	wait_mm_wakeup();
	writel(BIT_AON_APB_GPU_EB | BIT_AON_APB_MM_EB,
			REG_AON_APB_APB_EB0 + 0x1000);
	writel(BIT_MM_AHB_CKG_EB, REG_MM_AHB_AHB_EB + 0x1000);
}

static void battery_init(void)
{
	sprdchg_common_cfg();
	sprdchg_bq2560x_init();
	if (sprdchg_ic_ops_get()){
		dprintf(INFO,"sprdchg_bq2560x_ops find\n");
		goto bat_init;
	}

	sprdchg_sgm41511_init();
	if (sprdchg_ic_ops_get()){
		dprintf(INFO,"sprdchg_sgm41511_ops find\n");
		goto bat_init;
	}

	sprd_i2c_init();
	sprdchg_rt9471_init();
	if (sprdchg_ic_ops_get()){
		dprintf(INFO,"sprdchg_rt9471_ops find\n");
		goto bat_init;
	}
bat_init:
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
	FTL_Savepoint_Private(PHASE_POWER_DONE);
	enable_global_clocks();
	FTL_Savepoint_Private(PHASE_CLK_DONE);
#endif /* endof CONFIG_FPGA */
	board_mmc_initialize();
	FTL_Savepoint_Private(PHASE_BLOCK_DONE);

	board_keypad_init();

	battery_init();
	FTL_Savepoint_Private(PHASE_BATTERY_DONE);
}

#ifdef CONFIG_SENSOR_HUB_LK
int sprd_sensor_bandlist(struct sensor_phypara **sensorlist)
{
	*sensorlist = sensor_board_phylist;
	return sizeof(sensor_board_phylist)/sizeof(sensor_board_phylist[0]);
}
#endif

