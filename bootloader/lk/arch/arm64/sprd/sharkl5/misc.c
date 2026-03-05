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
#include <secureboot/sec_efuse_sharkl5.h>
#include <sprd_common.h>
#include <sprd_pmic_misc.h>
#include <sprd_hwfeature.h>

#include "adi_hal_internal.h"

/*
	REG_AON_APB_BOND_OPT0  ==> romcode set
	REG_AON_APB_BOND_OPT1  ==> set it later

	!!! notice: these two registers can be set only one time!!!

	B1[0] : B0[0]
	0     : 0     Jtag enable
	0     : 1     Jtag disable
	1     : 0     Jtag enable
	1     : 1     Jtag enable
*/

/*************************************************
* 1 : enable jtag success                        *
* 0 : enable jtag fail                           *
*************************************************/
int sprd_jtag_enable(void)
{
	return 1;
}

/*************************************************
* 1 : disable jtag success                       *
* 0 : disable jtag fail                          *
*************************************************/
int sprd_jtag_disable(void)
{
	return 1;
}

static void ap_slp_cp_dbg_cfg(void)
{
//	*((volatile unsigned int *)(REG_AP_AHB_MCU_PAUSE)) |= BIT_MCU_SLEEP_FOLLOW_APCPU_EN; //when ap sleep, cp can continue debug
}

static void ap_cpll_rel_cfg(void)
{
}

static void bb_bg_auto_en(void)
{
	*((volatile unsigned int *)(REG_AON_APB_RES_REG0)) |= 1<<8;
}

static void ap_close_wpll_en(void)
{
}

static void ap_close_cpll_en(void)
{
}

static void ap_close_wifipll_en(void)
{
}

static void bb_ldo_auto_en(void)
{
	*((volatile unsigned int *)(REG_AON_APB_RES_REG0)) |= 1<<9;
}

#ifdef CONFIG_OF_LIBFDT
void scx35_pmu_reconfig(void)
{
	/* FIXME:
	 * turn on gpu/mm domain for clock device initcall, and then turn off asap.
	 */
	REG32(REG_AON_APB_APB_EB0) |= BIT_AON_APB_GPU_EB;
#ifndef PLATFORM_SHARKL5
	REG32(REG_AON_APB_APB_EB1) |= BIT_AON_APB_DISP_EB;
	REG32(REG_AON_APB_APB_EB1) |= BIT_AON_APB_CAM_EB;
	REG32(REG_AON_APB_APB_EB1) |= BIT_AON_APB_VSP_EB;
#endif
}

#else
void scx35_pmu_reconfig(void) {}
#endif

#define Shar  0x53686172
#define kL5  0x6B4C3500

int sprd_get_chipid(int *chip_id, int *version_id)
{

	unsigned int chip_id0 = 0, chip_id1 = 0;
	unsigned int ver_id;

	chip_id0 = __raw_readl((void *)REG_AON_APB_AON_CHIP_ID0);
	chip_id1 = __raw_readl((void *)REG_AON_APB_AON_CHIP_ID1);

	if ((chip_id1 == Shar) && (chip_id0 == kL5)) {
		chip_id1 = 0x756d7333;//ums3
		chip_id0 = 0x31320000;//12
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

#if defined CONFIG_SMT
void set_smt(void)
{
	if(0x00 == CONFIG_SMT_VALUE){
		__raw_writel(__raw_readl(JVT_MT_CFG) & (~(0x3)),JVT_MT_CFG);/*open smt*/
	}else if(0x01 == CONFIG_SMT_VALUE){
		__raw_writel(__raw_readl(JVT_MT_CFG) | 0x3,JVT_MT_CFG);/*close smt*/
	}else if(0x10 == CONFIG_SMT_VALUE){
		/*get config info from mmc*/
	}
}
#endif

void misc_init(void)
{
	scx35_pmu_reconfig();
	ap_slp_cp_dbg_cfg();
	ap_cpll_rel_cfg();
#ifndef  CONFIG_SPX15
	ap_close_wpll_en();
	ap_close_cpll_en();
	ap_close_wifipll_en();
#endif
	bb_bg_auto_en();
	bb_ldo_auto_en();
	pmic_misc_init();
	sprd_get_chipid(NULL, NULL);

#ifdef CONFIG_SMT
	set_smt();
#endif
}

boot_device_t get_bootdevice(void)
{
	return BOOT_DEVICE_EMMC;
}
