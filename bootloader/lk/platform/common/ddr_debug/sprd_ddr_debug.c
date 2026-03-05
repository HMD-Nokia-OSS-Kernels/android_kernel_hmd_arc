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

#include <miscdata_def.h>
#include <sprd_common_rw.h>
#include <sprd_ddr_dvfs.h>
#include <sprd_ddr_debug.h>

//#define DDR_MISCDATA_DEBUG
#define MAGIC_NUM 0x5aa5

typedef struct DDRC_DDR_DEBUG_INFO
{
	u16 magic_num;
	u16 boot_freq;
	u16 ddr_dvfs_switch;
	u16 boot_freq_vol;
	u16 mask_freq[8];
	u16 ddr_retention_switch;
}DDRC_DDR_DEBUG_INFO_T;

struct manufacture_id {
	int index;
	char *name;
};

struct manufacture_id dram_id[] = {
	{0,	"Reserved"},
	{1,	"Samsung"},
	{2,	"Reserved"},
	{3,	"Reserved"},
	{4,	"Reserved"},
	{5,	"Nanya"},
	{6,	"Hynix"},
	{7,	"Reserved"},
	{8,	"Winbond"},
	{9,	"Esmt"},
	{0xa,	"Reserved"},
	{0xb,	"Reserved"},
	{0xc,	"Reserved"},
	{0xd,	"Reserved"},
	{0xe,	"Reserved"},
	{0xf,	"Reserved"},
	{0x12,	"Reserved"},
	{0x13,	"Cxmt"},
	{0x1a,	"Xi'an UniIC"},
	{0x1c,	"Jsc"},
	{0xf1,	"Fidelix"},
	{0xf9,	"Ultra Memory"},
	{0xfd,	"AP memory"},
	{0xff,	"Micron"},
	{0x1ff,	"Unsupported type"}
};

#if defined(PLATFORM_SHARKL3) || defined(PLATFORM_SHARKL5) || defined(PLATFORM_SHARKL5PRO) || \
	defined(PLATFORM_QOGIRL6) || defined(PLATFORM_QOGIRN6PRO) || defined(PLATFORM_QOGIRN6L)
void ddr_eng_mode_debug(void)
{
	int ret;
	u32 fn, i, cur_vol;
	u32 ddr_dvfs_disable, ddr_retention_disable;
	u32 mask_mode = 0;
	DDRC_DDR_DEBUG_INFO_T ddr_debug;

	//1. read target freq from miscdata
	ret = common_raw_read("miscdata", (uint64_t)MISCDATA_DDR_DEBUG_LEN,
			      (uint64_t)MISCDATA_DDR_DEBUG_OFFSET, (char*)(&ddr_debug));
	if (ret < 0) {
		errorf("read miscdata ddr debug paras err\n");
		return;
	}

#ifdef DDR_MISCDATA_DEBUG
	debugf("magic_num:0x%x\n", ddr_debug.magic_num);
	debugf("boot_freq:%d\n", ddr_debug.boot_freq);
	debugf("ddr_dvfs_switch:0x%x\n", ddr_debug.ddr_dvfs_switch);
	debugf("boot_freq_vol:%d\n", ddr_debug.boot_freq_vol);
	for (i =0; i < 8; i++)
		debugf("mask_freq[%d]:%d\n", i, ddr_debug.mask_freq[i]);
	debugf("ddr_retention_switch:0x%x\n", ddr_debug.ddr_retention_switch);
#endif

	if (ddr_debug.magic_num == MAGIC_NUM) {
		debugf("ddr debug mode enable\n");
		//2. ddr dvfs to misc target freq & vol
		ret = dmc_freq_sel_search(ddr_debug.boot_freq, &fn);
		if (ret < 0) {
			errorf("illegal ddr boot freq: %dMZ, check it\n", ddr_debug.boot_freq);
		} else {
			sw_dvfs_go(fn);
			debugf("ddr boot freq: %dMHZ\n", ddr_debug.boot_freq);
			if ((ddr_debug.ddr_dvfs_switch == 0x5a) && (ddr_debug.boot_freq_vol != 0)) {
				cur_vol = get_freq_vol(fn);
				if (ddr_debug.boot_freq_vol > cur_vol) {
					ret = ddr_boot_freq_voltage_set(ddr_debug.boot_freq_vol);
					if (ret == 0)
						debugf("ddr boot freq vol: %dmv\n", ddr_debug.boot_freq_vol);
					else
						errorf("illegal vddpub: %dmv, check it\n",
							   ddr_debug.boot_freq_vol);
				} else if (ddr_debug.boot_freq_vol < cur_vol) {
					errorf("ddr %dMHZ set vddpub %dmv less minimum %dmv\n, check it",
						  ddr_debug.boot_freq, ddr_debug.boot_freq_vol,
						  cur_vol);
				} else {
					debugf("ddr boot freq vol: %dmv\n", ddr_debug.boot_freq_vol);
				}
			}
		}

		//3. change ddr_mode in iram
		if (ddr_debug.ddr_dvfs_switch == 0x5a)
			ddr_dvfs_disable = 1;
		else
			ddr_dvfs_disable = 0;
		debugf("ddr dvfs is %s\n", (ddr_dvfs_disable? "off":"on"));

		if (ddr_debug.ddr_retention_switch == 0x5a)
			ddr_retention_disable = 1;
		else
			ddr_retention_disable = 0;
		debugf("ddr retention is %s\n", (ddr_retention_disable? "off":"on"));

		for (i = 0; i < 8; i++) {
			ret = dmc_freq_sel_search(ddr_debug.mask_freq[i], &fn);
			if (ret == 0) {
				mask_mode |= 1 << fn;
			} else {
				if (ddr_debug.mask_freq[i] != 0)
					errorf("illegal ddr mask freq: %dMHZ, check it\n",
					       ddr_debug.mask_freq[i]);
			}
		}

		ddr_paras_set_to_sp(ddr_dvfs_disable, ddr_retention_disable, &mask_mode);
		debugf("ddr hw dfs disable fn: 0x%x\n", mask_mode);
	}
}

void ddr_debug_paras_erase(void)
{
	int ret;

	ret = common_raw_erase("miscdata", (uint64_t)MISCDATA_DDR_DEBUG_LEN,
			       (uint64_t)MISCDATA_DDR_DEBUG_OFFSET);
	if (ret != 0)
		errorf("erase ddr debug paras fail\n");
}

char *get_dram_id(void)
{
	int i;
	u8 id = get_dram_info();
	for (i = 0; i < sizeof(dram_id)/sizeof(dram_id[0]); i++)
		if (id == dram_id[i].index)
			return dram_id[i].name;
	return dram_id[sizeof(dram_id)/sizeof(dram_id[0]) - 1].name;
}

#elif defined(PLATFORM_SHARKLE) || defined(PLATFORM_PIKE2)

char *get_dram_id(void)
{
    int i;
    u8 id = get_dram_info();

    for (i = 0; i < sizeof(dram_id)/sizeof(dram_id[0]); i++)
        if (id == dram_id[i].index)
            return dram_id[i].name;
    return dram_id[sizeof(dram_id)/sizeof(dram_id[0]) - 1].name;
}

void ddr_eng_mode_debug(void)
{
    return;
}

void ddr_debug_paras_erase(void)
{
    return;
}

#else

void ddr_eng_mode_debug(void)
{
	return;
}

void ddr_debug_paras_erase(void)
{
	return;
}

char *get_dram_id(void)
{
	return NULL;
}

#endif
