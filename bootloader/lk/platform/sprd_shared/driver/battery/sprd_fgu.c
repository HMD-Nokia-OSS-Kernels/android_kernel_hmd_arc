/*
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2016-2023 Unisoc (Shanghai) Technologies Co. Ltd

 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

#include <linux/kernel.h>
#include <regs_adi.h>
#include "adi_hal_internal.h"
#include <asm/arch/sprd_reg.h>
#include <asm/arch/sprd_eic.h>
#include <sprd_battery.h>
#include "sprd_chg_helper.h"
#include <lk/debug.h>
#include <delay.h>

bool charge_first_power_on;
extern u32 sprd_pmic_efuse_read_bits(int bit_index, int length);

#define REGS_FGU_BASE			ANA_FGU_BASE
#if defined(CONFIG_ADIE_UMP518)
#define REG_FGU_START	                SCI_ADDR(REGS_FGU_BASE, 0x0000)
#define REG_FGU_CONFIG                  SCI_ADDR(REGS_FGU_BASE, 0x0004)
#define REG_FGU_ADC_CTRL1               SCI_ADDR(REGS_FGU_BASE, 0x000c)
#define REG_FGU_STATUS                  SCI_ADDR(REGS_FGU_BASE, 0x0014)
#define REG_FGU_INT_EN                  SCI_ADDR(REGS_FGU_BASE, 0x0018)
#define REG_FGU_INT_CLR                 SCI_ADDR(REGS_FGU_BASE, 0x001c)
#define REG_FGU_INT_RAW                 SCI_ADDR(REGS_FGU_BASE, 0x0020)
#define REG_FGU_VOLT_VAL                SCI_ADDR(REGS_FGU_BASE, 0x0028)
#define REG_FGU_OCV_VAL                 SCI_ADDR(REGS_FGU_BASE, 0x002c)
#define REG_FGU_POCV_VAL                SCI_ADDR(REGS_FGU_BASE, 0x0030)
#define REG_FGU_CURT_VAL_H              SCI_ADDR(REGS_FGU_BASE, 0x0034)
#define REG_FGU_CURT_VAL_L              SCI_ADDR(REGS_FGU_BASE, 0x0038)
#define REG_FGU_CURT_OFFSET_H           SCI_ADDR(REGS_FGU_BASE, 0x0084)
#define REG_FGU_CURT_OFFSET_L           SCI_ADDR(REGS_FGU_BASE, 0x0088)
#define REG_FGU_USER_AREA_SET           SCI_ADDR(REGS_FGU_BASE, 0x008c)
#define REG_FGU_USER_AREA_CLEAR         SCI_ADDR(REGS_FGU_BASE, 0x0090)
#define REG_FGU_USER_AREA_STATUS        SCI_ADDR(REGS_FGU_BASE, 0x0094)
#else
#define REG_FGU_START                   SCI_ADDR(REGS_FGU_BASE, 0x0000)
#define REG_FGU_CONFIG                  SCI_ADDR(REGS_FGU_BASE, 0x0004)
#define REG_FGU_INT_EN                  SCI_ADDR(REGS_FGU_BASE, 0x0010)
#define REG_FGU_INT_CLR                 SCI_ADDR(REGS_FGU_BASE, 0x0014)
#define REG_FGU_INT_RAW                 SCI_ADDR(REGS_FGU_BASE, 0x0018)
#define REG_FGU_VOLT_VAL                SCI_ADDR(REGS_FGU_BASE, 0x0020)
#define REG_FGU_OCV_VAL                 SCI_ADDR(REGS_FGU_BASE, 0x0024)
#define REG_FGU_POCV_VAL                SCI_ADDR(REGS_FGU_BASE, 0x0028)
#define REG_FGU_CURT_VAL                SCI_ADDR(REGS_FGU_BASE, 0x002c)
#define REG_FGU_CURT_OFFSET             SCI_ADDR(REGS_FGU_BASE, 0x0090)
#define REG_FGU_USER_AREA_SET           SCI_ADDR(REGS_FGU_BASE, 0x00A0)
#define REG_FGU_USER_AREA_CLEAR         SCI_ADDR(REGS_FGU_BASE, 0x00A4)
#define REG_FGU_USER_AREA_STATUS        SCI_ADDR(REGS_FGU_BASE, 0x00A8)
#endif

#define BIT_FGU_DISABLE_EN		( BIT(11) )
#define BIT_CLK_SEL_FGU                 ( BIT(9) )
#define BIT_VOL_READY_INT               ( BIT(6) )
#define BIT_FGU_RESET                   ( BIT(1) )
#define BIT_WRITE_ACTIVE_STS		( BIT(0) )
#define BITS_CLK_SEL_512K		0x3E0

#define SPRD_FGU_RTC_MODE_BIT		0xf000
#define SPRD_FGU_RTC_MODE_SHIFT		12
#define SPRD_FGU_RTC_CAP_BIT		0xfff
#define SPRD_FGU_RTC_CAP_SHIFT		0
#define FIRST_POWERON_MODE_VALUE	0xf
#define FIRST_POWERON_CAP_VALUE		0xfff

#define SPRDBAT_FGUADC_CAL_NO		0
#define SPRDBAT_FGUADC_CAL_NV		1
#define SPRDBAT_FGUADC_CAL_CHIP		2
#define VOL_READY_INT_THRESHOLD		1000
#define WAIT_WRITE_ACTIVE_RETRY_CNT	3

#define SPRD_FGU_IDEAL_RESISTANCE	20000
#define SPRD_FGU_CUR_BASIC_ADC		8192
#define BITSINDEX(b, o, w)		((b) * (w) + (o))

#if defined(CONFIG_ADIE_UMP518)
#define SPRD_FGU_CUR_ZERO_POINT		65536
#define SPRD_FGU_CUR_CODE_LSB		1068
#define SPRD_FGU_RESIST_MOHM		2
#endif

#define mdelay(_ms)			udelay(_ms*1000)

static int fgu_nv_4200mv = 2752;
static int fgu_nv_3600mv = 2374;

struct sprdfgu_cal {
	int vol_1000mv_adc;
	int cur_1000ma_adc;
	int vol_offset;
	int cur_offset;
	int cal_type;
};

#ifndef CALIB_RESISTANCE_MICRO_OHMS
#define CALIB_RESISTANCE_MICRO_OHMS	0
#endif

static struct sprdfgu_cal fgu_cal =
	{ 2872, 0, 0, 0, SPRDBAT_FGUADC_CAL_NO};

static u32 sprdfgu_adc2vol_mv(s64 adc)
{
	return ((adc + fgu_cal.vol_offset) * 1000)
		/ fgu_cal.vol_1000mv_adc;
}

#if defined(CONFIG_ADIE_UMP518)
static int sprdfgu_adc2cur_ma(s64 adc)
{
	return (adc / 15 - SPRD_FGU_CUR_ZERO_POINT) *
		SPRD_FGU_CUR_CODE_LSB / SPRD_FGU_RESIST_MOHM / 1000;
}
#else
static int sprdfgu_adc2cur_ma(s64 adc)
{
	if (fgu_cal.cur_1000ma_adc == 0)
		return -1;

	return ((adc + fgu_cal.cur_offset) * 1000)
		/ fgu_cal.cur_1000ma_adc;
}
#endif

static int sprdfgu_reg_get(unsigned long reg)
{
	int old_value = sci_adi_read(reg);
	int new_value, times = 1000;

	while ((old_value != (new_value = sci_adi_read(reg))) &&
		(times > 0)) {
		old_value = new_value;
		times--;
	}
	return new_value;
}

uint32_t sprdfgu_read_vbat_vol(void)
{
	int cur_vol_raw;
	uint32_t temp;

	cur_vol_raw = sprdfgu_reg_get(REG_FGU_VOLT_VAL);
	temp = sprdfgu_adc2vol_mv(cur_vol_raw);
	return temp;
}

#if defined(CONFIG_ADIE_UMP518)
int sprdfgu_read_ibat_cur(void)
{
	int cur_adc_l, cur_adc_h, cur_adc;
	int temp;

	cur_adc_h = sprdfgu_reg_get(REG_FGU_CURT_VAL_H);
	cur_adc_l = sprdfgu_reg_get(REG_FGU_CURT_VAL_L);
	cur_adc = (cur_adc_h << 16) | cur_adc_l;

	temp = sprdfgu_adc2cur_ma(cur_adc);
	return temp;
}
#else
int sprdfgu_read_ibat_cur(void)
{
	int cur_now_raw, temp = -1;

	if (CALIB_RESISTANCE_MICRO_OHMS == 0) {
		dprintf(ALWAYS,"sprd_fgu: error!!! calib resist is not defined!!!\n");
		return temp;
	}

	cur_now_raw = sprdfgu_reg_get(REG_FGU_CURT_VAL)-SPRD_FGU_CUR_BASIC_ADC;
	temp = sprdfgu_adc2cur_ma(cur_now_raw);
	return temp;
}
#endif

#if defined(CONFIG_ADIE_UMP518)
static void sprdfgu_switch_512k_clk(void)
{
	int cnt = WAIT_WRITE_ACTIVE_RETRY_CNT;

	sci_adi_write(REG_FGU_CONFIG, BIT_FGU_DISABLE_EN, BIT_FGU_DISABLE_EN);

	while ((sci_adi_read(REG_FGU_STATUS) & BIT_WRITE_ACTIVE_STS) && cnt--){
		udelay(50);
	}

	if (cnt <= 0)
		dprintf(ALWAYS,"sprd_chg: error!!! failed get fgu status\n!");

	sci_adi_write(ANA_REG_GLB_RTC_CLK_EN, 0, BIT_RTC_FGU_EN);
	sci_adi_write(REG_FGU_ADC_CTRL1, 0, BITS_CLK_SEL_512K);
	sci_adi_write(ANA_REG_GLB_RTC_CLK_EN, BIT_RTC_FGU_EN, BIT_RTC_FGU_EN);
	sci_adi_write(REG_FGU_START, BIT_FGU_RESET, BIT_FGU_RESET);
	udelay(100);
	sci_adi_write(REG_FGU_CONFIG, 0, BIT_FGU_DISABLE_EN);
	/* switch the 512kHZ clk is complete, need to wait for a voltage
	 * sample period (delay_time > 250ms) before reading the voltage.
	 */
	mdelay(252);
}
#endif

