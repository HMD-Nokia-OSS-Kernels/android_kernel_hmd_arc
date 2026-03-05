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

#ifndef __SPRD_DDR_MEMTEST_H__
#define __SPRD_DDR_MEMTEST_H__

#define DDR_MEMTEST_LENGTH (0x140000000 - 0x1000)

int ddr_memtester(ulong base_addr, ulong test_len, ulong test_times, ulong is_loop);

#endif
