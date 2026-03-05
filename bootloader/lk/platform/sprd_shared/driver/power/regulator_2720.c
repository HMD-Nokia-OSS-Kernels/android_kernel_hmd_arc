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

#define WR_UNLOCK 0x6e7f

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

struct regulator_desc {
	const char *name;
	int typ; /* BIT4: default on/off(0: off, 1: on); BIT0~BIT3: dcdc/ldo type(0: ldo; 2: dcdc) */
	unsigned long pd_set;
	u32 pd_set_bit;
	unsigned long vol_trm;
	u32 vol_trm_bits;
	unsigned long cal_ctl;
	u32 cal_ctl_bits;
	//u32 min_mV, step_uV;
	u32 vol_def;
	unsigned long vol_ctl;
	u32 vol_ctl_bits;
	u32 vol_sel_cnt;
	u32 offset_mv;
	u32 step_uv;
};

#define REGU_VERIFY_DLY	(1000)	/*ms */

static struct regulator_desc sc2720_regs_desc[] = {
	{ "vddcore", 0x12, ANA_REG_GLB_POWER_PD_SW, BIT_DCDC_CORE_PD,
	ANA_REG_GLB_DCDC_CORE_VOL, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8), ANA_REG_GLB_DCDC_CH_CTRL, BIT(13)|BIT(14)|BIT(16)|BIT(18)|BIT(19),
	900, 0, 0, 2, 0, 3125},
	{ "vddgen", 0x12, ANA_REG_GLB_POWER_PD_SW, BIT_DCDC_GEN_PD,
	ANA_REG_GLB_DCDC_GEN_VOL, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7), ANA_REG_GLB_DCDC_CH_CTRL, BIT(13)|BIT(14)|BIT(16)|BIT(18)|BIT(19),
	1850, 0, 0, 2, 1300, 12500},
	{ "vddwpa", 0x2, ANA_REG_GLB_DCDC_WPA_REG2, BIT_PD_BUCK_VPA,
	ANA_REG_GLB_DCDC_WPA_VOL, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6), 0, BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(18)|BIT(19),
	3400, 0, 0, 2, 400, 25000},
	{ "avdd18", 0x10, ANA_REG_GLB_POWER_PD_SW, BIT_LDO_AVDD18_PD,
	ANA_REG_GLB_LDO_AVDD18_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5), ANA_REG_GLB_DCDC_CH_CTRL, BIT(8)|BIT(10)|BIT(17)|BIT(18)|BIT(20),
	1800, 0, 0, 2, 1400, 12500},
	{ "vddcamio", 0x0, ANA_REG_GLB_LDO_CAMIO_REG0, BIT_LDO_CAMIO_PD,
	ANA_REG_GLB_LDO_CAMIO_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5), ANA_REG_GLB_DCDC_CH_CTRL, BIT(8)|BIT(10)|BIT(17)|BIT(18)|BIT(20),
	1800, 0, 0, 2, 1400, 12500},
	{ "vddrf18a", 0x10, ANA_REG_GLB_LDO_RF18A_REG0, BIT_LDO_RF18A_PD,
	ANA_REG_GLB_LDO_RF18A_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5), ANA_REG_GLB_DCDC_CH_CTRL, BIT(8)|BIT(10)|BIT(17)|BIT(18)|BIT(20),
	1800, 0, 0, 2, 1400, 12500},
	{ "vddrf18b", 0x00, ANA_REG_GLB_LDO_RF18B_REG0, BIT_LDO_RF18B_PD,
	ANA_REG_GLB_LDO_RF18B_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5), ANA_REG_GLB_DCDC_CH_CTRL, BIT(8)|BIT(10)|BIT(17)|BIT(18)|BIT(20),
	1800, 0, 0, 2, 1400, 12500},
	{ "vddcamd", 0x0, ANA_REG_GLB_LDO_CAMD_REG0, BIT_LDO_CAMD_PD,
	ANA_REG_GLB_LDO_CAMD_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5), ANA_REG_GLB_DCDC_CH_CTRL, BIT(8)|BIT(10)|BIT(17)|BIT(18)|BIT(20),
	1200, 0, 0, 2, 800, 12500},
	{ "vddcon", 0x0, ANA_REG_GLB_LDO_CON_REG0, BIT_LDO_CON_PD,
	ANA_REG_GLB_LDO_CON_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5), ANA_REG_GLB_DCDC_CH_CTRL, BIT(8)|BIT(10)|BIT(17)|BIT(18)|BIT(20),
	1200, 0, 0, 2, 800, 12500},
	{ "vddmem", 0x10, ANA_REG_GLB_POWER_PD_SW, BIT_LDO_MEM_PD,
	ANA_REG_GLB_LDO_MEM_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5), ANA_REG_GLB_DCDC_CH_CTRL, BIT(8)|BIT(10)|BIT(17)|BIT(18)|BIT(20),
	1200, 0, 0, 2, 800, 12500},
	{ "vddsim0", 0x0, ANA_REG_GLB_LDO_SIM0_PD_REG, BIT_LDO_SIM0_PD,
	ANA_REG_GLB_LDO_SIM0_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6), ANA_REG_GLB_DCDC_CH_CTRL, BIT(8)|BIT(10)|BIT(17)|BIT(18)|BIT(20),
	3000, 0, 0, 2, 1612, 12500},
	{ "vddsim1", 0x0, ANA_REG_GLB_LDO_SIM1_PD_REG, BIT_LDO_SIM1_PD,
	ANA_REG_GLB_LDO_SIM1_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6), ANA_REG_GLB_DCDC_CH_CTRL, BIT(8)|BIT(10)|BIT(17)|BIT(18)|BIT(20),
	3000, 0, 0, 2, 1612, 12500},
	{ "vddsim2", 0x0, ANA_REG_GLB_LDO_SIM2_PD_REG, BIT_LDO_SIM2_PD,
	ANA_REG_GLB_LDO_SIM2_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6), ANA_REG_GLB_DCDC_CH_CTRL, BIT(8)|BIT(10)|BIT(17)|BIT(18)|BIT(20),
	3000, 0, 0, 2, 1612, 12500},
	{ "vddcama", 0x0, ANA_REG_GLB_LDO_CAMA_REG0, BIT_LDO_CAMA_PD,
	ANA_REG_GLB_LDO_CAMA_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6), ANA_REG_GLB_DCDC_CH_CTRL, BIT(9)|BIT(17)|BIT(18)|BIT(20),
	2800, 0, 0, 2, 1612, 12500},
	{ "vddcammot", 0x0, ANA_REG_GLB_LDO_CAMMOT_REG0, BIT_LDO_CAMMOT_PD,
	ANA_REG_GLB_LDO_CAMMOT_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6), ANA_REG_GLB_DCDC_CH_CTRL, BIT(9)|BIT(17)|BIT(18)|BIT(20),
	3000, 0, 0, 2, 2000, 12500},
	{ "vddemmccore", 0x10, ANA_REG_GLB_LDO_EMMCCORE_PD_REG, BIT_LDO_EMMCCORE_PD,
	ANA_REG_GLB_LDO_EMMCCORE_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6), ANA_REG_GLB_DCDC_CH_CTRL, BIT(8)|BIT(10)|BIT(17)|BIT(18)|BIT(20),
	3000, 0, 0, 2, 2000, 12500},
	{ "vddsdcore", 0x10, ANA_REG_GLB_LDO_SD_PD_REG, BIT_LDO_SDCORE_PD,
	ANA_REG_GLB_LDO_SD_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6), ANA_REG_GLB_DCDC_CH_CTRL, BIT(8)|BIT(10)|BIT(17)|BIT(18)|BIT(20),
	3000, 0, 0, 2, 2000, 12500},
	{ "vddsdio", 0x10, ANA_REG_GLB_LDO_SDIO_PD_REG, BIT_LDO_SDIO_PD,
	ANA_REG_GLB_LDO_SDIO_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6), ANA_REG_GLB_DCDC_CH_CTRL, BIT(8)|BIT(10)|BIT(17)|BIT(18)|BIT(20),
	3000, 0, 0, 2, 1612, 12500},
	{ "vdd28", 0x10, ANA_REG_GLB_POWER_PD_SW, BIT_LDO_VDD28_PD,
	ANA_REG_GLB_LDO_VDD28_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6), ANA_REG_GLB_DCDC_CH_CTRL, BIT(8)|BIT(10)|BIT(17)|BIT(18)|BIT(20),
	2800, 0, 0, 2, 1612, 12500},
	{ "vddwifipa", 0x0, ANA_REG_GLB_LDO_WIFIPA_REG0, BIT_LDO_WIFIPA_PD,
	ANA_REG_GLB_LDO_WIFIPA_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6), ANA_REG_GLB_DCDC_CH_CTRL, BIT(8)|BIT(10)|BIT(17)|BIT(18)|BIT(20),
	3300, 0, 0, 2, 2100, 12500},
	{ "vdddcxo", 0x10, ANA_REG_GLB_POWER_PD_SW, BIT_LDO_DCXO_PD,
	ANA_REG_GLB_LDO_DCXO_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6), ANA_REG_GLB_DCDC_CH_CTRL, BIT(8)|BIT(10)|BIT(17)|BIT(18)|BIT(20),
	1800, 0, 0, 2, 1500, 12500},
	{ "vddusb33", 0x10, ANA_REG_GLB_LDO_USB_PD_REG, BIT_LDO_USB33_PD,
	ANA_REG_GLB_LDO_USB_REG1, BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6), ANA_REG_GLB_DCDC_CH_CTRL, BIT(8)|BIT(10)|BIT(17)|BIT(18)|BIT(20),
	3300, 0, 0, 2, 2100, 12500}
};

