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
#include <sprd_common.h>
#include <asm/arch/common.h>
#include <sprd_regulator.h>
#include <sprd_ddr_dvfs.h>

#define DRAM_INFO_ADDR				(SPRD_DDR_PHYS + 0x01b4)
#define REG_ADJS_WB				(SPRD_DDRPHY_CHN0_PHYS+0x0c)
#define REG_ADJS_WB_1			(SPRD_DDRPHY_CHN0_PHYS+0x10)
#define REG_CUR_FREQ			(SPRD_DDR_PHYS+0x12c)
/*sharkl3 BW mon not enable, use this reg to transfer ddr_mode to cm4 in uboot*/
#define REG_DFS_SWITCH			REG_PUB_QOSC_AHB_BWMON0_UP_WBW_SET
#define REG_DFS_HW_DIS_FREQ		REG_PUB_QOSC_AHB_BWMON0_UP_RBW_SET

static u32 g_dfs_table[8] = {160, 233, 311, 400, 533, 622, 800, 933};

static void reg_bit_set(u32 addr, u32 start_bit, u32 bits_num, u32 val)
{
	u32 tmp_val,bit_msk = ((1<<bits_num)-1);
	tmp_val = readl(addr);
	tmp_val &= ~(bit_msk << start_bit);
	tmp_val |= ((val & bit_msk)<< start_bit);
	writel(tmp_val, addr);
}

int dmc_freq_sel_search(u32 freq, u32 *fn)
{
	int i;
	for(i=0; i<8; i++)
	{
		if (freq == g_dfs_table[i]) {
			*fn = i;
			return 0;
		}
	}
	return -1;
}

static void get_dfs_para(u32 sel, u32* ratio, u32* clk_mode,
			 u32* ratio_d2, u32* ddl_adjs)
{
	u32 temp_adjs,temp_adjs_1;
	switch(sel)
	{
		case 0://160
			*ratio = 0x21;
			*clk_mode = 0x2;
			*ratio_d2 = 0x9;
			break;
		case 1://233
			*ratio = 0x18;
			*clk_mode = 0x2;
			*ratio_d2 = 0x9;
			break;
		case 2:// 311
			*ratio = 0x10;
			*clk_mode = 0x2;
			*ratio_d2 = 0x9;
			break;
		case 3:// 400
			*ratio = 0x9;
			*clk_mode = 0x2;
			*ratio_d2 = 0x9;
			break;
		case 4://533
			*ratio = 0x11;
			*clk_mode = 0x1;
			*ratio_d2 = 0x4;
			break;
		case 5://622
			*ratio = 0x10;
			*clk_mode = 0x1;
			*ratio_d2 = 0x4;
			break;
		case 6://800
			*ratio = 0x1;
			*clk_mode = 0x2;
			*ratio_d2 = 0xa;
			break;
		case 7://933
			*ratio = 0x18;
			*clk_mode = 0x1;
			*ratio_d2 = 0x0;
			break;
		default:
			break;
	}
	temp_adjs = readl(REG_ADJS_WB+0x50*sel);
	temp_adjs_1 = readl(REG_ADJS_WB_1+0x50*sel);
	*ddl_adjs = (((temp_adjs_1&0x4000)>>6) | (temp_adjs&0xff));
}

static void sw_dfs_mode(void)
{
	reg_bit_set(REG_PUB_AHB_WRAP_DFS_HW_CTRL,0,1,0);
	reg_bit_set(REG_PUB_TOP_AHB_DMC_CLK_INIT_CFG,0,1,1);
	reg_bit_set(REG_PUB_AHB_WRAP_DFS_PURE_SW_CTRL,0,1,1);
	reg_bit_set(REG_PUB_AHB_WRAP_DFS_SW_CTRL,0,1,1);
}

void sw_dvfs_go(u32 fn)
{
	u32 sw_ratio = 0;
	u32 sw_clk_mode = 0;
	u32 sw_ratio_d2 = 0;
	u32 sw_ddl_adjs = 0;

	if (fn == 0 || fn == 4) {
		errorf("not supprot this freq: %d\n", g_dfs_table[fn]);
		return;
	}

	if (fn == ((readl(REG_CUR_FREQ) & (0xF00)) >> 8))
		return;

	sw_dfs_mode();

	get_dfs_para(fn, &sw_ratio, &sw_clk_mode, &sw_ratio_d2, &sw_ddl_adjs);

	reg_bit_set(REG_PUB_AHB_WRAP_DFS_SW_CTRL, 8, 7, sw_ratio);
	reg_bit_set(REG_PUB_AHB_WRAP_DFS_SW_CTRL, 15, 7, sw_ratio);

	reg_bit_set(REG_PUB_AHB_WRAP_DFS_SW_CTRL1, 16, 2, sw_clk_mode);
	reg_bit_set(REG_PUB_AHB_WRAP_DFS_SW_CTRL1, 18, 2, sw_clk_mode);

	reg_bit_set(REG_PUB_AHB_WRAP_DFS_SW_CTRL2, 0, 9, sw_ddl_adjs);
	reg_bit_set(REG_PUB_AHB_WRAP_DFS_SW_CTRL2, 16, 9, sw_ddl_adjs);

	reg_bit_set(REG_PUB_AHB_WRAP_DFS_SW_CTRL1, 0, 4, sw_ratio_d2);
	reg_bit_set(REG_PUB_AHB_WRAP_DFS_SW_CTRL1, 8, 4, sw_ratio_d2);

	reg_bit_set(REG_PUB_AHB_WRAP_DFS_SW_CTRL, 4, 3, fn);

	reg_bit_set(REG_PUB_AHB_WRAP_DFS_SW_CTRL, 1, 1, 1);
	while(0 == (readl(REG_PUB_AHB_WRAP_DFS_SW_CTRL) & 0x4));
	reg_bit_set(REG_PUB_AHB_WRAP_DFS_SW_CTRL, 1, 1, 0);

	return;
}

u32 get_freq_vol(u32 fn)
{
	return 900;
}

int ddr_boot_freq_voltage_set(u32 vol)
{
	return regulator_set_voltage("vddcore", vol);
}

void ddr_paras_set_to_sp(u32 dvfs_dis, u32 retention_dis, u32* mask_mode)
{
	u32 dvfs_disable = 0, retention_disable = 0;

	if (dvfs_dis == 1)
		dvfs_disable = 0x5a;

	if (retention_dis == 1)
		retention_disable = 0x5a;

	writel(((retention_disable << 8) | dvfs_disable), REG_DFS_SWITCH);
	*mask_mode = (*mask_mode) | readl(REG_DFS_HW_DIS_FREQ);
	writel(*mask_mode, REG_DFS_HW_DIS_FREQ);
}

u8 get_dram_info(void)
{
	return readl(DRAM_INFO_ADDR) & 0xff;
}
