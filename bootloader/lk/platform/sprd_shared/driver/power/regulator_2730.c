/*
 * Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
*/

#include <lk/debug.h>
#include <sys/types.h>
#include <linux/kernel.h>
#include <asm/arch/sprd_reg.h>
#include <power/sprd_pmic/pmic_glb_reg.h>
#include <sprd_efuse.h>
#include <adi_hal_internal.h>

#define REGU_CALI_DEBUG

#ifdef REGU_CALI_DEBUG
#define regu_debug(fmt, arg...)		dprintf(INFO,fmt, ## arg)
#else
#define regu_debug(fmt, arg...)
#endif

#undef debug
#define debug0(format, arg...)
#define debug(format, arg...)		regu_debug(format, ## arg)
#define debug1(format, arg...)		regu_debug("\t" format, ## arg)
#define debug2(format, arg...)		regu_debug("\t\t" format, ## arg)

#define CBANK_VALUE 0x5
#define WR_UNLOCK 0x6e7f
#define TSX_WR_UNLOCK 0x1990
#define TRANS_26M_EN 1
#define DIV_COEF 0x750
#define IB_ZCD_THRESHOLD_MAX 111
#define IB_ZCD_THRESHOLD_MIN 67

#ifdef CONFIG_ADIE_UMP518
#define IB_TRIM_BLOCK		5
#define IB_TRIM_MASK		GENMASK(15,9)
#define VDD18_REFTRIM_LP_BLOCK	4
#define VDD18_REFTRIM_LP_MASK	GENMASK(5,2)
#else
#define IB_TRIM_BLOCK		14
#define IB_TRIM_MASK		GENMASK(15,9)
#define VDD18_REFTRIM_LP_BLOCK	14
#define VDD18_REFTRIM_LP_MASK	GENMASK(4,1)
#endif

#define GENMASK(h, l) \
	(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

/* abs() handles unsigned and signed longs, ints, shorts and chars.  For all input types abs()
 * returns a signed long.
 * abs() should not be used for 64-bit types (s64, u64, long long) - use abs64() for those.*/
#define abs(x) ({									\
				long ret;							\
				if (sizeof(x) == sizeof(long)) {		\
					long __x = (x);				\
					ret = (__x < 0) ? -__x : __x;		\
				} else {							\
					int __x = (x);					\
					ret = (__x < 0) ? -__x : __x;		\
				}								\
				ret;								\
			})


#undef ffs
#undef fls

/* On ARMv5 and above those functions can be implemented around the clz instruction for
 * much better code efficiency.		*/

static inline int fls(int x)
{
	int ret;

	asm("clz\t%0, %1": "=r"(ret):"r"(x));
	ret = 32 - ret;
	return ret;
}

#define __fls(x) (fls(x) - 1)
#define ffs(x) ({ unsigned long __t = (x); fls(__t & -__t); })
#define __ffs(x) (ffs(x) - 1)
#define ffz(x) __ffs( ~(x) )

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define DIV_ROUND(n,d)		(((n) + ((d)/2)) / (d))

/* Simple shorthand for a section definition */
#ifndef __section
# define __section(S) __attribute__ ((__section__(#S)))
#endif

#define __init0	__section(.rodata.regu.init0)
#define __init1	__section(.rodata.regu.init1)
#define __init2	__section(.rodata.regu.init2)




struct regulator_regs {
	int typ; /* BIT4: default on/off(0: off, 1: on); BIT0~BIT3: dcdc/ldo type(0: ldo; 2: dcdc) */
	unsigned long pd_set;
	u32 pd_set_bit;
	unsigned long vol_trm;
	u32 vol_trm_bits;
	//u32 min_mV, step_uV;
	u32 vol_def;
	u32 vol_sel_cnt, vol_sel[];
};

