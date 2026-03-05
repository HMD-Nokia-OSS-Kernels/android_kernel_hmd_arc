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

#ifndef _TRUSTZONE_OPERATE_H_
#define _TRUSTZONE_OPERATE_H_

int uboot_set_root_of_trust(unsigned long start_addr, uint32_t lenth);
int uboot_config_os_version(unsigned long start_addr, uint32_t lenth);
int uboot_verify_product_sn_signature(unsigned long start_addr, uint32_t lenth);
int uboot_vboot_verify_img(unsigned long start_addr, uint32_t lenth);
int uboot_verify_img(unsigned long start_addr, uint32_t lenth);
int uboot_update_swVersion(uint64_t start_addr, uint64_t lenth);
int uboot_vboot_set_ver(unsigned long start_addr, uint32_t lenth);
int uboot_get_hbk(uint64_t start_addr, uint64_t lenth);
int uboot_write_hbk(uint64_t start_addr, uint64_t lenth);

#endif
