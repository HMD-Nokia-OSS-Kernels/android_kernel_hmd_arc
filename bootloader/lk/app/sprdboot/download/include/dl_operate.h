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

#ifndef _FDL_EMMC_OPERATE_H
#define _FDL_EMMC_OPERATE_H

#include "dl_cmd_proc.h"

#define IMG_BAK_HEADER 0x42544844

#define NV_HEADER_SIZE            (512)

#define F2FS_SUPER_MAGIC	0xF2F52010
#define SECTOR_SIZE		0x200
#define EXT2_MAGIC		0xEF53

#define ORIGIN_BACKUP_NV_OK 0x0
#define ORIGIN_NV_DAMAGED      0x1
#define BACKUP_NV_DAMAGED   0x2

#define MAGIC_DATA	0xAA55A5A5

/* used for function partition_fixnv_ab_exist_check */
#define PAC_WITHOUT_SLOT_AB     0x1
#define PAC_WITH_SLOT_AB        0x2
#define DEVICE_WITHOUT_SLOT_AB  0x4
#define DEVICE_WITH_SLOT_AB     0x8

/*according to standard ,the first usable LBA is 34,
but actually reserve 1MB space before the first partition can enhance the erase speed*/
#define FIRST_USABLE_LBA_FOR_PARTITION    (34)

#define MAX_SIZE_FLAG	0xFFFFFFFF

#define ALTERNATIVE_BUFFER_SIZE  0x200000

#define TRUE   1		/* Boolean true value. */
#define FALSE  0		/* Boolean false value. */

#define DATATIME_PARTNAME "miscdata"

typedef enum _PARTITION_IMG_TYPE {
	IMG_RAW = 0,
	IMG_WITH_SPARSE = 1,
	IMG_TYPE_MAX
} PARTITION_IMG_TYPE;

typedef enum _PARTITION_PURPOSE {
	PARTITION_PURPOSE_NORMAL,
	PARTITION_PURPOSE_NV,
	PARTITION_PURPOSE_MAX
} PARTITION_PURPOSE;

typedef struct DL_EMMC_STATUS_TAG {
	uint64_t offset;
	const char curUserPartitionName[32];
	PARTITION_PURPOSE partitionpurpose;
	PARTITION_IMG_TYPE curImgType;
} DL_EMMC_STATUS;

typedef struct DL_FILE_STATUS_TAG {
	uint64_t total_size;
	uint64_t total_recv_size;
	uint64_t unsave_recv_size;
} DL_EMMC_FILE_STATUS;

typedef struct _SPECIAL_PARTITION_CFG {
	const char *partition;
	const char *bak_partition;
	PARTITION_IMG_TYPE imgattr;
	PARTITION_PURPOSE purpose;
} SPECIAL_PARTITION_CFG;

#ifdef CONFIG_DL_SKIP_PARTITION
typedef enum _PARTITION_SKIP {
	PARTITION_DOWNLOAD,
	PARTITION_SKIP_DOWNLOAD,
	PARTITION_SKIP_MAX
} PARTITION_SKIP;

typedef struct _SPECIAL_PARTITION_SKIP {
	const char *partition;
	int skip;
} SPECIAL_PARTITION_SKIP;
#endif

typedef struct {
	uint32_t version;
	uint32_t magicData;
	uint32_t checkSum;
	uint32_t hashLen;
} EMMC_BootHeader;

typedef struct _ALTER_BUFFER_ATTR {
	uchar* addr;
	uchar* pointer;
	uint32_t size;
	uint32_t used;
	uint32_t fixed;
	uint32_t spare;
	uint32_t status;
	struct _ALTER_BUFFER_ATTR* next;
} ALTER_BUFFER_ATTR;

typedef enum _ALTERNATIVE_BUFFER_STATUS {
	BUFFER_CLEAN,
	BUFFER_DIRTY
} ALTERNATIVE_BUFFER_STATUS;

typedef struct FLASH_TYPE {
	int mid;/* as ddr size */
	int did;/* as flash size */
	int eid;/* reserved */
	int flag;
} FLASH_T;

