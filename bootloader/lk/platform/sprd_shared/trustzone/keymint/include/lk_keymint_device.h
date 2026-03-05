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

#ifndef LK_KEYMINT_DEVICE_H_
#define LK_KEYMINT_DEVICE_H_

#include <interface/keymint.h>
#include <ipc/lk_keymint_ipc.h>
#include <lk_keymint_messages.h>
#include <secureboot/sec_common.h>

/**
 * Send 'boot patch level' value to KM server.
 * The 'boot patch level' is stored in the vbmeta image.Everytime the device
 * is powered on, the image is parsed by AVB in the bootloader phase, and
 * the AVB provides an interface for KM to obatin 'boot patch level'.
 */
void km_config_boot_patchlevel(void);

/**
 * Send 'boot params' value to KM server.
 * The 'boot params' is stored in the vbmeta image.Everytime the device
 * is powered on, the image is parsed by AVB in the bootloader phase, and
 * the AVB provides an interface for KM to obatin 'boot params'.
 */
void km_set_boot_params(void);


#endif /* LK_KEYMINT_DEVICE_H_ */
