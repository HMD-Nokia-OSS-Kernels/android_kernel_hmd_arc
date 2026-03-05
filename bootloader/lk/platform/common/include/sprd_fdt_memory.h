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

#ifndef __SPRD_FDT_MEMORY__
#define __SPRD_FDT_MEMORY__

#if (defined CONFIG_ARM64) || (defined CONFIG_X86)
typedef uint64_t mem_addr_t;
typedef uint64_t mem_size_t;
#define fdt_to_cpu(reg) fdt64_to_cpu(reg)
#define cpu_to_fdt(reg) cpu_to_fdt64(reg)
#else
typedef uint32_t mem_addr_t;
typedef uint32_t mem_size_t;
#define fdt_to_cpu(reg) fdt32_to_cpu(reg)
#define cpu_to_fdt(reg) cpu_to_fdt32(reg)
#endif

int fdt_get_addr_size(const void *fdt, int node, const char *propname,
                        mem_addr_t *addrp, mem_size_t *sizep);
static int fdt_set_addr_size(void *fdt, int node, const char *propname,
                        mem_addr_t addr, mem_size_t size);
static int fdt_node_offset_by_name(const void *fdt,  const char *nodename);
int check_mem_region_auto(void *fdt, char *name);
int fixup_memory_addr(void *fdt, int offset, mem_addr_t addr, mem_size_t *size);
int move_prev_memory(void *fdt, mem_addr_t offset, char *name);
int scan_memory_region(void *fdt, int *auto_mem_num);
int get_dt_end_addr(void *fdt, uchar **addr);
int fdt_fixup_memory_region(void *fdt, int *auto_mem_num_p);

#endif
