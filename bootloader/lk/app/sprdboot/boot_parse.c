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
#include <sprd_common.h>
#include <sprd_common_rw.h>
#include "android_bootimg.h"
#include <android_ab.h>
#include "boot_parse.h"
#include <malloc.h>
#include <miscdata_def.h>
#include <serial_sprd.h>
#include <string.h>
#include <arch/ops.h>
#include <boot_mode.h>
#include <lcd.h>
#include <arch/sprd_cache.h>
#include <linux/byteorder/generic.h>
#include <linux/byteorder/little_endian.h>
#include <linux/kernel.h>
#include <part.h>
#include "dl_operate.h"
#include "sprd_common_rw.h"
#include <chipram_env.h>
#include <logo_bin.h>
#include <errno.h>
#include <decompress_data.h>
#include <sprd_cpcmdline.h>
#include "sprd_sysdump.h"
#ifdef CONFIG_MEM_LAYOUT_DECOUPLING
#include <cp_mem_decoupling.h>
#endif
#ifdef CONFIG_BOOTLOADER_HWFEATURE
#include <sprd_hwfeature.h>
#endif
#include <secureboot/sprdsec_header.h>
#include <secureboot/sec_common.h>
#include <lk_sec_drv.h>
#ifndef CONFIG_ZEBU
#include <keymint.h>
#endif
#include <uboot_avb_ops.h>
#if WITH_PLATFORM_SPRD_SHARED_TRUSTZONE
#include <tee_smc_call.h>
#endif

#ifdef CONFIG_MINI_TRUSTZONE
#include "trustzone_def.h"
#endif

#include <libfdt.h>
#include "sprd_fdt_support.h"
#include "sprd_fdt_memory.h"

#ifdef CONFIG_MATCH_DTBO_BY_BOARDID
#include <sprd_keys.h>
#endif

#if defined CONFIG_CHIP_UID
#include <uid_helper.h>
#endif

#ifdef CONFIG_BOOTCONFIG
#include <sprd_bootconfig_support.h>
#endif

#ifndef POWEROFF_CHARGE_LOGO_SUPPORT
#define POWEROFF_CHARGE_LOGO_SUPPORT	0
#endif

#ifdef CONFIG_ARM7_RAM_ACTIVE
extern void pmic_arm7_RAM_active(void);
#endif

#if (defined SPRD_SECBOOT) && (defined SPRD_VBOOT_V2)
unsigned long os_version __attribute__((aligned(4096))); /*must be PAGE ALIGNED*/
unsigned char root_of_trust_str[ROOT_OF_TRUST_MAXSIZE] __attribute__((aligned(4096)));
#ifdef KCE_ENCRYPT_FLAG
extern volatile uint32_t g_start_merge_dtbo_flag;
#endif
#endif

#ifdef CONFIG_BOARD_KERNEL_CMDLINE
unsigned char vendorboot_cmdline[VENDOR_BOOT_ARGS_SIZE];
#endif

extern enVerifiedState g_verifiedbootstate;
extern int uboot_verify_lockstatus(uint64_t start_addr, uint64_t lenth);
#if ARM_WITH_MMU
extern void arch_disable_mmu(void);
#endif

unsigned char raw_header[8192];
char serial_number_to_transfer[SP15_MAX_SN_LEN];
static char boot_v3[128] = "boot";
static char recovery_v3[128] = "recovery";
static char vendor_boot_v3[128] = "vendor_boot";
static char init_boot_v3[128] = "init_boot";
void *g_fdt_blob = NULL;
int g_res_spbootcode_info = 0;

boot_img_info_t s_bootimg_info;
unsigned int g_DeviceStatus = VBOOT_STATUS_LOCK;
unsigned int g_DtboIndex = 0;
extern int gunzip(void *, int, unsigned char *, unsigned long *);/* external/gzip/gunzip.c */

#ifdef CONFIG_HIGHFLASH_DTBO
extern int sprd_get_tpic_version(void);
#endif

#ifdef CONFIG_SUPPORT_TDLTE
static boot_image_required_t const s_boot_image_tl_table[] = {
#if !defined( CONFIG_KERNEL_BOOT_CP )
	{"tl_fixnv1", "tl_fixnv2", LTE_FIXNV_SIZE, (char*)LTE_FIXNV_ADDR},
	{"tl_runtimenv1", "tl_runtimenv2", LTE_RUNNV_SIZE, (char*)LTE_RUNNV_ADDR},
	{"tl_modem", "", LTE_MODEM_SIZE, (char*)LTE_MODEM_ADDR},
	{"tl_ldsp", "", LTE_LDSP_SIZE, (char*)LTE_LDSP_ADDR},	//ltedsp
	{"tl_tgdsp", "", LTE_GDSP_SIZE, (char*)LTE_GDSP_ADDR},
#endif
	{"", "", 0, 0}
};
#endif

#ifdef CONFIG_SUPPORT_WLTE
static boot_image_required_t const s_boot_image_wl_table[] = {
#if !defined( CONFIG_KERNEL_BOOT_CP )
	{"wl_fixnv1", "wl_fixnv2", LTE_FIXNV_SIZE, (char*)LTE_FIXNV_ADDR},
	{"wl_runtimenv1", "wl_runtimenv2", LTE_RUNNV_SIZE, (char*)LTE_RUNNV_ADDR},
	{"wl_modem", "", LTE_MODEM_SIZE, (char*)LTE_MODEM_ADDR},
	{"wl_ldsp", "", LTE_LDSP_SIZE, (char*)LTE_LDSP_ADDR},
	{"wl_gdsp", "", LTE_GDSP_SIZE, (char*)LTE_GDSP_ADDR},
	{"wl_warm", "", WL_WARM_SIZE, (char*)WL_WARM_ADDR},
#endif
	{"", "", 0, 0}
};
#endif

#ifdef CONFIG_SUPPORT_GSM
static boot_image_required_t const s_boot_image_gsm_table[] = {
#if !defined( CONFIG_KERNEL_BOOT_CP )
	{"g_fixnv1", "g_fixnv2", GSM_FIXNV_SIZE, (char*)GSM_FIXNV_ADDR},
	{"g_runtimenv1", "g_runtimenv2", GSM_RUNNV_SIZE, (char*)GSM_RUNNV_ADDR},
	{"g_modem", "", GSM_MODEM_SIZE, (char*)GSM_MODEM_ADDR},
	{"g_dsp", "", GSM_DSP_SIZE, (char*)GSM_DSP_ADDR},
#endif
	{"", "", 0, 0}
};
#endif

#ifdef CONFIG_SUPPORT_LTE
static boot_image_required_t const s_boot_image_lte_table[] = {
#if (!defined( CONFIG_KERNEL_BOOT_CP )) && (!defined(CONFIG_MEM_LAYOUT_DECOUPLING))
#ifdef CONFIG_ADVANCED_LTE
	{"l_fixnv1", "l_fixnv2", LTE_FIXNV_SIZE, LTE_FIXNV_ADDR},
	{"l_runtimenv1", "l_runtimenv2", LTE_RUNNV_SIZE, (char*)LTE_RUNNV_ADDR},
	{"l_modem", "", LTE_MODEM_SIZE, (char*)LTE_MODEM_ADDR},
	{"l_tgdsp", "", LTE_TGDSP_SIZE, (char*)LTE_TGDSP_ADDR}, //tddsp
	{"l_ldsp", "", LTE_LDSP_SIZE, (char*)LTE_LDSP_ADDR},
#else
	{"l_fixnv1", "l_fixnv2", LTE_FIXNV_SIZE, (char*)LTE_FIXNV_ADDR},
	{"l_runtimenv1", "l_runtimenv2", LTE_RUNNV_SIZE, (char*)LTE_RUNNV_ADDR},
	{"l_modem", "", LTE_MODEM_SIZE, (char*)LTE_MODEM_ADDR},
	{"l_ldsp", "", LTE_LDSP_SIZE, (char*)LTE_LDSP_ADDR},
	{"l_gdsp", "", LTE_GDSP_SIZE, (char*)LTE_GDSP_ADDR},
	{"l_warm", "", WL_WARM_SIZE, (char*)WL_WARM_ADDR},
#endif
#endif
#ifdef  CONFIG_ADVANCED_LTE
#ifdef CONFIG_SUPPORT_AGDSP
	{"l_agdsp", "", LTE_AGDSP_SIZE, (char*)LTE_AGDSP_ADDR}, //agdsp now is boot by uboot
#endif// end CONFIG_SUPPORT_AGDSP
#endif
	{"", "", 0, 0}
};
#endif

#ifdef CONFIG_SUPPORT_NR
static boot_image_required_t const s_boot_image_nr_table[] = {
#if (!defined( CONFIG_KERNEL_BOOT_CP )) && (!defined(CONFIG_MEM_LAYOUT_DECOUPLING))
	{"nr_fixnv1", "nr_fixnv2", NR_FIXNV_SIZE, (char*)NR_FIXNV_ADDR},
	{"nr_runtimenv1", "nr_runtimenv2", NR_RUNNV_SIZE, (char*)NR_RUNNV_ADDR},
	{"nr_modem", "", NR_MODEM_SIZE, (char*)NR_MODEM_ADDR},
	{"nr_phy", "", NR_PHY_SIZE, (char*)NR_PHY_ADDR},
#endif
#ifdef CONFIG_SUPPORT_AGDSP
	{"l_agdsp", "", LTE_AGDSP_SIZE, (char*)LTE_AGDSP_ADDR}, //agdsp now is boot by uboot
#endif// end CONFIG_SUPPORT_AGDSP
	{"", "", 0, 0}
};
#endif

#ifdef CONFIG_SUPPORT_TD
static boot_image_required_t const s_boot_image_TD_table[] = {
#if !defined( CONFIG_KERNEL_BOOT_CP )
	{"tdfixnv1", "tdfixnv2", FIXNV_SIZE, (char*)TDFIXNV_ADR},
	{"tdruntimenv1", "tdruntimenv2", RUNTIMENV_SIZE, (char*)TDRUNTIMENV_ADR},
	{"tdmodem", "", TDMODEM_SIZE, (char*)TDMODEM_ADR},
	{"tddsp", "", TDDSP_SIZE, (char*)TDDSP_ADR},
#endif
	{"", "", 0, 0}
};
#endif

#ifdef CONFIG_SUPPORT_W
static boot_image_required_t const s_boot_image_W_table[] = {
#if (!defined( CONFIG_KERNEL_BOOT_CP )) && (!defined(CONFIG_MEM_LAYOUT_DECOUPLING))
	{"wfixnv1", "wfixnv2", FIXNV_SIZE, (char*)WFIXNV_ADR},
	{"wruntimenv1", "wruntimenv2", RUNTIMENV_SIZE, (char*)WRUNTIMENV_ADR},
	{"wmodem", "", WMODEM_SIZE, (char*)WMODEM_ADR},
	{"wdsp", "", WDSP_SIZE, (char*)WDSP_ADR},
#endif
	{"", "", 0, 0}
};
#endif

#ifdef CONFIG_SUPPORT_WIFI
static boot_image_required_t const s_boot_image_WIFI_table[] = {
	{"wcnfixnv1", "wcnfixnv2", FIXNV_SIZE, (char*)WCNFIXNV_ADR},
	{"wcnruntimenv1", "wcnruntimenv2", RUNTIMENV_SIZE, (char*)WCNRUNTIMENV_ADR},
	{"wcnmodem", "", WCNMODEM_SIZE, (char*)WCNMODEM_ADR},
	{"", "", 0, 0}
};
#endif

static boot_image_required_t const s_boot_image_COMMON_table[] = {
#ifdef CONFIG_SIMLOCK_ENABLE
	{"simlock", "", SIMLOCK_SIZE, (char*)SIMLOCK_ADR},
#endif
#ifdef CONFIG_DFS_ENABLE
#if !defined( CONFIG_KERNEL_BOOT_CP )
	{"pm_sys", "", DFS_SIZE, (char*)DFS_ADDR},
#endif
#endif
#ifdef CONFIG_CH_ENABLE
#if !defined( CONFIG_KERNEL_BOOT_CP )
/*
	For CH subsys, hardware cannot copy datas from emmc to
	ch iram by dma directly.
	when ch iram boot, software copy those datas from emmc
	to ch ddr by dma firstly, then copy those datas from ddr
	to ch iram.
	when ch ddr boot,software only copy those datas from emmc
	to ch ddr by dma.
*/
#if defined(CONFIG_CH_DDR_BOOT)
	{"ch_sys", "", CH_DDR_SIZE, (char*)CH_DDR_ADDR},
#else
	{"ch_sys", "", CH_IRAM_SIZE, (char*)CH_DDR_ADDR},
#endif
#endif
#endif
	{"", "", 0, 0}

};

#ifdef CONFIG_MINI_TRUSTZONE
static boot_image_required_t const s_boot_image_TZ_table[] = {
	{"sml", "", TRUSTRAM_SIZE, (char*)TRUSTRAM_ADR},
	{"", "", 0, 0}

};
#endif

static const boot_image_required_t *const s_boot_image_table[] = {
#ifdef CONFIG_SUPPORT_TDLTE
	s_boot_image_tl_table,
#endif

#ifdef CONFIG_SUPPORT_WLTE
	s_boot_image_wl_table,
#endif

#ifdef CONFIG_SUPPORT_LTE
	s_boot_image_lte_table,
#endif

#ifdef CONFIG_SUPPORT_NR
	s_boot_image_nr_table,
#endif

#ifdef CONFIG_SUPPORT_GSM
	s_boot_image_gsm_table,
#endif

#ifdef CONFIG_SUPPORT_TD
	s_boot_image_TD_table,
#endif

#ifdef CONFIG_SUPPORT_W
	s_boot_image_W_table,
#endif

#ifdef CONFIG_SUPPORT_WIFI
	s_boot_image_WIFI_table,
#endif
#ifdef CONFIG_MINI_TRUSTZONE
	s_boot_image_TZ_table,
#endif
	s_boot_image_COMMON_table,

	0
};

static struct pre_load_operations sprd_pre_load = {
#ifndef CONFIG_ZEBU
	.power = power_cfg,
#ifdef CONFIG_SPLASH_SCREEN
	.display   = logo_display,
#endif
	.vibrator  = vibrator_load
#endif
};

static struct post_load_operations sprd_post_load = {
#ifndef CONFIG_ZEBU
	.rpmb = rpmb_check,
	.keymint = km_initialize
#endif
};

#define VERSION_R	11
static uint32_t img_os_version = 0;
static void check_img_os_version(uint32_t os_version)
{
	if (os_version) {
		img_os_version = (uint32_t) os_version >> 25;
		dprintf(ALWAYS, "operate system is VERSION_%d from header\n", img_os_version);
	}
	//start add by hyinfeng for HMD NYX-296 show warning messages when device is unlocked or system is invalid.
#if defined(ZCFG_USERDEBUG_LOCKDEVICE_CANNOT_BOOT)
/*
	extern void showNoValidOSMessage(void);
	if(img_os_version < 13)
	{
		int sec_time_count = 30*5;
		int key_code;
		showNoValidOSMessage();
		while(sec_time_count) {
			mdelay(200);
			sec_time_count --;
			key_code = power_button_pressed();
			if (key_code == 0) {
				power_down_devices(0);
				return ;
			}				
		}
		power_down_devices(0);
	}
*/
#endif
	//end add by hyinfeng for HMD NYX-296 show warning messages when device is unlocked or system is invalid.
#ifdef SPRD_VBOOT_V2
	if (img_os_version == 0) {
		vboot_get_bootov_binding(&img_os_version);
		dprintf(ALWAYS, "operate system is VERSION_%d from footer\n", img_os_version);
	}
#endif

#ifdef CONFIG_ANDROID_OS_VERSION
	if (img_os_version == 0) {
		img_os_version = CONFIG_ANDROID_OS_VERSION;
		dprintf(ALWAYS, "operate system is VERSION_%d from config\n", img_os_version);
	}
#endif

	return ;
}

/* Note: return NULL if kernel offset was not found */
struct boot_img_hdr *get_boot_img_hdr(void)
{
	return !s_bootimg_info.kernel_offset ? NULL : (boot_img_hdr *)raw_header;
}

struct boot_img_hdr_v1 *get_boot_img_hdr_v1(struct boot_img_hdr *hdr)
{
	return (struct boot_img_hdr_v1 *)(hdr + 1);
}

struct boot_img_hdr_v2 *get_boot_img_hdr_v2(struct boot_img_hdr *hdr)
{
	struct boot_img_hdr_v1 *hdr_v1 = get_boot_img_hdr_v1(hdr);

	return (struct boot_img_hdr_v2 *)(hdr_v1 + 1);
}

void _boot_secure_check(void)
{
#ifdef SECURE_BOOT_ENABLE
	secure_check(DSP_ADR, 0, DSP_ADR + DSP_SIZE - VLR_INFO_OFF, CONFIG_SYS_NAND_U_BOOT_DST + CONFIG_SYS_NAND_U_BOOT_SIZE - KEY_INFO_SIZ - VLR_INFO_OFF);
	secure_check(MODEM_ADR, 0, MODEM_ADR + MODEM_SIZE - VLR_INFO_OFF,
		     CONFIG_SYS_NAND_U_BOOT_DST + CONFIG_SYS_NAND_U_BOOT_SIZE - KEY_INFO_SIZ - VLR_INFO_OFF);
#ifdef CONFIG_SIMLOCK
	secure_check(SIMLOCK_ADR, 0, SIMLOCK_ADR + SIMLOCK_SIZE - VLR_INFO_OFF,
		     CONFIG_SYS_NAND_U_BOOT_DST + CONFIG_SYS_NAND_U_BOOT_SIZE - KEY_INFO_SIZ - VLR_INFO_OFF);
#endif
#endif
	return;
}

