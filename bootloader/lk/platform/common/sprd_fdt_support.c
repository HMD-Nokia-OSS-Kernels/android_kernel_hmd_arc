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
//ZOVERLAY_TAG_HMD_ONEIMAGE
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/byteorder/little_endian.h>
#include <libfdt.h>
#include <lib/mincrypt/sha256.h>
#include <chipram_env.h>
#include <miscdata_def.h>
#include <sprd_sizes.h>
#include <sprd_common.h>
#include <sprd_common_rw.h>
#include <sprd_cpcmdline.h>
#include <sprd_log.h>
#include <sprd_wdt.h>
#include "boot_parse.h"
#include "boot_mode.h"
#include "sprd_fdt_support.h"
#include "sprd_fdt_memory.h"
#include <sprd_cpcmdline.h>
#include <fdtdec.h>
#ifndef CONFIG_ZEBU
#include <tee_smc_call.h>
#endif
#include <lk_sec_drv.h>
#ifdef SPRD_VBOOT_V2
#include <uboot_avb_ops.h>
#endif
#include <lk/board.h>
char *bootcause_cmdline = NULL;
char *pwroffcause_cmdline = NULL;
extern uint32_t get_first_mode;
extern uint32_t first_cali_mode;
extern unsigned int g_DtboIndex;

extern void *lcd_get_base_addr(void *lcd_base);
extern uint32_t lcd_get_bpix(void);
extern uint32_t lcd_get_pixel_clock(void);
extern uint32_t load_lcd_id_to_kernel(void);
extern uint32_t load_lcd_width_to_kernel(void);
extern uint32_t load_lcd_hight_to_kernel(void);
extern int get_dram_cs_number(void);
extern int get_dram_cs0_size(void);
extern char *get_product_sn(void);
extern void tee_call_request(unsigned int **mem_ptr);
extern boot_device_t get_bootdevice(void);
extern int poweron_by_calibration(void);
extern int sprd_check_ufs_version(void);
extern bool fixup_device_status(void);

#ifdef CONFIG_SPI_SLAVER_PANEL
extern const char *spi_panel_get_name(void);
#endif

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
extern int fb_oem_repair_get_booargs(char *buf, int len);
#endif

#ifdef CONFIG_USBPINMUX
/*  for usb_pin_mux */
extern void usb_mux_uart_config(void);
extern void usb_mux_jtag_config(void);
extern void usb_mux_jtag_apwdg_config(void);
#endif

/*for verified boot*/
enVerifiedState g_verifiedbootstate = v_state_green;
extern unsigned int g_DeviceStatus;

#define MAX_BOOTARG_LEN  (0x1000)  /*4kb*/

typedef enum {
	TOS_MEMORY = 0,
	MULTIMEDIA,
	AUDIO_MEMORY,
	SIPC_MEMORY,
	CP_MODEM,
	FRAMEBUFFER,
	OVERLAYBUFFER
} mem_type;

typedef struct {
	mem_type type;
	uint32_t start_high;
	uint32_t start_low;
	uint32_t size;
} mem_info;

typedef struct sprd_tee_mem_info_s {
	uint32_t tos_addr_h;
	uint32_t tos_addr_l;
	uint32_t tos_size_h;
	uint32_t tos_size_l;
	uint32_t teecfg_addr_h;
	uint32_t teecfg_addr_l;
	uint32_t teecfg_size_h;
	uint32_t teecfg_size_l;
}sprd_tee_mem_info_t;

#ifdef SPRD_VBOOT_V2
#include <libavb.h>
extern AvbSlotVerifyData* avb_slot_data[2];
#endif

#define DTBO_INDEX_DATA_LEN  40

int fdt_fixup_verified_boot(void *fdt)
{
	int ret = 0;
	char buf[64];
	int str_len;
	memset(buf, 0, 64);

	switch(g_verifiedbootstate)
	{
		case v_state_green:
			sprintf(buf,"androidboot.verifiedbootstate=green");
			break;
		case v_state_yellow:
			sprintf(buf,"androidboot.verifiedbootstate=yellow");
			break;
		case v_state_orange:
			sprintf(buf,"androidboot.verifiedbootstate=orange");
			break;
		case v_state_red:
			sprintf(buf,"androidboot.verifiedbootstate=red");
			break;
		default:
			sprintf(buf,"androidboot.verifiedbootstate=green");
			break;
	}

	str_len = strlen(buf);
	buf[str_len] = '\0';
	ret = fdt_chosen_bootargs_append(fdt, buf, 1);

	return ret;
}

int fdt_fixup_flash_lock_state(void *fdt)
{
	int ret = 0;
	char buf[64];
	int str_len;
	memset(buf, 0, 64);

	switch(g_DeviceStatus)
	{
		case 1:
			sprintf(buf,"androidboot.flash.locked=0");
			break;
		case 0:
			sprintf(buf,"androidboot.flash.locked=1");
			break;
		default:
			sprintf(buf,"androidboot.flash.locked=1");
			break;
	}

	str_len = strlen(buf);
	buf[str_len] = '\0';
	ret = fdt_chosen_bootargs_append(fdt, buf, 1);

	return ret;
}


#ifdef SPRD_VBOOT_V2
int fdt_fixup_vboot(void *fdt)
{
#ifdef CONFIG_VBOOT_SYSTEMASROOT
    fdt_fixup_vboot_system(fdt);
#else
    fdt_fixup_ando_vboot(fdt);
#endif
    return 0;
}

#ifdef CONFIG_VBOOT_SYSTEMASROOT
int fdt_fixup_vboot_system(void *fdt)
{
    int ret = 0;
    int str_len = 0;

    debugf("fdt_fixup_vboot_system enter \n");
    str_len = strlen((char *)g_vboot_sys_cmdline);
    if(str_len == 0) {
        debugf("g_vboot_sys_cmdline is null. \n");
        return ret;
    }
    g_vboot_sys_cmdline[str_len] = '\0';
    ret = fdt_chosen_bootargs_append(fdt, (char *)g_vboot_sys_cmdline, 1);
    debugf("fdt vboot system str_len is %d, ret is %d\n", str_len, ret);
    return ret;
}
#else
int fdt_fixup_ando_vboot(void *fdt)
{
        int ret = 0;
        int str_len = 0;

        str_len = strlen((char *)g_sprd_vboot_cmdline);
        if(str_len == 0) {
                debugf("g_sprd_vboot_cmdline is null. \n");
                return ret;
        }
        g_sprd_vboot_cmdline[str_len] = '\0';
        ret = fdt_chosen_bootargs_append(fdt, (char *)g_sprd_vboot_cmdline, 1);
        debugf("fdt vboot str_len is %d, ret is %d\n", str_len, ret);

        return ret;
}
#endif
#endif

int fdt_initrd_norsvmem(void *fdt, ulong initrd_start, ulong initrd_end, int force)
{
	int nodeoffset;
	int err;
	unsigned long tmp=0;
	const char *path;

	/* Find the "chosen" node.  */
	nodeoffset = fdt_path_offset(fdt, "/chosen");

	/* If there is no "chosen" node in the blob return */
	if (nodeoffset < 0) {
		errorf("fdt_initrd: %s\n", fdt_strerror(nodeoffset));
		return nodeoffset;
	}

	/* just return if initrd_start/end aren't valid */
	if ((initrd_start == 0) || (initrd_end == 0))
		return 0;

	path = fdt_getprop(fdt, nodeoffset, "linux,initrd-start", NULL);
	if ((path == NULL) || force) {
#ifdef CONFIG_ARM64
		tmp = __cpu_to_be64(initrd_start);
#else
		tmp = __cpu_to_be32(initrd_start);
#endif
		err = fdt_setprop(fdt, nodeoffset, "linux,initrd-start", &tmp, sizeof(tmp));
		if (err < 0) {
			errorf("could not set linux,initrd-start %s.\n", fdt_strerror(err));
			return err;
		}
#ifdef CONFIG_ARM64
		tmp = __cpu_to_be64(initrd_end);
#else
		tmp = __cpu_to_be32(initrd_end);
#endif
		err = fdt_setprop(fdt, nodeoffset, "linux,initrd-end", &tmp, sizeof(tmp));
		if (err < 0) {
			errorf("could not set linux,initrd-end %s.\n", fdt_strerror(err));

			return err;
		}
	}

	return 0;
}

int fdt_chosen_bootargs_append(void *fdt, const char *append_args, int force)
{
	int nodeoffset;
	int err;
	const char *path;
	char *strargs;
	int size;

	if (!append_args)
		return -1;

	err = fdt_check_header(fdt);
	if (err < 0) {
		errorf("fdt_chosen_bootargs_append: %s\n", fdt_strerror(err));
		return err;
	}

	/*
	 * Find the "chosen" node.
	 */
	nodeoffset = fdt_path_offset(fdt, "/chosen");

	/*
	 * If there is no "chosen" node in the blob, leave.
	 */
	if (nodeoffset < 0) {
		errorf("fdt_chosen_bootargs_append: cann't find chosen");
		return -1;
	}

	/*
	 * If the property exists, update it only if the "force" parameter
	 * is true.
	 */
	path = fdt_getprop(fdt, nodeoffset, "bootargs", NULL);
	if ((path == NULL) || force) {
		strargs = malloc(MAX_BOOTARG_LEN+1);
		if (!strargs)
			return -1;
		memset(strargs, 0, MAX_BOOTARG_LEN+1);
		if(path != NULL)
		{
			size = strlen(path) + strlen(append_args);
			if (size > (MAX_BOOTARG_LEN - 1)) {
				errorf("bootargs: %s.\n", path);
				errorf("append_args: %s.\n", append_args);
				panic("bootargs path len:%d overflow %d.\n", size, (MAX_BOOTARG_LEN - 1));
			}
			sprintf(strargs, "%s %s", path, append_args);
		}
		else
		{
			size = strlen(append_args);
			if (size > (MAX_BOOTARG_LEN - 1)) {
				errorf("append_args: %s.\n", append_args);
				panic("bootargs path len:%d overflow %d.\n", size, (MAX_BOOTARG_LEN - 1));
			}
			sprintf(strargs, "%s", append_args);
		}

		err = fdt_setprop(fdt, nodeoffset, "bootargs", strargs, strlen(strargs) + 1);
		if (err < 0)
			errorf("could not set bootargs %s.\n", fdt_strerror(err));
		free(strargs);
	}

	return err;
}

#ifdef CONFIG_TEE_FIREWALL
int uboot_set_secure_range_paraneters(mem_type type,fdt_addr_t start,fdt_addr_t size){
	mem_info mi __attribute__((aligned(4096)));
	smc_param *param;
	mi.type = type;
	if (sizeof(start) > 4)
		mi.start_high = (start >> 32);
	else
		mi.start_high = 0;
	mi.start_low = (start & 0xffffffff);
	mi.size = size;
	param = tee_common_call(FUNCTYPE_SET_SECURE_RANGE_PARAM, (uint32_t)(&mi), (uint32_t)(sizeof(mem_info)));

	return param->a0;
}

int fdt_reserved_mem_multimedia_parse(void*fdt)
{
	int parentoffset,nodeoffset;
	int offset;
	uchar * dt_addr;
	const fdt32_t *reg;
	int str_len;
	char nodename[256];
	fdt_addr_t  s_addr;
	fdt_addr_t  s_size ;
	char *pStr=NULL;
	mem_info mi;
	char name[64];
	char value[64];

	strncpy(name, "sprd,decoup", sizeof(name));
	/*check cproc-use-decoup*/
	strncpy(value, "cproc-use-decoup", sizeof(value));
	offset = fdt_node_offset_by_prop_value(fdt, 0, name, value, strlen(value) + 1);
	if(offset < 0) {
		/*check cproc-use-decoup-v1*/
		strncpy(value, "cproc-use-decoup-v1", sizeof(value));
		offset = fdt_node_offset_by_prop_value(fdt, 0, name, value, strlen(value) + 1);
		if(offset < 0)
		{
			debugf("the sipc info already exist, don't need to parse\n");
			return -1;
		}
	}
	if(0 == (fdtdec_decode_region_private(fdt, offset, "reg", &s_addr, &s_size)))
	{
		/*find reg prop ok*/
		uboot_set_secure_range_paraneters(CP_MODEM,s_addr,s_size);
	}
	parentoffset = fdt_path_offset(fdt, "/reserved-memory");
	if (parentoffset < 0) {
		errorf("fdt_reserved_mem_multimedia_parse: cann't find mm_reserved \n");
		return -1;
	}
	for (offset = fdt_first_subnode(fdt, parentoffset);
	      offset >= 0; offset = fdt_next_subnode(fdt, offset)) {
		sprintf(nodename, "%s", fdt_get_name(fdt, offset, NULL));
		str_len = strlen(nodename);
		nodename[str_len] = '\0';
		pStr = strstr(nodename,"multimediabuffer");
		if(pStr){
			if(fdtdec_decode_region(fdt, offset, "reg", &s_addr, &s_size) < 0)
			{
				debugf("cannot find reg prop, go to next node.\n" );
				goto next;
			}
			uboot_set_secure_range_paraneters(MULTIMEDIA,s_addr,s_size);
		}
		pStr = strstr(nodename,"tos-mem");
		if(pStr){
			if(fdtdec_decode_region(fdt, offset, "reg", &s_addr, &s_size) < 0)
			{
				debugf("cannot find reg prop, go to next node.\n" );
				goto next;
			}
			uboot_set_secure_range_paraneters(TOS_MEMORY,s_addr,s_size);
		}
		pStr = strstr(nodename,"audio-mem");
		if(pStr){
			if(fdtdec_decode_region(fdt, offset, "reg", &s_addr, &s_size) < 0)
			{
				debugf("cannot find reg prop, go to next node.\n" );
				goto next;
			}
			uboot_set_secure_range_paraneters(AUDIO_MEMORY,s_addr,s_size);
		}
		pStr = strstr(nodename,"framebuffer");
		if(pStr){
			if(fdtdec_decode_region(fdt, offset, "reg", &s_addr, &s_size) < 0)
			{
				debugf("cannot find reg prop, go to next node.\n" );
				goto next;
			}
			uboot_set_secure_range_paraneters(FRAMEBUFFER,s_addr,s_size);
		}
		pStr = strstr(nodename,"overlaybuffer");
		if(pStr){
			if(fdtdec_decode_region(fdt, offset, "reg", &s_addr, &s_size) < 0)
			{
				debugf("cannot find reg prop, go to next node.\n" );
				goto next;
			}
			uboot_set_secure_range_paraneters(OVERLAYBUFFER,s_addr,s_size);
		}
next:;
	}
	return 0;
}
#endif

