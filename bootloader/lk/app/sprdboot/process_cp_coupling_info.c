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
#include <libfdt_env.h>
#include <libfdt.h>
#include <errno.h>
#include <linux/kernel.h>
#include <linux/byteorder/little_endian.h>
#include "cp_mem_decoupling.h"
#include "boot_parse.h"
#include <asm/arch/sprd_reg.h>
#include <cp_boot.h>
#include "sprd_cpcmdline.h"
#include <sprd_boardid.h>
#include <android_ab.h>

#ifndef BASE_TO_APBASE
#define BASE_TO_APBASE(addr)	(addr)
#endif

#define DECOUPLING_INFO_PARTITION "modem"
//#define CP_MEM_DECOUP_DEBUG

int sprd_del_modem_node_flag(void) {
#ifdef CONFIG_ANDROIDBOOT_HARDWARE
	return sprd_get_wifi_mode();
#else
	return 0;
#endif
}

struct of_bus {
	const char	*name;
	const char	*addresses;
	int		(*match)(const void *blob, int parentoffset);
	uint32_t		(*count_cells)(const void *blob, int parentoffset,
				int *addrc);
	u64		(*map)(fdt32_t *addr, const fdt32_t *range,
				int na, int ns, int pna);
	int		(*translate)(fdt32_t *addr, u64 offset, int na);
};

static struct of_bus of_busses[] = {
	/* Default */
	{
		.name = "default",
		.addresses = "reg",
		.count_cells = fdt_getcells,
		.map = of_bus_default_map,
		.translate = of_bus_default_translate,
	},
};

/* returu size */
uint32_t fdt_getcells(const void *blob, int nodeoffset, int *addr)
{
	const fdt32_t *prop;
	uint32_t size = -1;

	if (addr != NULL)
		*addr = fdt_address_cells(blob, nodeoffset);

	prop = fdt_getprop(blob, nodeoffset, "#size-cells", NULL);
	if (prop)
		size = be32_to_cpup(prop);
	else
		size = 1;
	return size;
}

static u64 of_bus_default_map(fdt32_t *addr, const fdt32_t *range,
		int na, int ns, int pna)
{
	u64 cp, s, da;

	cp = fdtdec_get_number(range, na);
	s  = fdtdec_get_number(range + na + pna, ns);
	da = fdtdec_get_number(addr, na);

	debugf("OF: default map, cp=%llu, s=%llu, da=%llu\n", cp, s, da);

	if (da < cp || da >= (cp + s))
		return OF_BAD_ADDR;
	return da - cp;
}

static int of_bus_default_translate(fdt32_t *addr, u64 offset, int na)
{
	u64 a = fdtdec_get_number(addr, na);
	memset(addr, 0, na * 4);
	a += offset;
	if (na > 1)
		addr[na - 2] = cpu_to_fdt32(a >> 32);
	addr[na - 1] = cpu_to_fdt32(a & 0xffffffffu);

	return 0;
}

#if DEBUG
static void of_dump_addr(const char *s, const fdt32_t *addr, int na)
{
	dprintf(INFO,"%s", s);
	while(na--)
		dprintf(INFO," %08x", *(addr++));
	dprintf(INFO,"\n");
}
#else
static void of_dump_addr(const char *s, const fdt32_t *addr, int na) { }
#endif

u64 fdtdec_get_number(const fdt32_t *ptr, unsigned int cells)
{
	u64 number = 0;

	while (cells--)
		number = (number << 32) | fdt32_to_cpu(*ptr++);

	return number;
}

fdt_addr_t fdtdec_get_addr_size(const void *blob, int node,
				const char *prop_name, fdt_size_t *sizep)
{
	int ns = sizep ? (sizeof(fdt_size_t) / sizeof(fdt32_t)) : 0;

	return fdtdec_get_addr_size_fixed(blob, node, prop_name, 0,
					  sizeof(fdt_addr_t) / sizeof(fdt32_t),
					  ns, sizep);
}

fdt_addr_t fdtdec_get_addr_size_fixed(const void *blob, int node,
				      const char *prop_name, int index, int na,
				      int ns, fdt_size_t *sizep)
{
	const fdt32_t *prop, *prop_end;
	const fdt32_t *prop_addr, *prop_size, *prop_next_size;
	int len;
	fdt_addr_t addr;

	prop = fdt_getprop(blob, node, prop_name, &len);
	if (!prop) {
		errorf("(not found)\n");
		return (-1U);
	}
	prop_end = prop + (len / sizeof(*prop));

	prop_addr = prop + (index * (na + ns));
	prop_size = prop_addr + na;
	prop_next_size = prop_size + ns;
	if (prop_next_size > prop_end) {
		errorf("(not enough data: expected >= %d cells, got %d cells)\n", (u32)(prop_next_size - prop), ((u32)(prop_end - prop)));
		return (-1U);
	}

	addr = fdtdec_get_number(prop_addr, na);

	if (sizep) {
		*sizep = fdtdec_get_number(prop_size, ns);
		debugf("addr=%08llx, size=%llx\n", (unsigned long long)addr,
		      (unsigned long long)*sizep);
	} else {
		debugf("addr=%08llx\n", (unsigned long long)addr);
	}

	return addr;
}

static int of_translate_one(const void *blob, int parent, struct of_bus *bus,
		struct of_bus *pbus, fdt32_t *addr, int na, int ns, int pna, const char *rprop)
{
	const fdt32_t *ranges;
	int rlen;
	int rone;
	u64 offset = OF_BAD_ADDR;

	ranges = fdt_getprop(blob, parent, rprop, &rlen);
	if (ranges == NULL || rlen == 0) {
		offset = fdtdec_get_number(addr, na);
		memset(addr, 0, pna * 4);
		debugf("OF: no ranges, 1:1 translation\n");
		goto finish;
	}

	debugf("OF: walking ranges...\n");

	/* Now walk through the ranges */
	rlen /= 4;
	rone = na + pna + ns;
	for (; rlen >= rone; rlen -= rone, ranges += rone) {
		offset = bus->map(addr, ranges, na, ns, pna);
		if (offset != OF_BAD_ADDR)
			break;
	}
	if (offset == OF_BAD_ADDR) {
		debugf("OF: not found !\n");
		return 1;
	}
	memcpy(addr, ranges + na, 4 * pna);

 finish:
	of_dump_addr("OF: parent translation for:", addr, pna);
	dprintf(INFO,"OF: with offset: %llu\n", offset);

