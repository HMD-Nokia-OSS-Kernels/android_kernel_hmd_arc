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

#ifndef _PART_H
#define _PART_H

#include <stdlib.h>
#include <sprd_common.h>

#define CONFIG_PARTITION_UUIDS

/* Interface types: */
#define API_TYPE_UNKNOWN		0
#define API_TYPE_IDE		1
#define API_TYPE_SCSI		2
#define API_TYPE_ATAPI		3
#define API_TYPE_USB		4
#define API_TYPE_DOC		5
#define API_TYPE_MMC		6
#define API_TYPE_SD		7
#define API_TYPE_SATA		8
#define API_TYPE_HOST		9
#define API_TYPE_UFS		10
#define API_TYPE_MAX		11	/* Max number of IF_TYPE_* supported */


/* Part types */
#define PART_TYPE_UNKNOWN	0x00
#define PART_TYPE_MAC		0x01
#define PART_TYPE_DOS		0x02
#define PART_TYPE_ISO		0x03
#define PART_TYPE_AMIGA		0x04
#define PART_TYPE_EFI		0x05

/* When enabled, makes the IDE subsystem use 64bit sector addresses.Default is 32bit. */
#ifdef CONFIG_SYS_64BIT_LBA
typedef uint64_t lbaint_t;
#define LBAF "%llx"
#define LBAFU "%llu"
#else
typedef ulong lbaint_t;
#define LBAF "%lx"
#define LBAFU "%lu"
#endif

#define PAD_COUNT(s, pad) (((s) - 1) / (pad) + 1)
#define PAD_SIZE(s, pad) (PAD_COUNT(s, pad) * pad)
#define ALLOC_ALIGN_BUFFER_PAD(type, name, size, align, pad)		\
	char __##name[ROUND(PAD_SIZE((size) * sizeof(type), pad), align)  \
		      + (align - 1)];					\
									\
	type *name = (type *) ALIGN((uintptr_t)__##name, align)
#define ALLOC_ALIGN_BUFFER(type, name, size, align)		\
	ALLOC_ALIGN_BUFFER_PAD(type, name, size, align, 1)
#define ALLOC_CACHE_ALIGN_BUFFER_PAD(type, name, size, pad)		\
	ALLOC_ALIGN_BUFFER_PAD(type, name, size, ARCH_DMA_MINALIGN, pad)
#define ALLOC_CACHE_ALIGN_BUFFER(type, name, size)			\
	ALLOC_ALIGN_BUFFER(type, name, size, ARCH_DMA_MINALIGN)

#define BLOCK_CNT(size, block_dev_desc) (PAD_COUNT(size, block_dev_desc->blksz))
#define PAD_TO_BLOCKSIZE(size, block_dev_desc) \
	(PAD_SIZE(size, block_dev_desc->blksz))
#define LOG2(x) (((x & 0xaaaaaaaa) ? 1 : 0) + ((x & 0xcccccccc) ? 2 : 0) + \
		 ((x & 0xf0f0f0f0) ? 4 : 0) + ((x & 0xff00ff00) ? 8 : 0) + \
		 ((x & 0xffff0000) ? 16 : 0))
//#define LOG2_INVALID(type) ((type)((sizeof(type)<<3)-1))

typedef struct block_dev_desc {
	int		api_type;	/* type of the interface */
	int		dev_num;		/* device number */
	unsigned char	removable;	/* removable device */
	unsigned char	part_type;	/* partition type, only support for efi in lk */
	unsigned char	dev_type;		/* device type */
	unsigned char	target_id;		/* Not used, target SCSI ID */
	unsigned char	target_lun;		/* Not used, target LUN */
#ifdef CONFIG_LBA48
	unsigned char	lba48;		/* device can use 48bit addr (ATA/ATAPI v7) */
#endif
	lbaint_t	lba;		/* number of blocks */
	unsigned long	blksz;		/* block size */
	int		log2blksz;	/* for convenience: log2(blksz) */
	char		vendor_model [40+1];	/* IDE model, SCSI Vendor */
	char		product_model[20+1];	/* IDE Serial no, SCSI product */
	char		firmware_revision[8+1];	/* firmware revision */
	unsigned long	(*block_read)(int dev_part,
				      lbaint_t start_blk,
				      lbaint_t blkcnt,
				      void *data_buffer);
	unsigned long	(*block_write)(int dev_part,
				       lbaint_t start_blk,
				       lbaint_t blkcnt,
				       const void *data_buffer);
	unsigned long   (*block_erase)(int dev_part,
				       lbaint_t start_blk,
				       lbaint_t blkcnt);
	long            (*block_sync)(int dev_part);
	unsigned long   (*block_pwr_wp)(int dev_part,
				       lbaint_t start_blk,
				       int grpcnt);
	unsigned long	(*backstage_block_write)(int dev_part,
				      uint32_t start_blk,
				      uint32_t blkcnt,
				      const void *data_buffer);
	unsigned long	(*backstage_write_query)(int dev_part,
				       uint32_t blkcnt,
				       const void *data_buffer);
	unsigned long	(*backstage_block_read)(int dev_part,
				      uint32_t start_blk,
				      uint32_t blkcnt,
				      const void *data_buffer);
	unsigned long	(*backstage_read_query)(int dev_part,
				       uint32_t blkcnt,
				       const void *data_buffer);
	void		*priv;		/* driver private struct pointer */
}block_dev_desc_t;