static struct {
	int id;
	const char* name;
	int efuse_block_id;
	int efuse_bit_mask;
	int pmic_regs_addr;
	int pmic_regs_mask;
} reinit_node[] = {
	{0,"DCDC_3M",		0,	GENMASK(11,8),	ANA_REG_GLB_DCDC_CLK_REG0,	BITS_OSC3M_FREQ(0xF)},
	{1,"DCDC_CORE_PFM",	15,	GENMASK(1,0),	ANA_REG_GLB_DCDC_CORE_REG0,	BITS_PFM_VH_VCORE(0x3)},
	{2,"DCDC_GEN_PFM",	15,	GENMASK(3,2),	ANA_REG_GLB_DCDC_GEN_REG0,	BITS_PFM_VH_VGEN(0x3)},
	{3,"DCDC_WPA_PFM",	15,	GENMASK(5,4),	ANA_REG_GLB_DCDC_WPA_REG0,	BITS_PFM_THRESHOLD_VPA(0x3)},
	{4,"RESERVED_RTC",	6,	GENMASK(14,10),	ANA_REG_GLB_RESERVED_REG_RTC,	GENMASK(4,0)},
	{-1, NULL, 0, 0, 0, 0},
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

	for (i = 0; i < ARRAY_SIZE(sc2720_regs_desc); i++) {
		if (!strcmp(id, sc2720_regs_desc[i].name))
		{
			return &sc2720_regs_desc[i];
		}
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
	int i,shft,reload_val;
	u32 efuse_val;

	for(i = 0; reinit_node[i].id != -1; i++) {
		efuse_val = sprd_pmic_efuse_read(reinit_node[i].efuse_block_id);
		shft = __ffs(reinit_node[i].efuse_bit_mask);
		reload_val = (efuse_val & reinit_node[i].efuse_bit_mask)
			>> shft;
		shft = __ffs(reinit_node[i].pmic_regs_mask);
		ANA_REG_MSK_OR(reinit_node[i].pmic_regs_addr,
			       reload_val << shft,
			       reinit_node[i].pmic_regs_mask);
	}
/*
 * reset DCDC_CORE PFM/PWM for pmic issue workaround
 * pmic efuse block15[0] 1'.
 */
	efuse_val = sprd_pmic_efuse_read(15);
	if( efuse_val & BIT(0)) {
		ANA_REG_SET(ANA_REG_GLB_DCDC_CORE_REG0, 0x1443);
	}
	return 0;
}

int regulator_pmic_init(void)
{
	ANA_REG_SET(ANA_REG_GLB_PWR_WR_PROT_VALUE, BITS_PWR_WR_PROT_VALUE(WR_UNLOCK));
	reload_regulator_config();
	return 0;
}
