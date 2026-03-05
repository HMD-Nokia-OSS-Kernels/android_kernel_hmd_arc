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
#include "boot_mode.h"
#include <string.h>
#include <linux/types.h>
#include "fastboot.h"
#include <lk/debug.h>
#include <malloc.h>
#include <sprd_common.h>
#include <stdlib.h>
#include <asm/arch/check_reboot.h>
#include <part_efi.h>
#include <dl_operate.h>
#include <boot_parse.h>
#include <sprd_common_rw.h>
#include <dl_common.h>
#include <linux/usb/usb_uboot.h>
#include <sparse_format.h>
#include <bootloader_message.h>
#include <secureboot/sec_common.h>
#include <errno.h>
#include <sprd_keys.h>
#include <lcd.h>
#include <splash.h>
#include <lib/cksum.h>
#include <kernel/thread.h>
#include <sprd_ddr_memtest.h>
#include <android_ab.h>

#ifdef ZCFG_HMD_ENTERPRISE_API
//[HMDEnterpriseService] begin 2024-09-04 
#include <enterprise_api_info.h>
#include <sprd_cpcmdline.h>
//[HMDEnterpriseService] end 2024-09-04 
#endif

#if defined(CONFIG_RPMB_SECURE_WRITE_PROTECT)
#include <sprd_rpmb.h>
#endif

#ifdef CONFIG_HMD_FASTBOOT
#include "oem_fastboot_cmd.h"
#endif

#ifdef CONFIG_WR_SPARSE
#include <fb_sparse.h>
#include <arch/sprd_cache.h>
#define PART_NAME	"is-logical"
#define PART_NAME_LEN strlen(PART_NAME)
#define PART_TYPE	"partition-type"
#define PART_TYPE_LEN	strlen(PART_TYPE)
#endif

// ning.wei@hmd++ for fastboot ui display sync from solo begin
extern int key_listener_premssion;
extern void printinfo(void);
// ning.wei@hmd++ for fastboot ui display sync from solo end

#ifdef CONFIG_FASTBOOT_AUTHORIZE
//For platform/moto etc. fastboot authorize feature, same as download process, need modify constants.h FB_RESPONSE_SZ as 1024
#include <dl_cmd_proc.h>
#include <../lib/crypto/inc/authentication.h>
int fb_auth_sts = DL_AUTH_INIT;
char product_auth_key[256] __attribute__((aligned(4096)));
static char response[1024] __attribute__((aligned(64)));
#else
static char response[64] __attribute__((aligned(64)));
#endif

#ifdef CONFIG_VERIFY_GPT
#include <lk_sec_drv.h>
extern gpt_cmd_data gpt_data;
extern boot_device_t get_bootdevice(void);
#endif

extern char *get_cp_version_info(void);
extern int encryptFixnvPartition(uint8_t* ori_buf, uint32_t data_size);
extern SPECIAL_PARTITION_CFG const s_special_partition_cfg[];
extern const char *_get_backup_partition_name(const char * partition_name);
extern void lcd_printf(const char *fmt, ...);
extern char *get_dram_id(void);
extern void usb_serial_cleanup(void);
extern boot_device_t get_bootdevice(void);

#ifdef CONFIG_FASTBOOT_FLASH
extern BOOLEAN mergeItem(uint8_t * oldBuf, uint32_t oldNVlength, uint8_t * newBuf, uint32_t newNVlength);
extern BOOLEAN ___findItem( /*IN*/ uint32_t id, /*IN*/ uint8_t * nvBuf, /*IN*/ uint32_t nvLength, /*OUT*/ uint32_t * itemSize, /*OUT*/ uint32_t * itemPos);
#endif
unsigned int g_download_part_count = 0;

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
/* edl download flag offset */
#define MISCDATA_EDL_OFFSET		(9 * 1024 + 832)

#define MISCDATA_PSN_OFFSET		0//(9 * 1024 + 64)
#define MISCDATA_PSN_SZ			64//(64 + 2)
/* format: 7E+7E+.... */
#define MISCDATA_SKUID_OFFSET	(9 * 1024 + 832 + 64)
#define MISCDATA_SKUID_SZ		(16 + 2)
#define MISCDATA_WALLPP_OFFSET	(MISCDATA_SKUID_OFFSET + MISCDATA_SKUID_SZ)
#define MISCDATA_WALLPP_SZ		(2 + 2)

#define TYPE_PERMISSION_FLASH   (0)
#define TYPE_PERMISSION_REPAIR  (2)
#define TYPE_PERMISSION_SIMLOCK (1)
#endif

#define ALL_VERSION_OFFSET	(768 * 1024)//Customer customization.
#define PRODUCT_NAME_OFFSET	(768 * 1024 + 128)//Customer customization.

#define OEM_UPLOAD_FLASH_BUF	0x80000 //0x200000

#ifdef CONFIG_HMD_FASTBOOT
#define PW_KEY_PRESSED        0
#define PW_KEY_NOT_PRESSED    1
#define FLAG_BLOCKED		 (1)
#define FLAG_UNBLOCKED		 (0)
#define EMMC  0
extern int RSA_Verify(unsigned char *pub_D, unsigned char *mod_N, int bitLen_N, unsigned char *from, unsigned char *to);
extern void dumpHex(const char *title, uint8_t * data, int len);
extern int oem_repair_write_mmc_ex(const char *type,unsigned char *buf);
extern int powp_verify_and_set_flag(void);
extern void fb_cmd_reboot_edl(const char *arg, void *data, uint64_t sz);
#ifdef CONFIG_EMMC_WP
extern 	ulong mmc_set_pwr_wp(int dev_num, lbaint_t start, int grp_cnt);
#endif
#endif

enum ALL_VER{
	MODLE = 0,
	SUB_MD,
	SW_VR,
	SW_MD,
	BD_NU,
	HW_VR,
	RF_ID,
	ALL_VER_MAX,
};

#define FLASHING_LOCK_PARA_LEN (8)
#define NV_PARTITION_NAME_SIZE (36)

#ifdef CONFIG_NAND_BOOT
#include <nand.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <jffs2/load_kernel.h>

typedef struct {
	char *vol;
	char *bakvol;
} FB_NV_VOL_INFO;

static FB_NV_VOL_INFO s_nv_vol_info[] = {
	{"fixnv1", "fixnv2"},
	{"wfixnv1", "wfixnv2"},
	{"tdfixnv1", "tdfixnv2"},
	{NULL, NULL}
};

#endif

struct dl_image_inf {
	uint8_t *base_address;
	uint64_t max_size;
	uint64_t data_size;
#ifdef CONFIG_WR_SPARSE
	uint64_t max_size_raw;
	char part_name[PARTNAME_SZ];
	int first_pkt;
#endif
};

#define ROUND_TO_PAGE(x,y) (((x) + (y)) & (~(y)))

struct fastboot_cmd {
	struct fastboot_cmd *next;
	const char *prefix;
	unsigned prefix_len;
	void (*handle) (const char *arg, void *data, uint64_t sz);
};

struct fastboot_var {
	struct fastboot_var *next;
	const char *name;
	const char *value;
};

static struct fastboot_cmd *cmdlist;
struct dl_image_inf ImageInfo;
unsigned int fastboot_image_size = 0;
static unsigned char buffer[4096] __attribute__((aligned(64)));

typedef enum {
	STATE_OFFLINE = 0,
	STATE_COMMAND,
	STATE_COMPLETE,
	STATE_ERROR
} FB_USB_STATE;

uint8_t * fixnv_real_image_start_addr = NULL;
uint32_t fixnv_real_size = 0;
#ifdef FIXNV_SIGN
#define FIXNV_SIGN_HEADER_SIZE 512
typedef struct{
    uint32_t  mMagicNum;        // "BTHD"=="0x42544844"=="boothead"
    uint32_t  mVersion;         // 1
    uint8_t   mPayloadHash[MAX_HASH_BYTES_LEN]; // sha256 hash value
    uint64_t  mImgAddr;         // image loaded address
    uint32_t  mImgSize;         // image size
    uint8_t   iv_data[IV_BYTE_LEN]; // the parameter of AES crypto
    uint32_t  is_packed;        // packed image flag 0:false 1:true
    uint32_t  mFirmwareSize;    // runtime firmware size
    uint32_t  ImgRealSize;      //image real size befor sign
    uint32_t  SizeAfterSign;      //image all size after sign
    uint8_t   reserved[424];    // 424 + 17*4 +16 +4 = 512
    uint32_t  slot;             // spl double slot: slota or slotb
} FIXNV_SIGN_HEADER_T;
uint8_t * fixnv_sign_image_start_addr = NULL;
#endif

static unsigned fastboot_state = STATE_OFFLINE;
static struct fastboot_var *varlist;

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
extern int rsa_encrypt_data(unsigned char *send_data);
extern int rsa_decrypt_data(unsigned char *data_type, unsigned char *revice_data, unsigned char *sn_data);
unsigned char encrypt_data[344] = {0};
static unsigned int open_permission = 0;
extern int set_product_sn(char *psn, int len);
#endif

extern char *get_product_sn(void);
extern int usb_fastboot_init(void);
extern int usb_fastboot_exit(void);
extern int fb_usb_write(void *buf, unsigned len);
extern int fb_usb_read(void *_buf, unsigned len);
extern int fb_usb_read_triger(void *_buf, unsigned len);
extern int fb_read_usb_query_finish(void);

extern void power_down_devices(unsigned pd_cmd);
extern void reboot_devices(unsigned reboot_mode);

/*for fastboot getlcs, getsocid, setrma cmd*/
#ifdef SPRD_SECBOOT
extern unsigned int sprd_get_lcs(unsigned int *pLcs);
extern int get_lcs(uint32_t *p_lcs);
extern int get_socid(uint64_t start_addr, uint64_t lenth);
extern int set_rma(void);
extern unsigned int get_lock_status(void);
extern unsigned int check_kce_status(void);
extern int set_lock_status(unsigned int flag);
extern int get_rotpk0(uint64_t start_addr, uint64_t lenth);
extern int get_rotpk1(uint64_t start_addr, uint64_t lenth);
extern int get_secure_version(secure_version_info *ver_info);
extern int get_secdebug_bit(uint32_t *secdebug_bit);
extern unsigned int is_secure_boot_enable(void);
#endif

char product_sn_token[PRODUCT_SN_TOKEN_MAX_SIZE] __attribute__ ((aligned(4096)));
char product_sn_signature[PRODUCT_SN_SIGNATURE_SIZE] __attribute__((aligned(4096)));

// g_FbBuf default addr is 0x82000000
unsigned char *g_FbBuf = (unsigned char *)0x82000000;
uint64_t g_FbBuf_size = 0;

/* for sysdump fastboot command start*/
#ifdef SPRD_SYSDUMP
extern int sysdump_setdump_command(const char *cmd);
extern int sysdump_getdump_command(const char *cmd, int *fulldump_enable_status, int *minidump_enable_status);
#else
#define sysdump_setdump_command(cmd) -1
#define sysdump_getdump_command(cmd, fulldump_enable_status, minidump_enable_status) -1
#endif
/* for sysdump fastboot command end*/

#ifdef SPRD_DTS_MEM_LAYOUT
#define SET_FASTBOOT_BUFFER_BASE_SIZE(basep, sizep)		get_buffer_base_size_from_dt("heap@4", basep, sizep)
#endif

int set_fastboot_buf_base_size(void)
{
	unsigned long buf_base, buf_size;

#ifdef SPRD_DTS_MEM_LAYOUT
	if (SET_FASTBOOT_BUFFER_BASE_SIZE(&buf_base, &buf_size) < 0) {
		errorf("set fastboot buffer error\n");
		return -1;
	}
#else
	buf_base = FB_BUF_ADDR;
	buf_size = FB_BUF_SIZE;
#endif

	g_FbBuf = (unsigned char *)ALIGN(buf_base , 8);
	g_FbBuf_size = buf_size;

	debugf("fastboot buffer base %p, size %llx\n", g_FbBuf, g_FbBuf_size);
	debugf("in [%s] \n",__func__);
	return 0;
}

void fastboot_register(const char *prefix, void (*handle) (const char *arg, void *data, uint64_t sz))
{
	struct fastboot_cmd *cmd;
	cmd = malloc(sizeof(*cmd));
	if (cmd) {
		cmd->prefix = prefix;
		cmd->prefix_len = strlen(prefix);
		cmd->handle = handle;
		cmd->next = cmdlist;
		cmdlist = cmd;
	}
}

void fastboot_publish(const char *name, const char *value)
{
	struct fastboot_var *var;
	var = malloc(sizeof(*var));
	if (var) {
		var->name = name;
		var->value = value;
		var->next = varlist;
		varlist = var;
	}
}

void fastboot_ack(const char *code, const char *reason)
{
	if (fastboot_state != STATE_COMMAND)
		return;
	if (reason == 0)
		reason = "";

	memset(response, 0, sizeof(response));
	//snprintf(response, 64, "%s%s", code, reason);
	if (strlen(code) + strlen(reason) >= 64) {
		debugf("too long string\r\n");
	}
	sprintf(response, "%s%s", code, reason);
	fastboot_state = STATE_COMPLETE;
	fb_usb_write(response, strlen(response));
}

void fastboot_fail(const char *reason)
{
	fastboot_ack("FAIL", reason);
}

void fastboot_okay(const char *info)
{
	fastboot_ack("OKAY", info);
}

void fastboot_info(const char *format, ...)
{
	char tmp[sizeof(response) + 1];
	va_list args;

	strcpy(tmp, "INFO");
	if (format) {
		va_start(args, format);
		vsnprintf(tmp + strlen(tmp), sizeof(tmp) - strlen(tmp) - 1,
			format, args);
		va_end(args);
	}

	strlcpy(response, tmp, sizeof(response));
	fb_usb_write(response, strlen(response));

	strcpy(response, "OKAY");
	fb_usb_write(response, strlen(response));
	fastboot_state = STATE_COMPLETE;
}

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
static int fb_check_permission(int perm_type)
{
	return get_permission() & (1 << perm_type);
}
#endif

#ifdef SPRD_SECBOOT
#ifdef PRODUCT_USE_DYNAMIC_PARTITIONS
void fastboot_response_data(const char *name, const char *info)
{
	if (info == 0)
		info = "";
	if (strlen("INFO") + strlen(info) >= 64) {
		debugf("too long string\r\n");
	}
	sprintf(response, "INFO%s", info);
	fb_usb_write(response, strlen(response));
}
#else
void fastboot_response_data(const char *name, const char *info)
{
	if (info == 0)
		info = "";
	if (strlen(name) + strlen(info) >= 64) {
		debugf("too long string\r\n");
	}
	sprintf(response, "%s%s", name, info);
	fb_usb_write(response, strlen(response));
}

#endif //PRODUCT_USE_DYNAMIC_PARTITIONS
#if defined(CONFIG_FASTBOOT_SECURITY_DOWNLOAD) || defined(CONFIG_HMD_FASTBOOT)

/*
 * * Function: fb_verify_unlockkey
 * * Description: verify unlock digital signature unlock.key by decode it and compare with serial number
 * * Calls: RSA_Verify
 * * Called By:fb_cmd_flash fb_cmd_oem
 * * Return: 1- signature is valid
 * *   0- signature is invalid
 *              */
int fb_verify_unlockkey(unsigned char *encrypt_data, unsigned char *serialno)
{
    int ret = -1;
    int32_t length = -1;
            unsigned char revice_raw_data[32] = {0};

            unsigned char pub_E[4] = {0x00, 0x01, 0x00, 0x01};
#ifdef CONFIG_HMD_FASTBOOT
#if defined(CONFIG_HMD_SELFKEY)	
            unsigned char mod_N[256] =
                    { 0xb7,0x30,0xe7,0xd8,0x51,0xc9,0x7b,0x83,0xca,0xf2,0x04,0x4c,0xd9,0x05,0xa6,0x92,0xcd,0x51,0xc7,0x49,0xc3,0xc9,0x2b,0x30,0x64,0xcd,0x6b,0x70,0x15,0x59,0xf7,0x2b,0x97,0x05,0x3b,0x73,0x04,0xb8,0x86,0x1e,0x2b,0xb4,0xa7,0xf0,0x73,0x5c,0x70,0xb3,0x3f,0xfe,0x74,0x58,0xf8,0xb1,0x6d,0x2d,0x98,0x53,0xcd,0x1d,0x95,0x91,0x97,0x8b,0x48,0x6c,0xa2,0x51,0xe2,0x6c,0x12,0x02,0x9a,0x24,0xbf,0x69,0xc2,0x09,0x7b,0x5a,0x86,0x0d,0x64,0xeb,0x43,0xd3,0xea,0x99,0xeb,0xa5,0x76,0x1c,0xba,0xda,0x7c,0x1a,0x74,0x77,0x53,0x2d,0xe4,0xae,0x71,0x4a,0x8c,0x6c,0xc6,0xff,0x2c,0x8d,0x7a,0xbb,0x32,0x51,0x8d,0x04,0x22,0xaf,0x7e,0xc8,0xc1,0xdd,0x75,0x26,0xf9,0xb4,0xfb,0xf1,0x8b,0x89,0x67,0xaa,0x77,0x58,0x88,0xc7,0xc4,0x5c,0x2f,0xda,0x18,0xad,0x90,0xa3,0xab,0x77,0x10,0x05,0xc2,0x5b,0xd6,0x54,0x8c,0xde,0x60,0xb8,0x69,0x24,0x9c,0x1a,0xdc,0x82,0x09,0xb7,0x5b,0x49,0x95,0x60,0x39,0xb1,0x51,0x93,0xc2,0x92,0x78,0x8f,0xbb,0x6a,0x1f,0x75,0xa7,0xce,0x4d,0x12,0x0e,0x10,0xf0,0x21,0xa5,0xdf,0xea,0x0e,0x68,0x10,0x10,0x24,0x70,0xb3,0xa6,0x12,0xcd,0xcb,0x44,0xa9,0x18,0x1c,0x54,0xee,0x16,0xda,0xd5,0x98,0xe2,0x0f,0x0b,0x3a,0x2c,0x9b,0x43,0x0e,0x48,0x84,0xe6,0xa8,0x67,0xc7,0x37,0x5b,0xc8,0x44,0x10,0xb3,0xa1,0xd6,0x2b,0xe7,0x6c,0x93,0xf5,0x9c,0xd2,0x39,0xe0,0x87,0x30,0x0e,0xfc,0x77,0x98,0x69,0x33,0xee,0xa1,0xa8,0xf9,0x03 };
#else					
            unsigned char mod_N[256] =					
					{ 0xD1,0xE8,0xB4,0x72,0xF9,0x23,0xBC,0x72,0xBF,0xBB,0xD1,0xF9,0x8B,0xFF,0xF1,0xBF,0x44,0x0D,0xEB,0x1D,0x13,0x7B,0xC9,0x21,0x98,0xB5,0x71,0x46,0x7C,0x49,0x75,0x71,0x33,0x37,0x0A,0x42,0xB7,0x74,0x33,0xA0,0x90,0xD9,0xF2,0xF0,0x97,0x0E,0xC1,0xA7,0xCE,0x15,0xEE,0xC3,0xEA,0x7D,0xC9,0x73,0x82,0xD1,0x0D,0x5C,0x8D,0xDF,0x0A,0x08,0x9F,0x52,0x2C,0xE7,0x17,0xC6,0x4B,0xBE,0x45,0x1D,0x7C,0xBD,0xAE,0x46,0x49,0x45,0x31,0x3E,0x12,0xC0,0x19,0x1A,0x22,0xE6,0xAE,0x55,0x4E,0xEE,0xEC,0xBB,0x7A,0xF8,0x64,0x9C,0x1B,0xCD,0xE9,0x54,0xB3,0xAC,0x29,0x2A,0xF5,0xFB,0xEA,0x51,0x6E,0xB2,0xF5,0xCF,0x91,0x4B,0xA3,0x83,0x61,0x29,0xC0,0x09,0xE0,0xF0,0x5B,0xDB,0xCE,0x22,0x8E,0xB2,0xFA,0x11,0xB2,0x32,0x3D,0xD0,0xDC,0x8B,0xCF,0x44,0xB9,0x35,0x18,0x1B,0xEE,0x7B,0x10,0x10,0xE6,0xD9,0xE5,0x06,0xDE,0x96,0x57,0x63,0xE0,0xAC,0xBF,0xB1,0x1A,0xBA,0x33,0x7A,0x86,0x22,0x73,0x14,0x55,0x20,0x6D,0x4D,0x15,0xA2,0x54,0x71,0x69,0x65,0x5E,0x58,0xFD,0x9B,0xFB,0xAC,0x3B,0x36,0x84,0x11,0x7B,0x38,0x29,0x4B,0x19,0x54,0xE6,0xD8,0xAC,0xE4,0x45,0xAC,0x31,0x79,0x39,0xBB,0xFD,0xF6,0x10,0xF5,0x36,0xAC,0xAB,0x32,0xEA,0xE3,0x1C,0x7E,0x48,0xBA,0x5B,0x63,0x56,0x52,0x34,0x4E,0x18,0x07,0xDF,0xAD,0xA4,0xD8,0x35,0x11,0xE4,0x0F,0xBB,0x07,0x6C,0x90,0x39,0x6B,0xB0,0xE3,0x96,0xEE,0x92,0xA7,0xA3,0x63,0x29,0x70,0xAE,0x43,0x7D,0xCC,0xEA,0xAB };
#endif					
#else
            unsigned char mod_N[256] =
                    { 0xdd, 0x1e, 0xae, 0x44, 0xfc, 0xd4, 0xda, 0xf6, 0x7b, 0x17, 0xa9, 0x16, 0x40, 0x62, 0x9d, 0x79,
                      0xbd, 0x18, 0x94, 0x49, 0x0a, 0x6f, 0x7a, 0x6b, 0x8f, 0xa5, 0x44, 0xaa, 0x6a, 0x46, 0x4d, 0x75,
                      0x66, 0x29, 0xde, 0x1d, 0xb6, 0x37, 0x66, 0x20, 0xe1, 0xcb, 0x7c, 0x59, 0x1f, 0xe2, 0x87, 0xc6,
                      0x45, 0xe3, 0x00, 0x28, 0xc5, 0x9b, 0xc2, 0xd0, 0xc7, 0xb7, 0x51, 0x57, 0x61, 0x94, 0xaa, 0x2f,
                      0x3c, 0xb6, 0x36, 0x93, 0x65, 0x6f, 0x87, 0x49, 0x3c, 0x88, 0xa4, 0x1f, 0x43, 0x12, 0xb2, 0xc3,
                      0x80, 0x1c, 0xce, 0x49, 0x06, 0x43, 0x39, 0x3f, 0xbb, 0xf0, 0x12, 0x8a, 0x98, 0xa9, 0x60, 0x93,
                      0xa3, 0xc6, 0xfb, 0xe2, 0x4e, 0x2b, 0x61, 0xea, 0x95, 0x3d, 0x6b, 0x32, 0x65, 0xdb, 0x24, 0xba,
                      0xce, 0x2f, 0x24, 0x3d, 0xd4, 0x01, 0xa6, 0xdd, 0xf5, 0xf9, 0x27, 0x3b, 0x30, 0x28, 0x74, 0x9d,
                      0xdf, 0x20, 0xa6, 0x73, 0x06, 0xc0, 0x29, 0xc8, 0x68, 0x85, 0xeb, 0xe1, 0xa3, 0x4c, 0xe8, 0x22,
                      0x17, 0xdf, 0x43, 0xb3, 0x18, 0x29, 0x87, 0xc3, 0xc1, 0x7a, 0xd7, 0xb0, 0x42, 0x6c, 0xfe, 0x73,
                      0x61, 0x55, 0xde, 0x4a, 0xec, 0x60, 0xba, 0x6d, 0xf8, 0x9b, 0x35, 0xa7, 0x49, 0xbd, 0x24, 0xdd,
                      0x0c, 0x79, 0xbe, 0xd1, 0x26, 0x5c, 0xa1, 0x5b, 0x9f, 0x3e, 0x6a, 0x1a, 0x7c, 0xc8, 0x13, 0x42,
                      0x4c, 0x6f, 0x62, 0x34, 0x28, 0x25, 0x27, 0x9a, 0xcd, 0x75, 0x2e, 0xfb, 0x9a, 0x18, 0xac, 0x69,
                      0x1e, 0x35, 0x2a, 0x91, 0xca, 0xfb, 0x68, 0x32, 0x6e, 0xb5, 0x20, 0x1e, 0x9c, 0xaa, 0xbf, 0x98,
                      0x38, 0xa5, 0x7a, 0x65, 0x23, 0x92, 0xf9, 0xea, 0x23, 0x7c, 0x33, 0x19, 0xa3, 0x85, 0x70, 0x82,
                      0xbb, 0x5e, 0x99, 0x5d, 0x83, 0x66, 0x42, 0xaa, 0xb1, 0x72, 0x4d, 0x64, 0x4f, 0x41, 0x12, 0x9b };
#endif
            length = RSA_Verify(pub_E, mod_N, 2048, encrypt_data, revice_raw_data);

            if (0 > length)
            {
                errorf("rsa verify dec failed err:%d\n", length);
                ret = -1;
                return ret;
            }

            dumpHex("revice_raw_data", revice_raw_data, 32);
            dumpHex("plaintext", serialno, 32);

            if(!strncmp(revice_raw_data, serialno, length))
            {
                dprintf(INFO,"veriy serial number data sucess\n");
                ret = 1;
            } else {
                errorf("veriy serial number data failed\n");
                ret = 0;
            }

            return ret;
}
#endif
#endif

