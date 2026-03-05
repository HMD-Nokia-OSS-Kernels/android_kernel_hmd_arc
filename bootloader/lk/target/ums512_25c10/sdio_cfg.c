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
extern uint32_t (*sprd_sdhci_get_delay)(uint32_t device_type) = NULL;
const struct sdio_base_info sdio_ctrl_info[2] = {
{
	EMMC,
	SPRD_EMMC_BASE,
#ifdef CONFIG_FPGA
	NULL,
	NULL,
	36000000,
	100000,
#else
	REG_AP_CLK_CORE_CGM_EMMC_2X_CFG,
	BIT_2,
	/* selsect 2x clock source, baseclk = freq / 2 */
	195000000,
	400000,
#endif
	REG_AP_APB_APB_EB,
	BIT_AP_APB_EMMC_EB,
	REG_AP_APB_APB_RST,
	BIT_AP_APB_EMMC_SOFT_RST,
	REG_AON_APB_CGM_CLK_TOP_REG1,
	BIT_AON_APB_CGM_EMMC_2X_EN|BIT_AON_APB_CGM_EMMC_1X_EN,

	"vddgen0",
	"vddemmccore",
	0x30303060,
	0xd4d4de7f,
},
{
	SD,
	SPRD_SDIO0_BASE,
#ifdef CONFIG_FPGA
	NULL,
	NULL,
	36000000,
	100000,
#else
	REG_AP_CLK_CORE_CGM_SDIO0_2X_CFG,
	BIT_0 | BIT_1,
	/* selsect 2x clock source, baseclk = freq / 2 */
	195000000,
	400000,
#endif

	REG_AP_APB_APB_EB,
	BIT_AP_APB_SDIO0_EB,
	REG_AP_APB_APB_RST,
	BIT_AP_APB_SDIO0_SOFT_RST,
	REG_AON_APB_CGM_CLK_TOP_REG1,
	BIT_AON_APB_CGM_SDIO0_2X_EN|BIT_AON_APB_CGM_SDIO0_1X_EN ,

	"vddsdio",
	"vddsdcore",
	0x9a9a1a7f,
	0x7272737f,
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