static int sprdfgu_cal_get(unsigned int *p_cal_data)
{
	unsigned int data;

#if defined(CONFIG_ADIE_SC2723)
	data = sprd_pmic_efuse_read_bits(BITSINDEX(12, 0, 8), 9);
	dprintf(INFO,"sprd_chg: fgu_cal_get 4.2 data data: 0x%x\n", data);
	p_cal_data[0] = (data + 6963) - 4096 - 256;

	return 0;

#elif defined(CONFIG_ADIE_SC2731) || defined(CONFIG_ADIE_SC2721)|| defined(CONFIG_ADIE_SC2720) || defined(CONFIG_ADIE_SC2730)
	data = sprd_pmic_efuse_read_bits(BITSINDEX(3, 0, 16), 9);
	dprintf(INFO,"sprd_chg: fgu_cal_get 4.2 data data: 0x%x\n", data);
	p_cal_data[0] = (data + 6963) - 4096 - 256;

	return 0;

#elif defined(CONFIG_ADIE_UMP9620)
	data = sprd_pmic_efuse_read_bits(BITSINDEX(38, 7, 16), 9);
	dprintf(INFO,"sprd_chg: ump9620 fgu_cal_get 4.2 data data: 0x%x\n", data);
	p_cal_data[0] = (data + 6963) - 4096 - 256;

	return 0;

#elif defined(CONFIG_ADIE_UMP518)
	data = sprd_pmic_efuse_read_bits(BITSINDEX(39, 7, 16), 9);
	dprintf(INFO,"sprd_chg: ump518 fgu_cal_get 4.2 data data: %#x\n", data);
	p_cal_data[0] = (data + 6963) - 4096 - 256;

	return 0;

#else
	data = sprd_pmic_efuse_read_bits(BITSINDEX(3, 0, 16), 9);
	dprintf(INFO,"sprd_chg: fgu_cal_get 4.2 data data: 0x%x\n", data);
	p_cal_data[0] = (data + 6963) - 4096 - 256;

	return 0;
#endif
}

