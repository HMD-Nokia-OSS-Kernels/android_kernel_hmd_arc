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

#include "dl_common.h"
#include <malloc.h>
#include <asm/arch/chip_releted_def.h>
#include "dl_operate.h"
#include <part.h>
#include <sparse_format.h>
#include <sprd_common_rw.h>
#include "boot_parse.h"
#include <uuid.h>
#include <android_ab.h>
#include <mmc.h>
#include <linux/kernel.h>
#include <secureboot/sec_common.h>
#include "boot_mode.h"
#include <miscdata_def.h>
#include <lk_sec_drv.h>
#ifndef CONFIG_ZEBU
#include <sprd_crypto_special.h>
#endif
#include <arch/sprd_cache.h>
#ifdef CONFIG_UFS
#include <sprd_ufs.h>
#endif
#ifdef CONFIG_WR_SPARSE
#include <fb_sparse.h>
#endif
#ifdef SPRD_SPARSE_SUPER_SPEEDUP
#include <sparse_format.h>

uint64_t first_emmc = 1;//emmc first write
uint64_t chunk_temp_offset = 0;//recore the length that not align to blksz

//#define DOWNLOAD_DEBUG 1
#ifdef DOWNLOAD_DEBUG
#define prt(fmt, args...) do { dprintf(INFO,"sparse_download %s(): ", __func__);dprintf(INFO,fmt, ##args); } while (0)
#else
#define prt(fmt, args...)
#endif

extern int sparse_download_process(uint32_t size, char *buf);
#endif

#define INVALID_ID   0xffff
#define PRINT_NV_DATA 0
#define BACKUPNV_CURRENT_SLOT 0x31
#define TOOL_FALG_OFFSET 0x23

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
extern int fb_require_reboot_edl(int on);
extern int fb_check_reboot_edl(void *ptr);
#endif

static DL_EMMC_FILE_STATUS g_status;
static DL_EMMC_STATUS g_dl_eMMCStatus;

static unsigned long g_checksum;
static unsigned long g_sram_addr;
static unsigned int g_download_part_count = 0;
#ifdef NV_PROTECT_BACKUP
static int g_nv_erase_flag = 0;
static int g_nv_read_flag = 0;
static int g_nv_read_succ_flag = 0;
static int g_nv_read_back_len = 0;
unsigned long g_nv_read_back = FB_NV_ADDR;
#endif

extern unsigned int fastboot_image_size;
extern boot_device_t get_bootdevice(void);

# ifndef CONFIG_DTS_MEM_LAYOUT
#ifdef CONFIG_X86
#ifdef CONFIG_SPRD_SOC_SP9853I
unsigned char *g_eMMCBuf = (unsigned char *)0x00800000;
#else
unsigned char *g_eMMCBuf = (unsigned char *)0x35000000;
#endif
#else
unsigned char *g_eMMCBuf = (unsigned char *)0x82000000;
#endif
uint64_t  emmc_buf_size = SZ_256M;

# else
unsigned char *g_eMMCBuf = NULL;
uint64_t  emmc_buf_size = 0;
# endif

/**
	partitions not for raw data or normal usage(e.g. nv and prodinfo) should config here.
	partitions not list here mean raw data/normal usage.
*/
SPECIAL_PARTITION_CFG const s_special_partition_cfg[] = {
	{"fixnv1", "fixnv2", IMG_RAW, PARTITION_PURPOSE_NV},
	{"runtimenv1", "runtimenv2", IMG_RAW, PARTITION_PURPOSE_NV},
	{"tdfixnv1", "tdfixnv2", IMG_RAW, PARTITION_PURPOSE_NV},
	{"tdruntimenv1", "tdruntimenv2", IMG_RAW, PARTITION_PURPOSE_NV},
	{"g_fixnv1", "g_fixnv2", IMG_RAW, PARTITION_PURPOSE_NV},
	{"g_runtimenv1", "g_runtimenv2", IMG_RAW, PARTITION_PURPOSE_NV},
	{"nr_fixnv1", "nr_fixnv2", IMG_RAW, PARTITION_PURPOSE_NV},
#ifdef CONFIG_ANDROID_AB
	{"nr_fixnv1_a", "nr_fixnv2_a", IMG_RAW, PARTITION_PURPOSE_NV},
	{"nr_fixnv1_b", "nr_fixnv2_b", IMG_RAW, PARTITION_PURPOSE_NV},
#endif
	{"nr_runtimenv1", "nr_runtimenv2", IMG_RAW, PARTITION_PURPOSE_NV},
	{"l_fixnv1", "l_fixnv2", IMG_RAW, PARTITION_PURPOSE_NV},
#ifdef CONFIG_ANDROID_AB
	{"l_fixnv1_a", "l_fixnv2_a", IMG_RAW, PARTITION_PURPOSE_NV},
	{"l_fixnv1_b", "l_fixnv2_b", IMG_RAW, PARTITION_PURPOSE_NV},
#endif
	{"l_runtimenv1", "l_runtimenv2", IMG_RAW, PARTITION_PURPOSE_NV},
	{"tl_fixnv1", "tl_fixnv2", IMG_RAW, PARTITION_PURPOSE_NV},
	{"tl_runtimenv1", "tl_runtimenv2", IMG_RAW, PARTITION_PURPOSE_NV},
	{"lf_fixnv1", "lf_fixnv2", IMG_RAW, PARTITION_PURPOSE_NV},
	{"lf_runtimenv1", "lf_runtimenv2", IMG_RAW, PARTITION_PURPOSE_NV},
	{"wfixnv1", "wfixnv2", IMG_RAW, PARTITION_PURPOSE_NV},
	{"wruntimenv1", "wruntimenv2", IMG_RAW, PARTITION_PURPOSE_NV},
	{"w_fixnv1", "w_fixnv2", IMG_RAW, PARTITION_PURPOSE_NV},
#ifdef CONFIG_ANDROID_AB
	{"w_fixnv1_a", "w_fixnv2_a", IMG_RAW, PARTITION_PURPOSE_NV},
	{"w_fixnv1_b", "w_fixnv2_b", IMG_RAW, PARTITION_PURPOSE_NV},
#endif
	{"w_runtimenv1", "w_runtimenv2", IMG_RAW, PARTITION_PURPOSE_NV},
	{"wl_fixnv1", "wl_fixnv2", IMG_RAW, PARTITION_PURPOSE_NV},
#ifdef CONFIG_ANDROID_AB
	{"wl_fixnv1_a", "wl_fixnv2_a", IMG_RAW, PARTITION_PURPOSE_NV},
	{"wl_fixnv1_b", "wl_fixnv2_b", IMG_RAW, PARTITION_PURPOSE_NV},
#endif
	{"wl_runtimenv1", "wl_runtimenv2", IMG_RAW, PARTITION_PURPOSE_NV},
	{"wcnfixnv1", "wcnfixnv2", IMG_RAW, PARTITION_PURPOSE_NV},
	{"wcnruntimenv1", "wcnruntimenv2", IMG_RAW, PARTITION_PURPOSE_NV},
	{"uboot", "uboot_bak", IMG_RAW, PARTITION_PURPOSE_NORMAL},
	{"system", NULL, IMG_RAW, PARTITION_PURPOSE_NORMAL},
	{"userdata", NULL, IMG_WITH_SPARSE, PARTITION_PURPOSE_NORMAL},
	{"cache", NULL, IMG_WITH_SPARSE, PARTITION_PURPOSE_NORMAL},
	{"prodnv", NULL, IMG_RAW, PARTITION_PURPOSE_NORMAL},
	{"splloader", "splloader_bak", IMG_RAW, PARTITION_PURPOSE_NORMAL},
	{NULL, NULL, IMG_TYPE_MAX, PARTITION_PURPOSE_MAX}
};

uint8_t is_fill_flash = 0;
#ifdef CONFIG_DL_SKIP_PARTITION
//for skip partition (demo code)
SPECIAL_PARTITION_SKIP const special_partition_cfg[] = {
	{"teecfg_a", 1},
	{"trustos_a", 1},
	{NULL, NULL}
};
#endif

enum {
	PARTITION_CONFIG_EXIST = 0x1,
	GPT_HEADER_EXIST = 0x2,
	RPMB_KEY_EXIST = 0x4,
};

ALTER_BUFFER_ATTR alter_buffer1;
ALTER_BUFFER_ATTR alter_buffer2;
ALTER_BUFFER_ATTR* current_buffer;

#ifdef SPRD_SPARSE_SUPER_SPEEDUP
ALTER_BUFFER_ATTR emmc_buffer1;
ALTER_BUFFER_ATTR emmc_buffer2;
ALTER_BUFFER_ATTR* emmc_cur_buf;
ALTER_BUFFER_ATTR* sparse2emmc_buf = NULL;
#endif

#ifdef NV_MULTICORES
#define CORENUM_OFFSET 10
#define CORENAME_LEN 16
#define MAX_CORENUM 4

typedef struct _CORE_INFO {
    uint8_t core_name[CORENAME_LEN];
    uint32_t core_index;
    uint32_t core_size;
    uint32_t core_offset;
} runnv_coreinfo_t;
typedef struct _RUNNV_HEADER {
    uint32_t magic;
    uint32_t timestamp;
    uint16_t minid;
    uint16_t maxid;
    uint16_t totalSector;
    uint16_t bytesPerSector;
    uint16_t dirCount;
    uint16_t dirSize;
    uint32_t next_offset;
    uint8_t backup_npb;
    uint8_t backup_dir;
    uint16_t reserved;
    runnv_coreinfo_t core_info[MAX_CORENUM];
} runnv_header_t;
#else
typedef struct _RUNNV_HEADER {
    uint32_t magic;
    uint32_t timestamp;
    uint16_t minid;
    uint16_t maxid;
    uint16_t totalSector;
    uint16_t bytesPerSector;
    uint16_t dirCount;
    uint16_t dirSize;
    uint32_t nextBlock;
    uint32_t nextDataOffset;
} runnv_header_t;
#endif
typedef struct _DIR_STRUCT{
    uint16_t itemSize;
    uint16_t checksum;
    uint32_t offset;
    uint32_t status;
    uint32_t reserved;
} nv_dir_t;

extern BOOLEAN mergeItem(uint8_t * oldBuf, uint32_t oldNVlength, uint8_t * newBuf, uint32_t newNVlength);
int32_t _read_repair_nv_img(const char * partition_name, uchar * buf, uint32_t image_size, uint16_t flag);

/***********************************************************/
#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
static int dl_check_reboot_edl(void)
{
	static int by = 0;

	if (by)
		return 0;

	/*
	 * operation was not allow when requirement was not from edl,
	 * or factory download
	 */
	if (fb_check_reboot_edl(NULL)) {
		return -1; /* return BSL_REP_OPERATION_FAILED to pctool */
	}

	by = 1;
	return 0;
}
#endif


//#ifdef CONFIG_DTS_MEM_LAYOUT

// #define SET_DOWNLOAD_BUFFER_BASE_SIZE(basep, sizep)		get_buffer_base_size_from_dt("heap@5", basep, sizep)
// #define SET_ALT_BUFFER1_BASE_SIZE(basep, sizep)			get_buffer_base_size_from_dt("heap@6", basep, sizep)
// #define SET_ALT_BUFFER2_BASE_SIZE(basep, sizep)			get_buffer_base_size_from_dt("heap@7", basep, sizep)

int get_ab_partition(char *temp)
{
#ifdef CONFIG_ANDROID_AB
	int cnt;

	if (!temp)
		return -1;

	strcpy(temp, g_dl_eMMCStatus.curUserPartitionName);
	cnt = strlen(temp);
	if ((*(temp + cnt - 1) == 'a') && (*(temp + cnt - 2) == '_')) {
		*(temp + cnt -1) = 'b';
		debugf("get partition slot: %s\n", temp);
		return 0;
	} else if ((*(temp + cnt - 1) == 'b') && (*(temp + cnt - 2) == '_')) {
		*(temp + cnt -1) = 'a';
		debugf("get partition slot: %s\n", temp);
		return 0;
	}
#endif
	return 1;
}

int get_special_partition_size(void)
{
	int num = 0;
	num = sizeof(s_special_partition_cfg)/sizeof(SPECIAL_PARTITION_CFG);
	return (num - 1);
}

int set_buf_base_size(void)
{
	unsigned long buf_base, alt_buf1_base, alt_buf2_base;
	//Fixme:wait for dts ready
	// if (SET_DOWNLOAD_BUFFER_BASE_SIZE(&buf_base, &emmc_buf_size) < 0) {
	// 	errorf("set download buffer error\n");
	// 	return -1;
	// }

	// if (SET_ALT_BUFFER1_BASE_SIZE(&alt_buf1_base, &alter_buffer1.size) < 0) {
	// 	errorf("set download alt buffer1 error\n");
	// 	return -1;
	// }

	// if (SET_ALT_BUFFER2_BASE_SIZE(&alt_buf2_base, &alter_buffer2.size) < 0) {
	// 	errorf("set download alt buffer2 error\n");
	// 	return -1;
	// }
	buf_base = DL_EMMC_BUF_ADDR;
	emmc_buf_size = DL_EMMC_BUF_SIZE;

	alt_buf1_base = DL_ALT1_BUF_ADDR;
	alter_buffer1.size = DL_ALT1_BUF_SIZE;

	alt_buf2_base = DL_ALT2_BUF_ADDR;
	alter_buffer2.size = DL_ALT2_BUF_SIZE;

	g_eMMCBuf = (unsigned char *)ALIGN(buf_base , 8);
	alter_buffer1.addr = (unsigned char *)ALIGN(alt_buf1_base , 8);
	alter_buffer2.addr = (unsigned char *)ALIGN(alt_buf2_base , 8);

	debugf("download buffer base %p, size %llx\nalter buffer1 base %p, size %x\nalter buffer2 base %p, size %x\n",
			g_eMMCBuf, emmc_buf_size, alter_buffer1.addr, alter_buffer1.size, alter_buffer2.addr, alter_buffer2.size);

	return 0;
}
//#endif

