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
#include <asm/arch/pinmap.h>

#define PULL_UP_20K_BIT BIT_7
#define PULL_UP_4K7_BIT BIT_12
#define PULL_UP_1K8_BIT BIT_7 | BIT_12
#define PULL_UP_BIT_MARKS BIT_7 | BIT_12
#define PULL_DOWN_50K_BIT BIT_6
#define GPIO_FUNC_BIT BIT_4 | BIT_5

int gpio_value_table[2] = {0};
int pinaf_value_table_original[2] = {0};
int pinds_value_table_original[2] = {0};

int gpio_table[2] = {180, 177};
int pinaf_addr_table[2] = {
	REG_PIN_SPI2_CSN, REG_PIN_SPI2_CLK
};
int pinds_addr_table[2] = {
	REG_MISC_PIN_SPI2_CSN, REG_MISC_PIN_SPI2_CLK
};

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

#define ADC_CHANNEL_FOR_NV    5
#define WCN_GPIO	135
#define BOARD_ID0_GPIO	133

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
	{K32, M52_DCXO},
	{K_LESS, M52_TSX},
	{K_LESS, M52_TCXO},
	{K_LESS, NO_USE}
};

static int get_clockid(void)
{
	int reg_val = 0;

	reg_val = ANA_REG_GET(REG_ANA_UMP9622_TSX_CTRL15);

	return reg_val;
}

static int get_32ksel_id(void)
{
	int reg_val = 0;

	reg_val = ANA_REG_GET(ANA_REG_GLB_CLK32KSEL_CTRL0);

	return reg_val;
}

static int get_id(void)
{
	int id = -1;
	int value = get_clockid();
	value &= DCXO_CRYSTAL_FLAG;

	if(value == M52_DCXO_CRYSTAL_STYLE)
		id = 0;
	else if(value == M52_TSX_CRYSTAL_STYLE)
		id = 1;
	else if(value == M52_TCXO_CRYSTAL_STYLE)
		id = 2;
	else if(value == NO_USE_CRYSTAL_STYLE)
		id = 3;

	return id;

}

static int get_32k_id(void)
{
	int id = -1;
	int value = get_32ksel_id();

	value &= CLK32KSEL_FLAG;

	if(value == K_STYLE)
		id = 0;
	else if(value == KLESS_STYLE)
		id = 1;

	return id;
}

#else
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

	int id;
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

static const int adc2deltanv_table[] = {
	-1, /* reserved */
	-1, /* reserved */
	-1, /* reserved */
	-1, /* reserved */
	-1, /* reserved */
	2, /* hw_ver02.nv */
	0, /* hw_ver00.nv */
	1 /* hw_ver01.nv */
};

