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
#include <asm/arch/sprd_reg.h>
#include <linux/kernel.h>
#include <sprd_adc.h>
#include <sprd_common.h>
#include <lk/board.h>
#include "adi_hal_internal.h"

#define OPTION_MASK		(BIT(2)|BIT(3)|BIT(4)|BIT(15))
#define M26_DCXO_32KLESS	(BIT(3)|BIT(4)|BIT(15))
#define M26_TCXO_32K		(BIT(3))
#define M26_TSX_32KLESS		(BIT(4)|BIT(15))
#define M52_TCXO_32K		(BIT(2)|BIT(3))
#define NO_USE_32KLESS		(BIT(4))

#define M52_DCXO_CRYSTAL_STYLE	(0x0)
#define M52_TSX_CRYSTAL_STYLE	(BIT(13))
#define M52_TCXO_CRYSTAL_STYLE	(BIT(14))
#define NO_USE_CRYSTAL_STYLE	(BIT(13)|BIT(14))
#define DCXO_CRYSTAL_FLAG	(BIT(13)|BIT(14))

#define K_STYLE			(0x0)
#define KLESS_STYLE		(BIT(6))
#define CLK32KSEL_FLAG		(BIT(6))

#define ADC_CHANNEL_FOR_NV	5
#define BOARD_ID_ADC1		6
#define WCN_GPIO		135
#define BOARD_ID0_GPIO		133

typedef enum k_less {
	K32 = 0,
	K_LESS,
} k_less_t;

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
} crystal_type_t;

struct clock_table {
	u32 raw_data;
	k_less_t k32_less_data;
	crystal_type_t crystal_data;
};

static const int wcn_crystal_type[] = {
	0,/*TCXO*/
	1/*pmic`*/
};

extern int sprd_gpio_request(unsigned offset);
extern int sprd_gpio_direction_input(unsigned offset);
extern int sprd_gpio_get(unsigned offset);

#if defined(CONFIG_ADIE_UMP9622)

static const struct clock_table clock_table[] = {
	{M52_DCXO_CRYSTAL_STYLE, K32, M52_DCXO},
	{M52_TSX_CRYSTAL_STYLE, K_LESS, M52_TSX},
	{M52_TCXO_CRYSTAL_STYLE, K_LESS, M52_TCXO},
	{NO_USE_CRYSTAL_STYLE, K_LESS, NO_USE}
};

static int get_clockid(void)
{
	return ANA_REG_GET(REG_ANA_UMP9622_TSX_CTRL15);
}

static int get_32ksel_id(void)
{
	return ANA_REG_GET(ANA_REG_GLB_CLK32KSEL_CTRL0);
}

static int get_id(void)
{
	int id = 0;
	int value = get_clockid();

	value &= DCXO_CRYSTAL_FLAG;

	for (id=0; id<ARRAY_SIZE(clock_table);id++) {
		if(clock_table[id].raw_data == value)
			return id;
	}

	return -1;
}

static int get_32k_id(void)
{
	int id = -1;
	int value = get_32ksel_id();

	value &= CLK32KSEL_FLAG;

	if(value == K_STYLE)
		id = K32;
	else if(value == KLESS_STYLE)
		id = K_LESS;

	return id;
}
#else
static const struct clock_table clock_table[] = {
	{M26_TCXO_32K, K32, M26_TCXO},
	{M26_TSX_32KLESS, K_LESS, M26_TSX},
	{M26_DCXO_32KLESS, K_LESS, M26_DCXO},
	{M52_TCXO_32K, K32, M52_TCXO},
	{NO_USE_32KLESS, K_LESS, NO_USE}
};

static int get_clockid(void)
{
	return ANA_REG_GET(ANA_REG_GLB_CLK32KLESS_CTRL0);
}

static int clk_crystal_data(void)
{
	int data = get_clockid();

	return data & OPTION_MASK;
}

static int get_id(void)
{
	int id = 0;
	int value = clk_crystal_data();

	for (id=0; id<ARRAY_SIZE(clock_table); id++) {
		if(clock_table[id].raw_data == value)
			return id;
	}

	errorf("option value is error\n");
	return -1;
}
#endif

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
	return gpio_state(WCN_GPIO);
}

/* Return wcn crystal type */
int sprd_get_wcn_crystal(void)
{
	int id = get_wcnid();

	return wcn_crystal_type[id];
}

struct adc2deltanv {
	u16 low_vol;
	u16 high_vol;
	int nv;
};

static const struct adc2deltanv adc2deltanv_table[] = {
	{9, 31, -1},		/* reserved */
	{180, 220, 4},		/* hw_ver04.nv */
	{275, 325, 3},		/* hw_ver03.nv */
	{446, 514, -1},		/* reserved */
	{560, 640, -1},		/* reserved */
	{731, 829, 2},		/* hw_ver02.nv */
	{883, 997, 0},		/* hw_ver00.nv */
	{1054, 1186, 1},	/* hw_ver01.nv */
};

static int get_adc_value_board(int channel)
{
	int adc_value = 0;
	int vol= 0;
	int level= 0;

	adc_value = pmic_adc_get_value_by_isen(channel,0,15,2000);
	vol = sprd_chan_small_adc_to_vol(channel, 0 , 0 ,adc_value);
	debugf("The voltage is = %d\n",vol);

	for (level=0; level<ARRAY_SIZE(adc2deltanv_table); level++) {
		if (vol >= adc2deltanv_table[level].low_vol
		&& vol <= adc2deltanv_table[level].high_vol) {
			debugf("level = %d\n",level);
			return level;
		}
	}

	errorf("vol is out of reasonable ranger \n");
	return -1;
}

/* Return rf band info */
int sprd_get_bandinfo(void)
{
	int adc_val = 0;
	int rfboard_id = 0;

	adc_val = get_adc_value_board(ADC_CHANNEL_FOR_NV);
	if(adc_val < 0)
		rfboard_id = adc_val;
	else
		rfboard_id = adc2deltanv_table[adc_val].nv;

	debugf("rfboard.id = %d\n", rfboard_id);

	return rfboard_id;
}

/* Return RF band type*/
int sprd_get_rf_band_type(void)
{
	int rf_band_type = 0;

	rf_band_type = get_adc_value_board(ADC_CHANNEL_FOR_NV);

	debugf("RF band type = %d\n", rf_band_type);

	return rf_band_type;
}

/* Return boardid info for wcn assert */
int sprd_get_pcbversion(void)
{
	int gpio_val = 0;

	gpio_val |= gpio_state(BOARD_ID0_GPIO);
	debugf("pcbversion = %d\n", gpio_val);

	return gpio_val;
}

/* check wifi only thought adc/gpio */
int sprd_get_wifi_mode(void)
{
/*modified according to the actual hardware*/
#ifdef CONFIG_SSMH_WIFI_MODE
	int boardid = 0;
	boardid = get_adc_value_board(ADC_CHANNEL_FOR_NV);
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
/*modified according to the actual hardware*/
#ifdef CONFIG_SSMH_DATA_MODE
	int boardid = 0;
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
int sprd_get_sim(void)
{
#ifdef CONFIG_SSMH_SIM_MODE
	int boardid = 0;
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

/* check gpio/adc return “UOB”or “N6L -1” */
int sprd_get_uob_boardid(void)
{
	int uobboard_id = 0;

	uobboard_id = get_adc_value_board(BOARD_ID_ADC1);

	debugf("uobboard.id = %d\n", uobboard_id);

	return uobboard_id;
}
