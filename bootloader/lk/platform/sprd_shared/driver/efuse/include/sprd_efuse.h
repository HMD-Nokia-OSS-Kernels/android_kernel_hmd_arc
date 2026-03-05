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

#ifndef __SPRD_EFUSE_H_
#define __SPRD_EFUSE_H_

#include <asm/arch/common.h>

u32 sprd_ap_efuse_read(int blk_index);
int sprd_ap_efuse_prog(int blk_index, u32 val);
int sprd_efuse_double_prog(u32 blk, bool backup, bool lock, u32 val);
u32 sprd_efuse_double_read(int blk, bool backup);
u32 sprd_pmic_efuse_read(int blk_index);
u32 sprd_pmic_efuse_read_bits(int bit_index, int length);
void pmic_efuse_block_dump(void);
void ap_efuse_block_dump(void);
void sprd_uid_bin_info(void);
int sprd_get_chip_uid(char *buf);
int sprd_get_chip_hex_uid(char *buf);
int sprd_secure_efuse_read(u32 start_id, u32 end_id, u32 *pReadData,u32 Isdouble);
void ap_sansa_efuse_prog_power_on(void);
void ap_sansa_efuse_power_off(void);
void sansa_enable_efuse_EB(void);

//high refresh efuse driver
uint32_t high_refresh_efuse_prog(int start_index, int end_index, bool isdouble, uint32_t *val);
uint32_t high_refresh_efuse_read(int start_index, int end_index, bool isdouble, uint32_t *val);
int high_refresh_efuse_power_on_read(uint32_t reg_offset, uint32_t *val);
#endif
