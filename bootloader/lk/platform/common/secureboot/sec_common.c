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

#include <linux/types.h>
#include <secureboot/sprdsec_header.h>
#include <secureboot/sec_string.h>
#include <secure_verify.h>
#include <asm/arch/sprd_reg.h>
#include <secureboot/sec_common.h>
#include <boot_parse.h>
#ifdef SPRD_VBOOT_V2
#include <libavb.h>
#include <uboot_avb_ops.h>
#endif
#include "sprd_common_rw.h"
#include "boot_parse.h"
#include <chipram_env.h>
#include <sprd_trustzone.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <secureboot/sprd_verify.h>
#include <lk/debug.h>
#include <android_ab.h>
#ifdef CONFIG_ANDROID_AB
#include <boot_mode.h>
#include <asm/arch/check_reboot.h>
extern char g_env_slot[3];
#endif

#define EFUSE_HASH_STARTID 2
#ifdef KCE_ENCRYPT_FLAG
volatile uint32_t g_start_merge_dtbo_flag = 0;
uint32_t g_read_dtbo_offset = 0;
extern uint64_t ddr_dtbo_img_addr;
#endif
extern uint64_t ddr_boot_img_addr;
extern const char* g_env_bootmode;
extern uint64_t vendorboot_ddr_img_addr;
extern int sprd_sec_verify_lockstatus(unsigned char *lockstatus, unsigned int status_len);
extern void lcd_printf(const char *fmt, ...);
extern int get_buffer_base_size_from_dt(const char *name, unsigned long *basep, unsigned long *sizep);

#ifdef TOS_TRUSTY
imgToVerifyInfo *img_verify_info_tos;
VbootVerifyInfo *vboot_verify_info;
VbootUnlockVerifyInfo *vboot_unlock_verify_info;
#else
VbootVerifyInfo vboot_verify_info_s = {0};
VbootVerifyInfo *vboot_verify_info = &vboot_verify_info_s;
VbootUnlockVerifyInfo vboot_unlock_verify_info;
#endif
imgToVerifyInfo img_verify_info = {0};
uint8_t pubkhash[32];
VbootVerInfo vboot_ver_info __attribute__((aligned(4096))); /*must be PAGE ALIGNED*/
unsigned char vboot_para_partition_name[SEC_MAX_PARTITION_NAME_LEN] __attribute__((aligned(4096))); /*must be PAGE ALIGNED*/
static uint32_t s_boot_patch_level = 0;

#ifdef CONFIG_VBOOT_SYSTEMASROOT
unsigned char g_vboot_sys_cmdline[SPRD_VBOOT_SYSTEM_CMDLINE_MAXSIZE];
#endif
extern AvbSlotVerifyData* avb_slot_data[2];

static uchar *const s_force_secure_check[] = {
    "splloader",
	"sml",
	"trustos",
	"teecfg",
#ifdef CONFIG_VERIFY_GPT
	"gpt",
	"partition",
#endif
#ifdef CONFIG_X86
	"mobilevisor",
	"mvconfig",
	"secvm",
#endif
	"uboot",
#ifdef SPRD_VBOOT_V2
	"vbmeta_system",
	"vbmeta_system_ext",
	"vbmeta_vendor",
	"vbmeta_product",
	"vbmeta_odm",
	"vbmeta",
#endif
	"boot",
#ifdef SPRD_K515_FLAG
	"init_boot",
#endif
	"vendor_boot",
	"recovery",
	"wl_modem",
	"wl_ldsp",
	"wmodem",
	"wl_gdsp",
	"wl_warm",
	"pm_sys",
	"tl_ldsp",
	"tl_tgdsp",
	"tl_modem",
#ifndef NOT_VERIFY_MODEM
	"l_modem",
	"l_ldsp",
	"l_gdsp",
#ifdef SHARKL5_CDSP
	"l_cdsp",
#endif
	"nr_modem",
	"nr_phy",
#endif
	"l_warm",
	"l_tgdsp",
	"l_agdsp",
	"wdsp",
	"w_modem",
	"w_gdsp",
	"dtb",
	"dtbo",
#if defined(VERIFY_WCN_GNSS)
	"gnssmodem",
	"wcnmodem",
#else
	#ifdef CONFIG_SUPPORT_WIFI
	"wcnmodem",
	#endif
#endif
#if defined(VERIFY_NV_LTE)
	"l_fixnv1",
#endif
	#ifdef CONFIG_SYSTEM_VERIFY
	"system",
	#endif
	NULL
};

extern enVerifiedState g_verifiedbootstate;
static void dl_secure_verify(void *partition_name,void *tocheck_partition_name,void *header,void *code)
{
#ifdef SPRD_SECBOOT
#ifdef SPRD_VBOOT_V2
	if (tocheck_partition_name != NULL && strlen(tocheck_partition_name)) {
		sprd_dl_vboot_verify(partition_name, tocheck_partition_name, header, code);
	} else {
		sprd_dl_verify(partition_name, header, code);
	}
#else
	sprd_dl_verify(partition_name, header, code);
#endif
#endif
}

static void fb_secure_verify(void *partition_name,void *tocheck_partition_name,void *header,void *code)
{
#ifdef SPRD_SECBOOT
#ifdef SPRD_VBOOT_V2
	if (tocheck_partition_name != NULL && strlen(tocheck_partition_name)) {
		sprd_fb_secure_vboot_verify(partition_name, tocheck_partition_name, header, code);
	} else {
		sprd_fb_secure_verify(partition_name, header, code);
	}
#else
	sprd_fb_secure_verify(partition_name, header, code);
#endif
#endif
}

char * my_strrstr(char const *s1, char const *s2)
{
	char *last = NULL;
	char *current = NULL;

	if(*s2 != '\0')
	{
		current = strstr(s1, s2);
		while(current != NULL){
			last = current;
			current = strstr(last + 1, s2);
		}
	}
	return last;
}

static int _check_secure_part(uchar * partition_name)
{
	int i = 0;
	do {
		if (SEC_TRUE == strcmp(s_force_secure_check[i], partition_name))
			return SEC_TRUE;
		i++;
	}
	while (s_force_secure_check[i] != 0);

	return SEC_FALSE;
}

uint32_t reversebytes_uint32t(uint32_t value){
    return (value & 0x000000FFU) << 24 | (value & 0x0000FF00U) <<  8 |
           (value & 0x00FF0000U) >> 8  | (value & 0xFF000000U) >> 24;
}

uint64_t reversebytes_uint64t(uint64_t value){
    uint64_t low_uint64;
    uint64_t high_uint64 = (uint64_t)reversebytes_uint32t((uint32_t)value);
    low_uint64 = (uint64_t)reversebytes_uint32t((uint32_t)(value >> 32));
    return (high_uint64 << 32) + low_uint64;
}

void secure_avb_verify_image(VbootVerifyInfo* start_addr, uint32_t lenth, secboot_display_info type_flag) {

	VbootVerifyInfo *verify_info = NULL;
	verify_info = (VbootVerifyInfo *)start_addr;

	uboot_vboot_verify_img((unsigned long)start_addr, lenth);

	if ((type_flag != SECBOOT_DISPLAY_ENABLEED) &&
			(verify_info->verify_return_data->vboot_verify_ret != AVB_SLOT_VERIFY_RESULT_OK)) {
		dprintf(INFO,"warning: secure_avb_verify_image() ret=%u\n", verify_info->verify_return_data->vboot_verify_ret);
		dprintf(INFO,"warning: secure_avb_verify_image() verify image name=%s\n",(uint32_t)vboot_verify_info->img_name);
		panic("(%s) secure avb verify failed!\n", __func__);
	}
	return;
}