struct ext2_sblock {
	uint32_t total_inodes;	/* Inodes count */
	uint32_t total_blocks;	/* Blocks count */
	uint32_t reserved_blocks;	/* Reserved blocks count */
	uint32_t free_blocks;	/* Free blocks count */
	uint32_t free_inodes;	/* Free inodes count */
	uint32_t first_data_block;	/* First Data Block */
	uint32_t log2_block_size;	/* Block size */
	uint32_t log2_fragment_size;	/* Fragment size */
	uint32_t blocks_per_group;	/* # Blocks per group */
	uint32_t fragments_per_group;	/* # Fragments per group */
	uint32_t inodes_per_group;	/* # Inodes per group */
	uint32_t mtime;	/* Mount time */
	uint32_t utime;	/* Write time */
	uint16_t mnt_count;	/* Mount count */
	uint16_t max_mnt_count;	/* Maximal mount count */
	uint16_t magic;	/* Magic signature */
	uint16_t fs_state;	/* File system state */
	uint16_t error_handling;	/* Behaviour when detecting errors */
	uint16_t minor_revision_level;	/* minor revision level */
	uint32_t lastcheck;	/* time of last check */
	uint32_t checkinterval;	/* max. time between checks */
	uint32_t creator_os;	/* OS */
	uint32_t revision_level;	/* Revision level */
	uint16_t uid_reserved;	/* Default uid for reserved blocks */
	uint16_t gid_reserved;	/* Default gid for reserved blocks */
	uint32_t first_inode;	/* First non-reserved inode */
	uint16_t inode_size;	/* size of inode structure */
	uint16_t block_group_number;	/* block group # of this superblock */
	uint32_t feature_compatibility;	/* compatible feature set */
	uint32_t feature_incompat;	/* incompatible feature set */
	uint32_t feature_ro_compat;	/* readonly-compatible feature set */
	uint32_t unique_id[4];	/* 128-bit uuid for volume */
	char volume_name[16];	/* volume name */
	char last_mounted_on[64];	/* directory where last mounted */
	uint32_t compression_info;
};

OPERATE_STATUS dl_download_start(uchar * partition_name, uint64_t size, uint32_t nv_checksum);
OPERATE_STATUS dl_download_midst(uint32_t size, char *buf);
OPERATE_STATUS dl_download_end(void);
OPERATE_STATUS dl_read_start(uchar * partition_name, uint64_t size, dl_packet_t *packet);
OPERATE_STATUS dl_read_midst(uint32_t size, uint64_t off, uchar * buf);
OPERATE_STATUS dl_read_end(void);
OPERATE_STATUS dl_erase(const char *part_name, uint64_t size);
OPERATE_STATUS dl_repartition(uchar * partition_cfg, uint16_t total_partition_num,
									uchar version, uchar size_unit);
OPERATE_STATUS dl_read_ref_info(const char *part_name, uint16_t size, uint64_t offset,
	char *receive_buf, char *transmit_buf);

OPERATE_STATUS dl_record_pacdatetime(char * buf, uint64_t size);
#ifdef CONFIG_EMMC_DDR_CHECK_TYPE
OPERATE_STATUS dl_get_flashtype(uchar * content, uint16_t * size);
#endif
//#ifdef CONFIG_DTS_MEM_LAYOUT
int set_buf_base_size(void);
//#endif

OPERATE_STATUS dl_download_flush_data(uint32_t total_len, unsigned char *total_buf, uint16_t size, unsigned char *buf);

PARTITION_PURPOSE dl_get_partition_purpose(const char *part_name);
OPERATE_STATUS dl_secboot_verify(ulong *strip, const char *part_name,
	uint64_t rcv_size, uint64_t total_size, unsigned char *buf);
#ifdef CONFIG_DL_SKIP_PARTITION
PARTITION_SKIP dl_get_partition_skip(const char *part_name);
#endif
OPERATE_STATUS dl_backup(const char *part_name, uint64_t rcv_size,
	unsigned char *buf);
int dl_image_write(const char *part_name, uint64_t write_size, uint64_t write_offset,
	uint64_t buf_max_size, unsigned char *buf, PARTITION_IMG_TYPE img_format,
	PARTITION_PURPOSE part_purpose);

int is_f2fs_filesystem(const char *part_name);
const char *_get_backup_partition_name(const char * partition_name);
int get_ab_partition(char *temp);
#ifndef CONFIG_NAND_BOOT
uint8_t flash_2ndhand_detect(void);
#endif
#endif //_FDL_EMMC_OPERATE_H
