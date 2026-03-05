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

#ifndef _EFUSE_OPERATE_H_
#define _EFUSE_OPERATE_H_

int arm7_key_write( char *key, u32 count);
int arm7_key_read( char *key, u32 count);
int efuse_hash_write( char *hash, u32 count);
int efuse_hash_read(char *hash, u32 count);
int efuse_uid_read(char *uid, int count);

#endif/*_EFUSE_OPERATE_H_*/
