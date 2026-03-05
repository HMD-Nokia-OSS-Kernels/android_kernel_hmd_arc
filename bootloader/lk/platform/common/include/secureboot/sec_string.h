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

#ifndef SEC_STRING_H
#define SEC_STRING_H

void *sec_memset(void *s, int c, unsigned int cnt);
void *sec_memcpy(void *dest, const void *src, unsigned int count);
int sec_memcmp(const void *cs, const void *st, unsigned int count);

#endif /* !HEADER_AES_H */