	/* Translate it into parent bus space */
	return pbus->translate(addr, offset, pna);
}

static struct of_bus *of_match_bus(const void *blob, int parentoffset)
{
	struct of_bus *bus;

	if (ARRAY_SIZE(of_busses) == 1)
		return of_busses;

	for (bus = of_busses; bus; bus++) {
		if (!bus->match || bus->match(blob, parentoffset))
			return bus;
	}

	/*
	 * We should always have matched the default bus at least, since
	 * it has a NULL match field. If we didn't then it somehow isn't
	 * in the of_busses array or something equally catastrophic has
	 * gone wrong.
	 */
	// assert(0);
	return NULL;
}

fdt_addr_t fdtdec_get_addr_size_auto_parent(const void *blob, int parent,
					    int node, const char *prop_name,
					    int index, fdt_size_t *sizep,
					    bool translate)
{
	int na, ns;

	debugf("%s: ", __func__);

	na = fdt_address_cells(blob, parent);
	if (na < 1) {
		debugf("(bad #address-cells)\n");
		return FDT_ADDR_T_NONE;
	}

	ns = fdt_size_cells(blob, parent);
	if (ns < 0) {
		debugf("(bad #size-cells)\n");
		return FDT_ADDR_T_NONE;
	}

	debugf("na=%d, ns=%d, ", na, ns);

	return fdtdec_get_addr_size_fixed(blob, node, prop_name, index, na,
					  ns, sizep);
}

extern int common_raw_read(char *part_name, uint64_t size,
			   uint64_t offset, char *buf);

static boot_image_required_t s_boot_image_cp_table[MAX_REGION_NUM];
static struct partition_name_info g_partition_info[MAX_REGION_NUM];

static char raw_decouple_data[MAX_HEADER_SIZE] __ALIGNED(64);

#ifdef CONFIG_SP_DDR_BOOT
static sp_ddr_boot_t sp_ddr_boot_info;

//extern int get_sp_bootcode_size(void);
//extern void* get_sp_bootcode_buf(void);

void *get_sp_ddr_boot_info(void)
{
	return (void*)&sp_ddr_boot_info;
}
#endif

static void print_fdt_node(struct fdt_header *fdt, int noffs)
{
#ifdef CP_MEM_DECOUP_DEBUG
	int index = 0;
	char fdt_cmd[MAX_VALUE_LEN] = {0};

	snprintf(fdt_cmd, sizeof(fdt_cmd), "fdt addr %p", fdt);
	run_command(fdt_cmd, 0);

	memset(fdt_cmd, 0, sizeof(fdt_cmd));
	strncpy(fdt_cmd, "fdt print ", sizeof(fdt_cmd) - 1);
	index = strlen(fdt_cmd);
	if (!fdt_get_path(fdt,
			 noffs,
			 fdt_cmd + index,
			 sizeof(fdt_cmd) - index)) {
		run_command(fdt_cmd, 0);
	}
#endif
	return;
}

static void prop_init(const char *name, uint32_t noffs, struct property *prop)
{
	int num;

	memset(prop, 0, sizeof(struct property));
	num = min(sizeof(prop->name) - 1, strlen(name));
	strncpy(prop->name, name, num);
	prop->offset = noffs;
}

static int fill_prop_value(int type,
			   void *value,
			   unsigned len,
			   struct property *prop)
{
	int n, lenp;
	uint32_t val;
	char *s, *p;

	if (len > sizeof(prop->value)) {
		errorf("the %s fill value too long, len = %d\n",
		       prop->name, len);
		return -EINVAL;
	}

	lenp = prop->len;
	p = prop->value + lenp;

	if (type == str_t) { //property value is string type
		strcpy(p, value);
		lenp += strlen(value) + 1;

	} else {
		n = len / sizeof(uint32_t);
		if (!n)
			n = 1;

		memset(p, 0, n * sizeof(uint32_t));

		p += n * sizeof(uint32_t);
		lenp += n * sizeof(uint32_t);

		for (s = (char *)value + len ; n > 0; n--) {
			s -= sizeof(uint32_t);
			p -= sizeof(uint32_t);
			memcpy(&val, s, sizeof(uint32_t));
			val = __cpu_to_be32(val);
			memcpy(p, &val, sizeof(uint32_t));
		}
	}

	prop->len = lenp;
	prop->flag = filled_prop;

	return 0;
}

static int fill_cell_prop_value(void *value,
				uint32_t size,
				int cell,
				struct property *prop)
{
	int i, delta;
	char tmp[64];
	char *p;

	if (!value || !prop) {
		errorf("param error\n");
		return -EINVAL;
	}

	p = (char *)value;
	//n = cell * sizeof(uint32_t) < size ? cell * sizeof(uint32_t) : size;
	delta = cell * sizeof(uint32_t) - sizeof(uint32_t);
	memset(tmp, 0, sizeof(tmp));
	for (i = cell; i > 0; i--) {
		memcpy(tmp + delta, p, sizeof(uint32_t));
		delta -= sizeof(uint32_t);
		p += sizeof(uint32_t);
	}

	return fill_prop_value(num_t, tmp, cell * sizeof(uint32_t), prop);
}

static int fill_reg_prop_value(uint64_t base,
			       uint64_t size,
			       int ac,
			       int sc,
			       struct property *prop)
{
	if (ac > 2 || sc > 2 || !prop) {
		errorf("param error\n");
		return -1;
	}

	if (fill_cell_prop_value(&base, sizeof(base), ac, prop)) {
		errorf("fill the base to %s prop failed\n", prop->name);
		return -1;
	}

	if (fill_cell_prop_value(&size, sizeof(size), sc, prop)) {
		errorf("fill the size to %s prop failed\n", prop->name);
		return -1;
	}

	return 0;
}

static int fill_ranges_prop_value(uint64_t offs,
				  uint64_t base,
				  uint64_t size,
				  int c_ac,
				  int ac,
				  int c_sc,
				  struct property *prop)
{
	if (ac > 2 || c_ac > 2 || c_sc > 2 || !prop) {
		errorf("param error\n");
		return -1;
	}

	if (fill_cell_prop_value(&offs, sizeof(offs), c_ac, prop)) {
		errorf("fill the offs to %s prop failed\n", prop->name);
		return -1;
	}

	if (fill_cell_prop_value(&base, sizeof(base), ac, prop)) {
		errorf("fill the base to %s prop failed\n", prop->name);
		return -1;
	}

	if (fill_cell_prop_value(&size, sizeof(size), c_sc, prop)) {
		errorf("fill the size to %s prop failed\n", prop->name);
		return -1;
	}