int fdt_chosen_bootargs_replace(void *fdt, const char *old_args, const char *new_args)
{
	int nodeoffset;
	int err, i;
	char *str, *dst;
	const char *src;
	const char *path;
	char *strargs;
	int size_path, size_args;

	if (!old_args || !new_args)
		return -1;
	debugf("fdt_chosen_bootargs_replace start!");

	err = fdt_check_header(fdt);
	if (err < 0) {
		errorf("fdt_chosen_bootargs_replace: %s\n", fdt_strerror(err));
		return err;
	}

	/*
	 * Find the "chosen" node.
	 */
	nodeoffset = fdt_path_offset(fdt, "/chosen");

	/*
	 * If there is no "chosen" node in the blob, leave.
	 */
	if (nodeoffset < 0) {
		errorf("fdt_chosen_bootargs_replace: cann't find chosen\n");
		return -1;
	}

	/*
	 * If the property exists, update it only if the "force" parameter
	 * is true.
	 */
	path = fdt_getprop(fdt, nodeoffset, "bootargs", NULL);
	if (path != NULL ) {
		debugf("fdt_chosen_bootargs_replace: old bootargs %s!\n", path);
		size_path = strlen(path);
		size_args = strlen(new_args);
		if((size_path > (MAX_BOOTARG_LEN - 1)) || (size_args >(MAX_BOOTARG_LEN - 1))
			|| (size_path + size_args > (MAX_BOOTARG_LEN - 1))) {
			debugf("%s: path size %d, new args size = %d\n", __func__, size_path, size_args);
			debugf("new_args: %s!\n", new_args);
			panic("bootargs replace len overflow %d.\n", (MAX_BOOTARG_LEN - 1));
		}

		str = strstr(path, old_args);
		if(!str) {
		    errorf("fdt_chosen_bootargs_replace: cann't find str %s!\n", old_args);
		    return -1;
		}
		strargs = malloc(MAX_BOOTARG_LEN);
		if (strargs == NULL) {
			errorf("fdt_chosen_bootargs_replace: malloc size %d failed.\n", MAX_BOOTARG_LEN);
			return -1;
		}

		src = path;
		dst = strargs;
		i = 0;
		/* copy the front str */
		while (src != str && i < (MAX_BOOTARG_LEN - 1)) {
			*dst++ = *src++;
			i++;
		}

		/* copy the new str */
		src = new_args;
		while (*src && i < (MAX_BOOTARG_LEN - 1)) {
			*dst++ = *src++;
			i++;
		}

		/* copy the back str */
		src = str + strlen(old_args);
		while (*src && i < (MAX_BOOTARG_LEN - 1)) {
			*dst++ = *src++;
			i++;
		}
		*dst = 0;

		debugf("fdt_chosen_bootargs_replace: new bootargs %s!\n", strargs);
		err = fdt_setprop(fdt, nodeoffset, "bootargs", strargs, strlen(strargs) + 1);
		if (err < 0)
			errorf("could not set bootargs %s.\n", fdt_strerror(err));
		free(strargs);
	}
	return err;
}

void fdt_fixup_pmic_wa(void *fdt)
{
	int cpunode, vddnode;
	u32 cpureg;
	int vddarm0_node, vddarm1_node;
	u32 vddarm0_phandle, vddarm1_phandle;
	char buf[16];
	int str_len;

	if (ANA_REG_GET(ANA_REG_GLB_CHIP_ID_LOW) >= 0xC003) {
		debugf("2731 later than CE version, skip pmic wa\n");
		return;
	}

	vddarm0_node = fdt_node_offset_by_prop_value(fdt, -1, "regulator-name", "vddarm0", 8);
	if (vddarm0_node == -FDT_ERR_NOTFOUND) {
		errorf("vddarm0 not found\n");
		return;
	} else
		vddarm0_phandle = fdt_get_phandle(fdt, vddarm0_node);

	vddarm1_node = fdt_node_offset_by_prop_value(fdt, -1, "regulator-name", "vddarm1", 8);
	if (vddarm1_node == -FDT_ERR_NOTFOUND)
		return;
	else
		vddarm1_phandle = fdt_get_phandle(fdt, vddarm1_node);

	debugf("vddarm0 phandle 0x%x, vddarm1 phandle 0x%x\n", vddarm0_phandle, vddarm1_phandle);

	cpunode = fdt_node_offset_by_prop_value(fdt, 0, "device_type", "cpu", 4);
	if (cpunode < 0) {
		errorf("cannot find bullhill cpu\n");
		/* Exit if there is no CPU devices but this should never happen */
		return;
	} else {
		while (cpunode != -FDT_ERR_NOTFOUND) {
			u32 *reg = (u32 *)fdt_getprop(fdt, cpunode, "reg", 0);
			if (reg) {
				cpureg = *reg >> 24;
				if (cpureg == 0) {
					debugf("add vddarm0 to cpu0\n");
					vddnode = fdt_appendprop_u32(fdt, cpunode, "cpu0-supply", vddarm0_phandle);
					if (vddnode < 0) {
						char s[64];
						fdt_get_path(fdt, cpunode, s, sizeof(s));
						debugf("could not add vddarm0 node to %s, %s\n", s,
								fdt_strerror(vddnode));
					}
					vddnode = fdt_appendprop(fdt, cpunode, "dual-phase-supply", NULL, 0);
					if (vddnode < 0) {
						char s[64];
						fdt_get_path(fdt, cpunode, s, sizeof(s));
						debugf("could not add dual phase supply node to %s, %s\n", s,
								fdt_strerror(vddnode));
					}
				} else if (cpureg == 4) {
					debugf("add vddarm1 to cpu2\n");
					vddnode = fdt_appendprop_u32(fdt, cpunode, "cpu1-supply", vddarm1_phandle);
					if (vddnode < 0) {
						char s[64];
						fdt_get_path(fdt, cpunode, s, sizeof(s));
						debugf("could not add vddarm1 node to %s, %s\n", s,
								fdt_strerror(vddnode));
					}
					vddnode = fdt_appendprop(fdt, cpunode, "dual-phase-supply", NULL, 0);
					if (vddnode < 0) {
						char s[64];
						fdt_get_path(fdt, cpunode, s, sizeof(s));
						debugf("could not add dual phase supply node to %s, %s\n", s,
								fdt_strerror(vddnode));
					}
				}
			}
			cpunode = fdt_node_offset_by_prop_value(fdt, cpunode, "device_type", "cpu", 4);
		}
	}

	/* append bootargs if all above dts edits sucess */
	memset(buf, 0, 16);
	sprintf(buf, "pmic2731_wa");
	str_len = strlen(buf);
	buf[str_len] = '\0';
	fdt_chosen_bootargs_append(fdt, buf, 1);

	return;
}

#if 0
static char *Int2String(int num ,char *str)
{
	int i = 0, j = 0;
	int ptr;
	char ptr_str[4] = {0};
	do{
		str[i++] = num%10 + 48;
		num /= 10;
	}while(num);
	str[i] = '\0';

	for (j = 0;j < i/2; j++){
		ptr = str[j];
		ptr_str[j] = str[i - j -1];
		ptr_str[i - j -1] = ptr;
	}
	strcpy(str,ptr_str);
	return str;
}

