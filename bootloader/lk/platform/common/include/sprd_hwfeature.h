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

#ifndef __SPRD_HWFEATURE__
#define __SPRD_HWFEATURE__
#include <stdlib.h>
#include <stdarg.h>
#include <lk/debug.h>
#include <string.h>
#include <libfdt_env.h>
#include <part_efi.h>
#include <libfdt.h>
#ifndef NULL
#if defined(__cplusplus)
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif
#define HWFEATURE_KEY_MAX 2048
#define HWFEATURE_VALUE_MAX 64

struct hwfeature {
    const char *param;
    char *param_start;
    char *param_curr;
    int count;
    int append_to_cmdline;
    int hwf_off;
    int root_off;
    char *key;
    char *value;
    char socinfo_value[HWFEATURE_VALUE_MAX];
    void *fdt;
    char value_fixed[HWFEATURE_VALUE_MAX];
    char format_buf[HWFEATURE_KEY_MAX+HWFEATURE_VALUE_MAX+12+1]; // androidboot.=
    char hwfeature_bootconfig[128];
    int hwfeature_bootconfig_offset;

    union {
        long value;
    } result;

    char *result_last_pos;
    char *result_max_pos;
    void (*init)(struct hwfeature *phwf, void *fdt);
    void (*late_initcall)(struct hwfeature *phwf, void *fdt);
#define HW_GET_MODE(mode) \
    long (*get_##mode)(struct hwfeature *phwf);
    HW_GET_MODE(efuse)
    HW_GET_MODE(chipid)
    HW_GET_MODE(lowv)
    HW_GET_MODE(adc)
    HW_GET_MODE(gpio)
    void (*parse)(struct hwfeature *phwf);
    void (*parse_auto)(struct hwfeature *phwf);
    void (*trim)(struct hwfeature *phwf, char **start, char *end);
    void *(*fix_value)(struct hwfeature *phwf, char *value, char *value_fixed);
    void (*replace)(struct hwfeature *phwf, char *value, char from, char to);
    int (*format)(struct hwfeature *phwf, const char *fmt, ...);
    int (*pick_element)(struct hwfeature *phwf, const char *token);
    int (*process_element)(struct hwfeature *phwf);
    int (*fdt_generate)(struct hwfeature *phwf);
    int (*fdt_header)(struct hwfeature *phwf);
    int (*fdt_mknodes)(struct hwfeature *phwf, char *path, int *dt_node_off);
    int (*fdt_mkprop)(struct hwfeature *phwf, char *prop, char *value, int dt_node_off);
};

int fdt_fixup_hwfeature(void *fdt);
int fdt_fixup_cpuinfo_hwfeature(void *fdt);
int fdt_fixup_soc_module(void *fdt);
void hwfeature_hook_late_initcall(void (*late_initcall)(struct hwfeature *phwf, void *fdt));
extern char *bootconfig_get_hwfeature(void);

#define HW_GET_HOOK_MODE(mode) \
    extern void hwfeature_hook_get_##mode(long (*get)(struct hwfeature *phwf));

HW_GET_HOOK_MODE(efuse)
HW_GET_HOOK_MODE(chipid)
HW_GET_HOOK_MODE(lowv)
HW_GET_HOOK_MODE(adc)
HW_GET_HOOK_MODE(gpio)
#endif //__SPRD_HWFEATURE__