struct regulator_desc {
	const char *name;
	int typ; /* BIT4: default on/off(0: off, 1: on); BIT0~BIT3: dcdc/ldo type(0: ldo; 2: dcdc) */
	unsigned long pd_set;
	u32 pd_set_bit;
	unsigned long vol_trm;
	u32 vol_trm_bits;
	//u32 min_mV, step_uV;
	u32 vol_def;
	u32 vol_sel_cnt;
	u32 offset_mv;
	u32 step_uv;
};

#define REGU_VERIFY_DLY	(1000)	/*ms */

static struct regulator_desc sc2730_regs_desc[] = {
    {"vddcore", 0x12, ANA_REG_GLB_POWER_PD_SW, BIT_DCDC_CORE_PD,
	ANA_REG_GLB_DCDC_CORE_VOL, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8), 800, 2, 0, 3125},
    {"vddcpu", 0x12, ANA_REG_GLB_POWER_PD_SW, BIT_DCDC_CPU_PD,
	ANA_REG_GLB_DCDC_CPU_VOL, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8), 800, 2, 0, 3125},
    {"vddgpu", 0x12, ANA_REG_GLB_POWER_PD_SW, BIT_DCDC_GPU_PD,
	ANA_REG_GLB_DCDC_GPU_VOL, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8), 800, 2, 0, 3125},
    {"vddmodem", 0x12, ANA_REG_GLB_POWER_PD_SW, BIT_DCDC_MODEM_PD,
	ANA_REG_GLB_DCDC_MODEM_VOL, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8), 800, 2, 0, 3125},
    {"vddmem", 0x12, ANA_REG_GLB_POWER_PD_SW, BIT_DCDC_MEM_PD,
	ANA_REG_GLB_DCDC_MEM_VOL, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 1100, 2, 0, 6250},
    {"vddmemq", 0x12, ANA_REG_GLB_POWER_PD_SW, BIT_DCDC_MEMQ_PD,
	ANA_REG_GLB_DCDC_MEMQ_VOL, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8), 600, 2, 0, 3125},
    {"vddgen0", 0x12, ANA_REG_GLB_POWER_PD_SW, BIT_DCDC_GEN0_PD,
	ANA_REG_GLB_DCDC_GEN0_VOL, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 1875, 2, 20, 9375},
    {"vddgen1", 0x12, ANA_REG_GLB_POWER_PD_SW, BIT_DCDC_GEN1_PD,
	ANA_REG_GLB_DCDC_GEN1_VOL, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 1350, 2, 50, 6250},
    {"vddsram", 0x12, ANA_REG_GLB_POWER_PD_SW, BIT_DCDC_SRAM_PD,
	ANA_REG_GLB_DCDC_SRAM_VOL, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8), 800, 2, 0, 3125},
    {"avdd18", 0x10, ANA_REG_GLB_POWER_PD_SW, BIT_LDO_AVDD18_PD,
	ANA_REG_GLB_LDO_AVDD18_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5), 1775, 2, 1175, 10000},
    {"vddrf1v8", 0x10, ANA_REG_GLB_LDO_VDDRF1V8_REG0, BIT_LDO_VDDRF1V8_PD,
	ANA_REG_GLB_LDO_VDDRF1V8_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5), 1775, 2, 1175, 10000},
    {"vddwcn", 0x10, ANA_REG_GLB_LDO_VDDWCN_REG0, BIT_LDO_VDDWCN_PD,
	ANA_REG_GLB_LDO_VDDWCN_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5), 900, 2, 900, 15000},
    {"vddcamd1", 0x10, ANA_REG_GLB_LDO_VDDCAMD1_REG0, BIT_LDO_VDDCAMD1_PD,
	ANA_REG_GLB_LDO_VDDCAMD1_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4), 1050, 2, 900, 15000},
    {"vddcamd0", 0x10, ANA_REG_GLB_LDO_VDDCAMD0_REG0, BIT_LDO_VDDCAMD0_PD,
	ANA_REG_GLB_LDO_VDDCAMD0_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4), 1050, 2, 900, 15000},
    {"vddrf1v25", 0x10, ANA_REG_GLB_LDO_VRF1V25_REG0, BIT_LDO_VDDRF1V25_PD,
	ANA_REG_GLB_LDO_VDDRF1V25_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4), 1275, 2, 900, 15000},
    {"avdd12", 0x10, ANA_REG_GLB_LDO_AVDD12_REG0, BIT_LDO_AVDD12_PD,
	ANA_REG_GLB_LDO_AVDD12_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4), 1200, 2, 900, 15000},
    {"vddcama0", 0x10, ANA_REG_GLB_LDO_VDDCAMA0_REG0, BIT_LDO_VDDCAMA0_PD,
	ANA_REG_GLB_LDO_VDDCAMA0_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 2800, 2, 1200, 10000},
    {"vddcama1", 0x10, ANA_REG_GLB_LDO_VDDCAMA1_REG0, BIT_LDO_VDDCAMA1_PD,
	ANA_REG_GLB_LDO_VDDCAMA1_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 2800, 2, 1200, 10000},
    {"vddcammot", 0x10, ANA_REG_GLB_LDO_VDDCAMMOT_REG0, BIT_LDO_VDDCAMMOT_PD,
	ANA_REG_GLB_LDO_VDDCAMMOT_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 3000, 2, 1200, 10000},
    {"vddsim0", 0x10, ANA_REG_GLB_LDO_VDDSIM0_REG0, BIT_LDO_VDDSIM0_PD,
	ANA_REG_GLB_LDO_VDDSIM0_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 3000, 2, 1200, 10000},
    {"vddsim1", 0x10, ANA_REG_GLB_LDO_VDDSIM1_REG0, BIT_LDO_VDDSIM1_PD,
	ANA_REG_GLB_LDO_VDDSIM1_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 3000, 2, 1200, 10000},
    {"vddsim2", 0x10, ANA_REG_GLB_LDO_VDDSIM2_REG0, BIT_LDO_VDDSIM2_PD,
	ANA_REG_GLB_LDO_VDDSIM2_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 3000, 2, 1200, 10000},
    {"vddemmccore", 0x10, ANA_REG_GLB_LDO_VDDEMMCCORE_REG0, BIT_LDO_VDDEMMCCORE_PD,
	ANA_REG_GLB_LDO_VDDEMMCCORE_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 3000, 2, 1200, 10000},
    {"vddsdcore", 0x10, ANA_REG_GLB_LDO_VDDSDCORE_REG0, BIT_LDO_VDDSDCORE_PD,
	ANA_REG_GLB_LDO_VDDSDCORE_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 3000, 2, 1200, 10000},
    {"vddsdio", 0x10, ANA_REG_GLB_LDO_VDDSDIO_REG0, BIT_LDO_VDDSDIO_PD,
	ANA_REG_GLB_LDO_VDDSDIO_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 3000, 2, 1200, 10000},
    {"vdd28", 0x10, ANA_REG_GLB_POWER_PD_SW, BIT_LDO_VDD28_PD,
	ANA_REG_GLB_LDO_VDD28_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 2800, 2, 1200, 10000},
    {"vddwifipa", 0x10, ANA_REG_GLB_LDO_VDDWIFIPA_REG0, BIT_LDO_VDDWIFIPA_PD,
	ANA_REG_GLB_LDO_VDDWIFIPA_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 3300, 2, 1200, 10000},
    {"vdddcxo", 0x10, ANA_REG_GLB_POWER_PD_SW, BIT_LDO_VDD18_DCXO_PD,
	ANA_REG_GLB_LDO_VDD18_DCXO_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 1800, 2, 1200, 10000},
    {"vddusb33", 0x10, ANA_REG_GLB_LDO_VDDUSB33_REG0, BIT_LDO_VDDUSB33_PD,
	ANA_REG_GLB_LDO_VDDUSB33_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 3300, 2, 1200, 10000},
    {"vddldo0", 0x10, ANA_REG_GLB_LDO_VDDLDO0_REG0, BIT_LDO_VDDLDO0_PD,
	ANA_REG_GLB_LDO_VDDLDO0_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 2800, 2, 1200, 10000},
    {"vddldo1", 0x10, ANA_REG_GLB_LDO_VDDLDO1_REG0, BIT_LDO_VDDLDO1_PD,
	ANA_REG_GLB_LDO_VDDLDO1_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 1800, 2, 1200, 10000},
    {"vddldo2", 0x10, ANA_REG_GLB_LDO_VDDLDO2_REG0, BIT_LDO_VDDLDO2_PD,
	ANA_REG_GLB_LDO_VDDLDO2_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), 3300, 2, 1200, 10000},
