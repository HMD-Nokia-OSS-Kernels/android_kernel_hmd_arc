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
#include <chipram_env.h>
#include <lk/reg.h>
#include <secureboot/sec_efuse_sharkl6pro.h>
#include <sprd_common.h>
#include <sprd_pmic_misc.h>
#ifdef CONFIG_BOOTLOADER_HWFEATURE
#include <sprd_hwfeature.h>
#endif

#include "adi_hal_internal.h"

#define Qogi	0x516f6769
#define rN6P	0x724e3650

static void emmc_dev_powerdown_mphy_core(void);
void bypass_ufs_powergate(void);
extern void pub_qos_cfg(void);
extern void ap_qos_cfg(void);

#ifdef CONFIG_BOOTLOADER_HWFEATURE
/* efuse soc_version and bin_version block index */
#define EFUSE_AP_CPU_INDEX      71
#define EFUSE_LIT_BIN_INDEX     73
#define EFUSE_MED_BIN_INDEX     74
#define EFUSE_BIG_BIN_INDEX     75

/* chip version mask */

#define CHIP_BONDING_MASK       0x7F

#define CHIP_CPU_MASK           ((1 << 24) | (1 << 25 ) |(1 << 26))
#define CHIP_AI_GPU_MASK        ((1 << 27) | (1 << 28 ) |(1 << 29))

#define NEW_CHIP_GPU_MASK       ((1 << 8)  | (1 << 9)   |(1 << 10))
#define NEW_CHIP_AI_MASK        ((1 << 11) | (1 << 12 ) |(1 << 13))
/* Bounding     bit     T760    T770    T820	UTS6560	UTS6560S
* APCPU
         BIT     0       1       1       0	1	1
         BIT     1       1       0       0	1	0
         BIT     2       0       0       0	0	1

* GPU
         BIT     3       1       1       0	1	0
         BIT     4       1       0       0	1	0
* AI
         BIT     5       1       1       0	0	0
         BIT     6       1       0       0	1	0
*/

#define T760_IC_VALUE     ((1 << 0) | (1 << 1) | (1 << 3) | (1 << 4) | (1 << 5)| (1 << 6))
#define T770_IC_VALUE     ((1 << 0) | (1 << 3) | (1 << 5))
#define UTS6560_IC_VALUE  ((1 << 0) | (1 << 1) | (1 << 3) |(1 << 4) | (1 << 6))
#define UTS6560S_IC_VALUE ((1 << 0) | (1 << 2))

static unsigned int get_chip_bonding(struct hwfeature *phwf)
{
	typedef enum {
		T760_IC = 0,
		T770_IC,
		T820_IC,
		UTS6560_IC,
		UTS6560S_IC,
		SIGN_OFF
	};

	u32  lit_reg = sprd_ap_efuse_read(EFUSE_LIT_BIN_INDEX);
	u32  med_reg = sprd_ap_efuse_read(EFUSE_MED_BIN_INDEX);
	u32  big_reg = sprd_ap_efuse_read(EFUSE_BIG_BIN_INDEX);
	u32  block71 = sprd_ap_efuse_read(EFUSE_AP_CPU_INDEX);

	u32 lit_cpu = lit_reg & CHIP_CPU_MASK;
	u32 med_cpu = med_reg & CHIP_CPU_MASK;
	u32 big_cpu = big_reg & CHIP_CPU_MASK;
	u32 gpu = big_reg  & CHIP_AI_GPU_MASK;
	u32 ai  = lit_reg  & CHIP_AI_GPU_MASK;

	u32 new_gpu = block71 & NEW_CHIP_GPU_MASK;
	u32 new_ai = block71 & NEW_CHIP_AI_MASK;

	if ((lit_cpu && med_cpu && big_cpu && gpu && ai) || (new_gpu && new_ai)) {
		u32  cpu = block71 & CHIP_BONDING_MASK;
		return cpu == T760_IC_VALUE ? T760_IC : cpu == T770_IC_VALUE ? T770_IC : cpu == UTS6560_IC_VALUE ? UTS6560_IC : cpu == UTS6560S_IC_VALUE ? UTS6560S_IC : T820_IC;
	} else
		return SIGN_OFF;
}

unsigned int get_soc_bonding(struct hwfeature *phwf)
{
	return get_chip_bonding(phwf);
}

unsigned int get_lowv_chip(struct hwfeature *phwf)
{
	u32 data = sprd_ap_efuse_read(71);
	u32 mask = (1 << 15);

	if (data & mask)
		return 0;
	else
		return 1;
}

