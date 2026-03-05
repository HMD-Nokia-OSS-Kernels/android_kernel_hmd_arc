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

#ifndef __ARCH_ARM_ASM_HW_I2C__
#define __ARCH_ARM_ASM_HW_I2C__

#include <asm/arch/sprd_reg.h>

#define USED_CHANNEL_NUM	2

struct hw_i2c_channel {
	unsigned channel_num;
	unsigned addr;
};

struct sprd_hw_i2c {
	void *apb_base;
	unsigned apb_eb;
	void *base;
	int num;
	struct hw_i2c_channel channel[USED_CHANNEL_NUM];
};

#define DVFS_ADDR(DA, RA)		(((DA) << 8 | (RA)) << 2)

#define SPRD_I2C_NUM		1

#define	DVFS_TX_ADDR		DVFS_ADDR(CONFIG_DVFS_ADDR, CONFIG_DVFS_TX_REG_ADDR)
#define	DVFS_RX_ADDR		DVFS_ADDR(CONFIG_DVFS_ADDR, CONFIG_DVFS_RX_REG_ADDR)
#define	DVFS_RX_CHNL		5
#define	DVFS_TX_CHNL		6

struct sprd_hw_i2c sprd_hw_i2c[SPRD_I2C_NUM] = {
	{
		.apb_base = (void *)REG_AON_APB_MPLL_CFG1,
		.apb_eb = BIT_AON_APB_I2C_EB,
		.base = (void *)SPRD_I2C0_PHYS,
		.num = USED_CHANNEL_NUM,
		.channel[0] = {
			.channel_num = DVFS_TX_CHNL,
			.addr= DVFS_TX_ADDR,
		},
		.channel[1] = {
			.channel_num = DVFS_RX_CHNL,
			.addr= DVFS_RX_ADDR,
		},
	},
};

#endif /*__ARCH_ARM_ASM_HW_I2C__*/