extern int sprd_get_32k(void);
void fdt_fixup_pinctrl_l3(void *fdt)
{
	int ret,i,lenp,string_count = 0,err;
	const char *pinctrl_name;
	char *tmp_string;
	char *shutdown_str = "shutdown";
	char pinctrl_prop[12] = "pinctrl-";
	char num_str[4] = {0};
	int offset_pinctrl;
	int shutdown0,shutdown1,shutdown2,shutdown3,shutdown4,shutdown5;
	int nshutdown0,nshutdown1,nshutdown2,nshutdown3,nshutdown4,nshutdown5;
	int pshutdown0,pshutdown1,pshutdown2,pshutdown3,pshutdown4,pshutdown5;

	if (sprd_get_32k() == 1) {
		offset_pinctrl = fdt_node_offset_by_prop_value(fdt, -1, "compatible", "sprd,sharkl3-pinctrl", 21);
		if (offset_pinctrl < 0){
			errorf("cann't find offset_pinctrl node\n");
			return;
		}else {
			shutdown0 = fdt_subnode_offset(fdt, offset_pinctrl, "rfsen0");
			if (shutdown0 < 0) {
				tmp_string = (char*)malloc(300);
				if (!tmp_string)
					return;
				memset(tmp_string, 0, 300);
				pinctrl_name = fdt_getprop(fdt, offset_pinctrl, "pinctrl-names", &lenp);
				if (!pinctrl_name) {
					dprintf(INFO,"have no pinctrl-names............ ");
					ret = fdt_setprop(fdt, offset_pinctrl, "pinctrl-names",shutdown_str, strlen(shutdown_str) + 1);
					if (ret < 0) {
						errorf("could not set pinctrl-names %s.\n", fdt_strerror(ret));
						return;
					}
					//add node
					/*************************************/
					shutdown0 = fdt_add_subnode(fdt, offset_pinctrl, "rfsck0");
					if (shutdown0 < 0)
						dprintf(INFO,"%s: %s: %s\n", __func__, "shutdown0: rfsck0", fdt_strerror(shutdown0));

					nshutdown0 = fdt_subnode_offset(fdt, offset_pinctrl, "rfsck0");
					if (nshutdown0 < 0) {
						errorf("can't get rfsck0\n");
						return;
					} else{
						fdt_setprop_string(fdt, shutdown0, "pins", "SHARKL3_RFSCK0");
						fdt_setprop_string(fdt, shutdown0, "function", "func4");
						err = fdt_setprop_u32(fdt, shutdown0, "phandle",nshutdown0);
						if (err < 0) {
							errorf("set property phandle error!\n");
							return;
						}
						pshutdown0 = fdt_get_phandle(fdt, nshutdown0);
						ret = fdt_appendprop_u32(fdt, offset_pinctrl, "pinctrl-0", pshutdown0);
						if (ret < 0) {
							errorf("can't add handle to pinctrl-0\n");
						}
					}

					shutdown1 = fdt_add_subnode(fdt, offset_pinctrl, "rfsen0");
					if (shutdown1 < 0)
						dprintf(INFO,"%s: %s: %s\n", __func__, "shutdown1: rfsen0 ", fdt_strerror(shutdown1));

					nshutdown1 = fdt_subnode_offset(fdt, offset_pinctrl, "rfsen0");
					if (nshutdown1 < 0) {
						errorf("can't get rfsen0\n");
						return;
					} else{
						fdt_setprop_string(fdt, shutdown1, "pins", "SHARKL3_RFSEN0");
						fdt_setprop_string(fdt, shutdown1, "function", "func4");
						err = fdt_setprop_u32(fdt, shutdown1, "phandle",nshutdown1);
						if (err < 0) {
							errorf("set property phandle error!\n");
							return;
						}
						pshutdown1 = fdt_get_phandle(fdt, nshutdown1);
						ret = fdt_appendprop_u32(fdt, offset_pinctrl, "pinctrl-0", pshutdown1);
						if (ret < 0) {
							errorf("can't add handle to pinctrl-0\n");
							return;
						}
					}

					shutdown2 = fdt_add_subnode(fdt, offset_pinctrl, "rfsda0");
					if (shutdown2 < 0)
						dprintf(INFO,"%s: %s: %s\n", __func__, "shutdown2: rfsda0", fdt_strerror(shutdown2));

					nshutdown2 = fdt_subnode_offset(fdt, offset_pinctrl, "rfsda0");
					if (nshutdown2 < 0) {
						errorf("can't get rfsda0\n");
						return;
					} else{
						fdt_setprop_string(fdt, shutdown2, "pins", "SHARKL3_RFSDA0");
						fdt_setprop_string(fdt, shutdown2, "function", "func4");
						err = fdt_setprop_u32(fdt, shutdown2, "phandle",nshutdown2);
						if (err < 0) {
							errorf("set property phandle error!\n");
							return;
						}
						pshutdown2 = fdt_get_phandle(fdt, nshutdown2);
						ret = fdt_appendprop_u32(fdt, offset_pinctrl, "pinctrl-0", pshutdown2);
						if (ret < 0) {
							errorf("can't add handle to pinctrl-0\n");
						}
					}

					shutdown3 = fdt_add_subnode(fdt, offset_pinctrl, "rfsen1");
					if (shutdown3 < 0)
						dprintf(INFO,"%s: %s: %s\n", __func__, "shutdown3: rfsen1", fdt_strerror(shutdown3));

					nshutdown3 = fdt_subnode_offset(fdt, offset_pinctrl, "rfsen1");
					if (nshutdown3 < 0) {
						errorf("can't get rfsen1\n");
						return;
					} else{
						fdt_setprop_string(fdt, shutdown3, "pins", "SHARKL3_RFSEN1");
						fdt_setprop_string(fdt, shutdown3, "function", "func4");
						err = fdt_setprop_u32(fdt, shutdown3, "phandle",nshutdown3);
						if (err < 0) {
							errorf("set property phandle error!\n");
							return;
						}
						pshutdown3 = fdt_get_phandle(fdt, nshutdown3);
						ret = fdt_appendprop_u32(fdt, offset_pinctrl, "pinctrl-0", pshutdown3);
						if (ret < 0) {
							errorf("can't add handle to pinctrl-0\n");
						}
					}

					shutdown4 = fdt_add_subnode(fdt, offset_pinctrl, "rfsck1");
					if (shutdown4 < 0)
						dprintf(INFO,"%s: %s: %s\n", __func__, "shutdown4: rfsck1", fdt_strerror(shutdown4));

					nshutdown4 = fdt_subnode_offset(fdt, offset_pinctrl, "rfsck1");
					if (nshutdown4 < 0) {
						errorf("can't get rfsck1\n");
						return;
					} else{
						fdt_setprop_string(fdt, shutdown4, "pins", "SHARKL3_RFSCK1");
						fdt_setprop_string(fdt, shutdown4, "function", "func4");
						err = fdt_setprop_u32(fdt, shutdown4, "phandle",nshutdown4);
						if (err < 0) {
							errorf("set property phandle error!\n");
							return;
						}
						pshutdown4 = fdt_get_phandle(fdt, nshutdown4);
						ret = fdt_appendprop_u32(fdt, offset_pinctrl, "pinctrl-0", pshutdown4);
						if (ret < 0) {
							errorf("can't add handle to pinctrl-0\n");
						}
					}

					shutdown5 = fdt_add_subnode(fdt, offset_pinctrl, "rfsda1");
					if (shutdown5 < 0)
						dprintf(INFO,"%s: %s: %s\n", __func__, "shutdown5: rfsda1", fdt_strerror(shutdown5));

					nshutdown5 = fdt_subnode_offset(fdt, offset_pinctrl, "rfsda1");
					if (nshutdown5 < 0) {
						errorf("can't get rfsda1\n");
						return;
					} else{
						fdt_setprop_string(fdt, shutdown5, "pins", "SHARKL3_RFSDA1");
						fdt_setprop_string(fdt, shutdown5, "function", "func4");
						err = fdt_setprop_u32(fdt, shutdown5, "phandle",nshutdown5);
						if (err < 0) {
							errorf("set property phandle error!\n");
							return;
						}
						pshutdown5 = fdt_get_phandle(fdt, nshutdown5);
						ret = fdt_appendprop_u32(fdt, offset_pinctrl, "pinctrl-0", pshutdown5);
						if (ret < 0) {
							errorf("cant add handle to pinctrl-0\n");
						}
					}
				} else {
					dprintf(INFO,"have pinctrl-names............\n");
					for (i = 0; i < lenp; i++){
						if(pinctrl_name[i] == 0)
						string_count++;
					}
					Int2String(string_count,num_str);
					strcat(pinctrl_prop,num_str);
					memcpy(tmp_string, pinctrl_name, lenp);
					memcpy(&tmp_string[lenp], shutdown_str, 9);
					ret = fdt_setprop(fdt, offset_pinctrl, "pinctrl-names", tmp_string, lenp+strlen(shutdown_str)+1);
					if (ret < 0) {
						errorf("could not set pinctrl-names %s.\n", fdt_strerror(ret));
						return;
					}
					free(tmp_string);

					/*************************************/
					shutdown0 = fdt_add_subnode(fdt, offset_pinctrl, "rfsck0");
					if (shutdown0 < 0)
						dprintf(INFO,"%s: %s: %s\n", __func__, "shutdown0: rfsck0", fdt_strerror(shutdown0));

					nshutdown0 = fdt_subnode_offset(fdt, offset_pinctrl, "rfsck0");
					if (nshutdown0 < 0) {
						errorf("can't get rfsck0\n");
						return;
					} else{
						fdt_setprop_string(fdt, shutdown0, "pins", "SHARKL3_RFSCK0");
						fdt_setprop_string(fdt, shutdown0, "function", "func4");
						err = fdt_setprop_u32(fdt, shutdown0, "phandle",nshutdown0);
						if (err < 0) {
							errorf("set property phandle error!\n");
							return;
						}
						pshutdown0 = fdt_get_phandle(fdt, nshutdown0);
						ret = fdt_appendprop_u32(fdt, offset_pinctrl, pinctrl_prop, pshutdown0);
						if (ret < 0) {
							errorf("cant add handle to pinctrl_prop\n");
						}
					}

					shutdown1 = fdt_add_subnode(fdt, offset_pinctrl, "rfsen0");
					if (shutdown1 < 0)
						dprintf(INFO,"%s: %s: %s\n", __func__, "shutdown1: rfsen0", fdt_strerror(shutdown1));

					nshutdown1 = fdt_subnode_offset(fdt, offset_pinctrl, "rfsen0");
					if (nshutdown1 < 0) {
						errorf("can't get rfsen0\n");
						return;
					} else{
						fdt_setprop_string(fdt, shutdown1, "pins", "SHARKL3_RFSEN0");
						fdt_setprop_string(fdt, shutdown1, "function", "func4");
						err = fdt_setprop_u32(fdt, shutdown1, "phandle",nshutdown1);
						if (err < 0) {
							errorf("set property phandle error!\n");
							return;
						}
						pshutdown1 = fdt_get_phandle(fdt, nshutdown1);
						ret = fdt_appendprop_u32(fdt, offset_pinctrl, pinctrl_prop, pshutdown1);
						if (ret < 0) {
							errorf("cant add handle to pinctrl_prop\n");
						}
					}

					shutdown2 = fdt_add_subnode(fdt, offset_pinctrl, "rfsda0");
					if (shutdown2 < 0)
						dprintf(INFO,"%s: %s: %s\n", __func__, "shutdown2: rfsda0", fdt_strerror(shutdown2));

					nshutdown2 = fdt_subnode_offset(fdt, offset_pinctrl, "rfsda0");
					if (nshutdown2 < 0) {
						errorf("cat get rfsda0\n");
						return;
					} else{
						fdt_setprop_string(fdt, shutdown2, "pins", "SHARKL3_RFSDA0");
						fdt_setprop_string(fdt, shutdown2, "function", "func4");
						err = fdt_setprop_u32(fdt, shutdown2, "phandle",nshutdown2);
						if (err < 0) {
							errorf("set property phandle error!\n");
							return;
						}
						pshutdown2 = fdt_get_phandle(fdt, nshutdown2);
						ret = fdt_appendprop_u32(fdt, offset_pinctrl, pinctrl_prop, pshutdown2);
						if (ret < 0) {
							errorf("cant add handle to pinctrl_prop\n");
						}
					}

					shutdown3 = fdt_add_subnode(fdt, offset_pinctrl, "rfsen1");
					if (shutdown3 < 0)
						dprintf(INFO,"%s: %s: %s\n", __func__, "shutdown3: rfsen1", fdt_strerror(shutdown3));

					nshutdown3 = fdt_subnode_offset(fdt, offset_pinctrl, "rfsen1");
					if (nshutdown3 < 0) {
						errorf("can't get rfsen1\n");
						return;
					} else{
						fdt_setprop_string(fdt, shutdown3, "pins", "SHARKL3_RFSEN1");
						fdt_setprop_string(fdt, shutdown3, "function", "func4");
						err = fdt_setprop_u32(fdt, shutdown3, "phandle",nshutdown3);
						if (err < 0) {
							errorf("set property phandle error!\n");
							return;
						}
						pshutdown3 = fdt_get_phandle(fdt, nshutdown3);
						ret = fdt_appendprop_u32(fdt, offset_pinctrl, pinctrl_prop, pshutdown3);
						if (ret < 0) {
							errorf("cant add handle to pinctrl_prop\n");
						}
					}

					shutdown4 = fdt_add_subnode(fdt, offset_pinctrl, "rfsck1");
					if (shutdown4 < 0)
						dprintf(INFO,"%s: %s: %s\n", __func__, "shutdown4: rfsck1", fdt_strerror(shutdown4));

					nshutdown4 = fdt_subnode_offset(fdt, offset_pinctrl, "rfsck1");
					if (nshutdown4 < 0) {
						errorf("can't get rfsck1\n");
						return;
					} else{
						fdt_setprop_string(fdt, shutdown4, "pins", "SHARKL3_RFSCK1");
						fdt_setprop_string(fdt, shutdown4, "function", "func4");
						err = fdt_setprop_u32(fdt, shutdown4, "phandle",nshutdown4);
						if (err < 0) {
							errorf("set property phandle error!\n");
							return;
						}
						pshutdown4 = fdt_get_phandle(fdt, nshutdown4);
						ret = fdt_appendprop_u32(fdt, offset_pinctrl, pinctrl_prop, pshutdown4);
						if (ret < 0) {
							errorf("cant add handle to pinctrl_prop\n");
						}
					}

					shutdown5 = fdt_add_subnode(fdt, offset_pinctrl, "rfsda1");
					if (shutdown5 < 0)
						dprintf(INFO,"%s: %s: %s\n", __func__, "shutdown5: rfsda1", fdt_strerror(shutdown5));

					nshutdown5 = fdt_subnode_offset(fdt, offset_pinctrl, "rfsda1");
					if (nshutdown5 < 0) {
						errorf("can't get rfsda1\n");
						return;
					} else{
						fdt_setprop_string(fdt, shutdown5, "pins", "SHARKL3_RFSDA1");
						fdt_setprop_string(fdt, shutdown5, "function", "func4");
						err = fdt_setprop_u32(fdt, shutdown5, "phandle", nshutdown5);
						if (err < 0) {
							errorf("set property phandle error!\n");
							return;
						}
						pshutdown5 = fdt_get_phandle(fdt, nshutdown5);
						ret = fdt_appendprop_u32(fdt, offset_pinctrl, pinctrl_prop, pshutdown5);
						if (ret < 0) {
							errorf("cant add handle to pinctrl_prop\n");
						}
					}
				}
			} else {
				dprintf(INFO,"have rfsen0 node\n");
			}
		}
	}
}
#endif

int fdt_fixup_iq_reserved_mem(void *fdt)
{
	int offset;
	int ret = 0;
	const char *mode;

	mode = g_env_bootmode;
	if (!mode || strcmp("iq", mode)) {
		offset = fdt_node_offset_by_prop_value(fdt, 0, "compatible", "sprd,iq-mem", strlen("sprd,iq-mem") + 1);
		if (offset < 0) {
			return 0;
		}
		ret = fdt_del_node(fdt, offset);
		if (ret < 0)
			errorf("del iq reserved mem failed\n");
	}

	return ret;
}

int fdt_fixup_parse_miscdata_cmd(char *cmd, char *value) {
	int len = 0;
	char buf[DL_IQ_DYN_DATA_LEN] = {0};
	char *str, *temp;

	if ((cmd == NULL) || (value == NULL)) {
		debugf("%s: cmd/value = NULL\n", __func__);
		return len;
	}

	if (common_raw_read("miscdata", DL_IQ_DYN_DATA_LEN,
			DL_IQ_DYN_OFFSET,buf)) {
		errorf("read miscdata value error.\n");
		goto EXIT;
	}

        /* find cmd in misdata */
	str = strstr(buf, cmd);
	if (str == NULL) {
		debugf("%s: miscdata '%s' is not exist\n",  __func__, cmd);
		goto EXIT;
	}

	if ((temp = strchr(str, ' ')) || (temp = strchr(str, '\0'))) {
		*temp = '\0';
	} else {
		debugf("%s: miscdata '%s' & blank not found or not the last one\n", __func__, str);
		goto EXIT;
	}

	if (temp == strchr(str, '\n'))
		*(temp) = '\0';

        /* get cmd value */
	temp = strchr(str, '=');
	if (temp) {
		temp = temp + 1;
	} else {
		debugf("%s: misdata '%s =' not found\n", __func__, cmd);
		goto EXIT;
	}

	len = strlen(temp);
	strncpy(value, temp, len + 1);
	debugf("%s: parse cmd '%s', value = '%s', len = %d\n",
                            __func__, cmd, value, len);
EXIT:
	debugf("%s: exit parse value!\n", __func__);
	return len;
}

int fdt_fixup_nrphy_iqmem(void *blob, const char *node_name)
{
	int offset;
	int len;
	const char *prop_value;
	struct fdt_header *fdt;
	int ret;
#if DEBUG
	char buf[DL_IQ_DYN_DATA_LEN] = {0};
#endif

	if (!blob) {
		errorf("blob is NULL\n");
		goto FAIL;
	}

	fdt = (struct fdt_header *)blob;

	offset = fdt_path_offset(fdt, "/reserved-memory");
	if (offset < 0) {
		errorf("cann't find reserved-memory node");
		goto FAIL;
	}

	offset = fdt_subnode_offset(fdt, offset, node_name);
	if (offset == -FDT_ERR_NOTFOUND) {
		errorf(" %s is not exist\n", node_name);
		goto FAIL;
	}

#if DEBUG
	if ((fdt_fixup_parse_miscdata_cmd(NRIQ_MODE, buf) > 0) && (!strcmp(buf, "1"))) {
		debugf("%s: miscdata flag set nrphy-iqmem ebabled.\n", __func__);
		ret = fdt_setprop(fdt, offset, "status", "okay", sizeof("okay"));
	} else
#endif
	{
		prop_value = fdt_getprop(fdt, offset, "status", &len);
		debugf("prop_value = %s, len=%d \n",prop_value, len);
		if (prop_value == NULL) {
			goto FAIL;
		}

		if (strstr(prop_value, "okay")) {
			 debugf("nrphyiqmem_reserved is okay");
		} else if (get_calibration_parameter()) {
			ret = fdt_setprop(fdt, offset, "status",
					"okay", sizeof("okay"));
			if(ret < 0) {
				errorf(" set prop fail %d\n", ret);
				goto FAIL;
			}
		}
	}

	return 0;

FAIL:
	return -1;
}

int fdt_fixup_cp_reserved_mem(void *fdt)
{
	int ret = 0;
	int offset, offset_cp, lenp;
	uint32_t value[16];
	fdt_addr_t  s_addr;
	fdt_addr_t  s_size;
	const fdt_addr_t *cell;

	offset = fdt_path_offset(fdt, "/reserved-memory");
	if (offset < 0) {
		errorf("cann't find reserved-memory node");
		return -1;
	}

	offset_cp = fdt_subnode_offset(fdt, offset, "cp-modem");
	if (offset_cp == -FDT_ERR_NOTFOUND) {
		offset_cp = fdt_subnode_offset(fdt, offset, "cp-mem");
		if (offset_cp == -FDT_ERR_NOTFOUND) {
			errorf("cann't find cp-modem&cp-mem@xxxx node\n");
			return -1;
		}
	}
	offset = offset_cp;

	cell = fdt_getprop(fdt, offset, "sprd,cpdbg-size", &lenp);
	if (!cell) {
		debugf("the cp has no debug area\n");
		return 0;
	}

	memcpy(&value[0], cell, sizeof(value[0]));
	value[0] = __be32_to_cpu(value[0]);
	debugf("cpdbg-size = %x\n", value[0]);

	if(fdtdec_decode_region(fdt, offset, "reg", &s_addr, &s_size) < 0)
	{
		errorf("cannot find reg prop in cp-modem@xxxx node");
		return -1;
	}
	debugf("s_addr = 0x%lx, s_size = 0x%lx\n", s_addr, s_size);

	s_size -= value[0];
#ifdef CONFIG_PHYS_64BIT
	s_addr = __cpu_to_be64(s_addr);
	s_size = __cpu_to_be64(s_size);
#else
	s_addr = __cpu_to_be32(s_addr);
	s_size = __cpu_to_be32(s_size);
#endif
	memcpy((char *)value, &s_addr, sizeof(fdt_addr_t));
	memcpy((char *)value + sizeof(fdt_addr_t), &s_size, sizeof(fdt_addr_t));

	ret = fdt_setprop(fdt, offset, "reg", value, 2 * sizeof(fdt_addr_t));

	return ret;
}