#ifdef CONFIG_ADIE_UMP518
    {"vddldo3", 0x10, ANA_REG_GLB_LDO_VBAT_REG1, BIT_LDO_VDDLDO3_PD,
	ANA_REG_GLB_LDO_VBAT_REG1 , BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9), 3300, 2, 1200, 10000},
#endif
};

static struct {
	int id;
	const char* name;
	int efuse_block_id;
	int efuse_bit_mask;
	int pmic_regs_addr;
	int pmic_regs_mask;
} reinit_node[] = {
	{0, "DCDC_CPU_ZCD", IB_TRIM_BLOCK, IB_TRIM_MASK, ANA_REG_GLB_DCDC_CPU_REG2, BITS_DCDC_CPU_ZCD(0x3)},
	{1, "DCDC_GPU_ZCD", IB_TRIM_BLOCK, IB_TRIM_MASK, ANA_REG_GLB_DCDC_GPU_REG2, BITS_DCDC_GPU_ZCD(0x3)},
	{2, "DCDC_CORE_ZCD", IB_TRIM_BLOCK, IB_TRIM_MASK, ANA_REG_GLB_DCDC_CORE_REG2, BITS_DCDC_CORE_ZCD(0x3)},
	{3, "DCDC_MODEM_ZCD", IB_TRIM_BLOCK, IB_TRIM_MASK, ANA_REG_GLB_DCDC_MODEM_REG2, BITS_DCDC_MODEM_ZCD(0x3)},
	{4, "DCDC_MEM_ZCD", IB_TRIM_BLOCK, IB_TRIM_MASK, ANA_REG_GLB_DCDC_MEM_REG2, BITS_DCDC_MEM_ZCD(0x3)},
	{5, "DCDC_MEMQ_ZCD", IB_TRIM_BLOCK, IB_TRIM_MASK, ANA_REG_GLB_DCDC_MEMQ_REG2, BITS_DCDC_MEMQ_ZCD(0x3)},
	{6, "DCDC_GEN0_ZCD", IB_TRIM_BLOCK, IB_TRIM_MASK, ANA_REG_GLB_DCDC_GEN0_REG2, BITS_DCDC_GEN0_ZCD(0x3)},
	{7, "DCDC_GEN1_ZCD", IB_TRIM_BLOCK, IB_TRIM_MASK, ANA_REG_GLB_DCDC_GEN1_REG2, BITS_DCDC_GEN1_ZCD(0x3)},
	{8, "DCDC_WPA_ZCD", IB_TRIM_BLOCK, IB_TRIM_MASK, ANA_REG_GLB_DCDC_WPA_REG3, BITS_DCDC_WPA_ZCD(0x3)},
	{9, "DCDC_SRAM_ZCD", IB_TRIM_BLOCK, IB_TRIM_MASK, ANA_REG_GLB_DCDC_SRAM_REG2, BITS_DCDC_SRAM_ZCD(0x3)},
	{10, "LDO_DCXO_LP", VDD18_REFTRIM_LP_BLOCK, VDD18_REFTRIM_LP_MASK, ANA_REG_GLB_LDO_VDD18_DCXO_REG2, BITS_LDO_VDD18_DCXO_REFTRIM_LP(0xF)},
	{-1, NULL, 0, 0, 0, 0},
};
static struct {
	int id;
	const char* name;
	int value;
	int pmic_regs_addr;
	int pmic_regs_mask;
} reinit_node_cfg[] = {
	{0, "DCDC_CPU_DTM", 0x2, ANA_REG_GLB_DCDC_CPU_REG0, BITS_DCDC_CPU_DEADTIME(0x3)},
	{1, "DCDC_GPU_DTM", 0x2, ANA_REG_GLB_DCDC_GPU_REG0, BITS_DCDC_GPU_DEADTIME(0x3)},
	{2, "DCDC_CORE_DTM", 0x2, ANA_REG_GLB_DCDC_CORE_REG0, BITS_DCDC_CORE_DEADTIME(0x3)},
	{3, "DCDC_MODEM_DTM", 0x2, ANA_REG_GLB_DCDC_MODEM_REG0, BITS_DCDC_MODEM_DEADTIME(0x3)},
	{4, "DCDC_MEM_DTM", 0x2, ANA_REG_GLB_DCDC_MEM_REG0, BITS_DCDC_MEM_DEADTIME(0x3)},
	{5, "DCDC_MEMQ_DTM", 0x2, ANA_REG_GLB_DCDC_MEMQ_REG0, BITS_DCDC_MEMQ_DEADTIME(0x3)},
	{6, "DCDC_GEN0_DTM", 0x2, ANA_REG_GLB_DCDC_GEN0_REG0, BITS_DCDC_GEN0_DEADTIME(0x3)},
	{7, "DCDC_GEN1_DTM", 0x2, ANA_REG_GLB_DCDC_GEN1_REG0, BITS_DCDC_GEN1_DEADTIME(0x3)},
	{8, "DCDC_SRAM_DTM", 0x2, ANA_REG_GLB_DCDC_SRAM_REG0, BITS_DCDC_SRAM_DEADTIME(0x3)},
	{9, "DCDC_WPA_DTM", 0x3, ANA_REG_GLB_DCDC_WPA_REG0, BITS_DCDC_WPA_DEADTIME(0x3)},
	{10, "DCDC_CPU_CF", 0x1, ANA_REG_GLB_DCDC_CPU_REG1, BITS_DCDC_CPU_CF(0x3)},
	{11, "DCDC_GPU_CF", 0x1, ANA_REG_GLB_DCDC_GPU_REG1, BITS_DCDC_GPU_CF(0x3)},
	{12, "DCDC_MODEM_CF", 0x1, ANA_REG_GLB_DCDC_MODEM_REG1, BITS_DCDC_MODEM_CF(0x3)},
	{13, "DCDC_WPA_CF", 0x1, ANA_REG_GLB_DCDC_WPA_REG0, BITS_DCDC_WPA_CF(0x3)},
	{14, "DCDC_CPU_STBOP", 0x2DF, ANA_REG_GLB_DCDC_CPU_REG2, GENMASK(12,0)},
	{15, "DCDC_GPU_STBOP", 0x2DF, ANA_REG_GLB_DCDC_GPU_REG2, GENMASK(12,0)},
	{16, "DCDC_CORE_STBOP", 0x2D8, ANA_REG_GLB_DCDC_CORE_REG2, GENMASK(12,0)},
	{17, "DCDC_MODEM_STBOP", 0xD9F, ANA_REG_GLB_DCDC_MODEM_REG2, GENMASK(12,0)},
	{18, "DCDC_SRAM_STBOP", 0x2D8, ANA_REG_GLB_DCDC_SRAM_REG2, GENMASK(12,0)},
	{-1, NULL, 0, 0, 0},
};