static void sprdfgu_cal_init(void)
{
	if (fgu_nv_3600mv == 0) {
		fgu_cal.vol_1000mv_adc =
		    DIV_ROUND_CLOSEST((fgu_nv_4200mv) * 10, 42);
		fgu_cal.vol_offset = 0;
	} else {
		fgu_cal.vol_1000mv_adc =
		    DIV_ROUND_CLOSEST((fgu_nv_4200mv - fgu_nv_3600mv) * 10, 6);
		fgu_cal.vol_offset =
		    0 - (fgu_nv_4200mv * 10 - fgu_cal.vol_1000mv_adc * 42) / 10;
	}
	fgu_cal.cur_1000ma_adc =
            DIV_ROUND_CLOSEST(fgu_cal.vol_1000mv_adc * 4 * CALIB_RESISTANCE_MICRO_OHMS,
		      SPRD_FGU_IDEAL_RESISTANCE);

	dprintf(INFO,"sprd_chg: 4200mv=%d, 3600mv=%d\n", fgu_nv_4200mv, fgu_nv_3600mv);
	dprintf(INFO,"sprd_chg: fgu_cal.vol_1000mv_adc=%d, vol_offset=%d\n",
		fgu_cal.vol_1000mv_adc, fgu_cal.vol_offset);
}