int fdt_fixup_cp_boot(void *fdt)
{
	int ret = 0;
#if defined( CONFIG_KERNEL_BOOT_CP )
	char buf[64];
	int str_len;
	memset(buf, 0, 64);
	sprintf(buf,"modem=shutdown");
	str_len = strlen(buf);
	buf[str_len] = '\0';
	ret = fdt_chosen_bootargs_append(fdt, buf, 1);
#endif

	ret += fdt_chosen_bootargs_append(fdt, cp_getcmdline(), 1);

	/* if is uart calibraton, remove ttys1 console */
	if(is_calibration_by_uart())
	{
		//__raw_writel(0x285480, CTL_PIN_BASE + REG_PIN_CTRL2);
		//ret = fdt_chosen_bootargs_replace(fdt,"console=ttyS1", "console=ttyS3");
		ret += fdt_chosen_bootargs_replace(fdt, "console=ttyS1", "console=null");
	}

	return ret;
}

int fdt_fixup_emmc_swcq(void *fdt)
{
	int nodeoffset;

	if (get_bootdevice() != BOOT_DEVICE_EMMC || !fixup_device_status())
		return 0;

	nodeoffset = fdt_node_offset_by_prop_value(fdt, 0, "sprd,name", "sdio_emmc", 10);
	if (nodeoffset == -FDT_ERR_NOTFOUND) {
		errorf("emmc sprd,name not found\n");
		return -1;
	}
	debugf("find node %s successfully\n", fdt_get_name(fdt, nodeoffset, NULL));

	fdt_delprop(fdt, nodeoffset, "supports-swcq");
	dprintf(INFO, "mmc del supports-swcq\n");

	return 0;
}

#ifdef CONFIG_MMC_POWP_SUPPORT
extern int g_part_protected;
extern char *g_part_name;

int fdt_fixup_protect_part(void *fdt)
{
#ifndef CONFIG_NAND_BOOT
	int nodeoffset;
	int err;
	disk_partition_t part_info;
	uint64_t part_addr_size = 0;

	if (get_bootdevice() != BOOT_DEVICE_EMMC) {
		errorf("write protection is not supported in current device\n");
		return 0;
	}

	nodeoffset = fdt_node_offset_by_prop_value(fdt, 0, "sprd,name", "sdio_emmc", 10);
	if (nodeoffset == -FDT_ERR_NOTFOUND) {
		errorf("prop sprd,name not found, part protect is impossible\n");
		return -1;
	}
	debugf("find node %s successfully\n", fdt_get_name(fdt, nodeoffset, NULL));

	if (g_part_protected) {
		if (get_img_partition_info(g_part_name, &part_info)) {
			errorf("unsupported part %s, pls check it\n", g_part_name);
			return -1;
		}

		if (0 != part_info.blk_cnt * part_info.blksz % SZ_8M) {
			errorf("%s's size should be 8M aligned\n", g_part_name);
			return -1;
		}

		part_addr_size = ((uint64_t)part_info.start_blk << 32) + part_info.blk_cnt;

		/* set protect part's address and size */
		fdt_delprop(fdt, nodeoffset, "sprd,part-address-size");
		err = fdt_setprop_u64(fdt, nodeoffset, "sprd,part-address-size", part_addr_size);
		if (err < 0) {
			errorf("set property sprd,part-address-size error!\n");
			return err;
		}

		/* set protect part's switch, open pwp */
		fdt_delprop(fdt, nodeoffset, "sprd,wp_fn");
		err = fdt_setprop(fdt, nodeoffset, "sprd,wp_fn", "enable", strlen("enable") + 1);
		if (err < 0) {
			errorf("set property sprd,wp_fn error!\n");
			return err;
		}
		debugf("property sprd,wp_fn enable\n");
	}
#endif
	return 0;
}
#endif

static phys_addr_t virt_to_phys(void *x)
{
	return (phys_addr_t)x;
}

static int fix_subnode_mem_reserved(void *fdt, int parentoffset, const char* name, fdt_addr_t addr, fdt_addr_t size)
{
	int ret = 0;
	fdt_addr_t  s_addr;
	fdt_addr_t  s_size;
	int suboffset;
	uint32_t value[16];

	suboffset = fdt_subnode_offset(fdt, parentoffset, name);
	if (suboffset == -FDT_ERR_NOTFOUND) {
		ret = fdt_add_mem_rsv(fdt, addr, size);
	} else {
		if(fdtdec_decode_region(fdt, suboffset, "reg", &s_addr, &s_size) < 0)
		{
			errorf("cannot find reg prop in %s node\n", name);
			return -1;
		}
#ifdef CONFIG_PHYS_64BIT
		s_addr = __cpu_to_be64(addr);
		s_size = __cpu_to_be64(size);
#else
		s_addr = __cpu_to_be32(addr);
		s_size = __cpu_to_be32(size);
#endif
		memcpy((char *)value, &s_addr, sizeof(fdt_addr_t));
		memcpy((char *)value + sizeof(fdt_addr_t), &s_size, sizeof(fdt_addr_t));

		ret = fdt_setprop(fdt, suboffset, "reg", value, 2 * sizeof(fdt_addr_t));
	}

	return ret;
}
#ifndef CONFIG_ZEBU
int fdt_fixup_tee_reserved_mem(void *fdt)
{
	int ret = 0;
	int offset;
	uint32_t addr_h = 0, addr_l = 0, size = 0;
	fdt_addr_t  s_addr;
	fdt_addr_t  s_size;
	sprd_tee_mem_info_t *tee_mem_info;
	phys_addr_t pa;

	offset = fdt_path_offset(fdt, "/reserved-memory");
	if (offset < 0) {
		errorf("cann't find reserved-memory node\n");
		ret = offset;
		return ret;
	}

	ret = tee_smc_sip_call(TEE_SIPCALL_ID_SML_MEM_INFO, &addr_h, &addr_l, &size);
	if(0 == ret) {
		s_size = (fdt_addr_t)size;
#ifdef CONFIG_PHYS_64BIT
		s_addr = addr_h;
		s_addr = s_addr << 32;
#else
		s_addr = 0;
#endif
	        s_addr |= addr_l;
		if((0 != s_addr) && (0 != s_size)) {
			ret = fix_subnode_mem_reserved(fdt, offset, "sml-mem", s_addr, s_size);
			if(0 != ret) {
				errorf("fix tos mem_reserved failed %d\n", ret);
				return ret;
			}
		}
	}

	tee_mem_info = (sprd_tee_mem_info_t*)memalign(PAGE_SIZE,PAGE_SIZE);
	if(NULL == tee_mem_info) {
		errorf("malloc failed \n");
		return -1;
	}

	memset((void*)tee_mem_info, 0 , PAGE_SIZE);

	pa = virt_to_phys((void*)tee_mem_info);
#ifdef CONFIG_PHYS_64BIT
	addr_h = (u32)(pa >> 32);
#else
	addr_h = 0;
#endif
	addr_l = (uint32_t)pa;

	ret = tee_smc_fast_call(TEESMC_FUNCID_ID_TEE_MEM_INFO, addr_l, addr_h, PAGE_SIZE);
	if(0 != ret) {
		debugf("smc get tos mem info failed! (0x%x)\n", ret);
		ret = 0;
		goto fix_end;
	}

#ifdef CONFIG_PHYS_64BIT
	s_size = tee_mem_info->tos_size_h;
	s_size = s_size << 32;
#else
	s_size = 0;
#endif
	s_size |= tee_mem_info->tos_size_l;

#ifdef CONFIG_PHYS_64BIT
	s_addr = tee_mem_info->tos_addr_h;
	s_addr = s_addr << 32;
#else
	s_addr = 0;
#endif
	s_addr |= tee_mem_info->tos_addr_l;
	if((0 == s_addr) || (0 == s_size)) {
		debugf("smc get tos addr: 0x%lx, size: 0x%lx. no tos ?\n", s_addr, s_size);
		goto fix_end;
	}
	ret = fix_subnode_mem_reserved(fdt, offset, "tos-mem", s_addr, s_size);
	if(0 != ret){
		errorf("fix tos mem_reserved failed %d\n", ret);
		goto fix_end;
	}

#ifdef CONFIG_PHYS_64BIT
	s_size = tee_mem_info->teecfg_size_h;
	s_size = s_size << 32;
#else
	s_size = 0;
#endif
	s_size |= tee_mem_info->teecfg_size_l;

#ifdef CONFIG_PHYS_64BIT
	s_addr = tee_mem_info->teecfg_addr_h;
	s_addr = s_addr << 32;
#else
	s_addr = 0;
#endif
	s_addr |= tee_mem_info->teecfg_addr_l;

	if((0 == s_addr) || (0 == s_size)) {
		debugf("smc get teecfg addr: 0x%lx, size: 0x%lx. no teecfg ?\n", s_addr, s_size);
		goto fix_end;
	}
	ret = fix_subnode_mem_reserved(fdt, offset, "teecfg-mem", s_addr, s_size);
	if(0 != ret){
		errorf("fix teecfg mem_reserved failed %d\n", ret);
	}
fix_end:
	free(tee_mem_info);
	return ret;
}
#endif
#ifdef CONFIG_NAND_BOOT
int fdt_fixup_mtd(void *fdt)
{
	char buf[128];
	int str_len;
	int ret;

	memset(buf, 0, 128);

	sprintf(buf, MTDPARTS_DEFAULT);
	str_len = strlen(buf);
	buf[str_len] = '\0';

	ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	return ret;
}

#ifdef CONFIG_UBI_ATTACH_MTD
int fdt_fixup_ubi_ai(void *fdt)
{
	char buf[128];
	int str_len;
	int ret;

	memset(buf, 0, 128);

	sprintf(buf, UBI_ATTACH_INFO);
	str_len = strlen(buf);
	buf[str_len] = '\0';

	ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	return ret;
}
#endif
#endif

int fdt_fixup_first_mode(void *fdt)
{
	char buf[32] = {0};
	char cali_buf[32] = {0};
	int ret = 0;
	uint32_t first_mode = 0;
	uint32_t cali_mode = 0;

	if ((get_first_mode & 0xFFFFFF00) == SET_FIRST_MODE_MAGIC){
		first_mode = get_first_mode & 0x000000FF;
		cali_mode = first_cali_mode;
		sprintf(buf, "first_mode=%x", first_mode);
		sprintf(cali_buf, "cali_mode=%x", cali_mode);
		debugf("cmdline first_mode=%x,cali_mode=%x\n", first_mode, cali_mode);

		if (fdt_chosen_bootargs_append(fdt, buf, 1))
			return -1;
		if (fdt_chosen_bootargs_append(fdt, cali_buf, 1))
			return -1;
	}

	return ret;
}

#ifdef DISABLE_UART
int fdt_close_console(void *fdt)
{
	int ret = 0;
	char *pStr=NULL;
	char nodename[256];
	int offset;
	int str_len;
	int err;
	//close console
	ret = fdt_chosen_bootargs_replace(fdt,"console=ttyS1", "console=null");
	//close earlycon
	ret = fdt_chosen_bootargs_replace(fdt,"earlycon=sprd_serial,0x70100000,115200n8", "");

	int nodeoffset;
	nodeoffset = fdt_path_offset(fdt, "/soc");

	if (nodeoffset < 0) {
	      errorf("fdt_chosen_bootargs_replace: cann't find chosen");
		return -1;
	}else{
		for (offset = fdt_first_subnode(fdt, nodeoffset);
		     offset >= 0; offset = fdt_next_subnode(fdt, offset)) {

			sprintf(nodename, "%s", fdt_get_name(fdt, offset, NULL));
			str_len = strlen(nodename);
			nodename[str_len] = '\0';
			pStr = strstr(nodename, "ap-apb");
			if(pStr)  {
				int suboffset;

				for (suboffset = fdt_first_subnode(fdt, offset);
				     suboffset >= 0; suboffset = fdt_next_subnode(fdt, suboffset)) {
				sprintf(nodename, "%s", fdt_get_name(fdt, suboffset, NULL));
				str_len = strlen(nodename);
				nodename[str_len] = '\0';
				pStr = strstr(nodename, "serial@70100000");
				if (pStr) {
					debugf("------fdt_find serial@70100000------ \n");
					char strargs[]="disabled";
					err = fdt_setprop(fdt, suboffset, "status", strargs, strlen(strargs) + 1);
					if(err < 0)
						errorf("could not set bootargs %s.\n", fdt_strerror(err));
					break;
				    }
				}
				break;
			}
		}
	}

	return ret;
}
#endif

#ifdef CONFIG_SPLASH_SCREEN
int fdt_fixup_lcdid(void *fdt)
{
	char buf[16];

	uint32_t lcd_id = 0;
	int str_len;
	int ret;
	lcd_id = load_lcd_id_to_kernel();
	memset(buf, 0, 16);
	sprintf(buf, "lcd_id=ID");
	str_len = strlen(buf);
	sprintf(&buf[str_len], "%x", lcd_id);
	str_len = strlen(buf);
	buf[str_len] = '\0';

	ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	return ret;
}

int fdt_fixup_lcdbase(void *fdt)
{
	char buf[32];
	int str_len;
	int ret;
	void *lcd_base;

	memset(buf, 0, 32);

	//add lcd frame buffer base, length should be lcd w*h*2(RGB565)
	lcd_base = lcd_get_base_addr(NULL);

	debugf("lcd_base=%p\n" ,lcd_base);
	sprintf(buf, "lcd_base=");
	str_len = strlen(buf);
	sprintf(&buf[str_len], "%lx", (unsigned long)lcd_base);
	str_len = strlen(buf);
	buf[str_len] = '\0';

	ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	return ret;
}