static int dcdc_get_voltage(struct regulator_desc *desc)
{
	u32 mv = 0;
	int cal = 0; /* uV */
	int i = 0;

	if (desc->vol_trm && desc->vol_sel_cnt == 2) {
		int shft_trm = __ffs(desc->vol_trm_bits);
		u32 trim =
		    (ANA_REG_GET(desc->vol_trm) & desc->vol_trm_bits) >> shft_trm;
		mv = desc->offset_mv + trim * desc->step_uv / 1000;

		dprintf(INFO, "%s %d +%dmv(trim %#x)\n", desc->name, desc->offset_mv, (mv - desc->offset_mv), trim);
	}
	return (mv + cal / 1000);
}

static int dcdc_set_voltage(struct regulator_desc *desc, int min_mV, int max_mV)
{
	int i = 0, mv = min_mV;
	/* dcdc calibration control bits (default 0) small adjust voltage: 100/32mv ~= 3.125mv */
	int shft_trm = __ffs(desc->vol_trm_bits);
	int shft_ctl = 0;
	int step = 0;
	int j = 0;


	if(desc->vol_sel_cnt == 2) {
		step = desc->step_uv;
		j = DIV_ROUND_UP((mv - desc->offset_mv) * 1000, step);
	}

	dprintf(INFO, "%s:set voltage:%d = %d + %dmv (trim=%d step=%duv);\n", desc->name,
		   mv, desc->offset_mv, mv - desc->offset_mv, j, step);

	if (desc->vol_trm) { /* small adjust first */
		if (j >= 0 && j <= (desc->vol_trm_bits >> shft_trm)) {
			ANA_REG_MSK_OR(desc->vol_trm, j << shft_trm, desc->vol_trm_bits);
		}
	}

	return 0;
}