static int get_adc_value_board(int channel)
{
	int adc_value = 0;
	int vol= 0;
	int level= 0;

	adc_value = pmic_adc_get_value_by_isen(channel,0,15,2000);
	vol = sprd_chan_small_adc_to_vol(channel, 0 , 0 ,adc_value);
	debugf("The voltage is = %d\n",vol);

	/*different rf band correspond to different adc voltages. The mapping table is provided by hardware engineers.*/
	if (vol <= 0 || vol >= 1200) {
		errorf("vol is out of ranger [0~1200]\n");
		return -1;
	}
	else if (vol >= 9 && vol <= 31) {
		level = 0;
	}
	else if (vol >= 180 && vol <= 220) {
		level = 1;
	}
	else if (vol >= 275 && vol <= 325) {
		level = 2;
	}
	else if (vol >= 446 && vol <= 514) {
		level = 3;
	}
	else if (vol >= 560 && vol <= 640) {
		level = 4;
	}
	else if (vol >= 731 && vol <= 829) {
		level = 5;
	}
	else if (vol >= 883 && vol <= 997) {
		level = 6;
	}
	else if (vol >= 1054 && vol <= 1186) {
		level = 7;
	}
	else {
		errorf("vol is out of reasonable ranger \n");
		return -1;
	}


	debugf("level = %d\n",level);

	if(level < 0 || level > 7) {
		errorf("Adc value of rf band if is wrong, the rfboard.id will be set to -1\n");
		return -1;
	}

	return level;
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
		rfboard_id = adc2deltanv_table[adc_val];

	debugf("rfboard.id = %d\n", rfboard_id);

	return rfboard_id;
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
	int boardid = 0;
/*modified according to the actual hardware*/
#ifdef CONFIG_SSMH_WIFI_MODE
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
int sprd_get_sim(void)
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

/* check gpio/adc return “UMW2651” or “UMW2652” or "SongshanW6"*/
int sprd_get_wcn_boardid(void)
{
	int wcn_boardid = 0;
	int reg_val = 0;
	int gpio_val = 0;
	int i;
	int gpio_value_table_pullup[2] = {0};
	int gpio_value_table_pulldown[2] = {0};

	for (i=0;i<2;i++) {
		/*get pin original states*/
		pinaf_value_table_original[i] = readl(CTL_PIN_BASE+pinaf_addr_table[i]);
		pinds_value_table_original[i] = readl(CTL_PIN_BASE+pinds_addr_table[i]);

		/*gpio and pin had been enabled, we just set pullup here*/
		//set to be gpio function
		reg_val = readl(CTL_PIN_BASE+pinaf_addr_table[i]);
		reg_val |= GPIO_FUNC_BIT;
		writel(reg_val, CTL_PIN_BASE+pinds_addr_table[i]);

		//enable pullup
		reg_val = readl(CTL_PIN_BASE+pinds_addr_table[i]);
		reg_val |= PULL_UP_20K_BIT;
		writel(reg_val, CTL_PIN_BASE+pinds_addr_table[i]);

		gpio_value_table_pullup[i] = gpio_state(gpio_table[i]);//pullup had been did in pinmap

		//disable pullup
		writel(~(PULL_UP_BIT_MARKS), CTL_PIN_BASE+pinds_addr_table[i]);

		//enable pulldown
		reg_val = readl(CTL_PIN_BASE+pinds_addr_table[i]);
		reg_val |= PULL_DOWN_50K_BIT;
		writel(reg_val, CTL_PIN_BASE+pinds_addr_table[i]);

		gpio_value_table_pulldown[i] = gpio_state(gpio_table[i]);

		//disable pulldown, incase of leakage
		writel(~(PULL_DOWN_50K_BIT), CTL_PIN_BASE+pinds_addr_table[i]);

		//calculate gpio value
		gpio_val = gpio_value_table_pullup[i] + gpio_value_table_pulldown[i];
		switch(gpio_val){
			case 0:
				gpio_value_table[i] = 0;//out pulldown, return 0
				break;
			case 1:
				gpio_value_table[i] = 2;//out no pulldown or pullup, return 2
				break;
			case 2:
				gpio_value_table[i] = 1;//out pullup, return 1
				break;
			default:
				gpio_value_table[i] = -1;
                }
	}

	if(gpio_value_table[1] != 0) {
		errorf("gpio value of wcn board if is wrong, the rfboard.id will be set to -1\n");
		return -1;
	} else if(gpio_value_table[0] == 1) {
		wcn_boardid = 1;
	} else if(gpio_value_table[0] == 0) {
		wcn_boardid = 0;
	} else if(gpio_value_table[0] == 2) {
		wcn_boardid = 2;
	} else {
		wcn_boardid = -1;
	}

	debugf("wcnboard.id = %d\n", wcn_boardid);

	/*recovery pin original states*/
	for (i=0;i<2;i++) {
		writel(pinaf_value_table_original[i], CTL_PIN_BASE+pinaf_addr_table[i]);
		writel(pinds_value_table_original[i], CTL_PIN_BASE+pinds_addr_table[i]);
	}

	return wcn_boardid;
}