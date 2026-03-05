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

#ifndef __COUPLING_INFO_H__
#define __COUPLING_INFO_H__

#include <sprd_fdt_support.h>
#define DECOUPLE_MAGIC_INFO	0x5043454D
#define HEADER_MAGIC_INFO	0x44485043

#define MAX_HEAD_LENGTH	0x100

#ifndef CONFIG_CP_MAX_REGION_NUM
#define MAX_REGION_NUM	0x8
#else
#define MAX_REGION_NUM	CONFIG_CP_MAX_REGION_NUM
#endif

//fdt func
#define OF_BAD_ADDR	FDT_ADDR_T_NONE
#define OF_MAX_ADDR_CELLS	4
#define OF_CHECK_COUNTS(na, ns)	((na) > 0 && (na) <= OF_MAX_ADDR_CELLS && \
			(ns) > 0)

#define MAX_NAME_LEN	0x14
#define MAX_VALUE_LEN	0x100
#define MAX_RSVMEM_NUM	0x4
#define MAX_RES_NUM		0x4
#define MAX_CODE_NUM	0x32
#define MAX_HEADER_SIZE	0x600
#define EXPAND_FDT_SIZE		0x400
#define MAX_VERSION_SIZE	0x40
#define CP_VERSION_OFFSET	(MAX_HEADER_SIZE - MAX_VERSION_SIZE)

#if defined(CONFIG_SUPPORT_NR)
#define PARTITION_PREFIX	"nr_"
#elif defined(CONFIG_SUPPORT_LTE)
#define PARTITION_PREFIX	"l_"
#elif defined(CONFIG_SUPPORT_W)
#define PARTITION_PREFIX	"w_"
#else
#error have no modem type defined
#endif

#define NV_PARTITION_KEY	"nv"

enum {
	ignore_t = 0,
	load_t, //require load to ddr
	clear_t, //require clear
	dump_t,
	reg_t
};

enum {
	str_t,
	num_t,
};

enum {
	init_prop,
	bad_prop,
	filled_prop,
};

struct partition_name_info {
	char partition[MAX_NAME_LEN];
	char partition_bk[MAX_NAME_LEN];
};

struct property {
	char name[MAX_NAME_LEN];
	char value[MAX_VALUE_LEN];
	uint32_t len;
	uint32_t offset;
	uint32_t flag;
};

struct mem_resource {
	char name[MAX_NAME_LEN];
	uint64_t base __ALIGNED(8);
	uint32_t size;
};

struct region_info {
	struct mem_resource res;
	uint32_t flag; //load flag
};

struct boot_code {
	uint32_t count;
	uint32_t code[MAX_CODE_NUM];
};

struct rsvmem_info_conf {
	struct region_info rsvmem[MAX_RSVMEM_NUM];
};

struct boot_info_conf {
	struct boot_code bcode;
	struct region_info regions[MAX_REGION_NUM];
	struct mem_resource res[MAX_RES_NUM];
	uint32_t ex_info;
};

struct comm_info_conf {
	struct mem_resource res[MAX_RES_NUM];
	uint32_t ex_info;
};

struct cp_mem_coupling_info {
	uint32_t magic;
	uint32_t version;
	uint32_t length;
	uint32_t ex_info;
	struct rsvmem_info_conf rsvmem_info;
	struct boot_info_conf boot_info;
	struct comm_info_conf comm_info;
};

/* using the new format */
struct img_desc {
	char desc[MAX_NAME_LEN];
	uint32_t offs;
	uint32_t size;
};

struct iheader_desc {
	uint32_t magic;
	struct img_desc desc[1];
};

struct region_desc {
	char name[MAX_NAME_LEN];
	uint64_t base __ALIGNED(8);
	uint32_t size;
	uint32_t attr;
};

struct mem_coupling_info {
	uint32_t magic;
	uint32_t version;
	struct region_desc region[1];
};

union composite_header {
	struct iheader_desc iheader;
	struct cp_mem_coupling_info mem_decoup;
};

int fdt_fixup_cp_coupling_info(void *blob, const char* node_name);
int fdt_fixup_sp_info(void *blob);

#ifdef CONFIG_MEM_LAYOUT_DECOUPLING
int load_cp_boot_code(void *loader);
#endif

int sprd_del_modem_node_flag(void);
char *get_cp_version_info(void);

uint32_t fdt_getcells(const void *blob, int parentoffset, int *addr);
fdt_addr_t fdtdec_get_addr_size(const void *blob, int node, const char *prop_name, fdt_size_t *sizep);
u64 fdtdec_get_number(const fdt32_t *ptr, unsigned int cells);
u64 fdt_translate_address(const void *blob, int node_offset, const fdt32_t *in_addr);
fdt_addr_t fdtdec_get_addr_size_auto_parent(const void *blob, int parent, int node, const char *prop_name, \
						int index, fdt_size_t *sizep, bool translate);
fdt_addr_t fdtdec_get_addr_size(const void *blob, int node,	const char *prop_name, fdt_size_t *sizep);
fdt_addr_t fdtdec_get_addr_size_fixed(const void *blob, int node, \
					const char *prop_name, int index, int na, \
					int ns, fdt_size_t *sizep);
static struct of_bus *of_match_bus(const void *blob, int parentoffset);
static u64 of_bus_default_map(fdt32_t *addr, const fdt32_t *range, \
		int na, int ns, int pna);
static int of_bus_default_translate(fdt32_t *addr, u64 offset, int na);
#endif
