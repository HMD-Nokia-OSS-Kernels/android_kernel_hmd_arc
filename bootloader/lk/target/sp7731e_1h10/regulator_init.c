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

#include <asm/arch/common.h>
#include <asm/arch/sprd_reg.h>
#include <adi_hal_internal.h>
#include <sprd_regulator.h>
//#include <asm/io.h>
#include <secureboot/sec_efuse_pike2.h>

static int power_on_voltage_init(void)
{
	unsigned int chip_version;

	chip_version = __raw_readl(REG_AON_APB_AON_VER_ID) & 0xFF;

	if (!chip_version) {
		regulator_set_voltage("vddgen", 1950);
		regulator_set_voltage("vddrf18a", 1850);
	} else {
		regulator_set_voltage("vddgen", 1875);
		regulator_set_voltage("vddrf18a", 1800);
	}

	regulator_set_voltage("avdd18", 1800);
	regulator_set_voltage("vddrf18b", 1800);

	/*only scaling voltage,didn't power on*/
	regulator_set_voltage("vddcamio", 1800);
	return 0;
}

int regulator_init(void)
{
	power_on_voltage_init();
	return 0;
}