int32_t _boot_read_partition_with_backup(const boot_image_required_t info)
{
	uchar __aligned(ARCH_DMA_MINALIGN) header_buf[NV_HEADER_SIZE];
	nv_header_t *header_p = (nv_header_t *)header_buf;
	int status = ORIGIN_BACKUP_NV_OK;
	int ret = 0;
#ifndef CONFIG_NOT_BACKUP_NV
	uchar __aligned(ARCH_DMA_MINALIGN) backup_header_buf[NV_HEADER_SIZE];
	nv_header_t *backup_header_p = (nv_header_t *)backup_header_buf;
	uchar * backup_nv_buf = NULL;

	backup_nv_buf = malloc_cache_aligned(info.size);
	if (NULL == backup_nv_buf) {
		errorf("no enough space for backup nv buffer\n");
		return 0;
	}
#endif
#ifdef CONFIG_ANDROID_AB
	if (!strstr(info.partition, "runtimenv")) {
		get_slot_ab(info.partition, NULL);
	}
#endif

	do {
		/*read origin image header */
		if (0 != common_raw_read((const char *)info.partition, NV_HEADER_SIZE, (uint64_t)0, (char *)header_buf)) {
			errorf("read origin <<<%s>>> image header failed\n", info.partition);
			status |= ORIGIN_NV_DAMAGED;
			break;
		}

		if (NV_HEAD_MAGIC != header_p->magic) {
			errorf("<<<%s>>> header magic error, wrong magic=0x%x\n", info.partition, header_p->magic);
			status |= ORIGIN_NV_DAMAGED;
			break;
		}

		/*read origin image */
		if (0 != common_raw_read((const char *)info.partition, (uint64_t)(info.size), NV_HEADER_SIZE, info.mem_addr)) {
			errorf("read origin <<<%s>>> image data failed\n", info.partition);
			status |= ORIGIN_NV_DAMAGED;
			break;
		}

		/*check crc */
		if (_chkNVEcc((uint8_t *)info.mem_addr, info.size, header_p->checksum)) {
			debugf("read origin <<<%s>>> image success and crc correct\n", info.partition);
		} else {
			errorf("check origin <<<%s>>> image crc wrong\n", info.partition);
			status |= ORIGIN_NV_DAMAGED;
		}
	} while(0);

#ifndef CONFIG_NOT_BACKUP_NV
	do {
		/*read backup header */
		if (0 != common_raw_read((const char *)info.bak_partition, NV_HEADER_SIZE, (uint64_t)0, (char *)backup_header_buf)) {
			errorf("read backup <<<%s>>> image header failed\n", info.bak_partition);
			status |= BACKUP_NV_DAMAGED;
			break;
		}

		if (NV_HEAD_MAGIC != backup_header_p->magic) {
			errorf("backup <<<%s>>> header magic error, wrong magic=0x%x\n", info.bak_partition, backup_header_p->magic);
			status |= BACKUP_NV_DAMAGED;
			break;
		}

		/*read bakup image */
		if (0 != common_raw_read((const char *)info.bak_partition, (uint64_t)(info.size), NV_HEADER_SIZE, (char *)backup_nv_buf)) {
			errorf("read backup <<<%s>>> image failed\n", info.bak_partition);
			status |= BACKUP_NV_DAMAGED;
			break;
		}

		/*check crc */
		if (_chkNVEcc(backup_nv_buf, info.size, backup_header_p->checksum)) {
			debugf("read backup <<<%s>>> image success and crc correct\n", info.bak_partition);
		} else {
			errorf("check backup <<<%s>>> image crc wrong\n", info.bak_partition);
			status |= BACKUP_NV_DAMAGED;
		}
	} while(0);
#endif

	switch (status) {
	case ORIGIN_BACKUP_NV_OK :
		debugf("both org and bak nv partition are ok\n");
		ret = 1;
		break;

#ifndef CONFIG_NOT_BACKUP_NV
	case ORIGIN_NV_DAMAGED :
		memcpy(info.mem_addr, backup_nv_buf, info.size);
		if (0 != common_raw_write((const char *)info.partition, NV_HEADER_SIZE, (uint64_t)0, (uint64_t)0, (char *)backup_header_buf)) {
			errorf("reapire origin <<<%s>>> image header fail\n", info.partition);
			ret = 0;
			break;
		}

		if (0 != common_raw_write((const char *)info.partition, (uint64_t)(info.size), (uint64_t)0, NV_HEADER_SIZE, (char *)backup_nv_buf)) {
			errorf("reapire origin <<<%s>>> image data fail\n", info.partition);
			ret = 0;
			break;
		}

		ret = 1;
		break;
	case BACKUP_NV_DAMAGED :
		if (0 != common_raw_write((const char *)info.bak_partition, NV_HEADER_SIZE, (uint64_t)0, (uint64_t)0, (char *)header_buf)) {
			errorf("reapire backup <<<%s>>> image header fail\n", info.bak_partition);
			ret = 0;
			break;
		}

		if (0 != common_raw_write((const char *)info.bak_partition, (uint64_t)(info.size), (uint64_t)0, NV_HEADER_SIZE, info.mem_addr)) {
			errorf("reapire backup <<<%s>>> image data fail\n", info.bak_partition);
			ret = 0;
			break;
		}
		debugf("repair backup <<<%s>>> image OK\n", info.bak_partition);
		ret = 1;
		break;
	case ORIGIN_NV_DAMAGED | BACKUP_NV_DAMAGED :
		errorf("both org <<<%s>>> and bak <<<%s>>> partition are damaged!\n", info.partition, info.bak_partition);
		ret = 0;
		break;
#endif
	}

#ifndef CONFIG_NOT_BACKUP_NV
	free(backup_nv_buf);
#endif
	return ret;
}

unsigned get_modem_img_info(const boot_image_required_t* img_info,
			    unsigned secure_offset,
			    int* is_sci,
			    size_t* total_len,
			    size_t* modem_exe_size) {
	unsigned offset = 0;

	if(!strstr((const char*)img_info->partition, "modem")) {
		*is_sci = 0;
		*total_len = img_info->size;
		*modem_exe_size = img_info->size;
		return 0;
	}

	/* Only support 10 effective headers at most for now. */
	data_block_header_t hdr_buf[11];
	size_t read_len = sizeof(hdr_buf);

	if (common_raw_read((const char*)img_info->partition,
			    read_len,
			    secure_offset,
			    (char*)hdr_buf)) {
		debugf("Read MODEM image header failed!\n");
		return 0;
	}

	/* Check whether it's SCI image. */
	if (memcmp(hdr_buf, MODEM_MAGIC, strlen(MODEM_MAGIC))) {
		/* Not SCI format. */
		*is_sci = 0;
		*total_len = img_info->size;
		*modem_exe_size = img_info->size;

		debugf("Not SCI image.\n");

		return 0;
	}

	/* SCI image. Parse the headers */
	*is_sci = 1;

	unsigned i;
	data_block_header_t* hdr_ptr;
	int modem_offset = -1;
	int image_len = -1;

	for (i = 1, hdr_ptr = hdr_buf + 1;
	     i < sizeof hdr_buf / sizeof hdr_buf[0];
	     ++i, ++hdr_ptr) {
		unsigned type = (hdr_ptr->type_flags & 0xff);
		if (SCI_TYPE_MODEM_BIN == type) {
			modem_offset = (int)hdr_ptr->offset;
			*modem_exe_size = hdr_ptr->length;
			if(hdr_ptr->type_flags & MODEM_SHA1_HDR) {
				modem_offset += MODEM_SHA1_SIZE;
				*modem_exe_size -= MODEM_SHA1_SIZE;
			}
		}
		if (hdr_ptr->type_flags & SCI_LAST_HDR) {
			image_len = (int)(hdr_ptr->offset + hdr_ptr->length);
			break;
		}
	}

	if (-1 == modem_offset) {
		debugf("No MODEM image found in SCI image!\n");
	} else if (-1 == image_len) {
		debugf("SCI header too long!\n");
	} else {
		*total_len = image_len;
		offset = modem_offset;
	}

	debugf("Modem SCI offset: 0x%x!\n", (unsigned)offset);

	return offset;
}

/*
 * Function for reading image which is needed when power on.
 */
int _boot_load_required_image(const boot_image_required_t img_info)
{
	char temp[36];

	debugf("load %s to addr %p, size = 0x%llx\n", img_info.partition, img_info.mem_addr, img_info.size);

	if (strlen(img_info.bak_partition)) {
		debugf("load %s with backup img %s\n", img_info.partition, img_info.bak_partition);
#ifdef CONFIG_ANDROID_AB
		if (!strstr(img_info.bak_partition, "runtimenv")) {
			get_slot_ab(temp, img_info.bak_partition);
			debugf("load backup %s to ddr\n", img_info.bak_partition);
		}
#endif
		_boot_read_partition_with_backup(img_info);
	} else {
#ifdef CONFIG_ANDROID_AB
		if (!strstr(img_info.partition, "runtimenv")) {
			get_slot_ab(temp, img_info.partition);
			debugf("load %s to ddr\n", img_info.partition);
		}
#endif
#if defined (SPRD_SECBOOT)
		if (0 == memcmp("pm_sys", img_info.partition, strlen("pm_sys")))
		{
		#ifndef PROJECT_SEC_CM4
			/***secboot 2nd step***/
			vboot_secure_process_flow(img_info.partition);
		#endif
		}
		else
		{
			/***secboot 2nd step***/
			vboot_secure_process_flow((char *)img_info.partition);
		}

	#if defined (SPRD_VBOOT_V2)
		memcpy(img_info.mem_addr,(const void *)VERIFY_BASE,img_info.size);
		flush_cache(img_info.mem_addr, img_info.size);
	#else
		memcpy(img_info.mem_addr,(const void *)(VERIFY_BASE + SYS_HEADER_SIZE),img_info.size);
		flush_cache(img_info.mem_addr, img_info.size);
	#endif
#else  // Secure boot is not turned on
	int is_sci;
	size_t total_len;
	size_t exe_size;
	unsigned exe_offset;
	boot_image_required_t i_info = {"", "", 0, NULL};

	strcpy(i_info.partition, temp);
	i_info.mem_addr = img_info.mem_addr;
	i_info.size = img_info.size;
	exe_offset = get_modem_img_info(&i_info, 0, &is_sci, &total_len, &exe_size);
	if (common_raw_read(temp, exe_size, exe_offset, img_info.mem_addr)) {
		errorf("read %s partition fail\n", img_info.partition);
	}
#endif
	}

	return 1;
}

static uint64_t boot_img_offset_v01(boot_img_hdr *hdr, uint64_t offset)
{
	uint64_t size;

	/*kernel image */
	s_bootimg_info.kernel_offset = offset + KERNL_PAGE_SIZE;
	size = PAD_SIZE(hdr->kernel_size, KERNL_PAGE_SIZE);
	if (0 == size) {
		errorf("kernel image should not be zero!\n");
		return 0;
	}

	debugf("s_bootimg_info.kernel_offset is 0x%llx\n", s_bootimg_info.kernel_offset );

	if( hdr->ramdisk_size) {
		/* ramdisk image */
		s_bootimg_info.ramdisk_offset = s_bootimg_info.kernel_offset + size;
		size = PAD_SIZE(hdr->ramdisk_size, KERNL_PAGE_SIZE);
		if (0 == size) {
			errorf("ramdisk image size should not be zero\n");
			return 0;
		}

		debugf("s_bootimg_info.ramdisk_offset is 0x%llx\n", s_bootimg_info.ramdisk_offset );

		/* dt image */
		s_bootimg_info.dt_offset = s_bootimg_info.ramdisk_offset + size;

		debugf("s_bootimg_info.dt_offset is 0x%llx\n", s_bootimg_info.dt_offset );
	}

	return 1;
}

static uint64_t boot_img_offset_v2(boot_img_hdr *hdr, uint64_t offset)
{
	struct boot_img_hdr_v1 *hdr_v1 = get_boot_img_hdr_v1(hdr);
	struct boot_img_hdr_v2 *hdr_v2 = get_boot_img_hdr_v2(hdr);
	uint64_t size;

	/* kernel image */
	s_bootimg_info.kernel_offset = offset + KERNL_PAGE_SIZE;
	size = PAD_SIZE(hdr->kernel_size, KERNL_PAGE_SIZE);
	if (0 == size) {
		errorf("kernel image should not be zero!\n");
		return 0;
	}

	debugf("s_bootimg_info.kernel_offset is 0x%llx\n", s_bootimg_info.kernel_offset );

	if( hdr->ramdisk_size) {
		/* ramdisk image */
		s_bootimg_info.ramdisk_offset = s_bootimg_info.kernel_offset + size;
		size = PAD_SIZE(hdr->ramdisk_size, KERNL_PAGE_SIZE);
		if (0 == size) {
			errorf("ramdisk image size should not be zero\n");
			return 0;
		}

		debugf("s_bootimg_info.ramdisk_offset is 0x%llx\n", s_bootimg_info.ramdisk_offset );
	}

	/* Fixme second stage */

	if (hdr_v1->recovery_dtbo_size) {
		/* recovery_dtbo_offset */
		s_bootimg_info.recovery_dtbo_offset = s_bootimg_info.ramdisk_offset + size;
		size = PAD_SIZE(hdr_v1->recovery_dtbo_size, KERNL_PAGE_SIZE);
		if (0 == size) {
			errorf("recover dtbo image size should not be zero\n");
			return 0;
		}

		debugf("s_bootimg_info.recovery_dtbo_offset is 0x%llx\n", s_bootimg_info.recovery_dtbo_offset);
	}

	/* dt image */
	if (hdr_v1->recovery_dtbo_size)
		s_bootimg_info.dt_offset = s_bootimg_info.recovery_dtbo_offset + size;
	else if (hdr->ramdisk_size)
		s_bootimg_info.dt_offset = s_bootimg_info.ramdisk_offset + size;

	debugf("s_bootimg_info.dt_offset is 0x%llx\n", s_bootimg_info.dt_offset );
	if (hdr_v2->dtb_addr != s_bootimg_info.dt_offset)
		debugf("Warning: dtb addr 0x%llx on boot img hdr\n", hdr_v2->dtb_addr);
	check_img_os_version(hdr->os_version);

	return 1;
}

static int parser_boot_image_header_v3(boot_img_hdr *hdr, uint64_t offset)
{
	uint64_t size;
	boot_img_hdr_v3 *hdr_v3 = (boot_img_hdr_v3 *)(hdr);

	/* judge generic kernel size */
	size = PAD_SIZE(hdr_v3->kernel_size, s_bootimg_info.page_size);
	if (size == 0) {
		errorf("bootimage: generic kernel size should not be zero!\n");
		return 0;
	}
	debugf("bootimage: generic kernel size is %d\n", hdr_v3->kernel_size);

	/* get generic kernel offset */
	s_bootimg_info.kernel_offset = offset + s_bootimg_info.page_size;
	debugf("bootimage: generic kernel offset is 0x%llx\n", s_bootimg_info.kernel_offset);

	/* judge generic ramdisk size */
	if (hdr_v3->ramdisk_size == 0) {
		errorf("bootimage: generic ramdisk size should not be zero!\n");
		return 0;
	}
	debugf("bootimage: generic ramdisk size is %d\n", hdr_v3->ramdisk_size);

	/* get generic ramdisk offset */
	s_bootimg_info.ramdisk_offset = s_bootimg_info.kernel_offset + size;
	debugf("bootimage: generic ramdisk offset is 0x%llx\n", s_bootimg_info.ramdisk_offset);
	check_img_os_version(hdr_v3->os_version);

	return 1;
}

static int parser_vendor_boot_image_header_v3(vendor_boot_img_hdr_v3 *vb_hdr_v3, uint64_t offset)
{
	uint64_t size = 0;

	s_bootimg_info.vendor_hdr_offset = offset;

	/* check vendor boot image header */
	if (memcmp(vb_hdr_v3->magic, VENDOR_BOOT_MAGIC, VENDOR_BOOT_MAGIC_SIZE)) {
		errorf("bad vendor boot image header, give up boot!\n");
		return 0;
	}

	/* judge vendor ramdisk size */
	size = PAD_SIZE(vb_hdr_v3->vendor_ramdisk_size, s_bootimg_info.page_size);
	if (size == 0) {
		errorf("vendor ramdisk size should not be zero!\n");
		return 0;
	}

	/* get vendor ramdisk size */
	s_bootimg_info.vendor_ramdisk_size = vb_hdr_v3->vendor_ramdisk_size;
	debugf("vendorbootimage: vendor ramdisk size is %lld\n", s_bootimg_info.vendor_ramdisk_size);

	/* get vendor ramdisk offset */
	s_bootimg_info.vendor_ramdisk_offset = s_bootimg_info.vendor_hdr_offset + s_bootimg_info.page_size;
	debugf("vendorbootimage: vendor ramdisk offset is 0x%llx\n", s_bootimg_info.vendor_ramdisk_offset);

	/* judge vendor dtb size */
	if (vb_hdr_v3->dtb_size == 0) {
		errorf("vendor dtb size should not be zero\n");
		return 0;
	}

	/* get vendor dtb size */
	s_bootimg_info.dt_size = vb_hdr_v3->dtb_size;
	debugf("vendorbootimage: vendor dt size is %lld\n", s_bootimg_info.dt_size);

	/* get vendor dtb offset */
	s_bootimg_info.dt_offset = s_bootimg_info.vendor_ramdisk_offset + size;
	debugf("vendorbootimage: vendor dt offset is 0x%llx\n", s_bootimg_info.dt_offset);

#ifdef CONFIG_BOARD_KERNEL_CMDLINE
	memset(vendorboot_cmdline, 0, VENDOR_BOOT_ARGS_SIZE);
	memcpy(vendorboot_cmdline, vb_hdr_v3->cmdline, VENDOR_BOOT_ARGS_SIZE);
#endif

	return 1;
}

static uint64_t boot_img_offset_v3(boot_img_hdr *hdr, uint64_t offset)
{
	int ret;
	char *vndr_boot_img_hdr;

#ifdef CONFIG_ANDROID_AB
	const char *ab_slot = g_env_slot;
	if (ab_slot && (strlen(ab_slot) + strlen(boot_v3) + 1) <= ARRAY_SIZE(boot_v3) &&
		(strlen(ab_slot)+ strlen(vendor_boot_v3) + 1) <= ARRAY_SIZE(vendor_boot_v3)) {
		strcat(boot_v3, ab_slot);
		strcat(vendor_boot_v3, ab_slot);
	}

	s_bootimg_info.bootimg_part = boot_v3;
#else
	/* header v3 page size is 4096 Byte */
	if (!strcmp("recovery", g_env_bootmode))
		s_bootimg_info.bootimg_part = recovery_v3;
	else
		s_bootimg_info.bootimg_part = boot_v3;
#endif
	s_bootimg_info.vendor_boot_part = vendor_boot_v3;
	s_bootimg_info.page_size = KERNEL_PAG_SIZE_V3;
	vndr_boot_img_hdr = (char *)(malloc_cache_aligned(KERNEL_PAG_SIZE_V3));
	if (vndr_boot_img_hdr == NULL) {
		errorf("malloc vndr_boot_img_hdr fail");
		goto err;
	}

	ret = parser_boot_image_header_v3(hdr, offset);
	if (!ret) {
		errorf("parser boot image header_v3 fail!\n");
		goto err;
	}

	/* read vendor boot image header */
	ret = common_raw_read(s_bootimg_info.vendor_boot_part, KERNEL_PAG_SIZE_V3, offset, vndr_boot_img_hdr);
	if (ret) {
		errorf("read %s header error!\n", s_bootimg_info.vendor_boot_part);
		goto err;
	}

	ret = parser_vendor_boot_image_header_v3((vendor_boot_img_hdr_v3 *)(vndr_boot_img_hdr), offset);
	if (!ret) {
		errorf("parser vendor boot image header_v3 fail!\n");
		goto err;
	}
	free(vndr_boot_img_hdr);
	return 1;

err:
	if (NULL != vndr_boot_img_hdr)
		free(vndr_boot_img_hdr);
	return 0;
}