uint16_t eMMCCheckSum(const uint32_t * src, uint32_t len)
{
	uint32_t sum = 0;
	unsigned short *src_short_ptr = NULL;

	while (len > 3) {
		sum += *src++;
		len -= 4;
	}

	src_short_ptr = (uint16_t *) src;

	if (0 != (len & 0x2)) {
		sum += *(src_short_ptr);
		src_short_ptr++;
	}

	if (0 != (len & 0x1)) {
		sum += *((unsigned char *)(src_short_ptr));
	}

	sum = (sum >> 16) + (sum & 0x0FFFF);
	sum += (sum >> 16);

	return (uint16_t) (~sum);
}

uint8_t flash_2ndhand_detect(void)
{
	debugf("[%s]...............\n", __func__);
	char part_config;
	char *ifname;
	int dev_id = 0;
	block_dev_desc_t *dev_desc;
	struct mmc *mmc;
	int rpmb_key_result;
	uint8_t result = 0x0;
	ifname = block_dev_get_name();
	dev_id = get_devnum_hwpart(ifname, 0);
	if (dev_id < 0) {
		errorf("%s: get dev_id %d fail\n", __func__, dev_id);
		return -1;
	}
	dev_desc = get_dev_hwpart(ifname, dev_id, USER_PART);
	if (!dev_desc) {
		errorf("%s: get dev_desc fail\n", __func__);
		return -1;
	}

#ifdef CONFIG_EFI_PARTITION
	if (test_part_efi(dev_desc) == 0) {
		dprintf(INFO,"Find gpt header from user partition, flash was used\n");
		result |= GPT_HEADER_EXIST;
		is_fill_flash = 1;
	}
#endif
	if (get_bootdevice() == BOOT_DEVICE_EMMC) {
		mmc = find_mmc_device(0);
		if (!mmc) {
			errorf("%s: get mmc device fail\n", __func__);
			return -1;
		}
		/* judge the partition config */
		part_config = mmc->part_config;
		/* spreadtrum do not use BOOT_ACK,if the BOOT_ACK is 1,the emmc is used by other vendor */
		if (((part_config >> 6) & 0x1) == 0x1) {
			dprintf(INFO,"The Emmc partition config was used\n");
			result |= PARTITION_CONFIG_EXIST;
		}
	}

	rpmb_key_result = is_wr_rpmb_key();
	if (rpmb_key_result != 0) {
		dprintf(INFO,"The flash rpmb partition was used\n");
		result |= RPMB_KEY_EXIST;
	}

	return result;
}

#ifdef CONFIG_MMC
uint8_t emmc_2ndhand_fix(uint8_t detect_result)
{
	struct mmc *mmc = find_mmc_device(0);

	/* clear the partition_boot_ack bit */
	if (detect_result & PARTITION_CONFIG_EXIST)
		mmc->part_config &= 0x3F;
	debugf("[%s]...............\n", __func__);

	return 0;
}
#endif

uint32_t get_pad_data(const uint32_t * src, uint32_t len, uint32_t offset, uint16_t sum)
{
	uint32_t sum_tmp;
	uint32_t sum1 = 0;
	uint32_t pad_data;
	uint32_t i;
	sum = ~sum;
	sum_tmp = sum & 0xffff;
	sum1 = 0;
	for (i = 0; i < offset; i++) {
		sum1 += src[i];
	}
	for (i = (offset + 1); i < len; i++) {
		sum1 += src[i];
	}
	pad_data = sum_tmp - sum1;
	return pad_data;
}

void splFillCheckData(uchar * splBuf)
{
	EMMC_BootHeader *header;
	uint32_t pad_data;
	uint32_t w_len;
	uint32_t w_offset;
	uchar * buf = splBuf + BOOTLOADER_HEADER_OFFSET + sizeof(EMMC_BootHeader);
	w_len = (SPL_CHECKSUM_LEN - (BOOTLOADER_HEADER_OFFSET + sizeof(*header))) / 4;
	w_offset = w_len - 1;

	/*pad the data inorder to make check sum to 0 */
	pad_data = get_pad_data((uint32_t *)buf, w_len, w_offset, 0);
	debugf("splloader fill pad_data=0x%x\n", pad_data);
	*(volatile unsigned int *)(splBuf + SPL_CHECKSUM_LEN - 4) = pad_data;
	header = (EMMC_BootHeader *) (splBuf + BOOTLOADER_HEADER_OFFSET);
	header->version = 0;
	header->magicData = MAGIC_DATA;
	header->checkSum = (uint32_t) eMMCCheckSum((const uint32_t *)(splBuf + BOOTLOADER_HEADER_OFFSET + sizeof(*header)),
						   SPL_CHECKSUM_LEN - (BOOTLOADER_HEADER_OFFSET + sizeof(*header)));
	header->hashLen = 0;
}


int _parser_repartition_cfg_v0(disk_partition_t * partition_info, uchar * partition_cfg, uint16_t total_partition_num)
{
	uint16_t i = 0;
	uint16_t j = 0;
	uint32_t partition_size_m = 0;
	lbaint_t partition_start_lba = 0;
	int32_t blksz = 0;
	uint32_t lbas_per_m = 0;

	blksz = common_get_lba_size();
	if (-1 == blksz)
		return -1;

	lbas_per_m = SZ_1M / blksz;

	/*first 1 MB will be reserved, no partition can located here*/
	partition_start_lba = SZ_1M / blksz;

	/*Decode String: Partition Name(72Byte)+SIZE(4Byte)+... */
	for (i = 0; i < total_partition_num; i++) {
		partition_info[i].blksz = (ulong)blksz;
		strcpy(partition_info[i].platform_type, "LK");	/*not useful */

		partition_size_m = *(uint32_t *) (partition_cfg + 76 * (i + 1) - 4);
		/*the last partition and partition_size_m is 0xFFFFFFFF means this partition use all the spare lba */
		if ((i == total_partition_num - 1) && (MAX_SIZE_FLAG == partition_size_m)) {
			/*size=0 represent use all the spare lba */
			partition_info[i].blk_cnt = 0;
		} else {
			/*calc the partition size of lba , raw data unit is Mb */
			partition_info[i].blk_cnt = (lbaint_t) partition_size_m * lbas_per_m;
		}

		/*in raw data rcv from download tool,partition_name is 38 uint16_t length;
		 **in part.h ,disk_partition_t.name is 32 uchar length;
		 **in part_efi.h ,gpt_entry.partition_name is 36 uint16_t length,
		 **we convert raw data to disk_partition_t format here , fill_gpt_pte() in part_efi.c will
		 **convert disk_partition_t to gpt_entry*/
		for (j = 0; j < 32 - 1; j++) {
			/*transform 64 bytes uint16_t to 32 bytes uchar ,and discard the last 8 bytes */
			partition_info[i].part_name[j] = *((uint16_t *) partition_cfg + 38 * i + j) & 0xFF;
		}
		partition_info[i].part_name[j] = '\0';
		partition_info[i].start_blk = partition_start_lba;

		partition_start_lba += partition_info[i].blk_cnt;
#ifdef CONFIG_PARTITION_UUIDS
		uuid_bin_to_str(partition_info[i].part_name, partition_info[i].uuid, UUID_STR_FORMAT_GUID);
#endif

		debugf("partition name:%s,partition_blk_cnt:0x"LBAF",partiton_start:0x"LBAF",\n", partition_info[i].part_name, partition_info[i].blk_cnt,
		       partition_info[i].start_blk);
	}

	return 0;
}


int _parser_repartition_cfg_v1(disk_partition_t * partition_info, uchar* partition_cfg, uint16_t total_partition_num,
									uchar size_unit)
{
	uint32_t i, j;

	uint64_t partition_size_raw;
	uint64_t partition_size_lba;
	/*attention here, we use int64 instead of uint64 to support minus gap size, such as "-2048"*/
	int64_t gap_raw;
	int64_t gap_lba;
	uint64_t partition_start_lba = 0;
	int32_t blksz = 0;

	blksz = common_get_lba_size();
	if (-1 == blksz)
		return -1;
	/*V1 partition table format: Partition Name(72Byte)+SIZE(8Byte)+GAP(8Byte)...*/
	for (i = 0; i < total_partition_num; i++) {
		partition_size_raw = *(uint64_t *)(partition_cfg + 88 * (i + 1) - 16);
		gap_raw = *(int64_t *)(partition_cfg + 88 * (i + 1) - 8);
		// debugf("partition_size_raw = 0x%llx, gap_raw = 0x%llx\n", partition_size_raw, gap_raw);

		switch (size_unit) {
		/*unit is MB*/
		case 0:
			partition_size_lba = partition_size_raw * SZ_1M / blksz;
			gap_lba = gap_raw * SZ_1M / blksz;
			break;
		/*unit is 512K*/
		case 1:
			partition_size_lba = partition_size_raw * SZ_512K / blksz;
			gap_lba = gap_raw * SZ_512K / blksz;
			break;
		/*unit is KB*/
		case 2:
			partition_size_lba = PAD_SIZE(partition_size_raw * SZ_1K, blksz) / blksz;
			if (0 == gap_raw)
				gap_lba = 0;
			else
				gap_lba = PAD_SIZE(gap_raw * SZ_1K, blksz) / blksz;
			break;
		/*unit is bytes, min partition unit supported is lba(512byte), so we pad it if neccessary*/
		case 3:
			partition_size_lba = PAD_SIZE(partition_size_raw, blksz) / blksz;
			if (0 == gap_raw)
				gap_lba = 0;
			else
				gap_lba = PAD_SIZE(gap_raw, blksz) / blksz;
			break;
		/*unit is sector*/
		case 4:
			partition_size_lba = partition_size_raw;
			gap_lba = gap_raw;
			break;
		/*unsupported unit we consider it as MB*/
		default:
			partition_size_lba = partition_size_raw * SZ_1M / blksz;
			gap_lba = gap_raw * SZ_1M / blksz;
			break;
		}

		/*check the gap of the first partition*/
		if (0 == i) {
			if (0 == gap_lba) {
				/*first 1 MB will be reserved, no partition can located here*/
				gap_lba = SZ_1M / blksz;
			} else if (gap_lba < FIRST_USABLE_LBA_FOR_PARTITION) {
				errorf("%s: FATAL ERROR, gap of the first partition counted in lba CAN NOT less than 34\n", __FUNCTION__);
				return -1;
			}
		}

		for (j = 0; j < 32 - 1; j++) {
			/*transform 64 bytes uint16_t to 32 bytes uchar ,and discard the last 8 bytes */
			partition_info[i].part_name[j] = *((uint16_t *)partition_cfg + 44 * i + j) & 0xFF;
		}
		partition_info[i].part_name[j] = '\0';

		/*all the 8 bytes are 0xFF in the packet, but to compatible with the old version,
			we only consider the lower 4 bytes.*/
		if ((i == total_partition_num - 1) && (partition_size_raw >= MAX_SIZE_FLAG)) {
			partition_size_lba = 0;
			debugf("the last partition use all the left lba\n");
		}
		partition_start_lba += gap_lba;
		partition_info[i].blksz = (ulong)blksz;
		partition_info[i].start_blk = (lbaint_t)partition_start_lba;
		partition_info[i].blk_cnt = (lbaint_t)partition_size_lba;
		if (0 != partition_size_lba)
			partition_start_lba += partition_size_lba;

		strcpy(partition_info[i].platform_type, "LK");	/*not useful */

#ifdef CONFIG_PARTITION_UUIDS
		uuid_bin_to_str(partition_info[i].part_name, partition_info[i].uuid, UUID_STR_FORMAT_GUID);
#endif

		debugf("partition name:%s,partition_blk_cnt:0x"LBAF", partition_start:0x"LBAF"\n",
			partition_info[i].part_name, partition_info[i].blk_cnt, partition_info[i].start_blk);
	}
	return 0;
}

int _parser_repartition_cfg(disk_partition_t * partition_info, uchar* partition_cfg, uint16_t total_partition_num,
								unsigned char version, unsigned char size_unit)
{
	int ret = 0;

	switch (version) {
	case 0:
		debugf("Handle repartition packet version 0  \n");
		ret = _parser_repartition_cfg_v0(partition_info, partition_cfg, total_partition_num);
		break;
	case 1:
	default:
		debugf("Handle repartition packet version 1  \n");
		ret = _parser_repartition_cfg_v1(partition_info, partition_cfg, total_partition_num, size_unit);
		break;
	}
	return ret;
}