int secure_load_partition(char *partition, uint64_t size, uint64_t offset, char *ram_addr)
{
    sys_img_header __aligned(ARCH_DMA_MINALIGN) img_header;
    int               img_size = 0;
    uint64_t          head_off = 0;
    uint64_t          head_len = sizeof(sys_img_header);
    AvbFooter    *footer = NULL;
    char partition_name[128] = {0};
    strcpy(partition_name, partition);
#ifdef CONFIG_ANDROID_AB
    char *ab_slot = g_env_slot;
    if (get_boot_role() == BOOTLOADER_MODE_LOAD && ab_slot)
        sprintf(partition_name, "%s%s", partition, ab_slot);
#endif

    if ((0 == strncmp(partition_name, "boot", 4))||(0 == strncmp(partition_name, "dtbo", 4)) ||
	(0 == strncmp(partition_name, "recovery", 8))){
#if (!defined SPRD_VBOOT_V2)
        if (0 != common_raw_read(partition_name, SYS_HEADER_SIZE, 0, (char*)&img_header))
            goto error;
        img_size = img_header.mImgSize;
        if (0 != common_raw_read(partition_name, img_size+CERT_SIZE, 0, ram_addr))
            goto error;
#else
    if(0 == common_raw_read(partition_name, AVB_FOOTER_SIZE,  size-AVB_FOOTER_SIZE, ram_addr+size-AVB_FOOTER_SIZE)) {
        footer = ram_addr+size-AVB_FOOTER_SIZE;
        if (0 == memcmp(footer->magic, AVB_FOOTER_MAGIC, AVB_FOOTER_MAGIC_LEN)) {
            if(0 != common_raw_read(partition_name, reversebytes_uint64t(footer->vbmeta_offset) +
                reversebytes_uint64t(footer->vbmeta_size), 0, ram_addr)) {
                goto error;
            }
        } else {
            if (0 != common_raw_read(partition_name,size, offset, (u8*)ram_addr))
                goto error;
        }
    }else {
        if (0 != common_raw_read(partition_name,size, offset, (u8*)ram_addr))
            goto error;
    }
#endif
    }
#if (defined SPRD_VBOOT_V2)
#ifdef CONFIG_ANDROID_AB
    else if ((strlen(partition_name) <= 8) && (0 == strncmp(partition_name, "vbmeta", 6))) {
        // read vbmeta header in footer
        head_off = VBMETA_PARTITION_MAX_SIZE - SYS_HEADER_SIZE;
        if (0 != common_raw_read(partition_name, head_len, head_off, (u8*)&img_header)) {
            goto error;
        }
        img_size = img_header.mImgSize;
        // read vbmeta payload
        if (0 != common_raw_read(partition_name, size, 0, (u8*)ram_addr)) goto error;
        // integrity check for vbmeta
        if (check_sprdimgheader((uint8_t *)&img_header, ram_addr) != 0) {

            AvbVBMetaImageHeader* hdr = (AvbVBMetaImageHeader*) ram_addr;

            if (hdr->flags != 0) {
                hdr->flags = 0;
                if (check_sprdimgheader((uint8_t *)&img_header, ram_addr) != 0) {
                    // check sprd image header magic failed
                    errorf("check vbmeta sprd sign image header magic number error!\n");
                    goto error;
                }
            }
        }
    }
#else
    else if (0 == strcmp(partition_name, "vbmeta")) {
        // read vbmeta header in footer
        head_off = VBMETA_PARTITION_MAX_SIZE - SYS_HEADER_SIZE;
        if (0 != common_raw_read("vbmeta", head_len, head_off, (u8*)&img_header)) {
            goto error;
        }
        img_size = img_header.mImgSize;
        // read vbmeta payload
        if (0 != common_raw_read("vbmeta", size, 0, (u8*)ram_addr)) goto error;
        // integrity check for vbmeta
        if (check_sprdimgheader((uint8_t *)&img_header, ram_addr) != 0) {
            // check failed, try to read vbmeta from vbmeta_bak
            // read vbmeta header in footer
            if (0 != common_raw_read("vbmeta_bak", head_len, head_off, (u8*)&img_header)) {
                goto error;
            }
            img_size = img_header.mImgSize;
            // read vbmeta payload
            if (0 != common_raw_read("vbmeta_bak", size, 0, (u8*)ram_addr)) goto error;
            // integrity check for vbmeta_bak
            if (check_sprdimgheader((uint8_t *)&img_header, ram_addr) != 0) {
                panic("error:check dual backup hash failed for vbmeta.img.\n");
            }
        }
    }
#endif
#endif
    else if (0 != common_raw_read(partition_name,size, offset, (u8*)ram_addr))
            goto error;
    return img_size;
error:
    errorf("read %s partition fail\n", partition_name);
    return -1;
}

static void secboot_get_pubkhash(uint64_t img_buf,uint64_t imginfo)
{
	sprd_get_hash_key((uint8_t *)img_buf,(uint8_t *)imginfo);//be carefull! the first param in chipram is not same as uboot64
}

void  secboot_param_set(uint64_t load_buf,imgToVerifyInfo *imginfo)
{
	imginfo->img_addr = load_buf;
	imginfo->img_len = SIMG_BUF;
	imginfo->hash_len = 32;
	imginfo->pubkeyhash = pubkhash;
	imginfo->flag = SPRD_FLAG;
	imginfo->img_name = 0;
	imginfo->img_name_len = 0;
}

int secure_efuse_program(void)
{
	int ret = 0;

#if defined(SPRD_SECBOOT)
			//ret = secure_efuse_program_sprd(); /*wait for efuse driver ready*/
#endif
	return ret;
}

VERIFY_RESULT dl_secure_process_flow(ulong * strip, unsigned char * part_name,
			uint32_t rcv_size, uint32_t total_size, unsigned char * buf)
{
	static uint32_t rcv_already = 0;
	int ret = 0;

	set_current_dl_partition(part_name);
#ifdef CONFIG_ANDROID_AB
	char partition_name[128] = {0};
	char *temp_part[32] = {0};
	char *last = NULL;
	char *last_a = NULL;
	char *last_b = NULL;

	strcpy(partition_name, part_name);

	last_a = my_strrstr(partition_name, "_a");
	last_b = my_strrstr(partition_name, "_b");
	last = (last_a == NULL)?last_b:last_a;
	debugf("last_a = %s, last_b = %s, last = %s\n",last_a, last_b, last);

	if(last){
		strncpy(temp_part, partition_name, last - partition_name);
		part_name = temp_part;
	}
	debugf("part_name = %s, temp_part = %s, partition_name = %s\n",part_name, temp_part, partition_name);
#endif

	if (0 == memcmp(part_name, BOOT_PART, strlen(part_name))
		||(0 == memcmp(part_name, VENDOR_BOOT_PART, strlen(part_name)))
		||(0 == memcmp(part_name, NV_LTE_PART, strlen(part_name)))) {
		return VERIFY_NO_NEED;
	}

	if (SEC_FALSE == _check_secure_part(part_name))	{
	    debugf("%s partition isn't secure image need to verify \n",part_name);
		return VERIFY_NO_NEED;
	}

	if (SEC_TRUE == strcmp("uboot", part_name)) {
		dl_secure_verify("splloader", "", buf, 0);
	} else if (SEC_TRUE == strcmp("splloader", part_name)) {
		dl_secure_verify("splloader0", "", buf, 0);
	}
#ifdef SPRD_SECBOOT
	else if ((SEC_TRUE == strcmp("sml",part_name))
				||(SEC_TRUE == strcmp("teecfg",part_name))
				||(SEC_TRUE == strcmp("mobilevisor",part_name))
				||(SEC_TRUE == strcmp("mvconfig",part_name))
				||(SEC_TRUE == strcmp("gpt",part_name))
				||(SEC_TRUE == strcmp("partition",part_name))
				||(SEC_TRUE == strcmp("secvm",part_name))){
		dl_secure_verify("fdl1","",buf,0);
	} else if (SEC_TRUE == strcmp("trustos",part_name)) {
		debugf("trustos download.\n");
		dl_secure_verify("fdl1", "", buf, "trustos");
	} else if ((0 == strcmp("gnssmodem", part_name)) || (0 == strcmp("wcnmodem", part_name))) {
		sprd_fdl2_dl_verify(part_name, buf, 0);
	} 
#endif
	else {
#ifdef SPRD_VBOOT_V2
		if((0 == strcmp("splloader", part_name)) ||\
			(0 == strcmp("uboot", part_name)) ||\
			(0 == strcmp("sml", part_name)) ||\
			(0 == strcmp("teecfg", part_name))) {
			dl_secure_verify("fdl2","",buf,0);
		} else if (0 == strcmp("trustos", part_name)) {
			debugf("trustos download.\n");
			dl_secure_verify("fdl2", "", buf, "trustos");
		} else {
			debugf("download vboot check images. \n");
			dl_secure_verify("fdl2",part_name,buf,0);
		}
#else
		dl_secure_verify("fdl2","",buf,0);
#endif
	}
	debugf("%s_verify_successful\n",part_name);
	return VERIFY_OK;
}

