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
#include <sprd_adc.h>
#include "adi_hal_internal.h"
#include <sprd_common.h>
#include <asm/arch/sprd_reg.h>

#define M26_DCXO_32KLESS 0x8018
#define M26_TCXO_32K 0x8
#define M26_TSX_32KLESS 0x8010
#define M52_TCXO_32K 0xc
#define NO_USE_32KLESS 0x10

#define K_STYLE 0x0
#define KLESS_STYLE 0x40
#define CLK32KSEL_FLAG 0x40

#define option3_check  BIT(2)
#define option4_check  BIT(3)
#define option5_check  BIT(15)
#define option6_check  BIT(4)

#define ADC_CHANNEL_FOR_NV    3
#define WCN_GPIO	173
#define POWER_FROM_PMIC 0x0
#define POWER_FROM_EXTERN 0x4
#define POWER_FLAG 0x6

#define HB_GPIO	130
#define MB_GPIO	131
#define LB_GPIO	132


typedef enum k_less {
	K32 = 0,
	K_LESS,
}k_less_t;

typedef enum crystal_type {
	VCTCXO = 0,
	DCXO,
	TSX,
	TCXO,
	M26_TCXO,
	M52_TCXO,
	M26_TSX,
	M52_TSX,
	M26_DCXO,
	M52_DCXO,
	NO_USE
}crystal_type_t;

struct clock_table {
	k_less_t k32_less_data;
	crystal_type_t crystal_data;
};

static const struct clock_table clock_table[] = {
	{K32, M26_TCXO},
	{K_LESS, M26_TSX},
	{K_LESS, M26_DCXO},
	{K32, M52_TCXO},
	{K_LESS, NO_USE}
};

static const int wcn_crystal_type[] = {
	0,/*TCXO*/
	1/*TSX*/
};

extern int sprd_gpio_request(unsigned offset);
extern int sprd_gpio_direction_input(unsigned offset);
extern int sprd_gpio_get(unsigned offset);

static int get_clockid(void)
{
	int reg_val = 0;

	reg_val = ANA_REG_GET(ANA_REG_GLB_CLK32KLESS_CTRL0);

	return reg_val;
}

static int clk_crystal_data(void)
{
	int clk_crystal = 0;
	int option3, option4, option5, option6;
	int data = get_clockid();

	option3 = data & option3_check;
	option4 = data & option4_check;
	option5 = data & option5_check;
	option6 = data & option6_check;

	clk_crystal = option3 + option4 + option5 + option6;

	return clk_crystal;
}

static int get_id(void)
{
	int id = -1;
	int value = clk_crystal_data();

	if(value != M26_TCXO_32K && value != M26_TSX_32KLESS && \
	   value != M26_DCXO_32KLESS && value != M52_TCXO_32K && \
	   value != NO_USE_32KLESS){
		errorf("option value is error\n");
		return -1;
	}

	if(value == M26_TCXO_32K)
		id = 0;
	else if(value == M26_TSX_32KLESS)
		id = 1;
	else if(value == M26_DCXO_32KLESS)
		id = 2;
	else if(value == M52_TCXO_32K)
		id = 3;
	else if(value == NO_USE_32KLESS)
		id = 4;

	return id;

}

/* Return crystal type */
int sprd_get_crystal(void)
{
	int value = get_id();

	if (value < 0){
		errorf("get err id\n");
		return -1;
	}

	return clock_table[value].crystal_data;
}

/* Return 32k less or not */
int sprd_get_32k(void)
{
/* sw auto-adaption */
#if defined(CONFIG_ADIE_UMP9622)
	int value = get_32k_id();
#else
	int value = get_id();
#endif
	if (value >= 0) {
		return clock_table[value].k32_less_data;
	} else {
		errorf("Failed to get_id for value.\n");
		return -1;
	}
}

/* Return board id */
int sprd_get_boardid(void)
{
	return get_clockid();
}

static unsigned int gpio_state(unsigned int GPIO_NUM)
{
	int value = 0 ;

	sprd_gpio_request(GPIO_NUM);
	sprd_gpio_direction_input(GPIO_NUM);
	value = sprd_gpio_get(GPIO_NUM);

	return value > 0;
}

static int get_wcnid(void)
{
	unsigned int gpio_val = 0;

	gpio_val |= gpio_state(WCN_GPIO);

	return gpio_val;
}

/* Return wcn crystal type */
int sprd_get_wcn_crystal(void)
{
	int id = get_wcnid();

	return wcn_crystal_type[id];
}

static const int gpio2deltanv_table[] = {
	-1, /* reserved */
	-1, /* reserved */
	-1, /* reserved */
	2, /* hw_ver02.nv */
	-1, /* reserved */
	1, /* hw_ver01.nv */
	-1, /* reserved.nv */
	0 /* hw_ver00.nv */
};

/* Return rf band info */
int sprd_get_bandinfo(void)
{
	unsigned int gpio_val = 0;
	unsigned int shift_bit = 0;

	gpio_val = gpio_state(LB_GPIO);
	shift_bit++;

	gpio_val |= gpio_state(MB_GPIO) << shift_bit;
	shift_bit++;

	gpio_val |= gpio_state(HB_GPIO) << shift_bit;

	debugf("board.id = %d\n", gpio_val);

	return gpio2deltanv_table[gpio_val];
}

/* check power mode thought option/gpio */
int sprd_get_power_mode(void)
{
	enum power_mode {
          PMIC_POWER = 0,
          EXTERN_POWER = 1,
        };
	int reg_val;
	int id = -1;

	reg_val = ANA_REG_GET( ANA_REG_GLB_CLK_26M_SEL);

	reg_val &= POWER_FLAG;

	if(reg_val == POWER_FROM_PMIC) {
		id = PMIC_POWER;
        }
	else if(reg_val == POWER_FROM_EXTERN) {
		id = EXTERN_POWER;
        }

	return id;
}