int fdt_fixup_lcdsize(void *fdt)
{
	char buf[32];
	int str_len;
	int ret;

	memset(buf,0,32);
	sprintf(buf, "lcd_size=");
	str_len = strlen(buf);

	sprintf(&buf[str_len], "%dx%d", load_lcd_width_to_kernel(), load_lcd_hight_to_kernel());

	str_len = strlen(buf);

	buf[str_len] = '\0';

	ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	return ret;
}

__weak const char *lcd_get_name(void) { return NULL; }

int fdt_fixup_lcdname(void *fdt)
{
	char buf[64];
	const char *lcd_name;
	lcd_name = lcd_get_name();
	if (!lcd_name)
		return 0;

	memset(buf, 0, sizeof(buf));

	sprintf(buf, "lcd_name=%s", lcd_name);

	return fdt_chosen_bootargs_append(fdt, buf, 1);
}

#ifdef CONFIG_SPI_SLAVER_PANEL
int fdt_fixup_spi_panel_name(void *fdt)
{
	char buf[64];
	const char *spi_panel_name;
	spi_panel_name = spi_panel_get_name();
	if (!spi_panel_name)
		return 0;

	memset(buf, 0, sizeof(buf));

	sprintf(buf, "spi_panel_name=%s", spi_panel_name);

	return fdt_chosen_bootargs_append(fdt, buf, 1);
}
#endif

int fdt_fixup_lcdbpix(void *fdt)
{
	char buf[16];
	uint32_t bpix = 0;

	memset(buf, 0, 16);
	bpix = lcd_get_bpix();
	sprintf(buf, "logo_bpix=%d", bpix);

	return fdt_chosen_bootargs_append(fdt, buf, 1);
}
#endif

int fdt_fixup_pixelclock(void *fdt)
{
	char buf[32];
	uint32_t pixel_clk;

	memset(buf, 0, 32);
	pixel_clk = lcd_get_pixel_clock();
	sprintf(buf, "pixel_clock=%u", pixel_clk);

	return fdt_chosen_bootargs_append(fdt, buf, 1);
}

#ifdef CONFIG_SENSOR_HUB_LK
__weak const char *load_sensor_to_kernel(void) { return NULL; };
int fdt_fixup_sensor_name(void *fdt)
{
	char buf[512];
	const char *temp_name;

	temp_name = load_sensor_to_kernel();
	if (!temp_name)
		return 0;

	memset(buf, 0, sizeof(buf));

	sprintf(buf, "sensor_name=%s", temp_name);

	return fdt_chosen_bootargs_append(fdt, buf, 1);
}
#endif

int fdt_fixup_serialno(void *fdt)
{
	char buf[255];
	int str_len;
	int ret;
	memset(buf, 0, 255);

	sprintf(buf, " androidboot.serialno=%s", get_product_sn());
	str_len = strlen(buf);
	buf[str_len] = '\0';
	ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	return ret;
}

void boot_adjust_wdt_flag(void)
{
	char buf_d[32] = {0};
	if (common_raw_read("miscdata", (uint64_t)DL_MODE_DATA_LEN,
			DL_MODE_OFFSET, buf_d)) {
		errorf("read download mode data error from miscdata...\n");
		return;
	}
	debugf("download mode flag is : %s\n", buf_d);
	if (!memcmp(buf_d, "auto", 4)) {
#if !DEBUG
		if (0 != common_raw_write("miscdata", WDTEN_DATA_LEN, 0, WDTEN_DATA_OFFSET, "enabled")) {
			errorf("fix user default WDT flag to enable fail\n");
			return;
		} else {
			dprintf(ALWAYS, "fix user default WDT flag to enable success\n");
		}
#else
		if (0 != common_raw_write("miscdata", WDTEN_DATA_LEN, 0, WDTEN_DATA_OFFSET, "disable")) {
			errorf("fix userdebug default WDT flag to disable fail\n");
			return;
		} else {
			debugf("fix userdebug default WDT flag to disable success\n");
		}
#endif
		if (0 != common_raw_erase("miscdata", DL_MODE_DATA_LEN, DL_MODE_OFFSET)) {
			errorf("erase download mode flag fail!\n");
			return;
		} else {
			debugf("erase download mode flag success\n");
		}
	}
}

int fdt_fixup_wdten(void *fdt)
{
	unsigned int val_en;
	char buf[32], buf_p[32] = {0};

#if !DEBUG
	val_en = WDTEN_MAGIC;
#else
	val_en = 0;
#endif
	memset(buf, 0, 32);
	boot_adjust_wdt_flag();
	if (common_raw_read("miscdata", (uint64_t)WDTEN_DATA_LEN,
			    WDTEN_DATA_OFFSET, buf_p))
		errorf("read wdten data error from miscdata...\n");
	else {
		if (!strncmp("enabled", buf_p, strlen("enabled"))) {
			debugf("wdt was enabled by miscdata\n");
			val_en = WDTEN_MAGIC;
		} else if (!strncmp("disable", buf_p, strlen("disable"))) {
			debugf("wdt was disabled by miscdata\n");
			val_en = 0;
		} else {
			debugf("wdt has not beed assigned by miscdata\n");
		}
	}
#ifndef CONFIG_BOOTCONFIG
	sprintf(buf, " androidboot.wdten=%x", val_en);
#else
	sprintf(buf, " sprdboot.wdten=%x", val_en);
#endif

#if !DEBUG
	if (!val_en)
		stop_watchdog();
#endif

	return fdt_chosen_bootargs_append(fdt, buf, 1);
}

int fdt_fixup_dswdten(void *fdt)
{
	char *str_en = NULL;
	char buf[32], buf_p[32] = {0};

	str_en = "enabled";

	memset(buf, 0, 32);
	if (common_raw_read("miscdata", (uint64_t)DSWDTEN_DATA_LEN,
			    DSWDTEN_DATA_OFFSET, buf_p))
		errorf("read dswdten data error from miscdata...\n");
	else {
		if (!strncmp("enabled", buf_p, strlen("enabled"))) {
			debugf("dswdt was enabled by miscdata\n");
			str_en = "enabled";
		} else if (!strncmp("disable", buf_p, strlen("disable"))) {
			debugf("dswdt was disabled by miscdata\n");
			str_en = "disable";
		} else {
			debugf("dswdt has not beed assigned by miscdata\n");
		}
	}
#ifndef CONFIG_BOOTCONFIG
	sprintf(buf, " androidboot.dswdten=%s", str_en);
#else
	sprintf(buf, " sprdboot.dswdten=%s", str_en);
#endif

	return fdt_chosen_bootargs_append(fdt, buf, 1);
}

#ifdef CONFIG_USBPINMUX
int fdt_fixup_usbmux(void *fdt)
{
	unsigned int cfg_val = 0;
	char buf[32], buf_p[32] = {0};

	memset(buf, 0, 32);
	if (common_raw_read("miscdata", (uint64_t)USBMUX_DATA_LEN,
			    USBMUX_DATA_OFFSET, buf_p))
		errorf("read usbmux data error from miscdata...\n");
	else {
		if (!strcmp("jtag", buf_p)) {
			debugf("usb is configed as jtag by miscdata\n");
			usb_mux_jtag_config();
			cfg_val = USBMUX_JTAG;
		} else if (!strcmp("uart", buf_p)) {
			debugf("usb is configed as uart by miscdata\n");
			usb_mux_uart_config();
			cfg_val = USBMUX_UART;
		} else if (!strcmp("jtag_apwdg", buf_p)) {
			debugf("usb is configed as jtag_apwdg by miscdata\n");
			usb_mux_jtag_apwdg_config();
			cfg_val = USBMUX_JTAG_APWDG;
		} else if (!strcmp("off", buf_p)) {
			debugf("usb is configed as off by miscdata\n");
			cfg_val = USBMUX_USB_DEFAULT;
		} else {
			debugf("usbmux has not been assigned by miscdata\n");
			cfg_val = USBMUX_UNASSIGNED;
		}
	}
#ifndef CONFIG_BOOTCONFIG
	sprintf(buf, " androidboot.usbmux=0x%x", cfg_val);
#else
	sprintf(buf, " sprdboot.usbmux=0x%x", cfg_val);
#endif

	return fdt_chosen_bootargs_append(fdt, buf, 1);
}
#endif
extern int g_cons_baudrate;
int fdt_fixup_baudrate(void *fdt)
{
	int nodeoffset;
	const char *path;
	char *path_copy, *s, *v, *v_copy, *k, *r, *old_baud;
	char buf[32] = {0};
	int baudrate = g_cons_baudrate;
	int ret = 0;

	if (!baudrate) {
		errorf("baudrate is not set");
		return -1;
	}

	sprintf(buf, "%dn8", baudrate);

	/*
	 * Find the "chosen" node.
	 */
	nodeoffset = fdt_path_offset(fdt, "/chosen");

	/*
	 * If there is no "chosen" node in the blob, leave.
	 */
	if (nodeoffset < 0) {
		errorf("fdt_chosen_bootargs_replace: cann't find chosen");
		return -1;
	}

	/*
	 * If the property exists, get the value
	 */
	path = fdt_getprop(fdt, nodeoffset, "bootargs", NULL);
	if (path == NULL) {
		errorf("bootargs is null\n");
		return -1;
	}

	path_copy = strdup(path);
	if (path_copy == NULL) {
		errorf("pathcopy string copy failed!\n");
		return -1;
	}
	s = path_copy;

	while ((v = strsep(&s, " ")) != NULL) {
		v_copy = strdup(v);
		if (v_copy == NULL) {
			errorf("string copy failed!\n");
			if (path_copy != NULL) {
				free(path_copy);
			}
			return -1;
		}
		/* get value */
		k = strsep(&v, "=");
		if (!k) {
			free(v_copy);
			v_copy = NULL;
			continue;
		}
		/* If earlycon has baudrate, replace it */
		if (strcmp("earlycon", k) == 0) {
			/* find last ',' in earlycon */
			r = strrchr(v_copy, ',');
			if (!r) {
				debugf("earlycon has no baudrate\n");
				if (v_copy != NULL) {
					free(v_copy);
					v_copy = NULL;
				}
				continue;
			}
			/* split old baudrate from str */
			old_baud = strsep(&r, " ") + 1;
			debugf("earlycon old baudrate: %s\n", old_baud);
			ret = fdt_chosen_bootargs_replace(fdt, old_baud, buf);

			if (ret < 0)
				errorf("update %s failed!\n", buf);
			else
				debugf("update kernel cmdline earlycon baudrate: %s\n", buf);

			if (v_copy != NULL) {
				free(v_copy);
				v_copy = NULL;
			}
			continue;
		}
		/* If console has baudrate, replace it */
		else if (strcmp("console", k) == 0) {
			/* find last ',' in concole */
			r = strrchr(v_copy, ',');
			if (!r) {
				debugf("console has no baudrate\n");
				if (v_copy != NULL) {
					free(v_copy);
				}
				break;
			};
			/* split old baudrate from str */
			old_baud = strsep(&r, " ") + 1;
			debugf("console old baudrate: %s\n", old_baud);
			ret = fdt_chosen_bootargs_replace(fdt, old_baud, buf);

			if (ret < 0)
				errorf("update %s failed!\n", buf);
			else
				debugf("update kernel cmdline console baudrate: %s\n", buf);

			if (v_copy != NULL) {
				free(v_copy);
			}
			break;
		}

		if (v_copy != NULL) {
			free(v_copy);
			v_copy = NULL;
		}
	}
	if (path_copy != NULL) {
		free(path_copy);
	}
	return ret;
}

int fdt_fixup_dvfs_set(void *fdt)
{
	unsigned int val = 0;
	char buf[32], dvfs_data[32] = {0};
	unsigned int *data_ptr = 0;

	data_ptr = (unsigned int *)&dvfs_data[0];

	memset(buf, 0, 32);
	if (common_raw_read("miscdata", (uint64_t)DVFS_SET_LEN,
			    (uint64_t)DVFS_SET_OFFSET, dvfs_data)) {
		errorf("read dvfs_set data error from miscdata...\n");
		return 0;
	}

	val = *data_ptr;
	debugf("update kernel cmdline dvfs_set: 0x%x\n", val);

#ifndef CONFIG_BOOTCONFIG
	sprintf(buf, " androidboot.dvfs_set=0x%x", val);
#else
	sprintf(buf, " sprdboot.dvfs_set=0x%x", val);
#endif

	return fdt_chosen_bootargs_append(fdt, buf, 1);
}

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
int fdt_fixup_oem_repair(void *fdt)
{
	char buf[256];

	memset(buf, 0, 256);
	if (fb_oem_repair_get_booargs(buf, sizeof(buf)) <= 0) {
		return 0;
	}

	return fdt_chosen_bootargs_append(fdt, buf, 1);
}
#endif

int fdt_fixup_memleakon(void *fdt)
{
	char buf[32];
	int str_len;
	int ret;
	memset(buf, 0, 32);


	sprintf(buf, "kmemleak=on");
	str_len = strlen(buf);
	buf[str_len] = '\0';
	ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	return ret;
}

#ifdef SPRD_SYSDUMP
int fdt_fixup_sysdump_magic(void *fdt)
{
	char buf[64];
	int str_len;
	int ret;
	int sysdump_re_flag;
	void* magic = (void *)SPRD_SYSDUMP_MAGIC;
	memset(buf, 0, 64);

	/* record whether sysdump is reserved in dts,1 yes, 0 no*/
	if (SPRD_SYSDUMP_MAGIC != RAMDISK_ADR)
		sysdump_re_flag = 1;
	else
		sysdump_re_flag = 0;

	sprintf(buf, " sysdump_magic=%lx sysdump_re_flag=%d", (unsigned long)magic, sysdump_re_flag);
	str_len = strlen(buf);
	buf[str_len] = '\0';
	//debugf("sysdump flag in fdt:%s\n", buf);
	ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	return ret;
}