void fail_and_enter_fastboot_mode(void)
{
	usb_serial_cleanup();
	usb_driver_exit();
	fastboot_mode();
}

void fb_cmd_get_dram_id(void)
{
	char *dram_type = NULL;

	dram_type = get_dram_id();
	if (dram_type) {
		fastboot_okay(dram_type);
	} else {
		fastboot_fail("Not found ddr_id");
	}
	return;
}

void fb_cmd_get_baseband(const char *arg, void *data, uint64_t sz)
{
	char baseband_str[64] = {0};

	dprintf(INFO,"enter baseband-version\n");

	strcpy(baseband_str, get_cp_version_info());
	dprintf(INFO,"baseband_version:%s\n", baseband_str);
	if (baseband_str[0] == '\0') {
		fastboot_fail("get baseband verision fail");
		return;
	} else {
		fastboot_okay(baseband_str);
		return;
	}
}

static void fb_cmd_getvar(const char *arg, void *data, uint64_t sz)
{
	struct fastboot_var *var;

	for (var = varlist; var; var = var->next) {
		if (!strcmp(var->name, arg)) {
			fastboot_okay(var->value);
			return;
		}
	}

#ifdef SPRD_SECBOOT
	if (!strcmp("serialno", arg)) {
		strncpy(product_sn_token, get_product_sn(), PRODUCT_SN_TOKEN_MAX_SIZE-1);
		fastboot_okay(product_sn_token);
		return;
	} else if (!strcmp("socid", arg)) {
		fb_cmd_getsocid(arg, data, sz);
		return;
	} else if (!strcmp("lcs", arg)) {
		fb_cmd_getlcs(arg, data, sz);
		return;
	} else if (!strcmp("secure-enable", arg)) {
		fb_cmd_get_secure_enable(arg, data, sz);
		return;
	} else if (!strcmp("secure-debug", arg)) {
		fb_cmd_get_secdebug_bit(arg, data, sz);
		return;
	} else if (!strcmp("rotpk0", arg)) {
		fb_cmd_get_rotpk0(arg, data, sz);
		return;
	} else if (!strcmp("rotpk1", arg)) {
		fb_cmd_get_rotpk1(arg, data, sz);
		return;
	}  else if (!strcmp("kce_status", arg)) {
		fb_cmd_check_kce_status(arg, data, sz);
		return;
	}  else if (!strcmp("rpmb-imgver", arg)) {
		fb_cmd_get_vboot_imgversion(arg, data, sz);
		return;
	}  else if (!strcmp("efuse-imgver", arg)) {
		fb_cmd_get_secure_version(arg, data, sz);
		return;
	}
#endif

	if (!strcmp("product", arg)) {
		char product_name[65];

		memset(product_name, 0, sizeof(product_name));

		if (0 != common_raw_read("miscdata", (uint64_t)(sizeof(product_name) - 1),
				(uint64_t)PRODUCT_NAME_OFFSET, product_name)) {
			errorf("<miscdata> read error\n");
			fastboot_fail("read miscdata fail!");
			return;
		}

		debugf("product_name: %s\n", product_name);
		fastboot_okay(product_name);

		return;
	}

	if (!strncmp("partition-size:", arg, strlen("partition-size:"))) {
		disk_partition_t part_info = {0};
		char response[64];
		uint64_t size;
		int ret;

		ret = get_img_partition_info(arg + strlen("partition-size:"), &part_info);
		if (ret) {
			errorf("get partition info fail, %d\n", ret);
			fastboot_fail("get partition info fail");
			return;
		}
		size = part_info.blk_cnt * part_info.blksz;
		debugf("partition-size:0x%llx \n", size);
		sprintf(response, "0x%llx", size);
		fastboot_okay(response);
		return;
	}

	if (!strncmp("partition-type:", arg, strlen("partition-type:"))) {
		disk_partition_t part_info = {0};
		char response[64];
		int ret;

		ret = get_img_partition_info(arg + strlen("partition-type:"), &part_info);
		if (ret) {
			errorf("get partition info fail, %d\n", ret);
			fastboot_fail("get partition info fail");
			return;
		}
		debugf("partition-type:%s \n", part_info.platform_type);
		sprintf(response, "%s", part_info.platform_type);
		fastboot_okay(response);
		return;
	}

	if (!strcmp("slot-count", arg)) {
		struct bootloader_control abc;
		ulong abc_offset, abc_size;
		int ret;
		char response[64];

		memset(&abc, 0, sizeof(struct bootloader_control));
		abc_offset = offsetof(struct bootloader_message_ab, slot_suffix);
		abc_size = sizeof(struct bootloader_control);

		ret = common_raw_read("misc", (u64)abc_size, (u64)abc_offset, (char *)&abc);
		if (ret < 0) {
			errorf("ANDROID: Could not read from boot ctrl partition\n");
			fastboot_fail("Could not read from boot ctrl partition");
			return;
		}

		debugf("nb_slot:%d \n", abc.nb_slot);

		sprintf(response, "%d", abc.nb_slot);

		fastboot_okay(response);
		return;
	}

	if (!strcmp("current-slot", arg)) {
		struct bootloader_control abc;
		ulong abc_offset, abc_size;
		int ret;
		char response[64];

		memset(&abc, 0, sizeof(struct bootloader_control));
		abc_offset = offsetof(struct bootloader_message_ab, slot_suffix);
		abc_size = sizeof(struct bootloader_control);

		ret = common_raw_read("misc", (u64)abc_size, (u64)abc_offset, (char *)&abc);
		if (ret < 0) {
			errorf("ANDROID: Could not read from boot ctrl partition\n");
			fastboot_fail("Could not read from boot ctrl partition");
			return;
		}
		debugf("current-slot:%c \n", abc.slot_suffix[1]);

		sprintf(response, "%c", abc.slot_suffix[1]);

		fastboot_okay(response);
		return;
	}
#ifdef CONFIG_HMD_FASTBOOT
	if (!strcmp("off-mode-charge", arg)) {
		char flag[1] = {0};
		char flag2[1] = {0};
		char response[64];
		memset(flag, 0, sizeof(flag));
		debugf("off-mode-charge flag 1 len is:%d  , %d\n", strlen(flag), sizeof(flag));
		if (common_raw_read("miscdata",(uint64_t)MISCDATA_OFF_MODE_CHARGE_FLAG_DATA_LEN, (uint64_t)MISCDATA_OFF_MODE_CHARGE_FLAG_BASE, flag)) {
			errorf("read off-mode-charge flag error!\n");
		}
		if (common_raw_read("miscdata",(uint64_t)MISCDATA_HMD_OFF_MODE_CHARGE_FLAG_DATA_LEN, (uint64_t)MISCDATA_HMD_OFF_MODE_CHARGE_FLAG_BASE, flag2)) {
			errorf("read hmd_off_mode_charge flag2 error!\n");
		}
//		memcmp(tmp, flag, sizeof(flag));
		debugf("off-mode-charge flag is:%d,hmd_off_mode_charge :%d\n", flag[0], flag2[0]);
		sprintf(response, "fastboot:%d,app:%d", flag[0], flag2[0]);
		fastboot_okay(response);
		return;
	}
#endif	

#ifdef CONFIG_HMD_FASTBOOT
// just for test.....
	#ifdef FASTBOOT_TEST_JX
	if (!strcmp(arg, "lock_jx")) {
		if (get_lock_status() == VBOOT_STATUS_LOCK) {
			debugf("Bootloader has been locked! Flashing lock is not allowed!\n");
			fastboot_fail("Flashing lock is not allowed!");
			return;
		}
		if (auth_flag != 1) {
			//lcd_printf("\n\n   bootloader lock is not allowed! auth_flag=%d!\n", auth_flag);
			fastboot_fail("bootloader lock is not allowed!");
			return;
		}
		if (0 != common_raw_erase("userdata", 0, 0)) {
        	debugf("erase userdata failed\n");
           	fastboot_fail("Erase userdata fail.");
           	return;
     	}
		if (0 != common_raw_erase("metadata", 0, 0)) {
			debugf("erase metadata failed\n");
			fastboot_fail("Erase metadata fail.");
			return;
		}
		if (set_lock_status(VBOOT_STATUS_LOCK)) {
			debugf("execute <fb_cmd_getvar lock_jx> command fail.\n");
			fastboot_fail("Lock bootloader fail.");
			return;
		}
		
		debugf("execute <fb_cmd_getvar lock_jx> command successfully.\n");
		lcd_printf("   Info:Lock bootloader success!\n");
		fastboot_okay("Lock bootloader successfully!   ");
	} 
	else if (!strcmp(arg, "unlock_jx")) {
		if (get_lock_status() == VBOOT_STATUS_UNLOCK) {
			debugf("Bootloader has been unlocked! Flashing unlock is not allowed!\n");
			fastboot_fail("Flashing unlock is not allowed!");
			return;
		}
		if (auth_flag != 1) {
			//lcd_printf("\n\n   bootloader unlock is not allowed! auth_flag=%d!\n", auth_flag);
			fastboot_fail("bootloader unlock is not allowed!");
			return;
		}
		if (0 != common_raw_erase("userdata", 0, 0)) {
        	debugf("erase userdata failed\n");
           	fastboot_fail("Erase userdata fail.");
           	return;
     	}
		if (0 != common_raw_erase("metadata", 0, 0)) {
			debugf("erase metadata failed\n");
			fastboot_fail("Erase metadata fail.");
			return;
		}
		if (set_lock_status(VBOOT_STATUS_UNLOCK)) {
			debugf("execute <fb_cmd_getvar unlock_jx> command fail.\n");
			fastboot_fail("Unlock bootloader fail.");
			return;
		}
		
		debugf("execute <fb_cmd_getvar unlock_jx> command successfully.\n");
		lcd_printf("   Info:Unlock bootloader success!\n");
		fastboot_okay("Unlock bootloader successfully!   ");
	}
	else if (!strcmp(arg, "powp_enable_jx")) {
		mmc_set_pwr_wp(EMMC,0x00024000,1);  
	}
	#endif
	if (!strcmp(arg, "reboot_edl")) {
		if (auth_flag != 1) {
			//lcd_printf("\n\n   reboot_edl is not allowed! auth_flag=%d!\n", auth_flag);
			fastboot_fail("permission denied,auth needed!");
			return;
		}
		fb_cmd_reboot_edl(NULL,NULL,NULL);
		debugf("execute <fb_cmd_getvar reboot_edl> command successfully.\n");
	}
	else if (!strcmp(arg, "power_down_jx")) {	
		fastboot_okay("");
		usb_driver_exit();
		printf("fb_cmd_getvar power_down");
		//reboot_devices(CMD_CALIBRATION_MODE);
		//reboot_devices(CMD_POWER_DOWN_DEVICE);
		//cmd_oem_enter_calibration(NULL,NULL,NULL);
		power_down_devices(0);
	}
	#ifdef FASTBOOT_TEST_JX
	else if (!strcmp(arg, "auth_disable_jx")) {
		auth_flag=0;//1=enable action    0=disable action
	} 
	else if (!strcmp(arg, "auth_enable_jx")) {
		auth_flag=1;//1=enable   0=disable
	} 						
	else if (!strcmp(arg, "auth_flag_jx")) {
		char response[64];
		sprintf(response, "auth flag=%d (1=enable action,0=disable action)", auth_flag);
		fastboot_okay(response);
	}
	#endif
#endif	
	if (!strcmp("dram-type", arg)) {
		fb_cmd_get_dram_id();
		return;
	}

	if (!strcmp("baseband-version", arg)) {
		fb_cmd_get_baseband(arg, data, sz);
		return;
	}

	if (!strcmp("flash-type", arg)) {
		if (get_bootdevice() == BOOT_DEVICE_UFS)
			fastboot_okay("ufs-flash");
		else
			fastboot_okay("emmc-flash");
		dprintf(ALWAYS, "get flash-type:%x\n", get_bootdevice());
		return;
	}

	fastboot_okay("");
}

static void fb_cmd_download(const char *arg, void *data, uint64_t sz)
{
	char response[64];
	uint64_t len = simple_strtoul(arg, NULL, 16);
	int total_rcv;
#ifdef CONFIG_WR_SPARSE
	char *pname = ImageInfo.part_name;
	sparse_header_t sparse_hdr;
	const uint64_t frag_len = SZ_2M;
	uint64_t i, j, k, t;
	int sparse_start = 0;
	uint64_t last_pos, unsave_sz;
	int retval;
	uint32_t ticks_usb_one_frag = 0, ticks_flash_wr = 0, ticks_total = 0;
	uint32_t ticks_cur1, ticks_cur2;
	uint64_t last_unsave_len;
	uint64_t total_size = 0;
#endif

	debugf("Start fastboot download, image len=0x%llx\n", len);
	debugf("buffer base %p, buffer size 0x%llx\n", ImageInfo.base_address, ImageInfo.max_size);
	if (len > ImageInfo.max_size) {
		debugf("Image size over the max buffer size,can not accept \n");
		fastboot_fail("data too large");
		return;
	}

	//add by hyinfeng for NYX-3230,NYX-3172 flash part_name xxx fail,but reboot exception begin
	if (strlen(ImageInfo.part_name) && strcmp(ImageInfo.part_name, "unlock")
		&& MODE_FASTBOOT_BASIC == fastboot_mode_flag && !auth_flag && get_lock_status() == VBOOT_STATUS_LOCK) {
		debugf("permission denied, ImageInfo.part_name:%s\n", ImageInfo.part_name);						
		fastboot_fail("permission denied,auth required.");
		memset(ImageInfo.part_name, 0, sizeof(ImageInfo.part_name));
		return;
	}
	//add by hyinfeng for NYX-3230,NYX-3172 flash part_name xxx fail,but reboot exception end

	sprintf(response, "DATA%16llx", len);
	if (fb_usb_write(response, strlen(response)) < 0) {
		fastboot_state = STATE_ERROR;
		return;
	}

#ifdef CONFIG_WR_SPARSE
	if (!strlen(ImageInfo.part_name)) {
		total_rcv = fb_usb_read(ImageInfo.base_address, len);
		if (total_rcv != len) {
			fastboot_state = STATE_ERROR;
			return;
		}

		ImageInfo.data_size = len;
		fastboot_image_size = ImageInfo.data_size;
		fastboot_okay("");
		return;
	}

	if (!strcmp(ImageInfo.part_name, "userdata") && g_download_part_count == 0) {
		debugf("userdata, erase...\n");
		get_img_partition_size(ImageInfo.part_name, &total_size);
		common_raw_erase(ImageInfo.part_name, total_size / 100, (uint64_t)0LL);
		g_download_part_count += 1;
	}

	wr_dbg("Wr rest ImageInfo.part_name:%s\n", ImageInfo.part_name);
	wr_sparse_rest(ImageInfo.part_name);

	ImageInfo.first_pkt = 1;
	last_unsave_len = 0;

	i = len;
	last_pos = total_rcv = 0;
	unsave_sz = 0;
	t = 0;
	while (i) {
		ticks_cur1 = SCI_GetTickCount();

		j = i > frag_len ? frag_len : i;

		if (fb_usb_read_triger(ImageInfo.base_address + total_rcv, j)) {
			fastboot_state = STATE_ERROR;
			goto out;
		}

		if (ImageInfo.first_pkt) {
			k = fb_read_usb_query_finish();
			if (k != j) {
				fastboot_state = STATE_ERROR;
				goto out;
			}

			if (!ticks_usb_one_frag)
				ticks_usb_one_frag = SCI_GetTickCount() - ticks_cur1;

			if (ImageInfo.first_pkt) {
				ImageInfo.first_pkt = 0;
				memcpy(&sparse_hdr, ImageInfo.base_address, sizeof(sparse_hdr));

				if ((sparse_hdr.magic == SPARSE_HEADER_MAGIC)
					&& (sparse_hdr.major_version == SPARSE_HEADER_MAJOR_VER)
					&& strlen(ImageInfo.part_name)) {
					wr_dbg("First packet is sparse\n");
					sparse_start = 1;
				}
			}

			total_rcv += j;
			i -= j;
			unsave_sz += j;
			t++;

			ticks_total += SCI_GetTickCount() - ticks_cur1;
			continue;
		}

		t = 0;
		if (sparse_start) {
			ticks_cur2 = SCI_GetTickCount();

#define DCACHE_BYTE_ALIGN 0x3F
			invalidate_dcache_range(((unsigned int)ImageInfo.base_address + last_pos) & (~DCACHE_BYTE_ALIGN),
				(((unsigned int)ImageInfo.base_address + last_pos) + unsave_sz + DCACHE_BYTE_ALIGN) & (~DCACHE_BYTE_ALIGN));

			//debugf("%s fb download saving image buf %p, size %llx\n",
			//	pname, ImageInfo.base_address + last_pos - last_unsave_len, unsave_sz + last_unsave_len);
			retval = write_sparse_img(pname, ImageInfo.base_address + last_pos - last_unsave_len, unsave_sz + last_unsave_len);
			if (-1 == retval) {
				errorf("%s write packet fail, buf %p, size %lx\n", pname,
					ImageInfo.base_address + last_pos, unsave_sz);
				fastboot_state = STATE_ERROR;
				goto out;
			} else if ((retval > 0) && (retval < unsave_sz + last_unsave_len)) {
				last_unsave_len = unsave_sz + last_unsave_len - retval;
				wr_dbg("unsave_sz %llx retval %x (%llx)\n",
					unsave_sz + last_unsave_len, retval, last_unsave_len);
			} else
				last_unsave_len = 0;
			//debugf("return value=%d\n", retval);
			last_pos = total_rcv;
			unsave_sz = 0;

			ticks_flash_wr += SCI_GetTickCount() - ticks_cur2;
		}

		if (!ImageInfo.first_pkt) {
			k = fb_read_usb_query_finish();
			if (k != j) {
				fastboot_state = STATE_ERROR;
				goto out;
			}
		}

		total_rcv += j;
		i -= j;
		unsave_sz = j;

		ticks_total += SCI_GetTickCount() - ticks_cur1;
	}

	if (sparse_start && (unsave_sz + last_unsave_len)) {
		wr_dbg("%s saving last packet, buf %p, size %llx\n",
			pname, ImageInfo.base_address + last_pos - last_unsave_len, unsave_sz + last_unsave_len);

		ticks_cur2 = SCI_GetTickCount();
		invalidate_dcache_range(((unsigned int)ImageInfo.base_address + last_pos) & (~DCACHE_BYTE_ALIGN),
			(((unsigned int)ImageInfo.base_address + last_pos) + unsave_sz + DCACHE_BYTE_ALIGN) & (~DCACHE_BYTE_ALIGN));

		retval = write_sparse_img(pname, ImageInfo.base_address + last_pos - last_unsave_len, unsave_sz + last_unsave_len);
		if (-1 == retval) {
			errorf("%s write last packet fail, buf %p, size %lx\n", pname,
				ImageInfo.base_address + last_pos, unsave_sz);
			fastboot_state = STATE_ERROR;
			goto out;
		} else if ((retval > 0) && (retval < unsave_sz + last_unsave_len)) {
			last_unsave_len = unsave_sz + last_unsave_len - retval;
			errorf("unsave_sz %llx retval %x (%d)\n",
				unsave_sz + last_unsave_len, retval, last_unsave_len);
			fastboot_state = STATE_ERROR;
			goto out;
		} else
			last_unsave_len = 0;
		//debugf("return value=%d\n", retval);

		ticks_flash_wr += SCI_GetTickCount() - ticks_cur2;
		ticks_total += SCI_GetTickCount() - ticks_cur2;

#ifdef CONFIG_WRBG_SPARSE
		(void)wrbg_sparse_flush(pname); /* !!!flush backstage write */
#endif
	}

out:
	if (fastboot_state == STATE_ERROR) {
#ifdef CONFIG_WRBG_SPARSE
		(void)wrbg_sparse_flush(pname); /* !!!flush backstage write */
#endif
		errorf("%s err out\n", pname);
		reset_sparse_status();
		return;
	}

	//report
	dprintf(INFO,"got %lld bytes\n", len);
	dprintf(INFO,"elapsed time total: %d ms\n", ticks_total);
	dprintf(INFO,"%d ms per receive\n", ticks_usb_one_frag);
	dprintf(INFO,"%d ms flash\n", ticks_flash_wr);
	dprintf(INFO,"%d ms usb\n", ticks_total - ticks_flash_wr);

	ImageInfo.data_size = len;
	fastboot_image_size = ImageInfo.data_size;
	fastboot_okay("");
	return;
#else
	total_rcv = fb_usb_read(ImageInfo.base_address, len);
	if (total_rcv != len) {
		fastboot_state = STATE_ERROR;
		return;
	}

	ImageInfo.data_size = len;
	fastboot_image_size = ImageInfo.data_size;
	fastboot_okay("");
#endif
}

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
static int fb_check_permission_part_flash(char *part_name)
{
	/*
	 * userdata could be flash on base permission,
	 *   and persist was not allow alway
	 */
	if (!strcmp(part_name, "persist"))
		return 0;

	if (!strcmp(part_name, "userdata"))
		return 1;

	return fb_check_permission(TYPE_PERMISSION_FLASH);
}

static int fb_check_permission_part_erase(char *part_name)
{
	/* userdata could be flash on base permission */
	if (!strcmp(part_name, "userdata"))
		return 1;

	return fb_check_permission(TYPE_PERMISSION_REPAIR);
}
#endif

