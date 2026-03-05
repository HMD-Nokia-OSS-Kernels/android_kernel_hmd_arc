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

/*
 *  For better adaptation for more target boards, some detail implementations
 *  should be placed in target side:
 *  project/$prject/rules.mk
 *  	CONFIG_BOARDID_ENCODING_GPIO_NUMS:	how many gpios for encoding boardid
 *  target/$project/boardid.c
 *  	gpio_table[]:				array for gpios
 *  	target_get_boardid():			get boardid with gpio info
 *  	pinaf_addr_table[]:			gpio related pin application function config
 *  	pinds_addr_table[]:			gpio related pnn driven strength config
 */

#include <asm/arch/sprd_reg.h>
#include <sprd_common.h>
#include <malloc.h>
#include <asm/arch/pinmap.h>
#include <sprd_fdt_support.h>

#ifdef CONFIG_MATCH_DTBO_BY_BOARDID
#ifdef CONFIG_CUSTOMER_PHONE
#ifdef GET_BOARDID_FROM_GPIO

#define PULL_UP_20K_BIT		BIT_7
#define PULL_UP_4K7_BIT		BIT_12
#define PULL_UP_1K8_BIT		BIT_7|BIT_12
#define PULL_UP_BIT_MARKS	BIT_7|BIT_12
#define PULL_DOWN_50K_BIT	BIT_6
#define GPIO_FUNC_BIT		BIT_4|BIT_5

extern int sprd_gpio_request(unsigned int offset);
extern int sprd_gpio_direction_input(unsigned int offset);
extern int sprd_gpio_get(unsigned int offset);
extern void sprd_gpio_init(void);
extern int target_get_boardid(void);
extern char* target_get_hwlevel(void);

#define GPIO_NUMS CONFIG_BOARDID_ENCODING_GPIO_NUMS
extern int gpio_value_table_pullup[GPIO_NUMS];
extern int gpio_value_table_pulldown[GPIO_NUMS];
extern unsigned int gpio_table[GPIO_NUMS];
extern int pinaf_addr_table[GPIO_NUMS];
extern int pinds_addr_table[GPIO_NUMS];

static unsigned int gpio_state(unsigned int gpiono)
{
	int value = 0 ;

	sprd_gpio_request(gpiono);
	sprd_gpio_direction_input(gpiono);
	value = sprd_gpio_get(gpiono);

	return value > 0;
}

int boardid_init(void)
{
	int i, reg_val = 0;

	/* enable gpio */
	sprd_gpio_init();

	/*gpio and pin had been enabled, we just set pullup here*/
	for(i = 0;i < GPIO_NUMS; i++) {
		/* set to be gpio function */
		reg_val = readl(CTL_PIN_BASE+pinaf_addr_table[i]);
		reg_val |= GPIO_FUNC_BIT;
		writel(reg_val, CTL_PIN_BASE+pinaf_addr_table[i]);
	}

	for(i = 0;i < GPIO_NUMS; i++) {
		/* enable pullup */
		reg_val = readl(CTL_PIN_BASE+pinds_addr_table[i]);
		reg_val |= PULL_UP_20K_BIT;
		writel(reg_val, CTL_PIN_BASE+pinds_addr_table[i]);
	}

        /* read gpio state */
	for(i = 0;i < GPIO_NUMS; i++) {
		gpio_value_table_pullup[i] = gpio_state(gpio_table[i]);
	}
	/* disable pullup */
	for(i = 0;i < GPIO_NUMS; i++) {
		writel(~(PULL_UP_BIT_MARKS), CTL_PIN_BASE+pinds_addr_table[i]);
	}
	/* enable pulldown */
	for(i = 0;i < GPIO_NUMS; i++) {
		reg_val = readl(CTL_PIN_BASE+pinds_addr_table[i]);
		writel(PULL_DOWN_50K_BIT | reg_val, CTL_PIN_BASE+pinds_addr_table[i]);
	}
	mdelay(10);

	for(i = 0;i < GPIO_NUMS; i++) {
		gpio_value_table_pulldown[i] = gpio_state(gpio_table[i]);
	}
	/* disable pulldown, in case of leakage */
	for(i = 0;i < GPIO_NUMS; i++) {
		writel(~(PULL_DOWN_50K_BIT), CTL_PIN_BASE+pinds_addr_table[i]);
	}
	mdelay(10);
	return 0;
}

/* Return dtbo info */
int sprd_get_dtboinfo(void)
{
	static int dtbo_val __attribute__((section(".data"))) = 0;
	int i = 0;

	if (dtbo_val != 0) {
		debugf("second get boardid, dtbo.info = 0x%x\n", dtbo_val);
		return dtbo_val;
	}

	dtbo_val = target_get_boardid();

	return dtbo_val;
}

void fdt_fixup_boardid_hwlevel(void *fdt) {
	char buf[64];
	char *hwlevel = target_get_hwlevel();
	if (hwlevel == NULL || strlen(hwlevel)==0) {
		hwlevel = "NA";
	}
	snprintf(buf, 63, "ro.boot.hwlevel=%s", hwlevel);
	if (fdt_chosen_bootargs_append(fdt, buf, 1)) {
		errorf("setup bootargs ro.boot.hwlevel fail!\n");
	}
	debugf("[%s] ro.boot.hwlevel=%s\n", __func__, hwlevel);
}
#endif
#endif
#endif

