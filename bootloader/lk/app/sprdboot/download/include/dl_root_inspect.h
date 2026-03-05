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

#ifndef _ROOT_INSPECT_H_
#define _ROOT_INSPECT_H_


#define ROOT_MAGIC 0x524F4F54 //"ROOT"
#define ROOT_OFFSET 0x1000

typedef struct {
	u32 magic;
	u32 root_flag;
} root_stat_t;

u32 get_rootflag(root_stat_t *stat);
u32 erase_rootflag(root_stat_t *stat);

#endif