/**
	Get the backup partition name
*/
const char *_get_backup_partition_name(const char * partition_name)
{
	int i = 0;

	for (i = 0; s_special_partition_cfg[i].partition != NULL; i++) {
		if (0 == strcmp(partition_name, s_special_partition_cfg[i].partition)) {
			return s_special_partition_cfg[i].bak_partition;
		}
	}

	return NULL;
}

#ifdef SPRD_SPARSE_SUPER_SPEEDUP
/* sparse data proc */
ALTER_BUFFER_ATTR *sparse_temp_buf = NULL;
ALTER_BUFFER_ATTR sparse_buf;

/*prepare for sparse temp buf*/
void prepare_sparse_temp_buf(void)
{
	sparse_buf.addr = DL_SPARSE_TEMP_BUF_ADDR;
	sparse_buf.size = DL_SPARSE_TEMP_BUF_SIZE;
	sparse_buf.used = 0;
	sparse_buf.fixed = 0;
	sparse_buf.status = BUFFER_CLEAN;

	sparse_temp_buf = &sparse_buf;
	return;
}

/*prepare for emmc write buf*/
void prepare_emmc_write_buf(void)
{
	emmc_buffer1.addr = g_eMMCBuf;
	emmc_buffer1.size = SZ_128M;
	emmc_buffer1.used = 0;
	emmc_buffer1.fixed = 0;
	emmc_buffer1.status = BUFFER_CLEAN;
	emmc_buffer1.next = &emmc_buffer2;

	emmc_buffer2.addr = g_eMMCBuf + emmc_buf_size/2;
	emmc_buffer2.size = SZ_128M;
	emmc_buffer2.used = 0;
	emmc_buffer2.fixed = 0;
	emmc_buffer2.status = BUFFER_CLEAN;
	emmc_buffer2.next = &emmc_buffer1;

	emmc_cur_buf = &emmc_buffer1;
	sparse2emmc_buf = &emmc_buffer1;
	return;
}
#endif

void prepare_alternative_buffers(void)
{
	alter_buffer1.pointer = alter_buffer1.addr;
	alter_buffer1.used = 0;
	alter_buffer1.spare = alter_buffer1.size;
	alter_buffer1.status = BUFFER_CLEAN;
	alter_buffer1.next = &alter_buffer2;

	alter_buffer2.pointer = alter_buffer2.addr;
	alter_buffer2.used = 0;
	alter_buffer2.spare = alter_buffer2.size;
	alter_buffer2.status = BUFFER_CLEAN;
	alter_buffer2.next = &alter_buffer1;

	current_buffer = &alter_buffer1;
	return;
}

int speedup_download_process(uint32_t size, char *buf)
{
	uint32_t tot_buf_size = current_buffer->size;
	uint32_t last_size = 0;
	char temp[32] = {0};
	//Fixme：delete later，no-AB
	int v_ab_flag = !get_ab_partition(temp);
	const char *temp_ab = temp;

	if (current_buffer->spare > size)  {
		memcpy(current_buffer->pointer, buf, size);
		current_buffer->used += size;
		current_buffer->spare -= size;
		current_buffer->pointer += size;

		if (g_status.total_recv_size == g_status.total_size) {
			if (BUFFER_DIRTY == current_buffer->next->status) {
				if (v_ab_flag) {
					if (0 != common_query_backstage(temp_ab,
						tot_buf_size, current_buffer->next->addr)) {
						errorf("last query backstage %s fail, offset=0x%llx\n", temp_ab, g_dl_eMMCStatus.offset);
						return OPERATE_WRITE_ERROR;
					}
				} else {
					if (0 != common_query_backstage(g_dl_eMMCStatus.curUserPartitionName,
						tot_buf_size, current_buffer->next->addr)) {
						errorf("last query backstage %s fail, offset=0x%llx\n", g_dl_eMMCStatus.curUserPartitionName, g_dl_eMMCStatus.offset);
						return OPERATE_WRITE_ERROR;
					}
				}
			}
			if (0 != common_raw_write(g_dl_eMMCStatus.curUserPartitionName,
				(uint64_t)(current_buffer->used), (uint64_t)0, (uint64_t)(g_dl_eMMCStatus.offset), (char *)current_buffer->addr)) {
				errorf("last write fail, offset=0x%llx\n", g_dl_eMMCStatus.offset);
				return OPERATE_WRITE_ERROR;
			}

			if (v_ab_flag) {
				debugf("download slot: %s\n", temp_ab);
				if (0 != common_raw_write(temp_ab,
					(uint64_t)(current_buffer->used), (uint64_t)0, (uint64_t)(g_dl_eMMCStatus.offset), (char *)current_buffer->addr)) {
					errorf("last write fail, offset=0x%llx\n", g_dl_eMMCStatus.offset);
					return OPERATE_WRITE_ERROR;
				}
			}
			g_status.unsave_recv_size = 0;
		}
	} else {
		memcpy(current_buffer->pointer, buf, current_buffer->spare);
		last_size = size - current_buffer->spare;
		if (BUFFER_DIRTY == current_buffer->next->status) {
			if (v_ab_flag) {
				if (0 != common_query_backstage(temp_ab,
					tot_buf_size, current_buffer->next->addr)) {
					errorf("last query backstage %s fail, offset=0x%llx\n", temp_ab, g_dl_eMMCStatus.offset);
					return OPERATE_WRITE_ERROR;
				}
			} else {
				if (0 != common_query_backstage(g_dl_eMMCStatus.curUserPartitionName,
					tot_buf_size, current_buffer->next->addr)) {
					errorf("last query backstage %s fail, offset=0x%llx\n", g_dl_eMMCStatus.curUserPartitionName, g_dl_eMMCStatus.offset);
					return OPERATE_WRITE_ERROR;
				}
			}
		}
		if (0 != common_write_backstage(g_dl_eMMCStatus.curUserPartitionName,
			tot_buf_size, g_dl_eMMCStatus.offset, current_buffer->addr)) {
			errorf("write backstage %s fail, offset=0x%llx\n", g_dl_eMMCStatus.curUserPartitionName, g_dl_eMMCStatus.offset);
			return OPERATE_WRITE_ERROR;
		}
		if (v_ab_flag) {
			if (0 != common_query_backstage(g_dl_eMMCStatus.curUserPartitionName,
					tot_buf_size, current_buffer->next->addr)) {
					errorf("last query backstage %s fail, offset=0x%llx\n", g_dl_eMMCStatus.curUserPartitionName, g_dl_eMMCStatus.offset);
					return OPERATE_WRITE_ERROR;
				}
			if (0 != common_write_backstage(temp_ab,
				tot_buf_size, g_dl_eMMCStatus.offset, current_buffer->addr)) {
				errorf("write backstage %s fail, offset=0x%llx\n", temp_ab, g_dl_eMMCStatus.offset);
				return OPERATE_WRITE_ERROR;
			}
		}
		g_dl_eMMCStatus.offset += tot_buf_size;
		if (0 != last_size)
			memcpy(current_buffer->next->addr, buf + current_buffer->spare, last_size);
		current_buffer->used = 0;
		current_buffer->spare = current_buffer->size;
		current_buffer->pointer = current_buffer->addr;
		current_buffer->status = BUFFER_DIRTY;

		current_buffer = current_buffer->next;
		current_buffer->pointer = current_buffer->addr + last_size;
		current_buffer->used = last_size;
		current_buffer->spare = current_buffer->size - last_size;
		if (g_status.total_recv_size == g_status.total_size) {
			if (v_ab_flag) {
				if (0 != common_query_backstage(temp_ab,
					tot_buf_size, current_buffer->next->addr)) {
					errorf("last cross query backstage %s fail, offset=0x%llx\n", temp_ab, g_dl_eMMCStatus.offset);
					return OPERATE_WRITE_ERROR;
				}
			} else {
				if (0 != common_query_backstage(g_dl_eMMCStatus.curUserPartitionName,
					tot_buf_size, current_buffer->next->addr)) {
					errorf("last cross query backstage %s fail, offset=0x%llx\n", g_dl_eMMCStatus.curUserPartitionName, g_dl_eMMCStatus.offset);
					return OPERATE_WRITE_ERROR;
				}
			}

			if (0 != current_buffer->used) {
				if (0 != common_raw_write(g_dl_eMMCStatus.curUserPartitionName,
					(uint64_t)(current_buffer->used), (uint64_t)0, (uint64_t)(g_dl_eMMCStatus.offset), (char *)current_buffer->addr)) {
					errorf("last cross write fail, offset=0x%llx\n", g_dl_eMMCStatus.offset);
					return OPERATE_WRITE_ERROR;
				}

				if (v_ab_flag) {
					debugf("download slot: %s\n", temp_ab);
					if (0 != common_raw_write(temp_ab,
						(uint64_t)(current_buffer->used), (uint64_t)0, (uint64_t)(g_dl_eMMCStatus.offset), (char *)current_buffer->addr)) {
						errorf("last cross write fail, offset=0x%llx\n", g_dl_eMMCStatus.offset);
						return OPERATE_WRITE_ERROR;
					}
				}
			}
			g_status.unsave_recv_size = 0;
		}
	}

	return OPERATE_SUCCESS;
}

#ifdef SPRD_SPARSE_SUPER_SPEEDUP
extern uint64_t have_dont_care;
extern uint64_t dont_care_temp;
int write_sparse2emmc_img(block_dev_desc_t *dev_desc) {
	int chunk_temp;

	if (sparse_header.total_blks == total_blocks) {
		/* first is the last(image little than 2MB)*/
		if (first_emmc != 1) {
			if (0 != common_query_backstage(g_dl_eMMCStatus.curUserPartitionName,
							emmc_cur_buf->fixed, emmc_cur_buf->addr)) {
							errorf("last query backstage %s fail, offset=0x%llx\n", g_dl_eMMCStatus.curUserPartitionName, g_dl_eMMCStatus.offset);
			}
			//prt("common_query cost:%lld\n",get_ticks()-emmc_start);
			prt("emmc_cur_buf:0x%x has been writed!!!\n", emmc_cur_buf->addr);
			g_dl_eMMCStatus.offset += emmc_cur_buf->fixed;
			prt("g_dl_eMMCStatus.offset after backstage:0x%llx\n", g_dl_eMMCStatus.offset);

			emmc_cur_buf->fixed = 0;
			emmc_cur_buf = emmc_cur_buf->next;
		}
		prt("it is emmc loop recvsize=0x%llx totalsize=0x%llx offset=0x%llx\n",g_status.total_recv_size,g_status.total_size, g_dl_eMMCStatus.offset);
		prt("ready to handle emmc_cur_buf->addr:0x%x,emmc_cur_buf->fixed:0x%x\n", emmc_cur_buf->addr, emmc_cur_buf->fixed);
		g_status.total_recv_size += emmc_cur_buf->fixed;

		/* for last packet, align for blksz bytes */
		if (emmc_cur_buf->fixed != 0) {
			prt("ready to handle:0x%x\n",emmc_cur_buf->addr);
			g_status.total_recv_size += sparse2emmc_buf->fixed;
			prt("last!!!,g_status.total_recv_size :0x%llx, emmc_cur_buf->fixed:0x%x\n", g_status.total_recv_size, emmc_cur_buf->fixed);
			prt("g_dl_eMMCStatus.offset before last write:0x%llx\n", g_dl_eMMCStatus.offset);
			if (0 != common_raw_write(g_dl_eMMCStatus.curUserPartitionName,
						emmc_cur_buf->fixed, (uint64_t)0,
						(uint64_t)(g_dl_eMMCStatus.offset),
						emmc_cur_buf->addr)) {
						errorf("last write fail, offset=0x%llx\n", g_dl_eMMCStatus.offset);
						return OPERATE_WRITE_ERROR;
					}
			g_dl_eMMCStatus.offset += emmc_cur_buf->fixed;
		}
		if (have_dont_care) {
			prt("g_dl_eMMCStatus.offset before common_raw_erase:0x%llx\n", g_dl_eMMCStatus.offset);
			if (0 != common_raw_erase(g_dl_eMMCStatus.curUserPartitionName, dont_care_temp, g_dl_eMMCStatus.offset)) {
				errorf("for last: erase dont care data failed! erase dont care type offset:0x%llx, size:%lld\n", g_dl_eMMCStatus.offset, dont_care_temp);
			}
			have_dont_care = 0;
			g_dl_eMMCStatus.offset += dont_care_temp;
		}
		chunk_temp_offset = 0;
		chunk_temp = 0;
		emmc_cur_buf->fixed = 0;

		prt("emmc enter wait\n");
		g_status.total_recv_size = 0;
		g_status.total_size = 0;
		total_blocks = 0;
		first_emmc = 1;

	/*for normal write */
	} else {
		prt("it is emmc loop recvsize=0x%llx totalsize=0x%llx offset=0x%llx\n", g_status.total_recv_size, g_status.total_size, g_dl_eMMCStatus.offset);
		prt("have_dont_care:%lld\n", have_dont_care);
		if (first_emmc == 1) {
			prt("first write emmc,ready to handle emmc_cur_buf->addr:0x%x,emmc_cur_buf->fixed:0x%x\n", emmc_cur_buf->addr, emmc_cur_buf->fixed);
			g_status.total_recv_size += emmc_cur_buf->fixed;
			prt("g_dl_eMMCStatus.offset before backstage:0x%llx\n", g_dl_eMMCStatus.offset);

			if (0 != common_write_backstage(g_dl_eMMCStatus.curUserPartitionName,
						emmc_cur_buf->fixed, g_dl_eMMCStatus.offset, emmc_cur_buf->addr)) {
						errorf("write backstage %s fail, offset=0x%llx\n", g_dl_eMMCStatus.curUserPartitionName, g_dl_eMMCStatus.offset);
			}
			first_emmc = 0;
		} else {
			if (emmc_cur_buf->fixed) {
				if (0 != common_query_backstage(g_dl_eMMCStatus.curUserPartitionName, emmc_cur_buf->fixed, emmc_cur_buf->addr)) {
					errorf("last query backstage %s fail, offset=0x%llx\n", g_dl_eMMCStatus.curUserPartitionName, g_dl_eMMCStatus.offset);
				}
				//prt("common_query cost:%lld\n",get_ticks()-emmc_start);
				prt("emmc_cur_buf:0x%x has been writed!!!\n", emmc_cur_buf->addr);
				g_dl_eMMCStatus.offset += emmc_cur_buf->fixed;
				prt("g_dl_eMMCStatus.offset after backstage:0x%llx\n", g_dl_eMMCStatus.offset);

				emmc_cur_buf->fixed = 0;
				emmc_cur_buf = emmc_cur_buf->next;
			}
			if (have_dont_care) {
				prt("erase dont care type at g_dl_eMMCStatus.offset:0x%llx, emmc_cur_buf->fixed:0x%x, dont care size:%lld\n", g_dl_eMMCStatus.offset+emmc_cur_buf->fixed, emmc_cur_buf->fixed, dont_care_temp);
				if (0 != common_raw_erase(g_dl_eMMCStatus.curUserPartitionName, dont_care_temp, g_dl_eMMCStatus.offset+emmc_cur_buf->fixed)) {
					errorf("erase dont care data failed! erase dont care type offset:0x%llx, emmc_cur_buf->fixed:0x%x, size:%lld\n", g_dl_eMMCStatus.offset+emmc_cur_buf->fixed, emmc_cur_buf->fixed, dont_care_temp);
				}
				have_dont_care = 0;
			}
			prt("emmc_cur_buf->fixed:0x%x\n", emmc_cur_buf->fixed);
			if (emmc_cur_buf->fixed) {
				prt("g_dl_eMMCStatus.offset before backstage:0x%llx\n", g_dl_eMMCStatus.offset);
				if (0 != common_write_backstage(g_dl_eMMCStatus.curUserPartitionName,
							emmc_cur_buf->fixed, g_dl_eMMCStatus.offset, emmc_cur_buf->addr)) {
							errorf("write backstage %s fail, offset=0x%llx\n", g_dl_eMMCStatus.curUserPartitionName, g_dl_eMMCStatus.offset);
				}
			}
			g_dl_eMMCStatus.offset += dont_care_temp;
		}
	}
    return 0;
}
#endif

