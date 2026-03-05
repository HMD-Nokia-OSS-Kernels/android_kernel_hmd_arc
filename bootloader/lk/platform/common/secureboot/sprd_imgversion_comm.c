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

#include <linux/compiler.h>
#include <linux/types.h>

#include <sprd_imgversion.h>

int __weak sprd_get_imgversion(int imgType, unsigned int* swVersion)
{
    return -1;
}

int __weak sprd_init_all_imgversion(uint8_t *rpmb_key)
{
    return -1;
}

