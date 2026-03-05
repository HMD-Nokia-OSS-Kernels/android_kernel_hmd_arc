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

#include <asm/arch/sprd_reg.h>
#include <asm/arch/sdio_cfg.h>
#include <asm/arch/sdio_reg.h>
#include <asm/arch/common.h>

uint32_t (*sprd_sdhci_get_delay)(uint32_t device_type) = NULL;
/*
 * TODO：
 * Now many macros(such as: SPRD_EMMC_BASE, REG_AP_CLK_EMMC_CFG)
 * have not defined in spreadtrum platform, so we use a immediate
 * value instead of it.
 * These immediate values should be removed if the corresponding
 * macros are defined.
 */
const struct sdio_base_info sdio_ctrl_info[2] = {
{
		EMMC,
		/* SPRD_EMMC_BASE, */
		0X20600000,
#ifdef CONFIG_FPGA
		NULL,
		NULL,
		36000000,
		100000,
#else
		REG_AON_CLK_EMMC_2X_CFG,
		BIT_0|BIT_1,
		192000000,
		400000,
#endif
		REG_AP_AHB_AHB_EB,
		BIT_AP_AHB_EMMC_EB,
		REG_AP_AHB_AHB_RST,
		BIT_AP_AHB_EMMC_SOFT_RST,
		REG_AON_APB_CLK_EB0,
		BIT_0|BIT_1,

		"vddgen",
		"vddemmccore",
		0x7f7f947f,
		0xcdcdcd7f,
	},
	{
		SD,
		0X20300000,
#ifdef CONFIG_FPGA
		NULL,
		NULL,
		36000000,
		100000,
#else
		REG_AON_CLK_SDIO0_2X_CFG,
		BIT_0|BIT_1,
		192000000,
		400000,
#endif

		REG_AP_AHB_AHB_EB,
		BIT_AP_AHB_SDIO0_EB,
		REG_AP_AHB_AHB_RST,
		BIT_AP_AHB_SDIO0_SOFT_RST,
		REG_AON_APB_CLK_EB0,
		BIT_2|BIT_3,

		"vddsdio",
		"vddsdcore",
		0x4646487f,
		0xb4b4b07f,
	}
};

uint32_t get_delay_value(uint32_t device_type)
{
	return sdio_ctrl_info[device_type].hs_dly;
}
struct sdio_base_info *get_sdcontrol_info(uint32_t device_type)
{
	sprd_sdhci_get_delay = get_delay_value;
	return  &sdio_ctrl_info[device_type];
}