	return 0;
}

static int do_add_prop_to_fdt(struct fdt_header *fdt,
			      int mode,
			      struct property *prop)
{
	int ret;

	if (!prop || !fdt) {
		errorf("param error\n");
		return -1;
	}

	if (prop->flag != filled_prop) {
		errorf("%s prop is bad or no value, don't add to fdt\n",
		       prop->name);
		return -1;
	}

	if (mode != 0) {
		ret = fdt_appendprop(fdt,
				     prop->offset,
				     prop->name,
				     prop->value,
				     prop->len);
	} else {
		ret = fdt_setprop(fdt,
				  prop->offset,
				  prop->name,
				  prop->value,
				  prop->len);
	}

	if (ret < 0) {
		errorf("add prop %s to %s failed, reason: %s\n",
			prop->name,
			fdt_get_name(fdt, prop->offset, 0),
			fdt_strerror(ret));
		return -1;
	}

	return 0;
}

static void add_fdt_total_size(uint size, struct fdt_header *fdt)
{
	if (NULL != fdt &&
		(fdt_totalsize(fdt) <= fdt_off_dt_strings(fdt) +
		 fdt_size_dt_strings(fdt) + size)) {
		fdt_set_totalsize(fdt, fdt_totalsize(fdt) + size);
	}

}

static int fdt_del_reserved_mem(void *fdt, const char *c_name)
{
	int offset;
	int ret = 0;

	offset = fdt_path_offset(fdt, "/reserved-memory");
	if (offset < 0) {
		errorf("cann't find reserved-memory node");
		return -1;
	}

	offset = fdt_subnode_offset(fdt, offset, c_name);
	if (offset == -FDT_ERR_NOTFOUND) {
		errorf(" %s is not exist\n", c_name);
		return -1;
	} else {
		ret = fdt_del_node(fdt, offset);
		if (ret < 0)
			errorf("del %s failed\n", c_name);
	}
	return 0;
}

static int fdt_close_dmc_mpu_node(struct fdt_header *fdt, const char *compatible)
{
	int offset;
	int len;
	int ret = 0;
	const char *prop_value;

	offset = fdt_node_offset_by_compatible(fdt, 0, compatible);
	if (offset < 0) {
		debugf("compatible = %s is not exist\n", compatible);
		return -1;
	}

	prop_value = fdt_getprop(fdt, offset, "status", &len);
	if (prop_value == NULL) {
		debugf("prop_value = %s, len=%d\n",prop_value, len);
		return -1;
        } else if (strstr(prop_value, "okay")) {
		ret = fdt_setprop(fdt, offset, "status",
				"disabled", sizeof("disabled"));
		if(ret < 0) {
			errorf("%s set prop fail %d\n",prop_value, ret);
			return -1;
		}
	} else {
		debugf("prop_value = %s, len=%d\n",prop_value, len);
		return -1;
        }
	return 0;
}

static int fdt_del_sipc_node(struct fdt_header *fdt, const char *compatible, const char *s_value)
{
        int ret = 0;
        int offset,offset_old;

        offset = fdt_node_offset_by_compatible(fdt, 0, compatible);
        if (offset < 0) {
                debugf("compatible = %s is not exist\n", compatible);
                return -1;
        }

        offset = fdt_node_offset_by_prop_value(fdt, offset, "label", s_value, strlen(s_value) + 1);
        offset_old = fdt_node_offset_by_prop_value(fdt, offset, "sprd,name", s_value, strlen(s_value) + 1);
        if ((offset < 0) && (offset_old < 0)) {
                debugf("label/sprd,name = %s is not exist\n", s_value);
                return -1;

        } else if (offset < 0) {
                ret = fdt_del_node(fdt, offset_old);
                if (ret < 0)
                        errorf("del sprd,name = %s failed\n", s_value);

        } else if (offset_old < 0) {
                ret = fdt_del_node(fdt, offset);
                if (ret < 0)
                        errorf("del label = %s failed\n", s_value);
        }
        return 0;
}

static int fdt_del_spipe_node(struct fdt_header *fdt, const char *compatible, const char *s_value)
{
	int ret = 0;
	int offset,offset_old;

	offset = fdt_node_offset_by_compatible(fdt, 0, compatible);
	if (offset < 0) {
		debugf("compatible = %s is not exist\n", compatible);
		return -1;
	}

	offset = fdt_node_offset_by_prop_value(fdt, offset, "label", s_value, strlen(s_value) + 1);
	offset_old = fdt_node_offset_by_prop_value(fdt, offset, "sprd,name", s_value, strlen(s_value) + 1);
	if ((offset < 0) && (offset_old < 0)) {
		debugf("label/sprd,name = %s is not exist\n", s_value);
		return -1;

	} else if (offset < 0) {
		ret = fdt_del_node(fdt, offset_old);
		if (ret < 0)
			errorf("del sprd,name = %s failed\n", s_value);

	} else if (offset_old < 0) {
		ret = fdt_del_node(fdt, offset);
		if (ret < 0)
			errorf("del label = %s failed\n", s_value);
	}
	return 0;
}

static int fdt_del_cproc_use_decoup(struct fdt_header *fdt, const char *compatible,  const char *s_value)
{
	int ret = 0;
	int offset;
	int len;
	const char *prop_value;

	offset = fdt_node_offset_by_compatible(fdt, 0, compatible);
	if (offset < 0) {
		debugf("compatible = %s is not exist\n", compatible);
		return -1;
	}

	offset = fdt_node_offset_by_prop_value(fdt, offset, "sprd,name", s_value, strlen(s_value) + 1);
	if (offset < 0) {
		debugf("sprd,name = %s is not exist\n", s_value);
		return -1;
	}

	prop_value = fdt_getprop(fdt, offset, "sprd,decoup", &len);
	if (NULL == prop_value) {
		debugf("decoup is not exist\n");
		return -1;
	} else {
		debugf("prop_value = %s \n",prop_value);
		if (0 == strncmp(prop_value, "cproc-use-decoup", len)) {
			ret = fdt_del_node(fdt, offset);
			if (ret < 0)
				errorf("del node by offs failed\n");
		}
	}
	return 0;
}

static int fdt_del_node_for_modem_remove(struct fdt_header *fdt, const char *compatible)
{
	int ret = 0;
	int offset;

	offset = fdt_node_offset_by_compatible(fdt, 0, compatible);
	if (offset < 0) {
		debugf("compatible = %s is not exist\n", compatible);
		return -1;
	} else {
		ret = fdt_del_node(fdt, offset);
		if (ret < 0)
			errorf("del compatible = %s failed\n", compatible);
	}
	return 0;
}