static int ldo_get_voltage(struct regulator_desc *desc)
{
	u32 vol;

	if (desc->vol_trm && desc->vol_sel_cnt == 2) {
		int shft = __ffs(desc->vol_trm_bits);
		u32 trim =
		    (ANA_REG_GET(desc->vol_trm) & desc->vol_trm_bits) >> shft;
		vol = desc->offset_mv * 1000 + trim * desc->step_uv;
		vol /= 1000;

		dprintf(INFO, "%s:get voltage %dmv(trim %#x)\n", desc->name, vol, trim);

		return vol;
	}

	return -1;
}

static int ldo_set_trimming(struct regulator_desc *desc, int def_vol, int to_vol, int adc_vol)
{
	int ret = -1;

	if (desc->vol_sel_cnt == 2) {
		/* ctl_vol = vol_base + reg[vol_trm] * vol_step  */
		int shft = __ffs(desc->vol_trm_bits);
		int ctl_vol = (to_vol - (adc_vol - def_vol));
		u32 trim = 0;

		if(adc_vol > def_vol)
			trim = DIV_ROUND_UP((ctl_vol - desc->offset_mv) * 1000, desc->step_uv);
		else
			trim = ((ctl_vol - desc->offset_mv) * 1000 / desc->step_uv);

		dprintf(INFO, "%s:set voltage:%d = %d + %dmv (trim=%d step=%duv);\n", desc->name,
				ctl_vol, desc->offset_mv, ctl_vol - desc->offset_mv, trim, desc->step_uv);

		if (trim <= (desc->vol_trm_bits >> shft)) {
			ANA_REG_MSK_OR(desc->vol_trm,
					trim << shft,
					desc->vol_trm_bits);
			ret = 0;
		}
	}

	return ret;
}

