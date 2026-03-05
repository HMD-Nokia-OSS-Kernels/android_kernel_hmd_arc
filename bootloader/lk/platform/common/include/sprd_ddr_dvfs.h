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

#ifndef __SPRD_DDR_DVFS_H
#define __SPRD_DDR_DVFS_H
u8 get_dram_info(void);
u32 get_freq_vol(u32 fn);
int dmc_freq_sel_search(u32 ddr_clk, u32* fn);
int ddr_boot_freq_voltage_set(u32 vol);
void sw_dvfs_go(u32 fn);
void ddr_paras_set_to_sp(u32 dvfs_dis, u32 retention_dis, u32* mask_mode);

#endif
