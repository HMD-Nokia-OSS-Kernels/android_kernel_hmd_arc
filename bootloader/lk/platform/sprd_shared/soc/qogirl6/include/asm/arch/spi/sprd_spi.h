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

#ifndef __ARCH_ARM_ASM_QOGIRL6_SPI__
#define __ARCH_ARM_ASM_QOGIRL6_SPI__

#include <common.h>
#include <asm/arch/sprd_reg.h>
#include <asm/io.h>

struct sprd_spi{
	void * spi_eb;
	unsigned apb_base_eb;
	unsigned spi_rst;
	unsigned apb_base_rst;
	unsigned spi_clk_base;
	void * spi_base;
};

#define SPRD_SPI_NUM		4

struct sprd_spi sprd_spi[SPRD_SPI_NUM] = {
	{
		.spi_eb = (void *)BIT_AP_APB_SPI0_EB,
		.apb_base_eb = REG_AP_APB_APB_EB,
		.spi_rst = BIT_AP_APB_SPI0_SOFT_RST,
		.apb_base_rst =REG_AP_APB_APB_RST,
		.spi_clk_base = REG_AP_CLK_CGM_SPI0_CFG,
		.spi_base = (void *)SPRD_SPI0_PHYS
	}, {
		.spi_eb = (void *)BIT_AP_APB_SPI1_EB,
		.apb_base_eb = REG_AP_APB_APB_EB,
		.spi_rst = BIT_AP_APB_SPI1_SOFT_RST,
		.apb_base_rst =REG_AP_APB_APB_RST,
		.spi_clk_base = REG_AP_CLK_CGM_SPI1_CFG,
		.spi_base = (void *)SPRD_SPI1_PHYS
	}, {
		.spi_eb = (void *)BIT_AP_APB_SPI2_EB,
		.apb_base_eb = REG_AP_APB_APB_EB,
		.spi_rst = BIT_AP_APB_SPI2_SOFT_RST,
		.apb_base_rst =REG_AP_APB_APB_RST,
		.spi_clk_base = REG_AP_CLK_CGM_SPI2_CFG,
		.spi_base = (void *)SPRD_SPI2_PHYS
	}, {
		.spi_eb = (void *)BIT_AP_APB_SPI3_EB,
		.apb_base_eb = REG_AP_APB_APB_EB,
		.spi_rst = BIT_AP_APB_SPI3_SOFT_RST,
		.apb_base_rst =REG_AP_APB_APB_RST,
		.spi_clk_base = REG_AP_CLK_CGM_SPI3_CFG,
		.spi_base = (void *)SPRD_SPI3_PHYS
	},
};


#endif /*__ARCH_ARM_ASM_QOGIRL6_SPI__*/