VERIFY_RESULT fb_secure_process_flow(ulong * strip, unsigned char * part_name,
			uint32_t rcv_size, uint32_t total_size, unsigned char * buf)
{
	static uint32_t rcv_already = 0;
	int ret = 0;
	uint32_t rpmb_version = 0;
	int ab_slot_flag = 0;

#ifdef CONFIG_ANDROID_AB
	char partition_name[128] = {0};
	char *temp_part[32] = {0};
	char *last = NULL;
	char *last_a = NULL;
	char *last_b = NULL;
	char *slot = g_env_slot;

	strcpy(partition_name, part_name);

	last_a = my_strrstr(partition_name, "_a");
	last_b = my_strrstr(partition_name, "_b");
	last = (last_a == NULL)?last_b:last_a;
	debugf("last_a = %s, last_b = %s, last = %s\n",last_a, last_b, last);

	if(last){
		strncpy(temp_part, partition_name, last - partition_name);
		part_name = temp_part;
	}
	debugf("part_name = %s, temp_part = %s, partition_name = %s\n",part_name, temp_part, partition_name);
#endif

    if ((0 == memcmp(part_name, BOOT_PART, strlen(part_name)))
            ||(0 == memcmp(part_name, VENDOR_BOOT_PART, strlen(part_name)))) {
//            &&(get_lock_status() == VBOOT_STATUS_UNLOCK)){
           return VERIFY_NO_NEED;
    }

	if (SEC_FALSE == _check_secure_part(part_name)){
		debugf("%s not secure image need to verify \n",part_name);
		return VERIFY_NO_NEED;
		}
#ifdef CONFIG_NONTRUSTY_ANTIROLLBACK
		ret = sprd_get_imgversion(0, &rpmb_version);
		if(!ret) {
			debug("read rpmb version: %d,ret = %d\n", rpmb_version,ret);
		} else {
			debug(">>>>> Warning:read rollback version return error! ret = %d\n",ret);
			rpmb_version = 0;
		}
		debug("Transfer non_trusty rollback_version here! rpmb_version = %d\n",rpmb_version);
		uboot_vboot_set_ver(rpmb_version, sizeof(rpmb_version));
#endif

	if (SEC_TRUE == strcmp("uboot", part_name)) {
		fb_secure_verify("splloader", "", buf, 0);
	} else if (SEC_TRUE == strcmp("splloader", part_name)) {
		fb_secure_verify("splloader0", "", buf, 0);
	}
#ifdef SPRD_SECBOOT
	else if ((SEC_TRUE == strcmp("sml",part_name))
				||(SEC_TRUE == strcmp("teecfg",part_name))
				||(SEC_TRUE == strcmp("mobilevisor",part_name))
				||(SEC_TRUE == strcmp("l_fixnv1", part_name))
				||(SEC_TRUE == strcmp("mvconfig",part_name))
				||(SEC_TRUE == strcmp("gpt",part_name))
				||(SEC_TRUE == strcmp("partition",part_name))
				||(SEC_TRUE == strcmp("secvm",part_name))) {
		fb_secure_verify("splloader","",buf,0);
	} else if(SEC_TRUE == strcmp("trustos",part_name)) {
		debugf("trustos download.\n");
		fb_secure_verify("splloader", "", buf, "trustos");
	} else if ((0 == strcmp("gnssmodem", part_name)) || (0 == strcmp("wcnmodem", part_name))) {
			//read version from rpmb block
			memset(&vboot_ver_info, 0, sizeof(VbootVerInfo));
	#ifdef CONFIG_ANDROID_AB
			ab_slot_flag = (int)(*(slot + 1)) - 'a';
	#endif
			vboot_ver_info.ab_slot_flag = (uint32_t)ab_slot_flag;
			sprd_get_all_imgversion(&vboot_ver_info);
	#ifdef CONFIG_VBOOT_DUMP
			debugf("wcn-gnss dump vboot_ver_info.img_ver \n");
			do_hex_dump((void *)&vboot_ver_info, sizeof(VbootVerInfo));
	#endif
			//pass the version msg to tos
			uboot_vboot_set_ver((VbootVerInfo*)(&vboot_ver_info),sizeof(VbootVerInfo));
			fb_secure_verify(part_name,"",buf,0);
	}
#endif
	else {
#ifdef SPRD_VBOOT_V2
		if((0 == strcmp("splloader", part_name)) ||\
			(0 == strcmp("uboot", part_name)) ||\
			(0 == strcmp("sml", part_name)) ||\
			(0 == strcmp("teecfg", part_name))) {
			fb_secure_verify("uboot","",buf,0);
		} else if(SEC_TRUE == strcmp("trustos",part_name)) {
			debugf("trustos download.\n");
			fb_secure_verify("uboot", "", buf, "trustos");
		} else {
			debugf("fastboot download vboot check images. \n");
			fb_secure_verify("uboot",part_name,buf,0);
		}
#else
		fb_secure_verify("uboot","",buf,0);
#endif
	}

	debugf("%s_verify_successful\n",part_name);
	return VERIFY_OK;
}

int get_secboot_base_from_dt(void)
{
#if 0
	int str_len = 0, offset;
	unsigned long base, size;
	char nodename[256];
	char *pStr = NULL;
	const void *fdt_blob = gd->fdt_blob;
	int parentoffset = fdt_path_offset(fdt_blob, "/reserved-memory");

	if (parentoffset < 0) {
		debugf("fdt_reserved_mem_multimedia_parse: cann't find mm_reserved \n");
		return -1;
	}

	for (offset = fdt_first_subnode(fdt_blob, parentoffset);
		 offset >= 0; offset = fdt_next_subnode(fdt_blob, offset)) {
		sprintf(nodename, "%s", fdt_get_name(fdt_blob, offset, NULL));
		str_len = strlen(nodename);
		nodename[str_len] = '\0';

		pStr = strstr(nodename,"secboot-arg-mem");
		if (pStr) {
			if (fdtdec_decode_region(fdt_blob, offset, "reg", &base, &size) < 0) {
				debugf("cannot find reg prop, go to next node.\n" );
				goto next;
			}
			ARG_START_BASE = base;
		}

		pStr = strstr(nodename,"secboot-vbmeta-mem");
		if (pStr) {
			if (fdtdec_decode_region(fdt_blob, offset, "reg", &base, &size) < 0) {
				debugf("cannot find reg prop, go to next node.\n" );
				goto next;
			}
			VBMETA_IMG_BASE = base;
		}

		pStr = strstr(nodename,"secboot-verify-mem");
		if (pStr) {
			if (fdtdec_decode_region(fdt_blob, offset, "reg", &base, &size) < 0) {
				debugf("cannot find reg prop, go to next node.\n" );
				goto next;
			}
			VERIFY_BASE = base;
		}

next:;
	}
#endif
	debugf("ARG_START_BASE: 0x%08x, VBMETA_IMG_BASE: 0x%08x, VERIFY_BASE: 0x%08x\n",
					ARG_START_BASE, VBMETA_IMG_BASE, VERIFY_BASE);

	if (!ARG_START_BASE || !VBMETA_IMG_BASE || !VERIFY_BASE) {
		debugf("secboot base address error\n");
		return -1;
	}

#ifdef TOS_TRUSTY
	img_verify_info_tos = (imgToVerifyInfo *)ARG_START_BASE;
	vboot_verify_info = (VbootVerifyInfo *)ARG_START_BASE;
	vboot_unlock_verify_info = (VbootUnlockVerifyInfo *)ARG_START_BASE;
#endif

	return 0;
}


void secboot_init(char *partition_name)
{
	int ret = 0;
	uint32_t rpmb_version = 0;
	VERIFY_PROCESS_FLAG process_flag = VERIFY_PROCESS_BOOT;

	ret = get_secboot_base_from_dt();
	if (ret) {
		debugf("Getting secure boot base failed\n");
		while(ret);
	}

#ifdef SPRD_VBOOT_V2
	/*malloc one memory to vbmeta image buffer use. ok? 2KB*/
//	debugf("VERIFY_BASE:0x%x, KERNEL_ADR:0x%x. \n", VERIFY_BASE, KERNEL_ADR);
	vboot_secure_process_prepare();
	vboot_secure_process_init(partition_name, VERIFY_BASE, VBMETA_IMG_BASE, NULL, process_flag);
#endif
#ifdef SPRD_SECBOOT
#ifdef CONFIG_NONTRUSTY_ANTIROLLBACK
	ret = sprd_get_imgversion(0, &rpmb_version);
	if(!ret) {
		debug("read rpmb version: %d,ret = %d\n", rpmb_version,ret);
	} else {
		debug(">>>>> Warning:read rollback version return error! ret = %d\n",ret);
		rpmb_version = 0;
	}
	debug("Transfer non_trusty rollback_version here! rpmb_version = %d\n",rpmb_version);
	uboot_vboot_set_ver(rpmb_version, sizeof(rpmb_version));
#endif
#endif
}

void secboot_terminal(void)
{
	#if defined (SPRD_SECBOOT) || defined (SPRD_VBOOT_V2)
	secboot_param_set(VERIFY_BASE,&img_verify_info);
	sprd_calc_hbk();
	#endif
}

int write_rotpk_in_uboot(void)
{
	int ret = 1;

	dprintf(INFO,"load spl in fastboot mode\n");
	ret = reload_spl_to_tos();

	if(0 == ret)
		dprintf(INFO,"write rotpk successful\n");
	else
		errorf("error:write rotpk failed!\n");

	return ret;
}

#ifdef SPRD_VBOOT_V2
static void vboot_replace_vbmeta_size_and_digest(void) {
	AvbSlotVerifyResult ret = AVB_SLOT_VERIFY_RESULT_OK;
	avb_ops_new();
	unsigned char vboot_cmdline_temp[SPRD_VBOOT_CMDLINE_MAXSIZE] = {0};
	ret = (AvbSlotVerifyResult)avb_check_image(NULL);
	debugf("avb_check_image() ret is %d.\n", ret);

	if (avb_slot_data[0]) {
		/*replace vbmeta_size digest and veritymode*/
		debugf("avb_slot_data[0]->cmdline is %s.\n", avb_slot_data[0]->cmdline);
		dprintf(INFO,"g_sprd_vboot_cmdline is %s.\n", g_sprd_vboot_cmdline);

		size_t str_global_len = strlen(strstr(g_sprd_vboot_cmdline, "androidboot.vbmeta.size"));
		size_t str_replace_len = strlen(strstr(avb_slot_data[0]->cmdline, "androidboot.vbmeta.size"));
		size_t global_len = strlen(g_sprd_vboot_cmdline);

		strncpy((char *)vboot_cmdline_temp, (char *)g_sprd_vboot_cmdline,
				global_len - str_global_len);
		strncpy((char *)vboot_cmdline_temp + (global_len - str_global_len),
				strstr(avb_slot_data[0]->cmdline, "androidboot.vbmeta.size"), str_replace_len);
		strncpy((char *)g_sprd_vboot_cmdline, (char *)vboot_cmdline_temp,
				SPRD_VBOOT_CMDLINE_MAXSIZE);
		g_sprd_vboot_cmdline[SPRD_VBOOT_CMDLINE_MAXSIZE-1] = '\0';
		dprintf(INFO,"g_sprd_vboot_cmdline is %s.\n", g_sprd_vboot_cmdline);

	}
}
#endif