static int sprdfgu_cal_from_chip(void)
{
	unsigned int fgu_data[4] = { 0 };

	if (sprdfgu_cal_get(fgu_data)) {
		dprintf(INFO,"sprd_chg: efuse no cal data\n");
		return 1;
	}

	dprintf(INFO,"sprd_chg: fgu_data: 0x%x\n", fgu_data[0]);

	fgu_nv_4200mv = fgu_data[0];
	fgu_nv_3600mv = 0;
	fgu_cal.cal_type = SPRDBAT_FGUADC_CAL_CHIP;
	dprintf(INFO,"sprd_chg: sprdfgu: one point\n");
	return 0;
}

void sprdfgu_init(void)
{
	int cnt = VOL_READY_INT_THRESHOLD;
	int ret;

	sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN, BIT_ANA_FGU_EN);
	/* Only sc2723 and sc2731 pmic needs to set
	 * BIT_RTC_FGUA_EN bit.
	 */
#if defined(CONFIG_ADIE_SC2723) || defined(CONFIG_ADIE_SC2731)
	sci_adi_set(ANA_REG_GLB_RTC_CLK_EN, BIT_RTC_FGU_EN | BIT_RTC_FGUA_EN);
#else
	sci_adi_set(ANA_REG_GLB_RTC_CLK_EN, BIT_RTC_FGU_EN);
#endif
	sci_adi_clr(REG_FGU_INT_EN, 0xFFFF);	//disable int after watchdog reset

	sprdfgu_cal_from_chip();
	sprdfgu_cal_init();

#if defined(CONFIG_ADIE_UMP518)
	sci_adi_write(REG_FGU_CURT_OFFSET_H, 0, ~0);
	sci_adi_write(REG_FGU_CURT_OFFSET_L, 0, ~0);	//init offset after watchdog reset
	/* 518 pmic fgu need to detect the present
	 * of battery voltage.
	 */
	while ((sprdfgu_read_vbat_vol() < 2000) && cnt--){
		mdelay(2);
	}

	if (cnt <= 0)
		dprintf(ALWAYS,"sprd_chg: error!!! ump518 fgu voltage = %dmV is not ready!\n",
			sprdfgu_read_vbat_vol());

	sprdfgu_switch_512k_clk();
#else
	sci_adi_write(REG_FGU_CURT_OFFSET, 0, ~0);	//init offset after watchdog reset
	/* When the symbol position BIT_VOL_READY_INT is 1,
	 * the FGU voltage can be picked up
	 */
	while (!(sci_adi_read(REG_FGU_INT_RAW) & BIT_VOL_READY_INT) && cnt--){
		mdelay(2);
	}

	if (cnt <= 0)
		dprintf(ALWAYS,"sprd_chg: error!!! fgu voltage not ready!\n");
#endif
}

static u32 sprdfgu_rtc_reg_read(u32 mask, int shift)
{
	return (sci_adi_read(REG_FGU_USER_AREA_STATUS) & mask) >> shift;
}

void sprdfgu_late_init(void)
{
	u32 mode, cap;

	mode = sprdfgu_rtc_reg_read(SPRD_FGU_RTC_MODE_BIT, SPRD_FGU_RTC_MODE_SHIFT);
	cap = sprdfgu_rtc_reg_read(SPRD_FGU_RTC_CAP_BIT, SPRD_FGU_RTC_CAP_SHIFT);

	if ((mode == FIRST_POWERON_MODE_VALUE) || (cap == FIRST_POWERON_CAP_VALUE)) {
		charge_first_power_on = true;
		dprintf(ALWAYS,"sprd_chg: charge first poweron reset, mode = %#x, cap = %#x\n", mode, cap);
		return;
	}

	dprintf(ALWAYS,"sprd_chg: mode = %#x, cap = %#x\n", mode, cap);
}
