/*
* Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
* Licensed under the Unisoc General Software License, version 1.0 (the License);
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
* Software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* See the Unisoc General Software License, version 1.0 for more details.
*/

#ifndef __ARCH_ARM_ASM_I2C__
#define __ARCH_ARM_ASM_I2C__

#include <asm/arch/sprd_reg.h>
#include <asm/arch/common.h>
#include <i2c.h>

#define SPRD_I2C_NUM		3

struct sprd_i2c I2C_BUS[SPRD_I2C_NUM] = {
	{
		.bus_name = "i2c0",
		.apb_base =  (void *)REG_AP_APB_APB_EB,
		.apb_eb = BIT_AP_APB_I2C0_EB,
		.base = (void *)SPRD_I2C0_PHYS,
//		.clk = CLK_I2C0,
		.freq = 100*1000,
		.src_clk = 26000000,
	}, {
		.bus_name = "i2c1",
		.apb_base =  (void *)REG_AP_APB_APB_EB,
		.apb_eb = BIT_AP_APB_I2C1_EB,
		.base = (void *)SPRD_I2C1_PHYS,
///		.clk = CLK_I2C1,
		.freq = 100*1000,
		.src_clk = 26000000,
	}, {
		.bus_name = "i2c2",
		.apb_base = (void *) REG_AP_APB_APB_EB,
		.apb_eb = BIT_AP_APB_I2C2_EB,
		.base = (void *)SPRD_I2C2_PHYS,
///		.clk = CLK_I2C2,
		.freq = 100*1000,
		.src_clk = 26000000,
	}, 
};

#endif /*__ARCH_ARM_ASM_I2C__*/