#ifdef CONFIG_FASTBOOT_FLASH
#ifdef CONFIG_HMD_FASTBOOT
static volatile unsigned int s_backupnv_flag = 1;
#else
static volatile unsigned int s_backupnv_flag = 0;
#endif
int fb_nvmerge(char *partition_name, uint8_t *buf, uint32_t size) {
	uint8_t old_header_buf[NV_HEAD_LEN];
	nv_header_t	*old_nv_header_p = NULL;
	uint32_t old_nv_size = 0;
	char *old_nv_buf = NULL;
	const char *backup_partition_name = _get_backup_partition_name(partition_name);
	memset(old_header_buf, 0x00, NV_HEAD_LEN);
	if (common_raw_read(partition_name, NV_HEAD_LEN, (uint64_t)0, old_header_buf)) {
		errorf("fail to read nv header!\n");
		return -1;
	}
	old_nv_header_p = (nv_header_t *)old_header_buf;
	if( old_nv_header_p->magic == NV_HEAD_MAGIC && old_nv_header_p->version == NV_VERSION) {
		old_nv_size = old_nv_header_p->len;
#ifndef CONFIG_FASTBOOT_N6
		old_nv_buf = malloc(old_nv_size);
#else
		old_nv_buf = (char *)FB_NV_ADDR;
#endif
		if (NULL == old_nv_buf) {
			errorf("no enough space for old nv buffer\n");
			return -2;
		}
		memset(old_nv_buf, 0x0, old_nv_size);
		debugf("old_nv_size 0x%x\n", old_nv_size);
		common_raw_read(partition_name, (uint64_t)(old_nv_size), NV_HEAD_LEN, old_nv_buf);
#ifdef NV_CHECK_WITH_SHA256
		if ((TRUE != fdl_check_sha256((uint8_t *)old_nv_buf, old_nv_size, old_nv_header_p->auth)) && (backup_partition_name != NULL)) {
#else
		if ((TRUE != fdl_check_crc((uint8_t *)old_nv_buf, old_nv_size, old_nv_header_p->checksum)) && (backup_partition_name != NULL)) {
#endif
			debugf("main nv partition %s is damaged!\n", partition_name);
			memset(old_header_buf, 0x00, NV_HEAD_LEN);
			if (common_raw_read(backup_partition_name, NV_HEAD_LEN, (uint64_t)0, (char *)old_header_buf)) {
				errorf("fail to read nv header!\n");
#ifndef CONFIG_FASTBOOT_N6
				free(old_nv_buf);
#endif
				return -3;
			}
			old_nv_header_p = (nv_header_t *)old_header_buf;
			if( old_nv_header_p->magic == NV_HEAD_MAGIC && old_nv_header_p->version == NV_VERSION) {
#ifndef CONFIG_FASTBOOT_N6
				free(old_nv_buf);
#endif
				old_nv_size = old_nv_header_p->len;
#ifndef CONFIG_FASTBOOT_N6
				old_nv_buf = malloc(old_nv_size);
#else
                                old_nv_buf = (char *)FB_NV_ADDR;
#endif
				if (NULL == old_nv_buf) {
					errorf("no enough space for old nv buffer\n");
					return -4;
				}
				if (common_raw_read(backup_partition_name, (uint64_t)(old_nv_size), NV_HEAD_LEN, old_nv_buf)) {
					errorf("fail to read nv header!\n");
#ifndef CONFIG_FASTBOOT_N6
					free(old_nv_buf);
#endif
					return -5;
				}
#ifdef NV_CHECK_WITH_SHA256
				if (TRUE != fdl_check_sha256((uint8_t *)old_nv_buf, old_nv_size, old_nv_header_p->auth)) {
#else
				if (TRUE != fdl_check_crc((uint8_t *)old_nv_buf, old_nv_size, old_nv_header_p->checksum)) {
#endif
#ifndef CONFIG_FASTBOOT_N6
					free(old_nv_buf);
#endif
					errorf("main nv partition and bakup nv partition is damaged!\n");
					return -6;
				}
			} else {
#ifndef CONFIG_FASTBOOT_N6
				free(old_nv_buf);
#endif
				errorf("back up nv partition header error!magic = %#x,version = %d\n",
					old_nv_header_p->magic, old_nv_header_p->version);
				return -7;
			}
		}

		debugf("start to mergeitem, old_nv_size 0x%x, new_nv_size 0x%x\n", old_nv_size, size);
		if(!mergeItem((uint8_t *)old_nv_buf, old_nv_size, buf, size)) {
#ifndef CONFIG_FASTBOOT_N6
			free(old_nv_buf);
#endif
			errorf("nv merge fail!\n");
			return -9;
		}
#ifndef CONFIG_FASTBOOT_N6
		free(old_nv_buf);
#endif
	} else {
		debugf("The current nv partition is empty, Nv cannot be backed up!\n");
	}
	return 0;

}

/* clear g_status.unsave_recv_size before return if necessary */
int fb_write_nv_img(char *part_name, uint8_t *buf, uint32_t size, unsigned int backup_flag)
{
	char *part_name_bak = NULL;
	uint8_t header_buf[NV_HEADER_SIZE];
#ifdef CONFIG_ANDROID_AB
 	char part_name_without_ab[NV_PARTITION_NAME_SIZE] = {0};
 	char part_name_with_ab[NV_PARTITION_NAME_SIZE] = {0};
	uint8_t string_size = 0;
#endif
	nv_header_t *nv_header_p = NULL;
	int retval = 0;

	debugf("fb_write_nv_img enter, size 0x%x\n", size);
#ifdef NV_ENCRYPTION
	encryptFixnvPartition(buf, size);
#endif

	debugf("fb_write_nv_img backup_flag %d\n", backup_flag);
	if (backup_flag) {
#ifdef CONFIG_ANDROID_AB
		string_size = strlen(part_name);
		if (string_size > strlen("_a")) {
			strncpy(part_name_without_ab, part_name, string_size - strlen("_a"));
		} else {
			return -1;
		}
		if ((*(part_name + string_size - 1) == 'a') || (*(part_name + string_size - 1) == 'b')) {
			get_slot_ab(part_name_with_ab, part_name_without_ab);
			debugf("part_name: %s\n", part_name);
			debugf("part_name_without_ab: %s\n", part_name_without_ab);
			debugf("part_name_with_ab: %s\n", part_name_with_ab);
			retval = fb_nvmerge(part_name_with_ab, buf, size);
		} else {
			return -1;
		}
#else
		retval = fb_nvmerge(part_name, buf, size);
#endif
		debugf("fb_nvmerge return %d\n", retval);
	}

	memset(header_buf, 0x00, NV_HEADER_SIZE);
	nv_header_p = (nv_header_t *)header_buf;
	nv_header_p->magic = NV_HEAD_MAGIC;
	nv_header_p->len = size;
	nv_header_p->checksum = (uint32_t)fdl_calc_checksum(buf, size);
	nv_header_p->version = NV_VERSION;
#ifdef NV_CHECK_WITH_SHA256
	fdl_get_sha256(buf, size, nv_header_p->auth);
#endif
	debugf("Start to write first block of NV partition(%s)\n", part_name);
	if (0 != common_raw_write(part_name, NV_HEADER_SIZE, (uint64_t)0, (uint64_t)0, (char *)header_buf))
		return -1;

	debugf("Start to write remain blocks of NV partition\n");
	if (0 != common_raw_write(part_name, (uint64_t)size, (uint64_t)0, NV_HEADER_SIZE, (char *)buf))
		return -1;

	/*write the backup partition */
	part_name_bak = _get_backup_partition_name((unsigned char*)part_name);
	if (NULL == part_name_bak) {
		errorf(" get backup partition name fail\n");
		return -1;
	}
	if (0 != common_raw_write(part_name_bak, NV_HEADER_SIZE, (uint64_t)0, (uint64_t)0, (char *)header_buf))
		return -1;

	if (0 != common_raw_write(part_name_bak, (uint64_t)size, (uint64_t)0, NV_HEADER_SIZE, buf))
		return -1;

	return 0;
}
#endif

#ifndef CONFIG_NAND_BOOT
static PARTITION_IMG_TYPE fb_get_partition_image_type(char *part_name,
	unsigned char *buf, ulong sechdr_offset)
{
	PARTITION_IMG_TYPE img_format = IMG_RAW;
	sparse_header_t *sparse_header;

	sparse_header = (sparse_header_t *)(buf + sechdr_offset);
	if ((sparse_header->magic == SPARSE_HEADER_MAGIC)
			&& (sparse_header->major_version == SPARSE_HEADER_MAJOR_VER)) {
		img_format = IMG_WITH_SPARSE;
		debugf("img_format = IMG_WITH_SPARSE\n");
	}

	return img_format;
}

static int _fb_repartition(uint8_t *pgpt, uint64_t size)
{
	block_dev_desc_t *dev_desc;
	int dev_id = 0;
	char *ifname;

	ifname = block_dev_get_name();
	dev_id = get_devnum_hwpart(ifname, 0);
	if (dev_id < 0) {
		errorf("%s: get dev_id %d fail\n", __func__, dev_id);
		return -1;
	}

	dev_desc = get_dev_hwpart(ifname, dev_id, 0);
	if (!dev_desc) {
		errorf("%s: get dev_desc fail\n", __func__);
		return -2;
	}

	debugf("%s: updating MBR, Primary and Backup GPT(s)\n", __func__);
	if (write_mbr_and_gpt_partitions(dev_desc, pgpt)) {
		errorf("Write pgpt fail(0)\n");
		return -3;
	}
	debugf("Write pgpt successfully\n");
	return 0;
}

#ifdef CONFIG_HMD_FASTBOOT
static int check_miscdata_blocked_flag(void)
{
	debugf("check miscdata is blocked or unblocked.\n");
	char buf[1] = {0};
	// Modify by lihongxiang for fastboot erase metadata on 2022/6/8 begin
	uint64_t offset = MISCDATA_HMD_BLOCK_FACTORY_RESET_FLAG_BASE;
	// Modify by lihongxiang for fastboot erase metadata on 2022/6/8 end
	int length = 1;
	if (common_raw_read("miscdata", (uint64_t)length, offset, buf)) {
		errorf("read miscdata %d fail on offset %lld.\n", length, offset);
		return FLAG_UNBLOCKED;
	}
	debugf("msicdata offset:%lld blocked flag:%x.\n", offset, *buf);
	// Modify by lihongxiang for fastboot erase metadata on 2022/6/8 begin
	extern int chrtodec(char chr);
	if (chrtodec(*buf) == 1) {
		return FLAG_BLOCKED;
	}
	// Modify by lihongxiang for fastboot erase metadata on 2022/6/8 end
	return FLAG_UNBLOCKED;
}
#endif
//[HMDEnterpriseService] for error code showing begin 2025-03-04 
#ifdef ZCFG_HMD_ENTERPRISE_API
bool isBlockFactoryReset(void)
{
    char block_factory_reset[1];
    char isBlocked[1] = "1";
    if (0 != common_raw_read(ENTERPRISEAPIINFO_PARTITION_NAME, (uint64_t)1, BLOCK_FACTORY_RESET_OFFSET, block_factory_reset)) {
        errorf("read error\n");
    } else {
        debugf("block_factory_reset=%s\n",block_factory_reset);
        if (!memcmp(isBlocked, block_factory_reset, sizeof(block_factory_reset))) {
            return true;
        }
    }
    return false;    
}
bool isBlockDownloadMode(void)
{
    char block_download[1];
    char isBlocked[1] = "1";
    if (0 != common_raw_read(ENTERPRISEAPIINFO_PARTITION_NAME, (uint64_t)1, BLOCK_DOWNLOAD_MODE_OFFSET, block_download)) {
        errorf("read error\n");
    } else {
        debugf(" block_download=%s\n",block_download);
        if (!memcmp(isBlocked, block_download, sizeof(block_download))) {
            return true;
        }
    }
    return false;
}
#endif
//[HMDEnterpriseService] for error code showing end 2025-03-04 
void fb_cmd_flash(const char *arg, void *data, uint64_t sz)
{
	PARTITION_IMG_TYPE img_format = IMG_RAW;
	OPERATE_STATUS status = OPERATE_SUCCESS;
	char part_name[PARTNAME_SZ];
	PARTITION_PURPOSE part_purpose;
	static sys_img_header *bakup_header;
	ulong sec_offset = 0x200;
	uint32_t write_size = 0;
	uint8_t * write_start = NULL;
	int32_t retval = 0, i;
	uint64_t total_size = 0;
#ifdef CONFIG_HMD_FASTBOOT
    char subcmd[64];
    const char *delim = " ";
    int ret = -1;
	debugf("data = %p,ImageInfo[0].base_address = %p\n", data, ImageInfo.base_address);
    memset(subcmd, 0, sizeof(subcmd));
    strncpy(subcmd, strtok(arg, delim), sizeof(subcmd)-1);

	if (!strcmp(subcmd, "powp")){
		debugf("POWP:fb_cmd_flash powp 1\n");
		if(MISCDATA_POWP_DATA_LEN < ImageInfo.data_size){
			fastboot_fail("flash powp.bin  failed\n");
			return ;
		}
		powp_size = ImageInfo.data_size;
		debugf("POWP:fb_cmd_flash powp 2 %d\n",powp_size);		
		memcpy(powp_buf,ImageInfo.base_address,powp_size);
		debugf("POWP:fb_cmd_flash powp 3 %s\n",powp_buf);		
		
		ret = oem_repair_write_mmc_ex("powp",powp_buf);
		if(!ret && !powp_verify_and_set_flag()) {
		    fastboot_okay("");
			
			debugf("POWP:fb_cmd_flash powp okay ret=%d\n",ret);
			//udelay(500);
		    usb_driver_exit();
			/* the last time to write log before reboot */
			//write_log_last();
		    reboot_devices(CMD_FASTBOOT_MODE);
		} else {
		    fastboot_fail("flash powp.bin  failed\n");
		}

		return;
	} else if (!strcmp(subcmd, "unlock")){
        strncpy(product_sn_token, get_product_sn(), PRODUCT_SN_TOKEN_MAX_SIZE-1);
        debugf("Product SerialNo: %s\n", product_sn_token);
        //debugf("Product SerialNo sign sizeof : %d , %d  \n", sizeof(product_sn_signature),PRODUCT_SN_SIGNATURE_SIZE);
        //debugf("Product SerialNo sign 1: %s\n", product_sn_signature);
        memset(product_sn_signature, 0, sizeof(product_sn_signature));
        memcpy(product_sn_signature, data, PRODUCT_SN_SIGNATURE_SIZE);
        //debugf("Product SerialNo sign 2: %s\n", product_sn_signature);

        ret = fb_verify_unlockkey(product_sn_signature, product_sn_token);
        if(ret == 1) {
            fastboot_okay("flash unlock success\n");
        } else {
            fastboot_fail("flash unlock failed\n");
        }

        return;
//modify by ysong for cali fastboot begin
    }else if(!strcmp(subcmd, "diag")){ 
		debugf("DIAG:fb_cmd_flash CALI 1\n");
		if(MISCDATA_CALI_DATA_LEN < ImageInfo.data_size){
			fastboot_fail("flash diag.bin  failed\n");
			return ;
		}
		powp_size = ImageInfo.data_size;
		debugf("DIAG:fb_cmd_flash diag 2 %d\n",powp_size);		
		memcpy(cali_buf,ImageInfo.base_address,powp_size);
		debugf("DIAG:fb_cmd_flash diag 3 %s\n",cali_buf);		
		
		ret = oem_repair_write_mmc_ex("cali",cali_buf);
		if(!ret) {
		    fastboot_okay("");
			
			debugf("CALI:fb_cmd_flash diag okay ret=%d\n",ret);
			//udelay(500);
		    usb_driver_exit();
			/* the last time to write log before reboot */
			//write_log_last();
		    reboot_devices(CMD_FASTBOOT_MODE);
		} else {
		    fastboot_fail("flash diag.bin  failed\n");
		}

		return;
    }
//modify by ysong for cali fastboot end
#else
#if defined(CONFIG_FASTBOOT_SECURITY_DOWNLOAD) || defined(CONFIG_FASTBOOT_AUTHORIZE)
        char subcmd[64];
        const char *delim = " ";
        int ret = -1;
#endif

	debugf("data = %p,ImageInfo[0].base_address = %p, ImageInfo.data_size = 0x%016lx, sz = 0x%016lx\n",
			data, ImageInfo.base_address, ImageInfo.data_size, sz);

#if defined(CONFIG_FASTBOOT_SECURITY_DOWNLOAD) && defined(SPRD_SECBOOT)
    memset(subcmd, 0, sizeof(subcmd));
    strncpy(subcmd, strtok(arg, delim), sizeof(subcmd)-1);

    if (!strcmp(subcmd, "unlock")){
        strncpy(product_sn_token, get_product_sn(), PRODUCT_SN_TOKEN_MAX_SIZE-1);
        debugf("Product SerialNo: %s.", product_sn_token);
        memset(product_sn_signature, 0, sizeof(product_sn_signature));
        memcpy(product_sn_signature, data, PRODUCT_SN_SIGNATURE_SIZE);

        ret = fb_verify_unlockkey(product_sn_signature, product_sn_token);
        if(ret == 1) {
            fastboot_okay("flash unlock success\n");
        } else {
            fastboot_fail("flash unlock failed\n");
        }

        return;
    }
#endif

#ifdef CONFIG_FASTBOOT_AUTHORIZE
	memset(subcmd, 0, sizeof(subcmd));
	strncpy(subcmd, strtok(arg, delim), sizeof(subcmd)-1);

	if (!strcmp(subcmd, "auth_key")){
		memset(product_auth_key, 0, sizeof(product_auth_key));
		memcpy(product_auth_key, data, sizeof(product_auth_key));
		fastboot_okay("flash auth_key success\n");
		return;
	}
#endif

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
#if defined (SPRD_SECBOOT)
	if (!fb_check_permission_part_flash(arg)
			&& (get_lock_status() == VBOOT_STATUS_LOCK)) {
		fastboot_fail("flash Not allow! Please unlock or get permission first!");
		return;
	}
#else
	if (!fb_check_permission_part_flash(arg)) {
		fastboot_fail("Not allow!");
		return;
	}
#endif
#else
#if defined (SPRD_SECBOOT) && !(DEBUG)
	if(get_lock_status() == VBOOT_STATUS_LOCK) {
		fastboot_fail("Flashing Lock Flag is locked. Please unlock it first!");
		return;
	}
#endif
#endif

#ifdef CONFIG_FASTBOOT_AUTHORIZE
	//For example:set persist as must need auth partition
	if (!strcmp(part_name, "persist")) {
		if (fb_auth_sts != DL_SECURE_VERIFY_PASS) {
			errorf("Not allow flash %s, need authorize!\n", part_name);
			fastboot_fail("Not allow flash, need authorize!");
			return;
		}
	}
#endif
#endif

	for (i = 0; i < PARTNAME_SZ; i++) {
		part_name[i] = arg[i];
		if (0 == arg[i])
			break;
	}

#ifdef CONFIG_VERIFY_GPT
	/* gpt-sign.bin structure:|MBR||GPT header||GPT entry||Verify header||Verify key| */
	char *gpt_sign_data = NULL;
	uint32_t prim_gpt_offset = 0;//GPT MBR and header
	gpt_entry *gpt_e = gpt_data.entry;
	static sys_img_header *verify_header;
	int gpt_verify_header = 0;
	uchar dpart_name[36] = {0};
	int ret;
	u32 gpt_enable = 0;

	gpt_enable = !check_gpt_efuse();
	if (!strcmp(part_name, "partition") || !strcmp(part_name, "gpt")) {
		if (get_bootdevice() == BOOT_DEVICE_UFS)
			prim_gpt_offset = 0x2000; //ufs
		else if (get_bootdevice() == BOOT_DEVICE_EMMC)
			prim_gpt_offset = 0x400; //eMMC
		else {
			fastboot_fail("Secboot verify fail!");
			return;
		}

		memcpy(gpt_data.header, data+prim_gpt_offset+SZ_E, SZ_H);
		memcpy(gpt_data.entry, data+prim_gpt_offset, SZ_E);
		memcpy(gpt_data.key, data+prim_gpt_offset+SZ_E+SZ_H, SZ_K);
		verify_header = (sys_img_header *)(gpt_data.header);
		dprintf(ALWAYS, "gpt_enable:%d\n", gpt_enable);
		if (verify_header->mMagicNum != IMG_BAK_HEADER) {
			if (gpt_enable) {
				fastboot_fail("gpt efuse enable, but no sign in gpt bin!");
				errorf("gpt efuse enable, but no sign in gpt bin\n");
				return;
			}
		} else {
			dprintf(ALWAYS, "gpt bin include sign key\n");
			gpt_verify_header = 1;
		}

		while (gpt_e[i].starting_lba != 0) {
			for (int j = 0; j < strlen("userdata"); j++) {
				dpart_name[j] = gpt_e[i].partition_name[j];
			}
			// dprintf(INFO,"dpart_name:%s\n", dpart_name);
			if (!strcmp(dpart_name, "userdata")) {
				gpt_e[i].ending_lba = 0;
				break;
			}
			i++;
			memset(dpart_name, 0, 36);
		}
		dprintf(INFO,"verify data addr:%llx, prim_gpt_offset:%llx\n", data, prim_gpt_offset);

		/* For GPT verify header and key */
		gpt_sign_data = malloc(SZ_H + SZ_K);
		if (gpt_sign_data == NULL) {
			errorf("No enough memory for GPT sign data!\n");
			return;
		}

		memset(gpt_sign_data, 0, SZ_H + SZ_K);
		memcpy(gpt_sign_data, gpt_data.header, SZ_H);
		memcpy(gpt_sign_data + SZ_H, gpt_data.key, SZ_K);
	}
#endif

#ifdef CONFIG_DL_SKIP_PARTITION
	if (PARTITION_DOWNLOAD == dl_get_partition_skip(part_name)) {
#endif
#if defined (SPRD_SECBOOT)
		/* secboot verify */
		ulong strip_header = 0;
#ifdef CONFIG_VERIFY_GPT
		if (!strcmp(part_name, "partition") || !strcmp(part_name, "gpt")) {
			if (gpt_verify_header) {
				status = dl_secboot_verify(&strip_header,
							part_name,
							0, 0, &gpt_data);
			} else {
				dprintf(INFO, "gpt bin not include sign key, do not need verify gpt!\n");
			}
		} else {
#endif
		status = dl_secboot_verify(&strip_header,
					part_name,
					0, 0, data);
#ifdef CONFIG_VERIFY_GPT
		}
#endif
		if (OPERATE_SUCCESS != status) {
			fastboot_fail("Secboot verify fail!");
			return;
		}

#endif
		bakup_header = (sys_img_header *)(ImageInfo.base_address);
		debugf("bakup_header->mMagicNum=0x%x\n", bakup_header->mMagicNum);
		if (bakup_header->mMagicNum != IMG_BAK_HEADER) {
			sec_offset = 0;
		}

		/* Write partition table */
		if (!strcmp(part_name, "partition") || !strcmp(part_name, "gpt")) {
			retval = _fb_repartition(ImageInfo.base_address, ImageInfo.data_size);
			if (retval) {
				errorf("Write pgpt fail(%d)\n", retval);
				fastboot_fail("Write gpt image fail!");
				return;
			} else {
#ifdef CONFIG_VERIFY_GPT
				if (gpt_verify_header) {
					ret = process_gpt_signdata(gpt_sign_data, SZ_H+SZ_K, 1, 0);
					if (ret != 0) {
						fastboot_fail("Write Primary gpt key fail!");
						return;
					}
					ret = process_gpt_signdata(gpt_sign_data, SZ_H+SZ_K, 1, 1);

					if (ret != 0) {
						fastboot_fail("Write backup gpt key fail!");
						return;
					}
					free(gpt_sign_data);
					memset(data, 0, prim_gpt_offset+SZ_E+SZ_H+SZ_K);
				}
#endif
				fastboot_okay("");
				return;
			}
		}

		img_format = fb_get_partition_image_type(part_name, ImageInfo.base_address,
						sec_offset);

		write_size = ImageInfo.data_size;
		write_start = ImageInfo.base_address;
		
#ifdef CONFIG_HMD_FASTBOOT	
	if (!strcmp(part_name, "userdata") || !strcmp(part_name, "metadata")) {
		if (FLAG_BLOCKED == check_miscdata_blocked_flag()) {
			fastboot_fail("operate fail, miscdata_blocked_flag set true");
			return;
		}
	}
#endif
//fixme
#if 0
		if (!strcmp(part_name, "userdata") && g_download_part_count == 0) {
			debugf("userdata image format is %d\n", img_format);
			get_img_partition_size(part_name, &total_size);
			if (0 != common_raw_erase(part_name, total_size/100, (uint64_t)0LL)) {
				errorf("erase partition %s fail!\n", part_name);
				return -1;
			}
			f2fs_init_resize_configuration();
			g_resize_config.resize_flag = RESIZE_FROM_EMMC;
			g_download_part_count += 1;
		}
#endif //fixme

#ifdef ZCFG_HMD_ENTERPRISE_API
        //[HMDEnterpriseService] add block download mode api begin 2024-09-04 
        /*char block_download[1];
        char isBlocked[1] = "1";
        if (0 != common_raw_read(ENTERPRISEAPIINFO_PARTITION_NAME, (uint64_t)1, BLOCK_DOWNLOAD_MODE_OFFSET, block_download)) {
            errorf("read error\n");
            fastboot_fail("read error\n");
        } else {
            debugf(" block_download=%s\n",block_download);
            if (!memcmp(isBlocked, block_download, sizeof(block_download))) {
                fastboot_fail("download mode is blocked!");
                return;
            }
        }*/
        //[HMDEnterpriseService] add block download mode api end 2024-09-04 
		//[HMDEnterpriseService] for error code showing begin 2025-03-04 
		if(isBlockDownloadMode()){
            fastboot_fail("download mode is blocked!");
            return;
		}
		//[HMDEnterpriseService] for error code showing end 2025-03-04
#endif

		/* image write */
		part_purpose = dl_get_partition_purpose(part_name);
#ifdef CONFIG_FASTBOOT_FLASH
		debugf("fb_cmd_flash notnandboot: part_purpose %d\n", part_purpose);
		if (PARTITION_PURPOSE_NV == part_purpose) {

#ifdef FIXNV_SIGN
		fixnv_sign_image_start_addr = write_start;
                fixnv_real_image_start_addr = fixnv_sign_image_start_addr + FIXNV_SIGN_HEADER_SIZE;
		fixnv_real_size = ((FIXNV_SIGN_HEADER_T *)fixnv_sign_image_start_addr)->ImgRealSize;
		debugf("fixnv_parser fixnv_real_size is 0x%x\n", fixnv_real_size);
#else
		fixnv_real_image_start_addr = write_start;
		fixnv_real_size = write_size;
#endif

			debugf("fb_cmd_flash eMMC: part_purpose PARTITION_PURPOSE_NV, fixnv_real_size %x,s_backupnv_flag %d\n", fixnv_real_size, s_backupnv_flag);
			if (0 != fb_write_nv_img(part_name, fixnv_real_image_start_addr, fixnv_real_size, s_backupnv_flag)) {
				errorf("Write nv img fail\n");
				retval = -1;
			}
		} else {
#endif
			retval = dl_image_write(part_name, write_size, 0, ImageInfo.max_size,
					write_start, img_format, part_purpose);
#ifdef CONFIG_FASTBOOT_FLASH
		}
#endif
		if (retval < 0) {
			errorf("Write img fail, code(%d)\n", retval);
			fastboot_fail("Write img fail!");
			return;
		} else if (retval > 0) {
			errorf("Error: after simg , unsave_recv_size=%d, saved value=%d\n",
				write_size - retval, retval);
			fastboot_fail("Write simg fail!");
			return;
		} else { /* write success */
		/* backup */
			status = dl_backup(part_name, ImageInfo.data_size, ImageInfo.base_address);
			if (OPERATE_WRITE_ERROR == status) {
				fastboot_fail("Backup fail!");
				return;
			}
		}
		if (is_f2fs_filesystem(part_name)) {
			total_size = 0;
			debugf("write %s size 0x%x ok\n", part_name, write_size);
			if (get_img_partition_size(part_name, &total_size) != 0) {
				errorf("get %s partition size fail!\n", part_name);
				fastboot_fail("get partition size fail!");
				return;
			}

#ifdef CONFIG_WR_SPARSE
			if (ImageInfo.max_size_raw < (total_size / 128)) {
#else
			if (ImageInfo.max_size < (total_size / 128)) {
#endif
				errorf("resize skip! small buffer, config dts!\n");
			}
		}
#ifdef CONFIG_DL_SKIP_PARTITION
	} else {
		dprintf(ALWAYS, "partition %s skip download!\n", part_name);
	}
#endif

	fastboot_okay("");
}

void fb_cmd_erase(const char *arg, void *data, uint64_t size)
{
	char partition_name[PARTNAME_SZ];
	int i;
	size = 0;
#if !defined(CONFIG_HMD_FASTBOOT)
#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
#if defined (SPRD_SECBOOT)
	if (!fb_check_permission_part_erase(arg)
			&& (get_lock_status() == VBOOT_STATUS_LOCK)) {
		fastboot_fail("flash Not allow! Please unlock or get permission first!");
		return;
	}

#ifdef ZCFG_HMD_ENTERPRISE_API
    //[HMDEnterpriseService] add block factory reset api begin 2024-09-04 
    /*if (!strcmp(arg, "userdata") || !strcmp(arg, "metadata") || !strcmp(arg, "cache")) {
        char block_factory_reset[1];
        char isBlocked[1] = "1";
        if (0 != common_raw_read(ENTERPRISEAPIINFO_PARTITION_NAME, (uint64_t)1, BLOCK_FACTORY_RESET_OFFSET, block_factory_reset)) {
            errorf("read error\n");
            fastboot_fail("read error\n");
        } else {
            debugf("block_factory_reset=%s\n",block_factory_reset);
            if (!memcmp(isBlocked, block_factory_reset, sizeof(block_factory_reset))) {
                fastboot_fail("factory reset is blocked!");
                return;
            }
        }
    }*/
    //[HMDEnterpriseService] add block factory reset api end 2024-09-04 
	//[HMDEnterpriseService] for error code showing begin 2025-03-04
	if (!strcmp(arg, "userdata") || !strcmp(arg, "metadata") || !strcmp(arg, "cache")) {
        if(isBlockFactoryReset() && isBlockDownloadMode()){
            fastboot_fail("factory reset is blocked!");
            return;
        }
    }
    //[HMDEnterpriseService] for error code showing end 2025-03-04
#endif

#else
	if (!fb_check_permission_part_erase(arg)) {
		fastboot_fail("Not allow!");
		return;
	}
#endif
#else
#if defined (SPRD_SECBOOT) && !(DEBUG)
	if(get_lock_status() == VBOOT_STATUS_LOCK) {
		fastboot_fail("Flashing Lock Flag is locked. Please unlock it first!");
		return;
	}
#endif
#endif
#endif

	for (i = 0; i < PARTNAME_SZ; i++) {
		partition_name[i] = arg[i];
		if (0 == arg[i])
			break;
	}

	if (!strcmp(partition_name, "persist")) {
		fastboot_fail("not support");
		return;
	} else if (!strcmp(partition_name, "config")) {
		strcpy(partition_name, "persist");
	}
#ifdef CONFIG_HMD_FASTBOOT	
	if (!strcmp(partition_name, "userdata") || !strcmp(partition_name, "metadata")) {
		if (FLAG_BLOCKED == check_miscdata_blocked_flag()) {
			fastboot_fail("operate fail, miscdata_blocked_flag set true");
			return;
		}
	}

#ifdef ZCFG_HMD_ENTERPRISE_API
    //[HMDEnterpriseService] add block factory reset api begin 2024-09-04 
    /*if (!strcmp(arg, "userdata") || !strcmp(arg, "metadata") || !strcmp(arg, "cache")) {
        char block_factory_reset[1];
        char isBlocked[1] = "1";
        if (0 != common_raw_read(ENTERPRISEAPIINFO_PARTITION_NAME, (uint64_t)1, BLOCK_FACTORY_RESET_OFFSET, block_factory_reset)) {
            errorf("read error\n");
            fastboot_fail("read error\n");
        } else {
            debugf("block_factory_reset=%s\n",block_factory_reset);
            if (!memcmp(isBlocked, block_factory_reset, sizeof(block_factory_reset))) {
                fastboot_fail("factory reset is blocked!");
                return;
            }
        }
    }*/
    //[HMDEnterpriseService] add block factory reset api end 2024-09-04 
	//[HMDEnterpriseService] for error code showing begin 2025-03-04
	if (!strcmp(arg, "userdata") || !strcmp(arg, "metadata") || !strcmp(arg, "cache")) {
        if(isBlockFactoryReset() && isBlockDownloadMode()){
            fastboot_fail("factory reset is blocked!");
            return;
        }
    }
    //[HMDEnterpriseService] for error code showing end 2025-03-04
#endif

#endif	
	if (OPERATE_SUCCESS != dl_erase(partition_name, size)) {
		fastboot_fail("operate fail");
		return;
	}
	
#ifdef CONFIG_HMD_FASTBOOT
	//add for DeviceKit FRP Erase(erase persist partition) need to Factory reset
	if (!strcmp(partition_name, "persist")) {
		if (0 != common_raw_erase("userdata", 0, 0)) {
			debugf("erase userdata failed\n");
		}
		debugf("erase userdata ok\n");
		if (0 != common_raw_erase("metadata", 0, 0)) {
			debugf("erase metadata failed\n");
		}
		debugf("erase metadata ok\n");
	}
#endif
    //[HMDEnterpriseService] for error code showing begin 2025-03-04
#ifdef ZCFG_HMD_ENTERPRISE_API
    if(!strcmp(arg, "userdata") && is_hmd_error_code_set(NULL)){
        char* error_code[ERROR_CODE_LEN];
        memset(error_code,0,ERROR_CODE_LEN);
        if (common_raw_write(ENTERPRISEAPIINFO_PARTITION_NAME, (uint64_t)ERROR_CODE_LEN, (uint64_t)0, ENTERPRISE_ERROR_CODE_OFFSET, error_code)) {
            errorf("write %s data %d fail on offset %lld.\n",ENTERPRISEAPIINFO_PARTITION_NAME, ERROR_CODE_LEN, ENTERPRISE_ERROR_CODE_OFFSET);
        }
    }
#endif
    //[HMDEnterpriseService] for error code showing end 2025-03-04
	debugf("Cmd Erase OK\n");
	fastboot_okay("");
	return;
}
#endif

/*for nand this can not work,TODO*/
#ifdef CONFIG_NAND_BOOT
void fb_cmd_flash(const char *arg, void *data, u32 sz)
{

	int ret = -1;
	int i;
	uint64_t *code_addr;

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
        char subcmd[64];
        const char *delim = " ";

        memset(subcmd, 0, sizeof(subcmd));
        strncpy(subcmd, strtok(arg, delim), sizeof(subcmd)-1);

        if (!strcmp(subcmd, "unlock")){
            strncpy(product_sn_token, get_product_sn(), PRODUCT_SN_TOKEN_MAX_SIZE-1);
            debugf("Product SerialNo: %s.", product_sn_token);
            memset(product_sn_signature, 0, sizeof(product_sn_signature));
            memcpy(product_sn_signature, data, PRODUCT_SN_SIGNATURE_SIZE);

            ret = fb_verify_unlockkey(product_sn_signature, product_sn_token);
            if(ret == 1) {
                fastboot_okay("flash unlock success\n");
            } else {
                fastboot_fail("flash unlock failed\n");
            }
            return;
        }
#endif

	debugf("arg:%x date: 0x%x, sz 0x%x, ImageInfo.data_size 0x%llx\n", arg, data, sz, ImageInfo.data_size);

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
#if defined (SPRD_SECBOOT)
	if (!fb_check_permission_part_flash(arg)
			&& (get_lock_status() == VBOOT_STATUS_LOCK)) {
		fastboot_fail("flash Not allow! Please unlock or get permission first!");
		return;
	}
#else
	if (!fb_check_permission_part_flash(arg)) {
		fastboot_fail("Not allow!");
		return;
	}
#endif
#endif

	//ImageInfo.data_size =0x10000;
	if (!strcmp(arg, "boot") || !strcmp(arg, "recovery")) {
		if (memcmp((void *)data, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
			fastboot_fail("image is not a boot image");
			return;
		}
	}

	/**
	 *	FIX ME!
	 *	assume first image buffer is big enough for nv
	 */
	for (i = 0; s_nv_vol_info[i].vol != NULL; i++) {
		if (!strcmp(arg, s_nv_vol_info[i].vol)) {
			uint32_t write_size = 0;
			write_size = ImageInfo.data_size;
#ifdef CONFIG_FASTBOOT_FLASH
			debugf("fb_cmd_flash nandboot: s_backupnv_flag = %d\n", s_backupnv_flag);
			if (s_backupnv_flag) {
				char 		*partition_name = arg;
				uint8_t 	header_buf[NV_HEAD_LEN];
				nv_header_t *nv_header_p = NULL;
				int 		retval = 0;
				void 		*buf = data;

				retval = fb_nvmerge(partition_name, (uint8_t *)buf, write_size);
				debugf("fb_nvmerge return %d\n", retval);

				memset(header_buf, 0x00, NV_HEAD_LEN);
				nv_header_p = header_buf;
				nv_header_p->magic = NV_HEAD_MAGIC;
				nv_header_p->len = write_size;
				nv_header_p->checksum = (uint32_t) calc_checksum((unsigned char *)buf, write_size);
				nv_header_p->version = NV_VERSION;
#ifdef NV_CHECK_WITH_SHA256
				fdl_get_sha256(buf, nv_header_p->len, nv_header_p->auth);
#endif
				/*write org nv */
				ret = do_raw_data_write(arg, write_size + NV_HEAD_LEN, NV_HEAD_LEN, 0, header_buf);
				if (ret)
					goto end;
				ret = do_raw_data_write(arg, 0, write_size, NV_HEAD_LEN, buf);
				if (ret)
					goto end;
				/*write bak nv */
				ret = do_raw_data_write(s_nv_vol_info[i].bakvol, write_size + NV_HEAD_LEN, NV_HEAD_LEN, 0, header_buf);
				if (ret)
					goto end;
				ret = do_raw_data_write(s_nv_vol_info[i].bakvol, 0, write_size, NV_HEAD_LEN, buf);
				goto end;
			}
			if (s_backupnv_flag == 0) {
#endif
				nv_header_t *header = NULL;
				uint8_t tmp[NV_HEAD_LEN];
				memset(tmp, 0x00, NV_HEAD_LEN);
				header = tmp;
				header->magic = NV_HEAD_MAGIC;
				header->len = write_size;
				header->checksum = (uint32_t) calc_checksum((unsigned char *)data, write_size);
				header->version = NV_VERSION;
#ifdef NV_CHECK_WITH_SHA256
				fdl_get_sha256(data, header->len, header->auth);
#endif
				/*write org nv */
				ret = do_raw_data_write(arg, write_size + NV_HEAD_LEN, NV_HEAD_LEN, 0, tmp);
				if (ret)
					goto end;
				ret = do_raw_data_write(arg, 0, write_size, NV_HEAD_LEN, data);
				if (ret)
					goto end;
				/*write bak nv */
				ret = do_raw_data_write(s_nv_vol_info[i].bakvol, write_size + NV_HEAD_LEN, NV_HEAD_LEN, 0, tmp);
				if (ret)
					goto end;
				ret = do_raw_data_write(s_nv_vol_info[i].bakvol, 0, write_size, NV_HEAD_LEN, data);
				goto end;
#ifdef CONFIG_FASTBOOT_FLASH
			}
#endif
		}

	}

		ret = do_raw_data_write(arg, ImageInfo.data_size, ImageInfo.data_size, 0, ImageInfo.base_address);


end:
	if (!ret)
		fastboot_okay("");
	else
		fastboot_fail("flash error");
	return;

}

void fb_cmd_erase(const char *arg, void *data, uint64_t sz)
{
	struct mtd_info *nand;
	struct mtd_device *dev;
	struct part_info *part;
	nand_erase_options_t opts;
	u8 pnum;
	int ret;
	char buf[1024];

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
	if (!fb_check_permission_part_erase(arg)) {
		fastboot_fail("Not allow!");
		return;
	}
#endif
	debugf("\n");

	if (!strcmp(arg, "persist")) {
		fastboot_fail("not support");
		return;
	} else if (!strcmp(arg, "config")) {
		arg = "persist";
	}

	ret = find_dev_and_part(arg, &dev, &pnum, &part);
	if (!ret) {
		nand = &nand_info[dev->id->num];
		memset(&opts, 0, sizeof(opts));
		opts.offset = (loff_t) (part->offset);
		opts.length = (loff_t) (part->size);
		opts.jffs2 = 0;
		opts.quiet = 1;
		ret = nand_erase_opts(nand, &opts);
		if (ret)
			goto end;
	}

	/*just erase 1k now */
	memset(buf, 0x0, 1024);
	ret = do_raw_data_write(arg, 1024, 1024, 0, buf);

end:
	if (ret)
		fastboot_fail("nand erase error");
	else
		fastboot_okay("");
	return;
}
#endif

void boot_linux(unsigned kaddr, unsigned taddr)
{
//fixme
#if 0
	void (*theKernel) (void *dtb_addr, int zero, int arch, int reserved) = (void*)kaddr;

	invalidate_dcache_all();

	theKernel(DT_ADR, 0, 0, 0);
#endif //fixme
}

void fb_cmd_setdump(const char *arg, void *data, uint64_t sz)
{
	int ret = 0;
	char subcmd[64];
	const char *delim = " ";

	memset(subcmd, 0, sizeof(subcmd));
	strncpy(subcmd, strtok(arg, delim), sizeof(subcmd)-1);

	ret = sysdump_setdump_command(subcmd);
	if (ret < 0)
		fastboot_fail("setdump fail");
	else
		fastboot_okay("");

	write_log();
	return;
}

void fb_cmd_getdump(const char *arg, void *data, uint64_t sz)
{
	int ret = 0;
	char subcmd[64];
	char status_info[60];
	const char *delim = " ";
	int fulldump_enable_status;
	int minidump_enable_status;

	memset(subcmd, 0, sizeof(subcmd));
	strncpy(subcmd, strtok(arg, delim), sizeof(subcmd)-1);

	ret = sysdump_getdump_command(subcmd, &fulldump_enable_status, &minidump_enable_status);
	if (ret < 0) {
		fastboot_fail("getdump command fail");
	} else {
#ifdef CONFIG_SPLASH_SCREEN
		lcd_printf("\nfulldump_enable_status:%d\nminidump_enable_status:%d\n", fulldump_enable_status, minidump_enable_status);
#endif
		sprintf(status_info, "INFO\nfulldump_enable_status:%d\nminidump_enable_status:%d\n",fulldump_enable_status, minidump_enable_status);
		fb_usb_write(status_info, strlen(status_info));
		fastboot_okay("");
	}
	write_log();
}


#ifdef SPRD_SECBOOT
#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
static int repair_set_key(uint64_t offset, char *buf, int length)
{
	if (common_raw_write("miscdata", (uint64_t)length, (uint64_t)0, offset,
			buf)) {
		errorf("write miscdata data %d fail on offset %lld.\n", length, offset);
		return -1;
	}

	return 0;
}

static int repair_get_key(uint64_t offset, char *buf, int length)
{
	if (common_raw_read("miscdata", (uint64_t)length, offset, buf)) {
		errorf("read miscdata data %d fail on offset %lld.\n", length, offset);
		return -1;
	}

	return 0;
}

static int repair_clr_key(uint64_t offset)
{
	const char buf[2] = {0};

	if (common_raw_write("miscdata", (uint64_t)2, (uint64_t)0, offset, buf)) {
		errorf("clr miscdata data %d fail on offset %lld.\n", 2, offset);
		return -1;
	}

	return 0;
}

int fb_oem_repair_get_booargs(char *buf, int len)
{
	char wallpp[MISCDATA_WALLPP_SZ + 1] = {0};
	char skuid[MISCDATA_SKUID_SZ + 1] = {0};
	int c = 0;

	if (!repair_get_key(MISCDATA_SKUID_OFFSET, skuid, MISCDATA_SKUID_SZ)
			&& skuid[0] == 0x7E && skuid[1] == 0x7E) {
		c += snprintf(buf, len - c - 1, " androidboot.skuid=%s ", skuid + 2);
	}

	if (!repair_get_key(MISCDATA_WALLPP_OFFSET, wallpp, MISCDATA_WALLPP_SZ)
			&& wallpp[0] == 0x7E && wallpp[1] == 0x7E) {
		c += snprintf(buf + c, len - c - 1, "androidboot.wallpapered=%s ",
			wallpp + 2);
	}

	return c;
}

static void getrepair_psn(char *key)
{
	char psn[PRODUCT_SN_TOKEN_MAX_SIZE + 1] = {0};

	strcpy(psn, get_product_sn());
	if (strlen(psn))
		fastboot_info("%s=%s", key, psn);
	else
		fastboot_okay("");
}

static void getrepair_skuid(char *key)
{
	char skuid[MISCDATA_SKUID_SZ + 1] = {0};

	if (!repair_get_key(MISCDATA_SKUID_OFFSET, skuid, MISCDATA_SKUID_SZ)
			&& skuid[0] == 0x7E && skuid[1] == 0x7E)
		fastboot_info("%s=%s", key, skuid + 2);
	else
		fastboot_okay("");
}

static void getrepair_wallpapered(char *key)
{
	char wallpp[MISCDATA_WALLPP_SZ + 1] = {0};

	if (!repair_get_key(MISCDATA_WALLPP_OFFSET, wallpp, MISCDATA_WALLPP_SZ)
			&& wallpp[0] == 0x7E && wallpp[1] == 0x7E)
		fastboot_info("%s=%s", key, wallpp + 2);
	else
		fastboot_okay("");
}

static const struct getrepair {
	const char *key;
	void (*get)(char *key);
} fb_getrepair[] = {
	{
		.key = "psn",
		.get = getrepair_psn,
	}, {
		.key = "SKUID",
		.get = getrepair_skuid,
	}, {
		.key = "wallpapered",
		.get = getrepair_wallpapered,
	},
};

static void setrepair_psn(char *key, char *value)
{
	if (!set_product_sn(value, !value ? 0 : strlen(value)))
		fastboot_info("%s=%s", key, get_product_sn());
	else
		fastboot_fail("");
}

static void setrepair_skuid(char *key, char *value)
{
	int sz;

	if (!value) {
		repair_clr_key(MISCDATA_SKUID_OFFSET);
		fastboot_info("%s=", key);
		return;
	} else
		sz = strlen(value) > MISCDATA_SKUID_SZ ? MISCDATA_SKUID_SZ
				: strlen(value);

	if (!repair_set_key(MISCDATA_SKUID_OFFSET, "\x7E\x7E", 2)
			&& !repair_set_key(MISCDATA_SKUID_OFFSET + 2, value, sz)) {
		fastboot_info("%s=%s", key, value);
	}
}

static void setrepair_wallpapered(char *key, char *value)
{
	int sz;

	if (!value) {
		repair_clr_key(MISCDATA_WALLPP_OFFSET);
		fastboot_info("%s=", key);
		return;
	} else
		sz = strlen(value) > MISCDATA_WALLPP_SZ ? MISCDATA_WALLPP_SZ
				: strlen(value);

	if (!repair_set_key(MISCDATA_WALLPP_OFFSET, "\x7E\x7E", 2)
			&& !repair_set_key(MISCDATA_WALLPP_OFFSET + 2, value, sz)) {
		fastboot_info("%s=%s", key, value);
	}
}

static const struct setrepair {
	const char *key;
	void (*set)(char *key, char *value);
} fb_setrepair[] = {
	{
		.key = "psn",
		.set = setrepair_psn,
	}, {
		.key = "SKUID",
		.set = setrepair_skuid,
	}, {
		.key = "wallpapered",
		.set = setrepair_wallpapered,
	},
};

/* fastboot oem repair */
void fb_cmd_oem_repair(const char *arg, void *data, uint64_t sz)
{
	char *key, *opt, *value, *end = &arg[strlen(arg) - 1];
	const char *delim = " ";
	int i;

	if (!fb_check_permission(TYPE_PERMISSION_REPAIR)) {
		fastboot_fail("Not allow!");
		return;
	}

	while (*arg == *delim)
		arg++;

	/* trim trailing space */
	while ((*end == *delim) && (end != arg))
		*end-- = '\0';

	key = strtok(arg + strlen("repair"), delim);
	if (!key) {
		return;
	}

	opt = strtok(NULL, delim);
	if (!opt) {
		return;
	}

	if (!strcmp(opt, "get")) {
		for (i = 0; i < ARRAY_SIZE(fb_getrepair); i++)
			if (!strcmp(key, fb_getrepair[i].key))
				fb_getrepair[i].get(key);
	} else if (!strcmp(opt, "set")) {
		value = strtok(NULL, delim);
		for (i = 0; i < ARRAY_SIZE(fb_setrepair); i++)
			if (!strcmp(key, fb_setrepair[i].key))
				fb_setrepair[i].set(key, value);
	}
	return;
}

void fb_cmd_upload(const char *arg, void *data, uint64_t sz)
{
	int i;

	dprintf(INFO,"%s\n", __func__);

	fastboot_ack("DATA", "0x158");

        for(i = 0; i < 344; i++)
                encrypt_data[i] = (unsigned char)encrypt_data[i];

	fb_usb_write(encrypt_data, 344);

	fastboot_state = STATE_COMMAND;
	fastboot_okay("");
	fastboot_state = STATE_COMPLETE;
}

int get_permission(void)
{
	return open_permission;
}
#endif
#endif

#if defined FASTBOOT_GET_IMEI_SUPPORT && defined CONFIG_FASTBOOT_FLASH
/* for test fastboot get imei */
extern int get_imei_from_nv(char* imei1, char* imei2);
void fb_cmd_oem_get_imei(void)
{
	int ret = 0;
	char imei1[16] = {0};
	char imei2[16] = {0};
	ret = get_imei_from_nv(imei1, imei2);
	if (ret != 0) {
		debugf("get imei fail!\n");
		fastboot_fail("get imei error");
		return;
	}
	debugf("get imei succ: [1:%s,2:%s]!\n", imei1, imei2);
	fastboot_okay("");
}
#endif
#ifndef SPRD_SECBOOT
void fb_cmd_oem(const char *arg, void *data, uint64_t sz)
{
	debugf("arg#%s#, data: %p, sz: 0x%llx\n", arg, data, sz);
#ifdef CONFIG_FASTBOOT_FLASH
	if (!strncmp(arg + 1, "backupnv", strlen("backupnv"))) {
		debugf("execute backupnv!\n");
		s_backupnv_flag = 1;
		fastboot_okay("");
		return;
	}
#endif
#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
	if (!strncmp(arg + 1, "repair", strlen("repair"))) {
		fb_cmd_oem_repair(arg + 1, data, sz);
		return;
	}
#endif
#ifdef FASTBOOT_GET_IMEI_SUPPORT && defined CONFIG_FASTBOOT_FLASH
	if (!strncmp(arg + 1, "imei", strlen("imei"))) {
		debugf("execute get imei!\n");
		fb_cmd_oem_get_imei();
		return;
	}
#endif
}

#else

static int fb_cmd_oem_upload_ddr(char *range)
{
	uint64_t addr, size = 0;
	uint64_t offset = 0, send;
	char ulen[64];
	int ret;
	char *ptr = NULL;

	addr = simple_strtoull(range, &ptr, 16);
	if (ptr && ('+' == *ptr) && ('+' == *(ptr + 1))) {
		ptr += 2;
		size = simple_strtoull(ptr, NULL, 16);
	}

	if (size) {
		dprintf(INFO,"get addr %llx size %llx\n", addr, size);

		debugf("upload %llx bytes\n", size);
		sprintf(ulen, "0x%llx", size);
		fastboot_ack("DATA", ulen);

		while (size) {
			send = size > 512 ? 512 : size;
			ret = fb_usb_write(addr + offset, send);
			if (ret < 0)
				break;
			size -= send;
			offset += send;
		}

		debugf("write remain: %llxbytes\n", size);
	}

	fastboot_state = STATE_COMMAND;
	if (!size)
		fastboot_okay("");
	fastboot_state = STATE_COMPLETE;
	return size;
}

static int fb_cmd_oem_upload_gpt(void)
{
	char *ptr = NULL;
	int ret;
	int dev_id;
	char *ifname;
	block_dev_desc_t *desc;
	uint64_t size = -1;
	uint64_t offset = 0, send;
	char ulen[64];

	ifname = block_dev_get_name();
	dev_id = get_devnum_hwpart(ifname, 0/*USER_PART*/);
	if (dev_id < 0) {
		errorf("get user part dev num fail!\n");
		goto out;
	}

	desc = get_dev_hwpart(ifname, dev_id, 0/*USER_PART*/);
	if (!desc) {
		errorf("get user part dev fail!\n");
		goto out;
	}

	/* read size */
#ifdef CONFIG_VERIFY_GPT
	uint64_t gpt_size;
	u32 gpt_enable = 0;

	gpt_enable = !check_gpt_efuse();
	/* gpt-sign.bin structure:|MBR||GPT header||GPT entry||Verify header||Verify key| */
	if (gpt_enable) {
		gpt_size = desc->blksz * 2 + GPT_ENTRY_NUMBERS * GPT_ENTRY_SIZE;
		size = gpt_size + SZ_H + SZ_K;
	} else {
		size = desc->blksz * 2 + GPT_ENTRY_NUMBERS * GPT_ENTRY_SIZE;
	}
#else
	/* gpt.bin structure:|MBR||GPT header||GPT entry| */
	size = desc->blksz * 2 + GPT_ENTRY_NUMBERS * GPT_ENTRY_SIZE;
#endif
	ptr = (char *)calloc(1, size);
	if (!ptr) {
		errorf("malloc fail\n");
		goto out;
	}

	if (desc->block_read(desc->dev_num, (lbaint_t)0,
			BLOCK_CNT(size, desc), ptr) != BLOCK_CNT(size, desc)) {
		errorf("Can't read GPT\n");
		goto out;
	}
#ifdef CONFIG_VERIFY_GPT
	if (gpt_enable) {
		ret = process_gpt_signdata(ptr + gpt_size, SZ_H+SZ_K, 0, 0);
		if (ret != 0) {
			errorf("read gpt signdata fail!\n");
			return -1;
		}
	} else {
		dprintf(INFO, "GPT efuse bit not enable, do not need gpt sign data!\n");
	}
#endif
	debugf("upload %llx bytes\n", size);
	sprintf(ulen, "0x%llx", size);
	fastboot_ack("DATA", ulen);

	offset = 0;
	while (size) {
		send = size > 512 ? 512 : size;
		ret = fb_usb_write(ptr + offset, send);
		if (ret < 0)
			break;
		size -= send;
		offset += send;
	}

	debugf("write remain: %llxbytes\n", size);

	fastboot_state = STATE_COMMAND;
	if (!size)
		fastboot_okay("");
	fastboot_state = STATE_COMPLETE;

out:
	if (ptr)
		free(ptr);
	return size;
}

static int fb_cmd_oem_upload_flash(char *part)
{
	uint64_t offset_s, offset_r, send, read;
	uint64_t addr = 0;
	uint64_t size = 0, part_size = 0;
	char ulen[64];
	int ret;
	char *ptr = NULL;
	char *range = part;
	char delim[] = ",";

	/* read gpt */
	if (!strcmp(part, "partition") || !strcmp(part, "gpt")) {
		return fb_cmd_oem_upload_gpt();
	}

	ptr = strsep(&range, delim);
	debugf("ptr: %s\n", ptr);
	if ((range!=NULL) && strlen(range)) {
		debugf("range: %s\n", range);

		addr = simple_strtoull(range, &ptr, 16);
		if (ptr && ('+' == *ptr) && ('+' == *(ptr + 1))) {
			ptr += 2;
			size = simple_strtoull(ptr, NULL, 16);
			size += 1;
		}

		debugf("addr: %llx, size %llx\n", addr, size);
	}
	ptr = NULL;
#ifdef CONFIG_DL_SKIP_PARTITION
	if (PARTITION_DOWNLOAD == dl_get_partition_skip(part)) {
#endif
		if ((get_img_partition_size(part, &part_size) < 0)
				|| part_size <= 0) {
			return -1;
		}

		if (addr >= part_size) {
			errorf("beyond the upper limit of the boundar,"
				" offset %llx part size %llx", addr, part_size);
			return -2;
		} else if (!addr && !size)
			size = part_size;
		else if ((addr + size) > part_size)
			size = part_size - addr;

		printf("get partition part_size %llx\n", part_size);

		ptr = (char *)malloc(OEM_UPLOAD_FLASH_BUF);
		if (!ptr) {
			errorf("malloc fail\n");
			return -3;
		}

		sprintf(ulen, "0x%llx", size);
		fastboot_ack("DATA", ulen);

		if (addr)
			offset_r = addr;
		else
			offset_r = 0;

		debugf("upload %llx bytes, from offset %llx\n", size, offset_r);
		while (size) {
			read = size > OEM_UPLOAD_FLASH_BUF ? OEM_UPLOAD_FLASH_BUF : size;
			ret = common_raw_read(part, read, offset_r, ptr);
			if (ret) {
				errorf("read fail offset %llx\n", offset_r);
				goto out;
			}
			size -= read;
			offset_r += read;

			offset_s = 0;
			while (read) {
				send = read > 512 ? 512 : read;

				ret = fb_usb_write(ptr + offset_s, send);
				if (ret < 0)
					goto out;
				read -= send;
				offset_s += send;
			}
		}
#ifdef CONFIG_DL_SKIP_PARTITION
	} else {
		dprintf(ALWAYS, "Do not support oem pull partition %s!\n", part);
	}
#endif

out:
	debugf("write remain: %llxbytes\n", size);

	fastboot_state = STATE_COMMAND;
	if (!size)
		fastboot_okay("");
	fastboot_state = STATE_COMPLETE;

	if (ptr)
		free(ptr);
	return size;
}

int fastboot_transfer_data(char *buffer, uint32_t size)
{
	uint64_t offset = 0, send = 0;
	char ulen[64];
	int ret = -1;

	if ((buffer == NULL) || (size == 0)) {
		errorf("buffer addr or size do not support 0\n");
		return ret;
	}

	dprintf(ALWAYS, "transfer %x bytes to PC\n", size);
	sprintf(ulen, "0x%x", size);
	fastboot_ack("DATA", ulen);

	while (size) {
		send = size > 512 ? 512 : size;
		ret = fb_usb_write(buffer + offset, send);
		if (ret < 0)
			break;
		size -= send;
		offset += send;
	}

	debugf("write remain: %x bytes\n", size);
	return size;
}

int fb_cmd_oem_upload_log_buffer(void)
{
	/* Upload LK log to PC */
	dprintf(INFO,"Upload log buffer to PC\n");
	if (fastboot_transfer_data((char *)LOG_RESERVED_ADDR, LOG_RESERVED_SIZE)) {
		errorf("transfer data to host error\n");
		return -1;
	}

	fastboot_state = STATE_COMMAND;
	fastboot_okay("");
	fastboot_state = STATE_COMPLETE;
	return 0;
}

int fb_cmd_oem_upload_lk_memory(void)
{
	/* Upload LK memory to PC, include mmu config */
	dprintf(INFO,"Upload lk memory to PC\n");
	if (fastboot_transfer_data(MEMBASE, MEMSIZE + 0x00100000)) {
		return -1;
	}

	fastboot_state = STATE_COMMAND;
	fastboot_okay("");
	fastboot_state = STATE_COMPLETE;
	return 0;
}

void fastboot_response_info(const char *info)
{
	if (info == 0)
		info = "";
	if (strlen("INFO") + strlen(info) >= 64) {
		debugf("too long string\r\n");
	}
	sprintf(response, "INFO%s", info);
	fb_usb_write(response, strlen(response));
}

#if defined(CONFIG_RPMB_SECURE_WRITE_PROTECT)
static void fb_write_protect_test(char *partition_name, char *len, char *action, char *write_pattern)
{
	unsigned long test_len;
	unsigned long size = SZ_32K;
	int ret = 0,i;
	int offset = 0;
	char *buf = NULL;

	test_len = simple_strtoul(len, NULL, 0);
	errorf("fb_write_protect_test action: %s:%lx:%s!!!\n", partition_name,
               test_len, action);
	buf = malloc(size);
	if (!buf) {
		errorf("Malloc for read buf fail\n");
		return;
	}

	if(!strcmp(action, "read")) {
		memset(buf, 0, size);

		ret = common_raw_read(partition_name, test_len, offset, buf);
		if (-1 == ret) {
			errorf("common_raw_read fail!\n");
			goto out;
		}
		fastboot_info("read data:0x%08x, %08x",*((uint32_t *)buf + 0),*((uint32_t *)buf + 1));
		debugf("read data:0x%08x, %08x",*((uint32_t *)buf + 0),*((uint32_t *)buf + 1));
		goto out_only;
	} else if(!strcmp(action, "write")) {
		errorf("fb_write_protect_test write_pattern:%c!!!\n", write_pattern[0]);
		memset(buf, write_pattern[0], size);
		ret = common_raw_write(partition_name, test_len, 0, offset, buf);
		if (-1 == ret) {
			errorf("common_raw_write fail!\n");
		} else {
			ret = 0;
			debugf("Write OK\n");
		}
		memset(buf, 0, size);
	} else if(!strcmp(action, "check_writeprotect")) {
		ret = storage_write_protect_check(partition_name, offset, test_len);
		if (0 == ret) {
			fastboot_info("This entry is protected!\n");
			debugf("This entry is protected!\n");
			goto out_only;
		}

		if (1 == ret) {
			ret = 0;
			fastboot_info("This entry is not protected!\n");
			debugf("This entry is not protected!\n");
			goto out_only;
		}

		if (-1 == ret)
			debugf("check swp entry fail!\n");
	} else if(!strcmp(action, "set_writeprotect")) {
		ret = storage_write_protect_set(partition_name, offset, test_len);
		if (-1 == ret)
			errorf("set swp entry fail!\n");
	} else if(!strcmp(action, "remove_writeprotect")) {
		ret = storage_write_protect_remove(partition_name, offset, test_len);
		if (-1 == ret)
			errorf("remove swp entry fail!\n");
	}
out:
	if(ret) {
		fastboot_info("FAIL");
		debugf("FAIL");
	} else {
		fastboot_info("OK");
		debugf("OK");
	}
out_only:
	free(buf);
	return;

}
#endif

void fb_cmd_ddr_memtest(char *addr, char *len, const char *times)
{
	unsigned long base_addr, test_len, test_times;

	base_addr = simple_strtoul(addr, NULL, 0);
	test_len = simple_strtoul(len, NULL, 0);
	test_times = simple_strtoul(times, NULL, 0);
	if (base_addr < CONFIG_SYS_SDRAM_BASE) {
		errorf("ddr_memtest memtest_addr invalid!!!\n");
		fastboot_fail("memtest_addr invalid");
		return;
	} else if (test_len > (unsigned long)(-1) - base_addr || test_len == 0) {
		errorf("ddr_memtest memtest_len out of range!!!\n");
		fastboot_fail("memtest_len out of range");
		return;
	} else if (test_times == 0) {
		debugf("ddr_memtest memtest_times is set to 1!!!\n");
		ddr_memtester(base_addr, test_len, 1, 0);
		fastboot_info("memtest_times is set to 1. ddr memtest end");
		return;
	}
	ddr_memtester(base_addr, test_len, test_times, 0);

	fastboot_okay("ddr memtest end");
}

void fb_cmd_ddr_set_config(char *config, char *paras)
{
	int i, ret = -1;
	unsigned long value, offset;
	struct ddrc_ddr_debug_info
	{
		u16 magic_num;
		u16 boot_freq;
		u16 ddr_dvfs_switch;
		u16 boot_freq_vol;
		u16 mask_freq[8];
		u16 ddr_retention_switch;
	} *p_ddr_debug;
	char ddr_data[32];
	u16 ddr_debug_value;
	u16 magic_num = 0x5aa5;

	if (!strncmp(config, "ddr_boot_freq", strlen(config))) {
		offset = 2;
		value = simple_strtoul(paras, NULL, 10);
		if (value > 0xffff) { //2byte
			errorf("ddr_set ddr_boot_freq out of range!!!\n");
			fastboot_fail("ddr_boot_freq out of range");
			return;
		}
	} else if(!strncmp(config, "ddr_dvfs", strlen(config))) {
		offset = 4;
		if (!strncmp(paras, "off", strlen(paras)))
			value = 0x5a;
		else if (!strncmp(paras, "on", strlen(paras)))
			value = 0x0;
		else {
			errorf("ddr_set ddr_dvfs illegal paras!!!\n");
			fastboot_fail("ddr_dvfs illegal paras");
			return;
		}
	} else if (!strncmp(config, "ddr_boot_freq_vol", strlen(config))) {
		offset = 6;
		value = simple_strtoul(paras, NULL, 10);
		if (value > 0xffff) { //2byte
			errorf("ddr_set ddr_boot_freq_vol out of range!!!\n");
			fastboot_fail("ddr_boot_freq_vol out of range");
			return;
		}
	} else if (!strncmp(config, "ddr_mask_freq", strlen(config))) {
		offset = 8;
		value = simple_strtoul(paras, NULL, 10);
		if (value > 0xffff) { //2byte
			errorf("ddr_set ddr_mask_freq out of range!!!\n");
			fastboot_fail("ddr_mask_freq out of range");
			return;
		}
	} else if(!strncmp(config, "ddr_retention", strlen(config))) {
		offset = 24;
		if (!strncmp(paras, "off", strlen(paras)))
			value = 0x5a;
		else if (!strncmp(paras, "on", strlen(paras)))
			value = 0x0;
		else {
			errorf("ddr_set ddr_retention illegal paras!!!\n");
			fastboot_fail("ddr_retention illegal paras");
			return;
		}
	} else if (!strncmp(config, "erase_all", strlen(config))) {
		ret = common_raw_erase("miscdata", (uint64_t)MISCDATA_DDR_DEBUG_LEN,
				       (uint64_t)MISCDATA_DDR_DEBUG_OFFSET);
		if (ret != 0) {
			errorf("erase ddr debug paras fail!!!\n");
			fastboot_fail("erase config fail");
		} else {
			fastboot_okay("please reboot for continue");
		}
		return;
	} else if (!strncmp(config, "get_configs", strlen(config))) {
		ret = common_raw_read("miscdata", (uint64_t)MISCDATA_DDR_DEBUG_LEN,
				      (uint64_t)MISCDATA_DDR_DEBUG_OFFSET, ddr_data);
		p_ddr_debug = (struct ddrc_ddr_debug_info *)ddr_data;
		if (ret < 0) {
			errorf("ddr_set read miscdata error!!!\n");
			fastboot_fail("read miscdata fail");
			return;
		}
		debugf("magic_num:0x%x\n", p_ddr_debug->magic_num);
		debugf("boot_freq:%d\n", p_ddr_debug->boot_freq);
		debugf("ddr_dvfs_switch:0x%x\n", p_ddr_debug->ddr_dvfs_switch);
		debugf("boot_freq_vol:%d\n", p_ddr_debug->boot_freq_vol);
		for (i =0; i < 8; i++)
			debugf("mask_freq[%d]:%d\n", i, p_ddr_debug->mask_freq[i]);
		debugf("ddr_retention_switch:0x%x\n", p_ddr_debug->ddr_retention_switch);
		fastboot_okay("get all ddr configs");
		return;
	} else {
		errorf("ddr_set unkonwn command!!!\n");
		fastboot_fail("unkonwn command");
		return;
	}

	ret = common_raw_read("miscdata", (uint64_t)MISCDATA_DDR_DEBUG_LEN,
			      (uint64_t)MISCDATA_DDR_DEBUG_OFFSET, ddr_data);
	p_ddr_debug = (struct ddrc_ddr_debug_info *)ddr_data;
	if (ret < 0) {
		errorf("ddr_set read miscdata error!!!\n");
		fastboot_fail("read miscdata fail");
		return;
	}

	//mask_freq
	if (offset == 8) {
		for (i = 0; i < 8; i++) {
			offset = 8 + 2 * i;
			ddr_debug_value = p_ddr_debug->mask_freq[i];
			if (ddr_debug_value == 0)
				break;
			else if (value == ddr_debug_value) {
				fastboot_okay("please reboot for continue");
				return;
			}
		}
		if (i == 8) {
			errorf("ddr_set mask_freq more than 8!!!\n");
			fastboot_fail("mask_freq num more than 8");
			return;
		}
	}

	ret = common_raw_write("miscdata", 2, (uint64_t)0,
			       (uint64_t)(MISCDATA_DDR_DEBUG_OFFSET + offset),
			       (char*)(&value));
	if (ret < 0) {
		errorf("ddr_set write miscdata error!!!\n");
		fastboot_fail("write miscdata fail");
		return;
	}

	ddr_debug_value = p_ddr_debug->magic_num;
	if (ddr_debug_value != magic_num) { //MAGIC_NUM
		value = magic_num;
		ret = common_raw_write("miscdata", 2, (uint64_t)0,
				       (uint64_t)(MISCDATA_DDR_DEBUG_OFFSET + 0),
				       (char*)&value);
		if (ret < 0) {
			errorf("ddr_set write miscdata error!!!\n");
			fastboot_fail("write miscdata fail");
			return;
		}
	}

	fastboot_okay("please reboot for continue");
}

void fb_cmd_startup_corex(char *core)
{
	unsigned short val = 0;
	unsigned long core_num;
	char *ptr;

	if (strcmp(core, "default")) {
		core_num = simple_strtoul(core, &ptr, 10);
		if (core_num >= 1 && core_num <= 8) {
			dprintf(INFO,"got core num %lu\n", core_num);
			val = (0x5A << 8) | ((unsigned char)core_num);
		} else {
			errorf("not support core num:%lu\n", core_num);
			fastboot_fail("not support core num");
			return;
		}
	}

	if (0 != common_raw_write("miscdata", CORE_STARTUP_FLAG_LEN, (uint64_t)0,
			(uint64_t)(CORE_STARTUP_FLAG_OFFSET), (char *)&val)) {
		errorf("write partition <miscdata> fail, val %x\n", val);
		fastboot_fail("Save fail");
		return;
	}

	fastboot_okay("please reboot for continue");
}

void fb_cmd_set_wdt_status(char *wdt_status)
{
	dprintf(INFO,"wdt_status %s\n", wdt_status);
	if ((!strcmp(wdt_status, "enabled")) || (!strcmp(wdt_status, "disabled"))) {
		//write miscdata
		if(common_raw_write("miscdata", (uint64_t)WDTEN_DATA_LEN, (uint64_t)0, (uint64_t)WDTEN_DATA_OFFSET, wdt_status)){
			fastboot_fail("write partition <miscdata> fail");
			return;
		}
	} else {
		errorf("unkonwn command !!! \n");
		fastboot_fail("unkonwn command !!! ");
		return;
	}

	fastboot_okay("please reboot for continue");
}

void fb_cmd_dvfs_set_config(char *nodes, char *status, char *flag)
{
	unsigned long dst = 0;
	unsigned int off, src, type = 0;
	char dvfs_data[32] = {0};
	unsigned int *data_ptr = (unsigned int *)&dvfs_data[0];

	if (!strncmp(nodes, "dvfs", strlen("dvfs"))) {
		off = 0;
		if (!strncmp(status, "disable", strlen("disable")))
			dst = 0x5A;
		else if (!strncmp(status, "enabled", strlen("enabled")))
			dst = 0x0;
		else
			goto ret_error;
	} else if (!strncmp(nodes, "cluster0", strlen("cluster0"))) {
		off = 8;
		dst = simple_strtoul(status, NULL, 10);
	} else if (!strncmp(nodes, "cluster1", strlen("cluster1"))) {
		off = 16;
		dst = simple_strtoul(status, NULL, 10);
	} else if (!strncmp(nodes, "cluster2", strlen("cluster2"))) {
		off = 24;
		dst = simple_strtoul(status, NULL, 10);
	} else if (!strncmp(nodes, "erase", strlen("erase"))) {
		src = 0;
		if (!strncmp(status, "all", strlen("all")))
			goto misc_write;
		else
			goto ret_error;
	} else {
		goto ret_error;
	}

	if (flag && strlen(flag)) {
		if (!strncmp(flag, "voltcut", strlen("voltcut")))
			type = 0x80;
		else
			goto ret_error;
	}

	if (common_raw_read("miscdata", (uint64_t)DVFS_SET_LEN,
			    (uint64_t)DVFS_SET_OFFSET, dvfs_data)) {
		errorf("dvfs_set read miscdata error...\n");
		return;
	} else {
		src = *data_ptr;
		dprintf(INFO,"before got miscdata (0x%x)\n", src);

		src = (src & (~(0xFF << off))) | (((dst & 0x7F) | type) << off);
		dprintf(INFO,"after set miscdata (0x%x)\n", src);
	}

misc_write:
	if (common_raw_write("miscdata", (uint64_t)DVFS_SET_LEN, (uint64_t)0,
			     (uint64_t)DVFS_SET_OFFSET, (char *)(&src)))
		fastboot_fail("write partition <miscdata> fail");
	else
		fastboot_okay("please reboot for continue");
	return;

ret_error:
	errorf("dvfs_set unkonwn command !!! \n");
	fastboot_fail("unkonwn command !!! ");
	return;
}

void fb_cmd_usb2spuart_set_config(char *nodes)
{
	/* get cmd pointer */
	char *status = nodes;
	char usb2spuart_on[]="enable";
	char usb2spuart_off[]="disable";

	/* turn on sub2spuart */
	if (!strncmp(status, "enable", strlen("enable"))) {

		if (common_raw_write("miscdata", (uint64_t)USB2SPUART_LENGTH_ENABLE, (uint64_t)0,
					 (uint64_t)USBMUX_DATA_OFFSET, usb2spuart_on)) {
			fastboot_fail("write partition <miscdata> fail");
		}
		else {
			dprintf(INFO,"write partition <miscdata> successfully \n");
		}

	} else if (!strncmp(status, "disable", strlen("disable"))) {
		/* turn off usb2spuart para: part_name lenth 0 address src*/

		if (common_raw_write("miscdata", (uint64_t)USB2SPUART_LENGTH_DISABLE, (uint64_t)0,
					 (uint64_t)USBMUX_DATA_OFFSET, usb2spuart_off)) {
			fastboot_fail("write partition <miscdata> fail");
		}
		else {
			dprintf(INFO,"write partition <miscdata> successfully \n");
		}

	}else {
		/* error cmd */
		errorf("usb2spuart_set read miscdata error...\n");
	}
}

#ifdef CONFIG_FASTBOOT_AUTHORIZE
int fb_cmd_oem_auth_begin(void)
{
	int len = 0;
	FDL_OT_Auth_ack_t ack;
	int i;
	int ack_length = sizeof(ack.M1);
	char *strH = malloc(ack_length * 2 + 1);
	if (strH == NULL) {
		errorf("No enougth memory for strH_M1\n");
		return;
	}
	memset(strH, 0, ack_length * 2);

	if ((len = authentication(ack.M1, 1)) <= 0) {
		errorf("auth fail return %d\n", len);
		free(strH);
		return -1;
	}

	/* For skip fastboot auth process */
	// if (fb_check_secboot_enable())
	// 	ack.mStatus = 1;
	// else
		ack.mStatus = 0;

	for (i = 0; i < sizeof(ack.M1); i++) {
		sprintf((char*)strH+2*i, "%02X", ack.M1[i]);
	}

	dprintf(INFO, "ack_length:%d, i:%d\n", ack_length, i);

	fastboot_response_info(strH);

	dprintf(ALWAYS, "auth_begin process success, ack.mStatus:%d\n", ack.mStatus);

	fastboot_state = STATE_COMMAND;
	fastboot_okay("");
	fastboot_state = STATE_COMPLETE;

	if (1 == ack.mStatus)
		fb_auth_sts = DL_SECURE_VERIFY_PASS;
	else
		fb_auth_sts = DL_AUTH_REQ;

	free(strH);
	return 0;
}

int fb_cmd_oem_auth_verify(void *data, uint64_t sz)
{
	if ((fb_auth_sts != DL_AUTH_REQ) || product_auth_key == '\0') {
		errorf("fb_auth_sts:0x%x, product_auth_key[0]:%s\n", fb_auth_sts, product_auth_key);
		goto fail;
	}

	if (authentication(product_auth_key, 0) < 0) {
		errorf("authentication fail\n");
		goto fail;
	}

	dprintf(ALWAYS, "auth_verify success\n");
	fb_auth_sts = DL_SECURE_VERIFY_PASS;

	return 0;

fail:
	fb_auth_sts = DL_AUTH_INIT;
	return -1;
}
#endif

#ifdef CONFIG_HMD_FASTBOOT
void display_HMD_unlock(bool isUnlock)
{
			lcd_clear();

			lcd_printf("\n\n\n\n");
			lcd_printf("\n\n\n\n");
			console_setFGColor(0x00FFF900);

			lcd_printf("   /|\\    \n"
			    	   "  / | \\   \n"
			   		   " /  0  \\  \n"
			   		   " -------  \n\n");

			console_setFGColor(0xFFFFFFFF);

			lcd_printf("HMD phones already feature a fully optimized,certificated and tested version of Android.\n");
			lcd_printf("While unlocking the bootloader will allow you to customize your device,\n");
			lcd_printf("we ask that you do not proceed unless you have read and fully agree to the following:\n\n");


			lcd_printf("- once your phone is unlocked, it will no longer be covered by the manufacturer's limited warranty provided by HMD Global.\n");
			lcd_printf("- Once a device is unlocked, the process cannot be undone.\n");
			lcd_printf("- Unlocking a device means you may lose some of its functionalities,\n"
				       "including but not limit to telephone, radio, audio, video, payment, encryption and DRM.\n");
			lcd_printf("- After unlocking, all media and content on the device will be erased,\n"
				       "and you will need to reinstall all applications.\n");
			lcd_printf("- Applications may not work anymore.\n");
			lcd_printf("- You could cause permanent/physical damage to your device.\n");
			lcd_printf("- Your device may become unsafe to the point of causing you harm.\n\n\n");

			lcd_printf("Press the Volume Up/Volume Down to select whether to unlock the bootloader,then the power button to continue.\n\n");
			lcd_printf("------------------------------------------------------------------------\n\n");

			if(isUnlock)
			{
				lcd_printf("       Don't unlock - Do not unlock bootloader and restart phone.\n\n");
				lcd_printf("   --> Unlock - Unlock bootloader.\n\n");
			}
			else
			{
				lcd_printf("   --> Don't unlock - Do not unlock bootloader and restart phone.\n\n");
				lcd_printf("       Unlock - Unlock bootloader.\n\n");
			}
}
void display_HMD_lock(bool isLock)
{
	lcd_clear();
	lcd_printf("\n   fastboot mode\n");
	lcd_printf("\n   INFO: LOCK FLAG IS : UNLOCK!!!\n");
	lcd_printf("\n   Warning: lock device may erase user data.\n");
	lcd_printf("   Press volume down button to confirm that,then press power button to start lock.\n");
	lcd_printf("   Press volume up button to cancel,then press power button to return bootloader.\n");

	if(isLock)
	{
		lcd_printf("\n\n   Info:confirm lock device,press power button to start lock!\n");
	}
	else
	{
		lcd_printf("\n\n   Info:cancel lock device,press power button to return bootloader!\n");
	}
}
#endif

void fb_cmd_oem(const char *arg, void *data, uint64_t sz)
{
	char subcmd[64];
	char s[34];
	int i, j;
	int key_code;
	bool butt_check = false;
	int ret = -1;
#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
	unsigned char permission[64] = {0};
	unsigned char *cmd_p = NULL;
	unsigned char *data_buf = ImageInfo.base_address;
#endif
	const char *delim = " ";

	debugf("arg#%s#, data: %p, sz: 0x%llx\n", arg, data, sz);
#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
	if (!strncmp(arg + 1, "repair", strlen("repair"))) {
		fb_cmd_oem_repair(arg + 1, data, sz);
		return;
	}

	cmd_p = (const char*)arg + 12;
#endif

	memset(subcmd, 0, sizeof(subcmd));
	memset(s, 0, sizeof(s));
	memset(product_sn_token, 0, sizeof(product_sn_token));

	strncpy(subcmd, strtok(arg, delim), sizeof(subcmd)-1);
	if (strlen(subcmd)) {
		debugf("subcmd +%s+\n", subcmd);
	} else {
		debugf("subcmd is null.\n");
		fastboot_fail("subcmd is null.");
		return;
	}

#ifdef CONFIG_FASTBOOT_AUTHORIZE
	if(!strcmp(subcmd, "auth_begin")) {
		ret = fb_cmd_oem_auth_begin();
		if (ret)
			fastboot_fail("auth_begin fail");
		return;
	} else if (!strcmp(subcmd, "auth_verify")) {
		dprintf(INFO,"%s\n", arg + 13);
		ret = fb_cmd_oem_auth_verify(arg+13, sz);
		if(ret == 0)
			fastboot_okay("");
		else
			fastboot_fail("auth_verify fail");
		return;
	}
#endif

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
	if(!strcmp(subcmd, "auth_start"))
	{
		ret = rsa_encrypt_data(encrypt_data);
		if(ret == 0)
			fastboot_okay("");
		else
			fastboot_fail("rsa encrypt failed");
		return;
	}else if (!strcmp(subcmd, "permission")){
		strncpy(product_sn_token, get_product_sn(), PRODUCT_SN_TOKEN_MAX_SIZE-1);
		if(!strcmp(cmd_p, "flash")){
			dprintf(INFO,"the cmd is flash\n");
			ret = rsa_decrypt_data("flash", data_buf, product_sn_token);
			if(ret == 0)
				fastboot_fail("flash decrypt failed");
			else
				fastboot_okay("");
		}else if(!strcmp(cmd_p, "simlock")){
			dprintf(INFO,"the cmd is simlock\n");
			ret = rsa_decrypt_data("simlock", data_buf, product_sn_token);
			if(ret == 0)
				fastboot_fail("simlock decrypt failed");
			else
				fastboot_okay("");
		}else if(!strcmp(cmd_p, "repair")){
			dprintf(INFO,"the cmd is repair\n");
			ret = rsa_decrypt_data("repair", data_buf, product_sn_token);
			if(ret == 0)
				fastboot_fail("repair decrypt failed");
			else
				fastboot_okay("");
		}
			open_permission = open_permission | ret;
			return;
	}else if (!strcmp(subcmd, "getpermissions")){
		dprintf(INFO,"the cmd is getpermissions\n");
		if(open_permission == 0x0)
		{
			fastboot_response_info("permissions=None");
			fastboot_okay("");
		}
		if(open_permission == 0x1)
		{
			fastboot_response_info("permissions=flash");
			fastboot_okay("");
		}
		if(open_permission == 0x2)
		{
			fastboot_response_info("permissions=simlock");
			fastboot_okay("");
		}
		if(open_permission == 0x3)
		{
			fastboot_response_info("permissions=flash|simlock");
			fastboot_okay("");
		}
		if(open_permission == 0x4)
		{
			fastboot_response_info("permissions=repair");
			fastboot_okay("");
		}
		if(open_permission == 0x5)
		{
			fastboot_response_info("permissions=flash|repair");
			fastboot_okay("");
		}
		if(open_permission == 0x6)
		{
			fastboot_response_info("permissions=simlock|repair");
			fastboot_okay("");
		}
		if(open_permission == 0x7)
		{
			fastboot_response_info("permissions=flash|simlock|repair");
			fastboot_okay("");
		}
			return;
	} else
#endif

	if (!strcmp(subcmd, "pull")) {
		char *param = strtok(NULL, delim);

		if (param && strlen(param)) {
			dprintf(INFO,"param %s\n", param);
			if (!strncmp(param, "0x", 2)) {
				if (fb_cmd_oem_upload_ddr(param)) {
					fastboot_fail("upload range fail.");
				}
			} else if (!strncmp(param, "log_buffer", sizeof("log_buffer"))) {
				if (fb_cmd_oem_upload_log_buffer()) {
					fastboot_fail("upload lk_log fail.");
				}
			} else if (!strncmp(param, "lk_memory", sizeof("lk_memory"))) {
				if (fb_cmd_oem_upload_lk_memory()) {
					fastboot_fail("upload lk_memory fail.");
				}
			} else {
				if (fb_cmd_oem_upload_flash(param)) {
					fastboot_fail("upload partition fail.");
				}
			}
		} else
			fastboot_fail("unknown format.");
		return;
	} else if (!strcmp(subcmd, "get_identifier_token")) {
		strncpy(product_sn_token, get_product_sn(), PRODUCT_SN_TOKEN_MAX_SIZE-1);
		debugf("Identifier token: %s.\n", product_sn_token);
		fastboot_response_data("SN" ,"Identifier token:\n");

		for (j = 0; j < 4; j++){

			for (i = 0; i < 16; i++){
				if (product_sn_token[16*j+i] == '\0') {
					strcat(s,"\n");
					break;
				}
				if (i < 15) {
					sprintf(s+i*2, "%02x", product_sn_token[16*j+i]);
				} else {
					sprintf(s+i*2, "%02x\n", product_sn_token[16*j+i]);
				}
			}

			fastboot_response_data("SN", s);

			if (product_sn_token[16*j+i] == '\0') {
				break;
			}
		}

		fastboot_okay("");
	} else if (!strcmp(subcmd, "startup_core")) {
		char *core = strtok(NULL, delim);

		if (core && strlen(core))
			fb_cmd_startup_corex(core);
		else
			fastboot_fail("unknown format.");
		return;
	} else if (!strncmp(subcmd, "fb_write_protect_test", strlen(subcmd))) {
		debugf("oem SECBOOT:execute fb_write_protect_test!\n");
		char *partition_name = strtok(NULL, delim);
		char *len = strtok(NULL, delim);
		char *action = strtok(NULL, delim);
		char *write_pattern = NULL;
		if (partition_name && strlen(partition_name) &&
			len && strlen(len) && action && strlen(action)) {
			if(!strcmp(action, "write")) {
				write_pattern = strtok(NULL, delim);
				if(write_pattern && strlen(write_pattern))
					fb_write_protect_test(partition_name, len, action, write_pattern);
				else
					fastboot_fail("write unknown format.");
				return;
			}
			fb_write_protect_test(partition_name, len, action, write_pattern);
		} else
			fastboot_fail("unknown format.");
		return;
	} else if (!strncmp(subcmd, "ddr_memtest", strlen(subcmd))) {
		debugf("oem SECBOOT:execute ddr_memtest!\n");
		char *test_addr = strtok(NULL, delim);
		char *test_len = strtok(NULL, delim);
		char *test_times = strtok(NULL, delim);
		if (test_addr && strlen(test_addr) && test_len && strlen(test_len)) {
			if (test_times && strlen(test_times))
				fb_cmd_ddr_memtest(test_addr, test_len, test_times);
			else
				fb_cmd_ddr_memtest(test_addr, test_len, "1");
		} else
			fastboot_fail("unknown format.");
		return;
	} else if (!strncmp(subcmd, "ddr_set", strlen(subcmd))) {
		debugf("oem SECBOOT:execute ddr_set!\n");
		char *para1 = strtok(NULL, delim);
		char *para2 = strtok(NULL, delim);
		if (para1 && strlen(para1) && para2 && strlen(para2))
			fb_cmd_ddr_set_config(para1, para2);
		else if(para1 && (!strncmp(para1, "erase_all", strlen(para1))))
			fb_cmd_ddr_set_config(para1, NULL);
		else if (para1 && (!strncmp(para1, "get_configs", strlen(para1))))
			fb_cmd_ddr_set_config(para1, NULL);
		else
			fastboot_fail("unknown format.");
		return;
	} else if (!strncmp(subcmd, "setwatchdog", strlen("setwatchdog"))) {
		debugf("oem SECBOOT:execute setwatchdog!\n");
		char *wdt_status = strtok(NULL, delim);

		if (wdt_status && strlen(wdt_status)){
			fb_cmd_set_wdt_status(wdt_status);
		} else
			fastboot_fail("unknown format.");
		return;
	} else if (!strncmp(subcmd, "dvfs_set", strlen("dvfs_set"))) {
		debugf("oem SECBOOT:execute dvfs_set!\n");
		char *para1 = strtok(NULL, delim);
		char *para2 = strtok(NULL, delim);
		char *para3 = strtok(NULL, delim);

		if (para1 && strlen(para1) && para2 && strlen(para2))
			fb_cmd_dvfs_set_config(para1, para2, para3);
		else
			fastboot_fail("unknown format.");
	}else if(!strncmp(subcmd, "usb2spuart", strlen("usb2spuart"))) { /* usb switch sp uart function */
		debugf("oem SECBOOT:execute usb2spuart!\n");
		char *para1 = strtok(NULL, delim);

		if (para1 && strlen(para1) ){  /* success cmd */
			fb_cmd_usb2spuart_set_config(para1);
			fastboot_okay("Info:fb_cmd_usb2spuart_set_config is ok! ");
		}
		else {
			fastboot_fail("unknown format.");
		}
	}else if (!strcmp(subcmd, "unlock")) {
#ifdef CONFIG_HMD_FASTBOOT
    	if (get_lock_status() == VBOOT_STATUS_UNLOCK) {
			debugf("bootloader has been unlocked\n");
			fastboot_fail("Bootloader can not been unlocked repeatly.");
			return;
       	}

      	strncpy(product_sn_token, get_product_sn(), PRODUCT_SN_TOKEN_MAX_SIZE-1);
		debugf("Identifier token: %s.\n", product_sn_token);

        if (1 != fb_verify_unlockkey(product_sn_signature, product_sn_token)) {// Modified by changmei.chen if success, the return value is 1 not -1
        	debugf("execute <fastboot oem unlock> command fail.\n");
        	fastboot_fail("Unlock bootloader fail.");
        	return;
        }

		display_HMD_unlock(false);

		int sec_time_count = 30*5;
		int power_key_code = PW_KEY_NOT_PRESSED;
		int flag_unlock = 0;
        while(sec_time_count) {
			mdelay(200);
			sec_time_count --;
        	/* continue check till button pressed */
			key_code = board_key_scan();
			power_key_code = power_button_pressed();
        	//key_code = wait_for_keypress();
        	if (key_code == KEY_VOLUMEDOWN) {
        		display_HMD_unlock(false);
				//lcd_printf("\n\n   Info:display_not_unlock!\n\n");
        		flag_unlock = 0;
        	} else if (key_code == KEY_VOLUMEUP) {
        		display_HMD_unlock(true);
				//lcd_printf("\n\n   Info:display_unlock!\n\n");
        		flag_unlock = 1;
        	}else if (power_key_code == PW_KEY_PRESSED) {
        		if (flag_unlock == 1) {
        			butt_check = true;
					break;
        		} else {
        			lcd_clear();
        			lcd_printf("\n\n  fastboot mode\n\n");
					lcd_printf("\n\n  Info:user cancel unlock bootloader!\n\n");
        			fastboot_okay("Info:user cancel unlock bootloader! ");
        			return;
        		}
        	}
        }

		if(!butt_check)
		{
			lcd_clear();
			lcd_printf("\n\n  fastboot mode\n\n");
			lcd_printf("\n\n  Info:unlock bootloader timeout! \n\n");
			fastboot_fail("unlock bootloader timeout.");
			return;
		}
        lcd_printf("   Begin to erase user data...\n");
        if (0 != common_raw_erase("userdata", 0, 0)) {
        	debugf("erase userdata failed\n");
           	fastboot_fail("Erase userdata fail.");
           	return;
     	}

        lcd_printf("   Begin to erase metadata ...\n");
		if (0 != common_raw_erase("metadata", 0, 0)) {
			debugf("erase metadata failed\n");
			fastboot_fail("Erase metadata fail.");
			return;
		}

      	if (set_lock_status(VBOOT_STATUS_UNLOCK)) {
           	debugf("set_lock_status failed\n");
           	fastboot_fail("Unlock bootloader fail.");
           	return;
      	}

      	debugf("execute <fastboot oem unlock> successfully.\n");
      	lcd_printf("Incfo:Unlock bootloader success!\n");
      	fastboot_okay("Info:Unlock bootloader success! ");
      	// after unlock bootloader, devices will reboot normal mode
      	reboot_devices(CMD_NORMAL_MODE);
#else
		if (get_lock_status() == VBOOT_STATUS_UNLOCK) {
			debugf("bootloader has been unlocked\n");
			fastboot_fail("Bootloader can not been unlocked repeatly.");
			return;
       	}

		strncpy(product_sn_token, get_product_sn(), PRODUCT_SN_TOKEN_MAX_SIZE-1);
		debugf("Identifier token: %s.\n", product_sn_token);

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
        if (-1 != fb_verify_unlockkey(product_sn_signature, product_sn_token)) {
        	debugf("execute <fastboot oem unlock> command fail.\n");
        	fastboot_fail("Unlock bootloader fail.");
        	return;
        }
#else
        if (verify_product_sn_signature()) {
          	debugf("execute <fastboot oem unlock> command fail.\n");
         	fastboot_fail("Unlock bootloader fail.");
           	return;
       	}
#endif

        lcd_printf("Warning: Unlock device may erase user data.\n");
        lcd_printf("Press volume down button to confirm that.\n");
        lcd_printf("Press verify_product_sn_signatureolume up button to cancel.\n");
        while(!butt_check) {
        	/* continue canheck till button pressed */
           	key_code = wait_for_keypress();
           	if (key_code == KEY_VOLUMEDOWN) {
               	butt_check = true;
            } else if (key_code == KEY_VOLUMEUP) {
             	lcd_printf("Info:user cancelled!\n");
               	fastboot_okay("Info:user cancel unlock bootloader! ");
               	return;
            }
       	}
       	lcd_printf("Begin to erase user data...\n");
        if (0 != common_raw_erase("userdata", 0, 0)) {
        	debugf("erase userdata failed\n");
           	fastboot_fail("Erase userdata fail.");
           	return;
     	}

      	if (set_lock_status(VBOOT_STATUS_UNLOCK)) {
           	debugf("set_lock_status failed\n");
           	fastboot_fail("Unlock bootloader fail.");
           	return;
      	}

      	debugf("execute <fastboot oem unlock> successfully.\n");
      	lcd_printf("Incfo:Unlock bootloader success!\n");
      	fastboot_okay("Info:Unlock bootloader success! ");
#endif
	} else if(!strcmp(subcmd, "lock")){
#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
            if (get_lock_status() == VBOOT_STATUS_LOCK) {
                fastboot_fail("Bootloader can not been locked repeatly.");
                return;
            }

            if (set_lock_status(VBOOT_STATUS_LOCK)) {
                debugf("execute <fastboot_response_snt flashing lock_bootloader> command fail.\n");
                fastboot_fail("Lock butt_checkootloader fail.");
                return;
            }

            debugf("execute <fastboot flashing lock_bootloader> command successfully.\n");
            lcd_printf("Info:Lock bootloader success!\n");
            fastboot_okay("Lock bootloader successfully!");
#endif
    } else if(!strcmp(subcmd, "getsecurityversion")) {
		debugf("execute getsecurityversion!\n");
#ifdef CONFIG_HMD_FASTBOOT
		//now it is 112-version for adapting HMD flash tool
		const char securityversion[] = "112";
#else
		const char securityversion[] = "1";//Customer-defined, defaults to 1
#endif	
		fastboot_response_info(securityversion);

		fastboot_okay("");
	} else if(!strcmp(subcmd, "getversions")) {
		debugf("execute getversions!\n");

		char temp[128];
		char model[64] = "model=";
		char sub_model[64] = "sub_model=";
		char sw_vr[64] = "software version=";
		char sw_model[64] = "SW model=";
		char build_number[64] = "build number=";
		char hw_vr[64] = "hardware version=";
		char rf_id[64] = "RF band id=";

		char temp_config[7][32];
		memset(temp_config, 0, sizeof(temp_config));

		if (0 != common_raw_read("miscdata", (uint64_t)sizeof(temp),
				(uint64_t)ALL_VERSION_OFFSET, temp)) {
			errorf("<miscdata> read error\n");
			fastboot_fail("read miscdata fail!");
			return;
		}

		char *token = strtok(temp, ";");
		i = 0;
		while (token != NULL) {
			debugf("%s\n", token);

			strlcpy(temp_config[i], token, sizeof(temp_config[0]));

			token = strtok(NULL, ";");
			i++;
		}

		debugf("get parameter: %d\n", i);

		/* 0 (bootloader) model=$(ro.product.model) */
		sprintf(model + strlen(model), "%s", temp_config[MODLE]);
		fastboot_response_info(model);

		/* 1 (bootloader) sub_model=none */
		sprintf(sub_model + strlen(sub_model), "%s", temp_config[SUB_MD]);
		fastboot_response_info(sub_model);

		/* 2 (bootloader) software version=$(ro.build.display.id) */
		sprintf(sw_vr + strlen(sw_vr), "%s", temp_config[SW_VR]);
		fastboot_response_info(sw_vr);

		/* 3 (bootloader) SW model=$(ro.product.model.num) */
		sprintf(sw_model + strlen(sw_model), "%s", temp_config[SW_MD]);
		fastboot_response_info(sw_model);

		/* 4 (bootloader) build number=A01 or B01 */
		sprintf(build_number + strlen(build_number), "%s", temp_config[BD_NU]);
		fastboot_response_info(build_number);

		/* 5 (bootloader) hardware version=$(Defined by ODM) */
		sprintf(hw_vr + strlen(hw_vr), "%s", temp_config[HW_VR]);
		fastboot_response_info(hw_vr);

		/* 6 (bootloader) RF band id=$(Defined by ODM) */
		sprintf(rf_id + strlen(rf_id), "%s", temp_config[RF_ID]);
		fastboot_response_info(rf_id);

		fastboot_okay("");
#ifdef CONFIG_FASTBOOT_FLASH
	} else if (!strncmp(subcmd, "backupnv", strlen("backupnv"))) {
			debugf("oem SECBOOT:execute backupnv!\n");
			s_backupnv_flag = 1;
			fastboot_okay("");
#endif
#if defined FASTBOOT_GET_IMEI_SUPPORT && defined CONFIG_FASTBOOT_FLASH
	} else if (!strncmp(subcmd, "imei", strlen("imei"))) {
			debugf("oem SECBOOT:execute imei!\n");
			fb_cmd_oem_get_imei();
			fastboot_okay("");
#endif
	}
    else if(!strcmp(subcmd, "set")) {
	//add by jinqiang for fastboot set loglevel  usage:fastboot oem set loglevel 7
		debugf("%s!\n",arg);
		memset(subcmd, 0, sizeof(subcmd));
		strncpy(subcmd, strtok(NULL, delim), sizeof(subcmd)-1);
		if(!strcmp(subcmd, "loglevel"))
		{
			 char buf[20]="enable:level=";
			 memset(subcmd, 0, sizeof(subcmd));
			 strncpy(subcmd, strtok(NULL, delim), sizeof(subcmd)-1);
			 sprintf(buf + strlen(buf), "%s",subcmd);

			 if(!strncmp(subcmd,"7",strlen(subcmd))){
				memset(buf, 0, sizeof(buf));
				strcpy(buf,"enable");
			 }
			 debugf("fastboot set loglevel------%s!\n",buf);
			 if(0 != common_raw_write("miscdata",sizeof(buf),(uint64_t)0, DEBUG_INFO_OFFSET,  buf)){
				debugf("write miscdata loglevel data error.\n");
			 }
			 fastboot_okay("set loglevel  OK!!!");
		}
	}	
#ifdef CONFIG_HMD_FASTBOOT	
	else if (!strcmp(subcmd, "off-mode-charge")) {
		debugf("oem off-mode-charge setting\n");
		char buf[2] = {0};
		if (!strncmp(arg+16,"0",strlen("0"))) {
			sprintf(buf, "%d", 1);
		} else if(!strncmp(arg+16,"1",strlen("1"))) {
			sprintf(buf, "%d", 0);
		} else if(!strncmp(arg+16,"2",strlen("2"))) {
			sprintf(buf, "%d", 2);
		}
		if(buf[0] != 0) {
			debugf("into set %s mode!\n", buf);
			if (common_raw_write("miscdata", MISCDATA_OFF_MODE_CHARGE_FLAG_DATA_LEN, (uint64_t)0, (uint64_t)MISCDATA_OFF_MODE_CHARGE_FLAG_BASE, buf)) {
				errorf("write miscdata off-mode-charge flag fail!\n");
			} else {
				if(strcmp(buf, "1")) {
					debugf("off-mode-charge on now\n");
				} else {
					debugf("off-mode-charge off now\n");
				}
			}
		}
		debugf("end of off-mode-charge setting!\n");
		fastboot_okay("");
	}
	else if(!cmd_fastboot_oem_handle(subcmd, arg+strlen(subcmd)+1, data, sz)) {
	}
#endif
	else {
		fastboot_fail("unknown cmd.");
	}
}

void fb_cmd_flashing(const char *arg, void *data, uint64_t sz)
{
	char subcmd[64];
	const char *delim = " ";
	int i = 0;
	unsigned int all_zero_flag = 1;
	bool butt_check = false;
	int key_code;

	debugf("arg#%s#, data: %p, sz: 0x%llx\n", arg, data, sz);

	memset(subcmd, 0, sizeof(subcmd));

	strncpy(subcmd, strtok(arg, delim), sizeof(subcmd)-1);
	if (strlen(subcmd)) {
		debugf("subcmd +%s+\n", subcmd);
	} else {
		debugf("submd is null.\n");
		fastboot_fail("subcmd is null.");
		return;
	}

	if (!strcmp(subcmd, "lock")) {
	#ifdef CONFIG_HMD_FASTBOOT
		if (get_lock_status() == VBOOT_STATUS_LOCK) {
			debugf("Bootloader has been locked! Flashing lock is not allowed!\n");
			fastboot_fail("Flashing lock is not allowed!");
			return;
		}
		if (fastboot_mode_flag != MODE_FASTBOOT_REPAIR) {
			//lcd_printf("\n\n   Flashing lock is not allowed! fastboot_mode_flag=%d!\n", fastboot_mode_flag);
			debugf("Flashing lock is not allowed! fastboot_mode_flag=%d!\n", fastboot_mode_flag);
			fastboot_fail("Flashing lock is not allowed!");
			return;
		}
		display_HMD_lock(false);//modify for NYX-992 incomplete character display by hyinfeng
		int flag_lock = 0;
		while(!butt_check) {
			/* continue check till button pressed */
			key_code = wait_for_keypress();
			if (key_code == KEY_VOLUMEUP) {
				display_HMD_lock(true);//modify for NYX-992 incomplete character display by hyinfeng
				//lcd_printf("\n   Info:confirm lock device,press power button to start lock!\n");
				flag_lock = 1;
			} else if (key_code == KEY_VOLUMEDOWN) {
				display_HMD_lock(false);//modify for NYX-992 incomplete character display by hyinfeng
				//lcd_printf("\n   Info:cancel lock device,press power button to return bootloader!\n");				
				flag_lock = 0;
			} else if (key_code == PW_KEY_PRESSED) {
				if (flag_lock == 1) {
					butt_check = true;
				} else {
					lcd_printf("\n\n   Info:user cancel lock bootloader!\n\n");
					fastboot_fail("Info:user cancel lock bootloader! ");
					reboot_devices(CMD_FASTBOOT_MODE);
					return;
				}
			}
		}
		lcd_printf("   Begin to erase user data...\n");
		if (0 != common_raw_erase("userdata", 0, 0)) {
			debugf("erase userdata failed\n");
			fastboot_fail("Erase userdata fail.");
			return;
		}

		lcd_printf("   Begin to erase metadata ...\n");
		if (0 != common_raw_erase("metadata", 0, 0)) {
			debugf("erase metadata failed\n");
			fastboot_fail("Erase metadata fail.");
			return;
		}

		if (set_lock_status(VBOOT_STATUS_LOCK)) {
			debugf("execute <fastboot flashing lock_bootloader> command fail.\n");
			fastboot_fail("Lock bootloader fail.");
			return;
		}

		debugf("execute <fastboot flashing lock_bootloader> command successfully.\n");
		lcd_printf("   Info:Lock bootloader success!\n");
		fastboot_okay("Lock bootloader successfully!   ");
		// after lock bootloader, devices will reboot normal mode
		reboot_devices(CMD_NORMAL_MODE);
	#else
		if (get_lock_status() == VBOOT_STATUS_LOCK) {
		        lcd_printf("   Info:Bootloader has been locked!\n");
			fastboot_okay("Bootloader has been locked!\n");
			return;
		}

		lcd_printf("\n   Warning: lock device may erase user data.\n");
		lcd_printf("   Press volume down button to confirm that.\n");
		lcd_printf("   Press volume up button to cancel.\n");
		while(!butt_check) {
			/* continue check till button pressed */
			key_code = wait_for_keypress();
			if (key_code == KEY_VOLUMEDOWN) {
				butt_check = true;
			} else if (key_code == KEY_VOLUMEUP) {
				lcd_printf("   Info:user cancelled!\n");
				fastboot_okay("Info:user cancel lock bootloader!   ");
				return;
			}
		}
		lcd_printf("   Begin to erase user data...\n");
		if (0 != common_raw_erase("userdata", 0, 0)) {
			debugf("erase userdata failed\n");
			fastboot_fail("Erase userdata fail.");
			return;
		}

		lcd_printf("   Begin to erase metadata ...\n");
		if (0 != common_raw_erase("metadata", 0, 0)) {
			debugf("erase metadata failed\n");
			fastboot_fail("Erase metadata fail.");
			return;
		}

		if (set_lock_status(VBOOT_STATUS_LOCK)) {
			debugf("execute <fastboot flashing lock_bootloader> command fail.\n");
			fastboot_fail("Lock bootloader fail.");
			return;
		}

		debugf("execute <fastboot flashing lock_bootloader> command successfully.\n");
		lcd_printf("   Info:Lock bootloader success!\n");
		fastboot_okay("Lock bootloader successfully!   ");
	#endif
	} else if (!strcmp(subcmd, "unlock_critical")) {
		fastboot_fail("Not implement.");
	} else if (!strcmp(subcmd, "lock_critical")) {
		fastboot_fail("Not implement.");
	} else if (!strcmp(subcmd, "get_unlock_ability")) {
		fastboot_fail("Not implement.");
	} else if (!strcmp(subcmd, "get_unlock_bootloader_nonce")) {
		fastboot_fail("Not implement.");
	} else if (!strcmp(subcmd, "lock_bootloader")) {
		if (get_lock_status() == VBOOT_STATUS_LOCK) {
		        lcd_printf("   Info:Bootloader has been locked!\n");
			fastboot_okay("Bootloader has been locked!\n");
			return;
		}

		lcd_printf("\n   Warning: lock device may erase user data.\n");
		lcd_printf("   Press volume down button to confirm that.\n");
		lcd_printf("   Press volume up button to cancel.\n");
		while(!butt_check) {
			/* continue check till button pressed */
			key_code = wait_for_keypress();
			if (key_code == KEY_VOLUMEDOWN) {
				butt_check = true;
			} else if (key_code == KEY_VOLUMEUP) {
				lcd_printf("   Info:user cancelled!\n");
				fastboot_okay("Info:user cancel lock bootloader!   ");
				return;
			}
		}
		lcd_printf("   Begin to erase user data...\n");
		if (0 != common_raw_erase("userdata", 0, 0)) {
			debugf("erase userdata failed\n");
			fastboot_fail("Erase userdata fail.");
			return;
		}

		lcd_printf("   Begin to erase metadata ...\n");
		if (0 != common_raw_erase("metadata", 0, 0)) {
			debugf("erase metadata failed\n");
			fastboot_fail("Erase metadata fail.");
			return;
		}

		if (set_lock_status(VBOOT_STATUS_LOCK)) {
			debugf("execute <fastboot flashing lock_bootloader> command fail.\n");
			fastboot_fail("Lock bootloader fail.");
			return;
		}

		debugf("execute <fastboot flashing lock_bootloader> command successfully.\n");
		lcd_printf("   Info:Lock bootloader success!\n");
		fastboot_okay("Lock bootloader successfully!   ");
	} else if (!strcmp(subcmd, "unlock_bootloader")) {
		if (get_lock_status() == VBOOT_STATUS_UNLOCK) {
			debugf("bootloader has been unlocked\n");
			fastboot_fail("Bootloader can not been unlocked repeatly.");
			return ;
		}

		for (i = 0; i < 64; i++) {
			if (product_sn_token[i]) {
				all_zero_flag = 0;
			}
		}
		if (all_zero_flag){
			fastboot_fail("Please firstly execute <fastboot oem get_identifier_token>");
			return;
		}

        memset(product_sn_signature, 0, sizeof(product_sn_signature));
        memcpy(product_sn_signature, data, PRODUCT_SN_SIGNATURE_SIZE);

		//dumpHex("flashing unlock_bootloader", data, PRODUCT_SN_SIGNATURE_SIZE);

		if (verify_product_sn_signature()) {
			debugf("execute <fastboot flashing unlock_bootloader> command fail.\n");
			fastboot_fail("Unlock bootloader fail.");
			return;
		}

        memset(product_sn_signature, 0, sizeof(product_sn_signature));

		lcd_printf("\n   Warning: Unlock device may erase user data.\n");
		lcd_printf("   Press volume up button to confirm that.\n");
		lcd_printf("   Press volume down button to cancel.\n");
		while(!butt_check) {
			/* continue check till button pressed */
			key_code = wait_for_keypress();
			if (key_code == KEY_VOLUMEDOWN) {
				butt_check = true;
			} else if (key_code == KEY_VOLUMEUP) {
				lcd_printf("   Info:user cancelled!\n");
				fastboot_okay("Info:user cancel unlock bootloader!   ");
				return;
			}
		}
		lcd_printf("   Begin to erase user data...\n");
		if (0 != common_raw_erase("userdata", 0, 0)) {
			debugf("erase userdata failed\n");
			fastboot_fail("Erase userdata fail.");
			return;
		}

		lcd_printf("   Begin to erase metadata ...\n");
		if (0 != common_raw_erase("metadata", 0, 0)) {
			debugf("erase metadata failed\n");
			fastboot_fail("Erase metadata fail.");
			return;
		}

		if (set_lock_status(VBOOT_STATUS_UNLOCK)) {
			debugf("set_lock_status failed\n");
			fastboot_fail("Unlock bootloader fail.");
			return;
		}

		debugf("execute <fastboot flashing unlock_bootloader> successfully.\n");
		lcd_printf("   Info:Unlock bootloader success!\n");
		fastboot_okay("Info:Unlock bootloader success!   ");
	} else {
		fastboot_fail("unknown cmd.");
	}
}

static void fastboot_lcs_init(void)
{
    int ret = -1;
    char lcs[8] = {0};
    unsigned int *t_lcs = memalign(SZ_4K, sizeof(unsigned int));
    if (t_lcs == NULL) {
    	errorf("no enough heap for set getlcs\n");
    	fastboot_fail("no enough heap for set getlcs!");
    	return;
    }
    ret = get_lcs(t_lcs);

    if(ret == 0){
        snprintf(lcs, 8, "%d", *t_lcs);
        fastboot_publish("lcs", strdup(lcs));
    }else{
        debugf("get lcs fail\n");
    }

    free(t_lcs);
}

static void fastboot_token_init(void)
{
    char token_value[64] = {0};
    char tokenname[16] = {0};
    int p = 0, i = 0;

    strncpy(product_sn_token, get_product_sn(), PRODUCT_SN_TOKEN_MAX_SIZE-1);
    for(p=0;p<4;p++){
        if('\0' == product_sn_token[p*16])
            break;
        memset(token_value, 0, 64);
        memset(tokenname, 0, 16);
        for(i=p*16;i<((p+1)*16);i++){
            if('\0' == product_sn_token[p*16])
                break;
            sprintf(token_value+(i%16)*2, "%02x", product_sn_token[i]);
        }
        sprintf(tokenname, "tokenp%d", p+1);
        fastboot_publish(strdup(tokenname), strdup(token_value));
    }
}


static void fastboot_socid_init(void)
{
    int ret = -1;
    char socid_buf[64] = {0};
    char socid_name[16] = {0};
    int p = 0, i = 0;
    uint8_t *socIdBuff = memalign(SZ_4K, 32);

    if (socIdBuff == NULL) {
    	errorf("no enough heap for set socIdBuff\n");
    	fastboot_fail("no enough heap for set socIdBuff!");
    	return;
    }
    ret = get_socid(socIdBuff, 32);
    if(ret == 0){
        for(p=0; p<2; p++){
            memset(socid_buf, 0, 64);
            memset(socid_name, 0, 16);
            for(i=p*16;i<((p+1)*16);i++){
                sprintf(socid_buf+(i%16)*2, "%02x", socIdBuff[i]);
            }
            sprintf(socid_name, "socidp%d", p+1);
            fastboot_publish(strdup(socid_name), strdup(socid_buf));
        }
    }else{
        debugf("get socid fail\n");
    }
    free(socIdBuff);
}
#endif //SPRD_SECBOOT

void fb_cmd_boot(const char *arg, void *data, uint64_t sz)
{
	boot_img_hdr *hdr = (boot_img_hdr *)raw_header;
	unsigned kernel_actual;
	unsigned ramdisk_actual;

	if (memcmp(hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
		debugf("boot image headr: %s\n", hdr->magic);
		fastboot_fail("bad boot image header");
		return;
	}
	kernel_actual = ROUND_TO_PAGE(hdr->kernel_size, (KERNL_PAGE_SIZE - 1));
	if (kernel_actual <= 0) {
		fastboot_fail("kernel image should not be zero");
		return;
	}
	ramdisk_actual = ROUND_TO_PAGE(hdr->ramdisk_size, (KERNL_PAGE_SIZE - 1));
	if (0 == ramdisk_actual) {
		fastboot_fail("ramdisk size error");
		return;
	}

	memcpy((void *)hdr->kernel_addr, (void *)data + KERNL_PAGE_SIZE, kernel_actual);
	memcpy((void *)hdr->ramdisk_addr, (void *)data + KERNL_PAGE_SIZE + kernel_actual, ramdisk_actual);

	debugf("kernel @0x%08x (0x%08x bytes)\n", hdr->kernel_addr, kernel_actual);
	debugf("ramdisk @0x%08x (0x%08x bytes)\n", hdr->ramdisk_addr, ramdisk_actual);

	fastboot_okay("");
	usb_driver_exit();
	boot_linux(hdr->kernel_addr, hdr->tags_addr);
}

void fb_cmd_continue(const char *arg, void *data, uint64_t sz)
{
	fastboot_okay("");
	usb_driver_exit();
	normal_mode();
}

void fb_cmd_reboot(const char *arg, void *data, uint64_t sz)
{
	fastboot_okay("");
	reboot_devices(CMD_NORMAL_MODE);
}

/**
 * enter userpace fastboot mode
 * 1. clear flags on misc
 * 2. reboot into recovery mode
 */
void fb_cmd_reboot_recovery(const char *arg, void *data, uint64_t sz)
{
	if (0 != clear_recovery_not_run_fastbootd()) {
		errorf("write flags fail\n");
		fastboot_fail("write misc fail\n");
		goto err;
	}

	fastboot_okay("");
	usb_driver_exit();

	if(panel_enabled != 0)
		set_panel_sleep_in();

	reboot_devices(CMD_RECOVERY_MODE);
	return;

err:
	usb_driver_exit();
	reboot_devices(CMD_FASTBOOT_MODE);
}

/**
 * enter userpace fastboot mode
 * 1. set flags on misc
 * 2. reboot into recovery mode
 */
void fb_cmd_reboot_userspace_fastboot(const char *arg, void *data, uint64_t sz)
{
	if (0 != set_recovery_run_fastbootd()) {
		errorf("write flags fail\n");
		fastboot_fail("write misc fail\n");
		goto err;
	}

	fastboot_okay("");
	usb_driver_exit();
	reboot_devices(CMD_RECOVERY_MODE);
	return;

err:
	usb_driver_exit();
	reboot_devices(CMD_FASTBOOT_MODE);
}

void fb_cmd_reboot_bootloader(const char *arg, void *data, uint64_t sz)
{
	fastboot_okay("");
	usb_driver_exit();
	reboot_devices(CMD_FASTBOOT_MODE);
}

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
#define MAGIC_ENTER_EDL		{0x5a, 0x5a, 0x12, 0x34}
/* set edl download flag on miscdata */
int fb_require_reboot_edl(int on)
{
	uint64_t offset = MISCDATA_EDL_OFFSET;
	int length = 4;
	char set[] = MAGIC_ENTER_EDL, clr[4] = {0};

	if (common_raw_write("miscdata", (uint64_t)length, (uint64_t)0, offset,
			on ? (char *)set : (char *)clr)) {
		errorf("write data %d fail on offset %lld.\n", length,
			offset);
		return -1;
	}

	debugf("%srequire reboot edl\n", on ? " " : "no ");
	return 0;
}

int fb_check_secboot_enable(void)
{

#ifdef SPRD_SECBOOT
	unsigned int lcs = 0;
	boot_mode_t bmode;
	unsigned int *t_lcs = NULL;

	bmode = get_boot_role();
	if (BOOTLOADER_MODE_DOWNLOAD == bmode) {
		if (sprd_get_lcs(&lcs) || (5 != lcs)) {
			debugf("secboot was disabled(lcs: %d)\n", lcs);
			return -1;
		}
		debugf("secboot was enabled\n");
	} else if (BOOTLOADER_MODE_LOAD == bmode) {
		t_lcs = memalign(SZ_4K, sizeof(unsigned int));
		if (t_lcs == NULL) {
			errorf("no enough heap for set fb_check_secboot_enable\n");
			fastboot_fail("no enough heap for set fb_check_secboot_enable!");
			return;
		}

		if (get_lcs(t_lcs) || (5 != *t_lcs)) {
			debugf("secboot was disabled(t_lcs: %d)\n", *t_lcs);
			free(t_lcs);
			return -2;
		}
		debugf("secboot was enabled\n");
	}
#else
	if (0) {
	}
#endif
	else {
		debugf("unknown state\n");
		return -3;
	}

	if (t_lcs)
		free(t_lcs);
	return 0;
}

/* if it was required to reboot edl, allow enter:0 */
int fb_check_reboot_edl(void *ptr)
{
	char tmp[] = MAGIC_ENTER_EDL, buf[4] = {0};
	uint64_t offset = MISCDATA_EDL_OFFSET;
	int length = 4;

	/* if secboot was not enabled, then go ahead, sec enable:0 */
	if (fb_check_secboot_enable())
		return 0;

	if (common_raw_read("miscdata", (uint64_t)length, offset, buf)) {
		errorf("read data %d fail on offset %lld.\n", length, offset);
		return -1;
	}

	if (memcmp(tmp, buf, sizeof(buf))) {
		debugf("(%x) operation was not required by reboot-edl\n", *(int *)buf);
		return -2;
	}

	return 0;
}

void fb_cmd_reboot_edl(const char *arg, void *data, uint64_t sz)
{
	if (!fb_check_permission(TYPE_PERMISSION_REPAIR)) {
		fastboot_fail("Not allow!");
		return;
	}

	if (fb_require_reboot_edl(1)) {
		fastboot_fail("reboot edl fail\n");
		return;
	}

	fastboot_okay("");
	usb_driver_exit();
	reboot_devices(CMD_AUTODLOADER_REBOOT);
}
#else
int fb_check_secboot_enable(void)
{

#ifdef SPRD_SECBOOT
	unsigned int lcs = 0;
	boot_mode_t bmode;
	unsigned int *t_lcs = NULL;

	bmode = get_boot_role();
	if (BOOTLOADER_MODE_DOWNLOAD == bmode) {
		if (sprd_get_lcs(&lcs) || (5 != lcs)) {
			debugf("secboot was disabled(lcs: %d)\n", lcs);
			return -1;
		}
		debugf("secboot was enabled\n");
	} else if (BOOTLOADER_MODE_LOAD == bmode) {
		t_lcs = memalign(SZ_4K, sizeof(unsigned int));
		if (t_lcs == NULL) {
			errorf("no enough heap for set fb_check_secboot_enable\n");
			fastboot_fail("no enough heap for set fb_check_secboot_enable!");
			return -4;
		}

		if (get_lcs(t_lcs) || (5 != *t_lcs)) {
			debugf("secboot was disabled(t_lcs: %d)\n", *t_lcs);
			free(t_lcs);
			return -2;
		}
		debugf("secboot was enabled\n");
	}
#else
	if (0) {
	}
#endif
	else {
		debugf("unknown state\n");
		return -3;
	}

	if (t_lcs)
		free(t_lcs);
	return 0;
}
#endif

void fb_cmd_powerdown(const char *arg, void *data, uint64_t sz)
{
	fastboot_okay("");
	power_down_devices(0);

}

/*add fastboot cmd for sharkl2*/
#ifdef SPRD_SECBOOT

int wait_for_keypress(void)
{
	debugf("Enter fastboot wait_for_keypress\n");
	int key_code;
	#ifdef CONFIG_HMD_FASTBOOT
	int power_key_code = PW_KEY_NOT_PRESSED;
	#endif
	int i=0;
	do {
		if(i>=300){
			debugf("wait_for_keypress time out\n");
			key_code = -1;
			break;
		}
		udelay(50 * 1000);
		key_code = board_key_scan();
		debugf("key_code is %x\n", key_code);
		#ifdef CONFIG_HMD_FASTBOOT
		power_key_code = power_button_pressed();
		debugf("power_key_code is %x\n", power_key_code);
		if (key_code == KEY_VOLUMEDOWN || key_code == KEY_VOLUMEUP || power_key_code == PW_KEY_PRESSED) {
			if (power_key_code == PW_KEY_PRESSED) {
				key_code = power_key_code;
			}
			break;
		}
		#else
		if (key_code == KEY_VOLUMEDOWN || key_code == KEY_VOLUMEUP)
			break;
		#endif
		i++;
	} while(1);
	debugf("fastboot wait_for_keypress key_code is %x\n", key_code);
	return key_code;
}

#define  SOCID_SIZE_IN_WORDS 8
void fb_cmd_getlcs(const char *arg, void *data, uint64_t sz){
	int ret = 0;
	char s[20]={0};
	unsigned int *t_lcs = memalign(SZ_4K, sizeof(unsigned int));
	if (t_lcs == NULL) {
		errorf("no enough heap for set getlcs\n");
		fastboot_fail("no enough heap for set getlcs!");
		return;
	}
	ret = get_lcs(t_lcs);

	if(ret!=0){
		snprintf(s, 20, " ret is %04x", ret);
		fastboot_fail(s);
	}else{
		snprintf(s, 20, "lcs is %d\n", *t_lcs);
		fastboot_okay(s);
	}

	free(t_lcs);
}

void fb_cmd_setrma(const char *arg, void *data, uint64_t sz){
	int ret = 0;
	lcd_printf(
		"\n\n------------------------------------------------------------------------\n"
		"   Warning:RMA will change hardware status and unrecoverable, also will make some secure data unaccessible!\n"
		"   Do you confirm that you are developer and need this operation really?\n"
		"   Press button 'Volumn-Down' for 'No'.\n"
		"   Press button 'Volumn-Up' for 'Yes'.\n"
		"   The operation will be cancelled automatically after 20 seconds.\n"
		"------------------------------------------------------------------------\n");
	int key_code = wait_for_keypress();
	if (key_code == KEY_VOLUMEDOWN) {
		fastboot_fail("user cancelled");
	} else if (key_code == KEY_VOLUMEUP) {
		ret = set_rma();
		char s[20]={0};
		if(ret!=0){
			snprintf(s, 20, " ret is %04x", ret);
			fastboot_fail(s);
		}else{
			fastboot_okay("");
		}
	} else if (key_code == -1) {
		fastboot_fail("time out");
	}
	lcd_clear();
	lcd_splash("logo");
	lcd_printf("\n   fastboot mode");
}

void response_secure_data(char *name, int status, uint8_t *data) {
	int i;
	char s[33] = {0};

	if (status != 0) {
		snprintf(s, 33, " failed status is %04x", status);
		fastboot_fail(s);
		return;
	} else {
		for(i = 0; i < 32; i++){
			if ((i+1) % 16 == 0) {
				snprintf(s + (i%16)*2, 4, "%02x\n", data[i]);
				fastboot_response_data(name, s);
			} else {
				snprintf(s + (i%16)*2, 3, "%02x", data[i]);
			}
		}
	}
	fastboot_okay("");
}

void fb_cmd_getsocid(const char *arg, void *data, uint64_t sz){
	int ret = 0;
	char s[33] = {0};
	uint8_t *socIdBuff = memalign(SZ_4K, 32);

	if (socIdBuff == NULL) {
		errorf("no enough heap for set socIdBuff\n");
		fastboot_fail("no enough heap for set socIdBuff!");
		return;
	}
	ret = get_socid(socIdBuff, 32);
	fastboot_response_data("SOCID", "socid is:\n");
	response_secure_data("socid is:\n", ret, socIdBuff);

	free(socIdBuff);
}

void fb_cmd_get_rotpk0(const char *arg, void *data, uint64_t sz) {
	int ret = 0;
	uint8_t *rotpk_buff = memalign(SZ_4K, 32);

	if (rotpk_buff == NULL) {
		errorf("no enough heap for set rotpk_buff\n");
		fastboot_fail("no enough heap for set rotpk_buff!");
		return;
	}

	ret = get_rotpk0((uint64_t)rotpk_buff, 32);
	fastboot_response_data("ROTPK0", "rotpk0 is:\n");
	response_secure_data("rotpk0 is:\n", ret, rotpk_buff);

	free(rotpk_buff);
}

void fb_cmd_get_rotpk1(const char *arg, void *data, uint64_t sz) {
	int ret = 0;
	uint8_t *rotpk_buff = memalign(SZ_4K, 32);

	if (rotpk_buff == NULL) {
		errorf("no enough heap for set rotpk_buff\n");
		fastboot_fail("no enough heap for set rotpk_buff!");
		return;
	}

	ret = get_rotpk1((uint64_t)rotpk_buff, 32);
	fastboot_response_data("ROTPK1", "rotpk1 is:\n");
	response_secure_data("rotpk1 is:\n", ret, rotpk_buff);

	free(rotpk_buff);
}

void fb_cmd_check_kce_status(const char *arg, void *data, uint64_t sz) {
	int kce_status = 0;
	char s[34] = {0};

	kce_status = check_kce_status();

	if (kce_status == 0) {
		snprintf(s, 34, "kce is locked, kce_status = %d\n", kce_status);
		fastboot_okay(s);
	} else {
		errorf("kce not lock, kce_status = %d\n", kce_status);
		snprintf(s, 34, "kce not lock, kce_status = %d\n", kce_status);
		fastboot_fail(s);
	}
}

void fb_cmd_get_vboot_imgversion(const char *arg, void *data, uint64_t sz) {
	int i;
	char s[50] = {0};
	char slot[2] = {'a', 'b'};
	VbootVerInfo vboot_ver_info __attribute__((aligned(4096))); /*must be PAGE ALIGNED*/

	for (i = 0; i < 2; i++) {
		memset(&vboot_ver_info, 0, sizeof(VbootVerInfo));
		vboot_ver_info.ab_slot_flag = i;
		if (sprd_get_all_imgversion(&vboot_ver_info)) {
			errorf("slot_%c get rpmb image version failed.\n", slot[i]);
			snprintf(s, 50, "\nslot_%c get rpmb image version failed.\n", slot[i]);
			fastboot_fail(s);
			return;
		}
		snprintf(s, 50, "\nslot_%c get rpmb image version success: \n", slot[i]);
		fastboot_response_data("", s);
		for (int index = 0; index < AVB_MAX_NUMBER_OF_ROLLBACK_INDEX_LOCATIONS; index++) {
			snprintf(s, 50, "Rollback Index Location [%02d] : version is 0x%x\n",
				index, vboot_ver_info.img_ver[index]);
			fastboot_response_data("", s);
		}
	}
	fastboot_okay("");
}

int cal_secure_efuse_version(secure_version_info *ver_info, uint32_t slot) {
	char s[40] = {0};
	uint32_t version_count = 0;
	uint64_t swVersion64 = 0;
	uint32_t swVersion32 = 0;

	if (EFUSE_VERSION_64_BIT == ver_info->version_type) {
		swVersion64 = ((uint64_t)ver_info->swVersion[1] << 32) | \
				(uint64_t)(ver_info->swVersion[0]);
		while (swVersion64) {
			if ((swVersion64 & 0x1) == 0) {
				errorf("\nswVersion64 is 0x%llx\n", swVersion64);
				return -1;
			}
			version_count ++;
			swVersion64 = swVersion64 >> 1;
		}
	} else {
		swVersion32 = ver_info->swVersion[slot];
		while (swVersion32) {
			if ((swVersion32 & 0x1) == 0) {
				errorf("\nswVersion32 is 0x%x\n", swVersion32);
				return -1;
			}
			version_count ++;
			swVersion32 = swVersion32 >> 1;
		}
	}

	snprintf(s, 40, "secure efuse version is: %d.\n", version_count);
	fastboot_response_data("", s);
	return 0;
}

void fb_cmd_get_secure_version(const char *arg, void *data, uint64_t sz) {
	secure_version_info ver_info __attribute__((aligned(4096)));

	memset(&ver_info, 0, sizeof(ver_info));
	if (get_secure_version(&ver_info)) {
		fastboot_fail("fastboot get secure version failed.");
		return;
	}
	if (SPL_DOUBLE_SLOT == ver_info.ab_slot_flag) {
		fastboot_response_data("", "\nCurrent device has double spl slot.\n");
		fastboot_response_data("", "slot_a ");
		if (cal_secure_efuse_version(&ver_info, SLOT_A_EFUSE_VERSION)) {
			goto err;
		}
		fastboot_response_data("", "slot_b ");
		if (cal_secure_efuse_version(&ver_info, SLOT_B_EFUSE_VERSION)) {
			goto err;
		}
	} else {
		fastboot_response_data("", "\nCurrent device has single spl slot.\n");
		if (cal_secure_efuse_version(&ver_info, SLOT_A_EFUSE_VERSION)) {
			goto err;
		}
	}
	fastboot_okay("");
	return;
err:
	fastboot_fail("incorrect efuse version!");
	return;
}

void fb_cmd_get_secure_enable(const char *arg, void *data, uint64_t sz) {
	char s[32] = {0};
	unsigned int secure_enable = 0;

	secure_enable = is_secure_boot_enable();
	if (secure_enable) {
		snprintf(s, 32, "secure bit has been enabled.\n");
		fastboot_okay(s);
	} else {
		snprintf(s, 32, "secure bit is not enable.\n");
		fastboot_fail(s);
	}
}

void fb_cmd_get_secdebug_bit(const char *arg, void *data, uint64_t sz) {
	int ret = 0;
	char s[50] = {0};
	unsigned int *secdebug_bit = memalign(SZ_4K, sizeof(unsigned int));
	if (secdebug_bit == NULL) {
		errorf("no enough heap for secdebug_bit\n");
		fastboot_fail("no enough heap for secdebug_bit!");
		return;
	}

	ret = get_secdebug_bit(secdebug_bit);
	if (ret != 0) {
		snprintf(s, 50, "failed to get secdebug bit, ret is %04x", ret);
		fastboot_fail(s);
		free(secdebug_bit);
		return;
	}
	if (*secdebug_bit) {
		snprintf(s, 50, "secure debug bit has been deployed\n");
		fastboot_okay(s);
	} else {
		snprintf(s, 50, "secure debug bit not deployed.\n");
		fastboot_okay(s);
	}
	free(secdebug_bit);
}
#endif

void fb_cmd_set_active(const char *arg, void *data, uint64_t sz)
{
	struct bootloader_control abc;
	disk_partition_t part_info;
	block_dev_desc_t *dev;
	ulong abc_offset, abc_size;
	int ret, slot;

	memset(&abc, 0, sizeof(struct bootloader_control));
	memset(&part_info, 0, sizeof(disk_partition_t));

	abc_offset = offsetof(struct bootloader_message_ab, slot_suffix);
	abc_size = sizeof(struct bootloader_control);

	ret = common_raw_read("misc", (u64)abc_size, (u64)abc_offset, (char *)&abc);
	if (ret < 0) {
		errorf("ANDROID: Could not read from boot ctrl partition\n");
		fastboot_fail("Could not read from boot ctrl partition");
		return;
	}

	if (abc.slot_suffix[1] != *arg) {
		slot = abc.slot_suffix[1] - 'a';
		abc.slot_info[slot].priority = 14;
		abc.slot_info[slot].tries_remaining = 1;
		abc.slot_suffix[1] = *arg;
		slot = *arg - 'a';
		abc.slot_info[slot].priority = 15;
		abc.slot_info[slot].tries_remaining = 6;
		abc.slot_info[slot].successful_boot = 0;

		abc.crc32_le = crc32(0, (void *)&abc, offsetof(typeof(abc), crc32_le));
		ret = common_raw_write("misc", (u64)abc_size, 0, (u64)abc_offset, (char *)&abc);
		if(ret) {
			errorf("ANDROID: Could not write to boot ctrl partition\n");
			fastboot_fail("Could not write from boot ctrl partition");
			return;
		}
	}

	fastboot_okay("");
	//modify by hyinfeng for CMT-2253 Failed to clear data flash using DeviceKit upgrade SW after FOTA/GOTA begin
	//usb_driver_exit();

	//reboot_devices(CMD_NORMAL_MODE);
	//modify by hyinfeng for CMT-2253 Failed to clear data flash using DeviceKit upgrade SW after FOTA/GOTA end
	return;
}

#ifdef CONFIG_WR_SPARSE
static void handle_partition_attribute(char *buffer)
{
	char *ptr = NULL;

	if (!buffer)
		return;

	ptr = strstr(buffer, PART_NAME);
	if (ptr && (ptr[PART_NAME_LEN] == ':')) {
		ptr += PART_NAME_LEN + 1;
		if (ptr[0] != '\0') {
			strncpy(ImageInfo.part_name, ptr, PARTNAME_SZ - 1);
			dprintf(INFO,"====== get is-logical [%s] ==== \n", ImageInfo.part_name);
		}
	} else if ((ptr = strstr(buffer, PART_TYPE)) != NULL) {
		/* try partition-type */
		if (ptr[PART_TYPE_LEN] == ':') {
			ptr += PART_TYPE_LEN + 1;
			if (ptr[0] != '\0') {
				strncpy(ImageInfo.part_name, ptr, PARTNAME_SZ - 1);
				dprintf(INFO,"====== get partition-type [%s] ==== \n", ImageInfo.part_name);
			}
		}
	}
	return;
}
#endif

#ifdef CONFIG_FASTBOOT_KEYPRESS_SCAN
int fastboot_wait_for_keypress(void *arg)
{
	printf("Enter fastboot ==> wait for keypress_ thread\n");
	int key_code;
	static int count = 0;
	uchar *buf = NULL;

	while(1) {
		/* 1.test for malloc and free */
		buf = malloc(1024);
		if (!buf) {
			errorf("No enough memory for malloc test!\n");
			return -1;
		}
		free(buf);
		/* 2.test for keypress scan */
		key_code = board_key_scan();
		if (key_code == KEY_VOLUMEDOWN) {
			printf("Volumedown key press, enter fastboot mode!!!\n");
			usb_driver_exit();
			reboot_devices(CMD_FASTBOOT_MODE);
			while(1);
		} else if (key_code == KEY_VOLUMEUP) {
			printf("Volumeup key press, enter normal mode!!!\n");
			usb_driver_exit();
			reboot_devices(CMD_NORMAL_MODE);
			while(1);
		}
		count ++;
		printf("fastboot_wait_for_keypress will sleep %d!\n", count);
		thread_sleep(60);
	}
	return 0;
}
#endif

static void fastboot_command_loop(void)
{
	struct fastboot_cmd *cmd;
	int r;
	char *pre_cmd;
	char *cmd_parameter;

	debugf("fastboot: processing commands\n");
#ifdef CONFIG_FASTBOOT_KEYPRESS_SCAN
	thread_t *td = thread_create("key_press", &fastboot_wait_for_keypress, NULL, HIGH_PRIORITY, DEFAULT_STACK_SIZE);
	thread_set_real_time(td);
	if(td == NULL)
		errorf("fastboot failed to thread_create\n");
	thread_detach_and_resume(td);
#endif

again:
	while (fastboot_state != STATE_ERROR) {
		// ning.wei@hmd++ for fastboot ui display sync from solo begin
		key_listener_premssion=1;
		// ning.wei@hmd++ for fastboot ui display sync from solo end
		memset(buffer, 0, 64);
		r = fb_usb_read(buffer, 64);
		if (r < 0) {
			if (-ESHUTDOWN ==  r) {
				/* disconnect usb  */
				debugf("fastboot exit\r\n");
				usb_fastboot_exit();
			}
			break;
		}

		buffer[r] = 0;
		debugf("fastboot: %s, r:%d\n", buffer, r);
		if (strchr(buffer, ':'))
			pre_cmd = strtok_r(buffer, ":", &cmd_parameter);
		else
			pre_cmd = strtok_r(buffer, " ", &cmd_parameter);
		for (cmd = cmdlist; cmd; cmd = cmd->next) {
			debugf("cmd->prefix: %s, cmd->prefix_len:%d\n", cmd->prefix, cmd->prefix_len);
			if (strcmp(pre_cmd, cmd->prefix))
				continue;
			debugf("Receive cmd from host :%s, cmd_parameter:%s\n", cmd->prefix, cmd_parameter);
			write_log();

#ifdef CONFIG_WR_SPARSE
			handle_partition_attribute(cmd_parameter);
#endif
			fastboot_state = STATE_COMMAND;
			// ning.wei@hmd++ for fastboot ui display sync from solo begin
			key_listener_premssion=0;
			// ning.wei@hmd++ for fastboot ui display sync from solo end

#ifdef CONFIG_HMD_FASTBOOT			
			if( !fastboot_mode_cmd_filter(cmd->prefix, cmd_parameter) || auth_flag) {
				cmd->handle((const char *)cmd_parameter, ImageInfo.base_address, ImageInfo.data_size);
            } else {
				debugf("permission denied, auth needed.\n");						
				fastboot_fail("permission denied, auth needed.");
            }
#else
			cmd->handle((const char *)cmd_parameter, ImageInfo.base_address, ImageInfo.data_size);
#endif
			if (fastboot_state == STATE_COMMAND)
				fastboot_fail("unknown reason");
			goto again;
		}

		fastboot_fail("unknown command");
	}
	fastboot_state = STATE_OFFLINE;
	debugf("fastboot: oops!\n");
	debugf("wait usb cable connect\n");
	/* wait usb connected  */
	while (fastboot_state == STATE_OFFLINE) {
		if (fastboot_charger_connected()) {
			debugf("fastboot re-init\r\n");
			r = usb_fastboot_init();
			if (r < 0) {
				debugf("usb re-init err\r\n");
				usb_fastboot_exit();
			}
			reset_sparse_status();
#ifdef CONFIG_WR_SPARSE
			wr_sparse_prepare(ImageInfo.base_address, &ImageInfo.max_size);
#endif
			goto again;
		}
	}
	write_log();
}

static int fastboot_handler(void *arg)
{
	for (;;) {
		debugf("[%s]\n",__func__);
		fastboot_command_loop();
	}
	return 0;
}

//[HMDEnterpriseService] for error code showing begin 2025-03-04
extern void console_setfgcolor(int);
extern void console_setbgcolor(int);
#ifdef ZCFG_HMD_ENTERPRISE_API
void printErrorCode(void) {
    char error_code[8];
    memset(error_code, 0, 8);
    if(is_hmd_error_code_set(error_code)){
	    
        console_setfgcolor(SPRD_CONSOLE_COLOR_WHITE2);
        console_setbgcolor(SPRD_CONSOLE_COLOR_BLACK2);
        lcd_position_cursor(0, ERROR_CODE_POS);

        lcd_printf("      Error: %s \n", error_code);

        lcd_position_cursor(0, 0);
        console_setfgcolor(SPRD_CONSOLE_COLOR_BLACK2);
        console_setbgcolor(SPRD_CONSOLE_COLOR_WHITE2);
    }
}
#endif
//[HMDEnterpriseService] for error code showing end 2025-03-04
int do_fastboot(void)
{
	char max_download_size[64];
	int ret = 0;
	dprintf(ALWAYS, "start fastboot\n");
	// ning.wei@hmd++ for fastboot ui display sync from solo begin
	logo_display(6, BACKLIGHT_ON, LCD_DISPLAY_ENABLE);
	printinfo();
    // ning.wei@hmd++ for fastboot ui display sync from solo end
	//[HMDEnterpriseService] for error code showing begin 2025-03-04
#ifdef ZCFG_HMD_ENTERPRISE_API
    printErrorCode();
#endif
    //[HMDEnterpriseService] for error code showing end 2025-03-04
	write_log();
#ifdef SPRD_SECBOOT
	/*Initialize product_sn_token to 0 */
	memset(product_sn_token, 0, PRODUCT_SN_TOKEN_MAX_SIZE);
#endif

	ret = usb_fastboot_init();

	if (ret < 0)
		return ret;

	ret = set_fastboot_buf_base_size();
	if (ret < 0)
		return ret;

	ImageInfo.base_address = g_FbBuf;
	ImageInfo.max_size = g_FbBuf_size;

	sprintf(max_download_size, "0x%llx", ImageInfo.max_size);
	ImageInfo.data_size = 0;

#ifdef CONFIG_WR_SPARSE
	ImageInfo.max_size_raw = ImageInfo.max_size;
	wr_sparse_prepare(ImageInfo.base_address, &ImageInfo.max_size);
	sprintf(max_download_size, "0x%llx", ImageInfo.max_size);
#endif

	fastboot_register("getvar", fb_cmd_getvar);
	/*when you input cmd"flash" in host, we will rcv cmd"download" first,then the "flash",
	   so even if we can't see cmd"download" in host fastboot cmd list,it is also used */
	fastboot_register("download", fb_cmd_download);
	fastboot_publish("version", "1.0");

	/* Add new command for AndroidQ */
	fastboot_publish("is-userspace", "no");
	fastboot_publish("max-download-size", max_download_size);
	fastboot_register("flash", fb_cmd_flash);
	fastboot_register("erase", fb_cmd_erase);
	fastboot_register("boot", fb_cmd_continue);
	fastboot_register("reboot", fb_cmd_reboot);
	fastboot_register("powerdown", fb_cmd_powerdown);
	fastboot_register("continue", fb_cmd_continue);
	fastboot_register("reboot-bootloader", fb_cmd_reboot_bootloader);
	fastboot_register("reboot-fastboot", fb_cmd_reboot_userspace_fastboot);
	fastboot_register("reboot-recovery", fb_cmd_reboot_recovery);
	fastboot_register("set_active", fb_cmd_set_active);

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
	fastboot_register("reboot-emergency", fb_cmd_reboot_edl);
	fastboot_register("upload", fb_cmd_upload);
#endif

	/* Add new command for Unisoc Sysdump */
	fastboot_register("setdump", fb_cmd_setdump);
	fastboot_register("getdump", fb_cmd_getdump);

	/* For oem cmd list */
	fastboot_register("oem", fb_cmd_oem);

#ifdef SPRD_SECBOOT
	fastboot_register("flashing", fb_cmd_flashing);
	fastboot_register("getlcs", fb_cmd_getlcs);
	fastboot_register("getsocid", fb_cmd_getsocid);
	fastboot_token_init();
#endif
	#ifdef CONFIG_HMD_FASTBOOT
	oem_fastboot_register_commands();
	#endif
	fastboot_handler(0);

	return 0;

}

// Add by changmei.chen for hw-anti-rollback version 20241211 begin
int fb_oem_booargs_get_eps_state(char *buf, int len)
{
    int c = 0;
    if (fb_check_secboot_enable()) {
        c += snprintf(buf + c, len - c - 1, "%s=0\n", ANDROIDBOOT_EPS_STATE);
    }else{
        c += snprintf(buf + c, len - c - 1, "%s=1\n", ANDROIDBOOT_EPS_STATE);
    }

    return c;
}
// Add by changmei.chen for hw-anti-rollback version 20241211 end

#ifdef CONFIG_HMD_FASTBOOT
void fb_cmd_upload(const char *arg, void *data, uint64_t sz)
{
	int i;
	char upload_ack[10] = {0};

	sprintf(upload_ack,"0x%x",upload_len);

	fastboot_ack("DATA", upload_ack);

	fb_usb_write(upload_buf, upload_len);
#if 1
	printf("fb_cmd_upload  lens = %d,the encrypt data: ",upload_len);
	for(i = 0; i < upload_len; i++)
	{
		printf("%c",upload_buf[i]);
	}
	printf("\n");
#endif
	fastboot_state = STATE_COMMAND;
	fastboot_okay("");
	fastboot_state = STATE_COMPLETE;
}

extern int hmd_rsa_decrypt_data(unsigned char *data_type, unsigned char *revice_data, unsigned char *sn_data, int fdl);
void fb_cmd_oem_permission(const char *arg, void *data, uint64_t sz)
{
	int i,cnt;
	int ret = 0;
	char flag = 0;
	char subcmd[64];
	const char *delim = " ";
	char response[30] = {0};
	unsigned char *data_buf = ImageInfo.base_address;
	struct mode_key_table table[] = {
		{ (char *)"flash", MODE_FASTBOOT_FLASH   },
		{ (char *)"repair", MODE_FASTBOOT_REPAIR  },
		{ (char *)"simlock", MODE_FASTBOOT_SIMLOCK },
		{ (char *)"factory", MODE_FASTBOOT_FACTORY },
	};
	
	memset(subcmd, 0, sizeof(subcmd));
	strcpy(subcmd, strtok(arg, delim));

	cnt = sizeof(table) / sizeof(table[0]);
	for(i = 0; i < cnt ; i++)
	{
		if(!strcmp((char *)subcmd, table[i].mode)){
			strcpy(product_sn_token, get_product_sn());
			ret = hmd_rsa_decrypt_data(table[i].mode, data_buf, product_sn_token, 0);
			if(ret == 0){
				sprintf(response,"%s decrypt failed",table[i].mode);
				fastboot_fail(response);
				return;
			}else{
				flag = 1;
				break;
			}
		}
	}
	if(flag == 0)
	{
		fastboot_fail("");
		return;
	}
	if (i >= cnt) {
	  return;
	}

	sprintf(response,"switch to %s mode successful.",table[i].mode);
	fastboot_response_info(response);

	fastboot_mode_flag = table[i].mode_flag;
	fastboot_okay("");
}

void fb_cmd_reboot_edl(const char *arg, void *data, uint64_t sz)
{
	char flag[1] = {1};
#ifdef XX_FINAL_RELEASE
	unsigned char fastboot_reboot_edl[2] = {0};
	fastboot_reboot_edl[0] = 1;

	oem_repair_write_mmc_ex("fastboot_reboot_edl", fastboot_reboot_edl);
#endif

	fastboot_okay("");
	udelay(500);
	usb_driver_exit();
	printf("fb_cmd_reboot_edl CMD_AUTODLOADER_REBOOT \n");
	//#ifdef CONFIG_HMD_ONE_IMAGE
	//if (common_raw_write("miscdata", (uint64_t)MISCDATA_HMD_FASTBOOT_SALE_DOWNLOAD_MODE_DATA_LEN, (uint64_t)0, MISCDATA_HMD_FASTBOOT_SALE_DOWNLOAD_MODE,flag)) {
	//	errorf("write miscdata data %d fail on offset %lld.\n", MISCDATA_HMD_FASTBOOT_SALE_DOWNLOAD_MODE_DATA_LEN, MISCDATA_HMD_FASTBOOT_SALE_DOWNLOAD_MODE);
	//}
	//#endif
	//add by hyinfeng for CMT-275,NYX-274 fdl verify repair or factory permission begin
	if(MODE_FASTBOOT_FACTORY == fastboot_mode_flag || MODE_FASTBOOT_REPAIR == fastboot_mode_flag) {
		debugf("mode %d permission data length:%d\n", fastboot_mode_flag, ImageInfo.data_size);
		char miscdata_buf[MISCDATA_FDL_VERIFY_DATA_LEN] = {0};
		memcpy(miscdata_buf, ImageInfo.base_address, (MISCDATA_FDL_VERIFY_DATA_LEN > ImageInfo.data_size)?ImageInfo.data_size:MISCDATA_FDL_VERIFY_DATA_LEN);
		if (common_raw_write("miscdata", (uint64_t)MISCDATA_FDL_VERIFY_DATA_LEN, (uint64_t)0, MISCDATA_FDL_VERIFY_BASE, miscdata_buf)) {
			errorf("write miscdata data %d fail on offset %lld.\n", MISCDATA_FDL_VERIFY_DATA_LEN, MISCDATA_FDL_VERIFY_BASE);
		}
		#if 0
		for(int i = 0; i < ImageInfo.data_size; i++) {
			debugf("%c", ImageInfo.base_address[i]);
		}
		debugf("\n");
		#endif
	}
	//add by hyinfeng for CMT-275,NYX-274 fdl verify repair or factory permission end
	
	reboot_devices(CMD_AUTODLOADER_REBOOT);
		
}
#endif