static char* bin2hex(const uint8_t* data, size_t data_len) {
	const char hex_digits[17] = "0123456789abcdef";
	char* hex_data;
	size_t n;

	hex_data = malloc(data_len * 2);
	if (hex_data == NULL) {
		return NULL;
	}

	for (n = 0; n < data_len; n++) {
		hex_data[n * 2] = hex_digits[data[n] >> 4];
		hex_data[n * 2 + 1] = hex_digits[data[n] & 0x0f];
	}

	return hex_data;
}

static void vboot_get_root_of_trust_str(char* root_of_trust, uint8_t* image_verify_result)
{
	char* additional_cmdline = root_of_trust;
	const char hex_digits[17] = "0123456789abcdef";
	char* pubkey = NULL;
	char* verify_result = NULL;
	uint32_t len = 0;

	if (root_of_trust == NULL || image_verify_result == NULL) {
		debugf("root_of_trust NULL.\n");
		return;
	}

	/* Initialize root_of_trust_str */
	memset(root_of_trust_str, 0, ROOT_OF_TRUST_MAXSIZE);

	/* Get vbmeta.pubkey */
	strcpy(additional_cmdline, "androidboot.vbmeta.pubkey");
	additional_cmdline = additional_cmdline + strlen("androidboot.vbmeta.pubkey");
	*additional_cmdline++= '=';
	if ((pubkey = bin2hex(g_sprd_vboot_key, g_sprd_vboot_key_len)) == NULL) {
		debugf("pubkey NULL.\n");
		return;
	}
	memcpy(additional_cmdline, (char *)pubkey, g_sprd_vboot_key_len*2);
	free(pubkey);
	additional_cmdline = additional_cmdline + g_sprd_vboot_key_len*2;
	*additional_cmdline++= ' ';

	/* Get verify.result */
	strcpy(additional_cmdline, "androidboot.verify.result");
	additional_cmdline = additional_cmdline + strlen("androidboot.verify.result");
	*additional_cmdline++= '=';
	len = sizeof(uint32_t);
	if ((verify_result = bin2hex((const int8_t*)image_verify_result, len)) == NULL) {
		debugf("verify_result NULL.\n");
		return;
	}
	memcpy(additional_cmdline, verify_result, len*2);
	free(verify_result);
	additional_cmdline = additional_cmdline + len*2;
	*additional_cmdline++= ' ';

	if (additional_cmdline - root_of_trust + strlen(g_sprd_vboot_cmdline)
			> ROOT_OF_TRUST_MAXSIZE) {
		debugf("root_of_trust overflow. \n");
		return;
	}

	/* Get command_line */
	memcpy(additional_cmdline, g_sprd_vboot_cmdline, strlen(g_sprd_vboot_cmdline));
}

static bool patch_level_to_uint32(const char *prop, size_t prop_size, uint32_t *out_value) {

	int		j, k, n;
	int		base = 10; //means Decimal conversion
	char		temp_data[12] = ""; //12 means max len of "2021-**-**"
	uint32_t	parsed_val = 0;

	if ((prop == NULL) || (out_value == NULL)) {
		debug("patch_level_to_uint32:Input parameter error.\n");
		return false;
	}

	for(j = k = 0; j <= prop_size; j++) {
		if (prop[j] != 0x2d) {		//"-" = 0x2d
			temp_data[k++] = prop[j];
		}
	}
	for (n = 0; temp_data[n] != '\0'; n++) {
		int c = temp_data[n];
		int digit;
		parsed_val *= base;

		if (c >= '0' && c <= '9') {
			digit = c - '0';
		} else {
			debug("Invalid digit.\n");
			return false;
		}
		parsed_val += digit;
	}
	*out_value = parsed_val;

	return true;
}

bool vboot_get_bootov_binding(uint64_t *os_value) {

	bool ret = false;
	uint8_t *vbmeta_addr = NULL;
	size_t key_size = 0;
	uint32_t vbmeta_size = 0;
	const char key_os[] = "com.android.build.boot.os_version";
	char partition_name[32] = {0};

	if (get_slot_ab(partition_name, "boot")) {
		errorf("%s: get partition_name error.\n", __func__);
		ret = false;
		goto out;
	}

	if (!os_value) {
		errorf("%s: invalid: para is null.\n", __func__);
		ret = false;
		goto out;
	}

	vbmeta_addr = get_vbmeta_form_partition((const char *)partition_name, &vbmeta_size);
	if ((vbmeta_addr == NULL) || (vbmeta_size == 0)) {
		errorf("%s: Error loading footer, vbmeta_size: %u.\n", partition_name, vbmeta_size);
		ret = false;
		goto out;
	}

	/*get os version*/
	key_size = strlen(key_os);
	debug("%s: key_os = %s key_size = %ld\n", __func__, key_os, key_size);
	ret = avb_property_lookup_uint64(vbmeta_addr,
					vbmeta_size,
					key_os,
					key_size,
					os_value);
	if (ret == false) {
		debug("%s: get boot os version failed.\n", __func__);
		goto out;
	}

	debug("%s: os_value %llu.\n",__func__, *os_value);

out:
	if (vbmeta_addr) {
		free(vbmeta_addr);
		vbmeta_addr = NULL;
	}

	return ret;
}


static void vboot_get_bootpl_binding(VbootVerifyInfo *vboot_verify_info) {

	const char	*prop = NULL;
	size_t		key_size = 0;
	size_t		out_value_size = 0;
	const char	boot_patch_level[] = "com.android.build.boot.security_patch";

	/*get security patch level of boot image*/
	key_size = strlen(boot_patch_level);
	debug("keyword: = %s key_size = %ld\n", boot_patch_level, key_size);
	prop = avb_property_lookup(vboot_verify_info->vbmeta_img_addr,
				vboot_verify_info->vbmeta_img_len,
				boot_patch_level,
				key_size,
				&out_value_size);
	if (prop == NULL) {
		debug("get boot patch level failed.\n");
		return;
	}
	if (!patch_level_to_uint32(prop, out_value_size, &s_boot_patch_level)) {
		debug("switch boot patch level failed.\n");
		return;
	}
}

void get_boot_patch_level(uint32_t *boot_patch_level) {
	debug("s_boot_patch_level = %u.\n", s_boot_patch_level);
	*boot_patch_level = s_boot_patch_level;
}

static void physical_partitions_secure_proces(void)
{
	char *boot_mode_type_str = g_env_bootmode;

	debug("verify boot check dtbo \n");
	vboot_secure_process_flow("dtbo");
#ifdef KCE_ENCRYPT_FLAG
	if (0 != vboot_kce_get_dtbo_addr()) {
		panic("the vboot_kce_get_dtbo_addr ret is error!\n");
	}
#if WITH_SMP
	g_start_merge_dtbo_flag = 1;
#endif
#endif

#ifdef SPRD_K515_FLAG
	debug("verify boot check init_boot \n");
	vboot_secure_process_flow("init_boot");
#endif

	if (g_DeviceStatus != VBOOT_STATUS_UNLOCK) {
		if (NULL != boot_mode_type_str) {
			if (!strncmp(boot_mode_type_str, "cali", 4)) {
				debugf("skip verify vendor_boot in calibration mode\n");
			} else if (!strncmp(boot_mode_type_str, "autotest", 8)) {
				debugf("skip verify vendor_boot in BBAT mode\n");
			} else {
				vboot_secure_process_flow("vendor_boot");
			}
		} else {
			debugf("bootmode error\n");
			vboot_secure_process_flow("vendor_boot");
		}
	}
}

void dynamic_partitions_secure_proces(void)
{
	vboot_secure_process_flow("vbmeta_system");
	vboot_secure_process_flow("vbmeta_vendor");
#ifdef CONFIG_ANDROID_AB
	vboot_secure_process_flow("vbmeta_system_ext");
	vboot_secure_process_flow("vbmeta_product");
	vboot_secure_process_flow("vbmeta_odm");
#endif
}