void _get_partition_attribute(uchar * partition_name)
{
	int i;

	for (i = 0; s_special_partition_cfg[i].partition != NULL; i++) {
		if (0 == strcmp(s_special_partition_cfg[i].partition, partition_name)) {
			g_dl_eMMCStatus.curImgType = s_special_partition_cfg[i].imgattr;
			g_dl_eMMCStatus.partitionpurpose = s_special_partition_cfg[i].purpose;
			debugf("partition %s image type is %d,partitionpurpose:%d\n", partition_name, g_dl_eMMCStatus.curImgType,
			       g_dl_eMMCStatus.partitionpurpose);
			return;
		}
	}

	/*default type is IMG_RAW */
	g_dl_eMMCStatus.curImgType = IMG_RAW;
	g_dl_eMMCStatus.partitionpurpose = PARTITION_PURPOSE_NORMAL;
	debugf("partition %s image type is RAW, normal partition!\n", partition_name);

	return;
}

int is_f2fs_filesystem(const char *part_name)
{
	__le32 f2fs_magic = 0;
	if (strcmp(part_name, "userdata"))
		return 0;
	if (0 != common_raw_read(part_name, sizeof(f2fs_magic), (uint64_t)0x400, (char *)&f2fs_magic)) {
		return 0;
	}
	if (F2FS_SUPER_MAGIC == f2fs_magic)
		return 1;
	return 0;
}

PARTITION_PURPOSE dl_get_partition_purpose(const char *part_name)
{
	PARTITION_PURPOSE partition_purpose = PARTITION_PURPOSE_NORMAL;
	int i;

	/*get the special partition info */
	for (i = 0; NULL != s_special_partition_cfg[i].partition; i++) {
		if (!strcmp(s_special_partition_cfg[i].partition, part_name)) {
			partition_purpose = s_special_partition_cfg[i].purpose;
			break;
		}
	}

	return partition_purpose;
}

#ifdef CONFIG_DL_SKIP_PARTITION
PARTITION_SKIP dl_get_partition_skip(const char *part_name)
{
	PARTITION_SKIP partition_skip = PARTITION_DOWNLOAD;
	int i;

	/*get the special partition info */
	for (i = 0; NULL != special_partition_cfg[i].partition; i++) {
		if (!strcmp(special_partition_cfg[i].partition, part_name)) {
			partition_skip = special_partition_cfg[i].skip;
			dprintf(INFO, "partition %s need skip operation\n", part_name);
			break;
		}
	}

	return partition_skip;
}
#endif

#ifdef SPRD_SECBOOT
OPERATE_STATUS dl_secboot_verify(ulong *strip, const char *part_name,
	uint64_t rcv_size, uint64_t total_size, unsigned char *buf)
{
	boot_mode_t boot_role = get_boot_role();
	VERIFY_RESULT verify_res;
	const char *mode;

	if (BOOTLOADER_MODE_DOWNLOAD == boot_role) {
		/* for normal download mode, use dl_secure_process_flow verify */
		verify_res = dl_secure_process_flow(strip,
			part_name,
			rcv_size, total_size, buf);
	} else {
		/* for autodloader mode, use fb_secure_process_flow verify */
		mode = g_env_bootmode;
		if (mode != NULL && !strcmp(mode, "autodloader"))
			fastboot_image_size = rcv_size;
		flush_cache(buf, rcv_size);
		verify_res = fb_secure_process_flow(strip,
			part_name,
			rcv_size, total_size, buf);
	}
	switch (verify_res) {
	case VERIFY_FAIL:
		if (!strcmp("system", part_name))
			common_raw_erase(part_name, 0, 0);
		return OPERATE_VERIFY_ERROR;
	/*for future extension*/
	case VERIFY_STOP_WRITE:
	case VERIFY_NO_NEED:
	case VERIFY_OK:
	default:
		break;
	}

	return OPERATE_SUCCESS;
}
#endif
/* clear g_status.unsave_recv_size before return if necessary */
OPERATE_STATUS dl_backup(const char *part_name, uint64_t rcv_size,
	unsigned char *buf)
{
	static const char *part_bk[] = {
		"splloader",
		"uboot",
		"trustos",
		"teecfg",
		"sml",
		"vbmeta",
	};
	char part_name_bak[PARTNAME_SZ + 8];
	int i;

	for (i = sizeof(part_bk) / sizeof(part_bk[0]) - 1; i >= 0; i--) {
		if (!strcmp(part_bk[i], part_name)) {
			sprintf(part_name_bak, "%s_bak", part_name);
			if (0 != common_raw_write(part_name_bak, rcv_size, (uint64_t)0,
						(uint64_t)0, (char *)buf))
				return OPERATE_WRITE_ERROR;

			break;
		}
	}

	if (i < 0)
		return OPERATE_IGNORE;

	return OPERATE_SUCCESS;
}

OPERATE_STATUS dl_check_nv_img(void)
{
	uint32_t fix_nv_checksum;

	fix_nv_checksum = Get_CheckSum((uchar *)g_eMMCBuf, g_status.total_recv_size);
	if (fix_nv_checksum != g_checksum) {
		/*may data transfer error */
		debugf("nv data transfer error,checksum error!\n");
		return -OPERATE_CHECKSUM_DIFF;
	}

	return OPERATE_SUCCESS;
}



void sprd_hexdump(uint32_t title, uint8_t * data, int len)
{
    int i, j;
    int N = len / 16 + 1;
    dprintf(INFO,"describe :0x%x, ", title);
    dprintf(INFO,"sprd_hexdump:%d bytes", len);
    for (i = 0; i < N; i++) {
        for (j = 0; j < 16; j++) {
            if (i * 16 + j >= len)
                goto end;
            dprintf(INFO,"%02x ", data[i * 16 + j]);
        }
    }

end:
    dprintf(INFO,"\n");
}
#ifdef NV_ENCRYPTION
extern int nv_aes_gcm(unsigned char *input, int input_bytelen, unsigned char *output, int *output_bytelen);
int decryptFixnvPartition(uint8_t* ori_buf, uint32_t data_size){
	uint32_t id = -1;
	uint32_t itemSize = -1;
	uint32_t itemPos = -1;
	uint32_t offset = 4;
	uint16_t tmp[2];
	uint8_t *databuf = NULL;
	uint32_t tempsize;
	uint32_t printsize = 0;

	debugf("[%s]:decrptFixnvPartition enter\n", __func__);
	while (1) {
		if (*(uint16_t *) (ori_buf + offset) == INVALID_ID) {
			debugf("findItem find the tail\n");
			break;
		}
		if (offset + sizeof(tmp) > data_size) {
			debugf("findItem Surpass the boundary of the part\r\n");
			break;
		}
		memcpy(tmp, ori_buf + offset, sizeof(tmp));
		offset += sizeof(tmp);
		id = (uint32_t) tmp[0];
		itemSize = (uint32_t) tmp[1];
		itemPos = offset;
#if PRINT_NV_DATA
		debugf("id = 0x%x, itemSize = 0x%x, itemPos = 0x%x. \n", id, itemSize, itemPos);
#endif
		databuf = malloc(sizeof(char) *itemSize);
		if(databuf == NULL){
			debugf("malloc databuf error\n");
			return -1;
		}
		memset(databuf, 0x0, itemSize);
#if PRINT_NV_DATA
		printsize = itemSize>32 ? 32 : itemSize;
		sprd_hexdump(id, ori_buf+itemPos, printsize);
#endif
#ifdef NV_MULTICORES
        if (id > 0) {
            nv_aes_gcm(ori_buf+itemPos, itemSize, databuf, &tempsize);
        }
#else
		nv_aes_gcm(ori_buf+itemPos, itemSize, databuf, &tempsize);
#endif
#if PRINT_NV_DATA
		sprd_hexdump(id, databuf, printsize);
#endif
#ifdef NV_MULTICORES
        if (id > 0) {
            memcpy(ori_buf+itemPos, databuf, itemSize);
        }
#else
        memcpy(ori_buf+itemPos, databuf, itemSize);
#endif

		offset += tmp[1];
		offset = (offset + 3) & 0xFFFFFFFC;
		free(databuf);
	}
	return 0;
}