extern int get_sysdump_status(void);
int fdt_fixup_sysdump_bootloader(void *fdt)
{
	int err;
	char *pStr=NULL;
	char nodename[256];
	int offset, str_len;
	fdt_addr_t s_addr, s_size;
	int parentoffset;

	parentoffset = fdt_path_offset(fdt, "/reserved-memory");
	if (parentoffset < 0) {
		debugf("cannot find reserved-memory node.\n" );
		return -1;
	}

	for (offset = fdt_first_subnode(fdt, parentoffset);
		offset >= 0; offset = fdt_next_subnode(fdt, offset)) {
		sprintf(nodename, "%s", fdt_get_name(fdt, offset, NULL));
		str_len = strlen(nodename);
		nodename[str_len] = '\0';

		pStr = strstr(nodename, "sysdump-uboot");
		if (pStr) {
			debugf("find sysdump-uboot reserved memory,pStr=%s, nodename = %s.\n", pStr, nodename);

			if (fdtdec_decode_region(fdt, offset, "reg", &s_addr, &s_size) < 0) {
				debugf("cannot find reg prop in sysdump-uboot node.\n" );
				return -1;
			} else {
				debugf("sysdump-uboot: addr is 0x%lx, size is 0x%lx\n", s_addr, s_size);
#if !DEBUG
				int sysdump_enabled = 0;

				sysdump_enabled = get_sysdump_status();
				if (!sysdump_enabled) {
					dprintf(INFO, "user version and sysdump disabled, remove sysdump-uboot reserved memory\n");
					err = fdt_del_node(fdt, offset);
					if (err < 0)
						errorf("del sysdump-uboot reserved mem failed: %s.\n", fdt_strerror(err));

				} else {
					dprintf(INFO, "user version and sysdump enabled, do not remove sysdump-uboot\n");
				}
#else
				dprintf(INFO, "UserDebug version do not fixup sysdump-uboot!\n");
#endif
			}
			return 0;
		}
	}

	debugf("can not find sysdump-uboot reserved memory\n" );
	return -1;
}
#endif

#if 0 //remove for this function due to No API Call
int fdt_fixup_dram_training(void *fdt)
{
	char buf[64];
	int str_len;
	int ret;
	uint64_t addr;
	uint64_t size;
	size = 0x1000;
	memset(buf, 0, 64);

	sprintf(buf, " mem_cs=%d, mem_cs0_sz=%08x", get_dram_cs_number(), get_dram_cs0_size());
	str_len = strlen(buf);
	buf[str_len] = '\0';
	ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	debugf(" mem_cs=%d, mem_cs0_sz=%08x", get_dram_cs_number(), get_dram_cs0_size());

	/* gerenally, reserved memory should be configured in dts file. Add fixup here to avoid the wide
	 * range of change for various boards.
	 * reserved for ddr training data used
	 */
	if (ret < 0)
		errorf("failed to append ddr training data args bootargs\n");
	else {
		addr = PHYS_SDRAM_1;
		ret = fdt_add_mem_rsv(fdt, addr, size);
		if(ret < 0) {
			errorf("failed to reserved cs0 ddr training data space\n");
		}
		else {
			if(2 == get_dram_cs_number()) {
				addr = PHYS_SDRAM_1 + get_dram_cs0_size();
				ret = fdt_add_mem_rsv(fdt, addr, size);
				if(ret < 0) {
					errorf("failed to reserved cs1 ddr training data space\n");
				}
			}
		}
	}

	return ret;
}
#endif

/**
 * You can re-define function void fdt_fixup_chosen_bootargs_board(char *buf, const char *boot_mode, int calibration_mode)
 * in your u-boot/board/spreadtrum/xxx/xxx.c to override this default function
 */
void __attribute__ ((weak)) fdt_fixup_chosen_bootargs_board(char *buf, int calibration_mode)
{
}

int fdt_fixup_chosen_bootargs_board_private(void *fdt)
{
	int ret = 0;
	char *buf = NULL;

	buf = malloc(512);
	if(buf == NULL) {
		errorf("%s: malloc error!\n", __func__);
		return -1;
	}
	memset(buf, 0, 512);
	fdt_fixup_chosen_bootargs_board(buf, poweron_by_calibration());
	if (buf[0]) {
		ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	}
	free(buf);

	return ret;
}

extern phys_size_t get_real_ram_size(void);
/**
 * Fix me:
 * fix_memory_size() interface which can be fixup memory size, but because of our
 * device tree is not suitable for the context, for simple, we add a new function. In arm64,
 * we should discard it. It should be ok now.
 */
int fdt_fixup_ddr_size(void *fdt)
{
#ifdef SPRD_DDR_AUTO_DETECT
	int nodeoffset;
	int err;
	phys_addr_t ddr_base = CONFIG_SYS_SDRAM_BASE;
	phys_size_t ddr_size = get_real_ram_size();

	nodeoffset = fdt_path_offset(fdt, "/memory");
	debugf("/memory nodeoffset = %d, ddr_base: 0x%lx, ddr_size: 0x%lx\n",
		nodeoffset, ddr_base, ddr_size);
	if (nodeoffset < 0) {
	    errorf("ERROR: device tree must have /memory node %s.\n", fdt_strerror(nodeoffset));
	    return nodeoffset;
	}

	fdt_delprop(fdt, nodeoffset, "reg");

#ifdef CONFIG_ARM64
	err = fdt_setprop_u64(fdt, nodeoffset, "reg", ddr_base);
	if (err < 0) {
	    errorf("ERROR: cannot set /memory node's reg property(addr)!\n");
	    return err;
	}

	err = fdt_appendprop_u64(fdt, nodeoffset, "reg", ddr_size);
	if (err < 0) {
	    errorf("ERROR: cannot set /memory node's reg property(size)!\n");
	}
#else
	err = fdt_setprop_u32(fdt, nodeoffset, "reg", ddr_base);
	if (err < 0) {
	    errorf("ERROR: cannot set /memory node's reg property(addr)!\n");
	    return err;
	}

	err = fdt_appendprop_u32(fdt, nodeoffset, "reg", ddr_size);
	if (err < 0) {
	    errorf("ERROR: cannot set /memory node's reg property(size)!\n");
	}
#endif

	return err;
#else
	return 0;
#endif
}

int fdt_fixup_ro_boot_ramsize(void *fdt)
{
#ifdef SPRD_DDR_AUTO_DETECT
	char buf[32];

	phys_size_t ddr_size;
	uint64_t ro_boot_ddrsize = 0;
	int str_len;
	int ret;

	ddr_size = get_real_ram_size();
	ddr_size = ALIGN(ddr_size, 0x100000);
	ro_boot_ddrsize = ddr_size >> 20;

	memset(buf, 0, 32);

	sprintf(buf, "androidboot.ddrsize=%lldM", ro_boot_ddrsize);
	str_len = strlen(buf);
	buf[str_len] = '\0';

	ret = fdt_chosen_bootargs_append(fdt, buf, 1);

	return ret;
#else
	return 0;
#endif
}

int fdt_fixup_ddrsize_range(void *fdt) {
    char buf[64], buf2[32];
    const char *range = NULL;
    int ret, str_len;
    memset(buf, 0, sizeof(buf));
#ifdef SPRD_DDR_AUTO_DETECT
    uint64_t ro_boot_ddrsize = 0;
    phys_size_t ddr_size;

    ddr_size = get_real_ram_size();
    ddr_size = ALIGN(ddr_size, 0x100000);
    ro_boot_ddrsize = ddr_size >> 20;

    if (ro_boot_ddrsize < 512) {
        range = "[0,512)";
    } else if (ro_boot_ddrsize < 1024) {
        range = "[512,1024)";
    } else if (ro_boot_ddrsize < 6144){
        uint64_t mid_size = 0;
        mid_size = (ro_boot_ddrsize/1024)*1024;
        sprintf(buf2, "[%lld,%lld)", mid_size, mid_size+1024);
        range = buf2;
    } else {
        range = "[6144,)";
    }
#else
    range = "[1024,2048)";
#endif
    sprintf(buf, "androidboot.ddrsize.range=%s", range);
    str_len = strlen(buf);
    buf[str_len] = '\0';

    ret = fdt_chosen_bootargs_append(fdt, buf, 1);

    return ret;
}

#ifdef CONFIG_SANSA_SECBOOT
int fdt_fixup_socid(void *fdt)
{
	char buf[128];
	char *tmp = NULL;
	uint8_t soc_id[32] = {0};
	int str_len;
	int ret;
	int i;

	ret = sansa_compute_socid(soc_id);
	if (ret != 0) {
		errorf("ERROR: compute socid fail!\n");
		return ret;
	}
	memset(buf, 0, sizeof(buf));

	str_len = sprintf(buf, "soc_id=");
	tmp = buf + str_len;
	for (i=0; i<32; i++,tmp+=2) {
		str_len += sprintf(tmp,"%02X",soc_id[i]);
	}
	buf[str_len] = '\0';

	ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	return ret;
}
#endif

#ifdef CONFIG_BOARD_KERNEL_CMDLINE
/**
 *  merge board kernel cmdline to bootargs
 */
int fdt_fixup_board_kernel_cmdline(void *fdt)
{
	int nodeoffset;
	int err;
	int append_flag = 0, ret = -1;
	const char *path;
	char *buf = NULL;
	boot_img_hdr *hdr = (boot_img_hdr *)raw_header;
	char *v, *k, *tmp_v, *tmp_k, *cmdline, *v_copy, *path_copy, *s;
	if(!strlen(hdr->cmdline)) {
		dprintf(ALWAYS, "android kernel cmdline from header is empty!\n");
		return ret;
	}

	buf = malloc(BOOT_ARGS_SIZE);
	if(buf == NULL) {
		errorf("%s: malloc error\n", __func__);
		return ret;
	}
	memset(buf, 0, BOOT_ARGS_SIZE);
	memcpy(buf, hdr->cmdline, BOOT_ARGS_SIZE);
	cmdline = buf;

	debugf("android kernel cmdline :%s\n",cmdline);

	/* first, get bootargs value */
	err = fdt_check_header(fdt);
	if (err < 0) {
		ret = err;
		errorf("fdt_chosen_bootargs_replace: %s\n", fdt_strerror(err));
		goto end;
	}

	/*
	 * Find the "chosen" node.
	 */
	nodeoffset = fdt_path_offset(fdt, "/chosen");

	/*
	 * If there is no "chosen" node in the blob, leave.
	 */
	if (nodeoffset < 0) {
		errorf("fdt_chosen_bootargs_replace: cann't find chosen");
		goto end;
	}

	/*
	 * If the property exists, get the value
	 */
	path = fdt_getprop(fdt, nodeoffset, "bootargs", NULL);
	debugf("before bootargs:%s\n",path);

	/* second, append */
	if (path == NULL) {/* It's almost never gonna happen */
		dprintf(INFO,"bootargs empty, board kernel cmdline append!\n");
		ret = fdt_chosen_bootargs_append(fdt, cmdline, 1);
		if (ret < 0)
			errorf("failed to append board kernel cmdline to bootargs\n");
		goto end;
	}
	/* loop for matching */
	while ((v = strsep(&cmdline, " ")) != NULL) {
		v_copy = strdup(v);
		if (v_copy == NULL) {
			errorf("string copy failed!\n");
			goto end;
		}

		k = strsep(&v, "=");
		if (!k)
			break;

		path_copy = strdup(path);
		if (path_copy == NULL) {
			errorf("pathcopy string copy failed!\n");
			free(v_copy);
			goto end;
		}
		s = path_copy;

		while ((tmp_v = strsep(&s, " ")) != NULL) {
			tmp_k = strsep(&tmp_v, "=");
			if (!tmp_k)
				break;

			if (strcmp(tmp_k, k) == 0) {
				append_flag = 0;
				break;
			}

			append_flag = 1;
		}

		if (append_flag) {
			ret = fdt_chosen_bootargs_append(fdt, v_copy, 1);
			if (ret < 0) {
				errorf("failed to append board kernel cmdline to bootargs\n");
				free(v_copy);
				free(path_copy);
				goto end;
			}
			append_flag = 0;
		}

		free(path_copy);
		free(v_copy);
	}

	path = fdt_getprop(fdt, nodeoffset, "bootargs", NULL);
	debugf("after bootargs:%s\n",path);
	ret = 0;

end:
	free(buf);
	return ret;
}

int fdt_fixup_board_kernel_cmdline_from_vendorboot(void *fdt)
{
	int nodeoffset;
	int err, ret = -1;
	int append_flag = 0;
	const char *path;
	char *buf = NULL;
	char *v, *k, *tmp_v, *tmp_k, *cmdline, *v_copy, *path_copy, *s;
	boot_img_hdr *hdr = (boot_img_hdr *)raw_header;

	if (BOOT_HEADER_VERSION_TWO == hdr->header_version ||
					BOOT_HEADER_VERSION_ONE == hdr->header_version) {
		dprintf(ALWAYS, "[%s] skip this while boot header version is not v3 or v4.\n", __func__);
		return 0;
	}

	if(!strlen(vendorboot_cmdline)) {
		dprintf(ALWAYS, "vendorboot cmdline is empty!\n");
		return ret;
	}

	buf = malloc(VENDOR_BOOT_ARGS_SIZE);
	if(buf == NULL) {
		errorf("%s: malloc error\n", __func__);
		return ret;
	}
	memset(buf, 0, VENDOR_BOOT_ARGS_SIZE);
	memcpy(buf, vendorboot_cmdline, VENDOR_BOOT_ARGS_SIZE);
	cmdline = buf;

	debugf("android kernel cmdline :%s\n",cmdline);

	/* first, get bootargs value */
	err = fdt_check_header(fdt);
	if (err < 0) {
		ret = err;
		errorf("fdt_chosen_bootargs_replace: %s\n", fdt_strerror(err));
		goto end;
	}

	/*
	 * Find the "chosen" node.
	 */
	nodeoffset = fdt_path_offset(fdt, "/chosen");

	/*
	 * If there is no "chosen" node in the blob, leave.
	 */
	if (nodeoffset < 0) {
		errorf("fdt_chosen_bootargs_replace: cann't find chosen");
		goto end;
	}

	/*
	 * If the property exists, get the value
	 */
	path = fdt_getprop(fdt, nodeoffset, "bootargs", NULL);
	debugf("before bootargs:%s\n",path);

	/* second, append */
	if (path == NULL) {/* It's almost never gonna happen */
		dprintf(INFO,"bootargs empty, board kernel cmdline append!\n");
		ret = fdt_chosen_bootargs_append(fdt, cmdline, 1);
		if (ret < 0)
			errorf("failed to append board kernel cmdline to bootargs\n");
		goto end;
	}

	/* loop for matching */
	while ((v = strsep(&cmdline, " ")) != NULL) {
		v_copy = strdup(v);
		if (v_copy == NULL) {
			errorf("string copy failed!\n");
			goto end;
		}

		k = strsep(&v, "=");
		if (!k)
			break;

		path_copy = strdup(path);
		if (path_copy == NULL) {
			errorf("pathcopy string copy failed!\n");
			free(v_copy);
			goto end;
		}
		s = path_copy;

		while ((tmp_v = strsep(&s, " ")) != NULL) {
			tmp_k = strsep(&tmp_v, "=");
			if (!tmp_k)
				break;

			if (strcmp(tmp_k, k) == 0) {
				append_flag = 0;
				break;
			}

			append_flag = 1;
		}

		if (append_flag) {
			ret = fdt_chosen_bootargs_append(fdt, v_copy, 1);
			if (ret < 0) {
				errorf("failed to append board kernel cmdline to bootargs\n");
				free(v_copy);
				free(path_copy);
				goto end;
			}
			append_flag = 0;
		}

		free(path_copy);
		free(v_copy);
	}

	path = fdt_getprop(fdt, nodeoffset, "bootargs", NULL);
	debugf("after bootargs:%s\n",path);
	ret = 0;

end:
	free(buf);
	return ret;
}
#endif

