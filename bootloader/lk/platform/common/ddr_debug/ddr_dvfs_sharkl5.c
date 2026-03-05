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

#include <asm/arch/sprd_reg.h>
#include <asm/arch/common.h>
#include <sprd_common.h>
#include <sprd_regulator.h>
#include <sprd_ddr_dvfs.h>

#define DDR_INIT_DEBUG_ADDR		0x3000
#define DDR_TYPE_INFO_ADDR		(DDR_INIT_DEBUG_ADDR + 0x8)
#define DRAM_INFO_ADDR			(DDR_INIT_DEBUG_ADDR + 0xc)
#define DRAM_TOP_FREQ			(DDR_INIT_DEBUG_ADDR + 0x20)

#define DDR_DFS_DEBUG_ADDR		0x3400
#define IRAM_DFS_SWITCH			(DDR_DFS_DEBUG_ADDR + 0x4)
#define IRAM_DFS_HW_DIS_FREQ		(DDR_DFS_DEBUG_ADDR + 0x8)

#define DDR_REINIT_DEBUG_ADDR		0x3700
#define IRAM_LIGHTSLEEP_DIS_ADDR	(DDR_REINIT_DEBUG_ADDR + 0x4)

typedef struct DDRC_FREQ_INFO
{
	u32 ddr_clk;
	u32 freq_sel;
	u32 ratio;
	u32 clk_mode;
	u32 ratio_d2;
	u32 half_freq_mode;
}DDRC_FREQ_INFO_T;

DDRC_FREQ_INFO_T ddrc_freq_info[]=
{	//ddr_clk ,freq_sel,ratio,clk_mode,ratio_d2,harf_freq_mode
	{256,	0,	0x44,	0x0,	0x9,	0x1},
	{384,	1,	0x45,	0x0,	0x9,	0x1},
	{512,	2,	0x44,	0x1,	0x4,	0x1},
	{622,	3,	0x28,	0x1,	0x0,	0x0},
	{768,	4,	0x43,	0x1,	0x0,	0x0},
	{933,	5,	0x18,	0x1,	0x0,	0x0},
	{1200,	6,	0x08,	0x1,	0x0,	0x0},//only lp4/4x, either 1200 or 1333 controll by top_frq
	{1333,	7,	0x08,	0x1,	0x0,	0x0},//only lp4/4x, either 1200 or 1333 controll by top_frq
};

static u32 g_cur_fn;
static DDRC_FREQ_INFO_T *freq_info = ddrc_freq_info;

static void reg_bit_set(u32 addr, u32 start_bit, u32 bits_num, u32 val)
{
	u32 tmp_val,bit_msk = ((1<<bits_num)-1);
	tmp_val = readl(addr);
	tmp_val &= ~(bit_msk << start_bit);
	tmp_val |= ((val & bit_msk) << start_bit);
	writel(tmp_val, addr);
}

static void ddr_lightsleep_open(void)
{
	reg_bit_set(REG_PMU_APB_DDR_SLEEP_CTRL, 17, 1, 0);
	writel(0xffff, REG_PMU_APB_PUB_SYS_AUTO_LIGHT_SLEEP_ENABLE);
}

static u32 ddr_lightsleep_shut(void)
{
	writel(0, REG_PMU_APB_PUB_SYS_AUTO_LIGHT_SLEEP_ENABLE);
	reg_bit_set(REG_PMU_APB_DDR_SLEEP_CTRL, 17, 1, 1);
	udelay(10);

	return (readl(REG_PMU_APB_PUB_ACC_RDY) & (0x1));
}

void dpll_cfg(u32 ddr_clk)
{
	u32 nint_val = 0, kint_val = 0;

	/*set div_s*/
	reg_bit_set(REG_ANLG_PHY_G0_ANALOG_DPLL_TOP_DPLL_CTRL0, 0, 1, 0x1);
	/*set sdm_en*/
	reg_bit_set(REG_ANLG_PHY_G0_ANALOG_DPLL_TOP_DPLL_CTRL0, 1, 1, 0x1);

	switch(ddr_clk)/* 256/384/512/768 use twpll */
	{
		case 1866://for 622/933
			nint_val = 0x47; kint_val = 0x627627; break;
		case 1200://for 1200
			nint_val = 0x2e; kint_val = 0x13b13b; break;
		case 1333://for 1333
			nint_val = 0x33; kint_val = 0x227627; break;
	}
	/***set nint and kint***/
	reg_bit_set(REG_ANLG_PHY_G0_ANALOG_DPLL_TOP_DPLL_CTRL2, 23, 7, nint_val);
	reg_bit_set(REG_ANLG_PHY_G0_ANALOG_DPLL_TOP_DPLL_CTRL2, 0, 23, kint_val);
	udelay(300);
}

int dmc_freq_sel_search(u32 ddr_clk, u32* fn)
{
	switch (ddr_clk)
	{
		case 1333: *fn = 0x7; break;//only lp4/4x
		case 1200: *fn = 0x6; break;//only lp4/4x
		case 933: *fn = 0x5; break;
		case 768: *fn = 0x4; break;
		case 622: *fn = 0x3; break;
		case 512: *fn = 0x2; break;
		case 384: *fn = 0x1; break;
		case 256: *fn = 0x0; break;
		default:
			return -1;
	}

	return 0;
}