static int fdt_del_seth_node(struct fdt_header *fdt, const char *compatible)
{
	int nodeoff = 0;
	int ret = 0;

	while ((nodeoff = fdt_node_offset_by_compatible(fdt, 0, compatible)) >= 0) {
		ret = fdt_del_node(fdt, nodeoff);
		if (ret < 0)
			errorf("del seth failed\n");
	}
	return 0;
}

static int do_parse_decoup_sipc_info(struct cp_mem_coupling_info *info, struct fdt_header *fdt)
{
	int i;
	int p_offs, offs;
	uint64_t base;
	uint32_t deta, size;
	uint32_t na = 2, ns = 1;
	uint32_t c_na = 2, c_ns = 1;
	char name[64];
	char value[64];
	struct property reg, c_reg, ranges;
	struct comm_info_conf *comm_info = NULL;

	if(!info || !fdt) {
		errorf("param error\n");
		return -1;
	}

	comm_info = &info->comm_info;

	strncpy(name, "sprd,decoup", sizeof(name));
	strncpy(value, "sipc-use-decoup", sizeof(value));
	offs = fdt_node_offset_by_prop_value(fdt, 0, name, value, strlen(value) + 1);
	if(offs < 0) {
		debugf("the sipc info already exist, don't need to parse\n");
		return 1;
	}

	p_offs = fdt_parent_offset(fdt, offs);
	if(p_offs < 0) {
		na = 2;
		ns = 1;
	}

	debugf("start update sipc node info\n");
	print_fdt_node(fdt, offs);

	ns = fdt_getcells(fdt, p_offs, &na);
	debugf("a_cells = %d, s_cells = %d\n", na, ns);

	c_ns = fdt_getcells(fdt, offs, &c_na);
	debugf("ca_cells: %d, cs_cells: %d\n", c_na, c_ns);

	prop_init("reg", 0, &c_reg);
	prop_init("reg", offs, &reg);
	prop_init("ranges", offs, &ranges);

	for(i = 0; comm_info->res[i].size > 0; i++) {
		base = comm_info->res[i].base;
		size = comm_info->res[i].size;
		deta = comm_info->res[i].base - 0x80000000;
		debugf("%s: i = %d, base = 0x%llx, size = 0x%x\n",comm_info->res[i].name, i, base, size);
		if (i == 0) {
			fill_reg_prop_value(base, size, na, ns, &reg);
		}

		fill_reg_prop_value(deta, size, c_na, c_ns, &c_reg);

		fill_ranges_prop_value(deta, base, size, c_na, na, c_ns, &ranges);
	}

	if(do_add_prop_to_fdt(fdt, 0, &reg) < 0)
		goto err;

	if(do_add_prop_to_fdt(fdt, 1, &ranges) < 0)
		goto err;

	p_offs = fdt_first_subnode(fdt, offs);
	if(p_offs < 0) {
		errorf("the sipc child node not found\n");
		goto err;
	}
	c_reg.offset = p_offs;
	if(do_add_prop_to_fdt(fdt, 0, &c_reg) < 0)
		goto err;

	debugf("update sipc node info done\n");
	print_fdt_node(fdt, offs);

	return 0;

err:
	print_fdt_node(fdt, offs);
	return -1;
}

static int create_cproc_subnode(struct fdt_header *fdt,
				int offs,
				char *name,
				uint64_t p_base,
				uint64_t base,
				uint32_t size)
{
	int p_offs, c_offs, delta;
	uint32_t na, ns;
	uint32_t c_na, c_ns;
	char node_name[64];
	struct property prop;

	p_offs = fdt_parent_offset(fdt, offs);
	if (p_offs < 0) {
		na = 2;
		ns = 1;
	}

	ns = fdt_getcells(fdt, p_offs, &na);
	c_ns = fdt_getcells(fdt, offs, &c_na);

	delta = base - p_base;
	snprintf(node_name, sizeof(node_name), "%s@%x", name, delta);
	c_offs = fdt_add_subnode(fdt, offs, node_name);
	if (c_offs < 0) {
		errorf("create %s node failed\n", node_name);
		return -1;
	}

	prop_init("reg", c_offs, &prop);
	fill_reg_prop_value(delta, size, c_na, c_ns, &prop);
	if (do_add_prop_to_fdt(fdt, 0, &prop) < 0)
		goto err;

	prop_init("cproc,name", c_offs, &prop);
	fill_prop_value(str_t, name, strlen(name), &prop);
	if (do_add_prop_to_fdt(fdt, 0, &prop) < 0)
		goto err;

	prop_init("ranges", offs, &prop);
	fill_ranges_prop_value(delta, base, size, c_na, na, c_ns, &prop);
	if (do_add_prop_to_fdt(fdt, 1, &prop) < 0)
		goto err;

	return c_offs;

err:
	fdt_del_node(fdt, c_offs);

	return -1;
}

static int do_parse_decoup_info_v1(struct cp_mem_coupling_info *info,
				   struct fdt_header *fdt, const char *name_value)
{
	int i, val = 1;
	int p_offs, offs;
	int is_new_format;
	int len;
	uint64_t base;
	uint32_t size;
	uint32_t cp_base = 0;
	uint32_t na, ns;
	char name[64];
	char value[64];
	const char *prop_value;
	struct property prop;
	struct region_info *region;
	struct mem_resource *res;

	if (!info || !fdt) {
		errorf("param error\n");
		return -1;
	}

#if !defined(CONFIG_KERNEL_BOOT_CP)
#ifdef PSCP_IRAM_ADDRESS
	if (!strncmp(name_value, "cptl", 4))
		load_cp_boot_code((void*)PSCP_IRAM_ADDRESS);
#endif
#ifdef CONFIG_SUPPORT_NR
	#ifdef PHYCP_IRAM_ADDRESS
	if (!strncmp(name_value, "phy", 3))
		load_cp_boot_code((void*)PHYCP_IRAM_ADDRESS);
	#endif
#endif
#endif

	res = info->boot_info.res;
	region = info->boot_info.regions;

	is_new_format = 0;

	strncpy(name, "sprd,name", sizeof(name));
	strncpy(value, name_value, sizeof(value));
	offs = fdt_node_offset_by_prop_value(fdt, 0, name, value, strlen(value) + 1);
	if (offs < 0) {
		debugf("the node is not exist, don't need to parse\n");
		return 1;
	}