/**
 *  Fix me:
 *  set dtbo index
 */
int fdt_fixup_dtbo_index(void *fdt)
{
	int nodeoffset;
	const char *path;
	char *path_copy = NULL, *s, *v, *v_copy = NULL, *k;
	char buf[DTBO_INDEX_DATA_LEN];
	uchar is_done = 0;
	int dtboindex = g_DtboIndex;
	int ret = 0;
	char *saved_p0 = NULL;
	char *saved_p1 = NULL;

	memset(buf, 0, sizeof(buf));

	/*
	 * Find the "chosen" node.
	 */
	nodeoffset = fdt_path_offset(fdt, "/chosen");

	/*
	 * If there is no "chosen" node in the blob, leave.
	 */
	if (nodeoffset < 0) {
		errorf("fdt_chosen_bootargs_replace: cann't find chosen");
		return -1;
	}

	/*
	 * If the property exists, get the value
	 */
	path = fdt_getprop(fdt, nodeoffset, "bootargs", NULL);
	if (path == NULL) {
		errorf("bootargs is null\n");
		return -1;
	}

	path_copy = strdup(path);
	if (path_copy == NULL) {
		errorf("pathcopy string copy failed!\n");
		return -1;
	}
	s = path_copy;

	while ((v = strtok_r(s, " ", &saved_p0)) != NULL) {
		s = NULL;
		v_copy = strdup(v);
		if (v_copy == NULL) {
			errorf("string copy failed!\n");
			free(path_copy);
			return -1;
		}
		saved_p1 = NULL;
		k = strtok_r(v, "=", &saved_p1);
		if (!k) {
			free(v_copy);
			continue;
		}

		if (strcmp("androidboot.dtbo_idx", k) == 0) {
			/* get value */
			k = strtok_r(NULL, "=", &saved_p1);
			if (!k) {
				free(v_copy);
				continue;
			}

			if (dtboindex) {
				sprintf(buf, "androidboot.dtbo_idx=%d",dtboindex);
				ret = fdt_chosen_bootargs_replace(fdt, v_copy, buf);
				if (ret < 0)
					errorf("update %s failed!\n", buf);
				else
					debugf("update kernel cmdline %s\n", buf);
			}
			is_done = 1;
			break;
		}
	}

	/*
         * If dtbo idx doesn't exist,append it
         */
	if (!is_done) {
		sprintf(buf, "androidboot.dtbo_idx=%d",dtboindex);
		ret = fdt_chosen_bootargs_append(fdt, buf, 1);
		if (ret < 0)
			errorf("set %s failed!\n", buf);
		else
			debugf("set kernel cmdline %s\n", buf);
	}

	if (NULL != v_copy)
		free(v_copy);

	if (NULL != path_copy)
		free(path_copy);

	return ret;
}

/**
 *  Fix me:
 *  set loglevel = 7
 */
int fdt_fixup_loglevel(void *fdt)
{
	int nodeoffset;
	const char *path;
	int ret = 0;
	int loglevel;
	char *path_copy, *s, *v, *v_copy = NULL, *k, *p;
	char buffer[DEBUG_INFO_LEN];
	char *saved_p0 = NULL;
	char *saved_p1 = NULL;

	memset(buffer, 0, sizeof(buffer));
	p = buffer;

	/*
	 * Find the "chosen" node.
	 */
	nodeoffset = fdt_path_offset(fdt, "/chosen");

	/*
	 * If there is no "chosen" node in the blob, leave.
	 */
	if (nodeoffset < 0) {
		errorf("fdt_chosen_bootargs_replace: cann't find chosen");
		return -1;
	}

	/*
	 * If the property exists, get the value
	 */
	path = fdt_getprop(fdt, nodeoffset, "bootargs", NULL);
	if (path == NULL) {
		errorf("bootargs is null\n");
		return -1;
	}

	path_copy = strdup(path);
	if (path_copy == NULL) {
		errorf("pathcopy string copy failed!\n");
		return -1;
	}
	s = path_copy;

	while ((v = strtok_r(s, " ", &saved_p0)) != NULL) {
		s = NULL;
		v_copy = strdup(v);
		if (v_copy == NULL) {
			errorf("string copy failed!\n");
			free(path_copy);
			return -1;
		}

		saved_p1 = NULL;
		k = strtok_r(v, "=", &saved_p1);
		if (!k) {
			free(v_copy);
			continue;
		}

		if (strcmp("loglevel", k) == 0) {
			/* get value */
			k = strtok_r(NULL, "=", &saved_p1);
			if (!k) {
				free(v_copy);
				continue;
			}

			loglevel = strtol(k, NULL, 10);
			debugf("kernel loglevel = %d\n",loglevel);
			if (loglevel > 7) {
				free(v_copy);
				break;
			}

			if(common_raw_read("miscdata", (uint64_t)DEBUG_INFO_LEN, DEBUG_INFO_OFFSET, p)) {
				errorf("read miscdata loglevel data error.\n");
				free(v_copy);
				break;
			}

			debugf("read loglevel data {%s} from misc\n", p);
			if (!strncmp("enable", p, strlen("enable"))) {
				char buf[16], *t = p, l = '\0';

				t += strlen("enable");
				if (!t[0])
					l = '7';
				else if (!strncmp(t, ":level=", strlen(":level="))) {
					t += strlen(":level=");
					if (t[0] >= '1' && t[0] <= '6' && t[1] == '\0')
						l = t[0];
				}

				if (l) {
					sprintf(buf, "loglevel=%c", l);
					ret = fdt_chosen_bootargs_replace(fdt, v_copy, buf);
					if (ret < 0)
						errorf("set %s failed!\n", buf);
					else
						debugf("\nupdate kernel %s\n", buf);
				}
			}

			free(v_copy);
			break;
		}

		free(v_copy);
	}

	free(path_copy);
	return ret;
}

int fdt_fixup_selinux_switch(void *fdt)
{
	char selinux_info[SELINUX_INFO_LEN] = {0};

	if (common_raw_read("miscdata", (u64)SELINUX_INFO_LEN,
			(u64)SELINUX_SWITCH_OFFSET, selinux_info)) {
		errorf("read miscdata selinux authority error.\n");
		return -1;
	}

	if (!strcmp(selinux_info, "Selinux:0")) {
		dprintf(INFO, "should setup selinux permissive\n");
		if (fdt_chosen_bootargs_append(fdt, "androidboot.selinux=permissive", 1)) {
			errorf("setup selinux fail!\n");
			return -2;
		}
		debugf("androidboot.selinux=permissive\n");
	}

	return 0;
}
char s_mask_serial_num[65];

int fdt_setprop_cpu_serial_number(void *fdt, char *serial)
{
	//modify by hyinfeng for NYX-275 Unique SoC serial number to ro.boot.uniqueno begin
	//int ret;
	int ret1;
	//char buf[100];
	//memset(buf, 0, 100);
	strcpy(s_mask_serial_num,serial);
	//sprintf(buf, "androidboot.uniqueno=%s", serial);
	//ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	ret1 = fdt_setprop(fdt, 0, "serial-number", serial, strlen(serial) + 1);
	return ret1;
	//modify by hyinfeng for NYX-275 Unique SoC serial number to ro.boot.uniqueno end
}

#if defined(CONFIG_GET_CPU_SERIAL_NUMBER_NO_WD)
extern int sprd_get_chip_hex_uid(char *buf);
int fdt_fixup_cpu_serial_number(void *fdt)
{
	int err;
	char serial_num[17];

	memset(serial_num, 0, sizeof(serial_num));
	err = sprd_get_chip_hex_uid(serial_num);
	if (err < 0) {
		errorf("read serial number error\n");
		return err;
	}

	err = fdt_setprop_cpu_serial_number(fdt, serial_num);
	if (err < 0) {
		errorf("set serial number error\n");
		return err;
	}

	return 0;
}
#elif defined(CONFIG_GET_CPU_SERIAL_NUMBER)
extern int sprd_get_chip_hex_uid(char *buf);
extern void sha256_csum_wd_sw(const unsigned char *input,unsigned int ilen,
		unsigned char *output,unsigned int chunk_sz);
int fdt_fixup_cpu_serial_number(void *fdt)
{
	int err = 0;
	char serial_num[17];
	char raw_serial_num[32];
	char mask_serial_num[65];
	int cnt;

	memset(serial_num, 0, sizeof(serial_num));
	err = sprd_get_chip_hex_uid(serial_num);
	if (err < 0) {
		errorf("read serial number error\n");
		return err;
	}

	memset(raw_serial_num, 0, sizeof(raw_serial_num));
	//sha256_csum_wd_sw(serial_num, strlen(serial_num), raw_serial_num, NULL);
	SHA256_hash(serial_num, strlen(serial_num), raw_serial_num);

	memset(mask_serial_num, 0, sizeof(mask_serial_num));
	for (cnt = 0; cnt < sizeof(raw_serial_num); cnt++) {
		sprintf(mask_serial_num + cnt * 2, "%02x", raw_serial_num[cnt]);
	}

	err = fdt_setprop_cpu_serial_number(fdt, mask_serial_num);
	if (err < 0) {
		errorf("set serial number error\n");
		return err;
	}

	return 0;
}
#endif

#ifdef CONFIG_ANDROID_AB
int fdt_fixup_add_slot_suffix(void *fdt)
{
	int ret;
	char buf[64];
	const char *slot;

	slot = g_env_slot;
	if (!slot) {
		errorf("get env: slot fail!\n");
		return -1;
	}

#ifndef CONFIG_BOOTCONFIG
	snprintf(buf, ARRAY_SIZE(buf), "androidboot.slot_suffix=%s", slot);
#else
	snprintf(buf, ARRAY_SIZE(buf), "sprdboot.slot_suffix=%s", slot);
#endif

	ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	if (ret) {
		errorf("setup slot_suffix fail!\n");
		return -2;
	}

	return ret;
}

int fdt_fixup_add_force_mode(void *fdt)
{
	int ret;
	const char *buf;

	if (g_env_bootmode && (!strcmp("recovery", g_env_bootmode)))
		buf = "androidboot.force_normal_boot=0";
	else
		buf = "androidboot.force_normal_boot=1";

	debugf("set %s\n", buf);

	ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	if (ret) {
		errorf("setup force mode fail!\n");
		return -1;
	}

	return ret;
}
#endif

int fdt_fixup_bootcause(void *fdt)
{
	char buf[128] = {0};
	const char *boot_cause = bootcause_cmdline;
	int ret = 0;
	unsigned int len;

	if (!boot_cause) {
		errorf("bootcause_cmdline is not assigned\n");
		return -1;
	}

	len = snprintf(buf, ARRAY_SIZE(buf), "bootcause=\"%s\"", boot_cause);
	if (len < ARRAY_SIZE(buf)) {
		debugf("cmdline boot_cause=%s\n", boot_cause);
		ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	} else {
		errorf("bootcause cmdline was out of range\n");
		ret = -1;
	}

	return ret;
}

int fdt_fixup_pwroffcause(void *fdt)
{
	char buf[128] = {0};
	char *pwroff_cause = pwroffcause_cmdline;
	int ret = 0;

	if (!pwroff_cause) {
		errorf("pwroffcause_cmdline is not assigned\n");
		return -1;
	}

	if (snprintf(buf, ARRAY_SIZE(buf), "pwroffcause=\"%s\"", pwroff_cause) <
	    ARRAY_SIZE(buf)) {
		debugf("cmdline pwroff_cause=%s\n", pwroff_cause);
		ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	} else {
		errorf("pwroffcause cmdline was out of range\n");
		return -1;
	}

	return ret;
}

#if defined(SPRD_UFS_BOOTDEVICE) || defined(SPRD_EMMC_BOOTDEVICE)
int fdt_fixup_boot_device(void *fdt)
{
	char buf[255];
	int str_len;
	int ret;
	memset(buf, 0, 255);

#if defined(CONFIG_BLK_DEV_BOOT)
	if (get_bootdevice() == BOOT_DEVICE_UFS)
		sprintf(buf, SPRD_UFS_BOOTDEVICE); //ufs
	else if (get_bootdevice() == BOOT_DEVICE_EMMC)
		sprintf(buf, SPRD_EMMC_BOOTDEVICE); //eMMC
#else
	sprintf(buf, SPRD_EMMC_BOOTDEVICE); //eMMC
#endif

	str_len = strlen(buf);
	buf[str_len] = '\0';
	ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	return ret;
}
#endif

#if defined(SPRD_UFS_BOOTDEVICE) || defined(SPRD_EMMC_BOOTDEVICE)
int fdt_fixup_sprdboot_device(void *fdt)
{
	char buf[255];
	int str_len;
	int ret;
	memset(buf, 0, 255);

#if defined(CONFIG_BLK_DEV_BOOT)
	if (get_bootdevice() == BOOT_DEVICE_UFS)
		sprintf(buf, "sprdboot.flash=ufs"); //ufs
	else if (get_bootdevice() == BOOT_DEVICE_EMMC)
		sprintf(buf, "sprdboot.flash=emmc"); //eMMC
#else
	sprintf(buf, "sprdboot.flash=emmc"); //eMMC
#endif

	str_len = strlen(buf);
	buf[str_len] = '\0';
	ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	return ret;
}
#else
int fdt_fixup_sprdboot_device(void *fdt) {return 0;}
#endif

