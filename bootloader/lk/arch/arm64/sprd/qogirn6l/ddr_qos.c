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

#define	REGU_OT_CTRL_EN			0x0
#define REGU_OT_CTRL_AW_CFG		0x4
#define REGU_OT_CTRL_AR_CFG		0x8
#define REGU_OT_CTRL_AX_CFG		0xc
#define REGU_LAT_EN			0x10
#define REGU_LAT_W_CFG			0x14
#define REGU_LAT_R_CFG			0x18
#define REGU_BW_NRT_EN			0x40
#define REGU_BW_NRT_W_CFG_0		0x44
#define	REGU_BW_NRT_W_CFG_1		0x48
#define REGU_BW_NRT_R_CFG_0		0x4c
#define REGU_BW_NRT_R_CFG_1		0x50
#define AXQOS_GEN_EN			0x60
#define AXQOS_GEN_CFG			0x64
#define URG_CNT_CFG			0x68

struct qos_cfg
{
	u32 addr;
	u32 start_bit;
	u32 bits;
	u32 value;
};

static void reg_bit_set(u32 addr, u32 start_bit, u32 bits_num, u32 val)
{
	u32 tmp_val, bit_mask = ((1 << bits_num) - 1);
	tmp_val = __raw_readl(addr);
	tmp_val &= ~(bit_mask << start_bit);
	tmp_val |= ((val & bit_mask) << start_bit);
	__raw_writel(tmp_val, addr);
}

struct qos_cfg pub_cpcpu_mtx_qos_cfg[] = {
	//nic400_pub_cpcpu_merge_mtx_m0_qos_list
	{0x60084000 + REGU_OT_CTRL_EN, 0, 1, 0x0},		//otctrl_en
	{0x60084000 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x0},		//urg_03_max_aw_ot
	{0x60084000 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x0},		//urg_03_aw_intvl_mode
	{0x60084000 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x0},		//urg_01_max_aw_ot
	{0x60084000 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60084000 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x0},		//urg_00_max_aw_ot
	{0x60084000 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60084000 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x0},		//urg_13_max_aw_ot
	{0x60084000 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60084000 + REGU_OT_CTRL_AR_CFG, 0, 6, 0x0},		//urg_03_max_ar_ot
	{0x60084000 + REGU_OT_CTRL_AR_CFG, 8, 6, 0x0},		//urg_01_max_ar_ot
	{0x60084000 + REGU_OT_CTRL_AR_CFG, 16, 6, 0x0},		//urg_00_max_ar_ot
	{0x60084000 + REGU_OT_CTRL_AR_CFG, 24, 6, 0x0},		//urg_13_max_ar_ot
	{0x60084000 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x0},		//urg_1x_max_aw_ot
	{0x60084000 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60084000 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x0},		//urg_3x_max_aw_ot
	{0x60084000 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60084000 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x0},		//urg_1x_max_ar_ot
	{0x60084000 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x0},		//urg_3x_max_ar_ot

