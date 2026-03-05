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

#include "asm/arch/common.h"
#include <asm/arch/sprd_reg.h>
#include "adi_hal_internal.h"
#include <sprd_pmic_misc.h>
#include <asm/arch/pinmap.h>
#include "gpio_plus.h"
#include <chipram_env.h>
#include <sprd_common.h>
#ifdef CONFIG_BOOTLOADER_HWFEATURE
#include <sprd_hwfeature.h>
#endif

#define KLT8	0x6B4C5438
#define KL	0x6B4C0000
#define SHAR	0x53686172
#define E2	0x65320000
#define WHAL	0x5768616C

#ifdef CONFIG_BOOTLOADER_HWFEATURE
static unsigned int get_chip_bonding(struct hwfeature *phwf)
{
/*
	typedef enum {
		SHARKL3R = 0,
		SHARKL3
	};

	phwf = phwf;

	if (__raw_readl(REG_AON_APB_AON_MFT_ID) == 0x00000a00)
		return SHARKL3R;
	else
		return SHARKL3;
*/
	return 0;
}

unsigned int get_soc_bonding(struct hwfeature *phwf)
{
        return 0; //get_chip_bonding(phwf);
}
#endif

void rf_sen_gpio_init(void)
{
/*
	sprd_gpio_request(1);
	sprd_gpio_direction_output(1,0);
	sprd_gpio_request(2);
	sprd_gpio_direction_output(2,0);
	sprd_gpio_request(3);
	sprd_gpio_direction_output(3,0);
	sprd_gpio_request(12);
	sprd_gpio_direction_output(12,0);
	sprd_gpio_request(13);
	sprd_gpio_direction_output(13,0);
	sprd_gpio_request(14);
	sprd_gpio_direction_output(14,0);
*/
}

extern int (*arch_poweroff)(void);
static int sharkl3_poweroff(void)
{
/*
	writel(0x30, CTL_PIN_BASE + REG_PIN_RFSDA1);
	writel(0x30, CTL_PIN_BASE + REG_PIN_RFSCK1);
	writel(0x30, CTL_PIN_BASE + REG_PIN_RFSEN1);
	writel(0x30, CTL_PIN_BASE + REG_PIN_RFSDA0);
	writel(0x30, CTL_PIN_BASE + REG_PIN_RFSCK0);
	writel(0x30, CTL_PIN_BASE + REG_PIN_RFSEN0);
*/
	return 0;
}

int sprd_get_chipid(int *chip_id, int *version_id)
{
/*
	unsigned int chip_id0 = 0, chip_id1 = 0;
	int ch_id;
	int ver_id;

	chip_id0 = readl((void *)REG_AON_APB_AON_CHIP_ID0);
	chip_id1 = readl((void *)REG_AON_APB_AON_CHIP_ID1);

	switch (chip_id1) {
	case SHAR:
		switch (chip_id0) {
		case KL:
			ch_id = 0x9830;
			break;
		case KLT8:
			ch_id = 0x9838;
			break;
		default:
			ch_id = 0;
			break;
		}
		break;

	case WHAL:
		switch (chip_id0) {
		case E2:
			ch_id = 0x9850;
			break;
		default:
			ch_id = 0;
			break;
		}
		break;

	default:
		ch_id = 0;
		break;
	}

	ver_id = readl((void *)REG_AON_APB_AON_VER_ID);

	if (!chip_id || !version_id) {
		debugf("chip id = 0x%x, VID = 0x%x\n",ch_id, ver_id);
		return 0;
	}

	*chip_id = ch_id;
	*version_id = ver_id;
*/
	return 0;
}

void sprd_get_manufacturel_id(void)
{
	int mft_id = 0;
	mft_id = CHIP_REG_GET(REG_AON_APB_AON_MFT_ID);
	debugf("manufacturel_id is : 0x%x \n", mft_id);
}

void misc_init(void)
{

	pmic_misc_init();
	sprd_get_chipid(NULL, NULL);
	sprd_get_manufacturel_id();
	rf_sen_gpio_init();
#ifdef CONFIG_BOOTLOADER_HWFEATURE
	hwfeature_hook_get_efuse(get_chip_bonding);
#endif
	arch_poweroff = sharkl3_poweroff;

}

boot_device_t get_bootdevice(void)
{
	return BOOT_DEVICE_EMMC;
}

