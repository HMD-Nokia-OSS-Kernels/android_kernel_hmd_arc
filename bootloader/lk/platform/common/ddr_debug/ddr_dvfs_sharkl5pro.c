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

#define DDR_INIT_DEBUG_ADDR			0x3000
#define DRAM_INFO_ADDR				(DDR_INIT_DEBUG_ADDR + 0xc)
#define DDR_DFS_DEBUG_ADDR			0x3400
#define IRAM_DFS_SWITCH				(DDR_DFS_DEBUG_ADDR + 0x4)
#define DDR_REINIT_DEBUG_ADDR		0x3700
#define IRAM_LIGHTSLEEP_DIS_ADDR	(DDR_REINIT_DEBUG_ADDR + 0x4)
#define IRAM_DFS_HW_DIS_FREQ		(DDR_DFS_DEBUG_ADDR + 0x8)

typedef struct DDRC_FREQ_INFO
{
	u32 ddr_clk;
	u32 freq_sel;
	u32 ratio;
	u32 clk_mode;
	u32 ratio_d2;
	u32 half_freq_mode;
}DDRC_FREQ_INFO_T;

DDRC_FREQ_INFO_T ddrc_freq_info_lp4[]=
{	//ddr_clk ,freq_sel,ratio,clk_mode,ratio_d2,harf_freq_mode
	{256,	0,	0x44,	0x0,	0x9,	0x1},
	{384,	1,	0x45,	0x0,	0x9,	0x1},
	{512,	2,	0x44,	0x1,	0x4,	0x1},
	{768,	3,	0x43,	0x1,	0x0,	0x0},
	{1024,	4,	0x44,	0x1,	0x0,	0x0},
	{1333,	5,	0x08,	0x1,	0x0,	0x0},
	{1536,	6,	0x45,	0x1,	0x0,	0x0},
	{1866,	7,	0x08,	0x1,	0x0,	0x0},
};

static u32 g_cur_fn;
static DDRC_FREQ_INFO_T  *freq_info = ddrc_freq_info_lp4;

static void reg_bit_set(u32 addr, u32 start_bit, u32 bits_num, u32 val)
{
	u32 tmp_val,bit_msk = ((1<<bits_num)-1);
	tmp_val = readl(addr);
	tmp_val &= ~(bit_msk << start_bit);
	tmp_val |= ((val & bit_msk)<< start_bit);
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

	return (readl(REG_PMU_APB_PUB_ACC_RDY)&(0x1));
}

void dpll_cfg(u32 fn)
{
	u32 nint_val=0, kint_val=0, dpll_icp = 0;
	/***set div_s***/
	reg_bit_set(REG_ANLG_PHY_G0_ANALOG_DPLL_TOP_DPLL_CTRL0, 0,1, 0x1);

	/***set sdm_en***/
	reg_bit_set(REG_ANLG_PHY_G0_ANALOG_DPLL_TOP_DPLL_CTRL0, 1,1, 0x1);
	switch(fn)
	{
		case 7:
			nint_val=0x47; kint_val=0x627627; dpll_icp=0x5; break;
		case 5:
			nint_val=0x33; kint_val=0x227627; dpll_icp=0x3; break;
	}
	/***set nint and kint***/
	reg_bit_set(REG_ANLG_PHY_G0_ANALOG_DPLL_TOP_DPLL_CTRL2, 23, 7, nint_val);
	reg_bit_set(REG_ANLG_PHY_G0_ANALOG_DPLL_TOP_DPLL_CTRL2, 0, 23, kint_val);
	/***set dpll_icp***/
	reg_bit_set(REG_ANLG_PHY_G0_ANALOG_DPLL_TOP_DPLL_CTRL0, 4, 3, dpll_icp);
	udelay(300);
}

int dmc_freq_sel_search(u32 ddr_clk,u32* fn)
{
	switch (ddr_clk)
		{
			case 1866:*fn=0x7;break;
			case 1600:
			case 1536:*fn=0x6;break;
			case 1333:*fn=0x5;break;
			case 1024:*fn=0x4;break;
			case 768:*fn=0x3;break;
			case 512:*fn=0x2;break;
			case 384:*fn=0x1;break;
			case 256:*fn=0x0;break;
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
		case 7:  //1866
			target_vol = 900;break;
		case 6:  //1536/1600
			target_vol = 900;break;
		case 5:  //1333
			target_vol = 800;break;
		default:
			target_vol = 750;break;
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
	reg_bit_set(REG_PUB_APB_DFS_SW_CTRL, 8, 7,(freq_info+fn)->ratio);//pub_dfs_sw_ratio
	//pub_dfs_sw_clk_mode 00:pure bypass mode 01:deskew-pll mode 11:deskew-dll mode
	reg_bit_set(REG_PUB_APB_DFS_SW_CTRL1,16, 2,(freq_info+fn)->clk_mode);
	//pub_dfs_sw_ratio_d2 bit[1:0]:clk_x1_d2 select, 0:div0, 1:div2, 2:div4 bit[3:2]:clk_d2 select 0:div0, 1:div2, 2:div4
	reg_bit_set(REG_PUB_APB_DFS_SW_CTRL1, 0, 4,(freq_info+fn)->ratio_d2);
	reg_bit_set(REG_PUB_APB_DFS_SW_CTRL, 4, 3,fn);//pub_dfs_sw_frq_sel

	if (get_freq_vol(fn) > get_freq_vol(g_cur_fn)) {
		if (regulator_set_voltage("vddcore", get_freq_vol(fn))) {
			errorf("voltage set error\n");
			udelay(50);
		}
	}

	reg_bit_set(REG_PUB_APB_DFS_SW_CTRL, 1, 1,0x1);//pub_dfs_sw_req
	while(((readl(REG_PUB_APB_DFS_SW_CTRL)>>2)&0x1) == 0x0);//pub_dfs_sw_ack
	reg_bit_set(REG_PUB_APB_DFS_SW_CTRL, 1, 1,0x0);//pub_dfs_sw_req

	if ( get_freq_vol(fn) < get_freq_vol(g_cur_fn)) {
		if (regulator_set_voltage("vddcore", get_freq_vol(fn))) {
			errorf("voltage set error\n");
			udelay(50);
		}
	}

	ddr_lightsleep_open();
}

void sw_dvfs_go(u32 fn)
{

	g_cur_fn = (readl((SPRD_PUB_PHYS + 0x12c)) >>8 ) & 0x7;

	if (fn == g_cur_fn)
		return;

	if (fn == 7) {
		if (g_cur_fn != 4)
			__sw_dfs_go(4);
		dpll_cfg(7);
		__sw_dfs_go(fn);
	} else if (g_cur_fn == 7) {
		__sw_dfs_go(4);
		dpll_cfg(5);
		if (fn != 4)
			__sw_dfs_go(fn);
	} else {
		__sw_dfs_go(fn);
	}
}

int ddr_boot_freq_voltage_set(u32 vol)
{
	return regulator_set_voltage("vddcore", vol);
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