void secboot_secure_process_flow(char *partition_name, uint64_t size, uint64_t offset, char *ram_addr)
{
	uint32_t ret = 1;
	static bool system_vendor_product_done = false;
	static bool physical_partitions_done = false;

#ifdef SPRD_VBOOT_V2

	if (!physical_partitions_done) {
		physical_partitions_secure_proces();
		physical_partitions_done = true;
	}
#ifdef CONFIG_DMVERITY_DISABLE
	debug("skip to check system & vendor version.\n");
#else
	//secboot_secure_process_flow may be called several times, but system/vendor,only need to be checked on time
	debugf("begin to check system & vendor & product version.\n");
	if (!system_vendor_product_done && g_DeviceStatus != VBOOT_STATUS_UNLOCK)
	{
		if (0 != memcmp(partition_name, RECOVERY_PART, strlen(RECOVERY_PART)))
		{
			dynamic_partitions_secure_proces();
		} else {
			debugf("skip check system & vendor & product in recovery mode.\n");
		}
		system_vendor_product_done = true;
	}
#endif
#else
	if (-1 == secure_load_partition(partition_name,size,offset,ram_addr)){
		while(ret);
	}

#if defined (TOS_TRUSTY)
	secboot_param_set(VERIFY_BASE,img_verify_info_tos);
	img_verify_info_tos->pubkeyhash = (uint8_t*)ARG_START_BASE+PAGE_ALIGN;
	secboot_get_pubkhash(UBOOT_START,img_verify_info_tos->pubkeyhash);

	dprintf(INFO,"p1:%llx, p2:%llx,P3:%llx,p4:%llx,p5:%llx\n",img_verify_info_tos->img_addr,img_verify_info_tos->img_len,img_verify_info_tos->pubkeyhash,img_verify_info_tos->hash_len,img_verify_info_tos->flag);
	ret = uboot_verify_img((imgToVerifyInfo*)img_verify_info_tos,sizeof(imgToVerifyInfo));
	if (ret)
		panic("(%s) uboot verify img is failed !\n", __func__);
#else
	secboot_param_set(VERIFY_BASE,&img_verify_info);
	secboot_get_pubkhash(UBOOT_START,img_verify_info.pubkeyhash);
	ret = uboot_verify_img(&img_verify_info,sizeof(imgToVerifyInfo));
	if (ret)
		panic("(%s)uboot verify img is failed !\n", __func__);
#endif
#endif

#ifdef SPRD_VBOOT_V2
#ifdef PRODUCT_USE_DYNAMIC_PARTITIONS
	FTL_Savepoint_Private(PHASE_SECURE_AVB);
	vboot_replace_vbmeta_size_and_digest();
	check_dmverity_state();
#endif
#endif

	/* Get root_of_trust_str for KM4.0 */
	vboot_get_root_of_trust_str(root_of_trust_str,
			(uint8_t*)&(vboot_verify_info->verify_return_data->vboot_verify_ret));

	/* Get boot patch level for KM */
	vboot_get_bootpl_binding(vboot_verify_info);

}

#ifdef SPRD_VBOOT_V2
void vboot_secure_process_init(char *partition_name, char *img_ram_addr, char * vbmeta_ram_addr, sys_img_header *header, VERIFY_PROCESS_FLAG flag)
{
	antirb_image_type tmp_type;
	uint64_t partition_size = 0;
	uint8_t * avb_vboot_key = NULL;
	uint32_t avb_vboot_key_len = 0;
	int img_size;
	int ab_slot_flag = 0;

	if(!partition_name){
		debugf("para is invalid\n");
		return;
	}

	memset(vboot_verify_info, 0, sizeof(VbootVerifyInfo));

	/*public op1: when check vbmeta & other imgs(like boot.)*/
	if (-1 == secure_get_partition_size(partition_name, &partition_size)){
		debugf("get %s partition size fail\n", partition_name);
		return;
	}
	if(strcmp("vbmeta",partition_name) == 0){
		/*Info: when check vbmeta, img para should be null.*/
		vboot_verify_info->img_len = 0;
		vboot_verify_info->img_addr = NULL;

		vboot_verify_info->vbmeta_img_addr = vbmeta_ram_addr;

		switch (flag){
		case VERIFY_PROCESS_DOWNLOAD:
			debugf("In download mode\n");
			if (NULL == header){
				debugf("sys_img_header empty\n");
				return;
			}
			vboot_verify_info->vbmeta_img_len = header->mImgSize;
			break;

		case VERIFY_PROCESS_BOOT:
			debugf("In boot mode\n");
			img_size =  secure_load_partition("vbmeta",partition_size,
					(uint64_t)0, vboot_verify_info->vbmeta_img_addr);
			if (-1 == img_size){
				debugf("load vbmeata partition error\n");
				return;
			}
			vboot_verify_info->vbmeta_img_len = (uint32_t)(img_size);
			break;
		default:
			debugf("flag error\n");
			return;
		}

		debugf("partintion:%s, partition_size:0x%llx.\n", "vbmeta", vboot_verify_info->vbmeta_img_len);

		debugf("img_ram_addr:0x%x.   vbmeta_ram_addr:0x%x. \n", img_ram_addr, vbmeta_ram_addr);

		vboot_verify_info->vb_cmdline_addr = NULL;
		vboot_verify_info->vb_cmdline_len = 0;
	}else{
		vboot_verify_info->img_len = (uint32_t)partition_size;
		vboot_verify_info->img_addr = img_ram_addr;

		if (-1 == secure_get_partition_size("vbmeta", &partition_size)) {
			debugf("get vbmeta partition size fail\n");
			return;
		}

		vboot_verify_info->vbmeta_img_addr = vbmeta_ram_addr;

		img_size =  secure_load_partition("vbmeta",partition_size,
					(uint64_t)0, vboot_verify_info->vbmeta_img_addr);
		if (-1 == img_size){
			debugf("load vbmeta partition error\n");
			return;
		}

		vboot_verify_info->vbmeta_img_len = (uint32_t)(img_size);

		debugf("partintion:%s, partition_size:0x%llx\n", partition_name, vboot_verify_info->img_len);

		debugf("partintion:%s, partition_size:0x%llx.\n", "vbmeta", vboot_verify_info->vbmeta_img_len);

		debugf("img_ram_addr:0x%x.   vbmeta_ram_addr:0x%x. \n", img_ram_addr, vbmeta_ram_addr);


		memset((char *)g_sprd_vboot_cmdline, 0, sizeof(g_sprd_vboot_cmdline));
		vboot_verify_info->vb_cmdline_addr = (char *)g_sprd_vboot_cmdline;
		vboot_verify_info->vb_cmdline_len = sizeof(g_sprd_vboot_cmdline);

		memset(g_sprd_vboot_ret, 0, sizeof(g_sprd_vboot_ret));
		vboot_verify_info->verify_return_data = (VBootResultInfo *)g_sprd_vboot_ret;
		vboot_verify_info->verify_return_data_len = sizeof(VBootResultInfo);

		debugf("g_sprd_vboot_cmdline:0x%x.   g_sprd_vboot_cmdline_len:%d.   \n", (char *)g_sprd_vboot_cmdline, sizeof(g_sprd_vboot_cmdline));
		debugf("g_sprd_vboot_ret:0x%x.   g_sprd_vboot_ret_len:%d.   \n", g_sprd_vboot_ret, sizeof(g_sprd_vboot_ret));
	}

	/*public op2: when check vbmeta & other imgs(like boot.)*/
	vboot_verify_info->vbmeta_pubkey_addr = (char *)g_sprd_vboot_key;
	vboot_verify_info->vbmeta_pubkey_len = g_sprd_vboot_key_len;

	debugf("g_sprd_vboot_key:0x%x.   g_sprd_vboot_key_len:%d.   \n", g_sprd_vboot_key, g_sprd_vboot_key_len);

	//read version from rpmb block
	memset(&vboot_ver_info, 0, sizeof(VbootVerInfo));
#ifdef CONFIG_ANDROID_AB
	char *slot = g_env_slot;
	ab_slot_flag = (int)(*(slot + 1)) - 'a';
#endif
	vboot_ver_info.ab_slot_flag = (uint32_t)ab_slot_flag;
	sprd_get_all_imgversion(&vboot_ver_info);
#ifdef CONFIG_VBOOT_DUMP
	debugf("dump vboot_ver_info.img_ver \n");
	do_hex_dump((void *)&vboot_ver_info, sizeof(VbootVerInfo));
#endif
	//pass the version msg to tos
	uboot_vboot_set_ver((VbootVerInfo*)(&vboot_ver_info),sizeof(VbootVerInfo));
}

#ifdef KCE_ENCRYPT_FLAG //just for dtbo encrypt
int vboot_kce_get_dtbo_addr(void)
{
	int ret = 0;
	sprd_aes_header *aes_header = NULL;
	uint8_t aes_header_data[AES_HEADER_SIZE] = {0};
	char partition_name[32] = {"dtbo"};
#ifdef CONFIG_ANDROID_AB
	char *ab_slot = g_env_slot;
	if (ab_slot)
		sprintf(partition_name, "%s%s", "dtbo", ab_slot);
#endif

	ret = common_raw_read(partition_name, (uint64_t)(AES_HEADER_SIZE), 0, (char *)aes_header_data);
	if (ret != 0) {
		errorf("PARTITION %s read header error, ret:%d \n", partition_name, ret);
		return ret;
	}
	aes_header = (sprd_aes_header *)aes_header_data;
	if (aes_header->magic_num == AES_HEADER_MAGIC) {
		g_read_dtbo_offset = AES_HEADER_SIZE;
		ddr_dtbo_img_addr = ddr_dtbo_img_addr + AES_HEADER_SIZE;//for merge dtbo
	}
	return ret;
}
#endif //KCE_ENCRYPT_FLAG

