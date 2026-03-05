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
#include <lk/board.h>
#define ADC_CHANNEL_FOR_NV    3
#define M26_DCXO_32KLESS 0x8018
#define M26_TCXO_32K 0x8
#define M26_TSX_32KLESS 0x8010
#define M52_TCXO_32K 0xc
#define NO_USE_32KLESS 0x10

#define M52_DCXO_CRYSTAL_STYLE 0x0
#define M52_TSX_CRYSTAL_STYLE 0x2000
#define M52_TCXO_CRYSTAL_STYLE 0x4000
#define NO_USE_CRYSTAL_STYLE 0x6000
#define DCXO_CRYSTAL_FLAG 0x6000

#define K_STYLE 0x0
#define KLESS_STYLE 0x40
#define CLK32KSEL_FLAG 0x40

#define option3_check  BIT(2)
#define option4_check  BIT(3)
#define option5_check  BIT(15)
#define option6_check  BIT(4)

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

static int get_clockid(void)
{
	int reg_val = 0;
	#if defined(ZCFG_TSX_32KLESS)
	gpio_val =0;
	#else
	reg_val = ANA_REG_GET(ANA_REG_GLB_CLK32KLESS_CTRL0);
	#error "At present, we only support TSX solution"
	#endif

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
#ifdef PLATFORM_ORCA
	return sprd_get_versionid();
#endif

	return get_clockid();
}

static const int adc2deltanv_table[] = {
	7, /* reserved */
	6, /* reserved */
	5, /* reserved */
	4, /* reserved */
	3, /* reserved */
	2, /* hw_ver02.nv */
	1, /* hw_ver01.nv */
	0 /* hw_ver00.nv */
};

static int get_adc_value_board(int channel)
{
	int adc_value = 0,vol= 0 ,level= 0;

	adc_value = pmic_adc_get_value_by_isen(channel,0,15,2000);
	vol = sprd_chan_small_adc_to_vol(channel, 0 , 0 ,adc_value);
	debugf("The voltage is = %d\n",vol);

	if (vol <= 0 || vol >= 1200) {
		errorf("vol is out of ranger [0~1200]\n");
		return -1;
	}

	if (vol >= 9 && vol <= 31)
		level = 0;
	if (vol >= 180 && vol <= 220)
		level = 1;
	if (vol >= 275 && vol <= 325)
		level = 2;
	if (vol >= 446 && vol <= 514)
		level = 3;
	if (vol >= 560 && vol <= 640)
		level = 4;
	if (vol >= 731 && vol <= 829)
		level = 5;
	if (vol >= 883 && vol <= 997)
		level = 6;
	if (vol >= 1054 && vol <= 1186)
		level = 7;

	debugf("level = %d\n",level);

	if(level < 0 || level > 7) {
		errorf("Adc value of rf band if is wrong, the rfboard.id will be set to -1\n");
		return -1;
	}

	return adc2deltanv_table[level];
}

/* Return rf band info */
int sprd_get_bandinfo(void)
{
	unsigned int adc_val = 0;

	adc_val = get_adc_value_board(ADC_CHANNEL_FOR_NV);
	debugf("rfboard.id = %d\n", adc_val);

	return adc_val;
}


/* check wifi only thought adc/gpio */
int sprd_get_wifi_mode(void)
{
	int boardid = 0;
/*modified according to the actual hardware*/
#ifdef CONFIG_SSMH_WIFI_MODE
	boardid = get_adc_value_board();
	if (boardid == 0) {
		return 1;
	} else {
		return 0;
	}
#else
/* fixed to Wifionly */
#ifdef PRODUCT_WIFI_ONLY
	return 1;
#else
/* default setting */
	return 0;
#endif
#endif
}

/* check data only thought adc/gpio */
int sprd_get_data_mode(void)
{
	int boardid = 0;
/*modified according to the actual hardware*/
#ifdef CONFIG_SSMH_DATA_MODE
	boardid = get_adc_value_board();
	if (boardid == 1) {
		return 1;
	} else {
		return 0;
	}
#else
/* fixed to dataonly */
#ifdef PRODUCT_DATA_ONLY
	return 1;
#else
/* default setting */
	return 0;
#endif
#endif
}

/* check gpio/adc return “SINGLESIM”or “DUALSIM” */
char sprd_get_sim(void)
{
	int boardid = 0;
#ifdef CONFIG_SSMH_SIM_MODE
	boardid = get_adc_value_board();// modified according to the actual hardware
	if (boardid == 3) {
		return 1;
	} else {
		return 0;
	}
#else
/* fixed to SINGLESIM */
#ifdef PRODUCT_SINGLE_SIM
	return 1;
#else
/* default setting */
	return 0;
#endif
#endif
}

