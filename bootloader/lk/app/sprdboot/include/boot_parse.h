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

#ifndef _LOADER_COMMON_H_
#define _LOADER_COMMON_H_

#include "android_bootimg.h"
#include <app.h>
#include <lk/debug.h>
#include <dl_operate.h>
#include "sprd_fdt_support.h"
#include <vibrator.h>
#include <part_efi.h>

#define DT_TABLE_MAGIC 0xd7b7ab1e

#ifdef CONFIG_BOARD_KERNEL_CMDLINE
extern unsigned char vendorboot_cmdline[VENDOR_BOOT_ARGS_SIZE];
#endif

extern unsigned char raw_header[8192];

#define TRUE   1		/* Boolean true value. */
#define FALSE  0		/* Boolean false value. */

#define SPL_PART "spl"
#define LOGO_PART "logo"
#define CHARGER_LOGO_PART "chargelogo"
#define BOOT_PART "boot"
#define VENDOR_BOOT_PART "vendor_boot"
#define RECOVERY_PART "recovery"
#define FACTORY_PART "prodnv"
#define NV_LTE_PART "l_fixnv1"
#define PRODUCTINFO_FILE_PATITION  "miscdata"
#define DT_PART "dt"

/*FDT_ADD_SIZE used to describe the size of the new bootargs items*/

/*include lcd id, lcd base, etc*/
#define FDT_ADD_SIZE (0x20000)
#define TRUSTRAM_SIZE 0x32000

#define SIMLOCK_SIZE   1024

#define NV_HEAD_MAGIC	(0x00004e56)
#define NV_VERSION		(101)
#define NV_HEAD_LEN	(512)

#define MAX_SN_LEN 			(24)
#define SP09_MAX_SN_LEN			MAX_SN_LEN
#define SP09_MAX_STATION_NUM		(15)
#define SP09_MAX_STATION_NAME_LEN	(10)
#define SP09_SPPH_MAGIC_NUMBER          (0X53503039)	// "SP09"
#define SP09_MAX_LAST_DESCRIPTION_LEN   (32)

#define MODEM_MAGIC		"SCI1"
// #define MODEM_HDR_SIZE		(12 * 512) //size of a block
#define SCI_TYPE_MODEM_BIN	1
#define SCI_TYPE_PARSING_LIB	2
#define SCI_LAST_HDR		0x100
#define MODEM_SHA1_HDR		0x400
#define MODEM_SHA1_SIZE		20
#define DECOUPLING_INFO_PARTITION "modem"
#ifdef CONFIG_SUPPORT_NR
#define DECOUPLING_INFO_PARTITION_NRPHY "nrphy"
#define DECOUPLING_INFO_PARTITION_V3PHY "v3phy"
#define DECOUPLING_INFO_PARTITION_PHY "phy"
#endif

typedef enum {
	DTB_TYPE = 0,
	DTBO_TYPE,
	UNDEFINED_TYPE
} dt_img_type;

/*for verified boot*/
typedef enum verified_state{
	v_state_green = 0,
	v_state_yellow = 1,	//warning screen for LOCK devices with custom root of trust set.
	v_state_orange = 2,	//warning screen for UNLOCK devices.
	v_state_red = 3		//NO valid OS found
}enVerifiedState;

typedef enum verified_ret{
	v_state_ok = 0,
	v_state_failed = 1,
	v_state_initial = 10
}enVerifiedRet;

typedef struct _tagSP09_PHASE_CHECK {
	uint32_t Magic;	// "SP09"
	char SN1[SP09_MAX_SN_LEN];	// SN , SN_LEN=24
	char SN2[SP09_MAX_SN_LEN];	// add for Mobile
	int StationNum;		// the test station number of the testing
	char StationName[SP09_MAX_STATION_NUM][SP09_MAX_STATION_NAME_LEN];
	unsigned char Reserved[13];	//
	unsigned char SignFlag;
	char szLastFailDescription[SP09_MAX_LAST_DESCRIPTION_LEN];
	unsigned short iTestSign;	// Bit0~Bit14 ---> station0~station 14
	//if tested. 0: tested, 1: not tested
	unsigned short iItem;	// part1: Bit0~ Bit_14 indicate test Station,1 : Pass,

} SP09_PHASE_CHECK_T, *LPSP09_PHASE_CHECK_T;

/*add the struct add define to support the sp15*/
#define SP15_MAX_SN_LEN 	        (64)
#define SP15_MAX_STATION_NUM		(20)
#define SP15_MAX_STATION_NAME_LEN	(15)
#define SP15_SPPH_MAGIC_NUMBER          (0X53503135)	// "SP15"
#define SP15_MAX_LAST_DESCRIPTION_LEN   (32)

typedef struct _tagSP15_PHASE_CHECK {
    uint32_t Magic;	// "SP15"
    char SN1[SP15_MAX_SN_LEN];	// SN , SN_LEN=64
    char SN2[SP15_MAX_SN_LEN];	// add for Mobile
    int StationNum;		// the test station number of the testing
    char StationName[SP15_MAX_STATION_NUM][SP15_MAX_STATION_NAME_LEN];
    unsigned char Reserved[13];	//
    unsigned char SignFlag;
    char szLastFailDescription[SP15_MAX_LAST_DESCRIPTION_LEN];
    uint32_t iTestSign;	// Bit0~Bit14 ---> station0~station 14
    //if tested. 0: tested, 1: not tested
    uint32_t iItem;	// part1: Bit0~ Bit_14 indicate test Station,1 : Pass,

} SP15_PHASE_CHECK_T, *LPSP15_PHASE_CHECK_T;

