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

#pragma once

#include <sprd_common.h>
#include <asm/types.h>

typedef enum bootloader_mode {
	BOOTLOADER_MODE_UNKNOW = 0x100,
	BOOTLOADER_MODE_DOWNLOAD = 0x102,
	BOOTLOADER_MODE_LOAD =0x104
}boot_mode_t;

typedef enum boot_device {
	BOOT_DEVICE_EMMC = 0x205,
	BOOT_DEVICE_UFS =0x206
}boot_device_t;

#define CHIPRAM_ENV_MAGIC	0x43454e56
#define BIST_PASS_KEY		0xdeadbeefdeadbeef
typedef struct chipram_env {
	u32 magic; //0x43454e56
	boot_mode_t mode;
	u64 dram_size; //MB
	u64 vol_para_addr;
	u32 keep_charge;
	u32 channel_sel;//usb,uart0,uart1
	u32 cs_number;
	u64 cs0_size;
	u64 cs1_size;
	u64 bist_fail_addr;
#ifdef CONFIG_TIME_STATISTIC
	u32 tspl_s;
	u32 tspl_e;
#endif
	u32 spl_adjust_flag;
#ifdef CONFIG_SPL_DOUBLE_SLOT
	u32 spl_edit_flag;
	u32 dual_spl_flag;
#endif
	u32 reserved;
#if defined(CONFIG_ADIE_UMP518)
	u64 ocp_flag; //for eic_dbnc:ump518
	u32 scp_flag;
#endif
}chipram_env_t;

typedef struct section_info{
	u32 type;
	u32 start_address_high;
	u32 start_address_low;
	u32 end_address_high;
	u32 end_address_low;
}section_info_t;

#define max_sec_number 8
typedef struct ddr_info {
	u32 section_number;
	struct section_info sec_info[max_sec_number];
}ddr_info_t;
chipram_env_t* get_chipram_env(void);
boot_mode_t get_boot_role(void);