/*
verify the vbmeta struct only, maily for hashtree image like system/vendor to do antirollback in bootloader
*/
void vboot_verify_vbmeta(char *partition_name)
{
	uint64_t     partition_size=0;
	char        *footer_buf;
	AvbFooter    footer;
	int8_t       vb_ret = 0;

	if (!partition_name) {
		debugf("Error: partition_name is null. \n");
		return;
	}
	debugf("partition_name: %s \n", partition_name);
	// step1:read avbfooter
	if (-1 == secure_get_partition_size(partition_name, &partition_size)){
		debugf("secure_get_partition_size error\n");
		goto fail;
	}

	footer_buf = vboot_verify_info->img_addr;
	if (-1 == secure_load_partition(partition_name, AVB_FOOTER_SIZE, partition_size-AVB_FOOTER_SIZE, footer_buf)){
		debugf("load %s partition error.\n", partition_name);
		goto fail;
	}
	// step2:read vbmeta according to the avbfooter
	if (!avb_footer_validate_and_byteswap((const AvbFooter*)footer_buf, &footer)) {
		debugf("Error validating footer.\n");
		goto fail;
	}
	debugf("footer.vbmeta_offset=0x%x,footer.vbmeta_size=0x%x\n",footer.vbmeta_offset,footer.vbmeta_size);
	vboot_verify_info->img_len=footer.vbmeta_size;
#ifdef CONFIG_VBOOT_DUMP
	dumpHex(partition_name,  (uchar *)vboot_verify_info->img_addr, AVB_FOOTER_SIZE);
#endif
	if(-1 == secure_load_partition(partition_name,footer.vbmeta_size, footer.vbmeta_offset,vboot_verify_info->img_addr)) {
		debugf("load %s partition error.\n", partition_name);
		goto fail;
	}
#ifdef CONFIG_VBOOT_DUMP
	dumpHex(partition_name, (uchar *)vboot_verify_info->img_addr, vboot_verify_info->img_len);
#endif
	memset(vboot_para_partition_name, 0, sizeof(vboot_para_partition_name));
	strcpy(vboot_para_partition_name, partition_name);
	vboot_verify_info->img_name = vboot_para_partition_name;
	vboot_verify_info->img_name_len = SEC_MAX_PARTITION_NAME_LEN;
	debugf("vboot_verify_info->img_name :%s. \n", (uint32_t)vboot_verify_info->img_name);
	// step3:send the vbmeta struct and partname to tos to check the rollback version
	secure_avb_verify_image((VbootVerifyInfo*)vboot_verify_info,sizeof(VbootVerifyInfo), SECBOOT_DISPLAY_DISABLED);
#ifdef CONFIG_VBOOT_SYSTEMASROOT
	// step4: parse kernel cmdline from system image
	if(strcmp("system", partition_name) == 0) {
		debugf("parse kernel cmdline start\n");
		avb_ops_new();
		vb_ret = avb_check_image(partition_name);
		debugf("check image ret = %d\n", vb_ret);
		if (vb_ret != 0) goto fail;
		memset(g_vboot_sys_cmdline, 0, sizeof(g_vboot_sys_cmdline));
		strcpy(g_vboot_sys_cmdline, avb_slot_data[0]->cmdline);
		debugf("g_vboot_sys_cmdline: %s\n", g_vboot_sys_cmdline);
	}
#endif
	return;

fail:
	panic("[%s]:verify the vbmeta error\n", __func__);
}

void set_verified_boot_state(char *partition_name, uint32_t vboot_check_result)
{
	if(g_DeviceStatus == VBOOT_STATUS_UNLOCK) {
		g_verifiedbootstate = v_state_orange;
	}
	else if(vboot_check_result == AVB_SLOT_VERIFY_RESULT_OK) {
		g_verifiedbootstate = v_state_green;
	}
	else {
		/*just check verification result for bootimg*/
		dprintf(INFO,"partion %s, verify_ret:0x%x is invalid.\n",partition_name, vboot_check_result);
		g_verifiedbootstate = v_state_red;
		take_action_with_vbootret();
	}
}

void vboot_secure_process_flow_cm4(char *partition_name)
{
	int ret;
	uint32_t vboot_check_result;
	uint64_t partition_size=0;

	if(!partition_name)
	{
		debugf("Error: partition_name is null. \n");
		return;
	}

	ret = secure_get_partition_size(partition_name, &partition_size);
	debugf("ret = %d\n", ret);
	if (ret < 0) {
		debugf("secure_get_partition_size error!\n");
		return;
	}
	vboot_verify_info->img_len=partition_size;

#ifdef CONFIG_SP_DDR_BOOT
	uint64_t temp_addr = 0;
	debugf("cm4 old img_addr:0x%llx.  len:0x%llx. \n", vboot_verify_info->img_addr, vboot_verify_info->img_len);
	temp_addr = vboot_verify_info->img_addr;
	vboot_verify_info->img_addr = DFS_ADDR;
#endif
	debugf("cm4 new img_addr:0x%llx.  len:0x%llx. \n", vboot_verify_info->img_addr, vboot_verify_info->img_len);
	/*partition load*/
	if (-1 == secure_load_partition(partition_name,vboot_verify_info->img_len, (uint64_t)0, vboot_verify_info->img_addr))
	{
		debugf("load %s partition error\n", partition_name);
		return;
	}
#ifndef CONFIG_SP_DDR_BOOT
	memcpy(DFS_ADDR, vboot_verify_info->img_addr, DFS_SIZE);
#endif

	memset(vboot_para_partition_name, 0, sizeof(vboot_para_partition_name));
	strcpy(vboot_para_partition_name, partition_name);
	vboot_verify_info->img_name = vboot_para_partition_name;
	vboot_verify_info->img_name_len = SEC_MAX_PARTITION_NAME_LEN;
	debugf("vboot_verify_info->img_name :%s. \n", (uint32_t)vboot_verify_info->img_name);

	vboot_verify_info->vboot_unlock_status = g_DeviceStatus;

	secure_avb_verify_image((VbootVerifyInfo*)vboot_verify_info, sizeof(VbootVerifyInfo), SECBOOT_DISPLAY_DISABLED);
	debugf("g_sprd_vboot_cmdline len %d.\n", strlen(g_sprd_vboot_cmdline));
	debugf("g_sprd_vboot_cmdline is %s.\n", g_sprd_vboot_cmdline);

#ifdef CONFIG_SP_DDR_BOOT
	vboot_verify_info->img_addr = temp_addr;
#endif

	/*for verify boot*/
	debugf("partion %s, verify_ret:0x%x\n",partition_name, vboot_verify_info->verify_return_data->vboot_verify_ret);
	vboot_check_result = vboot_verify_info->verify_return_data->vboot_verify_ret;

	set_verified_boot_state(partition_name, vboot_check_result);
}