int encryptFixnvPartition(uint8_t* ori_buf, uint32_t data_size){
    uint32_t id = -1;
    uint32_t itemSize = -1;
    uint32_t itemPos = -1;
    uint32_t offset = 4;
    uint16_t tmp[2];
    uint8_t *databuf = NULL;
    uint32_t tempsize;
    uint32_t printsize = 0;
    debugf("encryptFixnvPartition enter\n");
    while (1) {
        if (*(uint16_t *) (ori_buf + offset) == INVALID_ID) {
            debugf("findItem find the tail\n");
            break;
        }
        if (offset + sizeof(tmp) > data_size) {
            debugf("findItem Surpass the boundary of the part\r\n");
            break;
        }
        memcpy(tmp, ori_buf + offset, sizeof(tmp));
        offset += sizeof(tmp);
        id = (uint32_t) tmp[0];
        itemSize = (uint32_t) tmp[1];
        itemPos = offset;
#if PRINT_NV_DATA
        debugf("id = 0x%x, itemSize = 0x%x, itemPos = 0x%x. \n", id, itemSize, itemPos);
#endif
        databuf = malloc(sizeof(char) *itemSize);
        if(databuf == NULL){
        	debugf("malloc databuf error\n");
        	return -1;
        }
        memset(databuf, 0x0, itemSize);
#if PRINT_NV_DATA
        printsize = itemSize>32 ? 32 : itemSize;
        debugf("before encrypt \n");
        sprd_hexdump(id, ori_buf+itemPos, printsize);
#endif
#ifdef NV_MULTICORES
        if (id > 0) {
            nv_aes_gcm(ori_buf+itemPos, itemSize, databuf, &tempsize);
        }
#else
        nv_aes_gcm(ori_buf+itemPos, itemSize, databuf, &tempsize);
#endif
#if PRINT_NV_DATA
        debugf("after encrypt, output len= 0x%x\n", tempsize);
        sprd_hexdump(id, databuf, printsize);
#endif
#ifdef NV_MULTICORES
        if (id > 0) {
            memcpy(ori_buf+itemPos, databuf, itemSize);
        }
#else
        memcpy(ori_buf+itemPos, databuf, itemSize);
#endif
        offset += tmp[1];
        offset = (offset + 3) & 0xFFFFFFFC;
        free(databuf);
    }
    debugf("fixnv encrypt success\n");
    return 0;
}
#ifdef NV_MULTICORES
int decryptRunnvPartition(uint8_t* ori_buf){
    uint32_t id = -1;
    uint32_t itemSize = -1;
    uint32_t itemPos = -1;
    uint16_t tmp[2];
    uint8_t *databuf = NULL;
    uint8_t *origal_buf = ori_buf;
    nv_dir_t dir, dir0;
    uint32_t dir_offset = SECTOR_SIZE * 2;
    uint32_t bak_dir_offset[MAX_CORENUM];
    uint32_t bak_dir_offset_all = 0;
    uint32_t offset = 0;
    runnv_header_t npb[MAX_CORENUM];
    uint32_t printsize = 0;
    uint32_t tempsize = 0;
    uint8_t corecount = 0;
    uint16_t corenum = 0;
    uint32_t id0_offset = 0;

    memcpy(&dir0, origal_buf+dir_offset, sizeof(dir0));
    id0_offset = dir0.offset;
    debugf("id0_offset = 0x%0x\n",id0_offset);
    corenum = *(uint16_t *)(origal_buf + id0_offset + CORENUM_OFFSET);
    debugf("corenum = 0x%0x\n",corenum);
    // get npb data
    memcpy(&npb[0], origal_buf, sizeof(runnv_header_t));
    for (int i = 1; i < corenum; i++) {
        memcpy(&npb[i], origal_buf + npb[0].core_info[i].core_offset, sizeof(runnv_header_t));
    }
#if PRINT_NV_DATA
    for(int i = 0; i < corenum; i++) {
        debugf("npb[%d]: magic=0x%x, timestamp=0x%x, minid=0x%x, maxid=0x%x\n",
        i, npb[i].magic, npb[i].timestamp, npb[i].minid, npb[i].maxid);
        debugf("npb[%d]: totalSector=0x%x, bytesPerSector=0x%x, dirCount=0x%x, dirSize=0x%x\n",
        i, npb[i].totalSector, npb[i].bytesPerSector, npb[i].dirCount, npb[i].dirSize);
    }
#endif
    // calu bak dir offset
    for (int j = 0; j < corenum; j++) {
        origal_buf += npb[0].core_info[j].core_offset;
        offset = dir_offset;
        if ((npb[j].dirCount * npb[j].dirSize) % SECTOR_SIZE == 0)
            bak_dir_offset[j] = (uint32_t)(npb[j].dirCount * npb[j].dirSize) + dir_offset;
        else
            bak_dir_offset[j] = (uint32_t)((npb[j].dirCount * npb[j].dirSize) / SECTOR_SIZE) * SECTOR_SIZE + SECTOR_SIZE  + dir_offset;
        while (offset < bak_dir_offset[j]) {
            if (*(uint16_t *) (origal_buf + offset) == 0) {
                debugf("item size = 0, get next dir\n");
                offset += sizeof(nv_dir_t);
                if (offset == bak_dir_offset[j]) {
                    break;
                }
                continue;
            }
            memcpy(&dir, origal_buf+offset, sizeof(dir));
#if PRINT_NV_DATA
            debugf("nv dir:itemSize=0x%x, checksum=0x%x, offset=0x%x, status=0x%x\n",
            dir.itemSize, dir.checksum, dir.offset, dir.status);
#endif
            if (dir.offset + sizeof(tmp) > npb[j].core_info[j].core_size) {
                debugf(" ___findItem Surpass the boundary of the part\r\n");
                break;
            }
            memcpy(tmp, origal_buf + dir.offset, sizeof(tmp));
            id = (uint32_t) tmp[0];
            itemSize = (uint32_t) tmp[1];
            itemPos = dir.offset + sizeof(tmp);
#if PRINT_NV_DATA
            debugf("id = 0x%x, itemSize = 0x%x, itemPos = 0x%x. \n", id, itemSize, itemPos);
#endif
    // check id/len/checksum
            if (id < npb[j].minid || id > npb[j].maxid) {
                debugf("id is invalid\n");
                break;
            }
            if (itemSize != dir.itemSize) {
                debugf("itemSize is diff with dir\n");
                break;
            }
    //TODO calc checksum
            if (fdl_calc_checksum(origal_buf+itemPos, itemSize) != dir.checksum) {
                debugf("nvid :0x%x checksum error from dir", id);
            }
            if (id == 0) {
                offset += sizeof(dir);
                continue;
            }
            databuf = malloc(sizeof(char) *itemSize);
            if (databuf == NULL) {
                debugf("malloc databuf error\n");
                break;
            }
            memset(databuf, 0x0, sizeof(char) *itemSize);
#if PRINT_NV_DATA
    // get data & encrypt
            printsize = itemSize>32 ? 32 : itemSize;
            debugf("before decrypt \n");
            sprd_hexdump(id, origal_buf+itemPos, printsize);
#endif
            nv_aes_gcm(origal_buf+itemPos, itemSize, databuf, &tempsize);
#if PRINT_NV_DATA
            debugf("after decrypt, output len = 0x%x \n", tempsize);
            sprd_hexdump(id, databuf, printsize);
#endif
            memcpy(origal_buf+itemPos, databuf, itemSize);
    // calu checksum afte encrypt
            dir.checksum = fdl_calc_checksum(databuf, itemSize);
            memcpy(origal_buf+offset, &dir, sizeof(dir));
            memcpy(origal_buf+(bak_dir_offset[j]+offset-dir_offset), &dir, sizeof(dir));
            offset += sizeof(dir);
            free(databuf);
            bak_dir_offset_all = bak_dir_offset[j];
            if (offset == bak_dir_offset[j]) {
                break;
            }
        }
    }
    if (offset < bak_dir_offset_all) {
        debugf("runnv encrypt exception");
        return -1;
    }
    return 0;
}
#else
int decryptRunnvPartition(uint8_t* ori_buf){
    uint32_t id = -1;
    uint32_t itemSize = -1;
    uint32_t itemPos = -1;
    uint16_t tmp[2];
    uint8_t *databuf = NULL;
    nv_dir_t dir;
    uint32_t dir_offset = SECTOR_SIZE * 2;
    uint32_t bak_dir_offset = 0;
    uint32_t offset = dir_offset;
    runnv_header_t npb, bak_npb;
    uint32_t printsize = 0;
    uint32_t tempsize = 0;

// get npb data
    memcpy(&npb, ori_buf, sizeof(npb));
#if PRINT_NV_DATA
    debugf("npb: magic=0x%x, timestamp=0x%x, minid=0x%x, maxid=0x%x\n",
        npb.magic, npb.timestamp, npb.minid, npb.maxid);
    debugf("npb: totalSector=0x%x, bytesPerSector=0x%x, dirCount=0x%x, dirSize=0x%x\n",
        npb.totalSector, npb.bytesPerSector, npb.dirCount, npb.dirSize);
// get bak npb data
    memcpy(&bak_npb, ori_buf+SECTOR_SIZE, sizeof(bak_npb));
    debugf("bak_npb: magic=0x%x, timestamp=0x%x, minid=0x%x, maxid=0x%x\n",
        bak_npb.magic, bak_npb.timestamp, bak_npb.minid, bak_npb.maxid);
    debugf("bak_npb: totalSector=0x%x, bytesPerSector=0x%x, dirCount=0x%x, dirSize=0x%x\n",
        bak_npb.totalSector, bak_npb.bytesPerSector, bak_npb.dirCount, bak_npb.dirSize);
#endif
// calu dir offset

// calu bak dir offset
    if((npb.dirCount * npb.dirSize)%SECTOR_SIZE == 0)
        bak_dir_offset = (uint32_t)(npb.dirCount * npb.dirSize) + dir_offset;
    else
        bak_dir_offset = (uint32_t)((npb.dirCount * npb.dirSize)/SECTOR_SIZE) * SECTOR_SIZE + SECTOR_SIZE  + dir_offset;;

    while (offset < bak_dir_offset) {
        if (*(uint16_t *) (ori_buf + offset) == 0) {
            debugf("item size = 0, get next dir\n");
            offset += sizeof(nv_dir_t);
            continue;
        }
        memcpy(&dir, ori_buf+offset, sizeof(dir));
#if PRINT_NV_DATA
        debugf("nv dir:itemSize=0x%x, checksum=0x%x, offset=0x%x, status=0x%x\n",
                dir.itemSize, dir.checksum, dir.offset, dir.status);
#endif
        if (dir.offset + sizeof(tmp) > 0x120000) {
            debugf(" ___findItem Surpass the boundary of the part\r\n");
            break;
        }

        memcpy(tmp, ori_buf + dir.offset, sizeof(tmp));
        id = (uint32_t) tmp[0];
        itemSize = (uint32_t) tmp[1];
        itemPos = dir.offset + sizeof(tmp);
#if PRINT_NV_DATA
        debugf("id = 0x%x, itemSize = 0x%x, itemPos = 0x%x. \n", id, itemSize, itemPos);
#endif
// check id/len/checksum
        if(id < npb.minid || id > npb.maxid){
            debugf("id is invalid\n");
            break;
        }

        if(itemSize != dir.itemSize){
            debugf("itemSize is diff with dir\n");
            break;
        }
//TODO calc checksum
        if(fdl_calc_checksum(ori_buf+itemPos, itemSize) != dir.checksum){
            debugf("nvid :0x%x checksum error from dir", id);
        }
        if (id == 0) {
            offset += sizeof(dir);
            continue;
        }
        databuf = malloc(sizeof(char) *itemSize);
        if(databuf == NULL){
        	debugf("malloc databuf error\n");
        	break;
        }
        memset(databuf, 0x0, itemSize);
#if PRINT_NV_DATA
// get data & encrypt
        printsize = itemSize>32 ? 32 : itemSize;
        debugf("before decrypt \n");
        sprd_hexdump(id, ori_buf+itemPos, printsize);
#endif
        nv_aes_gcm(ori_buf+itemPos, itemSize, databuf, &tempsize);
#if PRINT_NV_DATA
        debugf("after decrypt, output len = 0x%x \n", tempsize);
        sprd_hexdump(id, databuf, printsize);
#endif
        memcpy(ori_buf+itemPos, databuf, itemSize);

// calu checksum afte encrypt
        dir.checksum = fdl_calc_checksum(databuf, itemSize);
        memcpy(ori_buf+offset, &dir, sizeof(dir));
        memcpy(ori_buf+(bak_dir_offset+offset-dir_offset), &dir, sizeof(dir));
        offset += sizeof(dir);
        free(databuf);
    }
    if(offset < bak_dir_offset){
        debugf("runnv encrypt exception");
        return -1;
    }
    return 0;
}
#endif
#endif

#ifdef NV_PROTECT_BACKUP
int check_whether_be_backup(unsigned char *part_name, unsigned char *buf, uint64_t size)
{
    int status, ret;
#ifdef PRINT_NV_DATA
    sprd_hexdump(0, buf, 32);
#endif
    if (g_nv_erase_flag & !g_nv_read_flag) {
    	return 0;// unbackup
    } else {
    	if (!g_nv_read_succ_flag) {
    		debugf("un read back");
    		//read nv in partiton
    		status = _read_repair_nv_img(part_name, (unsigned char *)g_nv_read_back, size, BACKUPNV_CURRENT_SLOT);
    		debugf("read nv %d\n", status);
#ifdef PRINT_NV_DATA
    		sprd_hexdump(1, (unsigned char *) g_nv_read_back, 32);
#endif
    		if (!status || !g_nv_read_back_len) {
    			  debugf("read backup nv error\n");
                  return -1;
    		}
    	}
    	if (mergeItem((unsigned char *)g_nv_read_back, g_nv_read_back_len, buf, size) == TRUE) {
    		return 1;
    	}
    }
    return -2; //mergeitem error
}
#endif