	prop_value = fdt_getprop(fdt, offs, "sprd,decoup",&len);
	if(NULL == prop_value) {
		debugf("decoup is not exist, don't need to parse\n");
		return 1;
	} else {
		debugf("prop_value = %s \n",prop_value);
		if (0 != strncmp(prop_value, "cproc-use-decoup", len)) {
			if (0 != strncmp(prop_value, "cproc-use-decoup-v1", len)) {
				debugf("decoup value error, don't need to parse\n");
				return 1;
			}
			is_new_format = 1;
		}
	}

	add_fdt_total_size(EXPAND_FDT_SIZE, fdt);

	p_offs = fdt_parent_offset(fdt, offs);
	if (p_offs < 0) {
		na = 2;
		ns = 1;
	}

	debugf("start update cproc node info\n");
	print_fdt_node(fdt, offs);

	ns = fdt_getcells(fdt, p_offs, &na);
	debugf("a_cells: %d, s_cells: %d\n", na, ns);

	if (!is_new_format) {
		struct boot_code *bcode;

		bcode = &info->boot_info.bcode;
		prop_init("sprd,iram-dsize", offs, &prop);
		fill_prop_value(num_t, &bcode->count, sizeof(uint32_t), &prop);
		if(do_add_prop_to_fdt(fdt, 0, &prop) < 0)
			errorf("add iram-dsize prop to cproc node failed\n");

		prop_init("sprd,iram-data", offs, &prop);
		fill_prop_value(num_t, bcode->code, bcode->count * sizeof(uint32_t), &prop);
		if(do_add_prop_to_fdt(fdt, 0, &prop) < 0)
			errorf("add iram-data prop to cproc node failed\n");
	}

	for (i = 0; res[i].size > 0; i++) {
		base = BASE_TO_APBASE(res[i].base);
		size = res[i].size;

		if (!strncmp(res[i].name, "cp-mem", 6)) {
			prop_init("reg", offs, &prop);
			cp_base = base;
			fill_reg_prop_value(base, size, na, ns, &prop);

		} else if (!strncmp(res[i].name, "mini-dump", 9)) {
			if (is_new_format) {
				prop_init("sprd,mini-dump", offs, &prop);
				fill_reg_prop_value(base, size, 1, 1, &prop);
			} else {
				prop_init("reg", offs, &prop);
				fill_reg_prop_value(base, size, na, ns, &prop);
			}

		} else {
			continue;
		}

		if (do_add_prop_to_fdt(fdt, 1, &prop) < 0)
			goto err0;
	}

	if (!cp_base) {
		errorf("cp base is 0\n");
		goto err0;
	}

	prop_init("#address-cells", offs, &prop);
	fill_prop_value(num_t, &val, sizeof(uint32_t), &prop);
	if (do_add_prop_to_fdt(fdt, 0, &prop) < 0)
		goto err0;

	prop_init("#size-cells", offs, &prop);
	fill_prop_value(num_t, &val, sizeof(uint32_t), &prop);
	if (do_add_prop_to_fdt(fdt, 0, &prop) < 0)
		goto err0;

	for (i = 0; i < MAX_REGION_NUM; i++, region++) {
		if (strlen(region->res.name) > 0) {
			base = BASE_TO_APBASE(region->res.base);
			size = region->res.size;
			create_cproc_subnode(fdt,
					     offs,
					     region->res.name,
					     cp_base, base, size);

		} else {
			break;
		}
	}

	debugf("update cproc node info done\n");
	print_fdt_node(fdt, offs);

	return 0;

err0:
	print_fdt_node(fdt, offs);
	return -1;
}

static int fixup_image_partition_name(const char *rname,
				      struct partition_name_info *pinfo) {
	int num, index = 0;
	char *p = pinfo->partition;

	if (!rname || !pinfo) {
		errorf("param error\n");
		return -1;
	}

	strcpy(pinfo->partition, PARTITION_PREFIX);
	num = min(sizeof(pinfo->partition) - strlen(pinfo->partition) - 1,
		  strlen(rname));
	strncat(pinfo->partition, rname, num);
	index = strlen(pinfo->partition) - strlen(NV_PARTITION_KEY);
	if (index >= 0) {
		p = pinfo->partition + index;
	}

	if (!strcmp(p, NV_PARTITION_KEY) && strcmp(pinfo->partition + strlen(PARTITION_PREFIX), "deltanv")) {
		if (!strcmp(pinfo->partition + strlen(PARTITION_PREFIX), "runnv"))
			strcpy(pinfo->partition + strlen(PARTITION_PREFIX), "runtimenv");

		strcpy(pinfo->partition_bk, pinfo->partition);
		num = min(sizeof(pinfo->partition) -
			  strlen(pinfo->partition) - 1, strlen("1"));
		strncat(pinfo->partition, "1", num);
		num = min(sizeof(pinfo->partition_bk) -
			  strlen(pinfo->partition_bk) - 1, strlen("2"));
		strncat(pinfo->partition_bk, "2", num);
	}

	return 0;
}

static int do_parse_decoup_info(struct mem_coupling_info * info,
				struct fdt_header * fdt)
{
	int p_offs, offs;
	uint32_t val = 1;
	uint32_t na, ns;
	uint64_t base = 0;
	char name[64];
	char value[64];
	struct property prop;
	struct region_desc *region;

	debugf("start parse decoupling info\n");

	if (!info || !fdt) {
		errorf("param error\n");
		return -1;
	}

	strncpy(name, "sprd,decoup", sizeof(name));
	strncpy(value, "cproc-use-decoup-v1", sizeof(value));
	offs = fdt_node_offset_by_prop_value(fdt, 0, name, value, strlen(value) + 1);
	if (offs < 0) {
		debugf("the cp memory info already exist, don't need to parse\n");
		return 1;
	}

	add_fdt_total_size(EXPAND_FDT_SIZE, fdt);

	p_offs = fdt_parent_offset(fdt, offs);
	if (p_offs < 0) {
		na = 2;
		ns = 1;
	}

	if (info->magic != DECOUPLE_MAGIC_INFO) {
		errorf("format error, magic = 0x%x\n", info->magic);
		goto err0;
	}

	prop_init("#address-cells", offs, &prop);
	fill_prop_value(num_t, &val, sizeof(uint32_t), &prop);
	if (do_add_prop_to_fdt(fdt, 0, &prop) < 0)
		goto err0;

