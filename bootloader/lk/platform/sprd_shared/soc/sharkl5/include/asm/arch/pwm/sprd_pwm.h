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

#ifndef __ARCH_ARM_ASM_PWM__
#define __ARCH_ARM_ASM_PWM__

#include <asm/arch/sprd_reg.h>
#include <asm/arch/common.h>
#include <pwm.h>

#define SPRD_PWM_NUM		4
//saifixed:?????
struct sprd_pwm{
	unsigned pwm_eb;
	void * apb_base_eb;
	void * pwm_clk_base;
	void * pwm_base;
};

enum{
	EXT_32K = 0,
	EXT_26M,
	RCO_4M,
	RCO_25M,
	TWPLL_48M,
};

struct sprd_pwm sprd_pwm[SPRD_PWM_NUM] = {
	{
		.pwm_eb = BIT_AON_APB_PWM0_EB,
		.apb_base_eb = (void *)REG_AON_APB_APB_EB2,
		.pwm_clk_base = (void *)REG_AON_CLK_CORE_CGM_PWM0_CFG,
		.pwm_base = (void *)CTL_BASE_PWM,
	}, {
		.pwm_eb = BIT_AON_APB_PWM1_EB,
		.apb_base_eb = (void *)REG_AON_APB_APB_EB2,
		.pwm_clk_base = (void *)REG_AON_CLK_CORE_CGM_PWM1_CFG,
		.pwm_base = (void *)CTL_BASE_PWM,
	}, {
		.pwm_eb = BIT_AON_APB_PWM2_EB,
		.apb_base_eb = (void *)REG_AON_APB_APB_EB2,
		.pwm_clk_base = (void *)REG_AON_CLK_CORE_CGM_PWM2_CFG,
		.pwm_base = (void *)CTL_BASE_PWM,
	},{
		.pwm_eb = BIT_AON_APB_PWM3_EB,
		.apb_base_eb = (void *)REG_AON_APB_APB_EB2,
		.pwm_clk_base = (void *)REG_AON_CLK_CORE_CGM_PWM3_CFG,
		.pwm_base = (void *)CTL_BASE_PWM,
	},
};

#endif /*__ARCH_ARM_ASM_PWM__*/