/* clear g_status.unsave_recv_size before return if necessary */
OPERATE_STATUS dl_write_nv_img(const char *part_name, unsigned char *buf,
	uint64_t size)
{
	const char *part_name_bak = NULL;
	char header_buf[NV_HEADER_SIZE];
	nv_header_t *nv_header_p = NULL;
#ifdef CONFIG_ANDROID_AB
	char temp[MAX_PARTITION_NAME_SIZE] = {0};
#endif

	debugf("dl_write_nv_img partition name %s, size 0x%llx\n", part_name, size);
#ifdef NV_ENCRYPTION
	encryptFixnvPartition(buf, size);
#endif
#ifdef NV_PROTECT_BACKUP
	//add check backup nv
#ifdef PRINT_NV_DATA
	sprd_hexdump(2, buf, 32);
#endif
	if (check_whether_be_backup(part_name, buf, size) < 0) {
		debugf("check backup error\n");
		return 0;
	}
#ifdef PRINT_NV_DATA
	sprd_hexdump(3, buf, 32);
#endif
#endif
	memset(header_buf, 0x00, NV_HEADER_SIZE);
	nv_header_p = header_buf;
	nv_header_p->magic = NV_HEAD_MAGIC;
	nv_header_p->len = size;
	nv_header_p->checksum = (uint32_t)fdl_calc_checksum(buf, size);
	debugf("partition %s checkSum 0x%x, len 0x%x\n", part_name, nv_header_p->checksum, nv_header_p->len);
	nv_header_p->version = NV_VERSION;
#ifdef NV_CHECK_WITH_SHA256
	fdl_get_sha256(buf, size, nv_header_p->auth);
#if PRINT_NV_DATA
	sprd_hexdump(1, nv_header_p->auth, 32);
#endif
#endif
#ifndef ODIN_FIXNV2_DOWNLOAD
	debugf("Start to write first block of NV partition(%s)\n", part_name);
	if (0 != common_raw_write(part_name, NV_HEADER_SIZE, (uint64_t)0,
				(uint64_t)0, header_buf))
		return OPERATE_WRITE_ERROR;

	debugf("Start to write remain blocks of NV partition\n");
	if (0 != common_raw_write(part_name, (uint64_t)size, (uint64_t)0,
				NV_HEADER_SIZE, (char *)buf))
		return OPERATE_WRITE_ERROR;
#endif
	/*write the backup partition */
	part_name_bak = _get_backup_partition_name(part_name);
	if (NULL == part_name_bak) {
		errorf(" get backup partition name fail\n");
		return OPERATE_WRITE_ERROR;
	}
	debugf("Start to write first block of NV partition(%s)\n", part_name_bak);
	if (0 != common_raw_write(part_name_bak, NV_HEADER_SIZE, (uint64_t)0, (uint64_t)0, header_buf))
		return OPERATE_WRITE_ERROR;

	debugf("Start to write remain blocks of NV partition\n");
	if (0 != common_raw_write(part_name_bak, (uint64_t)size, (uint64_t)0, NV_HEADER_SIZE, (char *)buf))
		return OPERATE_WRITE_ERROR;

#ifdef CONFIG_ANDROID_AB
	/*write the slot-b and slot-b backup */
	if (!get_ab_partition(temp)) {
		const char *temp_ab = temp;
		if (0 != common_raw_write(temp_ab, NV_HEADER_SIZE, (uint64_t)0, (uint64_t)0, header_buf))
			return OPERATE_WRITE_ERROR;

		if (0 != common_raw_write(temp_ab, (uint64_t)size, (uint64_t)0, NV_HEADER_SIZE, (char *)buf))
			return OPERATE_WRITE_ERROR;

		part_name_bak = _get_backup_partition_name(temp_ab);
		if (NULL == part_name_bak) {
			errorf(" get slotb backup partition name fail\n");
			return OPERATE_WRITE_ERROR;
		}
		if (0 != common_raw_write(part_name_bak, NV_HEADER_SIZE, (uint64_t)0, (uint64_t)0, header_buf))
			return OPERATE_WRITE_ERROR;

		if (0 != common_raw_write(part_name_bak, (uint64_t)size, (uint64_t)0, NV_HEADER_SIZE, (char *)buf))
			return OPERATE_WRITE_ERROR;
	}
#endif
	return OPERATE_SUCCESS;
}

static PARTITION_IMG_TYPE dl_get_partition_image_type(const char *part_name,
	unsigned char *buf, ulong sechdr_offset)
{
	return g_dl_eMMCStatus.curImgType;
}

/**
 * fail return (< 0), otherwise return the remaining count
 */
int dl_image_write(const char *part_name, uint64_t write_size, uint64_t write_offset,
	uint64_t buf_max_size, unsigned char *buf, PARTITION_IMG_TYPE img_format,
	PARTITION_PURPOSE part_purpose)
{
	int retval = 0;
	char temp[32] = {0};
	int v_ab_flag = !get_ab_partition(temp);
	const char *temp_ab = temp;
	int succ;
	debugf("dl_image_write part_name=%s,write_size=0x%llx,buf_max_size=0x%llx\n",
				part_name, write_size, buf_max_size);

	if (IMG_WITH_SPARSE == img_format) {
		succ = 0;
#ifdef CONFIG_WR_SPARSE
		if (NULL != g_env_bootmode && !strcmp(g_env_bootmode, "fastboot")) {/* only fastboot mode */
			debugf("flush the saving of image partition %s\n", part_name);
			retval = wr_sparse_flush();
			if (retval == -1) {
				debugf("wr sparse not enable\n");
			} else if (retval < 0) {
				errorf("%s flush fail\n", part_name);
				return 0;
			} else {
				succ = 1;
				debugf("flush success\n");
			}
		}
#endif
		if (!succ) {
			debugf("Handle the saving of image with sparse,name=%s,buf start at 0x%p,size=0x%llx\n",
				part_name, buf, write_size);

			retval = write_sparse_img(part_name, buf, (ulong)write_size);
			if (-1 == retval) {
				errorf("Write sparse img fail\n");
				return -1;
			}
		}
	} else if (PARTITION_PURPOSE_NV == part_purpose) {
		if (OPERATE_SUCCESS != dl_write_nv_img(part_name, buf, write_size)) {
			errorf("Write nv img fail\n");
			return -2;
		}
	} else {
		if (0 != common_raw_write(part_name, write_size, (uint64_t)0,
						(uint64_t)write_offset, (char *)buf)) {
			errorf("write %s size 0x%llx fail\n", part_name, write_size);
			return -3;
		}
		if (v_ab_flag) {
			debugf("download slot: %s\n", temp_ab);
			if (0 != common_raw_write(temp_ab, (uint64_t)write_size, (uint64_t)0,
							(uint64_t)write_offset, (char *)buf)) {
				errorf("write %s size 0x%llx offset 0x%x fail\n", temp_ab, write_size, 0);
				return -4;
			}
		}
	}

	return retval;
}

OPERATE_STATUS _download_image(void)
{
	const char *part_name = g_dl_eMMCStatus.curUserPartitionName;
	PARTITION_IMG_TYPE img_format = IMG_RAW;
	OPERATE_STATUS status = OPERATE_SUCCESS;
	PARTITION_PURPOSE part_purpose;
	ulong strip_header = 0;
	uint32_t write_size = 0;
	uchar * write_start = NULL;
	int32_t retval = 0;

#if defined (SPRD_SECBOOT)
	/* secboot verify */
	debugf("SECUREBOOT_ENABLE!\n");
	status = dl_secboot_verify(&strip_header,
				part_name,
				g_status.unsave_recv_size, g_status.total_size, g_eMMCBuf);
	if (OPERATE_SUCCESS != status)
		return status;

	if (strncmp(part_name, "vbmeta", 6) ==0) {
		uint64_t total_size = 0;
		if(get_img_partition_size(part_name, &total_size)){
			errorf("get %s partition size fail\n", part_name);
			return OPERATE_WRITE_ERROR;
		}
		if (OPERATE_SUCCESS != dl_erase(part_name, total_size)) {
			errorf("erase vbmeta partition fail\n");
			return OPERATE_WRITE_ERROR;
		}
	}
#endif

	/* verify nv image */
	part_purpose = dl_get_partition_purpose(part_name);
	if ((PARTITION_PURPOSE_NV == part_purpose)
			&& (OPERATE_SUCCESS != dl_check_nv_img())) {
		errorf("Verify NV image fail\n");
		return OPERATE_WRITE_ERROR;
	}

	img_format = dl_get_partition_image_type(part_name, g_eMMCBuf, 0);
	write_size = g_status.unsave_recv_size;
	write_start = g_eMMCBuf;

	/* image write */
	retval = dl_image_write(part_name, write_size, g_dl_eMMCStatus.offset, emmc_buf_size,
				write_start, img_format, part_purpose);
	if (retval < 0) {
		g_status.unsave_recv_size = 0;
		errorf("Write sparse img fail\n");
		return OPERATE_WRITE_ERROR;
	} else if (retval > 0) {
		memmove(g_eMMCBuf, write_start + retval, write_size - retval);
		g_status.unsave_recv_size = write_size - retval;
		debugf("After simg , unsave_recv_size=%lld, saved value=%d\n", g_status.unsave_recv_size, retval);
	} else { /* write success */
	/* backup */
		status = dl_backup(part_name, write_size, write_start);
		if (OPERATE_WRITE_ERROR == status) {
			errorf("Write backup(%s) img fail\n", part_name);
			return status;
		} else {
			status = OPERATE_SUCCESS;
		}

		if (img_format == IMG_RAW) {
			g_dl_eMMCStatus.offset += write_size;
		}
		g_status.unsave_recv_size = 0;
	}
	return status;
}

OPERATE_STATUS regular_download_process(uint32_t size, char *buf)
{
	uint32_t lastSize;
	OPERATE_STATUS ret;
	uint64_t buf_size = emmc_buf_size > SZ_256M ? SZ_256M : emmc_buf_size;

	if (buf_size >= (g_status.unsave_recv_size + size)) {
		memcpy((uchar *) g_sram_addr, buf, size);
		g_sram_addr += size;
		g_status.unsave_recv_size += size;

		if (g_status.total_recv_size == g_status.total_size) {
			ret = _download_image();
			return ret;
		}
	} else {
		lastSize = buf_size - g_status.unsave_recv_size;
		debugf("Unsaved buf size overflow the whole buffer size,lastsize=%u,unsavedsize=%llu\n", lastSize, g_status.unsave_recv_size);
		if (0 != lastSize)
			memcpy((unsigned char *)g_sram_addr, buf, lastSize);

		g_status.unsave_recv_size = buf_size;

		ret = _download_image();
		if (OPERATE_SUCCESS != ret)
			return ret;

		g_sram_addr = (unsigned long) (g_eMMCBuf + g_status.unsave_recv_size);
		memcpy((uchar *) g_sram_addr, (char *)(&buf[lastSize]), size - lastSize);
		g_status.unsave_recv_size += size - lastSize;
		g_sram_addr = (unsigned long) (g_eMMCBuf + g_status.unsave_recv_size);
		debugf("After write,unsaved recv size=%lld\n", g_status.unsave_recv_size);

		if (g_status.total_recv_size == g_status.total_size) {
			ret = _download_image();
			return ret;
		}

	}
	return OPERATE_SUCCESS;
}

#ifdef CONFIG_DL_SKIP_PARTITION
OPERATE_STATUS erase_all_skip_partition(const char *part_name)
{
	int i, ret, total;
	disk_partition_t* info = NULL;
	const char *part_name_bak = NULL;

	info = malloc(sizeof(disk_partition_t) * GPT_ENTRY_NUMBERS);
	if (info == NULL) {
		errorf("No space to malloc info\n");
		return -1;
	}

	memset(info, 0, sizeof(disk_partition_t) * GPT_ENTRY_NUMBERS);

	ret = get_partition_information(info, &total);
	if (ret) {
		errorf("get partition information error!\n");
		free(info);
		return -1;
	}

	dprintf(INFO,"Get total partition nums:%d\n", total);
	for (i = 0; i < total; i++) {
		if (PARTITION_DOWNLOAD == dl_get_partition_skip(info[i].part_name)) {
			if(0 != common_raw_erase(info[i].part_name, 0, 0)){
				errorf("erase partition %s failed\n", info[i].part_name);
				return -1;
			}

			part_name_bak = _get_backup_partition_name(info[i].part_name);

			if (NULL != part_name_bak) {
				if (0 != common_raw_erase(part_name_bak, 0, 0))
					return OPERATE_WRITE_ERROR;
			}

			if (0 == strcmp(info[i].part_name, UBOOT_LOG_PARTITION)) {
			/* in download mode, initialize log if uboot log partition is erased */
				reinit_write_log();
			}
		} else {
			debugf("Do not erase partition %s!\n", info[i].part_name);
		}
	}
	dprintf(INFO,"Erase partition nums:%d\n", i);
	return 0;
}
#endif

OPERATE_STATUS dl_erase(const char *part_name, uint64_t size)
{
	const char *part_name_bak = NULL;

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
		/*
		 * operation was not allow when requirement was not from edl,
		 * or factory download
		 */
		if (dl_check_reboot_edl()) {
			return OPERATE_SYSTEM_ERROR; /* return BSL_REP_OPERATION_FAILED to pctool */
		}
#endif

	if ((0 == strcmp(part_name, "erase_all")) && (0xffffffff == size)) {
		debugf("Erase all!\n");
#ifdef NV_PROTECT_BACKUP
		g_nv_erase_flag = 1;
#endif
#ifndef CONFIG_DL_SKIP_PARTITION
		if (0 != common_raw_erase(part_name, 0, 0))
#else
		if (0 != erase_all_skip_partition(part_name))
#endif
			return OPERATE_WRITE_ERROR;
	}

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
	else if(0 == strcmp(part_name, "config")){
		part_name = "persist";
		if(0 != common_raw_erase(part_name, size, 0)){
			return OPERATE_WRITE_ERROR;
		}
	}
#endif

	else {
#ifdef CONFIG_DL_SKIP_PARTITION
		if (PARTITION_DOWNLOAD == dl_get_partition_skip(part_name)) {
#endif
			debugf("erase partition %s!\n", part_name);

			if (0 != common_raw_erase(part_name, size, 0)) {
				return OPERATE_WRITE_ERROR;
			}

			part_name_bak = _get_backup_partition_name(part_name);

			if (NULL != part_name_bak) {
				if (0 != common_raw_erase(part_name_bak, size, 0))
					return OPERATE_WRITE_ERROR;
			}

			if (0 == strcmp(part_name, UBOOT_LOG_PARTITION)) {
			/* in download mode, initialize log if uboot log partition is erased */
				reinit_write_log();
			}
#ifdef CONFIG_DL_SKIP_PARTITION
		} else {
			dprintf(ALWAYS, "Do not support erase partition %s!\n", part_name);
		}
#endif
	}
	return OPERATE_SUCCESS;
}

