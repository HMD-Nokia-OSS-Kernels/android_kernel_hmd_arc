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
#include <asm/arch/common.h>
#include <asm/arch/sprd_reg.h>
#include <adi_hal_internal.h>
#include <sprd_regulator.h>
#include <sprd_boardid.h>

#define DDIE_ID_AA	0x00
#define DDIE_ID_AB	0x01
#define DDIE_ID_MASK	0xFF

static int power_on_voltage_init(void)
{
	uint32_t ddie_id;

	ddie_id = CHIP_REG_GET(REG_AON_APB_AON_VER_ID) & DDIE_ID_MASK;
	if (ddie_id == DDIE_ID_AA)
		regulator_set_voltage("vddsram", 1000);
	else if (ddie_id == DDIE_ID_AB)
		regulator_set_voltage("vddsram", 900);

	regulator_set_voltage("vddrf1v25",1245);

	/*check dcdc_cpu1 spuuly mode */
	if(sprd_get_power_mode() != 1) { /*cpu1 power supply by pmic */
		regulator_set_voltage("vddgpu",1000);
	}

	return 0;
}

int regulator_init(void)
{
	power_on_voltage_init();
	return 0;
}