struct regulator_desc *regulator_get(void/*struct device*/ *dev, const char *id)
{
	unsigned int i;

	if(!id)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(sc2730_regs_desc); i++) {
		if (!strcmp(id, sc2730_regs_desc[i].name))
			return &sc2730_regs_desc[i];
	}

	return NULL;
}

int regulator_disable_all(void)
{
	ANA_REG_OR(ANA_REG_GLB_POWER_PD_SW, 0x7fff);
	ANA_REG_OR(ANA_REG_GLB_POWER_PD_HW, 0x1);
	return 0;
}

int regulator_enable_all(void)
{
	ANA_REG_BIC(ANA_REG_GLB_POWER_PD_HW, 0x1);
	return 0;
}

int regulator_disable(const char con_id[])
{
	struct regulator_desc *desc = regulator_get(0, con_id);

	if (desc == NULL)
		return -1;

	ANA_REG_OR(desc->pd_set, desc->pd_set_bit);

	return 0;
}

int regulator_enable(const char con_id[])
{
	struct regulator_desc *desc = regulator_get(0, con_id);

	if (desc == NULL)
		return -1;

	ANA_REG_BIC(desc->pd_set, desc->pd_set_bit);

	return 0;
}

int regulator_set_voltage(const char con_id[], int to_vol)
{
	int ret = 0;
	struct regulator_desc *desc = regulator_get(0, con_id);

	if (desc == NULL)
		return -1;

	int vdd_type = desc->typ & (BIT(4) - 1);

	if (vdd_type == 2 /*VDD_TYP_DCDC*/) {
		ret = dcdc_set_voltage(desc, to_vol, 0);
	} else if (vdd_type == 0 /*VDD_TYP_LDO*/) {
		ret = ldo_set_trimming(desc, 0, to_vol, 0);
	}

	return ret;
}