void vboot_secure_process_flow(char *partition_name)
{
	int ret;
	uint32_t vboot_check_result;
	uint64_t partition_size=0;
	uint64_t buf_base = 0;
	uint64_t buf_size = 0;
	uint64_t temp_base = 0;

	if(!partition_name)
	{
		debugf("Error: partition_name is null. \n");
		return;
	}

	if((strcmp("vbmeta_system",partition_name) == 0) ||
			(strcmp("vbmeta_vendor",partition_name) == 0) ||
			(strcmp("vbmeta_odm",partition_name) == 0) ||
			(strcmp("vbmeta_system_ext",partition_name) == 0) ||
			(strcmp("vbmeta_product",partition_name) == 0)) {
		vboot_verify_info->img_len = 4096;
	} else {
		ret = secure_get_partition_size(partition_name, &partition_size);
		debugf("ret = %d\n", ret);
		if (ret < 0) {
			debugf("secure_get_partition_size error!\n");
			return;
		}
		vboot_verify_info->img_len=partition_size;
	}

	debugf("img_addr:0x%llx.  len:0x%llx. \n", vboot_verify_info->img_addr, vboot_verify_info->img_len);

	if ((strcmp("boot",partition_name) == 0) || (strcmp("recovery",partition_name) == 0))
	{
#ifdef SPRD_DTS_MEM_LAYOUT
	        if (get_buffer_base_size_from_dt("heap@4", &buf_base, &buf_size)) {
		        errorf("get buffer error\n");
			return;
		}
#else
		buf_base = VBOOT_VERIFY_BUF_ADDR;
		buf_size = VBOOT_VERIFY_BUF_SIZE;
#endif
		temp_base = vboot_verify_info->img_addr;
		vboot_verify_info->img_addr = buf_base;
		debugf("bootimage and recovery addr is 0x%x, buf_size is 0x%x\n",buf_base,buf_size);
	} else if (strcmp("vendor_boot",partition_name) == 0) {
#ifdef SPRD_DTS_MEM_LAYOUT
		if (get_buffer_base_size_from_dt("heap@4", &buf_base, &buf_size)) {
			errorf("get buffer error...\n");
			return;
		}
#else
		buf_base = VBOOT_VERIFY_BUF_ADDR;
		buf_size = VBOOT_VERIFY_BUF_SIZE;
#endif
		temp_base = vboot_verify_info->img_addr;
		vboot_verify_info->img_addr = buf_base + 100*1024*1024;
		debugf("vendor_boot addr is 0x%x, buf_size is 0x%x, offset is 0x%x\n",buf_base, buf_size, 100*1024*1024);
	}

	debugf("img_addr:0x%llx.  len:0x%llx. \n", vboot_verify_info->img_addr, vboot_verify_info->img_len);

#ifdef KCE_ENCRYPT_FLAG
	if (strcmp("dtbo", partition_name) == 0) {
		buf_base = VBOOT_VERIFY_BUF_ADDR;
		temp_base = vboot_verify_info->img_addr;
		vboot_verify_info->img_addr = buf_base + 228*1024*1024;
		vboot_verify_info->kce_decrypt_flag = KCE_DECRYPT_FLAG;
		debugf("dtbo img addr is 0x%x, buf_size is 0x%x\n",vboot_verify_info->img_addr,buf_size);
	}
#endif

	/*partition load*/
	if (-1 == secure_load_partition(partition_name,vboot_verify_info->img_len, (uint64_t)0, vboot_verify_info->img_addr)) {
		debugf("load %s partition error\n", partition_name);
		return;
	}

#ifdef CONFIG_VBOOT_DUMP
	dumpHex("boot image",  (uchar *)vboot_verify_info->img_addr, 512);

	dumpHex("vbmeta image",  (uchar *)vboot_verify_info->vbmeta_img_addr, 512);

	dumpHex("vbmeta pubkey",  (uchar *)vboot_verify_info->vbmeta_pubkey_addr, 32);
#endif

	memset(vboot_para_partition_name, 0, sizeof(vboot_para_partition_name));
	strcpy(vboot_para_partition_name, partition_name);
	vboot_verify_info->img_name = vboot_para_partition_name;
	vboot_verify_info->img_name_len = SEC_MAX_PARTITION_NAME_LEN;
	debugf("vboot_verify_info->img_name :%s. \n", (uint32_t)vboot_verify_info->img_name);

	vboot_verify_info->vboot_unlock_status = g_DeviceStatus;

	debugf("g_sprd_vboot_cmdline len %d.\n", strlen(g_sprd_vboot_cmdline));
	debugf("g_sprd_vboot_cmdline is %s.\n", g_sprd_vboot_cmdline);

	if (((strcmp("boot",partition_name) == 0) || (strcmp("recovery",partition_name) == 0))
							&& g_DeviceStatus == VBOOT_STATUS_UNLOCK) {
		debugf("Device Status is unlock, skip %s image verify!.\n", partition_name);
		vboot_check_result = AVB_SLOT_VERIFY_RESULT_OK;
	} else {
		secure_avb_verify_image((VbootVerifyInfo*)vboot_verify_info, sizeof(VbootVerifyInfo), SECBOOT_DISPLAY_ENABLEED);
		/*for verify boot*/
		debugf("partion %s, verify_ret:0x%x\n",partition_name, vboot_verify_info->verify_return_data->vboot_verify_ret);
		vboot_check_result = vboot_verify_info->verify_return_data->vboot_verify_ret;
	}
	if ((strcmp("boot",partition_name) == 0) || (strcmp("recovery",partition_name) == 0))
	{
		vboot_verify_info->img_addr = temp_base;
		ddr_boot_img_addr = buf_base;
	} else if (strcmp("vendor_boot",partition_name) == 0) {
		vboot_verify_info->img_addr = temp_base;
		vendorboot_ddr_img_addr = buf_base + 100*1024*1024;
	}
#ifdef KCE_ENCRYPT_FLAG
	if (strcmp("dtbo", partition_name) == 0) {
		vboot_verify_info->img_addr = temp_base;
		ddr_dtbo_img_addr = buf_base + 228*1024*1024;
	}
#endif
	set_verified_boot_state(partition_name, vboot_check_result);
}

void vboot_secure_process_prepare(void)
{
	unsigned char sprd_vboot_key[SPRD_RSA4096PUBK_HASHLEN] = {0};
	sprd_get_vboot_key(UBOOT_START, sprd_vboot_key, GET_FROM_RAM);

#ifdef CONFIG_VBOOT_DUMP
	dumpHex("vboot pubk", sprd_vboot_key, SPRD_RSA4096PUBK_HASHLEN);
#endif
	avb_ops_set_vboot_key(sprd_vboot_key, sizeof(sprd_vboot_key));
}


void sprd_fb_secure_vboot_prepare(char *partition_name, char *img_ram_addr, char * vbmeta_ram_addr, sys_img_header *header)
{
	unsigned char sprd_vboot_key[SPRD_RSA4096PUBK_HASHLEN] = {0};
	uint64_t partition_size = 0;
	VERIFY_PROCESS_FLAG process_flag = VERIFY_PROCESS_DOWNLOAD;

	memset(vboot_verify_info, 0, sizeof(VbootVerifyInfo));

	/*from uboot get vboot key*/
	sprd_get_vboot_key((CONFIG_SYS_TEXT_BASE - 0x200), sprd_vboot_key, GET_FROM_RAM);

#ifdef CONFIG_VBOOT_DUMP
	dumpHex("vboot pubk", sprd_vboot_key, SPRD_RSA4096PUBK_HASHLEN);
#endif
	avb_ops_set_vboot_key(sprd_vboot_key, sizeof(sprd_vboot_key));

	secf("partition_name: %s, img_ram_addr: 0x%x, vbmeta_ram_addr:0x%x \r\n", partition_name, img_ram_addr, vbmeta_ram_addr);
	vboot_secure_process_init(partition_name, img_ram_addr, vbmeta_ram_addr, header, process_flag); //VBMETA_IMG_ADDR

	/*if it's not vbmeta image*/
	if(partition_name && (strcmp("vbmeta",partition_name) != 0)){
		if (secure_get_partition_size("vbmeta", &partition_size)) {
			secf("secure_get_partition_size fail\n");
			return;
		}
		if (-1 == secure_load_partition("vbmeta",partition_size, (uint64_t)0, vboot_verify_info->vbmeta_img_addr)){
			debugf("load vbmeta partition error\n");
			return;
		}
	}
	memset(vboot_para_partition_name, 0, sizeof(vboot_para_partition_name));
	strcpy(vboot_para_partition_name, partition_name);
	vboot_verify_info->img_name = vboot_para_partition_name;
	vboot_verify_info->img_name_len = SEC_MAX_PARTITION_NAME_LEN;

	memset(g_sprd_vboot_ret, 0, sizeof(g_sprd_vboot_ret));
	vboot_verify_info->verify_return_data = (VBootResultInfo *)g_sprd_vboot_ret;
	vboot_verify_info->verify_return_data_len = sizeof(VBootResultInfo);
	debugf("vboot_verify_info->img_name :%s. \n", (uint32_t)vboot_verify_info->img_name);
}

void sprd_dl_secure_vboot_prepare(void)
{
	unsigned char sprd_vboot_key_in_uboot[SPRD_RSA4096PUBK_HASHLEN];

	memset(sprd_vboot_key_in_uboot, 0, sizeof(sprd_vboot_key_in_uboot));
	sprd_get_vboot_key((CONFIG_SYS_TEXT_BASE - 0x200), sprd_vboot_key_in_uboot, 0);

#ifdef CONFIG_VBOOT_DUMP
	debug("dump uboot key hash \n");
	do_hex_dump(sprd_vboot_key_in_uboot, SPRD_RSA4096PUBK_HASHLEN);
#endif
	avb_ops_set_vboot_key(sprd_vboot_key_in_uboot, sizeof(sprd_vboot_key_in_uboot));
}
#endif

void secboot_update_swVersion(void)
{
	uint32_t flag;
	flag = SPRD_FLAG;

	uboot_update_swVersion(&flag,sizeof(flag));
}

#ifdef SPRD_SECBOOT
extern char product_sn_token[PRODUCT_SN_TOKEN_MAX_SIZE];
extern char product_sn_signature[PRODUCT_SN_SIGNATURE_SIZE];

int change_lock_status(unsigned int flag)
{
	if (flag == VBOOT_STATUS_UNLOCK) {
		debugf("set device status unlock\n");
	} else if(flag == VBOOT_STATUS_LOCK) {
		debugf("set device status lock\n");
	} else {
		debugf("lockflag is invalid.\n");
		return -1;
	}
	if (get_lock_status() == flag) {
		debugf("device can not been change status repeatly, ret = %d\n", flag);
		return 0;
	}
	lcd_printf("\n   Warning: change lock status have to erase user data.\n");
	debugf("Warning: change lock status have to erase user data.\n");
	if (0 != common_raw_erase("userdata", 0, 0)) {
		debugf("erase userdata failed\n");
		return -1;
	}
	if (0 != common_raw_erase("metadata", 0, 0)) {
		debugf("erase metadata failed\n");
		return -1;
	}
	if (set_lock_status(flag)) {
		debugf("set lock&unlock status failed.\n");
		return -1;
	}

	return 0;
}