static int fixnv_ab_status = 0;
static int partition_fixnv_ab_exist_check(char *partition_name, uint16_t flag)
{
	char write_temp[4] = {0};
	char temp[MAX_PARTITION_NAME_SIZE] = {0};
	char partition_without_ab[MAX_PARTITION_NAME_SIZE] = {0};
	char partition_with_ab[MAX_PARTITION_NAME_SIZE] = {0};
	size_t string_size = 0;
	uint64_t size = 0;

	string_size = strlen(g_dl_eMMCStatus.curUserPartitionName);
	/* Used to check whether the pac fixnv is used vab partition */
	if (!get_ab_partition(temp)) {
		strncpy(partition_without_ab, g_dl_eMMCStatus.curUserPartitionName, string_size - strlen("_a"));
		strncpy(partition_with_ab, g_dl_eMMCStatus.curUserPartitionName, string_size);
		fixnv_ab_status |= PAC_WITH_SLOT_AB;
	} else {
		strncpy(partition_without_ab, g_dl_eMMCStatus.curUserPartitionName, string_size);
		snprintf(partition_with_ab, string_size + 3, "%s%s", g_dl_eMMCStatus.curUserPartitionName, "_a");
		fixnv_ab_status |= PAC_WITHOUT_SLOT_AB;
	}
	/* Used to check whether the local partition fixnv is used vab */
	if (get_img_partition_size(partition_with_ab, &size) != 0) {
		fixnv_ab_status |= DEVICE_WITHOUT_SLOT_AB;
	} else {
		fixnv_ab_status |= DEVICE_WITH_SLOT_AB;
		debugf("feature_flag = 0x%x\n", flag);
		if (BACKUPNV_CURRENT_SLOT == flag) {
			debugf("enter get current ab feature!\n");
			get_slot_ab(partition_with_ab, partition_without_ab);
		}
	}
	switch(fixnv_ab_status) {
		case PAC_WITH_SLOT_AB | DEVICE_WITH_SLOT_AB:
			debugf("fixnv download status:PAC_WITH_SLOT_AB | DEVICE_WITH_SLOT_AB\n");
			strncpy(partition_name, partition_with_ab, sizeof(partition_with_ab));
			break;
		case PAC_WITH_SLOT_AB | DEVICE_WITHOUT_SLOT_AB:
			debugf("fixnv download status:PAC_WITH_SLOT_AB | DEVICE_WITHOUT_SLOT_AB\n");
			strncpy(partition_name, partition_without_ab, sizeof(partition_without_ab));
			break;
		case PAC_WITHOUT_SLOT_AB | DEVICE_WITH_SLOT_AB:
			debugf("fixnv download status:PAC_WITHOUT_SLOT_AB | DEVICE_WITH_SLOT_AB\n");
			strncpy(partition_name, partition_with_ab, sizeof(partition_with_ab));
			break;
		case PAC_WITHOUT_SLOT_AB | DEVICE_WITHOUT_SLOT_AB:
			debugf("fixnv download status:PAC_WITHOUT_SLOT_AB | DEVICE_WITHOUT_SLOT_AB\n");
			strncpy(partition_name, partition_without_ab, sizeof(partition_without_ab));
			break;
		default:
			errorf("Cannot be analyzed the status of fixnv partition\n");
			return -1;
	}
	return 0;
}

static void decode_packet_data_nv(dl_packet_t *packet, uint16_t *flag)
{
	uint16_t *data = (uint16_t *) (packet->body.content);
	/* legacy format */
	if (packet->body.size < BIT32_IDLEN_DATA_LENGTH) {
		*flag = 0;
		return;
	}

	debugf("*(data+70):0x%x\n", *(data + TOOL_FALG_OFFSET));
	if ('\0' != *(data + TOOL_FALG_OFFSET)) {
		*flag = *(data + TOOL_FALG_OFFSET);
		debugf("set nv slot flag = 0x%x\n", *flag);
	}
}

