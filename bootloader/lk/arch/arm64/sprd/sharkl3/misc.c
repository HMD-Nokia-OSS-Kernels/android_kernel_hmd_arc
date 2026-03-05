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
//ZOVERLAY_TAG_HMD_ONEIMAGE

#include <asm/arch/common.h>
#include <asm/arch/pinmap.h>
#include <asm/arch/sprd_reg.h>
#include <chipram_env.h>
#include <secureboot/sec_efuse_sharkl3.h>
#include <sprd_pmic_misc.h>
#include <sprd_efuse.h>
#include <sprd_common.h>
#ifdef CONFIG_BOOTLOADER_HWFEATURE
#include <sprd_hwfeature.h>
#endif

#include "adi_hal_internal.h"
#include "gpio_plus.h"

#define	Shar	0x53686172
#define	kL3	0x6b4c3300

#define EFUSE_BLK        44
#define EFUSE_FLAG        (BIT(0))
#define EFUSE_MASK(x)        ((x) & EFUSE_FLAG)

#ifdef CONFIG_BOOTLOADER_HWFEATURE
#define CHIP_FREQ_MASK 0xF0000
static unsigned int get_chip_bonding(struct hwfeature *phwf)
{
/*Freq     bit       le
     BIT     16       0
     BIT     17       0
     BIT     18       1
*/

	typedef enum {
		SHARKL3RE = 0,
		SHARKL3E,
		SHARKL3R,
		SHARKL3
	};

    u32 data = sprd_ap_efuse_read(46);
    u32 mask = data & CHIP_FREQ_MASK;
    u32 mask_le = ((1 << 18));

	if (mask == mask_le) {
		if (__raw_readl(REG_AON_APB_AON_MFT_ID) == 0x00000a00)
			return SHARKL3RE;
		else
			return SHARKL3E;
	} else if (__raw_readl(REG_AON_APB_AON_MFT_ID) == 0x00000a00) {
			return SHARKL3R;
	} else {
			return SHARKL3;
	}

}

unsigned int get_soc_bonding(struct hwfeature *phwf)
{
        return get_chip_bonding(phwf);
}

#endif
void rf_sen_gpio_init(void)
{
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
}

extern int (*arch_poweroff)(void);
static int sharkl3_poweroff(void)
{
	writel(0x30, CTL_PIN_BASE + REG_PIN_RFSDA1);
	writel(0x30, CTL_PIN_BASE + REG_PIN_RFSCK1);
	writel(0x30, CTL_PIN_BASE + REG_PIN_RFSEN1);
	writel(0x30, CTL_PIN_BASE + REG_PIN_RFSDA0);
	writel(0x30, CTL_PIN_BASE + REG_PIN_RFSCK0);
	writel(0x30, CTL_PIN_BASE + REG_PIN_RFSEN0);

	return 0;
}


int sprd_get_chipid(int *chip_id, int *version_id)
{
	unsigned int chip_id0 = 0, chip_id1 = 0;
	unsigned int ver_id;

	chip_id0 = readl((void *)REG_AON_APB_AON_CHIP_ID0);
	chip_id1 = readl((void *)REG_AON_APB_AON_CHIP_ID1);

	if ((chip_id1 == Shar) && (chip_id0 == kL3)) {
		chip_id1 = 0x73703938;//sp98
		chip_id0 = 0x36336100;//63a
	} else {
		chip_id1 = 0;
		chip_id0 = 0;
	}

	ver_id = readl((void *)REG_AON_APB_AON_VER_ID);

	if (!chip_id || !version_id) {
		debugf("chip id = 0x%x%x, VID = 0x%x\n",chip_id1, chip_id0, ver_id);
		return 0;
	}

	*chip_id = chip_id1;
	*(chip_id + 1) = chip_id0;
	*version_id = ver_id;

	return 0;
}

static unsigned int get_chip_id(struct hwfeature *phwf)
{
	unsigned int reg_val;

	reg_val = readl(REG_AON_APB_AON_VER_ID);

	return reg_val;
}

void sprd_get_manufacturel_id(void)
{
	unsigned int mft_id = 0;
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
int nsrp_efuse_read(void)
{
  u32 val_read = 0;
  int custom_efuse_val = 0;
  val_read = sprd_ap_efuse_read(EFUSE_BLK);
  custom_efuse_val = (int)EFUSE_MASK(val_read);
  return custom_efuse_val;
}