u32 get_freq_vol(u32 fn)
{
	u32 target_vol;

	switch(fn)
	{
		case 7: //1333
			target_vol = 900; break;
		case 6: //1200
			target_vol = 800; break;
		default: //other
			target_vol = 750; break;
	}

	return target_vol;
}

static void __sw_dfs_go(u32 fn)
{
	if (!ddr_lightsleep_shut()) {
		ddr_lightsleep_open();
		return;
	}

	//pub_dfs_sw_ratio bit[2:0]000:DPLL0 010:26m 101:TWPLL-768m 110:TWPLL-1536M bit[6:3]:DPLL0 div value=N+1
	reg_bit_set(REG_PUB_APB_DFS_SW_CTRL, 8, 7, (freq_info+fn)->ratio);//pub_dfs_sw_ratio
	//pub_dfs_sw_clk_mode 00:pure bypass mode 01:deskew-pll mode 11:deskew-dll mode
	reg_bit_set(REG_PUB_APB_DFS_SW_CTRL1,16, 2, (freq_info+fn)->clk_mode);
	//pub_dfs_sw_ratio_d2 bit[1:0]:clk_x1_d2 select, 0:div0, 1:div2, 2:div4 bit[3:2]:clk_d2 select 0:div0, 1:div2, 2:div4
	reg_bit_set(REG_PUB_APB_DFS_SW_CTRL1, 0, 4, (freq_info+fn)->ratio_d2);
	reg_bit_set(REG_PUB_APB_DFS_SW_CTRL, 4, 3, fn);//pub_dfs_sw_frq_sel

	reg_bit_set(REG_PUB_APB_DFS_SW_CTRL, 1, 1,0x1);//pub_dfs_sw_req
	while(((readl(REG_PUB_APB_DFS_SW_CTRL) >> 2) & 0x1) == 0x0);//pub_dfs_sw_ack
	reg_bit_set(REG_PUB_APB_DFS_SW_CTRL, 1, 1, 0x0);//pub_dfs_sw_req

	ddr_lightsleep_open();
}

int ddr_boot_freq_voltage_set(u32 vol)
{
	return regulator_set_voltage("vddcore", vol);
}

void sw_dvfs_go(u32 fn)
{
	u32 target_ddr_clk, top_freq, dram_type;
	int ret;

	if (fn > 7)
		return;

	dram_type = readl(DDR_TYPE_INFO_ADDR);//3--lp3;4--lp4;5--lp4x
	g_cur_fn = (readl((SPRD_PUB_PHYS + 0x12c)) >> 8) & 0x7;
	target_ddr_clk = (freq_info + fn)->ddr_clk;
	top_freq = readl(DRAM_TOP_FREQ);/*lp3 max 933; lp4/4x max 1200 or 1333*/

	if (target_ddr_clk > top_freq) {
		target_ddr_clk = top_freq;
		ret = dmc_freq_sel_search(target_ddr_clk, &fn);
		if (ret < 0) {
			errorf("illegal ddr target freq: %dMZ, check it\n", target_ddr_clk);
			return;
		}
	}

	if (fn == g_cur_fn)
		return;

	if ((dram_type & 0xf) == 3) {//lpddr3--voltage & dpll not need processing
		__sw_dfs_go(fn);
		return;
	}

	/*lpddr4&lpddr4x*/
	if ((fn == 7) || (fn == 6)) {//can only use one of 1200 or 1333 ,so dfs to top_freq
		if (g_cur_fn < 6) {//low freq dfs to 1200 or 1333
			__sw_dfs_go(4);//dfs to 768 as interim (not use dpll)
			ddr_boot_freq_voltage_set(get_freq_vol(fn));//voltage adjust
			udelay(50);
			dpll_cfg(target_ddr_clk);
			__sw_dfs_go(fn);
		} else {//cur_fn 6/7 dfs to 7/6 need not process
			return;
		}
	} else {
		if (g_cur_fn >= 6) {//from 1200/1333 dfs to low freq
			__sw_dfs_go(4);//dfs to 768 as interim (not use dpll)
			ddr_boot_freq_voltage_set(get_freq_vol(fn));//voltage adjust
			udelay(50);
			dpll_cfg(1866);
			__sw_dfs_go(fn);
		} else {
			__sw_dfs_go(fn);
		}
	}
}

void ddr_paras_set_to_sp(u32 dvfs_dis, u32 retention_dis, u32* mask_mode)
{
	writel(dvfs_dis, IRAM_DFS_SWITCH);
	writel(retention_dis, IRAM_LIGHTSLEEP_DIS_ADDR);
	*mask_mode = (*mask_mode) | readl(IRAM_DFS_HW_DIS_FREQ);
	writel(*mask_mode, IRAM_DFS_HW_DIS_FREQ);
}

u8 get_dram_info(void)
{
	return readl(DRAM_INFO_ADDR) & 0xff;
}
