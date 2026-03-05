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

#ifndef _SDIO_REG_V0_H_
#define _SDIO_REG_V0_H_

#include <sprd_regulator.h>

#define EMMC	0
#define SD	1

struct sdio_base_info {
	uint32_t  dev_type;
	uint32_t  regbase;
	uint32_t  baseclk_reg;
	uint32_t  baseclk_mask;

	uint32_t  maxclk;
	uint32_t  minclk;

	uint32_t  ahb_enable_reg;
	uint32_t  ahb_enable_bit;
	uint32_t  ahb_reset_reg;
	uint32_t  ahb_reset_bit;
	uint32_t aon_clk_reg;
	uint32_t aon_clk_bit;

	const char *ldo_io;
	const char *ldo_core;
	/*
	 * mmc_delay = (wr_dly) | ((cmd_dly) << 8) | ((posrd_dly) << 16) |
	 * ((negrd_dly) << 24)
	 */
	uint32_t hs_dly;
	uint32_t hs200_dly;
};

struct sdio_base_info *get_sdcontrol_info(uint32_t device_type);

#endif
