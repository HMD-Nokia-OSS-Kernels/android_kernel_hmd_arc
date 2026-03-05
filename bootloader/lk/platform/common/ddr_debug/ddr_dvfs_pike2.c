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

#include <sprd_common.h>

#define PUB_DMC_BASE        0x30000000
#define DRAM_INFO_ADDR      (PUB_DMC_BASE + 0x1c0)

u8 get_dram_info(void)
{
    u8 ManufactureId;
    unsigned int DramInfo = readl(DRAM_INFO_ADDR);

    ManufactureId = (u8)(DramInfo&0xff);

    printf("[get_dram_info]:MR5=0x%02x,MR6=0x%02x,MR7=0x%02x,MR8=0x%02x\n"
                    ,DramInfo&0xff,(DramInfo>>8)&0xff,(DramInfo>>16)&0xff,(DramInfo>>24)&0xff);

    return ManufactureId;
}