	prop_init("#size-cells", offs, &prop);
	fill_prop_value(num_t, &val, sizeof(uint32_t), &prop);
	if (do_add_prop_to_fdt(fdt, 0, &prop) < 0)
		goto err0;

	ns = fdt_getcells(fdt, p_offs, &na);
	debugf("addr cells: %d, size cells: %d\n", na, ns);

	for (region = info->region; region->size != 0; region++) {
		if (region->attr == reg_t && !strcmp("cp-mem", region->name)) {
			base = BASE_TO_APBASE(region->base);
			break;
		}
	}

	for (region = info->region; region->size != 0; region++) {
		uint64_t addr = BASE_TO_APBASE(region->base);
		uint32_t size = region->size;

		if (region->attr == reg_t) {
			prop_init("reg", offs, &prop);
			fill_reg_prop_value(addr, size, na, ns, &prop);
			if (do_add_prop_to_fdt(fdt, 0, &prop) < 0)
				goto err0;

		} else if (region->attr == dump_t) {
			snprintf(name, sizeof(name), "sprd,%s", region->name);
			prop_init(name, offs, &prop);
			fill_reg_prop_value(addr, size, 1, 1, &prop);
			if (do_add_prop_to_fdt(fdt, 0, &prop) < 0)
				goto err0;

		} else if (region->attr == load_t || region->attr == clear_t) {
			create_cproc_subnode(fdt,
					     offs,
					     region->name,
					     base,
					     addr,
					     size);

		}

		continue;
	}

	print_fdt_node(fdt, offs);

	return 0;

err0:
	print_fdt_node(fdt, offs);

	return -1;
}

static int load_cp_coupling_info(void)
{
	int ret;
	uint64_t normal_boot_offset = 0;
	struct partition_name_info mp;
	boot_image_required_t i_info;
	char modem_part[20];

	get_slot_ab(modem_part, DECOUPLING_INFO_PARTITION);

	ret = fixup_image_partition_name(modem_part, &mp);
	if (!ret) {
		memset(&i_info, 0, sizeof(boot_image_required_t));
		strcpy(i_info.partition, mp.partition);
		i_info.mem_addr = (unsigned long)raw_decouple_data;
		i_info.size = sizeof(raw_decouple_data);

		int is_sci;
		size_t total_len;
		size_t exe_size;

		normal_boot_offset = get_modem_img_info(&i_info,
							0,
							&is_sci,
							&total_len,
							&exe_size);
		if (0 != common_raw_read(i_info.partition,
					 i_info.size,
					 normal_boot_offset,
					 (char *)i_info.mem_addr)) {
			errorf("read %s partition fail\n", i_info.partition);
		}
	}

	return 0;
}

int fdt_fixup_cp_coupling_info(void *blob, const char* node_name)
{
	int ret = -1, has_dbg_region = 0;
	union composite_header *c_header;
	struct img_desc *i_desc;
	struct iheader_desc *ih_desc;
	struct fdt_header *fdt;
	struct mem_coupling_info *c_info;

	if (!blob)	{
		errorf("blob is NULL\n");
		return -1;
	}

	fdt = (struct fdt_header *)blob;
	if (sprd_del_modem_node_flag()) {
		fdt_del_spipe_node(fdt, "sprd,spipe", "stty_lte");
		fdt_del_spipe_node(fdt, "sprd,spipe","sdiag_lte");
		fdt_del_spipe_node(fdt, "sprd,spipe","spipe_lte");
		fdt_del_sipc_node(fdt, "sprd,sipc", "sipc-lte");
		fdt_del_node_for_modem_remove(fdt, "sprd,spool");
		fdt_del_node_for_modem_remove(fdt, "sprd,sipx");
		fdt_del_node_for_modem_remove(fdt, "sprd,slog_bridge");
		fdt_del_seth_node(fdt, "sprd,seth");
		fdt_del_cproc_use_decoup(fdt, "sprd,scproc_pubcp", "cptl");
		fdt_del_reserved_mem(fdt, "cp-modem");
		fdt_del_reserved_mem(fdt, "cp-mem");
		fdt_del_reserved_mem(fdt, "sipc-mem");
		fdt_del_spipe_node(fdt, "sprd,spipe", "spipe_nr");
		fdt_del_spipe_node(fdt, "sprd,spipe", "sdiag_nr");
		fdt_del_spipe_node(fdt, "sprd,spipe", "stty_nr");
		fdt_del_spipe_node(fdt, "sprd,spipe", "slog_nr");
		fdt_del_spipe_node(fdt, "sprd,spipe", "snv_nr");
		fdt_del_sipc_node(fdt, "sprd,sipc", "sipc-pscp");
		fdt_del_sipc_node(fdt, "sprd,sipc", "sipc-phycp");
		fdt_close_dmc_mpu_node(fdt, "sprd,qogirn6pro-dmc-mpu");
		fdt_close_dmc_mpu_node(fdt, "sprd,qogirl6-dmc-mpu");
		fdt_close_dmc_mpu_node(fdt, "sprd,sharkl3-dmc-mpu");
		fdt_close_dmc_mpu_node(fdt, "sprd,sharkl5pro-dmc-mpu");
		fdt_close_dmc_mpu_node(fdt, "sprd,sharkl5-dmc-mpu");
		fdt_close_dmc_mpu_node(fdt, "sprd,dmc-mpu");
	}
	if (raw_decouple_data[0] == 0)
		load_cp_coupling_info();

	c_header = (union composite_header *)raw_decouple_data;

	ih_desc = &c_header->iheader;
	if (ih_desc->magic != HEADER_MAGIC_INFO) {
		struct cp_mem_coupling_info *decoup_data;

		debugf("magic = 0x%x, it's not new format\n", ih_desc->magic);
		decoup_data = &c_header->mem_decoup;
		if (decoup_data->magic == DECOUPLE_MAGIC_INFO) {
			int i;

			for (i = 0; i < MAX_RSVMEM_NUM; i++) {
				if (!strcmp(decoup_data->rsvmem_info.rsvmem[i].res.name, "debug-mem")) {
					has_dbg_region = 1;
					break;
				}
			}

			if (!has_dbg_region)
				fdt_fixup_cp_reserved_mem(fdt);

			do_parse_decoup_sipc_info(decoup_data, fdt);

			if ((!strcmp("modem", node_name)) || (!strcmp("modem_b", node_name))
					|| (!strcmp("modem_a", node_name)))
				return do_parse_decoup_info_v1(decoup_data, fdt,"cptl");
			#ifdef CONFIG_SUPPORT_NR
			if(!strcmp("nrphy", node_name))
				return do_parse_decoup_info_v1(decoup_data, fdt,"nrphy");
			if(!strcmp("v3phy", node_name))
				return do_parse_decoup_info_v1(decoup_data, fdt,"v3phy");
			if(!strcmp("phy", node_name))
				return do_parse_decoup_info_v1(decoup_data, fdt,"phy");
			#endif

		} else {
			goto err;
		}
	}

	for (i_desc = ih_desc->desc; i_desc->size != 0; i_desc++) {
		if (!strcmp("decoup-desc", i_desc->desc)) {
			debugf("decoup desc was found\n");
			c_info = (struct mem_coupling_info *)((char *)raw_decouple_data + i_desc->offs);
			ret = do_parse_decoup_info(c_info, fdt);

		} else if (!strcmp("debug-desc", i_desc->desc)) {
			has_dbg_region = 1;
		}
	}

	if (!ret && !has_dbg_region)
		fdt_fixup_cp_reserved_mem(fdt);

	return ret;

err:
	errorf("cp memory decoupling failed\n");
	return -1;
}

