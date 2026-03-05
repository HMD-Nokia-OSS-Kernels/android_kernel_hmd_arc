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

#define Qogi	0x516f6769
#define rL6	0x724c3600

#ifdef CONFIG_BOOTLOADER_HWFEATURE
#include <sprd_hwfeature.h>

#define BIT_AP_SHIFT_7	 7
#define BIT_AP_SHIFT_16  16
#define BIT_AP_SHIFT_21  21

#define BIT_GPU_SHIFT_18 18
#define BIT_GPU_SHIFT_20 20
#define BIT_DEFAULT      (~0U)

extern void lcd_printf(const char *fmt, ...);
extern struct hwfeature hwf;
boot_device_t get_bootdevice(void);
static long get_chip_bonding(struct hwfeature *phwf)
{
	unsigned int default_code = BIT_DEFAULT;
	enum {
		T616_IC = 0,
		T606_IC,
		T612_IC,
		T619_IC
	};

/* Bounding bit      T606  T616  T612  T619  T616(efuse 未烧写)
 * APCPU
           BIT  5    0     0     0     1     0
           BIT  7    1     0     1     0     0
           BIT  21   1     1     0     0     0
           BIT  16   1     1     1     0     0

 * GPU
           BIT  20   1     1     1     0     0
           BIT  18   1     0     1     0     0
*/
	u32 bounding_mask = (1 << 7) | (1 << 5) | (1 << 21) | (1 << 16) | (1 << 20) | (1 << 18);

	u32 t606 = (1 << 7)  | (1 << 21) | (1 << 16) | (1 << 20) | (1 << 18);
	u32 t616 = (1 << 21) | (1 << 16) | (1 << 20);
	u32 t612 = (1 << 7)  | (1 << 16) | (1 << 20) | (1 << 18);
	u32 t619 = (1 << 5);

	u32 efuse_data = readl(REG_AON_APB_BOND_OPT0) & bounding_mask;

	if (efuse_data == t606)
		return T606_IC;
	else if (efuse_data == t616)
		return T616_IC;
	else if (efuse_data == t612)
		return T612_IC;
	else if (efuse_data == t619)
		return T619_IC;
	else
		return default_code;
}

unsigned int get_soc_bonding(struct hwfeature *phwf)
{
	return get_chip_bonding(phwf);
}

long get_chip_id(struct hwfeature *phwf)
{

	unsigned int  reg_val;

	reg_val = readl(REG_AON_APB_AON_VER_ID);

	return reg_val;
}

static void check_phase_version_process(void)
{
	u32 chip_bonding = 0, chip_id = 0, boot_device = 0;
	chip_bonding = get_soc_bonding(&hwf);
	chip_id = get_chip_id(&hwf);
	boot_device = get_bootdevice();

	dprintf(INFO,"chip_bonding:%d, chip_id:%d, boot_device:0x%x\n", chip_bonding, chip_id, boot_device);
	if ((chip_bonding == 0) && (chip_id == 1) && (boot_device == BOOT_DEVICE_EMMC)) {
		errorf("chip_bonding:%d, chip_id:%d, boot_device:0x%x, T616 && AB && EMMC combination !\n", chip_bonding, chip_id, boot_device);
		lcd_printf("T616 && AB && EMMC combination !\n");
		panic("bad combination...\n");
	}
	return;
}
#endif

int sprd_get_chipid(int *chip_id, int *version_id)
{
	unsigned int chip_id0 = 0, chip_id1 = 0;
	unsigned int ver_id;

	chip_id0 = readl((void *)REG_AON_APB_AON_CHIP_ID0);
	chip_id1 = readl((void *)REG_AON_APB_AON_CHIP_ID1);

	if ((chip_id1 == Qogi) && (chip_id0 == rL6)) {
		chip_id1 = 0x756d7339;//ums9
		chip_id0 = 0x32333000;//230
	}
	else {
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
	check_phase_version_process();
#endif
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

boot_device_t get_bootdevice(void)
{
	unsigned int boot_val = 0;

	boot_val = (* (volatile unsigned int *) (REG_AON_APB_BOOT_MODE)) & 0x3;
	if (boot_val == 0x3)
		return BOOT_DEVICE_EMMC;
	else
		return BOOT_DEVICE_UFS;
}

