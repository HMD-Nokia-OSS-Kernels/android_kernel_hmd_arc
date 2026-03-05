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

static int power_on_voltage_init(void)
{
	regulator_set_voltage("vddrf1v25",1250);

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