#ifdef CONFIG_SP_DDR_BOOT
int fdt_fixup_sp_info(void *blob)
{
	int offs, parent;
	struct fdt_header *fdt;
	int i;
	struct property prop;
	sp_ddr_boot_t *p_sp_ddr_bt_info = get_sp_ddr_boot_info();

	if (!blob) {
		errorf("param error\n");
		return -1;
	}
	fdt = (struct fdt_header *)blob;

	//find pmic node
	parent = offs = fdt_node_offset_by_prop_value(fdt, 0, "sprd,name", "pmic", sizeof("pmic"));
	if (offs < 0) {
		errorf("pmic node not exist!!!\n");
		return -1;
	}

	/* init iram data size */
	p_sp_ddr_bt_info->bootcode_word_num = get_sp_bootcode_size();
	prop_init("sprd,iram-dsize", offs, &prop);
	fill_prop_value(num_t, &p_sp_ddr_bt_info->bootcode_word_num, sizeof(uint32_t), &prop);
	if(do_add_prop_to_fdt(fdt, 0, &prop) < 0)
		errorf("add iram-dsize prop to pmic node failed\n");

	/* init iram data */
	p_sp_ddr_bt_info->bootcode = get_sp_bootcode_buf();
	prop_init("sprd,iram-data", offs, &prop);
	fill_prop_value(num_t, p_sp_ddr_bt_info->bootcode, p_sp_ddr_bt_info->bootcode_word_num * sizeof(uint32_t), &prop);
	if(do_add_prop_to_fdt(fdt, 0, &prop) < 0)
		errorf("add iram-data prop to pmic node failed\n");

	for (i = 0; i < p_sp_ddr_bt_info->bootcode_word_num; i++) {
		//debugf("sp_ddr_bootcode[%d]=0x%x \n", i, p_sp_ddr_bt_info->bootcode[i]);
	}
	debugf("sp_ddr_bootcode[%d]=0x%x \n", i-1, p_sp_ddr_bt_info->bootcode[i-1]);

	//get sp_iram_addr
	p_sp_ddr_bt_info->sp_iram_addr = fdtdec_get_addr_size(fdt, offs, "reg",NULL);
	if (p_sp_ddr_bt_info->sp_iram_addr == FDT_ADDR_T_NONE) {
		errorf("pmic reg prop not exist,sp_iram_addr = 0x%lx !!\n", p_sp_ddr_bt_info->sp_iram_addr);
		return -1;
	}
	debugf("sp_iram_addr = 0x%lx \n",p_sp_ddr_bt_info->sp_iram_addr);

	//find pm_sys node
	offs = fdt_node_offset_by_prop_value(fdt, 0, "cproc,name", "pm_sys", sizeof("pm_sys"));
	if (offs < 0) {
		errorf("pm_sys node not exist!!!\n");
		return -1;
	} else {
		//get sp_ddr_addr sp_ddr_img_size
		p_sp_ddr_bt_info->sp_ddr_addr = fdtdec_get_addr_size_auto_parent(fdt,
										parent,
										offs,
										"reg",
										0,
										&p_sp_ddr_bt_info->sp_ddr_img_size,
										false);
		if (p_sp_ddr_bt_info->sp_ddr_addr == FDT_ADDR_T_NONE) {
			errorf("pm_sys reg prop not exist,sp_ddr_addr = 0x%lx sp_ddr_img_size = 0x%lx!!\n",
				p_sp_ddr_bt_info->sp_ddr_addr,
				p_sp_ddr_bt_info->sp_ddr_img_size);
		}
		debugf("sp_ddr_addr = 0x%lx sp_ddr_img_size = 0x%lx \n",p_sp_ddr_bt_info->sp_ddr_addr, p_sp_ddr_bt_info->sp_ddr_img_size);
	}

	return 0;
}
#endif

static boot_image_required_t * get_cp_load_table_v0(struct cp_mem_coupling_info *data)
{
	int i, j;
	struct region_info *region;

	if (DECOUPLE_MAGIC_INFO != data->magic) {
		errorf("decoupling data format error, magic = 0x%x\n",
		       data->magic);
		return NULL;
	}

	region = &data->boot_info.regions[0];
	for (i = 0, j = 0; i < MAX_REGION_NUM; i++) {
		if (strlen(region->res.name) > 0 && region->flag == load_t) {
			fixup_image_partition_name(region->res.name, &g_partition_info[j]);
			if (strlen(g_partition_info[j].partition) > 0)
				strcpy(s_boot_image_cp_table[j].partition, g_partition_info[j].partition);
			if (strlen(g_partition_info[j].partition_bk) > 0)
				strcpy(s_boot_image_cp_table[j].bak_partition, g_partition_info[j].partition_bk);

			s_boot_image_cp_table[j].size = region->res.size;
			s_boot_image_cp_table[j].mem_addr = BASE_TO_APBASE(region->res.base);
			j++;
		}

		region++;
	}

	return s_boot_image_cp_table;
}

