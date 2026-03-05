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
#include <sprd_regulator.h>
#include <sprd_ddr_dvfs.h>

#define DDR_INIT_DEBUG_ADDR			0x65018c00
#define DRAM_INFO_ADDR				(DDR_INIT_DEBUG_ADDR + 0xc)
#define DDR_DFS_DEBUG_ADDR			0x65018c80
#define IRAM_DFS_SWITCH				(DDR_DFS_DEBUG_ADDR + 0x4)
#define IRAM_DFS_HW_DIS_FREQ			(DDR_DFS_DEBUG_ADDR + 0x8)
#define IRAM_DVS_SWITCH				(DDR_DFS_DEBUG_ADDR + 0xc)

#define DDR_REINIT_DEBUG_ADDR			0x65018d00
#define IRAM_LIGHTSLEEP_DIS_ADDR		(DDR_REINIT_DEBUG_ADDR + 0x4)

#define REG_PUB_DVFS_SW_CTL			(SPRD_PUB_DVFS_APB_PHYS + 0x40)


#define SET_OFFSET 0x1000
#define CLR_OFFSET 0x2000

static void reg_bit_set(u32 addr, u32 start_bit, u32 bits_num, u32 val)
{
	u32 tmp_val,bit_msk = ((1<<bits_num)-1);
	tmp_val = readl(addr);
	tmp_val &= ~(bit_msk << start_bit);
	tmp_val |= ((val & bit_msk)<< start_bit);
	writel(tmp_val, addr);
}

static void reg_bit_set_clr_op(u32 addr,u32 start_bit,u32 bits_num,u32 val)
{
	u32 bit_msk = ((1 << bits_num) - 1) << start_bit;
	u32 set, clr;

	set = (val << start_bit) & bit_msk;
	clr = ~set & bit_msk;

	if (set)
		writel(set, (addr + SET_OFFSET));

	if (clr)
		writel(clr, (addr + CLR_OFFSET));
}

u32 get_freq_vol(u32 fn)
{
	u32 target_vol;

	switch(fn)
	{
		case 7:		//2133
		case 6:		//1866
		case 5:		//1536
			target_vol = 750;break;
		case 4:		//1244
		case 3:		//1066
		case 2:		//768
			target_vol = 700;break;
		case 1:		//533
			target_vol = 650;break;
		case 0:	 	//384
			target_vol = 600;break;
		default:
			target_vol = 750;break;
	}

	return target_vol;
}

int dmc_freq_sel_search(u32 ddr_clk, u32* fn)
{
	switch (ddr_clk) {
		case 2133: *fn = 0x7; break;
		case 1866: *fn = 0x6; break;
		case 1536: *fn = 0x5; break;
		case 1244: *fn = 0x4; break;
		case 1066: *fn = 0x3; break;
		case 768:  *fn = 0x2; break;
		case 533:  *fn = 0x1; break;
		case 384:  *fn = 0x0; break;
		default: return -1;
	}

	return 0;
}

void sw_dvfs_go(u32 fn)
{
	u32 sel;

	sel = (readl((SPRD_PUB_DMC_PHYS + 0x012c)) & 0x700) >> 8;
	if (fn == sel)
		return;

	//disable light
	reg_bit_set_clr_op(REG_PMU_APB_PUB_AUTO_LIGHT_SLEEP_ENABLE,0, 16, 0);
	//get lock
	reg_bit_set(REG_PUB_DVFS_SW_CTL,16,1,1);
	/* polling lock ack */
	while((readl(REG_PUB_DVFS_SW_CTL) & 0x40000) == 0);

	reg_bit_set(REG_PUB_DVFS_SW_CTL, 1, 3, fn);
	reg_bit_set(REG_PUB_DVFS_SW_CTL,0,1,1);

	/*polling dvfs ack*/
	while(0 == (readl(REG_PUB_DVFS_SW_CTL) & 0x10));
	reg_bit_set(REG_PUB_DVFS_SW_CTL,0,1,0);

	//must release lock, if not DDR can't access
	reg_bit_set(REG_PUB_DVFS_SW_CTL,16,1,0);

	//ebable light
	reg_bit_set_clr_op(REG_PMU_APB_PUB_AUTO_LIGHT_SLEEP_ENABLE,0, 16, 0xffff);

	return;
}

int ddr_boot_freq_voltage_set(u32 vol)
{
	return regulator_set_voltage("vddpub", vol);
}

void ddr_paras_set_to_sp(u32 dvfs_dis, u32 retention_dis, u32* mask_mode)
{
	writel(dvfs_dis, IRAM_DFS_SWITCH);
	writel(dvfs_dis, IRAM_DVS_SWITCH);
	writel(retention_dis, IRAM_LIGHTSLEEP_DIS_ADDR);
	*mask_mode = (*mask_mode) | readl(IRAM_DFS_HW_DIS_FREQ);
	writel(*mask_mode, IRAM_DFS_HW_DIS_FREQ);
}

u8 get_dram_info(void)
{
	return readl(DRAM_INFO_ADDR) & 0xff;
}
