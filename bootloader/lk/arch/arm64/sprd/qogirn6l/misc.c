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
#include <secureboot/sec_efuse_qogirn6l.h>
#include <sprd_common.h>
#include <sprd_hwfeature.h>
#include <sprd_pmic_misc.h>

#include "adi_hal_internal.h"

#define Qogi	0x516F6769
#define rN6L	0x724E364C

/* efuse soc_version and bin_version block index */
#define EFUSE_AP_CPU_INDEX      70
#define EFUSE_AP_CPU_MASK       ((1 << 15) | (1 << 25 ))

/* chip version mask */
#define CHIP_CPU_T765           ((0 << 15) | (0 << 25 ))
#define CHIP_CPU_T750           ((0 << 15) | (1 << 25 ))

static void emmc_dev_powerdown_mphy_core(void);
void bypass_ufs_powergate(void);
extern void pub_qos_cfg(void);
extern void ap_qos_cfg(void);

static unsigned int get_chip_bonding(struct hwfeature *phwf)
{
	typedef enum {
		T765_IC = 0,
		T750_IC,
		SIGN_OFF
	};

	u32  cpu_version = sprd_ap_efuse_read(EFUSE_AP_CPU_INDEX);

	cpu_version = cpu_version & EFUSE_AP_CPU_MASK;

	switch (cpu_version) {
	case CHIP_CPU_T765:
		return T765_IC;
	case CHIP_CPU_T750:
		return T750_IC;
	default:
		return SIGN_OFF;

	}
}

unsigned int get_soc_bonding(struct hwfeature *phwf)
{
	return get_chip_bonding(phwf);
}

static unsigned int get_chip_id(struct hwfeature *phwf)
{

	unsigned int  reg_val;

	typedef enum {
		qogirn6l_AA = 0,
		qogirn6l_AB
	};

	reg_val = __raw_readl(REG_AON_APB_AON_VER_ID);

	if (reg_val)
		return qogirn6l_AB;
	else
		return qogirn6l_AA;

}

int sprd_get_chipid(int *chip_id, int *version_id)
{

	unsigned int chip_id0 = 0, chip_id1 = 0;
	unsigned int ver_id;

	chip_id0 = __raw_readl((void *)REG_AON_APB_AON_CHIP_ID0);
	chip_id1 = __raw_readl((void *)REG_AON_APB_AON_CHIP_ID1);

	if ((chip_id1 == Qogi) && (chip_id0 == rN6L)) {
		chip_id1 = 0x756d7339;//ums9
		chip_id0 = 0x36323100;//621
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

typedef struct mem_cs_info
{
	uint32_t cs_number;
	uint32_t cs0_size;//bytes
	uint32_t cs1_size;//bytes
} mem_cs_info_t;

int get_dram_cs_number(void)
{
	mem_cs_info_t *cs_info_ptr = (mem_cs_info_t *)0x1C00;
	return cs_info_ptr->cs_number;
}

int get_dram_cs0_size(void)
{
	mem_cs_info_t *cs_info_ptr = (mem_cs_info_t *)0x1C00;
	return cs_info_ptr->cs0_size;
}

boot_device_t get_bootdevice(void)
{
#ifdef CONFIG_FPGA
	return BOOT_DEVICE_EMMC;
#endif
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

void misc_init(void)
{
	pmic_misc_init();
	adi_hwchannel_config();
	sprd_get_chipid(NULL, NULL);
	hwfeature_hook_get_efuse(get_chip_bonding);
	hwfeature_hook_get_chipid(get_chip_id);
	bypass_ufs_powergate();
	pub_qos_cfg();
	ap_qos_cfg();
	disable_pad_clk26M_sinout_pcie_1P8();
	emmc_dev_powerdown_mphy_core();
}