boot_image_required_t * get_cp_load_table(void)
{
	int i;
	struct img_desc *i_desc;
	struct iheader_desc *ih_desc;
	union composite_header *c_header;
	struct mem_coupling_info *c_info;
	struct region_desc *region;

	if (raw_decouple_data[0] == 0)
		load_cp_coupling_info();

	c_header = (union composite_header *)raw_decouple_data;

	ih_desc = &c_header->iheader;
	if (ih_desc->magic != HEADER_MAGIC_INFO) {
		debugf("magic = 0x%x, it's not new format\n", ih_desc->magic);
		return get_cp_load_table_v0(&c_header->mem_decoup);
	}

	for (i_desc = ih_desc->desc; i_desc->size != 0; i_desc++) {
		if (!strcmp("decoup-desc", i_desc->desc)) {
			debugf("decoup desc was found\n");
			c_info = (struct mem_coupling_info *)((char *)raw_decouple_data + i_desc->offs);
			for (i = 0, region = c_info->region;
			     region->size > 0;
			     region++) {
				if (region->attr != load_t)
					continue;
				fixup_image_partition_name(region->name, &g_partition_info[i]);
				if (strlen(g_partition_info[i].partition) > 0)
					strcpy(s_boot_image_cp_table[i].partition, g_partition_info[i].partition);
				if (strlen(g_partition_info[i].partition_bk) > 0)
					strcpy(s_boot_image_cp_table[i].bak_partition, g_partition_info[i].partition_bk);

				s_boot_image_cp_table[i].size = region->size;
				s_boot_image_cp_table[i].mem_addr = BASE_TO_APBASE(region->base);
				i++;
			}
			return s_boot_image_cp_table;
		}
	}

	errorf("failed to get the cp load table\n");
	return NULL;
}

char *get_cp_version_info(void)
{
	static char cp_version_info[MAX_VERSION_SIZE] = {0};
	load_cp_coupling_info();

	strncpy(cp_version_info, (char *)(raw_decouple_data + CP_VERSION_OFFSET), MAX_VERSION_SIZE - 1);
	if (cp_version_info[0] == '\0')
		return cp_version_info;
	dprintf(INFO,"cp_version_info=%s\n", cp_version_info);
	if((strstr(cp_version_info, "FM_BASE_TRUNK_V2") == NULL) && (strstr(cp_version_info, "5G_MODEM_V2") == NULL)) {
		cp_version_info[0] = '\0';
		errorf("failed to get the cp version\n");
	}
	return cp_version_info;
}

#ifdef CONFIG_MEM_LAYOUT_DECOUPLING
int load_cp_boot_code(void *loader)
{
	int ret = -EAGAIN;
	struct img_desc *i_desc;
	struct iheader_desc *ih_desc;
	union composite_header *c_header;
	struct cp_mem_coupling_info *decoup_data;

	if (!loader) {
		ret = -EINVAL;
		goto err;
	}

	debugf("load cp boot code, addr = 0x%p\n", loader);

	c_header = (union composite_header *)raw_decouple_data;

	ih_desc = &c_header->iheader;
	if (ih_desc->magic != HEADER_MAGIC_INFO) {
		struct boot_info_conf *bconf;
		debugf("magic = 0x%x, it's not new format\n", ih_desc->magic);
		decoup_data = &c_header->mem_decoup;
		if (decoup_data->magic != DECOUPLE_MAGIC_INFO) {
			ret = -ENOMEDIUM;
			goto err;
		}

		bconf = &decoup_data->boot_info;
		memcpy(loader, bconf->bcode.code, bconf->bcode.count * sizeof(uint32_t));
		return 0;
	}

	if (ih_desc->magic != HEADER_MAGIC_INFO) {
		ret = -ENOMEDIUM;
		goto err;
	}

	for (i_desc = ih_desc->desc; i_desc->size != 0; i_desc++) {
		if (!strcmp("boot-code", i_desc->desc)) {
			debugf("boot-code was found\n");
			memcpy(loader,
			       (char *)raw_decouple_data + i_desc->offs,
			       i_desc->size);
			return 0;
		}
	}

err:
	errorf("load cp boot code error, err = %d\n", ret);
	return ret;
}
#endif

#ifdef CONFIG_SP_DDR_BOOT
extern int g_res_spbootcode_info;
extern void *get_sp_bootcode_buf(void);
extern int get_sp_bootcode_size(void);

int load_sp_boot_code(void)
{
	sp_ddr_boot_t *pinfo = get_sp_ddr_boot_info();
	u32 *sp_loader_copy = get_sp_bootcode_buf();
	int sp_loader_size = get_sp_bootcode_size();

	if(g_res_spbootcode_info == 0){
	memcpy((void*)pinfo->sp_iram_addr, pinfo->bootcode,
		pinfo->bootcode_word_num * sizeof(uint32_t));
	dprintf(INFO,"load sp bootcode form cproc");
        }
#ifdef SP_IRAM_ADDR
	else{
	memcpy((void *)SP_IRAM_ADDR,sp_loader_copy,sp_loader_size*sizeof(uint32_t));
	dprintf(INFO,"load sp bootcode form modem_loader");
	}
#endif
	return 0;
}
#endif

void *parse_cpcmdline_addr(void)
{
	uint64_t addr = 0;
	struct img_desc *i_desc;
	struct iheader_desc *ih_desc;
	union composite_header *c_header;
	struct cp_mem_coupling_info *decoup_data;

	debugf("parse cpcmdline addr\n");

	c_header = (union composite_header *)raw_decouple_data;

	ih_desc = &c_header->iheader;
	if (ih_desc->magic != HEADER_MAGIC_INFO) {
		int i;
		struct region_info *region;
		decoup_data = &c_header->mem_decoup;
		if (decoup_data->magic != DECOUPLE_MAGIC_INFO) {
			errorf("decoup data format error!\n");
			goto end;
		}

		region = &decoup_data->boot_info.regions;
		for (i = 0; i < MAX_REGION_NUM; i++) {
			if (!strncmp("cpcmdline", region[i].res.name, 9)) {
				addr = BASE_TO_APBASE(region[i].res.base);
				break;
			}
		}

		goto end;
	}

	if (ih_desc->magic != HEADER_MAGIC_INFO) {
		errorf("new decoup data format error!\n");
		goto end;
	}

	for (i_desc = ih_desc->desc; i_desc->size != 0; i_desc++) {
		if (!strcmp("decoup-desc", i_desc->desc)) {
			struct region_desc *reg;
			reg = ((struct mem_coupling_info *)((char *)c_header + i_desc->offs))->region;
			for (; reg->size > 0; reg++) {
				if (!strncmp("cpcmdline", reg->name, 9)) {
					addr = BASE_TO_APBASE(reg->base);
					goto end;
				}
			}
			break;
		}
	}

end:
	debugf("cpcmdline addr: 0x%p\n", (void *)(addr));
	return (void *)(addr);
}
