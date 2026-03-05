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

#include <stddef.h>
#include <lk/compiler.h>
#include "lk_sec_drv.h"

int __WEAK lk_write_rpmb_key(uint8_t *key)
{
    return -1;
}

int __WEAK lk_set_rpmb_size(void)
{
    return -1;
}


int __WEAK lk_is_wr_rpmb_key(void)
{
    return -1;
}

int __WEAK lk_check_rpmb_key(void)
{
    return -1;
}

int __WEAK lk_set_rpmb_device_type(void)
{
    return -1;
}