static int reload_regulator_config(void)
{
	int i,shft,ib_trim_val,reload_val;
	u32 efuse_val;

	for(i = 0; reinit_node_cfg[i].id != -1; i++){
		shft = __ffs(reinit_node_cfg[i].pmic_regs_mask);
		ANA_REG_MSK_OR(reinit_node_cfg[i].pmic_regs_addr,
				reinit_node_cfg[i].value << shft,
				reinit_node_cfg[i].pmic_regs_mask);
	}

	efuse_val = sprd_pmic_efuse_read(reinit_node[0].efuse_block_id);
	shft = __ffs(reinit_node[0].efuse_bit_mask);
	ib_trim_val = (efuse_val & reinit_node[0].efuse_bit_mask)
		>> shft;
	shft = __ffs(reinit_node[i].pmic_regs_mask);

	if(ib_trim_val >= IB_ZCD_THRESHOLD_MAX)
		reload_val = 2;
	else if(ib_trim_val <= IB_ZCD_THRESHOLD_MIN)
		reload_val = 0;
	else
		reload_val = 1;

	for(i = 0; reinit_node[i].id <= 9; i++)
		ANA_REG_MSK_OR(reinit_node[i].pmic_regs_addr,
				reload_val << shft,
				reinit_node[i].pmic_regs_mask);

	for(i = 10; reinit_node[i].id != -1; i++) {
		efuse_val = sprd_pmic_efuse_read(reinit_node[i].efuse_block_id);
		shft = __ffs(reinit_node[i].efuse_bit_mask);
		reload_val = (efuse_val & reinit_node[i].efuse_bit_mask)
			>> shft;
		shft = __ffs(reinit_node[i].pmic_regs_mask);
		ANA_REG_MSK_OR(reinit_node[i].pmic_regs_addr,
						reload_val << shft,
						reinit_node[i].pmic_regs_mask);
	}

	return 0;
}

void reset_cbank_value(void)
{
	u16 chip_id_low;
	chip_id_low = sci_adi_read(ANA_REG_GLB_CHIP_ID_LOW) & 0xffff;

	if(chip_id_low <= 0xb000)
		ANA_REG_SET(ANA_REG_GLB_TSX_CTRL11, BITS_DCXO_CORE_CBANK_LP(CBANK_VALUE));
}

int regulator_pmic_init(void)
{
	ANA_REG_SET(ANA_REG_GLB_PWR_WR_PROT_VALUE, BITS_PWR_WR_PROT_VALUE(WR_UNLOCK));
	ANA_REG_SET(ANA_REG_GLB_TSX_WR_PROT_VALUE, BITS_TSX_WR_PROT_VALUE(TSX_WR_UNLOCK));
	ANA_REG_SET(ANA_REG_GLB_TSX_CTRL12, BITS_DCXO_32K_FRAC_DIV_RATIO_CTRL_HP(DIV_COEF));
	ANA_REG_OR(ANA_REG_GLB_TSX_CTRL0, BITS_DCXO_26M_REF_OUT_EN(TRANS_26M_EN));
	reload_regulator_config();
	reset_cbank_value();
	return 0;
}