static int parser_boot_image_header_v4(boot_img_hdr *hdr, uint64_t offset)
{
	uint64_t size;
	boot_img_hdr_v4 *hdr_v4 = (boot_img_hdr_v4 *)(hdr);

	/* judge generic kernel size */
	size = PAD_SIZE(hdr_v4->kernel_size, s_bootimg_info.page_size);
	if (size == 0) {
		errorf("bootimage: generic kernel size should not be zero!\n");
		return 0;
	}
	debugf("bootimage: generic kernel size is %d\n", hdr_v4->kernel_size);

	/* get generic kernel offset */
	s_bootimg_info.kernel_offset = offset + s_bootimg_info.page_size;
	debugf("bootimage: generic kernel offset is 0x%llx\n", s_bootimg_info.kernel_offset);

	/* judge generic ramdisk size */
	if (hdr_v4->ramdisk_size == 0) {
		s_bootimg_info.ramdisk_size = 0;
		dprintf(ALWAYS, "bootimage: generic ramdisk size is zero, maybe ramdisk in init_boot image side!\n");
		//return 0;
	} else {
		debugf("bootimage: generic ramdisk size is %d\n", hdr_v4->ramdisk_size);
		/* get generic ramdisk offset */
		s_bootimg_info.ramdisk_size = hdr_v4->ramdisk_size;
		s_bootimg_info.ramdisk_offset = s_bootimg_info.kernel_offset + size;
	}

	debugf("bootimage: generic ramdisk offset is 0x%llx\n", s_bootimg_info.ramdisk_offset);
	check_img_os_version(hdr_v4->os_version);

	return 1;
}

static int parser_vendor_boot_image_header_v4(vendor_boot_img_hdr_v4 *vb_hdr_v4, uint64_t offset)
{
	uint64_t size = 0;

	s_bootimg_info.vendor_hdr_offset = offset;

	/* check vendor boot image header */
	if (memcmp(vb_hdr_v4->magic, VENDOR_BOOT_MAGIC, VENDOR_BOOT_MAGIC_SIZE)) {
		errorf("bad vendor boot image header, give up boot!\n");
		return 0;
	}

	/* judge vendor ramdisk size */
	size = PAD_SIZE(vb_hdr_v4->vendor_ramdisk_size, s_bootimg_info.page_size);
	if (size == 0) {
		errorf("vendor ramdisk size should not be zero!\n");
		return 0;
	}

	/* get vendor ramdisk size */
	s_bootimg_info.vendor_ramdisk_size = vb_hdr_v4->vendor_ramdisk_size;
	debugf("vendorbootimage: vendor ramdisk size is %lld\n", s_bootimg_info.vendor_ramdisk_size);

	/* get vendor ramdisk offset */
	s_bootimg_info.vendor_ramdisk_offset = s_bootimg_info.vendor_hdr_offset + s_bootimg_info.page_size;
	debugf("vendorbootimage: vendor ramdisk offset is 0x%llx\n", s_bootimg_info.vendor_ramdisk_offset);

	/* judge vendor dtb size */
	if (vb_hdr_v4->dtb_size == 0) {
		errorf("vendor dtb size should not be zero\n");
		return 0;
	}

	/* get vendor dtb size */
	s_bootimg_info.dt_size = vb_hdr_v4->dtb_size;
	debugf("vendorbootimage: vendor dt size is %lld\n", s_bootimg_info.dt_size);

	/* get vendor dtb offset */
	size = PAD_SIZE(s_bootimg_info.vendor_ramdisk_size, s_bootimg_info.page_size);
	s_bootimg_info.dt_offset = s_bootimg_info.vendor_ramdisk_offset + size;
	debugf("vendorbootimage: vendor dt offset is 0x%llx\n", s_bootimg_info.dt_offset);

	/* judge vendor ramdisk table size */
	if (vb_hdr_v4->vendor_ramdisk_table_size == 0) {
		errorf("vendor ramdisk table size should not be zero\n");
		return 0;
	}

	/* get vendor ramdisk table size */
	s_bootimg_info.vendor_ramdisk_table_size = vb_hdr_v4->vendor_ramdisk_table_size;
	debugf("vendorbootimage: vendor ramdisk table size is %lld\n", s_bootimg_info.vendor_ramdisk_table_size);

	/* get vendor ramdisk table offset */
	size = PAD_SIZE(s_bootimg_info.dt_size, s_bootimg_info.page_size);
	s_bootimg_info.vendor_ramdisk_table_offset = s_bootimg_info.dt_offset + size;
	debugf("vendorbootimage: vendor ramdisk table offset is 0x%llx\n", s_bootimg_info.vendor_ramdisk_table_offset);

	/* get vendor bootconfig size */
	s_bootimg_info.vendor_bootconfig_size = vb_hdr_v4->bootconfig_size;
	debugf("vendorbootimage: vendor bootconfig size is %lld\n", s_bootimg_info.vendor_bootconfig_size);

	/* get vendor bootconfig offset */
	size = PAD_SIZE(s_bootimg_info.vendor_ramdisk_table_size, s_bootimg_info.page_size);
	s_bootimg_info.vendor_bootconfig_offset = s_bootimg_info.vendor_ramdisk_table_offset + size;
	debugf("vendorbootimage: vendor bootconfig offset is 0x%llx\n", s_bootimg_info.vendor_bootconfig_offset);

#ifdef CONFIG_BOARD_KERNEL_CMDLINE
	memset(vendorboot_cmdline, 0, VENDOR_BOOT_ARGS_SIZE);
	memcpy(vendorboot_cmdline, vb_hdr_v4->cmdline, VENDOR_BOOT_ARGS_SIZE);
#endif

	return 1;
}