int set_lock_status(unsigned int flag)
{
	int ret = -1;
	unsigned char *buffer= malloc_cache_aligned(PDT_INFO_LOCK_FLAG_SECTION_SIZE);
	if (buffer == NULL) {
		errorf("no enough heap for set lockstatus buffer\n");
		return -ENOMEM;
	}
	unsigned char *lock_flag = memalign(4096, PDT_INFO_LOCK_FLAG_MAX_SIZE);
	if (lock_flag == NULL) {
		errorf("no enough heap for set lockstatus lock_flag\n");
		free(buffer);
		return -ENOMEM;
	}

	memset(lock_flag, 0, PDT_INFO_LOCK_FLAG_MAX_SIZE);
	memset(buffer, 0, PDT_INFO_LOCK_FLAG_SECTION_SIZE);

	if (flag == VBOOT_STATUS_UNLOCK) {
		strcat(lock_flag, "VerifiedBoot-UNLOCK");
	} else if(flag == VBOOT_STATUS_LOCK) {
		strcat(lock_flag, "VerifiedBoot-LOCK");
	} else {
		debugf("lockflag is invalid.\n");
		free(buffer);
		free(lock_flag);
		return -2;
	}

	/*read from emmc*/
	if (common_raw_read(PRODUCTINFO_FILE_PATITION, PDT_INFO_LOCK_FLAG_SECTION_SIZE,
				PDT_INFO_LOCK_FLAG_OFFSET, (char *)buffer)) {
		debugf("read miscdata error.\n");
		free(buffer);
		free(lock_flag);
		return -1;
	}

	//dumpHex("data before encrypt.", lock_flag, PDT_INFO_LOCK_FLAG_MAX_SIZE);

#if defined SPRD_VBOOT_V2
	ret = uboot_encrypt_data(lock_flag, PDT_INFO_LOCK_FLAG_MAX_SIZE);
	if (ret) {
		debugf("data encrypt error.\n");
		free(buffer);
		free(lock_flag);
		return -3;
	}
#endif

	//dumpHex("data after encrypt.", lock_flag, PDT_INFO_LOCK_FLAG_MAX_SIZE);

	/*data ready*/
	memcpy((char *)buffer , lock_flag, PDT_INFO_LOCK_FLAG_MAX_SIZE);

	/*write to emmc*/
	if (common_raw_write(PRODUCTINFO_FILE_PATITION, PDT_INFO_LOCK_FLAG_SECTION_SIZE, 0,
				PDT_INFO_LOCK_FLAG_OFFSET, (char *)buffer)) {
		debugf("write miscdata error.\n");
		free(buffer);
		free(lock_flag);
		return -1;
	}

	free(buffer);
	free(lock_flag);
	return 0;
}

unsigned int get_lock_status(void)
{
	int ret = -1;
	unsigned char *lock_flag = malloc(PDT_INFO_LOCK_FLAG_SECTION_SIZE);
	if (lock_flag == NULL) {
		errorf("no enough heap for get lockstatus\n");
		return -ENOMEM;
	}

	memset(lock_flag, 0, PDT_INFO_LOCK_FLAG_SECTION_SIZE);

	if (common_raw_read(PRODUCTINFO_FILE_PATITION, PDT_INFO_LOCK_FLAG_SECTION_SIZE, PDT_INFO_LOCK_FLAG_OFFSET, (char *)lock_flag)) {
		debugf("read miscdata error.\n");
	}

	ret = sprd_sec_verify_lockstatus(lock_flag, PDT_INFO_LOCK_FLAG_MAX_SIZE);

	if(ret != 0) {
		g_DeviceStatus = VBOOT_STATUS_LOCK; /*default lock status.*/
	}
	else {
		g_DeviceStatus = VBOOT_STATUS_UNLOCK; /*unlock status.*/
	}

	free(lock_flag);
	return g_DeviceStatus;
}

int verify_product_sn_signature(void)
{
	int ret = -1;
	uint64_t partition_size = 0;
	uint32_t unlock_bootloader_result = 0;
	int img_size = -1;

#ifdef TOS_TRUSTY
#if defined SPRD_VBOOT_V2
	memset(vboot_unlock_verify_info, 0,  sizeof(VbootUnlockVerifyInfo));
	vboot_secure_process_prepare();
	vboot_unlock_verify_info->vbmeta_pubkey_addr = g_sprd_vboot_key;
	vboot_unlock_verify_info->vbmeta_pubkey_len = g_sprd_vboot_key_len;

	if (-1 == secure_get_partition_size("vbmeta", &partition_size)){
		debugf("get vbmeta partition size fail\n");
		return -1;
	}

	vboot_unlock_verify_info->vbmeta_img_addr = VBMETA_IMG_BASE;

	img_size =  secure_load_partition("vbmeta",partition_size,
			(uint64_t)0, vboot_unlock_verify_info->vbmeta_img_addr);
	if (-1 == img_size){
		debugf("load vbmeta partition error\n");
		return -1;
	}

	vboot_unlock_verify_info->vbmeta_img_len = (uint32_t)(img_size);
	vboot_unlock_verify_info->product_sn_addr = product_sn_token;
	vboot_unlock_verify_info->product_sn_len = PRODUCT_SN_TOKEN_MAX_SIZE;
	vboot_unlock_verify_info->product_sn_signature_addr = product_sn_signature;
	vboot_unlock_verify_info->product_sn_signature_len = PRODUCT_SN_SIGNATURE_SIZE;
	vboot_unlock_verify_info->verify_return_data = (VBootResultInfo *)g_sprd_vboot_ret;
	vboot_unlock_verify_info->verify_return_data_len = sizeof(VBootResultInfo);

	ret = uboot_verify_product_sn_signature((VbootUnlockVerifyInfo*)vboot_unlock_verify_info,
			sizeof(VbootUnlockVerifyInfo));
#endif
#else
	memset(&vboot_unlock_verify_info, 0,  sizeof(VbootUnlockVerifyInfo));
	vboot_secure_process_prepare();
	vboot_unlock_verify_info.vbmeta_pubkey_addr = g_sprd_vboot_key;
	vboot_unlock_verify_info.vbmeta_pubkey_len = g_sprd_vboot_key_len;

	if (-1 == secure_get_partition_size("vbmeta", &partition_size)){
		debugf("get vbmeta partition size fail\n");
		return -1;
	}

	vboot_unlock_verify_info.vbmeta_img_addr = VBMETA_IMG_BASE;

	img_size =  secure_load_partition("vbmeta",partition_size,
			(uint64_t)0, vboot_unlock_verify_info.vbmeta_img_addr);
	if (-1 == img_size){
		debugf("load vbmeta partition error\n");
		return -1;
	}

	vboot_unlock_verify_info.vbmeta_img_len = (uint32_t)(img_size);
	vboot_unlock_verify_info.product_sn_addr = product_sn_token;
	vboot_unlock_verify_info.product_sn_len = PRODUCT_SN_TOKEN_MAX_SIZE;
	vboot_unlock_verify_info.product_sn_signature_addr = product_sn_signature;
	vboot_unlock_verify_info.product_sn_signature_len = PRODUCT_SN_SIGNATURE_SIZE;
	vboot_unlock_verify_info.verify_return_data = (VBootResultInfo *)g_sprd_vboot_ret;
	vboot_unlock_verify_info.verify_return_data_len = sizeof(VBootResultInfo);

	ret = uboot_verify_product_sn_signature((VbootUnlockVerifyInfo*)&vboot_unlock_verify_info,
			sizeof(VbootUnlockVerifyInfo));

#endif

	if (!ret) {
#ifdef TOS_TRUSTY
		unlock_bootloader_result = vboot_unlock_verify_info->verify_return_data->vboot_verify_ret;
#else
		unlock_bootloader_result = vboot_unlock_verify_info.verify_return_data->vboot_verify_ret;
#endif
		if (unlock_bootloader_result){
			debugf("uboot_verify_product_sn_signature unlock succeeded\n");
			return ret;
		} else {
			debugf("uboot_verify_product_sn_signature unlock failed\n");
			return -1;
		}
	}

	return ret;
}

int config_os_version(unsigned long *os_version, uint32_t os_version_len)
{
	int ret = -1;

	if (0 == ((*os_version)& 0xffffffff) || os_version_len != 4) {
		debugf("system version error\n");
		return -1;
	}

	ret = uboot_config_os_version((unsigned long)os_version, os_version_len);

	if (!ret) {
		debugf("uboot_config_os_version succeeded\n");
	} else {
		debugf("uboot_config_os_version failed\n");
	}

	return ret;
}

int set_root_of_trust(unsigned char *root_of_trust_str, uint32_t root_of_trust_len)
{
	int ret = -1;

	if (0 == ((*root_of_trust_str)& 0xffffffff)
	    || root_of_trust_len != ROOT_OF_TRUST_MAXSIZE) {
		debugf("root_of_trust information error\n");
		return -1;
	}

	ret = uboot_set_root_of_trust((unsigned long)root_of_trust_str, root_of_trust_len);

	if (!ret) {
		debugf("uboot_set_root_of_trust succeeded\n");
	} else {
		debugf("uboot_set_root_of_trust failed\n");
	}

	return ret;
}

#endif