int32_t _read_repair_nv_img(const char * partition_name, uchar * buf, uint32_t image_size, uint16_t flag)
{
	const char *backup_partition_name = NULL;
	char header_buf[NV_HEADER_SIZE] = {0};
	char backup_header_buf[NV_HEADER_SIZE] = {0};
	nv_header_t *header_p = header_buf;
	nv_header_t *backup_header_p = backup_header_buf;
	char *backup_nv_buf = NULL;
	int status = ORIGIN_BACKUP_NV_OK;
	int ret = 0;
#ifdef NV_PROTECT_BACKUP
	g_nv_read_flag = 1;
#endif

	uint32_t data_size = image_size;
	char partition_check[MAX_PARTITION_NAME_SIZE] = {0};
	backup_nv_buf = (char *)buf + SZ_16M;

#ifdef CONFIG_ANDROID_AB
	if(strstr(partition_name, "fixnv")!= NULL){
		if  (0 != partition_fixnv_ab_exist_check(partition_check, flag)){
			return -1;
		}
	}else{
        	strncpy(partition_check, partition_name, sizeof(partition_check)-1);
        }
#else
	strncpy(partition_check, partition_name, sizeof(partition_check));
#endif
	debugf("repair nv img with partition : %s\n", partition_check);

	do {
		/*read origin image header */
		if (0 != common_raw_read(partition_check, NV_HEADER_SIZE, (uint64_t)0, header_buf)) {
			errorf("read origin nv image header failed\n");
			status |= ORIGIN_NV_DAMAGED;
			break;
		}

		if (NV_HEAD_MAGIC != header_p->magic) {
			errorf("nv header magic error, wrong magic=0x%x\n", header_p->magic);
			status |= ORIGIN_NV_DAMAGED;
			break;
		}

		/*read origin image */
		if (0 != common_raw_read(partition_check, (uint64_t)(header_p->len), NV_HEADER_SIZE, (char *)buf)) {
			errorf("read origin nv image failed\n");
			status |= ORIGIN_NV_DAMAGED;
			break;
		}
#ifdef NV_CHECK_WITH_SHA256
		/*check sha256 */
		if (fdl_check_sha256(buf, header_p->len, header_p->auth)) {
#else
		/*check crc */
		if (fdl_check_crc(buf, header_p->len, header_p->checksum)) {
#endif
			data_size = header_p->len;
			debugf("read origin nv image success and crc correct\n");
		} else {
			errorf("check origin nv image crc wrong\n");
			status |= ORIGIN_NV_DAMAGED;
		}
	} while(0);

	/*get the backup partition name */
	backup_partition_name = _get_backup_partition_name(partition_check);
	debugf("partition_name is %s\n", partition_check);
	if (NULL == backup_partition_name){
#ifdef NV_ENCRYPTION
		if(strstr(partition_check, "fix")!= NULL){
			decryptFixnvPartition(buf, data_size);
		}else if(strstr(partition_check, "run")!= NULL){
			decryptRunnvPartition(buf);
		}
		debugf("partition_name is %s,decrypt success\n", partition_name);
#endif
		return 0;
	}

	do {
		/*read backup header */
		if (0 != common_raw_read(backup_partition_name, NV_HEADER_SIZE, (uint64_t)0, backup_header_buf)) {
			errorf("read backup nv image header failed\n");
			status |= BACKUP_NV_DAMAGED;
			break;
		}

		if (NV_HEAD_MAGIC != backup_header_p->magic) {
			errorf("backup nv header magic error, wrong magic=0x%x\n", backup_header_p->magic);
			status |= BACKUP_NV_DAMAGED;
			break;
		}

		/*read bakup image */
		if (0 != common_raw_read(backup_partition_name, (uint64_t)(backup_header_p->len), NV_HEADER_SIZE, backup_nv_buf)) {
			errorf("read backup nv image failed\n");
			status |= BACKUP_NV_DAMAGED;
			break;
		}
#ifdef NV_CHECK_WITH_SHA256
		/*check sha256 */
		if (fdl_check_sha256(buf, header_p->len, header_p->auth)) {
#else
		/*check crc */
		if (fdl_check_crc(backup_nv_buf, backup_header_p->len, backup_header_p->checksum)) {
#endif
			data_size = backup_header_p->len;
			debugf("read backup nv image success and crc correct\n");
		} else {
			errorf("check backup nv image crc wrong\n");
			status |= BACKUP_NV_DAMAGED;
		}
	} while(0);

	switch (status) {
	case ORIGIN_BACKUP_NV_OK :
		debugf("both org and bak nv partition are ok\n");
		ret = 1;
		break;
	case ORIGIN_NV_DAMAGED :
		memcpy(buf, backup_nv_buf, backup_header_p->len);
		if (0 != common_raw_write(partition_check, NV_HEADER_SIZE, (uint64_t)0, (uint64_t)0, backup_header_buf)) {
			ret = 0;
			break;
		}
		debugf("Start to write remain blocks of origin NV partition\n");
		if (0 != common_raw_write(partition_check, (uint64_t)(backup_header_p->len), (uint64_t)0, NV_HEADER_SIZE, backup_nv_buf)) {
			ret = 0;
			break;
		}
		ret = 1;
		break;
	case BACKUP_NV_DAMAGED :
		if (0 != common_raw_write(backup_partition_name, NV_HEADER_SIZE, (uint64_t)0, (uint64_t)0, header_buf)) {
			ret = 0;
			break;
		}
		debugf("Start to write remain blocks of backup NV partition\n");
		if (0 != common_raw_write(backup_partition_name, (uint64_t)(header_p->len), (uint64_t)0, NV_HEADER_SIZE, (char *)buf)) {
			ret = 0;
			break;
		}
		ret = 1;
		break;
	case ORIGIN_NV_DAMAGED | BACKUP_NV_DAMAGED :
		errorf("both org and bak partition are damaged!\n");
		ret = 0;
		break;
	}
	debugf("partition_name is %s, ret = %d\n", partition_name, ret);
#ifdef NV_ENCRYPTION
	if(strstr(partition_name, "fixnv")!= NULL){
		decryptFixnvPartition(buf, data_size);
	}else if(strstr(partition_name, "runtimenv")!= NULL){
		decryptRunnvPartition(buf);
	}
	debugf("partition_name is %s,decrypt success\n", partition_name);
#endif
#ifdef NV_PROTECT_BACKUP
	if (ret) {
		g_nv_read_back_len = (status & 0x10) ? ((uint64_t)(backup_header_p->len)):((uint64_t)(header_p->len));//select header or backup header
		//copy buf to internal ptr
		if (buf != g_nv_read_back) {
			memcpy((unsigned char *)g_nv_read_back, buf, (uint64_t)(g_nv_read_back_len));
		} else {
			debugf("same addr, skip\n");
		}
		g_nv_read_succ_flag = 1;
	}
#endif
	return ret;
}

OPERATE_STATUS dl_read_start(uchar * partition_name, uint64_t size, dl_packet_t *packet)
{
	int ret;
	size_t sblock_size = 0;
	struct ext2_sblock *sblock = NULL;
	uint16_t flag = 0;

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
	/*
	 * operation was not allow when requirement was not from edl,
	 * or factory download
	 */
	if (dl_check_reboot_edl()) {
		return OPERATE_SYSTEM_ERROR; /* return BSL_REP_OPERATION_FAILED to pctool */
	}
#endif

	strncpy(g_dl_eMMCStatus.curUserPartitionName, partition_name, sizeof(g_dl_eMMCStatus.curUserPartitionName)-1);
	/*get special partition attr */
	_get_partition_attribute(partition_name);

	if (PARTITION_PURPOSE_NV == g_dl_eMMCStatus.partitionpurpose) {
		decode_packet_data_nv(packet, &flag);
		if (!_read_repair_nv_img(g_dl_eMMCStatus.curUserPartitionName, g_eMMCBuf, size, flag))
			return OPERATE_SYSTEM_ERROR;
	}


	if (0 == strcmp("prodnv", g_dl_eMMCStatus.curUserPartitionName)) {
		sblock_size = sizeof(struct ext2_sblock);
		sblock = malloc_cache_aligned(sblock_size);
		if (NULL == sblock) {
			errorf("malloc sblock failed\n");
			ret = OPERATE_MALLOC_ERROR;
			goto err;
		}
		memset(sblock, 0, sblock_size);
		if (0 != common_raw_read(g_dl_eMMCStatus.curUserPartitionName, (uint64_t)sblock_size, (uint64_t)1024, (char *)sblock)) {
			errorf("read prodnv super block fail\n");
			ret = OPERATE_READ_ERROR;
			goto err;
		}

		if (sblock->magic != EXT2_MAGIC) {
			errorf("bad prodnv image magic(0x%x)\n", sblock->magic);
			ret = OPERATE_MAGIC_ERROR;
			goto err;
		}
		free(sblock);
	}

	return OPERATE_SUCCESS;
err:
	if (NULL != sblock)
		free(sblock);
	return ret;
}

OPERATE_STATUS dl_read_midst(uint32_t size, uint64_t off, uchar * buf)
{
#ifdef CONFIG_DL_SKIP_PARTITION
	if (PARTITION_DOWNLOAD == dl_get_partition_skip(g_dl_eMMCStatus.curUserPartitionName)) {
#endif
		if (PARTITION_PURPOSE_NV == g_dl_eMMCStatus.partitionpurpose) {
			memcpy(buf, (uchar *) (g_eMMCBuf + off), size);
		} else {
			if (0 != common_raw_read(g_dl_eMMCStatus.curUserPartitionName, (uint64_t)size, (uint64_t)off, (char *)buf)) {
				errorf("read error!\n");
				return OPERATE_READ_ERROR;
			}
		}
#ifdef CONFIG_DL_SKIP_PARTITION
	} else {
		dprintf(ALWAYS, "Do not support read partition %s!\n", g_dl_eMMCStatus.curUserPartitionName);
	}
#endif

	return OPERATE_SUCCESS;
}

OPERATE_STATUS dl_read_end(void)
{
	return OPERATE_SUCCESS;
}

OPERATE_STATUS dl_download_start(uchar * partition_name, uint64_t size, uint32_t nv_checksum)
{
	g_status.total_size = size;
	strncpy(g_dl_eMMCStatus.curUserPartitionName, partition_name, sizeof(g_dl_eMMCStatus.curUserPartitionName)-1);
	g_dl_eMMCStatus.offset = 0;

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
	/*
	 * operation was not allow when requirement was not from edl,
	 * or factory download
	 */
	if (dl_check_reboot_edl()) {
		return OPERATE_SYSTEM_ERROR; /* return BSL_REP_OPERATION_FAILED to pctool */
	}
#endif

	/*get special partition attr */
	_get_partition_attribute(partition_name);

	if (PARTITION_PURPOSE_NV == g_dl_eMMCStatus.partitionpurpose) {
		debugf("partition purpose is NV\n");
		if (size > FIXNV_SIZE) {
			errorf("size(0x%llx) beyond FIXNV_SIZE:0x%x !\n", size, FIXNV_SIZE);
			return OPERATE_INVALID_SIZE;
		}
		memset(g_eMMCBuf, 0xff, FIXNV_SIZE + NV_HEADER_SIZE);
		g_checksum = nv_checksum;
	} else if (0 == strcmp("splloader", partition_name)) {
		memset(g_eMMCBuf, 0xff, SPL_CHECKSUM_LEN);
	}

	g_status.total_recv_size = 0;
	g_status.unsave_recv_size = 0;
	g_sram_addr = (unsigned long) g_eMMCBuf;

	prepare_alternative_buffers();
#ifdef SPRD_SPARSE_SUPER_SPEEDUP
	prepare_sparse_temp_buf();
	prepare_emmc_write_buf();
#endif
	return OPERATE_SUCCESS;
}

OPERATE_STATUS dl_download_midst(uint32_t size, char *buf)
{
	int ret = 0;
	ulong sec_offset = 0;
	uint64_t total_size = 0;
	sparse_header_t sparse_header_test_head;
#ifdef CONFIG_DL_SKIP_PARTITION
	if (PARTITION_DOWNLOAD == dl_get_partition_skip(g_dl_eMMCStatus.curUserPartitionName)  || !is_fill_flash) {
		dprintf(ALWAYS, "download partition name:%s, is_fill_flash:%d\n", g_dl_eMMCStatus.curUserPartitionName, is_fill_flash);
#endif
		/*adjust the image type via the header magic*/
		if (0 == g_status.total_recv_size ) {
#ifdef SPRD_SECBOOT
			if (IMG_BAK_HEADER == ((sys_img_header *)buf)->mMagicNum)
				sec_offset = SYS_HEADER_SIZE;
#endif
			memset(&sparse_header_test_head, 0, sizeof(sparse_header_t));
			memcpy(&sparse_header_test_head, buf + sec_offset, sizeof(sparse_header_test_head));

			if (sparse_header_test_head.magic != SPARSE_HEADER_MAGIC)
				g_dl_eMMCStatus.curImgType = IMG_RAW;
			else
				g_dl_eMMCStatus.curImgType = IMG_WITH_SPARSE;

			if (!strcmp(g_dl_eMMCStatus.curUserPartitionName, "userdata")
				&&!get_img_partition_size(g_dl_eMMCStatus.curUserPartitionName, &total_size) && g_download_part_count == 0) {
				debugf("userdata img_format(%d),erase size (%lld/100)\n", g_dl_eMMCStatus.curImgType, total_size);
				common_raw_erase(g_dl_eMMCStatus.curUserPartitionName, total_size / 100, (uint64_t)0LL);
				g_download_part_count += 1;
			}
		}

		g_status.total_recv_size += size;

#if defined (SPRD_SECBOOT)

#ifdef CONFIG_SYSTEM_VERIFY
		ret = regular_download_process(size, buf);
#else
		if ((IMG_RAW == g_dl_eMMCStatus.curImgType) &&
					((0 == strcmp("system", g_dl_eMMCStatus.curUserPartitionName)) || (0 == strcmp("super", g_dl_eMMCStatus.curUserPartitionName))))
			ret = speedup_download_process(size, buf);
#ifdef SPRD_SPARSE_SUPER_SPEEDUP
		else if ((IMG_WITH_SPARSE == g_dl_eMMCStatus.curImgType) && (0 == strcmp("super", g_dl_eMMCStatus.curUserPartitionName)))
		{
			//dprintf(ALWAYS, "enter speedup sparse super download!\n");
			ret = sparse_download_process(size, buf);
		}
#endif
		else
			ret = regular_download_process(size, buf);
#endif

#else
		if ((IMG_RAW == g_dl_eMMCStatus.curImgType)
			&& ((PARTITION_PURPOSE_NV != g_dl_eMMCStatus.partitionpurpose)
			&& (g_status.total_size > ALTERNATIVE_BUFFER_SIZE)))
			ret = speedup_download_process(size, buf);
#ifdef SPRD_SPARSE_SUPER_SPEEDUP
		else if ((IMG_WITH_SPARSE == g_dl_eMMCStatus.curImgType) && (0 == strcmp("super", g_dl_eMMCStatus.curUserPartitionName)))
		{
			//dprintf(ALWAYS, "enter speedup sparse super download!\n");
			ret = sparse_download_process(size, buf);
		}
#endif
		else
			ret = regular_download_process(size, buf);
#endif

		return ret;
#ifdef CONFIG_DL_SKIP_PARTITION
	} else {
		dprintf(ALWAYS, "is_fill_flash:%d, %s partition skip download!\n", is_fill_flash, g_dl_eMMCStatus.curUserPartitionName);
		return OPERATE_SUCCESS;
	}
#endif

}

OPERATE_STATUS dl_download_flush_data(uint32_t total_len, unsigned char *total_buf,
						uint16_t size, unsigned char *buf)
{
#ifndef CONFIG_ZEBU
	unsigned char *pdata = (unsigned char *)total_buf;
	unsigned char hash[32];

	if (0x20 != size)
		return OPERATE_INVALID_SIZE;

	sha256_csum_wd((const unsigned char*)pdata, (uint32_t)total_len, hash, 0);

	if (memcmp(hash, buf, 32))
		return OPERATE_CHECKSUM_DIFF;
#endif
	return OPERATE_SUCCESS;
}

OPERATE_STATUS dl_download_end(void)
{
	if(!strcmp(g_dl_eMMCStatus.curUserPartitionName,"miscdata")){
#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
		/* restore reboot-edl flags if necessary */
		if (!dl_check_reboot_edl()) {
			if (!fb_check_secboot_enable() && fb_require_reboot_edl(1)) {
				errorf("restore reboot edl flags fail\n");
				return OPERATE_SYSTEM_ERROR;
			}
		}
#endif
		if(0 != common_raw_erase(g_dl_eMMCStatus.curUserPartitionName, DATETIME_LEN, DATETIME_OFFSET))
			return OPERATE_WRITE_ERROR;
		if(0 != common_raw_erase(g_dl_eMMCStatus.curUserPartitionName, DEBUG_INFO_LEN, DEBUG_INFO_OFFSET))
			return OPERATE_WRITE_ERROR;
	}
	if (g_status.unsave_recv_size != 0) {
		debugf("unsaved size is not zero!\n");
		return OPERATE_SYSTEM_ERROR;
	}

	g_status.total_size = 0;

	if (0 != common_partition_sync(g_dl_eMMCStatus.curUserPartitionName))
		return OPERATE_WRITE_ERROR;

	return OPERATE_SUCCESS;
}

OPERATE_STATUS dl_repartition(uchar * partition_cfg, uint16_t total_partition_num,
									uchar version, uchar size_unit)
{
	disk_partition_t *partition_info;
	int32_t res = 0;

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
	/*
	 * operation was not allow when requirement was not from edl,
	 * or factory download
	 */
	if (dl_check_reboot_edl()) {
		return OPERATE_SYSTEM_ERROR; /* return BSL_REP_OPERATION_FAILED to pctool */
	}
#endif

	/*prepare mem to store <total_partition_num> partitions' info */
	partition_info = malloc(sizeof(disk_partition_t) * total_partition_num);
	if (NULL == partition_info) {
		errorf("No space to store partition_info!\n");
		return OPERATE_MALLOC_ERROR;
	}

	res = _parser_repartition_cfg(partition_info, partition_cfg, total_partition_num, version, size_unit);
	if (res < 0) {
		free(partition_info);
		return OPERATE_REPARTITION_ERROR;
	}

	res = common_repartition(partition_info, (int)total_partition_num);
	free(partition_info);

	if (0 == res)
		return OPERATE_SUCCESS;
	else
		return OPERATE_REPARTITION_ERROR;
}

OPERATE_STATUS dl_read_ref_info(const char *part_name, uint16_t size, uint64_t offset,
	char *receive_buf, char  *transmit_buf)
{
	if(0 != common_raw_read(part_name, (uint64_t)size, offset, transmit_buf)) {
		/* read old info from flash fail, transmit the receive info back to tool */
		debugf("read old info from flash fail\n");
		memcpy(transmit_buf, receive_buf, (size_t)size);
	}
	/* update receive ref info to flash */
	if(0 != common_raw_write(part_name, (uint64_t)size, (uint64_t)0, offset, receive_buf)){
		debugf("update receive ref info to flash fail\n");
		return  OPERATE_WRITE_ERROR;
	}
	return OPERATE_SUCCESS;
}

OPERATE_STATUS dl_record_pacdatetime(char * buf, uint64_t size)
{
	if(0 != common_raw_write(DATATIME_PARTNAME, size, (uint64_t)0, DATETIME_OFFSET, buf)){
		return OPERATE_WRITE_ERROR;
	}
	return OPERATE_SUCCESS;
}

#ifdef CONFIG_EMMC_DDR_CHECK_TYPE
static uint32_t flash_ddr_size_tab[] = {128, 256, 512, 1*1024, 2*1024, 3*1024, 4*1024, \
                6*1024, 8*1024, 16*1024, 32*1024, 64*1024, 128*1024, 256*1024, 512*1024};

uint32_t get_correct_size(uint32_t size)
{
	uint32_t i = 0;
	uint32_t diff_size = 50;
	uint32_t read_size = size;
	int diff = 0;

	for(i = 0; i < sizeof(flash_ddr_size_tab)/sizeof(uint32_t); i++) {
		if(i < 3)
			diff_size = 50;
		else diff_size = 100;
		if(flash_ddr_size_tab[i]) {
			diff = flash_ddr_size_tab[i] - size;
			if((diff > 0 ? diff : -diff ) < diff_size) {
				read_size = flash_ddr_size_tab[i];
				break;
			}

		}
	}
	dprintf(INFO,"correct size:%u\r\n", read_size);
	return read_size;
}

OPERATE_STATUS dl_get_flashtype(uchar * content, uint16_t * size)
{
	struct FLASH_TYPE flash_type;
	char temp[sizeof(struct FLASH_TYPE)];
	int i = 0;
	int j = 0;

	memset(&flash_type, 0, sizeof(struct FLASH_TYPE));

	/* dram size */
	int dram_size_inMB = 0;
	dprintf(INFO,"get real ram size: 0x%lx\n", get_real_ram_size());
	dram_size_inMB = ((get_real_ram_size()/1024/1024)+5);
	dprintf(INFO,"dram_size_inMB:%u\n", dram_size_inMB);
	flash_type.mid = get_correct_size(dram_size_inMB);

	/* calculate flash size in GB */
	int mem_size = 0, size_inMB = 0;
	if (get_bootdevice() == BOOT_DEVICE_EMMC) {
		mem_size = emmc_get_capacity(PARTITION_USER)/1000/1000;
		size_inMB = (mem_size + 500)/1000 * 1024;
		debugf("emmc_size:%llu Byte, emmc_size_inMB:%d\n", emmc_get_capacity(PARTITION_USER), size_inMB);
	}
#ifdef CONFIG_UFS
	else if (get_bootdevice() == BOOT_DEVICE_UFS) {
		mem_size = ufs_info.dev_total_cap * 512 / 1000 / 1000;
		size_inMB = (mem_size + 500) / 1000 * 1024;
		debugf("ufs_size:%llu,emmc_size_inMB:%d\n", ufs_info.dev_total_cap * 512, size_inMB);
	}
#endif

	/* did field is used for flash size */
	flash_type.did = get_correct_size(size_inMB);

	*size = sizeof(struct FLASH_TYPE);
	memcpy(temp, &flash_type, *size);
	for (i = 0; i < *size; i++) {
		if ((i % 4) == 0)
			j = i + 4;
		content[i] = temp[--j];
	}
	return 1;
}
#endif
