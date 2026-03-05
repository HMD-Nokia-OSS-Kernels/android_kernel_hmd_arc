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
	REG_AON_CLK_CORE_CGM_EMMC_2X_CFG,
	BIT_2, /* div = 3,source:390m_rpll */
	/* selsect 2x clock source, baseclk = freq / 2 */
	195000000,
	400000,
#endif
	REG_AP_AHB_AHB_EB,
	BIT_11,
	REG_AP_AHB_AHB_RST,
	BIT_14,
	0,
	0,
	"vddgen",
	"vddemmccore",
	0x35353500,
	0xcdcdcd7f,
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
	REG_AON_CLK_CORE_CGM_SDIO0_2X_CFG,
	BIT_0 | BIT_1, /* div = 3,source:390m_rpll */
	/* selsect 2x clock source, baseclk = freq / 2 */
	195000000,
	400000,
#endif
	REG_AP_AHB_AHB_EB,
	BIT_8,
	REG_AP_AHB_AHB_RST,
	BIT_11,
	0,
	0,
	"vddsdio",
	"vddsdcore",
	0x9b9b9c7f,
	0xa5a5a57f,
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