	{0x60084000 + AXQOS_GEN_EN, 0, 1, 0x0},			//gen_en_w--enabel write urgent generate
	{0x60084000 + AXQOS_GEN_EN, 1, 1, 0x0},			//gen_en_r--enabel read urgent generate
	{0x60084000 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60084000 + AXQOS_GEN_CFG, 0, 4, 0x0},		//arqos_norm
	{0x60084000 + AXQOS_GEN_CFG, 4, 4, 0x0},		//arqos_high
	{0x60084000 + AXQOS_GEN_CFG, 8, 4, 0x0},		//arqos_ultra
	{0x60084000 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60084000 + AXQOS_GEN_CFG, 16, 4, 0x0},		//awqos_norm
	{0x60084000 + AXQOS_GEN_CFG, 20, 4, 0x0},		//awqos_high
	{0x60084000 + AXQOS_GEN_CFG, 24, 4, 0x0},		//awqos_ultra
	{0x60084000 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high
	{0x60084000 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60084000 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel


	//nic400_pub_cpcpu_merge_mtx_m1_qos_list
	{0x60084080 + REGU_OT_CTRL_EN, 0, 1, 0x0},		//otctrl_en
	{0x60084080 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x0},		//urg_03_max_aw_ot
	{0x60084080 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x0},		//urg_03_aw_intvl_mode
	{0x60084080 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x0},		//urg_01_max_aw_ot
	{0x60084080 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60084080 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x0},		//urg_00_max_aw_ot
	{0x60084080 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60084080 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x0},		//urg_13_max_aw_ot
	{0x60084080 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60084080 + REGU_OT_CTRL_AR_CFG, 0, 6, 0x0},		//urg_03_max_ar_ot
	{0x60084080 + REGU_OT_CTRL_AR_CFG, 8, 6, 0x0},		//urg_01_max_ar_ot
	{0x60084080 + REGU_OT_CTRL_AR_CFG, 16, 6, 0x0},		//urg_00_max_ar_ot
	{0x60084080 + REGU_OT_CTRL_AR_CFG, 24, 6, 0x0},		//urg_13_max_ar_ot
	{0x60084080 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x0},		//urg_1x_max_aw_ot
	{0x60084080 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60084080 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x0},		//urg_3x_max_aw_ot
	{0x60084080 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60084080 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x0},		//urg_1x_max_ar_ot
	{0x60084080 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x0},		//urg_3x_max_ar_ot

	{0x60084080 + AXQOS_GEN_EN, 0, 1, 0x0},			//gen_en_w--enabel write urgent generate
	{0x60084080 + AXQOS_GEN_EN, 1, 1, 0x0},			//gen_en_r--enabel read urgent generate
	{0x60084080 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60084080 + AXQOS_GEN_CFG, 0, 4, 0x0},		//arqos_norm
	{0x60084080 + AXQOS_GEN_CFG, 4, 4, 0x0},		//arqos_high
	{0x60084080 + AXQOS_GEN_CFG, 8, 4, 0x0},		//arqos_ultra
	{0x60084080 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60084080 + AXQOS_GEN_CFG, 16, 4, 0x0},		//awqos_norm
	{0x60084080 + AXQOS_GEN_CFG, 20, 4, 0x0},		//awqos_high
	{0x60084080 + AXQOS_GEN_CFG, 24, 4, 0x0},		//awqos_ultra
	{0x60084080 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high
	{0x60084080 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60084080 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel


	//nic400_pub_cpcpu_merge_mtx_m2_qos_list
	{0x60084100 + REGU_OT_CTRL_EN, 0, 1, 0x0},		//otctrl_en
	{0x60084100 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x0},		//urg_03_max_aw_ot
	{0x60084100 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x0},		//urg_03_aw_intvl_mode
	{0x60084100 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x0},		//urg_01_max_aw_ot
	{0x60084100 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60084100 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x0},		//urg_00_max_aw_ot
	{0x60084100 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60084100 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x0},		//urg_13_max_aw_ot
	{0x60084100 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60084100 + REGU_OT_CTRL_AR_CFG, 0, 6, 0x0},		//urg_03_max_ar_ot
	{0x60084100 + REGU_OT_CTRL_AR_CFG, 8, 6, 0x0},		//urg_01_max_ar_ot
	{0x60084100 + REGU_OT_CTRL_AR_CFG, 16, 6, 0x0},		//urg_00_max_ar_ot
	{0x60084100 + REGU_OT_CTRL_AR_CFG, 24, 6, 0x0},		//urg_13_max_ar_ot
	{0x60084100 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x0},		//urg_1x_max_aw_ot
	{0x60084100 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60084100 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x0},		//urg_3x_max_aw_ot
	{0x60084100 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60084100 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x0},		//urg_1x_max_ar_ot
	{0x60084100 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x0},		//urg_3x_max_ar_ot

	{0x60084100 + AXQOS_GEN_EN, 0, 1, 0x0},			//gen_en_w--enabel write urgent generate
	{0x60084100 + AXQOS_GEN_EN, 1, 1, 0x0},			//gen_en_r--enabel read urgent generate
	{0x60084100 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60084100 + AXQOS_GEN_CFG, 0, 4, 0x0},		//arqos_norm
	{0x60084100 + AXQOS_GEN_CFG, 4, 4, 0x0},		//arqos_high
	{0x60084100 + AXQOS_GEN_CFG, 8, 4, 0x0},		//arqos_ultra
	{0x60084100 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60084100 + AXQOS_GEN_CFG, 16, 4, 0x0},		//awqos_norm
	{0x60084100 + AXQOS_GEN_CFG, 20, 4, 0x0},		//awqos_high
	{0x60084100 + AXQOS_GEN_CFG, 24, 4, 0x0},		//awqos_ultra
	{0x60084100 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high
	{0x60084100 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60084100 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel
};

struct qos_cfg pub_dcam_dpu_mtx_qos_cfg[] = {
	//nic400_pub_dcam_dpu_merge_mtx_m0_qos_list
	{0x60080000 + REGU_OT_CTRL_EN, 0, 1, 0x0},		//otctrl_en
	{0x60080000 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x0},		//urg_03_max_aw_ot
	{0x60080000 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x0},		//urg_03_aw_intvl_mode
	{0x60080000 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x0},		//urg_01_max_aw_ot
	{0x60080000 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60080000 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x0},		//urg_00_max_aw_ot
	{0x60080000 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60080000 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x0},		//urg_13_max_aw_ot
	{0x60080000 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60080000 + REGU_OT_CTRL_AR_CFG, 0, 6, 0x0},		//urg_03_max_ar_ot
	{0x60080000 + REGU_OT_CTRL_AR_CFG, 8, 6, 0x0},		//urg_01_max_ar_ot
	{0x60080000 + REGU_OT_CTRL_AR_CFG, 16, 6, 0x0},		//urg_00_max_ar_ot
	{0x60080000 + REGU_OT_CTRL_AR_CFG, 24, 6, 0x0},		//urg_13_max_ar_ot
	{0x60080000 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x0},		//urg_1x_max_aw_ot
	{0x60080000 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60080000 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x0},		//urg_3x_max_aw_ot
	{0x60080000 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60080000 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x0},		//urg_1x_max_ar_ot
	{0x60080000 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x0},		//urg_3x_max_ar_ot

	{0x60080000 + AXQOS_GEN_EN, 0, 1, 0x0},			//gen_en_w--enabel write urgent generate
	{0x60080000 + AXQOS_GEN_EN, 1, 1, 0x0},			//gen_en_r--enabel read urgent generate
	{0x60080000 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60080000 + AXQOS_GEN_CFG, 0, 4, 0x0},		//arqos_norm
	{0x60080000 + AXQOS_GEN_CFG, 4, 4, 0x0},		//arqos_high
	{0x60080000 + AXQOS_GEN_CFG, 8, 4, 0x0},		//arqos_ultra
	{0x60080000 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60080000 + AXQOS_GEN_CFG, 16, 4, 0x0},		//awqos_norm
	{0x60080000 + AXQOS_GEN_CFG, 20, 4, 0x0},		//awqos_high
	{0x60080000 + AXQOS_GEN_CFG, 24, 4, 0x0},		//awqos_ultra
	{0x60080000 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high
	{0x60080000 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60080000 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel


	//nic400_pub_dcam_dpu_merge_mtx_m1_qos_list
	{0x60080080 + REGU_OT_CTRL_EN, 0, 1, 0x0},		//otctrl_en
	{0x60080080 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x0},		//urg_03_max_aw_ot
	{0x60080080 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x0},		//urg_03_aw_intvl_mode
	{0x60080080 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x0},		//urg_01_max_aw_ot
	{0x60080080 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60080080 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x0},		//urg_00_max_aw_ot
	{0x60080080 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60080080 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x0},		//urg_13_max_aw_ot
	{0x60080080 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60080080 + REGU_OT_CTRL_AR_CFG, 0, 6, 0x0},		//urg_03_max_ar_ot
	{0x60080080 + REGU_OT_CTRL_AR_CFG, 8, 6, 0x0},		//urg_01_max_ar_ot
	{0x60080080 + REGU_OT_CTRL_AR_CFG, 16, 6, 0x0},		//urg_00_max_ar_ot
	{0x60080080 + REGU_OT_CTRL_AR_CFG, 24, 6, 0x0},		//urg_13_max_ar_ot
	{0x60080080 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x0},		//urg_1x_max_aw_ot
	{0x60080080 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60080080 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x0},		//urg_3x_max_aw_ot
	{0x60080080 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60080080 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x0},		//urg_1x_max_ar_ot
	{0x60080080 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x0},		//urg_3x_max_ar_ot

	{0x60080080 + AXQOS_GEN_EN, 0, 1, 0x0},			//gen_en_w--enabel write urgent generate
	{0x60080080 + AXQOS_GEN_EN, 1, 1, 0x0},			//gen_en_r--enabel read urgent generate
	{0x60080080 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60080080 + AXQOS_GEN_CFG, 0, 4, 0x0},		//arqos_norm
	{0x60080080 + AXQOS_GEN_CFG, 4, 4, 0x0},		//arqos_high
	{0x60080080 + AXQOS_GEN_CFG, 8, 4, 0x0},		//arqos_ultra
	{0x60080080 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60080080 + AXQOS_GEN_CFG, 16, 4, 0x0},		//awqos_norm
	{0x60080080 + AXQOS_GEN_CFG, 20, 4, 0x0},		//awqos_high
	{0x60080080 + AXQOS_GEN_CFG, 24, 4, 0x0},		//awqos_ultra
	{0x60080080 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high
	{0x60080080 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60080080 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel
};

struct qos_cfg pub_ap_aon_ipa_mtx_qos_cfg[] = {
	//nic400_pub_ap_aon_ipa_mtx_m0_qos_list
	{0x60082000 + REGU_OT_CTRL_EN, 0, 1, 0x0},		//otctrl_en
	{0x60082000 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x0},		//urg_03_max_aw_ot
	{0x60082000 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x0},		//urg_03_aw_intvl_mode
	{0x60082000 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x0},		//urg_01_max_aw_ot
	{0x60082000 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60082000 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x0},		//urg_00_max_aw_ot
	{0x60082000 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60082000 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x0},		//urg_13_max_aw_ot
	{0x60082000 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60082000 + REGU_OT_CTRL_AR_CFG, 0, 6, 0x0},		//urg_03_max_ar_ot
	{0x60082000 + REGU_OT_CTRL_AR_CFG, 8, 6, 0x0},		//urg_01_max_ar_ot
	{0x60082000 + REGU_OT_CTRL_AR_CFG, 16, 6, 0x0},		//urg_00_max_ar_ot
	{0x60082000 + REGU_OT_CTRL_AR_CFG, 24, 6, 0x0},		//urg_13_max_ar_ot
	{0x60082000 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x0},		//urg_1x_max_aw_ot
	{0x60082000 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60082000 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x0},		//urg_3x_max_aw_ot
	{0x60082000 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60082000 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x0},		//urg_1x_max_ar_ot
	{0x60082000 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x0},		//urg_3x_max_ar_ot

	{0x60082000 + AXQOS_GEN_EN, 0, 1, 0x0},			//gen_en_w--enabel write urgent generate
	{0x60082000 + AXQOS_GEN_EN, 1, 1, 0x0},			//gen_en_r--enabel read urgent generate
	{0x60082000 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60082000 + AXQOS_GEN_CFG, 0, 4, 0x0},		//arqos_norm
	{0x60082000 + AXQOS_GEN_CFG, 4, 4, 0x0},		//arqos_high
	{0x60082000 + AXQOS_GEN_CFG, 8, 4, 0x0},		//arqos_ultra
	{0x60082000 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60082000 + AXQOS_GEN_CFG, 16, 4, 0x0},		//awqos_norm
	{0x60082000 + AXQOS_GEN_CFG, 20, 4, 0x0},		//awqos_high
	{0x60082000 + AXQOS_GEN_CFG, 24, 4, 0x0},		//awqos_ultra
	{0x60082000 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high
	{0x60082000 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60082000 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel


	//nic400_pub_ap_aon_ipa_mtx_m1_qos_list
	{0x60082080 + REGU_OT_CTRL_EN, 0, 1, 0x0},		//otctrl_en
	{0x60082080 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x0},		//urg_03_max_aw_ot
	{0x60082080 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x0},		//urg_03_aw_intvl_mode
	{0x60082080 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x0},		//urg_01_max_aw_ot
	{0x60082080 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60082080 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x0},		//urg_00_max_aw_ot
	{0x60082080 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60082080 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x0},		//urg_13_max_aw_ot
	{0x60082080 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60082080 + REGU_OT_CTRL_AR_CFG, 0, 6, 0x0},		//urg_03_max_ar_ot
	{0x60082080 + REGU_OT_CTRL_AR_CFG, 8, 6, 0x0},		//urg_01_max_ar_ot
	{0x60082080 + REGU_OT_CTRL_AR_CFG, 16, 6, 0x0},		//urg_00_max_ar_ot
	{0x60082080 + REGU_OT_CTRL_AR_CFG, 24, 6, 0x0},		//urg_13_max_ar_ot
	{0x60082080 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x0},		//urg_1x_max_aw_ot
	{0x60082080 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60082080 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x0},		//urg_3x_max_aw_ot
	{0x60082080 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60082080 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x0},		//urg_1x_max_ar_ot
	{0x60082080 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x0},		//urg_3x_max_ar_ot

	{0x60082080 + AXQOS_GEN_EN, 0, 1, 0x1},			//gen_en_w--enabel write urgent generate
	{0x60082080 + AXQOS_GEN_EN, 1, 1, 0x1},			//gen_en_r--enabel read urgent generate
	{0x60082080 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60082080 + AXQOS_GEN_CFG, 0, 4, 0x1},		//arqos_norm
	{0x60082080 + AXQOS_GEN_CFG, 4, 4, 0x1},		//arqos_high
	{0x60082080 + AXQOS_GEN_CFG, 8, 4, 0x1},		//arqos_ultra
	{0x60082080 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60082080 + AXQOS_GEN_CFG, 16, 4, 0x1},		//awqos_norm
	{0x60082080 + AXQOS_GEN_CFG, 20, 4, 0x1},		//awqos_high
	{0x60082080 + AXQOS_GEN_CFG, 24, 4, 0x1},		//awqos_ultra
	{0x60082080 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high
	{0x60082080 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60082080 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel


	//nic400_pub_ap_aon_ipa_mtx_m2_qos_list
	{0x60082100 + REGU_OT_CTRL_EN, 0, 1, 0x0},		//otctrl_en
	{0x60082100 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x0},		//urg_03_max_aw_ot
	{0x60082100 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x0},		//urg_03_aw_intvl_mode
	{0x60082100 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x0},		//urg_01_max_aw_ot
	{0x60082100 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60082100 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x0},		//urg_00_max_aw_ot
	{0x60082100 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60082100 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x0},		//urg_13_max_aw_ot
	{0x60082100 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60082100 + REGU_OT_CTRL_AR_CFG, 0, 6, 0x0},		//urg_03_max_ar_ot
	{0x60082100 + REGU_OT_CTRL_AR_CFG, 8, 6, 0x0},		//urg_01_max_ar_ot
	{0x60082100 + REGU_OT_CTRL_AR_CFG, 16, 6, 0x0},		//urg_00_max_ar_ot
	{0x60082100 + REGU_OT_CTRL_AR_CFG, 24, 6, 0x0},		//urg_13_max_ar_ot
	{0x60082100 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x0},		//urg_1x_max_aw_ot
	{0x60082100 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60082100 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x0},		//urg_3x_max_aw_ot
	{0x60082100 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60082100 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x0},		//urg_1x_max_ar_ot
	{0x60082100 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x0},		//urg_3x_max_ar_ot

	{0x60082100 + AXQOS_GEN_EN, 0, 1, 0x0},			//gen_en_w--enabel write urgent generate
	{0x60082100 + AXQOS_GEN_EN, 1, 1, 0x0},			//gen_en_r--enabel read urgent generate
	{0x60082100 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60082100 + AXQOS_GEN_CFG, 0, 4, 0x0},		//arqos_norm
	{0x60082100 + AXQOS_GEN_CFG, 4, 4, 0x0},		//arqos_high
	{0x60082100 + AXQOS_GEN_CFG, 8, 4, 0x0},		//arqos_ultra
	{0x60082100 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60082100 + AXQOS_GEN_CFG, 16, 4, 0x0},		//awqos_norm
	{0x60082100 + AXQOS_GEN_CFG, 20, 4, 0x0},		//awqos_high
	{0x60082100 + AXQOS_GEN_CFG, 24, 4, 0x0},		//awqos_ultra
	{0x60082100 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high

	{0x60082100 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60082100 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel
};

struct qos_cfg pub_mm_mtx_qos_cfg[] = {
	//nic400_pub_mm_merge_mtx_m0_qos_list
	{0x60081000 + REGU_OT_CTRL_EN, 0, 1, 0x1},		//otctrl_en
	{0x60081000 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x8},		//urg_03_max_aw_ot
	{0x60081000 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x0},		//urg_03_aw_intvl_mode
	{0x60081000 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x8},		//urg_01_max_aw_ot
	{0x60081000 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60081000 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x8},		//urg_00_max_aw_ot
	{0x60081000 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60081000 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x1},		//urg_13_max_aw_ot
	{0x60081000 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60081000 + REGU_OT_CTRL_AR_CFG, 0, 6, 0x7},		//urg_03_max_ar_ot
	{0x60081000 + REGU_OT_CTRL_AR_CFG, 8, 6, 0x7},		//urg_01_max_ar_ot
	{0x60081000 + REGU_OT_CTRL_AR_CFG, 16, 6, 0x10},	//urg_00_max_ar_ot
	{0x60081000 + REGU_OT_CTRL_AR_CFG, 24, 6, 0x10},	//urg_13_max_ar_ot
	{0x60081000 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x8},		//urg_1x_max_aw_ot
	{0x60081000 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60081000 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x8},		//urg_3x_max_aw_ot
	{0x60081000 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60081000 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x10},	//urg_1x_max_ar_ot
	{0x60081000 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x10},	//urg_3x_max_ar_ot

	{0x60081000 + AXQOS_GEN_EN, 0, 1, 0x0},			//gen_en_w--enabel write urgent generate
	{0x60081000 + AXQOS_GEN_EN, 1, 1, 0x0},			//gen_en_r--enabel read urgent generate
	{0x60081000 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60081000 + AXQOS_GEN_CFG, 0, 4, 0x0},		//arqos_norm
	{0x60081000 + AXQOS_GEN_CFG, 4, 4, 0x0},		//arqos_high
	{0x60081000 + AXQOS_GEN_CFG, 8, 4, 0x0},		//arqos_ultra
	{0x60081000 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60081000 + AXQOS_GEN_CFG, 16, 4, 0x0},		//awqos_norm
	{0x60081000 + AXQOS_GEN_CFG, 20, 4, 0x0},		//awqos_high
	{0x60081000 + AXQOS_GEN_CFG, 24, 4, 0x0},		//awqos_ultra
	{0x60081000 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high
	{0x60081000 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60081000 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel


	//nic400_pub_mm_merge_mtx_m1_qos_list
	{0x60081080 + REGU_OT_CTRL_EN, 0, 1, 0x1},		//otctrl_en
	{0x60081080 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x1},		//urg_03_max_aw_ot
	{0x60081080 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x0},		//urg_03_aw_intvl_mode
	{0x60081080 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x1},		//urg_01_max_aw_ot
	{0x60081080 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60081080 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x1},		//urg_00_max_aw_ot
	{0x60081080 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60081080 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x1},		//urg_13_max_aw_ot
	{0x60081080 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60081080 + REGU_OT_CTRL_AR_CFG, 0, 6, 0x8},		//urg_03_max_ar_ot
	{0x60081080 + REGU_OT_CTRL_AR_CFG, 8, 6, 0x10},		//urg_01_max_ar_ot
	{0x60081080 + REGU_OT_CTRL_AR_CFG, 16, 6, 0x10},	//urg_00_max_ar_ot
	{0x60081080 + REGU_OT_CTRL_AR_CFG, 24, 6, 0x10},	//urg_13_max_ar_ot
	{0x60081080 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x8},		//urg_1x_max_aw_ot
	{0x60081080 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60081080 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x8},		//urg_3x_max_aw_ot
	{0x60081080 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60081080 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x10},	//urg_1x_max_ar_ot
	{0x60081080 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x20},	//urg_3x_max_ar_ot

	{0x60081080 + AXQOS_GEN_EN, 0, 1, 0x0},			//gen_en_w--enabel write urgent generate
	{0x60081080 + AXQOS_GEN_EN, 1, 1, 0x0},			//gen_en_r--enabel read urgent generate
	{0x60081080 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60081080 + AXQOS_GEN_CFG, 0, 4, 0x0},		//arqos_norm
	{0x60081080 + AXQOS_GEN_CFG, 4, 4, 0x0},		//arqos_high
	{0x60081080 + AXQOS_GEN_CFG, 8, 4, 0x0},		//arqos_ultra
	{0x60081080 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60081080 + AXQOS_GEN_CFG, 16, 4, 0x0},		//awqos_norm
	{0x60081080 + AXQOS_GEN_CFG, 20, 4, 0x0},		//awqos_high
	{0x60081080 + AXQOS_GEN_CFG, 24, 4, 0x0},		//awqos_ultra
	{0x60081080 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high
	{0x60081080 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60081080 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel
};

struct qos_cfg cpacc_mtx_qos_cfg[] = {
	//nic400_cpacc_merge_mtx_m0_qos_list
	{0x60083000 + REGU_OT_CTRL_EN, 0, 1, 0x0},		//otctrl_en
	{0x60083000 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x0},		//urg_03_max_aw_ot
	{0x60083000 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x0},		//urg_03_aw_intvl_mode
	{0x60083000 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x0},		//urg_01_max_aw_ot
	{0x60083000 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60083000 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x0},		//urg_00_max_aw_ot
	{0x60083000 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60083000 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x0},		//urg_13_max_aw_ot
	{0x60083000 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60083000 + REGU_OT_CTRL_AR_CFG, 0, 6, 0x0},		//urg_03_max_ar_ot
	{0x60083000 + REGU_OT_CTRL_AR_CFG, 8, 6, 0x0},		//urg_01_max_ar_ot
	{0x60083000 + REGU_OT_CTRL_AR_CFG, 16, 6, 0x0},		//urg_00_max_ar_ot
	{0x60083000 + REGU_OT_CTRL_AR_CFG, 24, 6, 0x0},		//urg_13_max_ar_ot
	{0x60083000 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x0},		//urg_1x_max_aw_ot
	{0x60083000 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60083000 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x0},		//urg_3x_max_aw_ot
	{0x60083000 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60083000 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x0},		//urg_1x_max_ar_ot
	{0x60083000 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x0},		//urg_3x_max_ar_ot

	{0x60083000 + AXQOS_GEN_EN, 0, 1, 0x0},			//gen_en_w--enabel write urgent generate
	{0x60083000 + AXQOS_GEN_EN, 1, 1, 0x0},			//gen_en_r--enabel read urgent generate
	{0x60083000 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60083000 + AXQOS_GEN_CFG, 0, 4, 0x0},		//arqos_norm
	{0x60083000 + AXQOS_GEN_CFG, 4, 4, 0x0},		//arqos_high
	{0x60083000 + AXQOS_GEN_CFG, 8, 4, 0x0},		//arqos_ultra
	{0x60083000 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60083000 + AXQOS_GEN_CFG, 16, 4, 0x0},		//awqos_norm
	{0x60083000 + AXQOS_GEN_CFG, 20, 4, 0x0},		//awqos_high
	{0x60083000 + AXQOS_GEN_CFG, 24, 4, 0x0},		//awqos_ultra
	{0x60083000 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high
	{0x60083000 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60083000 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel


	//nic400_cpacc_merge_mtx_m1_qos_list
	{0x60083080 + REGU_OT_CTRL_EN, 0, 1, 0x0},		//otctrl_en
	{0x60083080 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x0},		//urg_03_max_aw_ot
	{0x60083080 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x0},		//urg_03_aw_intvl_mode
	{0x60083080 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x0},		//urg_01_max_aw_ot
	{0x60083080 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60083080 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x0},		//urg_00_max_aw_ot
	{0x60083080 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60083080 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x0},		//urg_13_max_aw_ot
	{0x60083080 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60083080 + REGU_OT_CTRL_AR_CFG, 0, 6, 0x0},		//urg_03_max_ar_ot
	{0x60083080 + REGU_OT_CTRL_AR_CFG, 8, 6, 0x0},		//urg_01_max_ar_ot
	{0x60083080 + REGU_OT_CTRL_AR_CFG, 16, 6, 0x0},		//urg_00_max_ar_ot
	{0x60083080 + REGU_OT_CTRL_AR_CFG, 24, 6, 0x0},		//urg_13_max_ar_ot
	{0x60083080 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x0},		//urg_1x_max_aw_ot
	{0x60083080 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60083080 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x0},		//urg_3x_max_aw_ot
	{0x60083080 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60083080 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x0},		//urg_1x_max_ar_ot
	{0x60083080 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x0},		//urg_3x_max_ar_ot

	{0x60083080 + AXQOS_GEN_EN, 0, 1, 0x0},			//gen_en_w--enabel write urgent generate
	{0x60083080 + AXQOS_GEN_EN, 1, 1, 0x0},			//gen_en_r--enabel read urgent generate
	{0x60083080 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60083080 + AXQOS_GEN_CFG, 0, 4, 0x0},		//arqos_norm
	{0x60083080 + AXQOS_GEN_CFG, 4, 4, 0x0},		//arqos_high
	{0x60083080 + AXQOS_GEN_CFG, 8, 4, 0x0},		//arqos_ultra
	{0x60083080 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60083080 + AXQOS_GEN_CFG, 16, 4, 0x0},		//awqos_norm
	{0x60083080 + AXQOS_GEN_CFG, 20, 4, 0x0},		//awqos_high
	{0x60083080 + AXQOS_GEN_CFG, 24, 4, 0x0},		//awqos_ultra
	{0x60083080 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high
	{0x60083080 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60083080 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel
};

struct qos_cfg pub_cross_mtx_qos_cfg[] = {
	//nic400_pub_cross_mtx_m0_qos_list--CPU
	{0x60085000 + REGU_OT_CTRL_EN, 0, 1, 0x1},		//otctrl_en
	{0x60085000 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x10},		//urg_03_max_aw_ot
	{0x60085000 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x2},		//urg_03_aw_intvl_mode
	{0x60085000 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x1},		//urg_01_max_aw_ot
	{0x60085000 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60085000 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x20},	//urg_00_max_aw_ot
	{0x60085000 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60085000 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x1},		//urg_13_max_aw_ot
	{0x60085000 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60085000 + REGU_OT_CTRL_AR_CFG, 0, 6, 0x1},		//urg_03_max_ar_ot
	{0x60085000 + REGU_OT_CTRL_AR_CFG, 8, 6, 0x4},		//urg_01_max_ar_ot
	{0x60085000 + REGU_OT_CTRL_AR_CFG, 16, 6, 0x3f},	//urg_00_max_ar_ot
	{0x60085000 + REGU_OT_CTRL_AR_CFG, 24, 6, 0x2},		//urg_13_max_ar_ot
	{0x60085000 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x8},		//urg_1x_max_aw_ot
	{0x60085000 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60085000 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x10},		//urg_3x_max_aw_ot
	{0x60085000 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60085000 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x10},	//urg_1x_max_ar_ot
	{0x60085000 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x20},	//urg_3x_max_ar_ot

	{0x60085000 + AXQOS_GEN_EN, 0, 1, 0x0},			//gen_en_w--enabel write urgent generate
	{0x60085000 + AXQOS_GEN_EN, 1, 1, 0x0},			//gen_en_r--enabel read urgent generate
	{0x60085000 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60085000 + AXQOS_GEN_CFG, 0, 4, 0x0},		//arqos_norm
	{0x60085000 + AXQOS_GEN_CFG, 4, 4, 0x0},		//arqos_high
	{0x60085000 + AXQOS_GEN_CFG, 8, 4, 0x0},		//arqos_ultra
	{0x60085000 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60085000 + AXQOS_GEN_CFG, 16, 4, 0x0},		//awqos_norm
	{0x60085000 + AXQOS_GEN_CFG, 20, 4, 0x0},		//awqos_high
	{0x60085000 + AXQOS_GEN_CFG, 24, 4, 0x0},		//awqos_ultra
	{0x60085000 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high
	{0x60085000 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60085000 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel


	//nic400_pub_cross_mtx_m1_qos_list--GPU
	{0x60085080 + REGU_OT_CTRL_EN, 0, 1, 0x1},		//otctrl_en
	{0x60085080 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x10},		//urg_03_max_aw_ot
	{0x60085080 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x2},		//urg_03_aw_intvl_mode
	{0x60085080 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x1},		//urg_01_max_aw_ot
	{0x60085080 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60085080 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x20},	//urg_00_max_aw_ot
	{0x60085080 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60085080 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x1},		//urg_13_max_aw_ot
	{0x60085080 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60085080 + REGU_OT_CTRL_AR_CFG, 0, 6, 0x1},		//urg_03_max_ar_ot
	{0x60085080 + REGU_OT_CTRL_AR_CFG, 8, 6, 0x4},		//urg_01_max_ar_ot
	{0x60085080 + REGU_OT_CTRL_AR_CFG, 16, 6, 0x3f},	//urg_00_max_ar_ot
	{0x60085080 + REGU_OT_CTRL_AR_CFG, 24, 6, 0x4},		//urg_13_max_ar_ot
	{0x60085080 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x8},		//urg_1x_max_aw_ot
	{0x60085080 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60085080 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x10},		//urg_3x_max_aw_ot
	{0x60085080 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60085080 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x10},	//urg_1x_max_ar_ot
	{0x60085080 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x20},	//urg_3x_max_ar_ot

	{0x60085080 + AXQOS_GEN_EN, 0, 1, 0x0},			//gen_en_w--enabel write urgent generate
	{0x60085080 + AXQOS_GEN_EN, 1, 1, 0x0},			//gen_en_r--enabel read urgent generate
	{0x60085080 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60085080 + AXQOS_GEN_CFG, 0, 4, 0x0},		//arqos_norm
	{0x60085080 + AXQOS_GEN_CFG, 4, 4, 0x0},		//arqos_high
	{0x60085080 + AXQOS_GEN_CFG, 8, 4, 0x0},		//arqos_ultra
	{0x60085080 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60085080 + AXQOS_GEN_CFG, 16, 4, 0x0},		//awqos_norm
	{0x60085080 + AXQOS_GEN_CFG, 20, 4, 0x0},		//awqos_high
	{0x60085080 + AXQOS_GEN_CFG, 24, 4, 0x0},		//awqos_ultra
	{0x60085080 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high
	{0x60085080 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60085080 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel


	//nic400_pub_cross_mtx_m2_qos_list--DPU/DCAM
	{0x60085100 + REGU_OT_CTRL_EN, 0, 1, 0x0},		//otctrl_en
	{0x60085100 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x0},		//urg_03_max_aw_ot
	{0x60085100 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x0},		//urg_03_aw_intvl_mode
	{0x60085100 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x0},		//urg_01_max_aw_ot
	{0x60085100 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60085100 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x0},		//urg_00_max_aw_ot
	{0x60085100 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60085100 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x0},		//urg_13_max_aw_ot
	{0x60085100 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60085100 + REGU_OT_CTRL_AR_CFG, 0, 6, 0x0},		//urg_03_max_ar_ot
	{0x60085100 + REGU_OT_CTRL_AR_CFG, 8, 6, 0x0},		//urg_01_max_ar_ot
	{0x60085100 + REGU_OT_CTRL_AR_CFG, 16, 6, 0x0},		//urg_00_max_ar_ot
	{0x60085100 + REGU_OT_CTRL_AR_CFG, 24, 6, 0x0},		//urg_13_max_ar_ot
	{0x60085100 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x0},		//urg_1x_max_aw_ot
	{0x60085100 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60085100 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x0},		//urg_3x_max_aw_ot
	{0x60085100 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60085100 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x0},		//urg_1x_max_ar_ot
	{0x60085100 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x0},		//urg_3x_max_ar_ot

	{0x60085100 + AXQOS_GEN_EN, 0, 1, 0x1},			//gen_en_w--enabel write urgent generate
	{0x60085100 + AXQOS_GEN_EN, 1, 1, 0x1},			//gen_en_r--enabel read urgent generate
	{0x60085100 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60085100 + AXQOS_GEN_CFG, 0, 4, 0xc},		//arqos_norm
	{0x60085100 + AXQOS_GEN_CFG, 4, 4, 0xc},		//arqos_high
	{0x60085100 + AXQOS_GEN_CFG, 8, 4, 0xc},		//arqos_ultra
	{0x60085100 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60085100 + AXQOS_GEN_CFG, 16, 4, 0xc},		//awqos_norm
	{0x60085100 + AXQOS_GEN_CFG, 20, 4, 0xc},		//awqos_high
	{0x60085100 + AXQOS_GEN_CFG, 24, 4, 0xc},		//awqos_ultra
	{0x60085100 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high
	{0x60085100 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60085100 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel


	//nic400_pub_cross_mtx_m3_qos_list--ISP/VSP/GSP
	{0x60085180 + REGU_OT_CTRL_EN, 0, 1, 0x1},		//otctrl_en
	{0x60085180 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x8},		//urg_03_max_aw_ot
	{0x60085180 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x0},		//urg_03_aw_intvl_mode
	{0x60085180 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x8},		//urg_01_max_aw_ot
	{0x60085180 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60085180 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x8},		//urg_00_max_aw_ot
	{0x60085180 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60085180 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x8},		//urg_13_max_aw_ot
	{0x60085180 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60085180 + REGU_OT_CTRL_AR_CFG, 0, 6, 0x3f},		//urg_03_max_ar_ot
	{0x60085180 + REGU_OT_CTRL_AR_CFG, 8, 6, 0x3f},		//urg_01_max_ar_ot
	{0x60085180 + REGU_OT_CTRL_AR_CFG, 16, 6, 0x3f},	//urg_00_max_ar_ot
	{0x60085180 + REGU_OT_CTRL_AR_CFG, 24, 6, 0x3f},	//urg_13_max_ar_ot
	{0x60085180 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x8},		//urg_1x_max_aw_ot
	{0x60085180 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60085180 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x8},		//urg_3x_max_aw_ot
	{0x60085180 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60085180 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x3f},	//urg_1x_max_ar_ot
	{0x60085180 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x3f},	//urg_3x_max_ar_ot

	{0x60085180 + AXQOS_GEN_EN, 0, 1, 0x0},			//gen_en_w--enabel write urgent generate
	{0x60085180 + AXQOS_GEN_EN, 1, 1, 0x0},			//gen_en_r--enabel read urgent generate
	{0x60085180 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60085180 + AXQOS_GEN_CFG, 0, 4, 0x0},		//arqos_norm
	{0x60085180 + AXQOS_GEN_CFG, 4, 4, 0x0},		//arqos_high
	{0x60085180 + AXQOS_GEN_CFG, 8, 4, 0x0},		//arqos_ultra
	{0x60085180 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60085180 + AXQOS_GEN_CFG, 16, 4, 0x0},		//awqos_norm
	{0x60085180 + AXQOS_GEN_CFG, 20, 4, 0x0},		//awqos_high
	{0x60085180 + AXQOS_GEN_CFG, 24, 4, 0x0},		//awqos_ultra
	{0x60085180 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high
	{0x60085180 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60085180 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel


	//nic400_pub_cross_mtx_m4_qos_list--ap/aon/ipa
	{0x60085200 + REGU_OT_CTRL_EN, 0, 1, 0x1},		//otctrl_en
	{0x60085200 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x4},		//urg_03_max_aw_ot
	{0x60085200 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x0},		//urg_03_aw_intvl_mode
	{0x60085200 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x4},		//urg_01_max_aw_ot
	{0x60085200 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60085200 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x4},		//urg_00_max_aw_ot
	{0x60085200 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60085200 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x4},		//urg_13_max_aw_ot
	{0x60085200 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60085200 + REGU_OT_CTRL_AR_CFG, 0, 6, 0xa},		//urg_03_max_ar_ot
	{0x60085200 + REGU_OT_CTRL_AR_CFG, 8, 6, 0xa},		//urg_01_max_ar_ot
	{0x60085200 + REGU_OT_CTRL_AR_CFG, 16, 6, 0xa},		//urg_00_max_ar_ot
	{0x60085200 + REGU_OT_CTRL_AR_CFG, 24, 6, 0xa},		//urg_13_max_ar_ot
	{0x60085200 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x8},		//urg_1x_max_aw_ot
	{0x60085200 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60085200 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x8},		//urg_3x_max_aw_ot
	{0x60085200 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60085200 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x10},	//urg_1x_max_ar_ot
	{0x60085200 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x10},	//urg_3x_max_ar_ot

	{0x60085200 + AXQOS_GEN_EN, 0, 1, 0x0},			//gen_en_w--enabel write urgent generate
	{0x60085200 + AXQOS_GEN_EN, 1, 1, 0x0},			//gen_en_r--enabel read urgent generate
	{0x60085200 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60085200 + AXQOS_GEN_CFG, 0, 4, 0x0},		//arqos_norm
	{0x60085200 + AXQOS_GEN_CFG, 4, 4, 0x0},		//arqos_high
	{0x60085200 + AXQOS_GEN_CFG, 8, 4, 0x0},		//arqos_ultra
	{0x60085200 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60085200 + AXQOS_GEN_CFG, 16, 4, 0x0},		//awqos_norm
	{0x60085200 + AXQOS_GEN_CFG, 20, 4, 0x0},		//awqos_high
	{0x60085200 + AXQOS_GEN_CFG, 24, 4, 0x0},		//awqos_ultra
	{0x60085200 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high
	{0x60085200 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60085200 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel


	//nic400_pub_cross_mtx_m5_qos_list--cp acc
	{0x60085280 + REGU_OT_CTRL_EN, 0, 1, 0x1},		//otctrl_en
	{0x60085280 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x8},		//urg_03_max_aw_ot
	{0x60085280 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x0},		//urg_03_aw_intvl_mode
	{0x60085280 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x8},		//urg_01_max_aw_ot
	{0x60085280 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60085280 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x8},		//urg_00_max_aw_ot
	{0x60085280 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60085280 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x8},		//urg_13_max_aw_ot
	{0x60085280 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60085280 + REGU_OT_CTRL_AR_CFG, 0, 6, 0x10},		//urg_03_max_ar_ot
	{0x60085280 + REGU_OT_CTRL_AR_CFG, 8, 6, 0x10},		//urg_01_max_ar_ot
	{0x60085280 + REGU_OT_CTRL_AR_CFG, 16, 6, 0x10},	//urg_00_max_ar_ot
	{0x60085280 + REGU_OT_CTRL_AR_CFG, 24, 6, 0x10},	//urg_13_max_ar_ot
	{0x60085280 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x8},		//urg_1x_max_aw_ot
	{0x60085280 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60085280 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x8},		//urg_3x_max_aw_ot
	{0x60085280 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60085280 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x10},	//urg_1x_max_ar_ot
	{0x60085280 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x10},	//urg_3x_max_ar_ot

	{0x60085280 + AXQOS_GEN_EN, 0, 1, 0x1},			//gen_en_w--enabel write urgent generate
	{0x60085280 + AXQOS_GEN_EN, 1, 1, 0x1},			//gen_en_r--enabel read urgent generate
	{0x60085280 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60085280 + AXQOS_GEN_CFG, 0, 4, 0xc},		//arqos_norm
	{0x60085280 + AXQOS_GEN_CFG, 4, 4, 0xc},		//arqos_high
	{0x60085280 + AXQOS_GEN_CFG, 8, 4, 0xc},		//arqos_ultra
	{0x60085280 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60085280 + AXQOS_GEN_CFG, 16, 4, 0xc},		//awqos_norm
	{0x60085280 + AXQOS_GEN_CFG, 20, 4, 0xc},		//awqos_high
	{0x60085280 + AXQOS_GEN_CFG, 24, 4, 0xc},		//awqos_ultra
	{0x60085280 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high
	{0x60085280 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60085280 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel


	//nic400_pub_cross_mtx_m6_qos_list--CP CPU
	{0x60085300 + REGU_OT_CTRL_EN, 0, 1, 0x0},		//otctrl_en
	{0x60085300 + REGU_OT_CTRL_AW_CFG, 0, 6, 0x0},		//urg_03_max_aw_ot
	{0x60085300 + REGU_OT_CTRL_AW_CFG, 6, 2, 0x0},		//urg_03_aw_intvl_mode
	{0x60085300 + REGU_OT_CTRL_AW_CFG, 8, 6, 0x0},		//urg_01_max_aw_ot
	{0x60085300 + REGU_OT_CTRL_AW_CFG, 14, 2, 0x0},		//urg_01_aw_intvl_mode
	{0x60085300 + REGU_OT_CTRL_AW_CFG, 16, 6, 0x0},		//urg_00_max_aw_ot
	{0x60085300 + REGU_OT_CTRL_AW_CFG, 22, 2, 0x0},		//urg_00_aw_intvl_mode
	{0x60085300 + REGU_OT_CTRL_AW_CFG, 24, 6, 0x0},		//urg_13_max_aw_ot
	{0x60085300 + REGU_OT_CTRL_AW_CFG, 30, 2, 0x0},		//urg_13_aw_intvl_mode
	{0x60085300 + REGU_OT_CTRL_AR_CFG, 0, 6, 0x0},		//urg_03_max_ar_ot
	{0x60085300 + REGU_OT_CTRL_AR_CFG, 8, 6, 0x0},		//urg_01_max_ar_ot
	{0x60085300 + REGU_OT_CTRL_AR_CFG, 16, 6, 0x0},		//urg_00_max_ar_ot
	{0x60085300 + REGU_OT_CTRL_AR_CFG, 24, 6, 0x0},		//urg_13_max_ar_ot
	{0x60085300 + REGU_OT_CTRL_AX_CFG, 0, 6, 0x0},		//urg_1x_max_aw_ot
	{0x60085300 + REGU_OT_CTRL_AX_CFG, 6, 2, 0x0},		//urg_1x_aw_intvl_mode
	{0x60085300 + REGU_OT_CTRL_AX_CFG, 8, 6, 0x0},		//urg_3x_max_aw_ot
	{0x60085300 + REGU_OT_CTRL_AX_CFG, 14, 2, 0x0},		//urg_3x_aw_intvl_mode
	{0x60085300 + REGU_OT_CTRL_AX_CFG, 16, 6, 0x0},		//urg_1x_max_ar_ot
	{0x60085300 + REGU_OT_CTRL_AX_CFG, 24, 6, 0x0},		//urg_3x_max_ar_ot

	{0x60085300 + AXQOS_GEN_EN, 0, 1, 0x1},			//gen_en_w--enabel write urgent generate
	{0x60085300 + AXQOS_GEN_EN, 1, 1, 0x1},			//gen_en_r--enabel read urgent generate
	{0x60085300 + AXQOS_GEN_EN, 31, 1, 0x0},		//urgency_feedthr
	{0x60085300 + AXQOS_GEN_CFG, 0, 4, 0xe},		//arqos_norm
	{0x60085300 + AXQOS_GEN_CFG, 4, 4, 0xe},		//arqos_high
	{0x60085300 + AXQOS_GEN_CFG, 8, 4, 0xe},		//arqos_ultra
	{0x60085300 + AXQOS_GEN_CFG, 12, 2, 0x0},		//arurgency--choose urgent noraml/low/high
	{0x60085300 + AXQOS_GEN_CFG, 16, 4, 0xe},		//awqos_norm
	{0x60085300 + AXQOS_GEN_CFG, 20, 4, 0xe},		//awqos_high
	{0x60085300 + AXQOS_GEN_CFG, 24, 4, 0xe},		//awqos_ultra
	{0x60085300 + AXQOS_GEN_CFG, 28, 2, 0x0},		//awurgency--choose urgent noraml/low/high
	{0x60085300 + URG_CNT_CFG, 0, 1, 0x1},			//cnt_en
	{0x60085300 + URG_CNT_CFG, 8, 3, 0x0},			//cnt_sel
};

void pub_qos_cfg(void)
{
	u32 i;

	for (i = 0; i < sizeof(pub_cpcpu_mtx_qos_cfg)/sizeof(pub_cpcpu_mtx_qos_cfg[0]); i++)
		reg_bit_set(pub_cpcpu_mtx_qos_cfg[i].addr, pub_cpcpu_mtx_qos_cfg[i].start_bit,
			    pub_cpcpu_mtx_qos_cfg[i].bits, pub_cpcpu_mtx_qos_cfg[i].value);

	for (i = 0; i < sizeof(pub_dcam_dpu_mtx_qos_cfg)/sizeof(pub_dcam_dpu_mtx_qos_cfg[0]); i++)
		reg_bit_set(pub_dcam_dpu_mtx_qos_cfg[i].addr, pub_dcam_dpu_mtx_qos_cfg[i].start_bit,
			    pub_dcam_dpu_mtx_qos_cfg[i].bits, pub_dcam_dpu_mtx_qos_cfg[i].value);

	for (i = 0; i < sizeof(pub_ap_aon_ipa_mtx_qos_cfg)/sizeof(pub_ap_aon_ipa_mtx_qos_cfg[0]); i++)
		reg_bit_set(pub_ap_aon_ipa_mtx_qos_cfg[i].addr, pub_ap_aon_ipa_mtx_qos_cfg[i].start_bit,
			    pub_ap_aon_ipa_mtx_qos_cfg[i].bits, pub_ap_aon_ipa_mtx_qos_cfg[i].value);

	for (i = 0; i < sizeof(pub_mm_mtx_qos_cfg)/sizeof(pub_mm_mtx_qos_cfg[0]); i++)
		reg_bit_set(pub_mm_mtx_qos_cfg[i].addr, pub_mm_mtx_qos_cfg[i].start_bit,
			    pub_mm_mtx_qos_cfg[i].bits, pub_mm_mtx_qos_cfg[i].value);

	for (i = 0; i < sizeof(cpacc_mtx_qos_cfg)/sizeof(cpacc_mtx_qos_cfg[0]); i++)
		reg_bit_set(cpacc_mtx_qos_cfg[i].addr, cpacc_mtx_qos_cfg[i].start_bit,
			    cpacc_mtx_qos_cfg[i].bits, cpacc_mtx_qos_cfg[i].value);

	for (i = 0; i < sizeof(pub_cross_mtx_qos_cfg)/sizeof(pub_cross_mtx_qos_cfg[0]); i++)
		reg_bit_set(pub_cross_mtx_qos_cfg[i].addr, pub_cross_mtx_qos_cfg[i].start_bit,
			    pub_cross_mtx_qos_cfg[i].bits, pub_cross_mtx_qos_cfg[i].value);

	return;
}

