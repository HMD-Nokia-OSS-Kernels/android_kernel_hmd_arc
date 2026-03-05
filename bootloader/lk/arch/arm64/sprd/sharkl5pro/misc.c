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
#include <asm/arch/pinmap.h>
#include <asm/arch/sprd_reg.h>
#include <chipram_env.h>
#include <sprd_common.h>
#include <sprd_pmic_misc.h>

#include "adi_hal_internal.h"
#include "gpio_plus.h"

#define	Shar	0x53686172
#define	kL5P	0x6b4c3550

#ifdef CONFIG_BOOTLOADER_HWFEATURE
#include <sprd_hwfeature.h>

#define BIT_AP_SHIFT_7	 7
#define BIT_AP_SHIFT_16  16
#define BIT_AP_SHIFT_18  18
#define BIT_AP_SHIFT_20  20
#define BIT_AP_SHIFT_21  21

#define BIT_DEFAULT (~0U)

static long get_chip_bonding(struct hwfeature *phwf)
{

	unsigned int default_code = BIT_DEFAULT;
	enum {
		T618_IC = 0,
		T610_IC,
		T700_IC
	};

	if ((!(readl(REG_AON_APB_BOND_OPT0) & (1 << BIT_AP_SHIFT_7))) &&
		(readl(REG_AON_APB_BOND_OPT0) & (1 << BIT_AP_SHIFT_16)) &&
			(readl(REG_AON_APB_BOND_OPT0) & (1 << BIT_AP_SHIFT_18)) &&
				(readl(REG_AON_APB_BOND_OPT0) & (1 << BIT_AP_SHIFT_20)) &&
					(!(readl(REG_AON_APB_BOND_OPT0) & (1 << BIT_AP_SHIFT_21))))
		return T610_IC;
	else if ((!(readl(REG_AON_APB_BOND_OPT0) & (1 << BIT_AP_SHIFT_16))) &&
			(!(readl(REG_AON_APB_BOND_OPT0) & (1 << BIT_AP_SHIFT_18))) &&
				(!(readl(REG_AON_APB_BOND_OPT0) & (1 << BIT_AP_SHIFT_20))) &&
					(!(readl(REG_AON_APB_BOND_OPT0) & (1 << BIT_AP_SHIFT_21))))
		return T618_IC;
	else if ((readl(REG_AON_APB_BOND_OPT0) & (1 << BIT_AP_SHIFT_16)) &&
			(!(readl(REG_AON_APB_BOND_OPT0) & (1 << BIT_AP_SHIFT_18))) &&
				(!(readl(REG_AON_APB_BOND_OPT0) & (1 << BIT_AP_SHIFT_20))) &&
					(!(readl(REG_AON_APB_BOND_OPT0) & (1 << BIT_AP_SHIFT_21))))
		return T700_IC;
	else
		return default_code;
}

unsigned int get_soc_bonding(struct hwfeature *phwf)
{
	return get_chip_bonding(phwf);
}

static long get_chip_id(struct hwfeature *phwf)
{

	unsigned int  reg_val;

	reg_val = readl(REG_AON_APB_AON_VER_ID);

        return reg_val;
}
#endif

int sprd_get_chipid(int *chip_id, int *version_id)
{
	unsigned int chip_id0 = 0, chip_id1 = 0;
	unsigned int ver_id;

	chip_id0 = readl((void *)REG_AON_APB_AON_CHIP_ID0);
	chip_id1 = readl((void *)REG_AON_APB_AON_CHIP_ID1);

	if ((chip_id1 == Shar) && (chip_id0 == kL5P)) {
		chip_id1 = 0x756d7335;//ums5
		chip_id0 = 0x31320000;//12
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

void adi_hwchannel_config(void)
{
	adi_hwchannel_set(DCDC_CORE_VAL_ADI_CHN, DCDC_CORE_VAL_ADDR);
}

void misc_init(void)
{
	pmic_misc_init();
	adi_hwchannel_config();
	sprd_get_chipid(NULL, NULL);
#ifdef CONFIG_BOOTLOADER_HWFEATURE
	hwfeature_hook_get_efuse(get_chip_bonding);
	hwfeature_hook_get_chipid(get_chip_id);
#endif
}

boot_device_t get_bootdevice(void)
{
	return BOOT_DEVICE_EMMC;
}

#if 0
typedef struct mem_cs_info
{
	uint32_t cs_number;
	uint32_t cs0_size;//bytes
	uint32_t cs1_size;//bytes
}mem_cs_info_t;
PUBLIC int get_dram_cs_number(void)
{
	mem_cs_info_t *cs_info_ptr = 0x1C00;
	return cs_info_ptr->cs_number;
}
PUBLIC int get_dram_cs0_size(void)
{
	mem_cs_info_t *cs_info_ptr = 0x1C00;
	return cs_info_ptr->cs0_size;
}
#endif
