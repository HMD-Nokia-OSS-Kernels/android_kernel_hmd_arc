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

#ifndef _FASTBOOT_H_
#define _FASTBOOT_H_
void fastboot_register(const char *prefix, void (*handle) (const char *arg, void *data, uint64_t sz));
void fastboot_publish(const char *name, const char *value);
int set_fastboot_buf_base_size(void);
int do_fastboot(void);
int wait_for_keypress(void);
void fail_and_enter_fastboot_mode(void);
#ifdef SPRD_SECBOOT
void fb_cmd_getsocid(const char *arg, void *data, uint64_t sz);
void fb_cmd_getlcs(const char *arg, void *data, uint64_t sz);
void fb_cmd_get_secdebug_bit(const char *arg, void *data, uint64_t sz);
void fb_cmd_get_rotpk0(const char *arg, void *data, uint64_t sz);
void fb_cmd_get_rotpk1(const char *arg, void *data, uint64_t sz);
void fb_cmd_check_kce_status(const char *arg, void *data, uint64_t sz);
void fb_cmd_get_vboot_imgversion(const char *arg, void *data, uint64_t sz);
void fb_cmd_get_secure_version(const char *arg, void *data, uint64_t sz);
void fb_cmd_get_secure_enable(const char *arg, void *data, uint64_t sz);
#endif
#if defined(CONFIG_RPMB_SECURE_WRITE_PROTECT)
static void fb_write_protect_test(char *partition_name, char *len, char *action ,char *write_pattern);
#else
static void fb_write_protect_test(char *partition_name, char *len, char *action ,char *write_pattern)
{
}
#endif
#endif
