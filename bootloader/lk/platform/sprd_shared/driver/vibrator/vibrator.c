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

#include <config.h>
#include <linux/types.h>
#include "adi_hal_internal.h"
#include <asm/arch/sprd_reg.h>
#include <lk/debug.h>
#include <lk/board.h>

#define ANA_VIBRATOR_CTRL0_U      (ANA_REG_GLB_VIBR_CTRL0)

#ifdef BIT_SLP_LDO_VDDVIB_PD_EN
#define SLP_LDOVIBR_PD_EN_U	BIT_SLP_LDO_VDDVIB_PD_EN
#else
#define SLP_LDOVIBR_PD_EN_U	(0x1 << 9)
#endif

#ifdef BIT_LDO_VDDVIB_PD
#define LDO_VIBR_PD_U		BIT_LDO_VDDVIB_PD
#else
#define LDO_VIBR_PD_U		(0x1 << 8)
#endif

#ifdef ZCFG_VIBRATOR_VOLTAGE
#define LDO_VIBR_V_U		ZCFG_VIBRATOR_VOLTAGE
#else
#define LDO_VIBR_V_U		0xB4
#endif

#define LDO_VIBR_V_MASK		(0xff)

#ifdef CONFIG_NONE_VIBRATOR

void set_vibrator(int on) {}
void vibrator_hw_init(void) {}

#else

void set_vibrator(int on)
{
#ifdef CONFIG_SC27XX_VDDVIB_VSD2
	dprintf(ALWAYS,"set_vibrator on!\n");
	on = 1;
#endif

	if (on) {
		ANA_REG_AND(ANA_VIBRATOR_CTRL0_U, ~LDO_VIBR_PD_U);
		ANA_REG_AND(ANA_VIBRATOR_CTRL0_U, ~SLP_LDOVIBR_PD_EN_U);
	} else {
		ANA_REG_MSK_OR(ANA_VIBRATOR_CTRL0_U, LDO_VIBR_PD_U, LDO_VIBR_PD_U);
		ANA_REG_MSK_OR(ANA_VIBRATOR_CTRL0_U, SLP_LDOVIBR_PD_EN_U, SLP_LDOVIBR_PD_EN_U);
	}
}

void vibrator_hw_init(void)
{
#if defined(CONFIG_ADIE_SC2723) || defined(CONFIG_ADIE_SC2731)
	ANA_REG_OR(ANA_REG_GLB_RTC_CLK_EN, BIT_RTC_VIBR_EN);
#endif

	ANA_REG_MSK_OR(ANA_VIBRATOR_CTRL0_U,LDO_VIBR_V_U,LDO_VIBR_V_MASK);
	dprintf(ALWAYS,"LDO_VIBR_V_U = 0x%x\n",LDO_VIBR_V_U);
}

#endif
