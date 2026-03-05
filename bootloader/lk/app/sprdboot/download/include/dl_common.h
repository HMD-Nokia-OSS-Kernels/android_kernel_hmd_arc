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

#ifndef __DL_COMMON_H
#define __DL_COMMON_H

#include <chipram_env.h>
#include <asm/arch/common.h>
#include <dl_packet.h>
#include <dl_operate.h>
#include <sprd_common.h>
#include <sprd_sizes.h>
#include <linux/types.h>
#include <sprd_log.h>

#define BOOTLOADER_HEADER_OFFSET 0x20
#define HASH_SHA256_BUF_LEN     32

unsigned short fdl_calc_checksum(unsigned char *data, unsigned long len);
unsigned char fdl_check_crc(char * buf, uint32_t size, uint32_t checksum);
unsigned long Get_CheckSum(const unsigned char *src, int len);
#ifdef NV_CHECK_WITH_SHA256
int fdl_check_sha256(char* buf, uint32_t size,uint8_t* auth256);
int fdl_get_sha256(char* buf, uint32_t size, uint8_t* output);
#endif
int do_download(void);
static void dl_pre_init(void);

#endif