static unsigned int get_chip_id(struct hwfeature *phwf)
{

	unsigned int  reg_val;

	typedef enum {
		qogirn6pro_AA = 0,
		qogirn6pro_AB
	};

	reg_val = __raw_readl(REG_AON_APB_AON_VER_ID);

	if (reg_val)
		return qogirn6pro_AB;
	else
		return qogirn6pro_AA;

}
#endif

int sprd_get_chipid(int *chip_id, int *version_id)
{
	unsigned int chip_id0 = 0, chip_id1 = 0;
	unsigned int ver_id;

	chip_id0 = __raw_readl((void *)REG_AON_APB_AON_CHIP_ID0);
	chip_id1 = __raw_readl((void *)REG_AON_APB_AON_CHIP_ID1);

	if ((chip_id1 == Qogi) && (chip_id0 == rN6P)) {
		chip_id1 = 0x756d7339;//ums9
		chip_id0 = 0x36323000;//620
	} else {
		chip_id1 = 0;
		chip_id0 = 0;
	}

	ver_id = __raw_readl((void *)REG_AON_APB_AON_VER_ID);

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

/* Close PAD_CLK26M_SINOUT_PCIE_1P8 */
void disable_pad_clk26M_sinout_pcie_1P8(void)
{
        u32 temp, val, ana_eb = 0;

        /* Check ANA_EB*/
        if(CHIP_REG_GET(REG_AON_APB_APB_EB1) & BIT_AON_APB_ANA_EB)
                ana_eb = 1;
        else
                CHIP_REG_OR(REG_AON_APB_APB_EB1, BIT_AON_APB_ANA_EB);

        temp = CHIP_REG_GET(REG_ANLG_PHY_G1_ANALOG_BB_TOP_SINE_DRV_CTRL);
        val = temp & ~(BIT_ANLG_PHY_G1_ANALOG_BB_TOP_SINDRV_26M_ENA_PCIE);
        CHIP_REG_SET(REG_ANLG_PHY_G1_ANALOG_BB_TOP_SINE_DRV_CTRL, val);

        if(!ana_eb)
		CHIP_REG_AND(REG_AON_APB_APB_EB1, ~(BIT_AON_APB_ANA_EB));
}

void misc_init(void)
{
	pmic_misc_init();
	adi_hwchannel_config();
	sprd_get_chipid(NULL, NULL);
#ifdef CONFIG_BOOTLOADER_HWFEATURE
	hwfeature_hook_get_efuse(get_chip_bonding);
	hwfeature_hook_get_chipid(get_chip_id);
	hwfeature_hook_get_lowv(get_lowv_chip);
#endif
	bypass_ufs_powergate();
	pub_qos_cfg();
	ap_qos_cfg();
	disable_pad_clk26M_sinout_pcie_1P8();
	emmc_dev_powerdown_mphy_core();
}

typedef struct mem_cs_info
{
	uint32_t cs_number;
	uint32_t cs0_size;//bytes
	uint32_t cs1_size;//bytes
} mem_cs_info_t;

int get_dram_cs_number(void)
{
	mem_cs_info_t *cs_info_ptr = 0x1C00;
	return cs_info_ptr->cs_number;
}

int get_dram_cs0_size(void)
{
	mem_cs_info_t *cs_info_ptr = 0x1C00;
	return cs_info_ptr->cs0_size;
}

boot_device_t get_bootdevice(void)
{
	unsigned int boot_val = 0;

	boot_val = (* (volatile unsigned int *) (REG_AON_APB_BOOT_MODE)) & 0x3;
	if (boot_val == 0x3)
		return BOOT_DEVICE_EMMC;
	else
		return BOOT_DEVICE_UFS;
}

void bypass_ufs_powergate(void)
{
	if (get_bootdevice() == BOOT_DEVICE_EMMC)
		CHIP_REG_OR(REG_PMU_APB_UFS_PWR_GATE_BYP_CFG,
					   BIT_PMU_APB_UFS_PWR_GATE_BYP);
}

/*
 * EMMC device deletes UFS probe, and MPHY is in the high power consumption state after power-on.
 * Therefore, MPHY needs to be set to disable state.
 */
static void emmc_dev_powerdown_mphy_core(void)
{
	if (get_bootdevice() == BOOT_DEVICE_EMMC)
		CHIP_REG_OR(SPRD_AON_ANLG_PHY_G12_PHYS, 0x2000);
}