static int parser_init_boot_image_header_v4(boot_img_hdr *hdr, uint64_t offset)
{
	uint64_t size;
	boot_img_hdr_v4 *hdr_init_boot = (boot_img_hdr_v4 *)(hdr);

	if (memcmp(hdr_init_boot->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
		panic("bad init boot image header, give up boot!\n");
		return 0;
	}

	size = PAD_SIZE(hdr_init_boot->ramdisk_size, s_bootimg_info.page_size);
	s_bootimg_info.initboot_ramdisk_size = hdr_init_boot->ramdisk_size;
	if (size == 0) {
		panic("init_bootimage: generic ramdisk size should not be zero!\n");
		return 0;
	}
	debugf("init_bootimage: generic ramdisk size is %lld\n", s_bootimg_info.initboot_ramdisk_size);

	/* get generic kernel offset */
	s_bootimg_info.initboot_ramdisk_offset = offset + s_bootimg_info.page_size;
	debugf("init_bootimage: generic ramdisk offset is 0x%llx\n", s_bootimg_info.initboot_ramdisk_offset);

	return 1;
}

static uint64_t boot_img_offset_v4(boot_img_hdr *hdr, uint64_t offset)
{
	int ret;
	char *vndr_boot_img_hdr = NULL;
	char *init_boot_img_hdr = NULL;

#ifdef CONFIG_ANDROID_AB
	const char *ab_slot = g_env_slot;
	if (ab_slot && (strlen(ab_slot) + strlen(boot_v3) + 1) <= ARRAY_SIZE(boot_v3) &&
		(strlen(ab_slot)+ strlen(vendor_boot_v3) + 1) <= ARRAY_SIZE(vendor_boot_v3) &&
		(strlen(ab_slot)+ strlen(init_boot_v3) + 1) <= ARRAY_SIZE(init_boot_v3)) {
		strcat(boot_v3, ab_slot);
		strcat(vendor_boot_v3, ab_slot);
		strcat(init_boot_v3, ab_slot);
	}

	s_bootimg_info.bootimg_part = boot_v3;
#else
	/* header v3 page size is 4096 Byte */
	if (!strcmp("recovery", g_env_bootmode))
		s_bootimg_info.bootimg_part = recovery_v3;
	else
		s_bootimg_info.bootimg_part = boot_v3;
#endif

	s_bootimg_info.vendor_boot_part = vendor_boot_v3;
	s_bootimg_info.init_boot_part = init_boot_v3;
	s_bootimg_info.page_size = KERNEL_PAG_SIZE_V3;
	vndr_boot_img_hdr = (char *)(malloc_cache_aligned(VENDOR_BOOT_HEADER_SIZE_V4));
	if (vndr_boot_img_hdr == NULL) {
		panic("malloc vndr_boot_img_hdr fail.\n");
		goto err;
	}

	ret = parser_boot_image_header_v4(hdr, offset);
	if (!ret) {
		panic("parser boot image header_v4 fail!\n");
		goto err;
	}

	/* read vendor boot image header */
	ret = common_raw_read(s_bootimg_info.vendor_boot_part, VENDOR_BOOT_HEADER_SIZE_V4, offset, vndr_boot_img_hdr);
	if (ret) {
		panic("read %s header error!\n", s_bootimg_info.vendor_boot_part);
		goto err;
	}

	ret = parser_vendor_boot_image_header_v4((vendor_boot_img_hdr_v4 *)(vndr_boot_img_hdr), offset);
	if (!ret) {
		panic("parser vendor boot image header_v4 fail!\n");
		goto err;
	}

	if (s_bootimg_info.ramdisk_size == 0) {
		init_boot_img_hdr = (char *)(malloc_cache_aligned(INIT_BOOT_PAG_SIZE_V4));
		if (init_boot_img_hdr == NULL) {
			panic("malloc init_boot_img_hdr fail.\n");
			goto err;
		}

		/* read init boot image header */
		ret = common_raw_read(s_bootimg_info.init_boot_part, INIT_BOOT_PAG_SIZE_V4, offset, init_boot_img_hdr);
		if (ret) {
			panic("read %s header error!\n", s_bootimg_info.init_boot_part);
			goto err;
		}

		ret = parser_init_boot_image_header_v4(init_boot_img_hdr, offset);
		if (!ret) {
			panic("parser init boot image header_v4 fail!\n");
			goto err;
		}

		free(init_boot_img_hdr);
	}

	free(vndr_boot_img_hdr);
	return 1;

err:
	if (NULL != vndr_boot_img_hdr)
		free(vndr_boot_img_hdr);
	if (NULL != init_boot_img_hdr)
		free(init_boot_img_hdr);
	return 0;
}

typedef uint64_t (*boot_img_offset_hdl)(boot_img_hdr *, uint64_t);
static boot_img_offset_hdl boot_img_offset_hdl_tbl[] = {
	boot_img_offset_v01,
	boot_img_offset_v01,
	boot_img_offset_v2,
	boot_img_offset_v3,
	boot_img_offset_v4,
};

extern int enter_sysdump_flag;
uint64_t _get_kernel_ramdisk_dt_offset(boot_img_hdr * hdr, const char *partition)
{
	uint64_t offset = 0;

#if (defined SPRD_SECBOOT) && (!(defined SPRD_VBOOT_V2))
	offset = 512;/**header of image size is 512B**/
#endif

	s_bootimg_info.hdr_offset = offset;
	s_bootimg_info.page_size = KERNL_PAGE_SIZE;
	if (0 != common_raw_read(partition, KERNEL_PAG_SIZE_V3, (uint64_t)offset, (char *)hdr)) {
		errorf("read %s header error!\n", partition);
		return 0;
	}

	/* check bootimage header */
	if (0 != memcmp(hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
		errorf("bad boot image header, give up boot!!!!\n");
		return 0;
	}

	debugf("boot_img_hdr header_version: %d\n", hdr->header_version);
	if (hdr->header_version >= ARRAY_SIZE(boot_img_offset_hdl_tbl)) {
		if (!strcmp(partition, RECOVERY_PART)) {
			debugf("recovery header version is 0");
			hdr->header_version = BOOT_HEADER_VERSION_0;
		} else
			hdr->header_version = BOOT_HEADER_VERSION_ONE;
	}

	if (!boot_img_offset_hdl_tbl[hdr->header_version](hdr, offset))
		return 0;

#if (defined SPRD_SYSDUMP)
	if(enter_sysdump_flag){
		debugf("Now doing sysdump ,go back here .\n");
		return 1;
	}
#endif

	return 1;
}

/*
*	Function for load dtbo and merge into dtb
*
*	The 64-bit and 32-bit parameters "sprd,sc-id" are the same,
*	but they will not appear in the same DTB file.
*
*/
static int look_for_matching_device_from_dt(char *fdt_temp, dt_img_type dt_type)
{
	char *device_string = NULL;
	char *device_string_t = NULL;
	char delim[] = " ";
	char *dts_plat_token = NULL;
	char *board_plat_token = NULL;
	char *board_info = NULL;
	char *board_info_t = NULL;
	char *saved_p0 = NULL;
	char *saved_p1 = NULL;
	int nodeoffset = fdt_path_offset(fdt_temp, "/");
	int ret = 0;

	if(dt_type == UNDEFINED_TYPE) {
		errorf("dtb_type has not been set\n");
		return -1;
	}
	if (nodeoffset == -FDT_ERR_NOTFOUND)
		return -1;

	board_info = strdup(SPRD_BOARD_INFO_ID);
	board_info_t = board_info;

	device_string = strdup(fdt_getprop(fdt_temp, nodeoffset, "sprd,sc-id", NULL));
	device_string_t = device_string;
	if (!device_string) {
		debugf("Cannot find prop name \"sprd,sc-id\" in kernel dtb\n");
		free(board_info);
		return -1;
	}

	dprintf(INFO, "Find kernel dtb prop name sprd,sc-id=%s, g_fdt_blob sprd,sc-id=%s\n", device_string, board_info);

	/*match dtbo*/
	if (DTBO_TYPE == dt_type) {
		if (!strcmp(device_string, board_info)) {
			dprintf(ALWAYS, "Matches the corresponding dtbo in multiple DTBO\n");
			goto end;
		} else {
			dprintf(ALWAYS, "This dtbo file is not match\n");
			ret = -1;
			goto end;
		}
	}

	/*match dtb*/
	if (DTB_TYPE == dt_type) {
		/*PLATFORM ID for dtb\dtbo*/
		dts_plat_token = strtok_r(device_string_t, delim, &saved_p0);
		board_plat_token = strtok_r(board_info_t, delim, &saved_p1);
		if (!dts_plat_token || !board_plat_token) {
			errorf("Property sprd,sc-id is not formatted correctly\n");
			ret = -1;
			goto end;
		}

		if (!strcmp(board_plat_token, dts_plat_token))
			dprintf(ALWAYS,"Matches the corresponding dtb in multiple DTB\n");
		else {
			dprintf(ALWAYS,"This dtb file is not match\n");
			ret = -1;
		}
	}
end:
	free(device_string);
	free(board_info);
	return ret;
}

#ifdef CONFIG_MATCH_DTBO_BY_BOARDID
__weak int sprd_get_dtboinfo(void)
{
#ifdef CONFIG_HIGHFLASH_DTBO
	int hr_version = 0;

	hr_version = sprd_get_tpic_version();
	if (hr_version == 1) {
		return CONFIG_BOARDID_UMB9230S;
	} else if (hr_version == 2) {
		return CONFIG_BOARDID_CUSTOMER;
	} else {
		return CONFIG_BOARDID_DEFAULTID;
	}
#else
	return CONFIG_BOARDID_DEFAULTID;
#endif
}

/* Function for load dtbo by board id */
static int look_for_matching_by_boardid(struct dt_table_entry *entry, int *mt)
{
	static int board_vid = -1;
	static int board_id = -1;
	int ret = -1;
	int tmp;

	if (-1 == board_vid) {
		if (!common_raw_read("miscdata", SET_VIRTUAL_BOARD_ID_LEN,
							 SET_VIRTUAL_BOARD_ID_OFFSET, &tmp)) {
			if (SET_VIRTUAL_BOARD_ID_MAGIC == (tmp & 0xFFFF0000)) {
				board_vid = tmp & 0xFFFF;
				debugf("got virtual board id 0x%x\n", board_vid);
			} else
				board_vid = -2;
		}
	}

	if (-1 == board_id) {
		board_id = sprd_get_dtboinfo();
		debugf("got board id 0x%x\n", board_id);
	}

	/* comparison of choice */
	if (be32_to_cpu(entry->id) == board_vid) {
		*mt = 0;
		debugf("matches dtbo entry by virtual id(0x%x)!\n", be32_to_cpu(entry->id));
		return 0;
	} else if (be32_to_cpu(entry->id) == board_id) {
		*mt = 1;
		debugf("matches dtbo entry(id:0x%x)\n", be32_to_cpu(entry->id));
		return 0;
	} else
		debugf("mismatching dtbo entry(id:0x%x)\n", be32_to_cpu(entry->id));

	return ret;
}
#endif

/*
 * Function for load dtbo and merge into dtb
 */
void load_and_merge_dtbo(const char *partition, uchar *dt_start_addr)
{
	int ret = 0, idx = 0;
	int offset = 0;
	int count,i;
	struct dt_table_header __aligned(ARCH_DMA_MINALIGN) table_header;
	struct dt_table_entry *entry=NULL;
	struct fdt_header *dtbo_header = NULL;
	u8 *dtbo_start_addr = NULL;
	u8 *dtbo_addr = NULL;
	struct boot_img_hdr_v1 __aligned(ARCH_DMA_MINALIGN) hdr;
	uint64_t hdr_offset = s_bootimg_info.hdr_offset + sizeof(boot_img_hdr);
#ifdef CONFIG_MATCH_DTBO_BY_BOARDID
	struct dt_table_entry *sav_entry_v = NULL, *sav_entry = NULL;
	int hit_cnt = 0;
	int mt = -1;
#endif

	debugf("load dtbo from %s\n", partition);
	if (0 != common_raw_read(partition, sizeof(struct boot_img_hdr_v1), hdr_offset, (char *)&hdr)) {
		errorf("read %s header error!\n", partition);
		return;
	}

	debugf("recovery_dtbo_size: 0x%x\n", hdr.recovery_dtbo_size);
	debugf("recovery_dtbo_offset: 0x%llx\n", hdr.recovery_dtbo_offset);
	debugf("header_size: 0x%x\n", hdr.header_size);

	/* read dtbo table header */
	if (0 != common_raw_read(partition, (uint64_t)sizeof(struct dt_table_header), hdr.recovery_dtbo_offset, (char *)&table_header)) {
		errorf("read dtbo fail\n");
		goto error;
	}

	/* dtbo image header check */
	if (be32_to_cpu(table_header.magic) != DT_TABLE_MAGIC) {
		errorf("invalid dtbo partition\n");
		goto error;
	}

	if (be32_to_cpu(table_header.total_size) == 0)
		goto error;

	/* dtbo alloc memory*/
	dtbo_start_addr = malloc_cache_aligned(be32_to_cpu(table_header.total_size));
	if (NULL == dtbo_start_addr) {
		errorf("malloc size=%dfor dt entrys fail\n", be32_to_cpu(table_header.total_size));
		goto error;
	}

	/* load dtbo image */
	if (0 != common_raw_read(partition, (uint64_t)be32_to_cpu(table_header.total_size), hdr.recovery_dtbo_offset, (char *)dtbo_start_addr)) {
		errorf("read dtbo fail\n");
		goto error;
	}

	if (be32_to_cpu(table_header.dt_entry_count) == 0) {
		errorf("not found dtbo !\n");
		goto error;
	}

	count = be32_to_cpu(table_header.dt_entry_count);
	entry = (struct dt_table_entry *)(dtbo_start_addr +
				be32_to_cpu(table_header.dt_entries_offset));

	for (i = 0; i < count; i++) {
#ifdef CONFIG_MATCH_DTBO_BY_BOARDID
		if (!look_for_matching_by_boardid(entry, &mt)) { /* hit */
			if (!sav_entry_v && (mt == 0)) { /* virtual board id hit */
				sav_entry_v = entry;
				idx = i;
			} else if (!sav_entry && (mt == 1)) {
				sav_entry = entry;
				idx = i;
			}
		}

		if (!sav_entry_v) {
			if ((i < count - 1) || !sav_entry) {
				entry++;
				continue;
			} else {
				entry = sav_entry;
			}
		} else {
			entry = sav_entry_v;
		}

		hit_cnt++;
#endif

		offset = be32_to_cpu(entry->dt_offset);
		debugf("offset = 0x%x size=0x%x\n", offset, be32_to_cpu(entry->dt_size));

		dtbo_addr = (u8 *)malloc(be32_to_cpu(entry->dt_size));
		if (NULL == dtbo_addr) {
			errorf("malloc size=%d for dt entrys fail\n", be32_to_cpu(entry->dt_size));
			goto error;
		}
		memcpy(dtbo_addr, dtbo_start_addr + offset, be32_to_cpu(entry->dt_size));
		dtbo_header = (struct fdt_header *)dtbo_addr;
		if (fdt_check_header(dtbo_header) != 0) {
			errorf("image is not a fdt\n");
			goto error;
		}

#ifndef CONFIG_MATCH_DTBO_BY_BOARDID
		idx = i;
		if (img_os_version >= VERSION_R) {
			if (look_for_matching_device_from_dt((char *)dtbo_addr, DTBO_TYPE)) {
				free(dtbo_addr);
				dtbo_addr = NULL;
				entry++;
				continue;
			}
		}
#endif

		ret = fdt_overlay_apply(dt_start_addr, dtbo_addr);
		if (ret) {
			errorf("fdt_overlay_apply(): %s\n", fdt_strerror(ret));
			goto error;
		}
		free(dtbo_addr);
		dtbo_addr = NULL;
		/* Only need to overlay one dtbo */
		break;
	}
error:
	if (NULL != dtbo_start_addr) {
		free(dtbo_start_addr);
	}
	if (NULL != dtbo_addr) {
		free(dtbo_addr);
	}
}

/*
 * Function for do memory defragment and load dtb
 */
void merge_dtbo(uchar *dt_start_addr)
{
#ifdef CONFIG_OF_LIBFDT_OVERLAY
	struct dt_table_header *table_header;
	struct dt_table_entry *entry=NULL;
	struct dt_table_entry *cur_entry=NULL;
	struct fdt_header __aligned(ARCH_DMA_MINALIGN) dtbo_header;
	char __aligned(ARCH_DMA_MINALIGN) data[64];
	char *dtbo_addr = NULL;
	int offset = 0;
	int ret = 0, idx = 0;
	int count,size,i;
#ifdef CONFIG_MATCH_DTBO_BY_BOARDID
	struct dt_table_entry *sav_entry_v = NULL, *sav_entry = NULL;
	int hit_cnt = 0;
	int mt = -1;
#endif

	const char dtbo_ab[8] = "dtbo";
#ifdef CONFIG_ANDROID_AB
	get_slot_ab(dtbo_ab, NULL);
#endif
	debugf("[%s] merging dtb/dtbo ...\n", __func__);

	if (0 != common_raw_read(dtbo_ab, sizeof(*table_header), 0, &data[0])) {
		errorf("read dtbo fail\n");
		goto error;
	}
	table_header = (struct dt_table_header *)data;
	if (be32_to_cpu(table_header->magic) != DT_TABLE_MAGIC) {
		errorf("invalid dtbo partition\n");
		goto error;
	}
	if (be32_to_cpu(table_header->dt_entry_count)==0)
		goto error;
	size =be32_to_cpu(table_header->dt_entry_size) *
		 be32_to_cpu(table_header->dt_entry_count);
	entry = malloc_cache_aligned(size);
	if (NULL == entry) {
		errorf("malloc size=%dfor dt entrys fail\n",size);
		goto error;
	}
	offset = be32_to_cpu(table_header->dt_entries_offset);
	count = be32_to_cpu(table_header->dt_entry_count);

	if (0 != common_raw_read(dtbo_ab, size, offset, (char *)entry)) {
		errorf("read dt entries fail\n");
		goto error;
	}

	cur_entry = entry;
	for (i=0; i < count; i++) {
#ifdef CONFIG_MATCH_DTBO_BY_BOARDID
		if (!look_for_matching_by_boardid(cur_entry, &mt)) { /* hit */
			if (!sav_entry_v && (mt == 0)) { /* virtual board id hit */
				sav_entry_v = cur_entry;
				idx = i;
			} else if (!sav_entry && (mt == 1)) {
				sav_entry = cur_entry;
				idx = i;
			}
		}

		if (!sav_entry_v) {
			if ((i < count - 1) || !sav_entry) {
				cur_entry++;
				continue;
			} else {
				cur_entry = sav_entry;
			}
		} else {
			cur_entry = sav_entry_v;
		}

		hit_cnt++;
#endif
		debugf("%s : entry id is 0x%x, offset = 0x%x size=0x%x\n",
			__func__, be32_to_cpu(cur_entry->id), be32_to_cpu(cur_entry->dt_offset), be32_to_cpu(cur_entry->dt_size));

		offset = be32_to_cpu(cur_entry->dt_offset);
		size = sizeof(struct fdt_header);
		if (0 != common_raw_read(dtbo_ab, size, offset, (char *)&dtbo_header)) {
			errorf("read dtbo fail\n");
			goto error;
		}

		if (fdt_check_header(&dtbo_header) != 0) {
			errorf("image is not a fdt\n");
			goto error;
		}
		size = fdt_totalsize(&dtbo_header);
		dtbo_addr = calloc(1, size);
		if (NULL == dtbo_addr) {
			errorf("malloc size %d for dtbo fail\n", size);
			goto error;
		}
		if (0 != common_raw_read(dtbo_ab, size, offset, dtbo_addr)) {
			errorf("read dtbo fail\n");
			goto error;
		}

#ifndef CONFIG_MATCH_DTBO_BY_BOARDID
		idx = i;
		if (img_os_version >= VERSION_R) {
			if (look_for_matching_device_from_dt(dtbo_addr, DTBO_TYPE)) {
				free(dtbo_addr);
				dtbo_addr = NULL;
				cur_entry++;
				continue;
			}
		}
#endif

		ret = fdt_overlay_apply(dt_start_addr, dtbo_addr);
		if (ret) {
			errorf("fdt_overlay_apply(): %s\n", fdt_strerror(ret));
			goto error;
		}

		free(dtbo_addr);
		dtbo_addr = NULL;
		/* Only need to overlay one dtbo */
		g_DtboIndex = idx;
		break;
	}
error:
	if (NULL != entry)
		free(entry);
	if (NULL != dtbo_addr)
		free(dtbo_addr);

#ifdef CONFIG_MATCH_DTBO_BY_BOARDID
	if (!hit_cnt) {
		int key_code;
		lcd_printf("Merge dtbo fail, press volume up or volume down key to reboot into fastboot\n");
		errorf("Merge dtbo fail, press key to reboot into fastboot\n");
		do {
			udelay(50 * 1000);
			key_code = board_key_scan();
			if (key_code == KEY_VOLUMEDOWN || key_code == KEY_VOLUMEUP || key_code == KEY_HOME
				|| key_code == (KEY_VOLUMEDOWN+KEY_VOLUMEUP))
				break;
		} while (1);
		//reboot_devices(CMD_FASTBOOT_MODE);
		fastboot_mode();
	}
#endif
#endif
}

/**
 * merge_bootargs - Function for bootargs in DTB merge with bootargs_ext in DTBO
 * @fdt_blob: Base Device Tree blob
 *
 * This function is used to solve BUG 1458956, Make sure that the node contents
 * changed in the overlay are not allowed to change after the merge.
 *
 * returns:
 *      0 on success
 *      Negative error code on failure
 */
int merge_bootargs(u8 *fdt_blob)
{
	int nodeoffset;
	const char *path = NULL;
	char *path_copy = NULL;
	char *path_copy_t = NULL;
	char *prop = NULL;
	char *prop_name = NULL;
	char *prop_name_t = NULL;
	const char *path_ext = NULL;
	char *path_copy_ext = NULL;
	char *path_copy_ext_t = NULL;
	char *prop_ext = NULL;
	char *prop_ext_name = NULL;
	char *prop_ext_name_t = NULL;
	char *saved_ext_ptr = NULL;
	char *saved_copy_ptr = NULL;
	char *saved_p0 = NULL;
	char *saved_p1 = NULL;
	int ret = -1;

	nodeoffset = fdt_path_offset(fdt_blob, "/chosen");
	if (nodeoffset < 0) {
		errorf("merge_bootargs: cann't find chosen");
		goto end;
	}

	path = fdt_getprop(fdt_blob, nodeoffset, "bootargs", NULL);
	if (path == NULL) {
		errorf("bootargs is null\n");
		goto end;
	}

	path_ext = fdt_getprop(fdt_blob, nodeoffset, "bootargs_ext", NULL);
	if (path_ext == NULL) {
		debugf("The Bootargs_ext node does not exist\n");
		ret = 0;
		goto end;
	}

	path_copy_ext = strdup(path_ext);
	if (path_copy_ext == NULL) {
		errorf("pathextcopy string copy failed!\n");
		goto end;
	}
	path_copy_ext_t = path_copy_ext;
	debugf("bootargs: %s\n", path);
	debugf("bootargs_ext: %s\n", path_copy_ext);
	if(!strlen(path_copy_ext)) {
		debugf("bootargs_ext is empty, so don't need merge bootargs\n");
		ret = 0;
		goto end;
	}

	while ((prop_ext = strtok_r(path_copy_ext, " ", &saved_ext_ptr)) != NULL) {
		path_copy = strdup(path);
		path_copy_ext = NULL;
		if (path_copy == NULL) {
			errorf("pathcopy string copy failed!\n");
			goto end;
		}
		path_copy_t = path_copy;
		prop_ext_name_t = strdup(prop_ext);
		if (prop_ext_name_t == NULL) {
			errorf("prop_ext string copy failed!\n");
			goto end;
		}
		prop_ext_name = strtok_r(prop_ext, "=", &saved_p0);
		while ((prop = strtok_r(path_copy, " ", &saved_copy_ptr)) != NULL) {
			path_copy = NULL;
			prop_name_t = strdup(prop);
			if (prop_name_t == NULL) {
				errorf("prop string copy failed!\n");
				goto end;
			}
			prop_name = strtok_r(prop, "=", &saved_p1);
			if (!strcmp(prop_name, prop_ext_name)) {
				debugf("replace info : prop: %s, prop_ext: %s\n", prop_name_t, prop_ext_name_t);
				ret = fdt_chosen_bootargs_replace(fdt_blob, prop_name_t, prop_ext_name_t);
				if (ret) {
					errorf("bootargs_ext replace to bootargs failed\n");
					goto end;
				}
				free(prop_name_t);
				prop_name_t = NULL;
				break;
			}
			free(prop_name_t);
			prop_name_t = NULL;
		}

		if (NULL == prop) {
			debugf("append cmdline: %s\n", prop_ext_name_t);
			ret = fdt_chosen_bootargs_append(fdt_blob, prop_ext_name_t, 1);
			if (ret) {
				errorf("bootargs_ext apppend to bootargs failed\n");
				goto end;
			}
		}
		free(prop_ext_name_t);
		prop_ext_name_t = NULL;
		free(path_copy_t);
		path_copy_t = NULL;
	}

end:
	if (NULL != path_copy_t)
		free(path_copy_t);
	if (NULL != path_copy_ext_t)
		free(path_copy_ext_t);
	if (NULL != prop_name_t)
		free(prop_name_t);
	if (NULL != prop_ext_name_t)
		free(prop_ext_name_t);
	return ret;
}

int load_fixup_dt_img(const char *partition, uchar **dt_start_addr) {
	u8 *fdt_blob = NULL;
	struct dt_table_t __aligned(ARCH_DMA_MINALIGN) table;
	char * header = NULL;
	struct dt_entry_t *dt_entry_ptr;
	uint64_t size = 0;
	uint64_t fdt_size = 0;
	uint64_t fdt_offset = 0;
	int ret = -1;
	uchar *dt_end_addr;
	int auto_mem_num = 0;
	struct boot_img_hdr *hdr;
	struct boot_img_hdr_v2 *hdr_v2 = NULL;
	struct dt_table_header *table_header;
	struct dt_table_entry *entry = NULL;
	struct dt_table_entry *cur_entry = NULL;
	struct fdt_header  __aligned(ARCH_DMA_MINALIGN) dtb_header;
	char __aligned(ARCH_DMA_MINALIGN) data[64];
	uint64_t offset_temp = 0;
	int count,i;

	hdr = get_boot_img_hdr();
	if (hdr) {
		hdr_v2 = get_boot_img_hdr_v2(hdr);
	}
	if (BOOT_HEADER_VERSION_THREE == hdr->header_version ||
			BOOT_HEADER_VERSION_FOUR == hdr->header_version) {
		size = s_bootimg_info.dt_size;
		fdt_offset = s_bootimg_info.dt_offset;
	} else if (!hdr_v2 || !((BOOT_HEADER_VERSION_TWO == hdr->header_version)
			&& (hdr_v2->dtb_addr && hdr_v2->dtb_size))) {
		size = sizeof(struct dt_table_t);
		if (0 != common_raw_read(partition, size, (uint64_t)(s_bootimg_info.dt_offset), (char *)&table)) {
			errorf("read dt image table header fail\n");
			goto error;
		}

		/* Validate the device tree table header */
		if((table.magic != SPRD_DT_MAGIC) || (table.version != SPRD_DT_VERSION)) {
			errorf("Cannot validate Device Tree Table(magic%x,version%d) on %s\n",
				table.magic, table.version, partition);
			goto error;
		}

		size = sizeof(struct dt_table_t) + sizeof(struct dt_entry_t) * (uint64_t)table.num_of_entries;
		header = (char *)malloc_cache_aligned(size);
		if (NULL == header) {
			errorf("malloc size %lld for dt header fail\n", size);
			goto error;
		}
		if (0 != common_raw_read(partition, size,  (uint64_t)(s_bootimg_info.dt_offset), header)) {
			errorf("read dt image table and entries header fail\n");
			goto error;
		}

		/* Calculate the offset of device tree within device tree table */
		dt_entry_ptr = fdt_get_entry_ptr_by_table((struct dt_table_t *)header);
		if(NULL == dt_entry_ptr) {
			errorf("Getting device tree address failed\n");
			goto error;
		}

		size = dt_entry_ptr->dt_size;
		fdt_offset = s_bootimg_info.dt_offset + dt_entry_ptr->dt_offset;
	} else {
		fdt_offset = s_bootimg_info.dt_offset;
		size = hdr_v2->dtb_size;
		if (fdt_offset != hdr_v2->dtb_addr)
			debugf("Warning: dtb addr 0x%llx on boot img hdr\n", hdr_v2->dtb_addr);
		debugf("v2 header fdt_offset:0x%llx size:0x%llx\n", fdt_offset, size);
	}

	if (img_os_version >= VERSION_R) {
		if (0 != common_raw_read(partition, sizeof(*table_header), fdt_offset, data)) {
			errorf("read dtb table header fail\n");
			goto error;
		}
		table_header = (struct dt_table_header *)data;
		if (be32_to_cpu(table_header->magic) != DT_TABLE_MAGIC) {
			errorf("invalid dtb partition\n");
			goto error;
		}
		if (be32_to_cpu(table_header->dt_entry_count) == 0)
			goto error;
		size = (uint64_t) (be32_to_cpu(table_header->dt_entry_size) *
					be32_to_cpu(table_header->dt_entry_count));
		entry = malloc_cache_aligned(size);
		if (NULL == entry) {
			errorf("malloc size=%lld for dt entrys fail\n",size);
			goto error;
		}
		offset_temp = be32_to_cpu(table_header->dt_entries_offset);
		count = be32_to_cpu(table_header->dt_entry_count);

		if (0 != common_raw_read(partition, (uint64_t)size, (uint64_t)(fdt_offset + offset_temp), (char *)entry)) {
			errorf("read dt entries fail\n");
			goto error;
		}

		for (i = 0, cur_entry = entry; i < count; i++, cur_entry++) {
			offset_temp = be32_to_cpu(cur_entry->dt_offset);
			size = sizeof(struct fdt_header);
			if (0 != common_raw_read(partition, (uint64_t)size, (uint64_t)(fdt_offset + offset_temp), (char *)&dtb_header)) {
				errorf("read dtb header fail\n");
				goto error;
			}

			if (fdt_check_header(&dtb_header) != 0) {
				errorf("image is not a fdt\n");
				continue;
			}
			size = fdt_totalsize(&dtb_header);
			debugf("offset = 0x%llx size=0x%llx\n", fdt_offset + offset_temp, size);
			/* malloc reserve FDT_ADD_SIZE for fdt fixup */
			fdt_blob = malloc_cache_aligned(size + FDT_ADD_SIZE);
			if (NULL == fdt_blob) {
				errorf("malloc size %lld for fdt_blob fail\n", size + FDT_ADD_SIZE);
				goto error;
			}
			g_fdt_blob = fdt_blob;
			if (0 != common_raw_read(partition, (uint64_t)size, (uint64_t)(fdt_offset + offset_temp), (char *)fdt_blob)) {
				errorf("read dtb fail\n");
				goto error;
			}

			ret = look_for_matching_device_from_dt((char *)fdt_blob, DTB_TYPE);
			if (0 != ret) {
				free(fdt_blob);
				fdt_blob = NULL;
			} else
				break;
		}

		if (-1 == ret) {
			errorf("No matching DTB could be found\n");
			goto error;
		}
	} else {
		/* malloc reserve FDT_ADD_SIZE for fdt fixup */
		fdt_blob = (u8 *)malloc_cache_aligned(size + FDT_ADD_SIZE);
		g_fdt_blob = fdt_blob;
		if (NULL == fdt_blob) {
			errorf("malloc size 0x%llx for fdt_blob fail\n", size + FDT_ADD_SIZE);
			goto error;
		} else
			debugf("malloc size for fdt: 0x%llx\n", size + FDT_ADD_SIZE);
		if (0 != common_raw_read(partition, (uint64_t)size, (uint64_t)fdt_offset, (char *)fdt_blob)) {
			errorf("dt entry size read error!\n");
			goto error;
		}

		if (fdt_check_header(fdt_blob) != 0) {
			errorf("image is not a fdt\n");
			goto error;
		}
	}
	fdt_size = fdt_totalsize(fdt_blob);
	ret = fdt_open_into(fdt_blob, fdt_blob, fdt_size + FDT_ADD_SIZE);
	if (0 != ret) {
		errorf("libfdt fdt_open_into(): %s\n", fdt_strerror(ret));
		goto error;
	}
	ret = fdt_fixup_memory_region(fdt_blob, &auto_mem_num);
	if(ret < 0)
		goto error;
	fdt_size = fdt_totalsize(fdt_blob);
	size = PAD_SIZE(fdt_size, s_bootimg_info.page_size);
	if (0 == ret && 0 != auto_mem_num) {
		if(0 != get_dt_end_addr(fdt_blob , &dt_end_addr))
			goto end;
		*dt_start_addr = dt_end_addr - size;
	}
end:
	memcpy(*dt_start_addr, fdt_blob, size);
	ret = 0;
error:
	if (NULL != header)
		free(header);
	if (NULL != fdt_blob)
		free(fdt_blob);
	if (NULL != entry)
		free(entry);
	return ret;
}

static int load_kernel_image(boot_img_hdr *hdr, const char *partition)
{
	uint64_t size;
	char kernel_header[2];
	size = PAD_SIZE(hdr->kernel_size, s_bootimg_info.page_size);
	if (0 == size) {
		errorf("kernel image should not be zero!\n");
		return 0;
	}

	/* read kernel image head, determine if it is a compressed image header */
	if (0 != common_raw_read(partition, sizeof(kernel_header), s_bootimg_info.kernel_offset, (char *)kernel_header)) {
		errorf("%s kernel read error!\n", partition);
		return 0;
	}

	if ((kernel_header[0] == '\x1f') && (kernel_header[1] == '\x8b')) {
		uint64_t buf_base;
		uint64_t buf_size;
		uint64_t max_size;
		uint64_t kernel_size = hdr->kernel_size;

		// ToDo: find a implementation for gunzip
		debugf("The kernel is in GZIP format!\n");
#ifdef CONFIG_DTS_MEM_LAYOUT
		/* get buffer size */
		if (get_buffer_base_size_from_dt("heap@4", &buf_base, &buf_size)) {
			errorf("get buffer error\n");
			return 0;
		}
#else
		buf_base = FB_BUF_ADDR;
		buf_size = FB_BUF_SIZE;
#endif
		if (size > buf_size) {
			errorf("kernel size > buffer size\n");
			return 0;
		}

		/* read compressed IMG into temporary buffer */
		if (0 != common_raw_read(partition, size, s_bootimg_info.kernel_offset, (char *)buf_base)) {
			errorf("%s kernel read error!\n", partition);
			return 0;
		}

		/* get partition max size */
		if (get_img_partition_size(partition, &max_size)) {
			errorf("get %s partition size error!\n", partition);
			return 0;
		}

		/* decompress the compressed kernel to the kernel address */
		if (gunzip((void *)KERNEL_ADR, (int)max_size, (uchar *)buf_base, &kernel_size)) {
			errorf("Gunzip kernel img error!\n");
			return 0;
		}
	} else if ((kernel_header[0] == '\x78') && (kernel_header[1] == '\x9c')) {
		//decompress_data zlib
		debugf("The kernel is in zlib format!\n");

		uint64_t max_size;
		uint64_t kernel_size = hdr->kernel_size;

		/* get buffer size */
		//FB_BUF_ADDR=0x82000000
		//FB_BUF_SIZE=0x10000000
		if (size > FB_BUF_SIZE) {
			errorf("kernel size > buffer size\n");
			return 0;
		}

		/* read compressed IMG into temporary buffer */
		if (0 != common_raw_read(partition, size, s_bootimg_info.kernel_offset, (char *)FB_BUF_ADDR)) {
			errorf("%s kernel read error!\n", partition);
			return 0;
		}

		/* get partition max size */
		if (get_img_partition_size(partition, &max_size)) {
			errorf("get %s partition size error!\n", partition);
			return 0;
		}

		/* decompress the compressed kernel to the kernel address */
		if (decompress_data((void *)FB_BUF_ADDR, (int)size, (void *)KERNEL_ADR, max_size)) {
			errorf("Zlib kernel img error!\n");
			return 0;
		}
	} else {
		debugf("The kernel is in raw format!\n");
		/* read kernel image */
		if (0 != common_raw_read(partition, size, s_bootimg_info.kernel_offset, (char *)KERNEL_ADR)) {
			errorf("%s kernel read error!\n", partition);
			return 0;
		}
	}
	dprintf(ALWAYS,"%s kernel read OK, page size = 0x%llx, locate to 0x%lx \n", partition, size, (ulong)KERNEL_ADR);

	return 1;
}

/* Simple real checksum for bootconfig */
static int checksum(unsigned char *buf, int len)
{
        int i, sum = 0;

        for (i = 0; i < len; i++)
                sum += buf[i];

        return sum;
}

static int read_vendor_bootconfig(uchar *ramdisk_addr)
{
	int ret;

	if (s_bootimg_info.vendor_bootconfig_size != 0) {
		ret = common_raw_read(s_bootimg_info.vendor_boot_part, s_bootimg_info.vendor_bootconfig_size,
				s_bootimg_info.vendor_bootconfig_offset, ramdisk_addr);
		if (ret != 0) {
			errorf("%s bootconfig read error!\n", s_bootimg_info.vendor_boot_part);
			return 0;
		}
	}
	debugf("%s bootconfig read OK, size = %lld, locate to %p!\n",
		s_bootimg_info.vendor_boot_part, s_bootimg_info.vendor_bootconfig_size, ramdisk_addr);
	return 1;
}

uchar *add_bootconfig_trailer(uchar *ramdisk_addr)
{
	char bootconfig_trailer[BOOTCONFIG_TRAILER_SIZE];
	uint32_t bootconfig_size_v4 = 0;
	uint32_t bootconfig_checksum_v4 = 0;
	uint8_t bootconfig_gap = 0;

	bootconfig_gap = s_bootimg_info.vendor_bootconfig_size % 4;
	if(bootconfig_gap!=0){
		bootconfig_gap = 4 - bootconfig_gap;
		bootconfig_size_v4 = s_bootimg_info.vendor_bootconfig_size + bootconfig_gap;
	}else
		bootconfig_size_v4 = s_bootimg_info.vendor_bootconfig_size;

	/*Add the bootconfig trailer to end of the parameters */
	if(bootconfig_gap!=0)
		memset((ramdisk_addr + s_bootimg_info.vendor_bootconfig_size),'\0',bootconfig_gap);
	bootconfig_checksum_v4 = checksum((unsigned char *)ramdisk_addr, bootconfig_size_v4);
	ramdisk_addr = ramdisk_addr + bootconfig_size_v4;

	memset(bootconfig_trailer, 0, BOOTCONFIG_TRAILER_SIZE);
	//setbits_le32(&bootconfig_trailer[0], bootconfig_size_v4);
	//setbits_le32(&bootconfig_trailer[4], bootconfig_checksum_v4);
	memcpy(bootconfig_trailer, &bootconfig_size_v4, sizeof(bootconfig_size_v4));
	memcpy(bootconfig_trailer+4, &bootconfig_checksum_v4, sizeof(bootconfig_checksum_v4));
	memcpy(&bootconfig_trailer[8], "#BOOTCONFIG\n", 12);
	debugf("add bootconfig tailer:size = %08x, checksum: %d locate to %p!\n",
		bootconfig_size_v4, bootconfig_checksum_v4, ramdisk_addr);

	memcpy(ramdisk_addr, &bootconfig_trailer[0], BOOTCONFIG_TRAILER_SIZE);
	ramdisk_addr += BOOTCONFIG_TRAILER_SIZE;

	return ramdisk_addr;
}

char *g_ramdisk_addr = NULL;
int _boot_load_kernel_ramdisk_image(const char *bootpartition, boot_img_hdr * hdr, uchar **dt_addr )
{
	char partition[20];
	const char *dtb_partname = NULL;
	uint64_t size = 0;
	const char *boot_mode_type_str;
	char *ramdisk_addr = (char *)RAMDISK_ADR;
	int ret;
	boot_img_hdr_v3 *hdr_v3 = (boot_img_hdr_v3 *)hdr;
	uint64_t ramdisk_size;

	if (0 == memcmp(bootpartition, RECOVERY_PART, strlen(RECOVERY_PART))) {
#ifdef CONFIG_ANDROID_AB
		strcpy(partition, "boot");
#else
		strcpy(partition, "recovery");
#endif
		debugf("enter recovery mode!\n");
	} else {
		strcpy(partition, "boot");
		debugf("enter boot mode!\n");
	}

#ifdef CONFIG_ANDROID_AB
	get_slot_ab(partition, NULL);
#endif

	if(0 == _get_kernel_ramdisk_dt_offset(hdr, partition))
		return 0;

	debugf("[%s] loading kernel ...\n", __func__);
	ret = load_kernel_image(hdr, partition);
	if (!ret) {
		errorf("load kernel image fail\n");
		return 0;
	}

	debugf("[%s] loading fix dtb ...\n", __func__);
	if ((hdr->header_version == BOOT_HEADER_VERSION_THREE || hdr->header_version == BOOT_HEADER_VERSION_FOUR)
		&& (hdr_v3->ramdisk_size)) {
		size = PAD_SIZE(hdr_v3->ramdisk_size, s_bootimg_info.page_size);
		ramdisk_size = hdr_v3->ramdisk_size;
	} else if (hdr->ramdisk_size) {
		size = PAD_SIZE(hdr->ramdisk_size, s_bootimg_info.page_size);
		ramdisk_size = hdr->ramdisk_size;
	}

	/*read dt image*/
#if defined(CONFIG_SEPARATE_DT)
	if (!_boot_load_separate_dt())
		return 0;
	uint64_t fdt_size = fdt_totalsize(DT_ADR);
	ret = fdt_open_into(DT_ADR, DT_ADR, fdt_size + FDT_ADD_SIZE);
	if (0 != ret)
		errorf("libfdt fdt_open_into(): %s\n", fdt_strerror(ret));
	fdt_fixup_all((uchar *)DT_ADR);
#else
	if (BOOT_HEADER_VERSION_ONE == hdr->header_version) {
		/* fixbug1007672 */
		struct boot_img_hdr_v2 *hdr_v2 = get_boot_img_hdr_v2(hdr);
		if (hdr_v2->dtb_size && hdr_v2->dtb_addr) {
			dtb_partname = partition;
			s_bootimg_info.dt_offset = hdr_v2->dtb_addr;
		} else if (0 == common_raw_read("dtb", 10, 0, (char *)DT_ADR)) {
			dtb_partname = "dtb";
			s_bootimg_info.dt_offset = 0;
		} else {
			dtb_partname = partition;
		}
	} else if (BOOT_HEADER_VERSION_THREE == hdr->header_version ||
			BOOT_HEADER_VERSION_FOUR == hdr->header_version) {
		dtb_partname = s_bootimg_info.vendor_boot_part;
	} else {
		dtb_partname = partition;
	}

	/* for memory defragment, dr_addr and ramdisk_adr is allocated dynamically*/
	if (0 == load_fixup_dt_img(dtb_partname, dt_addr)) {
		if ((uchar *)DT_ADR != *dt_addr) {
			debugf("allocate dt_addr:%p \n", *dt_addr);
			/*allocate  ramdisk addr */
			ramdisk_addr = (char *)(*dt_addr - size);
		} else {
			debugf(" use default value : DT_ADDR & RAMDISK_ADR\n");
		}
	} else {
		errorf("load dt error!\n");
		return 0;
	}
#endif

#ifdef KCE_ENCRYPT_FLAG
#if WITH_SMP
	/*wait for dtbo decrypto*/
	while(!g_start_merge_dtbo_flag);
#endif
#endif
	FTL_Savepoint_Private(PHASE_KERNEL_DTBO);
	debugf("[%s] loading dtbo ...\n", __func__);
#ifdef CONFIG_OF_LIBFDT_OVERLAY
	if (BOOT_HEADER_VERSION_ONE == hdr->header_version) {
		if (0 == memcmp(bootpartition, RECOVERY_PART, strlen(RECOVERY_PART))) {
			load_and_merge_dtbo(partition, *dt_addr);
		} else {
			merge_dtbo(*dt_addr);
		}
	} else if (BOOT_HEADER_VERSION_TWO == hdr->header_version) {
		if (s_bootimg_info.recovery_dtbo_offset && g_env_bootmode && (!strcmp("recovery", g_env_bootmode)))
			load_and_merge_dtbo(partition, *dt_addr);
		else
			merge_dtbo(*dt_addr);
	} else if (BOOT_HEADER_VERSION_THREE == hdr_v3->header_version ||
                  BOOT_HEADER_VERSION_FOUR == hdr_v3->header_version) {
		merge_dtbo(*dt_addr);
	}

	if (img_os_version >= VERSION_R) {
		if (merge_bootargs(*dt_addr))
			errorf("bootargs and bootargs_ext merge failed\n");
	}
#endif

	fdt_fixup_all(*dt_addr);

	FTL_Savepoint_Private(PHASE_KERNEL_RAMDISK);
	debugf("[%s] loading ramdisk ...\n", __func__);

	if(ramdisk_size) {
		if (BOOT_HEADER_VERSION_THREE == hdr_v3->header_version) {
			fdt_initrd_norsvmem(*dt_addr, (ulong)ramdisk_addr,
				(ulong)(ramdisk_addr + hdr_v3->ramdisk_size + s_bootimg_info.vendor_ramdisk_size), 1);

			/* read vendor ramdisk image */
			if (0 != common_raw_read(s_bootimg_info.vendor_boot_part, s_bootimg_info.vendor_ramdisk_size,
					s_bootimg_info.vendor_ramdisk_offset, ramdisk_addr)) {
				errorf("%s ramdisk read error!\n", s_bootimg_info.vendor_boot_part);
				return 0;
			}
			debugf("%s ramdisk read OK, size = %lld, locate to %p!\n",
				s_bootimg_info.vendor_boot_part, s_bootimg_info.vendor_ramdisk_size, ramdisk_addr);

			ramdisk_addr = ramdisk_addr + s_bootimg_info.vendor_ramdisk_size;

			/* read ramdisk image */
			if (0 != common_raw_read(s_bootimg_info.bootimg_part, hdr_v3->ramdisk_size,
					s_bootimg_info.ramdisk_offset, ramdisk_addr)) {
				errorf("%s ramdisk read error!\n", s_bootimg_info.bootimg_part);
				return 0;
			}
			debugf("%s ramdisk read OK, size = %d, locate to %p!\n",
				s_bootimg_info.bootimg_part, hdr_v3->ramdisk_size, ramdisk_addr);

		} else if(BOOT_HEADER_VERSION_FOUR == hdr_v3->header_version){

			/* read vendor ramdisk image */
			if (0 != common_raw_read(s_bootimg_info.vendor_boot_part, s_bootimg_info.vendor_ramdisk_size,
					s_bootimg_info.vendor_ramdisk_offset, ramdisk_addr)) {
				errorf("%s ramdisk read error!\n", s_bootimg_info.vendor_boot_part);
				return 0;
			}
			debugf("%s ramdisk read OK, size = %lld, locate to %p!\n",
				s_bootimg_info.vendor_boot_part, s_bootimg_info.vendor_ramdisk_size, ramdisk_addr);

			ramdisk_addr = ramdisk_addr + s_bootimg_info.vendor_ramdisk_size;

			/* read ramdisk image from boot.img or init_boot.img partition */
			if (s_bootimg_info.ramdisk_size !=0) {
				if (0 != common_raw_read(s_bootimg_info.bootimg_part, hdr_v3->ramdisk_size,
					s_bootimg_info.ramdisk_offset, ramdisk_addr)) {
					errorf("%s ramdisk read error!\n", s_bootimg_info.bootimg_part);
					return 0;
				}
				debugf("%s ramdisk read OK, size = %d, locate to %p!\n",
					s_bootimg_info.bootimg_part, hdr_v3->ramdisk_size, ramdisk_addr);

				ramdisk_addr = ramdisk_addr + hdr_v3->ramdisk_size;
			} else if (s_bootimg_info.initboot_ramdisk_size !=0) {
				if (0 != common_raw_read(s_bootimg_info.init_boot_part, s_bootimg_info.initboot_ramdisk_size,
					s_bootimg_info.initboot_ramdisk_offset, ramdisk_addr)) {
					errorf("%s ramdisk read error!\n", s_bootimg_info.init_boot_part);
					return 0;
				}
				debugf("%s ramdisk read OK, size = %lld, locate to %p!\n",
					s_bootimg_info.init_boot_part, s_bootimg_info.initboot_ramdisk_size, ramdisk_addr);

				ramdisk_addr = ramdisk_addr + s_bootimg_info.initboot_ramdisk_size;
			} else {
				errorf("ERROR: Please check boot img and init_boot img ramdisk config, they were bad image!!!\n");
				return 0;
			}

			/*read bootconfig (from vendor_boot) right after the generic ramdisk.*/
			read_vendor_bootconfig(ramdisk_addr); /* To reduce ccn */
			g_ramdisk_addr = ramdisk_addr;
#ifdef CONFIG_BOOTCONFIG
			s_bootimg_info.vendor_bootconfig_size =
				bootconfig_fixup_all(ramdisk_addr, s_bootimg_info.vendor_bootconfig_size);
#endif
#if !WITH_SMP
			ramdisk_addr = add_bootconfig_trailer(ramdisk_addr);
			fdt_initrd_norsvmem(*dt_addr, (ulong)RAMDISK_ADR, (ulong)ramdisk_addr, 1);
#endif
		} else {
			fdt_initrd_norsvmem(*dt_addr, (ulong)ramdisk_addr, (ulong)(ramdisk_addr + hdr->ramdisk_size), 1);
			if (0 == size) {
				errorf("ramdisk image size should not be zero\n");
				return 0;
			}
			/*read ramdisk image */
			if (0 != common_raw_read(partition, size, (uint64_t)(s_bootimg_info.ramdisk_offset), ramdisk_addr)) {
				errorf("%s ramdisk read error!\n", partition);
				return 0;
			}
			debugf("%s ramdisk read OK,size=0x%llx, locate to %p \n", partition, size, ramdisk_addr);

			boot_mode_type_str = g_env_bootmode;
			if (NULL != boot_mode_type_str)
			{
				if(!strncmp(boot_mode_type_str, "sprdisk", 7)) {
					int ramdisk_size;
					ramdisk_size = boot_sprdisk(0, ramdisk_addr);
					if (ramdisk_size > 0) {
						hdr->ramdisk_size = ramdisk_size;
						if ((char *)RAMDISK_ADR != ramdisk_addr) {
							/* get ramdisk size , calculate addr, so we can have enough room for sprdisk ramdisk */
							size = PAD_SIZE(hdr->ramdisk_size, KERNL_PAGE_SIZE);
							ramdisk_addr = (char *)(*dt_addr - size);
							hdr->ramdisk_addr = (uint64_t)ramdisk_addr;
						}
						fdt_initrd_norsvmem(*dt_addr, (ulong)ramdisk_addr, (ulong)(ramdisk_addr+hdr->ramdisk_size), 1);
						boot_sprdisk(ramdisk_size, ramdisk_addr);
					} else {
						errorf("%s, sprdisk mode failure!\n", __FUNCTION__);
					}
					return 1;
				}
			}
		}

#ifdef CONFIG_SDRAMDISK
		int sd_ramdisk_size = 0;
#ifdef WDSP_ADR
		size = WDSP_ADR - RAMDISK_ADR;
#else
		size = TDDSP_ADR - RAMDISK_ADR;
#endif
		if (size > 0) {
			_sd_fat_mount();
			sd_ramdisk_size = load_sd_ramdisk((void *) RAMDISK_ADR, size);
		}
		if (sd_ramdisk_size > 0)
			hdr->ramdisk_size = sd_ramdisk_size;
#endif
	}

	return 1;
}

#ifdef CONFIG_SEPARATE_DT
int _boot_load_separate_dt(void)
{
	struct dt_table_t table;
	char * header = NULL;
	struct dt_entry_t *dt_entry_ptr;
	uint64_t offset = 0;
	uint64_t size = 0;

#ifdef SPRD_SECBOOT
	offset = 512;/**header of image size is 512B**/
#endif

	size = sizeof(struct dt_table_t);
	if (0 != common_raw_read(DT_PART, size, offset, &table)) {
		errorf("read dt image table header fail\n");
		goto fail;
	}

	/* Validate the device tree table header */
	if((table.magic != SPRD_DT_MAGIC) || (table.version != SPRD_DT_VERSION)) {
		errorf("Cannot validate Device Tree Table \n");
		goto fail;
	}

	size = sizeof(struct dt_table_t) + sizeof(struct dt_entry_t) * (uint64_t)table.num_of_entries;
	header = (char *)malloc_cache_aligned(size);
	if (NULL == header) {
		errorf("malloc size %d for dt header fail\n", size);
		goto fail;
	}
	if (0 != common_raw_read(DT_PART, size, offset, header)) {
		errorf("read dt image table and entries header fail\n");
		goto fail;
	}

	/* Calculate the offset of device tree within device tree table */
	dt_entry_ptr = fdt_get_entry_ptr_by_table((struct dt_table_t *)header);
	if(NULL == dt_entry_ptr) {
		errorf("Getting device tree address failed\n");
		goto fail;
	}

	size = dt_entry_ptr->dt_size;
	offset += dt_entry_ptr->dt_offset;
	if (0 != common_raw_read(DT_PART, size, offset, (char *)DT_ADR)) {
		errorf("dt entry size 0x%x offset 0x%x read error!\n", size, offset);
		goto fail;
	}

	debugf("load separate DT OK,size=0x%llx, locate to 0x%lx \n", size, (ulong)DT_ADR);
	free(header);
	return 1;

fail:
	if (NULL != header)
		free(header);
	return 0;
}
#endif

#ifdef SPRD_VBOOT_V2
int secure_get_partition_size(char * partition_name, uint64_t * size)
{
	int ret = 0;
#ifdef CONFIG_ANDROID_AB
	char partition[20] = {0};
    get_slot_ab(partition, partition_name);
	ret = get_img_partition_size(partition, size);
#else
	ret = get_img_partition_size(partition_name, size);
#endif

	debugf("partition:%s,size:0x%llx\n", partition_name, *size);
	return ret;
}
#endif

#ifdef SPRD_SECBOOT
extern int power_button_status(void);
int sprd_vboot_set_display(int sec_time_count)
{
	int press_status = 0, count = 0;
	int pwr_key_reset_flag = 0, display_count = 0;

	if (sec_time_count == 300) {
		while(sec_time_count > 0) {
			mdelay(100);
			if ((press_status == 0) && (power_button_status() == 0)) {
				press_status = 1;
			}
			if ((press_status == 1) && (power_button_status() == 1)) {
				press_status = 2;
				sec_time_count = 0;
			}
			sec_time_count --;
		}
	} else if (sec_time_count == 100) {
		lcd_printf("\n\n\nINFO: Press power button to pause.\n");//modify for NYX-992 incomplete character display by hyinfeng
		dprintf(ALWAYS, "UNLOCK: check the status of the power button...\n");
		while(sec_time_count > 0) {
			mdelay(100);
			if ((pwr_key_reset_flag == 0) && (power_button_status() == 1))
				pwr_key_reset_flag  = 1;/*write for power button to reset*/
			if (pwr_key_reset_flag == 1) {
				if ((press_status == 0) && (power_button_status() == 0)) {
					press_status = 1;
				}
				if ((press_status == 1) && (power_button_status() == 1)) {
					press_status = 0;
					count++;
				}
				if (count == 1) {
					if (display_count == 0) {
						lcd_printf("INFO: Press power button to continue.\n");
						display_count = 1;
					}
					continue;
				} else if (count >= 2) {
					sec_time_count = 0;
				}
			}
			sec_time_count--;
		}
	}
	dprintf(ALWAYS, "UNLOCK: finish checking the status of the power button\n");

	return press_status;
}

void secboot_unlock_display(void) {

	if(g_DeviceStatus == VBOOT_STATUS_UNLOCK) {
		dprintf(ALWAYS, "[%s]: DeviceStatus is unlock.\n",__func__);
		sprd_vboot_set_display(100);
	}

}

#if WITH_SMP
extern volatile u32 lcd_done_flag;
#endif

// ning.wei@hmd++ for fastboot ui display sync from solo begin
extern char *get_product_sn(void);
extern int fb_check_secboot_enable(void);
extern void console_setfgcolor(int);
extern void console_setbgcolor(int);
int key_listener_premssion=0;
int bg_counts = 1;
void printinfo(void) {
	//lcd_splash("logo");
	console_setfgcolor(SPRD_CONSOLE_COLOR_WHITE2);
	console_setbgcolor(SPRD_CONSOLE_COLOR_BLACK2);



	lcd_position_cursor(0, 33);
	lcd_printf("      Product name: Nyx4G\n");

	lcd_printf("      Platform: s9863a\n");
	lcd_printf("      Serial number: %s \n", get_product_sn());
	if (fb_check_secboot_enable()) {
		lcd_printf("      Secure boot: Disable\n");
	} else{
		lcd_printf("      Secure boot: Enable\n");
	}
	if (get_lock_status() == VBOOT_STATUS_UNLOCK) {
		lcd_printf("      Device state: Unlock \n");
	} else {
		lcd_printf("      Device state: Lock \n");
	}
#if DEBUG
	lcd_printf("      Build variants: Userdebug\n");
#else
	lcd_printf("      Build variants: User\n");
#endif
	lcd_position_cursor(0, 0);
	console_setfgcolor(SPRD_CONSOLE_COLOR_BLACK2);
	console_setbgcolor(SPRD_CONSOLE_COLOR_WHITE2);
}
void sprocomm_set_avb_display_background(int counts, int is_need)
{
	mdelay(100);
	int key_code, old_counts;
	bg_counts = counts;
	old_counts = counts;
	key_code = board_key_scan();
	if (KEY_VOLUMEUP == key_code) { //KEY_VOLUMEUP
		bg_counts++;
		if (bg_counts > 2)
			bg_counts = 0;
	} else if (KEY_VOLUMEDOWN == key_code) { //KEY_VOLUMEDOWN
		bg_counts--;
		if (bg_counts < 0)
			bg_counts = 2;
	}
	if (old_counts != bg_counts) {
		old_counts = counts;
		logo_display(bg_counts + 5, BACKLIGHT_ON, LCD_DISPLAY_ENABLE);
		printinfo();
	}
	if (power_button_pressed() == 0) {
		switch (bg_counts) {
			case 0: reboot_devices(CMD_FASTBOOT_MODE);
			case 1: reboot_devices(CMD_NORMAL_MODE);
			case 2: power_down_devices(0);
			default: lcd_printf("\t error,counts:%d \n", bg_counts);
		}
	}
	//logo_display(4, BACKLIGHT_ON, LCD_DISPLAY_ENABLE);
	if (is_need == 1)
		lcd_position_cursor(0, 0);
}
// ning.wei@hmd++ for fastboot ui display sync from solo end

int loader_binding_data_set(void)
{
	/*set device state*/
	get_lock_status();

	if(g_DeviceStatus == VBOOT_STATUS_LOCK){
		debugf("INFO: LOCK FLAG IS : LOCK!!!\n");
	}else if(g_DeviceStatus == VBOOT_STATUS_UNLOCK){
		debugf("INFO: LOCK FLAG IS : UNLOCK!!!\n");
#if WITH_SMP
		while(!lcd_done_flag); // wait for lcd init ok
#endif
		lcd_printf("INFO: LOCK FLAG IS : UNLOCK!!!\n");
	}

	return 0;
}

int loader_binding_state_get(void)
{
	if(g_DeviceStatus == 0xFFFFFFFF) {
		debugf("Warning: get device state error: state is not initiated!\n");
		return -1;
	}

	return g_DeviceStatus;
}

int sprd_sec_verify_lockstatus(unsigned char *lockstatus, unsigned int status_len)
{
#ifndef SPRD_VBOOT_V2
	return -1; /*default status : locked*/
#else
	int ret = -1;
	char *data_buffer = NULL;
	data_buffer = memalign(4096, 128);
	if (data_buffer == NULL) {
		errorf("no enough heap for verify lockstatus\n");
		return -ENOMEM;
	}
	uint8_t * v_lock_state_ret = (uint8_t *)data_buffer + PDT_INFO_LOCK_FLAG_MAX_SIZE;

	if(!lockstatus || !status_len || (status_len > PDT_INFO_LOCK_FLAG_MAX_SIZE))
	{
		debugf("para error. \n");
		free(data_buffer);
		return -1;
	}

	memset(data_buffer, 0, 128);
	memcpy(data_buffer, lockstatus, status_len);

#ifdef CONFIG_VBOOT_DUMP
	do_hex_dump(data_buffer, status_len);
#endif

	ret = uboot_verify_lockstatus((uint64_t)data_buffer, 128);

	if(*v_lock_state_ret == VBOOT_STATUS_LOCK)
	{
		//debugf("verify unlock status failed. \n");
		ret = -1;
	}

	free(data_buffer);
	return ret;
#endif
}
#endif

#if defined (SPRD_SECBOOT)
//start add by hyinfeng for HMD NYX-296 show warning messages when device is unlocked or system is invalid.
#if defined(ZCFG_USERDEBUG_LOCKDEVICE_CANNOT_BOOT)
extern int lcd_splash(char *logo_part_name);
extern char *get_product_sn(void);
extern int get_lcs(uint32_t *p_lcs);
void showWarnIcon(int color)
{
	lcd_clear();
	
	lcd_printf("\n\n\n\n");
#ifdef CONFIG_SYS_WHITE_ON_BLACK
	console_setFGColor(color);
#endif
	lcd_printf("   /|\\    \n"
			   "  / | \\   \n"
			   " /  0  \\  \n"
			   " -------  \n\n");
#ifdef CONFIG_SYS_WHITE_ON_BLACK
	console_setFGColor(0xFFFFFFFF);
#endif
}

void showSupportUrl(int color)
{
	lcd_printf("Visit this link on another device to learn more:\n");
#ifdef CONFIG_SYS_WHITE_ON_BLACK
	console_setFGColor(color);
#endif
	lcd_printf("https://www.hmd.com/support\n\n\n\n\n\n\n\n\n\n");
#ifdef CONFIG_SYS_WHITE_ON_BLACK
	console_setFGColor(0xFFFFFFFF);
#endif
}

void showSnAndPowerKey(bool isPowerKeyPress)
{
	lcd_printf("ID: %s\n\n\n\n\n\n",get_product_sn());
	
	if(isPowerKeyPress)
		lcd_printf("Press power button to continue.\n");
	else
		lcd_printf("Press power button to pause.\n");
}


void showYellowMessage(bool isPowerKeyPress)
{
	showWarnIcon(0x00ffff00);
	
	lcd_printf("Your device has loaded a different operating system.\n\n");
	
	showSupportUrl(0x00ffff00);

	showSnAndPowerKey(isPowerKeyPress);
}

void showOrangeMessage(bool isPowerKeyPress)
{
	showWarnIcon(0x00FFA200);
	
	lcd_printf("The boot loader is unlocked and software integrity cannot be guaranteed."
			   "Any data stored on the device may be available to attackers.\n"
			   "Do not store any sensitive data on the device.\n\n");
	
	showSupportUrl(0x00FFA200);

	showSnAndPowerKey(isPowerKeyPress);
}

void showRedMessage(bool isPowerKeyPress)
{
	showWarnIcon(0x00FFF900);
	
	lcd_printf("Your device is corrupt. It can't be trusted and may not work properly.\n\n");
	
	showSupportUrl(0x00FFF900);

	showSnAndPowerKey(isPowerKeyPress);
	/*
	lcd_printf("ID: %s\n\n\n\n\n\n",get_product_sn());
	
	if(isPowerKeyPress)
		lcd_printf("Press power button to power off.\n");
	else
		lcd_printf("Press power button to pause.\n");
	*/
}

void showNoValidOSMessage(void)
{
	showWarnIcon(0x00FFF900);
	
	lcd_printf("No valid operating system could be found. The device will not boot.\n\n");	

	showSupportUrl(0x00FFF900);
	
	lcd_printf("ID: %s \n\n\n\n",get_product_sn());
	
	lcd_printf("Press power button to power off.\n");
}

void showUserdebugLockedMessage(void)
{
	showWarnIcon(0x00FFF900);
	
	lcd_printf("The userdebug build image does not boot on locked device.\n\n");
	
	showSupportUrl(0x00FFF900);
	
	lcd_printf("Press power button to enter fastboot.\n");
}

void showSnErrorMessage(void)
{
	showWarnIcon(0x00FFF900);
	
	lcd_printf("The SN %s of this device is abnormal or empty,\n",get_product_sn());			
	
	showSupportUrl(0x00FFF900);

	lcd_printf("Press power button to power off and rewrite the SN.\n\n");
}

int isCaliBootMode(void)
{
	const char*boot_mode_type_str = g_env_bootmode;
	//boot_mode_type_str = getenv("bootmode");
	if (NULL != boot_mode_type_str)
	{
		if(!strncmp(boot_mode_type_str, "cali", 4)) {
			return 1;
		}
	}
	return 0;
}

int checkSerialNumber(void)
{
	if(isCaliBootMode()) return 0;
	
	char psn[PRODUCT_SN_TOKEN_MAX_SIZE + 1] = {0};
	strcpy(psn, get_product_sn());
	bool isSnEmpty = true;
	int snLength = strlen(psn);
	for(int i = 0;i < snLength;++i)
	{
		if(psn[i] != '0')
		{
			isSnEmpty = false;
			break;
		}
	}
	if (0 == snLength || isSnEmpty)    
	{   
		int key_code;
		int sec_time_count = 30*5;
		showSnErrorMessage();
		while(sec_time_count) {
			mdelay(200);
			sec_time_count --;
			key_code = power_button_pressed();
			if (key_code == 0) {
				break;
			}				
		}
		power_down_devices(0);
		return 0;
	}
	return 1;
}

void showWarnMessage(int color)
{
	if(0x00ffff00 == color) {
		showYellowMessage(false);
	} else if(0x00FFA200 == color) {
		showOrangeMessage(false);
	} else {
		showRedMessage(false);
	}
	int bPowerPress = 0;
	int key_code;
	int sec_time_count = 10*5;
	while(sec_time_count) {
		mdelay(200);
		//key_code = board_key_scan();
		key_code = power_button_pressed();
		//debugf("board_key_scan key_code = %d\n",key_code);
		//#define PW_KEY_PRESSED		0
		//#define PW_KEY_NOT_PRESSED	1
		if (key_code == 0) {
			bPowerPress = 1;
			break;
		}
		sec_time_count--;	
	}
	if(bPowerPress == 1)
	{
		if(0x00ffff00 == color) {
			showYellowMessage(true);
		} else if(0x00FFA200 == color) {
			showOrangeMessage(true);
		} else {
			showRedMessage(true);
		}
		do {
			udelay(500 * 1000);
			//key_code = board_key_scan();
			key_code = power_button_pressed();	
			if (key_code == 0) {
				break;
			}
		} while (1);	
	}
	lcd_clear();
	lcd_splash(LOGO_PART);
}
#endif
//end add by hyinfeng for HMD NYX-296 show warning messages when device is unlocked or system is invalid.

void take_action_with_dmverity_ret(void)
{
	int sec_time_count = 300;
	if (g_dmverity_flag == HWRST_STATUS_SECBOOT) {
		debugf("INFO: your device is corrupt, It can't be trusted and may not work properly!\n");
		//start modify by hyinfeng for HMD NYX-296 NYX-3447 show warning messages when device is unlocked or system is invalid.
#if defined(ZCFG_USERDEBUG_LOCKDEVICE_CANNOT_BOOT)
		int key_code;
		//int bPowerPress = 0;
		showRedMessage(true);
		while(sec_time_count) {
			mdelay(100);
			sec_time_count --;
			key_code = power_button_pressed();
			if (key_code == 0) {
				//bPowerPress = 1;
				//sec_time_count = 300;
				//lcd_clear();
				//lcd_splash(LOGO_PART);
				//break;
				logo_display(LOGO_NORMAL_POWER, BACKLIGHT_ON, LCD_ON);
				reboot_devices(CMD_FASTBOOT_MODE);
				return;
			}
		}
		power_down_devices(0);
#else
		//end modify by hyinfeng for HMD NYX-296 show warning messages when device is unlocked or system is invalid.
		lcd_printf("INFO: your device is corrupt, It can't be trusted and may not work properly!\n");
		if (g_dmverity_corrupt_flag == 1) {
			debugf("INFO: first scene of dm-verity corrupt, start minidump...\n");
			lcd_printf("INFO: first scene of dm-verity corrupt, start minidump...\n");
			save_minidump(CMD_UNKNOW_REBOOT_MODE, "dm-verity");
			debugf("INFO: end minidump!\n");
			lcd_printf("INFO: end minidump!\n");
		} else {
			debugf("INFO: not first scene of dm-verity corrupt, skip.\n");
		}
		lcd_printf("INFO: Press power button to continue.\n");

		if (sprd_vboot_set_display(sec_time_count) != 2) {
			power_down_devices(0);
		}
#endif
	}
}

int take_action_with_vbootret(void)
{
	int sec_time_count = 100;

#if WITH_SMP
	while(!lcd_done_flag); // wait for lcd init ok
#endif
	//start add by hyifeng for HMD device do not boot when SN is empty.
#if defined(ZCFG_SN_EMPTY_CANNOT_BOOT)
	if (checkSerialNumber() == 0) {
		return 0;
	}
#endif
	//end add by hyinfeng for HMD device do not boot when SN is empty.
	switch(g_verifiedbootstate) {
		case v_state_red:
			debugf("WARNNING: NO valid OS found!!!\n");
			//start modify by hyinfeng for HMD NYX-296 show warning messages when device is unlocked or system is invalid.
#if defined(ZCFG_USERDEBUG_LOCKDEVICE_CANNOT_BOOT)
			int key_code;
			showNoValidOSMessage();
			sec_time_count = 30*5;
			while(sec_time_count) {
				mdelay(200);
				sec_time_count --;
				key_code = power_button_pressed();
				if (key_code == 0) {
					lcd_clear();
					lcd_splash(LOGO_PART);
					break;
				}
			}
			//end modify by hyinfeng for HMD NYX-296 show warning messages when device is unlocked or system is invalid.
#else
			lcd_printf("WARNNING: NO VALID OS FOUND!!!\n");
			sec_time_count = 300;
			sprd_vboot_set_display(sec_time_count);
#endif
			power_down_devices(0);
			break;

		case v_state_yellow:
			debugf("WARNNING: your device has loaded a different operating system.\n");
			//start modify by hyinfeng for HMD NYX-296 show warning messages when device is unlocked or system is invalid.
#if defined(ZCFG_USERDEBUG_LOCKDEVICE_CANNOT_BOOT)
			if(isCaliBootMode()) break;
			showWarnMessage(0x00ffff00);
#else
			//end modify by hyinfeng for HMD NYX-296 show warning messages when device is unlocked or system is invalid.
			lcd_printf("WARNNING: USER KEY VERIFY SUCCESS!!!\n");
			sprd_vboot_set_display(sec_time_count);
#endif
			break;

		case v_state_green:
			debugf("WARNNING: OEM KEY VERIFY SUCCESS!!!\n");
			//start add by hyinfeng for HMD NYX-194,NYX-297 The userdebug build image shall not boot up when bootloader is locked
#if defined(ZCFG_USERDEBUG_LOCKDEVICE_CANNOT_BOOT)
#if DEBUG
			if(isCaliBootMode()) break;
			unsigned int t_lcs __attribute__((aligned(4096))) = 0;
			int ret = get_lcs(&t_lcs);//check if device is efused(t_lcs=5 means efused)
			if(get_lock_status() == VBOOT_STATUS_LOCK && 0 == ret && 5 == t_lcs) 
			{
				showUserdebugLockedMessage();
				sec_time_count = 10*5;
				while(sec_time_count) {
					mdelay(200);
					sec_time_count --;
					key_code = power_button_pressed();
					if (key_code == 0) {
						reboot_devices(CMD_FASTBOOT_MODE);
						return 0;
					}				
				}
				power_down_devices(0);
			}
#endif
#endif
			//end add by hyinfeng for HMD NYX-194,NYX-297 The userdebug build image shall not boot up when bootloader is locked
			break;

		case v_state_orange:
			debugf("WARNNING: LOCK FLAG IS : UNLOCK, SKIP VERIFY!!!\n");
			//start modify by hyinfeng for HMD NYX-194,NYX-297 show warning messages when device is unlocked or system is invalid.
#if defined(ZCFG_USERDEBUG_LOCKDEVICE_CANNOT_BOOT)
			if(isCaliBootMode()) break;
#if DEBUG
			showWarnMessage(0x00FFA200);
#else
			unsigned int t_lcs __attribute__((aligned(4096))) = 0;
			int ret = get_lcs(&t_lcs);//check if device is efused(t_lcs=5 means efused)
			if(0 == ret && 5 == t_lcs) {
				if (set_lock_status(VBOOT_STATUS_LOCK)) {
					debugf("set_lock_status lock fail.\n");
					showWarnMessage(0x00FFA200);
				} else {
					reboot_devices(CMD_NORMAL_MODE);
				}
			} else {
				showWarnMessage(0x00FFA200);
			}
#endif
#else
			//end modify by hyinfeng for HMD NYX-194,NYX-297 show warning messages when device is unlocked or system is invalid.
			lcd_printf("WARNNING: LOCK FLAG IS : UNLOCK, SKIP VERIFY!!!\n");
#endif
			break;

		default:
			debugf("WARNNING: VERIFY RESULT UNKNOWN!!!\n");
			break;
	}

	return 0;
}

#endif
// add for SOTER start
#if defined (CONFIG_CHIP_UID)
static u32 blocks[2] __attribute__((aligned(4096)));

void pass_chip_uid_to_tos(void)
{
 #ifndef CONFIG_ZEBU
    //u32 blocks[2]/* = {0} */__attribute__((aligned(4096)));
    //u32 *blocks = VERIFY_BASE;
    smc_param *param;

    dprintf(INFO,"pass_chip_uid_to_tos()... sizeof(blocks)=%ld\n", sizeof(blocks));
    memset(blocks, 0, sizeof(blocks));
#if defined (CONFIG_SPRD_UID)
    blocks[1] = sprd_efuse_double_read(UID_START, UID_DOUBLE);
    blocks[0] = sprd_efuse_double_read(UID_END, UID_DOUBLE);
    param = tee_common_call(FUNCTYPE_SET_CHIP_UID, (uint32_t)blocks, sizeof(u32)*2);
    dprintf(INFO,"blks:0x%08x 0x%08x, result: 0x%x \n", blocks[0], blocks[1], param->a0);
#else
    blocks[0]= sprd_ap_efuse_read(0);
    blocks[1]= sprd_ap_efuse_read(1);
    param = tee_common_call(FUNCTYPE_SET_CHIP_UID, (uint32_t)blocks, sizeof(u32)*2);
    dprintf(INFO,"blks:0x%08x 0x%08x, result: 0x%x \n", blocks[0], blocks[1], param->a0);
#endif
    return;
#endif
    return;
}
#endif
// add for SOTER end

void power_cfg(void)
{
	//wakeup_source_enable();
	//ap_clk_doze_enable();
	return ;
}

void vibrator_load(int enable)
{
	set_vibrator(enable);
	return ;
}

int load_require_image(void)
{
	int row=0, colum=0;

	//do something.... befor loading required image
#ifdef CONFIG_ARM7_RAM_ACTIVE
	pmic_arm7_RAM_active();
#endif

	//begin loading required image ....
	while (s_boot_image_table[row]) {
		colum = 0;
		while (strlen(s_boot_image_table[row][colum].partition)) {
			_boot_load_required_image(s_boot_image_table[row][colum]);
			#if defined(CONFIG_CH_ENABLE) && !defined(CONFIG_CH_DDR_BOOT)
			if (0 == memcmp("ch_sys", s_boot_image_table[row][colum].partition, strlen("ch_sys")))
				memcpy(CH_IRAM_ADDR, CH_DDR_ADDR, CH_IRAM_SIZE);
			#endif
			colum++;
		}
		row++;
	}

#if defined(CONFIG_KERNEL_BOOT_CP)
	if(g_charger_mode) {
		const boot_image_required_t pm_image = {"pm_sys", "", (uint64_t)DFS_SIZE, (char *)DFS_ADDR};
		_boot_load_required_image(pm_image);
	}
#endif

#if !defined(CONFIG_KERNEL_BOOT_CP) && defined(CONFIG_MEM_LAYOUT_DECOUPLING)
	extern boot_image_required_t *get_cp_load_table(void);
	do {
		boot_image_required_t *cp_load_table = NULL;
		cp_load_table = get_cp_load_table();
		dprintf(INFO,"cp_load_table = 0x%p\n", cp_load_table);
		if (NULL != cp_load_table) {
			for(row = 0; cp_load_table[row].size > 0; row++) {
				_boot_load_required_image(cp_load_table[row]);
			}
		}
	} while(0);
#endif

	return 0;
}

int load_kernel_ramdisk(const char *bootpartition, boot_img_hdr *hdr, uchar *dt_adr, uchar *ramdisk_adr)
{
	int ret = 0;
	uchar *adr = dt_adr;

#ifdef OTA_BACKUP_MISC_RECOVERY
	ret = memcmp(bootpartition, RECOVERY_PART, strlen(RECOVERY_PART));
	if ((ret != 0) || (boot_load_recovery_in_sd(hdr) != 0))
		if (!_boot_load_kernel_ramdisk_image(bootpartition, hdr, &adr))
			ret = -1;
#else
	/*loader kernel and ramdisk*/
	if (!_boot_load_kernel_ramdisk_image(bootpartition, hdr, &adr))
		ret = -1;
#endif

	return ret;
}


int sprd_set_postload(void)
{
	struct post_load_operations *postload = &sprd_post_load;

	// add for SOTER start
#if defined (CONFIG_CHIP_UID)
	pass_chip_uid_to_tos();
#endif
	// add for SOTER end
	if (postload->rpmb)
		postload->rpmb();

#ifdef SPRD_VBOOT_V2
	if (postload->keymint)
		postload->keymint();
#endif

	dprintf(INFO, "bootloader postload\n");
	return 0;
}

int sprd_set_preload(int lcd_on, uint32_t brightness)
{
	struct pre_load_operations *preload = &sprd_pre_load;

	//pmic & wakeup configure
	if (preload->power)
		preload->power();

#ifdef CONFIG_SPLASH_SCREEN
	//lcd/backlight/vibrator configure
	if (preload->display) {
		if (POWEROFF_CHARGE_LOGO_SUPPORT && !strcmp("charger", g_env_bootmode))
			preload->display(LOGO_POWEROFF_CHARGE, brightness, lcd_on);
		else
			preload->display(LOGO_NORMAL_POWER, brightness, lcd_on);
	}
#endif
	if (preload->vibrator)
		preload->vibrator(0);

	return 0;
}

void rpmb_check(void)
{
#if defined(CONFIG_SPRD_WRITE_RPMB_KEY)
	uint8_t key[32];
	int rc;

	rc = lk_write_rpmb_key(key);
	if (0 == rc) {
		sprd_init_all_imgversion(key);
	}
	memset((void*)key,  0, sizeof(key));
#endif

	lk_set_rpmb_size();
	lk_is_wr_rpmb_key();
	lk_check_rpmb_key();
	lk_set_rpmb_device_type();

	return;
}

void cleanup_cache_environment(void)
{
#ifdef SPRD_ASYNC_SUPPORT
    arch_disable_async();
#endif
    arch_disable_ints();
    icache_disable();
    invalidate_icache_all();
    dcache_disable();
    invalidate_dcache_all();
}

unsigned short calc_checksum(unsigned char *dat, unsigned long len)
{
	unsigned short num = 0;
	unsigned long chkSum = 0;
	while (len > 1) {
		num = (unsigned short)(*dat);
		dat++;
		num |= (((unsigned short)(*dat)) << 8);
		dat++;
		chkSum += (unsigned long)num;
		len -= 2;
	}
	if (len) {
		chkSum += *dat;
	}
	chkSum = (chkSum >> 16) + (chkSum & 0xffff);
	chkSum += (chkSum >> 16);
	return (~chkSum);
}

unsigned char _chkNVEcc(uint8_t *buf, uint64_t size, uint32_t checksum)
{
	uint16_t crc;

	crc = calc_checksum(buf, (unsigned long)size);
	debugf("_chkNVEcc calcout 0x%x, org 0x%x\n", crc, (uint16_t)checksum);
	return (crc == (uint16_t) checksum);
}

#ifdef CONFIG_HMD_FASTBOOT
/*modif to support the sp15 64 bit sn NO */
char *get_product_sn(void)
{
	SP09_PHASE_CHECK_T *phase_check_sp09 = NULL;
	SP15_PHASE_CHECK_T *phase_check_sp15 = NULL;
	uint32_t magic = 0;

	memset(serial_number_to_transfer, 0x0, SP15_MAX_SN_LEN);
	strncpy(serial_number_to_transfer, "0000000000000000", SP15_MAX_SN_LEN);

	phase_check_sp09 = malloc(sizeof(SP09_PHASE_CHECK_T));
	if (phase_check_sp09 == NULL) {
		errorf("no enough heap for phase_check_sp09\n");
		return serial_number_to_transfer;
	}
	phase_check_sp15 = malloc(sizeof(SP15_PHASE_CHECK_T));
	if (phase_check_sp15 == NULL) {
		errorf("no enough heap for phase_check_sp15\n");
		free(phase_check_sp09);
		return serial_number_to_transfer;
	}
	memset(phase_check_sp09, 0, sizeof(SP09_PHASE_CHECK_T));
	memset(phase_check_sp15, 0, sizeof(SP15_PHASE_CHECK_T));

	if (0 != common_raw_read(PRODUCTINFO_FILE_PATITION, sizeof(magic), (uint64_t)MISCDATA_PHONE_SN_BASE, (char *)&magic)) {
		errorf("read miscdata error.\n");
		free(phase_check_sp09);
		free(phase_check_sp15);
		return serial_number_to_transfer;
	}
	if(magic == SP09_SPPH_MAGIC_NUMBER){
		if(common_raw_read(PRODUCTINFO_FILE_PATITION,sizeof(SP09_PHASE_CHECK_T), (uint64_t)MISCDATA_PHONE_SN_BASE, (char *)phase_check_sp09)){
			debugf("sp09 read miscdata error.\n");
			free(phase_check_sp09);
			free(phase_check_sp15);
			return serial_number_to_transfer;
		}
		if(strlen(phase_check_sp09->SN1)){
			memcpy(serial_number_to_transfer, phase_check_sp09->SN1, SP09_MAX_SN_LEN);
		}
	}else if(magic == SP15_SPPH_MAGIC_NUMBER){
		if(common_raw_read(PRODUCTINFO_FILE_PATITION, sizeof(SP15_PHASE_CHECK_T), (uint64_t)MISCDATA_PHONE_SN_BASE, (char *)phase_check_sp15)){
			debugf("sp15 read miscdata error.\n");
			free(phase_check_sp09);
			free(phase_check_sp15);
			return serial_number_to_transfer;
		}
		if(strlen(phase_check_sp15->SN1)){
			memcpy(serial_number_to_transfer, phase_check_sp15->SN1, SP15_MAX_SN_LEN);
		}
	}
	free(phase_check_sp09);
	free(phase_check_sp15);
	return serial_number_to_transfer;
}

int set_product_sn(char *psn, int len)
{
	static const char *serialno_def = "0000000000000000";
	SP09_PHASE_CHECK_T phase_check_sp09;
	SP15_PHASE_CHECK_T phase_check_sp15;
	uint32_t magic;

	if (len < 0 || (len && !psn))
		return -1;

	if (0 != common_raw_read(PRODUCTINFO_FILE_PATITION, sizeof(magic),
			(uint64_t)MISCDATA_PHONE_SN_BASE, (char *)&magic)) {
		errorf("read miscdata error.\n");
		return -1;
	}

	if (magic == SP09_SPPH_MAGIC_NUMBER) {
		if (common_raw_read(PRODUCTINFO_FILE_PATITION, sizeof(phase_check_sp09),
				(uint64_t)MISCDATA_PHONE_SN_BASE, (char *)&phase_check_sp09)) {
			errorf("sp09 read miscdata error.\n");
			return -1;
		}

		if (len > sizeof(phase_check_sp09.SN1)) {
			errorf("set length(%d) is larger than sp09 limit.\n", len);
			return -1;
		}

		memset(phase_check_sp09.SN1, 0, sizeof(phase_check_sp09.SN1));
		if (!len)
			memcpy(phase_check_sp09.SN1, serialno_def, strlen(serialno_def));
		else
			memcpy(phase_check_sp09.SN1, psn, len);

		if (common_raw_write(PRODUCTINFO_FILE_PATITION, sizeof(phase_check_sp09),
				(uint64_t)0, (uint64_t)MISCDATA_PHONE_SN_BASE, (char *)&phase_check_sp09)) {
			errorf("sp09 write miscdata error.\n");
			return -1;
		}

		return 0;
	} else if(magic == SP15_SPPH_MAGIC_NUMBER) {
		if (common_raw_read(PRODUCTINFO_FILE_PATITION, sizeof(phase_check_sp15),
				(uint64_t)MISCDATA_PHONE_SN_BASE ,(char *)&phase_check_sp15)) {
			errorf("sp15 read miscdata error.\n");
			return -1;
		}

		if (len > sizeof(phase_check_sp15.SN1)) {
			errorf("set length(%d) is larger than sp15 limit.\n", len);
			return -1;
		}

		memset(phase_check_sp15.SN1, 0, sizeof(phase_check_sp15.SN1));
		if (!len)
			memcpy(phase_check_sp15.SN1, serialno_def, strlen(serialno_def));
		else
			memcpy(phase_check_sp15.SN1, psn, len);

		if (common_raw_write(PRODUCTINFO_FILE_PATITION, sizeof(phase_check_sp15),
				(uint64_t)0, (uint64_t)MISCDATA_PHONE_SN_BASE, (char *)&phase_check_sp15)) {
			errorf("sp15 write miscdata error.\n");
			return -1;
		}

		return 0;
	} else {
		errorf("unsupport magic %x.\n", magic);
	}

	return -1;
}
#else
/*modif to support the sp15 64 bit sn NO */
char *get_product_sn(void)
{
	SP09_PHASE_CHECK_T *phase_check_sp09 = NULL;
	SP15_PHASE_CHECK_T *phase_check_sp15 = NULL;
	uint32_t magic = 0;

	memset(serial_number_to_transfer, 0x0, SP15_MAX_SN_LEN);
	strncpy(serial_number_to_transfer, "0123456789ABCDEF", SP15_MAX_SN_LEN);

	phase_check_sp09 = malloc_cache_aligned(sizeof(SP09_PHASE_CHECK_T));
	if (phase_check_sp09 == NULL) {
		errorf("no enough heap for phase_check_sp09\n");
		return serial_number_to_transfer;
	}
	phase_check_sp15 = malloc_cache_aligned(sizeof(SP15_PHASE_CHECK_T));
	if (phase_check_sp15 == NULL) {
		errorf("no enough heap for phase_check_sp15\n");
		free(phase_check_sp09);
		return serial_number_to_transfer;
	}
	memset(phase_check_sp09, 0, sizeof(SP09_PHASE_CHECK_T));
	memset(phase_check_sp15, 0, sizeof(SP15_PHASE_CHECK_T));

	if (0 != common_raw_read(PRODUCTINFO_FILE_PATITION, sizeof(magic), (uint64_t)0, (char *)&magic)) {
		errorf("read miscdata error.\n");
		free(phase_check_sp09);
		free(phase_check_sp15);
		return serial_number_to_transfer;
	}
	if(magic == SP09_SPPH_MAGIC_NUMBER){
		if(common_raw_read(PRODUCTINFO_FILE_PATITION,sizeof(SP09_PHASE_CHECK_T), (uint64_t)0, (char *)phase_check_sp09)){
			debugf("sp09 read miscdata error.\n");
			free(phase_check_sp09);
			free(phase_check_sp15);
			return serial_number_to_transfer;
		}
		if(strlen(phase_check_sp09->SN1)){
			memcpy(serial_number_to_transfer, phase_check_sp09->SN1, SP09_MAX_SN_LEN);
		}
	}else if(magic == SP15_SPPH_MAGIC_NUMBER){
		if(common_raw_read(PRODUCTINFO_FILE_PATITION, sizeof(SP15_PHASE_CHECK_T), (uint64_t)0 ,(char *)phase_check_sp15)){
			debugf("sp15 read miscdata error.\n");
			free(phase_check_sp09);
			free(phase_check_sp15);
			return serial_number_to_transfer;
		}
		if(strlen(phase_check_sp15->SN1)){
			memcpy(serial_number_to_transfer, phase_check_sp15->SN1, SP15_MAX_SN_LEN);
		}
	}
	free(phase_check_sp09);
	free(phase_check_sp15);
	return serial_number_to_transfer;
}

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
int set_product_sn(char *psn, int len)
{
	static const char *serialno_def = "0123456789ABCDEF";
	SP09_PHASE_CHECK_T __aligned(ARCH_DMA_MINALIGN) phase_check_sp09;
	SP15_PHASE_CHECK_T __aligned(ARCH_DMA_MINALIGN) phase_check_sp15;
	uint32_t magic;

	if (len < 0 || (len && !psn))
		return -1;

	if (0 != common_raw_read(PRODUCTINFO_FILE_PATITION, sizeof(magic),
			(uint64_t)0, (char *)&magic)) {
		errorf("read miscdata error.\n");
		return -1;
	}

	if (magic == SP09_SPPH_MAGIC_NUMBER) {
		if (common_raw_read(PRODUCTINFO_FILE_PATITION, sizeof(phase_check_sp09),
				(uint64_t)0, (char *)&phase_check_sp09)) {
			errorf("sp09 read miscdata error.\n");
			return -1;
		}

		if (len > sizeof(phase_check_sp09.SN1)) {
			errorf("set length(%d) is larger than sp09 limit.\n", len);
			return -1;
		}

		memset(phase_check_sp09.SN1, 0, sizeof(phase_check_sp09.SN1));
		if (!len)
			memcpy(phase_check_sp09.SN1, serialno_def, strlen(serialno_def));
		else
			memcpy(phase_check_sp09.SN1, psn, len);

		if (common_raw_write(PRODUCTINFO_FILE_PATITION, sizeof(phase_check_sp09),
				(uint64_t)0, (uint64_t)0, (char *)&phase_check_sp09)) {
			errorf("sp09 write miscdata error.\n");
			return -1;
		}

		return 0;
	} else if(magic == SP15_SPPH_MAGIC_NUMBER) {
		if (common_raw_read(PRODUCTINFO_FILE_PATITION, sizeof(phase_check_sp15),
				(uint64_t)0 ,(char *)&phase_check_sp15)) {
			errorf("sp15 read miscdata error.\n");
			return -1;
		}

		if (len > sizeof(phase_check_sp15.SN1)) {
			errorf("set length(%d) is larger than sp15 limit.\n", len);
			return -1;
		}

		memset(phase_check_sp15.SN1, 0, sizeof(phase_check_sp15.SN1));
		if (!len)
			memcpy(phase_check_sp15.SN1, serialno_def, strlen(serialno_def));
		else
			memcpy(phase_check_sp15.SN1, psn, len);

		if (common_raw_write(PRODUCTINFO_FILE_PATITION, sizeof(phase_check_sp15),
				(uint64_t)0, (uint64_t)0, (char *)&phase_check_sp15)) {
			errorf("sp15 write miscdata error.\n");
			return -1;
		}

		return 0;
	} else {
		errorf("unsupport magic %x.\n", magic);
	}

	return -1;
}
#endif
#endif

int g_cons_baudrate;
const int baudrate_conf_tab[] = {115200, 230400, 460800, 921600};
int get_baudrate_from_file(void)
{
	size_t i;
	char __aligned(ARCH_DMA_MINALIGN) baudrate_conf[32] = {0};
	int baudrate_f = 0;
	char *t;

	if (0 != common_raw_read("miscdata", (uint64_t)CONS_BAUDRATE_LEN, CONS_BAUDRATE_OFFSET, baudrate_conf)) {
		errorf("read miscdata loglevel data error.\n");
		return -1;
	}
	debugf("read baudrate data {%s} from miscdata\n", baudrate_conf);

	if (!strncmp("enable:", baudrate_conf, strlen("enable:"))) {
		t = baudrate_conf;
		t += strlen("enable:");
		if (!t[0]) {
			return -1;
		} else {
			baudrate_f = simple_strtoul(t, NULL, 10);
			/* Not actually changing */
			if (g_cons_baudrate == baudrate_f)
				return -1;

			for (i = 0; i < ARRAY_SIZE(baudrate_conf_tab); i++) {
				if (baudrate_conf_tab[i] == baudrate_f)
					break;
			}

			if (i == ARRAY_SIZE(baudrate_conf_tab)) {
				errorf("buadrate from file do not match any cofigure item\n");
				return -1;
			}
		}
	} else
		return -1;

	return baudrate_f;
}

void reconfig_baudrate(void)
{
	int baudrate = get_baudrate_from_file();

	if (-1 != baudrate) {
		dprintf(INFO, "reconfig console's baudrate as %d\n", baudrate);
		g_cons_baudrate = baudrate;
		sprd_uart_setbaudrate(baudrate);
		udelay(1000);
	} else {
		dprintf(ALWAYS, "cannot get baudrate from file,use default buadrate\n");
	}
}

void fdt_fixup_all(u8 *fdt_blob)
{
	const char *bootmode;

	if (fdt_check_header(fdt_blob) != 0) {
		errorf("image is not a fdt\n");
	}

#ifdef CONFIG_SPI_SLAVER_PANEL
	fdt_fixup_spi_panel_name(fdt_blob);
#endif

#ifdef CONFIG_SPLASH_SCREEN
	fdt_fixup_lcdid(fdt_blob);
	fdt_fixup_lcdname(fdt_blob);
	fdt_fixup_lcdbase(fdt_blob);
	fdt_fixup_lcdsize(fdt_blob);

	fdt_fixup_lcdbpix(fdt_blob);
#endif

	//fdt_fixup_dram_training(fdt_blob);
	fdt_fixup_ddr_size(fdt_blob);

#ifdef CONFIG_BOOTLOADER_HWFEATURE
	fdt_fixup_hwfeature(fdt_blob);
	fdt_fixup_cpuinfo_hwfeature(fdt_blob);
	fdt_fixup_soc_module(fdt_blob);
#endif

#ifdef SPRD_SYSDUMP

	fdt_fixup_sysdump_bootloader(fdt_blob);
	fdt_fixup_sysdump_magic(fdt_blob); /*hdy++*/
#endif
#ifdef CONFIG_USBPINMUX
	fdt_fixup_usbmux(fdt_blob);
#endif
	fdt_fixup_baudrate(fdt_blob);
#ifdef CONFIG_NAND_BOOT
	fdt_fixup_mtd(fdt_blob);
#ifdef CONFIG_UBI_ATTACH_MTD
	fdt_fixup_ubi_ai(fdt_blob);
#endif
#endif
#ifdef CONFIG_MEM_LAYOUT_DECOUPLING
	char modem_part[20];
	get_slot_ab(modem_part, DECOUPLING_INFO_PARTITION);

	fdt_fixup_cp_coupling_info(fdt_blob, modem_part);
#ifdef CONFIG_SUPPORT_NR
	fdt_fixup_cp_coupling_info(fdt_blob, DECOUPLING_INFO_PARTITION_NRPHY);
	fdt_fixup_cp_coupling_info(fdt_blob, DECOUPLING_INFO_PARTITION_V3PHY);
	fdt_fixup_cp_coupling_info(fdt_blob, DECOUPLING_INFO_PARTITION_PHY);
	fdt_fixup_nrphy_iqmem(fdt_blob, "nrphy-iqmem");
#endif
#endif

#if defined (CONFIG_SP_DDR_BOOT) && defined (CONFIG_MEM_LAYOUT_DECOUPLING)
	/* sp ddr boot info */
	g_res_spbootcode_info = fdt_fixup_sp_info(fdt_blob);
#endif

	/*max let cp_cmdline_fixup before fdt_fixup_cp_boot*/
#ifndef CONFIG_FPGA
	cp_cmdline_fixup();
	fdt_fixup_cp_boot(fdt_blob);
#endif

	bootmode = g_env_bootmode;
	if (NULL != bootmode)
	{
		if(!strncmp(bootmode, "sprdisk", 7)) {
			fdt_fixup_memleakon(fdt_blob);
		}
	}


#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
	fdt_fixup_oem_repair(fdt_blob);
#endif
	fdt_fixup_chosen_bootargs_board_private(fdt_blob);

#ifdef CONFIG_SANSA_SECBOOT
	//fdt_fixup_socid(fdt_blob);
#endif

	/* for non iq mode, remove reserved iq memory from dtb */
	fdt_fixup_iq_reserved_mem(fdt_blob);

#ifdef CONFIG_BOARD_KERNEL_CMDLINE
	/* board kernel cmdline in header of boot.img */
	fdt_fixup_board_kernel_cmdline(fdt_blob);
	/* board kernel cmdline in header of vendorboot.img */
	fdt_fixup_board_kernel_cmdline_from_vendorboot(fdt_blob);
#endif

#if ((defined CONFIG_FOR_LOGLEVEL) || (DEBUG))
	/* if enabled, set loglevel = 7*/
	fdt_fixup_loglevel(fdt_blob);
#endif

#if (defined(CONFIG_GET_CPU_SERIAL_NUMBER) || defined(CONFIG_GET_CPU_SERIAL_NUMBER_NO_WD))
	fdt_fixup_cpu_serial_number(fdt_blob);
#endif

	fdt_fixup_first_mode(fdt_blob);
	fdt_fixup_bootcause(fdt_blob);
	fdt_fixup_pwroffcause(fdt_blob);
	fdt_fixup_boardid_hwlevel(fdt_blob);
	fdt_fixup_charger_parameters(fdt_blob);

#ifdef DISABLE_UART
	extern int con_en;
	if(!con_en)
		fdt_close_console(fdt_blob);
#endif

#ifdef CONFIG_SENSOR_HUB_LK
	fdt_fixup_sensor_name(fdt_blob);
#endif
#ifndef CONFIG_ZEBU
	if(reboot_mode_check() != CMD_TOS_PANIC_MODE)
		fdt_fixup_tee_reserved_mem(fdt_blob);
#endif
	fdt_fixup_bootloader_log_reserved(fdt_blob);

	fdt_fixup_startup_core(fdt_blob);

	fdt_fixup_wdten(fdt_blob);
	fdt_fixup_dswdten(fdt_blob);
	fdt_fixup_dvfs_set(fdt_blob);

#ifdef CONFIG_ANDROID_AB
	fdt_fixup_add_slot_suffix(fdt_blob);
#endif

	fdt_fixup_switch_storage_probe(fdt_blob);
	fdt_fixup_emmc_swcq(fdt_blob);

#ifdef CONFIG_EMMC_WP
	extern int g_part_protected;
	//if(g_part_protected==1){
     fdt_fixup_protect_part(fdt_blob);
   //  }
#endif

	fdt_fixup_sprdboot_device(fdt_blob);
//fixup end if the parameter is not like androidboot.xxx,
//if you want to fixup androidboot.xxx please fixup in the CONFIG_BOOTCONFIG
#ifndef CONFIG_BOOTCONFIG
#if DEBUG
	fdt_fixup_selinux_switch(fdt_blob);
#endif
	fdt_fixup_dtbo_index(fdt_blob);

	fdt_fixup_ro_boot_ramsize(fdt_blob);
	fdt_fixup_ddrsize_range(fdt_blob);

//	fdt_fixup_wdten(fdt_blob);
//	fdt_fixup_dswdten(fdt_blob);

	fdt_fixup_serialno(fdt_blob);

#ifdef CONFIG_ANDROID_AB
//	fdt_fixup_add_slot_suffix(fdt_blob);
	fdt_fixup_add_force_mode(fdt_blob);
#endif

#ifndef CONFIG_ZEBU
#if defined(SPRD_UFS_BOOTDEVICE) || defined(SPRD_EMMC_BOOTDEVICE)
	fdt_fixup_boot_device(fdt_blob);
#endif
#endif

#if !WITH_SMP
	/*for verified boot*/
	fdt_fixup_verified_boot(fdt_blob);
	fdt_fixup_flash_lock_state(fdt_blob);
#ifdef SPRD_VBOOT_V2
	fdt_fixup_vboot(fdt_blob);
#endif
#endif

        fdt_fixup_vendor_init_flag(fdt_blob);
#endif

#ifdef CONFIG_TEE_FIREWALL
	fdt_reserved_mem_multimedia_parse(dt_adr);
#endif

#if 0
	fdt_fixup_pinctrl_l3(fdt_blob);
#endif

#ifdef CONFIG_CREATE_KASLR_SEED
	fdt_fixup_kaslr_seed(fdt_blob);
#endif
	fdt_fixup_ptm_reserved(fdt_blob);

#if !WITH_SMP
	fdt_print_bootargs(fdt_blob);
#endif
	return;
}

#ifdef CONFIG_BOOTCONFIG
uint64_t bootconfig_fixup_all(uint8_t *ramdisk_addr, uint64_t bootconfig_size)
{
	set_bootconfig_len(bootconfig_size);
/*
	this sample code just for boot config fixup API.
	bootconfig_size = bootconfig_fixup_vendor_init_flag(ramdisk_addr, bootconfig_size);
*/
#if DEBUG
	bootconfig_fixup_selinux_switch(ramdisk_addr);
#endif
	bootconfig_fixup_dtbo_index(ramdisk_addr);
	bootconfig_fixup_ro_boot_ramsize(ramdisk_addr);
	bootconfig_fixup_ddrsize_range(ramdisk_addr);
	bootconfig_fixup_wdten(ramdisk_addr);//sprdboot.wdten is in bootargs
	bootconfig_fixup_dswdten(ramdisk_addr);//sprdboot.dswdten is in bootargs
	bootconfig_fixup_dvfs_set(ramdisk_addr);

	bootconfig_fixup_serialno(ramdisk_addr);

#ifdef CONFIG_ANDROID_AB
	bootconfig_fixup_add_slot_suffix(ramdisk_addr);//sprdboot.slot_suffix is in bootargs
#ifdef CONFIG_SPL_DOUBLE_SLOT
	bootconfig_fixup_dual_slot_flag(ramdisk_addr);
#endif
	bootconfig_fixup_add_force_mode(ramdisk_addr);
#endif

#ifndef CONFIG_ZEBU
#if defined(SPRD_UFS_BOOTDEVICE) || defined(SPRD_EMMC_BOOTDEVICE)
        bootconfig_fixup_boot_device(ramdisk_addr);
#endif
#endif

#if !WITH_SMP
	/*for verified boot*/
	bootconfig_fixup_verified_boot(ramdisk_addr);
	bootconfig_fixup_flash_lock_state(ramdisk_addr);
#ifdef SPRD_VBOOT_V2
	bootconfig_fixup_vboot(ramdisk_addr);
#endif
#endif
	bootconfig_fixup_vendor_init_flag(ramdisk_addr);

	bootconfig_fixup_hwfeature(ramdisk_addr);

	bootconfig_fixup_high_refresh(ramdisk_addr);
	bootconfig_fixup_hmd_cmdline(ramdisk_addr);

	cp_bootconfig_fixup(ramdisk_addr);//sprdboot.mode is in bootargs

// Add by changmei.chen for hw-anti-rollback version 20241211 begin
	bootconfig_fixup_eps_state(ramdisk_addr);
// Add by changmei.chen for hw-anti-rollback version 20241211 end

#if !WITH_SMP
	print_bootconfig_param(ramdisk_addr);
#endif

	return get_bootconfig_len();
}
#endif