typedef struct boot_image_required {
	char partition[PARTNAME_SZ];	//partition name record on disk
	char bak_partition[PARTNAME_SZ];	//if no backup partition, set NULL
	uint64_t size;	//partition size to be read
	char *mem_addr;	//target memory addr
} boot_image_required_t;

#define HASH_SHA256_BUF_LEN     32
typedef struct _NV_HEADER {
	uint32_t magic;
	uint32_t len;
	uint32_t checksum;
	uint32_t version;
#ifdef NV_CHECK_WITH_SHA256
	uint8_t auth[HASH_SHA256_BUF_LEN];
#endif
} nv_header_t;

#ifdef CONFIG_SP_DDR_BOOT
typedef struct sp_ddr_boot {
	uint32_t *bootcode;
	uint32_t bootcode_word_num;
	fdt_addr_t sp_iram_addr;
	fdt_addr_t sp_ddr_addr;
	fdt_size_t sp_ddr_img_size;
} sp_ddr_boot_t;
#endif

struct pre_load_operations {
	void (*power)(void);
	void (*display)(int index, int backlight_value, int lcd_enable);
	void (*vibrator)(int enable);
	void (*backlight)(uint32_t brightness);
};

struct post_load_operations {
	void (* secboot_terminal)(void);
	void (* rpmb)(void);
	void (* keymint)(void);
};

struct dt_table_header {
	uint32_t magic;
	uint32_t total_size;
	uint32_t header_size;
	uint32_t dt_entry_size;
	uint32_t dt_entry_count;
	uint32_t dt_entries_offset;
	uint32_t page_size;
	uint32_t reserved[1];
};
struct dt_table_entry {
	uint32_t dt_size;
	uint32_t dt_offset;
	uint32_t id;
	uint32_t rev;
	uint32_t custom[4];
};

typedef struct {
	uint32_t type_flags;
	uint32_t offset;
	uint32_t length;
} data_block_header_t;

typedef struct {
	char *bootimg_part;            /* partition name of boot.img */
	char *vendor_boot_part;        /* partition name of vendor boot image */
	char *init_boot_part;        /* partition name of init boot image */
	uint64_t page_size;		/* page_size */
	uint64_t hdr_offset;		/* boot_image_hdr offset on storage*/
	uint64_t kernel_offset;	/* kernel offset */
	uint64_t ramdisk_offset;	/* generic ramdisk offset on v3 */
	uint64_t ramdisk_size;	/* generic ramdisk offset on v3 */
	uint64_t dt_offset;		/* dtb offset */
	uint64_t recovery_dtbo_offset;	/* recovery dtbo/acpio */
	uint64_t vendor_hdr_offset;	/* vendor boot header offset on vendor_boot_part */
	uint64_t vendor_ramdisk_offset;/* vendor ramdisk offset */
	uint64_t vendor_bootconfig_offset;/* vendor bootconfig offset */
	uint64_t vendor_ramdisk_table_offset;/* vendor ramdisk table offset */
	char *dtb_part;		/* dtb partition name */
	char *dtbo_part;		/* dtbo partition name */
	uint64_t dt_size;		/* dtb size */
	uint64_t vendor_ramdisk_size;
	uint64_t vendor_bootconfig_size;
	uint64_t vendor_ramdisk_table_size;
	uint64_t initboot_ramdisk_offset;/* init boot  ramdisk offset */
	uint64_t initboot_ramdisk_size;/* init boot  ramdisk offset */
} boot_img_info_t;

unsigned char _chkNVEcc(uint8_t *buf, uint64_t size, uint32_t checksum);

/*uboot pre/post load interface*/
void rpmb_check(void);
void power_cfg(void);
void vibrator_load(int enable);
int sprd_set_postload(void);
int sprd_set_preload(int lcd_enable, uint32_t brightness);
int load_require_image(void);
int load_fixup_dt_img(const char *partition, uchar **dt_start_addr);
int load_kernel_ramdisk(const char *bootpartition, boot_img_hdr *hdr, uchar *dt_adr, uchar *ramdisk_adr);
int get_baudrate_from_file(void);
void reconfig_baudrate(void);
void fdt_fixup_all(u8 *fdt_blob);
#ifdef CONFIG_BOOTCONFIG
uint64_t bootconfig_fixup_all(uint8_t *ramdisk_addr, uint64_t bootconfig_size);
#endif
int boot_sprdisk(int offset, char *ramdisk_addr);

unsigned get_modem_img_info(const boot_image_required_t* img_info,
			    unsigned secure_offset,
			    int* is_sci,
			    size_t* total_len,
			    size_t* modem_exe_size);

extern char* get_calibration_parameter(void);
extern bool is_calibration_by_uart(void);
#if defined (SPRD_SECBOOT)
extern int set_lock_status(unsigned int flag);
extern unsigned int get_lock_status(void);
extern int uboot_encrypt_data(uint64_t start_addr, uint64_t lenth);
int sprd_vboot_set_display(int sec_time_count);
int loader_binding_data_set(void);
int loader_binding_state_get(void);
int sprd_sec_verify_lockstatus(unsigned char *lockstatus, unsigned int status_len);
void take_action_with_dmverity_ret(void);
int take_action_with_vbootret(void);
void secboot_unlock_display(void);
#endif

#ifdef SPRD_VBOOT_V2
int secure_get_partition_size(char * partition_name, uint64_t * size);
#endif

#ifdef CONFIG_SP_DDR_BOOT
void *get_sp_ddr_boot_info(void);
#endif

#endif /* _LOADER_COMMON_H_ */