/* partition info in disk table */
typedef struct disk_partition {
	lbaint_t	start_blk;	/* starting block of disk partition */
	lbaint_t	blk_cnt;	/* number of blocks disk partition contains */
	ulong	blksz;		/* block size in bytes */
	uchar	part_name[32];	/* partition name in disk table */
	char	platform_type[32];	/* platform type string in disk_partition */
	int	bootable;	/* Obtain by get_bootable_from_entry, active Bootable always be 1 */
#ifdef CONFIG_PARTITION_UUIDS
	char	uuid[37];	/* CONFIG_PARTITION_UUIDS always be define, filesystem UUID */
#endif
	unsigned int	part_num;	/* Number of partition in disk */
} disk_partition_t;

/* We have define CONFIG_PARTITIONS for each soc, Functions list to declare */
#ifdef CONFIG_PARTITIONS
block_dev_desc_t *get_dev(const char *ifname, int dev);
block_dev_desc_t *get_dev_hwpart(const char *ifname, int dev, int hwpart);
int get_devnum_hwpart(const char *ifname, int hwpart);
u64 get_devsize_hwpart(const char *ifname, int hwpart);

#ifdef CONFIG_MMC
block_dev_desc_t* mmc_get_dev(int dev);
u64 mmc_get_hwpartsize(int hwpart);
int sprd_mmc_select_hwpart(int dev_num, int hwpart);
#endif

#if defined(CONFIG_CMD_USB) && defined(CONFIG_USB_STORAGE)
block_dev_desc_t* usb_stor_get_dev(int dev);
#endif

#ifdef CONFIG_SD
block_dev_desc_t* sd_get_dev(int dev);
#endif

#ifdef CONFIG_UFS
block_dev_desc_t *ufs_get_dev(int dev);
int ufs_get_dev_id(int hwpart);
u64 ufs_get_hwpartsize(int hwpart);
#endif

/* disk/part.c */
int get_partition_info (block_dev_desc_t * dev_desc, int part, disk_partition_t *info);
void dump_part_info (block_dev_desc_t *dev_desc);
void init_part (block_dev_desc_t *dev_desc);
#endif

#ifdef CONFIG_DOS_PARTITION
/* disk/part_dos.c */
int get_partition_info_dos (block_dev_desc_t * dev_desc, int part, disk_partition_t *info);
void print_part_dos (block_dev_desc_t *dev_desc);
int   test_part_dos (block_dev_desc_t *dev_desc);
#endif

#ifdef CONFIG_EFI_PARTITION
#include <part_efi.h>
/* disk/part_efi.c */
int get_partition_info_efi (block_dev_desc_t * dev_desc, u32 part, disk_partition_t *info);
int match_partition_info_efi_by_part_name(block_dev_desc_t *dev_desc,
	const char *name, disk_partition_t *info);
void dump_gpt_info (block_dev_desc_t *dev_desc);
int   test_part_efi (block_dev_desc_t *dev_desc);
int write_gpt_table(block_dev_desc_t *dev_desc,
		  gpt_header *gpt_h, gpt_entry *gpt_e);
int gpt_pte_fill(gpt_header *gpt_h, gpt_entry *gpt_e,
		disk_partition_t *partitions, int parts);
int gpt_header_fill(block_dev_desc_t *dev_desc, gpt_header *gpt_h,
		char *str_guid, int parts_count);
int gpt_info_fill(block_dev_desc_t *dev_desc, char *str_disk_guid,
		disk_partition_t *partitions, const int parts_count);
int is_gpt_mem_validate(block_dev_desc_t *dev_desc, void *buf);
int write_mbr_and_gpt_partitions(block_dev_desc_t *dev_desc, void *buf);
#endif

#endif /* _PART_H */