#ifdef CONFIG_TIME_STATISTIC
extern uint32_t lk_start_time;
extern uint32_t lk_end_time;
int fdt_fixup_timeconsuming(u8 *fdt)
{
	int ret;
	char buf[128] = {0};
	int str_len;

	chipram_env_t* cr_env = get_chipram_env();

	if ((cr_env->tspl_s > cr_env->tspl_e) || (cr_env->tspl_e > lk_start_time)
		|| (lk_start_time > lk_end_time)) {
		errorf("error occuered in time consuming\n");
		return -1;
	}

	sprintf(buf, "tspl=%dms tsml=%dms tuboot=%dms ",
		cr_env->tspl_e - cr_env->tspl_s, lk_start_time - cr_env->tspl_e, lk_end_time - lk_start_time);

	/* append time consuming to kernel cmdline*/
	ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	if (ret < 0) {
		errorf("failed to append time consuming to bootargs\n");
		return ret;
	}

	return 0;
}
#endif

int fdt_fixup_bootloader_log_reserved(void *fdt)
{
	int ret = 0;
#ifdef CONFIG_SPRD_LOG
	int rsv_offset, log_offset;
	uint32_t value[16];
	fdt_addr_t s_addr;
	fdt_addr_t s_size;

	rsv_offset = fdt_path_offset(fdt, "/reserved-memory");
	if (rsv_offset < 0) {
		errorf("Cannot get /reserved-memory node %s\n", fdt_strerror(rsv_offset));
		return rsv_offset;
	}

	ret = fdt_add_subnode(fdt, rsv_offset, "uboot_log-mem");
	if (ret < 0) {
		errorf("Cannot appendprop uboot_log-mem on/reserved-memory: %s.\n",
				fdt_strerror(ret));
		return ret;
	}

	log_offset = fdt_subnode_offset(fdt, rsv_offset, "uboot_log-mem");
	if (log_offset < 0) {
		errorf("Get log offset fail\n");
		return -1;
	}
#ifdef CONFIG_PHYS_64BIT
	s_addr = __cpu_to_be64((uint64_t)p_log_buffer);
	s_size = __cpu_to_be64((uint64_t)LOG_RESERVED_SIZE);
#else
	s_addr = __cpu_to_be32((uint32_t)p_log_buffer);
	s_size = __cpu_to_be32((uint32_t)LOG_RESERVED_SIZE);
#endif

	memcpy((char *)value, &s_addr, sizeof(fdt_addr_t));
	memcpy((char *)value + sizeof(fdt_addr_t), &s_size, sizeof(fdt_addr_t));

	ret = fdt_setprop(fdt, log_offset, "reg", value, 2 * sizeof(fdt_addr_t));
	if (ret < 0) {
		errorf("Setprop /reserved-memory fail %s\n", fdt_strerror(ret));
		return ret;
	}
#endif
	return ret;
}

#if DEBUG
int fdt_fixup_ptm_reserved(void *fdt)
{
	int ret = 0;
#ifdef PTM_RESERVED_ADDR
	int rsv_offset, ptm_offset;
	uint32_t value[16];
	fdt_addr_t s_addr;
	fdt_addr_t s_size;

	rsv_offset = fdt_path_offset(fdt, "/reserved-memory");
	if (rsv_offset < 0) {
		errorf("Cannot get /reserved-memory node %s\n", fdt_strerror(rsv_offset));
		return rsv_offset;
	}

	fdt_delprop(fdt, rsv_offset, "ptm-mem");

	ret = fdt_add_subnode(fdt, rsv_offset, "ptm-mem");
	if (ret < 0) {
		errorf("Cannot appendprop ptm-mem on/reserved-memory%s\n",
				fdt_strerror(ret));
		return ret;
	}

	ptm_offset = fdt_subnode_offset(fdt, rsv_offset, "ptm-mem");
	if (ptm_offset < 0) {
		errorf("Get ptm-mem offset fail\n");
		return -1;
	}

#ifdef CONFIG_PHYS_64BIT
	s_addr = __cpu_to_be64((uint64_t)PTM_RESERVED_ADDR);
	s_size = __cpu_to_be64((uint64_t)PTM_RESERVED_SIZE);
#else
	s_addr = __cpu_to_be32((uint32_t)PTM_RESERVED_ADDR);
	s_size = __cpu_to_be32((uint32_t)PTM_RESERVED_SIZE);
#endif

	memcpy((char *)value, &s_addr, sizeof(fdt_addr_t));
	memcpy((char *)value + sizeof(fdt_addr_t), &s_size, sizeof(fdt_addr_t));

	ret = fdt_setprop(fdt, ptm_offset, "reg", value, 2 * sizeof(fdt_addr_t));
	if (ret < 0) {
		errorf("Setprop /reserved-memory fail %s\n", fdt_strerror(ret));
		return ret;
	}
#endif

	return ret;
}

#endif

int fdt_fixup_startup_core(void *fdt)
{
	char buf[32] = {0};

#ifdef BOOTARG_MAX_CPUS
	sprintf(buf, "maxcpus=%d", BOOTARG_MAX_CPUS);
	return fdt_chosen_bootargs_append(fdt, buf, 1);
#else
	unsigned short val;
	unsigned char core_num = -1;

	if (!common_raw_read("miscdata", CORE_STARTUP_FLAG_LEN,
						 		CORE_STARTUP_FLAG_OFFSET, (char *)&val)) {
		if (0x5A == ((val & 0xFF00) >> 8)) {
			core_num = val & 0xFF;
			debugf("got startup core num=%d\n", core_num);
			sprintf(buf, "maxcpus=%d", core_num);
			return fdt_chosen_bootargs_append(fdt, buf, 1);
		}
	}

	return 0;
#endif
}

int fdt_fixup_vendor_init_flag(void *fdt)
{
	char buf[64] = {0};
	const char *boot_mode = NULL;
	int ret;

	boot_mode = g_env_bootmode;
	debugf("fdt_fixup_vendor_init_flag boot mode %s\n", boot_mode);
	/*cali/charger/factorytest*/
	if (strcmp("cali",boot_mode) == 0 || strcmp("charger",boot_mode) == 0 ||
		strcmp("factorytest",boot_mode) == 0) {
		sprintf(buf, "androidboot.vendor.skip.init=1");

	} else {
		sprintf(buf, "androidboot.vendor.skip.init=0");

	}
	ret = fdt_chosen_bootargs_append(fdt, buf, 1);

	return ret;
}

int fdt_print_bootargs(void *fdt)
{
	int nodeoffset;
	const char *path = NULL;

	nodeoffset = fdt_path_offset(fdt, "/chosen");
	if (nodeoffset < 0) {
		errorf("print bootargs: cann't find chosen");
		return -1;
	}

	path = fdt_getprop(fdt, nodeoffset, "bootargs", NULL);
	if (path == NULL) {
		errorf("print bootargs: bootargs is null\n");
		return -1;
	}

	dprintf(CRITICAL, "final bootargs: %s\n", path);

	return 0;
}

#ifdef CONFIG_CREATE_KASLR_SEED
int fdt_fixup_kaslr_seed(void *fdt)
{
	int nodeoffset;
	int err,i;
	const char *path;
	uchar kaslr_seed_array[8] __attribute__((aligned(4096))) = {0};
	u64 kaslr_seed_mask;
	u64 *kaslr_seed = &kaslr_seed_array;

	/* Get 64-bit kaslr_seed */
	err = uboot_get_tos_random((unsigned long)kaslr_seed_array, 8);
	if(err < 0)
	{
		errorf("ERROR:failed to generate tos random seed\n");
		return err;
	}

	/* Find the "chosen" node. */
	nodeoffset = fdt_path_offset(fdt, "/chosen");

	/* If there is no "chosen" node in the blob return */
	if (nodeoffset < 0) {
		errorf("ERROR: device tree must have /chosen node %s.\n", fdt_strerror(nodeoffset));
		return nodeoffset;
	}

	/* set kaslr-seed value*/
	fdt_delprop(fdt, nodeoffset, "kaslr-seed");
	err = fdt_setprop_u64(fdt, nodeoffset, "kaslr-seed", *kaslr_seed);
	if (err < 0) {
	    errorf("ERROR: cannot set /memory node's kaslr-seed property!\n");
	    return err;
	}
	return 0;
}
#endif

int fdt_fixup_switch_storage_probe(void *fdt)
{
	int err = 0;

#ifdef CONFIG_BLK_DEV_BOOT
	int nodeoffset, vddemmccore_node, ufs_version;
	const char *path;
	char strings[] = "disabled";
	if (get_bootdevice() == BOOT_DEVICE_EMMC) {
		nodeoffset = fdt_path_offset(fdt, CONFIG_UFS_DTS_PATH);
		if (nodeoffset < 0) {
			errorf("cann't find %s\n", CONFIG_UFS_DTS_PATH);
			return -1;
		}
	} else if (get_bootdevice() == BOOT_DEVICE_UFS) {
		nodeoffset = fdt_path_offset(fdt, CONFIG_EMMC_DTS_PATH);
		if (nodeoffset < 0) {
			errorf("cann't find %s\n", CONFIG_EMMC_DTS_PATH);
			return -1;
		}

		fdt_delprop(fdt, nodeoffset, "vmmc-supply");

		ufs_version = sprd_check_ufs_version();
		if (ufs_version == 0x310 || ufs_version == 0x300) {
			vddemmccore_node = fdt_node_offset_by_prop_value(fdt, -1, "regulator-name", "vddemmccore", 12);
			if (vddemmccore_node == -FDT_ERR_NOTFOUND) {
				errorf("vddemmccore_node is not found!\n");
				return -1;
			}
			err = fdt_appendprop(fdt, vddemmccore_node, "regulator-always-on", NULL, 0);
			if (err < 0) {
				errorf("vddemcmccore_node failed to set prop!.\n");
				return err;
			}
		}

		goto end;
	} else {
		return -1;
	}

	err = fdt_setprop(fdt, nodeoffset, "status", strings, strlen(strings) + 1);
	if (err < 0)
		errorf("could not set bootargs %s.\n", fdt_strerror(err));
end:
#endif
	return err;
}

struct dt_entry_t * fdt_get_entry_ptr_by_table(struct dt_table_t *table)
{
	uint32_t i = 0;
	struct dt_entry_t *tmp_entry;
	struct dt_entry_t *latest_entry = NULL;

	tmp_entry = (struct dt_entry_t *)((char *)table + SPRD_DT_HEADER_SIZE);

	debugf("magic=%x,ver=%d,num=%d\n", table->magic, table->version, table->num_of_entries);

	for(i = 0; i < table->num_of_entries; i++)
	{
		debugf("dt_platform_id=%d,v=%x,rev=%x\n",
				tmp_entry->dt_platform_id,
				tmp_entry->dt_hardware_id,
				tmp_entry->dt_soc_rev);

		if((tmp_entry->dt_platform_id == DT_PLATFORM_ID) &&
		   (tmp_entry->dt_hardware_id == DT_HARDWARE_ID) &&
		   (tmp_entry->dt_soc_rev == DT_SOC_VER))
			{
				return tmp_entry;
			}

		if((tmp_entry->dt_platform_id == DT_PLATFORM_ID) &&
		  (tmp_entry->dt_hardware_id == DT_HARDWARE_ID) &&
		  (tmp_entry->dt_soc_rev <= DT_SOC_VER)) {
			latest_entry = tmp_entry;
		}
		tmp_entry++;
	}

	if (latest_entry) {
		debugf( "Loading DTB with SOC version:%x\n", latest_entry->dt_soc_rev);
		return latest_entry;
	}

	return NULL;
}

__WEAK void fdt_fixup_boardid_hwlevel(void *fdt) {}
#ifdef CONFIG_EMMC_WP
extern int powp_verify_and_set_flag(void);
extern int is_sprd_enter_cali;
int fdt_fixup_protect_part(void *fdt)
{
#ifndef CONFIG_NAND_BOOT
	int nodeoffset;
	int err;
	uint64_t part_addr_size = 0;
	uint64_t protect_size_addr=0,protect_size=0;
	protect_size_addr=0x2400000000000;//(0x24000<<32);0x24000=72M
	protect_size=0x4000;
	part_addr_size=protect_size_addr+protect_size;
	//part_addr_size=(uint64_t)(0x24000<<32)+(uint64_t)(0x4000); //((72*1024*1024/512)<<31)+(8*1024*1024/512) =protect_size_addr+protect_size  

	debugf("part_addr_size =0x%llx   protect_size_addr=0x%llx    protect_size=0x%llx   \n", part_addr_size,protect_size_addr,protect_size);

	nodeoffset = fdt_node_offset_by_prop_value(fdt, 0, "sprd,name", "sdio_emmc", 10);
	if (nodeoffset == -FDT_ERR_NOTFOUND) {
		errorf("prop sprd,name not found,part protect is impossible\n");
		return -1;
	}
	debugf("find node %s successfully\n", fdt_get_name(fdt, nodeoffset, NULL));
	/* set protect part's address and size */
	fdt_delprop(fdt, nodeoffset, "sprd,part-address-size");
	err = fdt_setprop_u64(fdt, nodeoffset, "sprd,part-address-size", part_addr_size);
	if (err < 0) {
		errorf("set property sprd,part-address-size error!\n");
		return err;
	}

/* set protect part's switch, open pwp */
	fdt_delprop(fdt, nodeoffset, "sprd,wp_fn");
	if(powp_verify_and_set_flag()&&(is_sprd_enter_cali==0))
	{
		
		err = fdt_setprop(fdt, nodeoffset, "sprd,wp_fn", "enable", strlen("enable") + 1);
	}
	else
	{

		err = fdt_setprop(fdt, nodeoffset, "sprd,wp_fn", "disable", strlen("disable") + 1);
	}

	if (err < 0) {
		errorf("set property sprd,wp_fn error!\n");
		return err;
	}
	debugf("property sprd is_sprd_enter_cali=%d,wp_fn = %s\n",is_sprd_enter_cali,
					fdt_getprop(fdt, nodeoffset, "sprd,wp_fn", strlen("disable") +	1));
#endif
	return 0;

}

#endif
