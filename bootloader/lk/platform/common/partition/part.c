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
#include <malloc.h>
#include <part.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <part_efi.h>

#ifdef CONFIG_LBA48
typedef uint64_t lba512_t;
#else
typedef lbaint_t lba512_t;
#endif

//no DECLARE_GLOBAL_DATA_PTR in lk;
struct block_drvr {
	const char *blk_name;
	block_dev_desc_t* (*get_blk_dev)(int dev);
	int (*select_hwpart)(int dev_num, int hwpart);
	int (*get_dev_num_hwpart)(int hwpart);
	u64 (*get_dev_size_hwpart)(int hwpart);
};

#ifdef CONFIG_EFI_PARTITION
	extern int get_partition_info_by_part_name(block_dev_desc_t * dev_desc, const char *partition_name, disk_partition_t *info);
#endif

static const struct block_drvr block_drvr[] = {
#if defined(CONFIG_MMC)
	{
		.blk_name = "mmc",
		.get_blk_dev = mmc_get_dev,
		.select_hwpart = sprd_mmc_select_hwpart,
		.get_dev_size_hwpart = mmc_get_hwpartsize,
	},
#endif
#if defined(CONFIG_UFS)
	{
		.blk_name = "ufs",
		.get_blk_dev = ufs_get_dev,
		.get_dev_num_hwpart = ufs_get_dev_id,
		.get_dev_size_hwpart = ufs_get_hwpartsize,
	},
#endif
#if defined(CONFIG_CMD_USB) && defined(CONFIG_USB_STORAGE)
	{ .blk_name = "usb", .get_blk_dev = usb_stor_get_dev, },
#endif

	{0},
};

#ifdef HAVE_BLOCK_DEVICE
block_dev_desc_t *get_dev_hwpart(const char *ifname, int dev, int hwpart)
{
	const struct block_drvr *drv_ops = block_drvr;
	block_dev_desc_t* (*reloc_get_dev)(int dev);
	int (*select_hwpart)(int dev_num, int hwpart);
	const char *blk_name;
	int ret;

	if (!ifname)
		return NULL;

	blk_name = drv_ops->blk_name;
	/* If config reloc, should add reloc offset to blk_name */
	while (drv_ops->blk_name) {
		blk_name = drv_ops->blk_name;
		reloc_get_dev = drv_ops->get_blk_dev;
		select_hwpart = drv_ops->select_hwpart;
		/* If config reloc, should add reloc offset to blk_name,reloc_get_dev,select_hwpart */
		if (strncmp(ifname, blk_name, strlen(blk_name)) == 0) {
			block_dev_desc_t *dev_desc = reloc_get_dev(dev);
			if (!dev_desc)
				return NULL;
			if (!select_hwpart)
				return dev_desc;
			ret = select_hwpart(dev_desc->dev_num, hwpart);
			if (ret < 0)
				return NULL;
			return dev_desc;
		}
		drv_ops++;
	}
	return NULL;
}

block_dev_desc_t *get_dev(const char *ifname, int dev)
{
	return get_dev_hwpart(ifname, dev, 0);
}

u64 get_devsize_hwpart(const char *ifname, int hwpart)
{
	const struct block_drvr *drv_ops = block_drvr;
	u64 (*get_devsize)(int hwpart);
	const char *blk_name;
	u64 ret = 0;

	if (!ifname)
		return 0;

	blk_name = drv_ops->blk_name;
	/* If config reloc, should add reloc offset to blk_name */
	while (drv_ops->blk_name) {
		blk_name = drv_ops->blk_name;
		get_devsize = drv_ops->get_dev_size_hwpart;
		/* If config reloc, should add reloc offset to blk_name,get_devsize */
		if (strncmp(ifname, blk_name, strlen(blk_name)) == 0) {
			if (!get_devsize)
				return 0;
			ret = get_devsize(hwpart);
			return ret;
		}
		drv_ops++;
	}
	return 0;
}

int get_devnum_hwpart(const char *ifname, int hwpart)
{
	const struct block_drvr *drv_ops = block_drvr;
	int (*get_dev_num)(int hwpart);
	const char *blk_name;
	int ret;

	if (!ifname)
		return -1;

	blk_name = drv_ops->blk_name;
	/* If config reloc, should add reloc offset to blk_name */
	while (drv_ops->blk_name) {
		blk_name = drv_ops->blk_name;
		get_dev_num = drv_ops->get_dev_num_hwpart;
		/* If config reloc, should add reloc offset to blk_name,get_dev_num */
		if (strncmp(ifname, blk_name, strlen(blk_name)) == 0) {
			if (!get_dev_num)
				return 0;
			ret = get_dev_num(hwpart);
			if (ret < 0)
				return -1;
			return ret;
		}
		drv_ops++;
	}
	return -1;
}

void init_part(block_dev_desc_t *dev_desc)
{
/* We must detect EFI part first, if not exist, then detect dos etc. */
#ifdef CONFIG_EFI_PARTITION
	if (test_part_efi(dev_desc) == 0) {
		dev_desc->part_type = PART_TYPE_EFI;
		return;
	}
#endif

#ifdef CONFIG_DOS_PARTITION
	if (test_part_dos(dev_desc) == 0) {
		dev_desc->part_type = PART_TYPE_DOS;
		return;
	}
#endif
	/* otherwise, part_type = PART_TYPE_UNKNOWN */
	dev_desc->part_type = PART_TYPE_UNKNOWN;
}


#if defined(CONFIG_DOS_PARTITION) || defined(CONFIG_EFI_PARTITION)

static void print_part_header(const char *type, block_dev_desc_t *dev_desc)
{
	debugf("\nPartition Map for ");
	if (dev_desc->api_type == API_TYPE_IDE) {
		debugf("IDE");
	} else if (dev_desc->api_type == API_TYPE_SATA) {
		debugf("SATA");
	} else if (dev_desc->api_type == API_TYPE_SCSI) {
		debugf("SCSI");
	} else if (dev_desc->api_type == API_TYPE_ATAPI) {
		debugf("ATAPI");
	} else if (dev_desc->api_type == API_TYPE_USB) {
		debugf("USB");
	} else if (dev_desc->api_type == API_TYPE_DOC) {
		debugf("DOC");
	} else if (dev_desc->api_type == API_TYPE_MMC) {
		debugf("MMC");
	} else if (dev_desc->api_type == API_TYPE_HOST) {
		debugf("HOST");
	} else {
		debugf("UNKNOWN");
	}
	debugf (" device %d  --   Partition Type: %s\n\n",
			dev_desc->dev_num, type);
}

#endif /* CONFIG_DOS_PARTITION or CONFIG_EFI_PARTITION */

/* Used for cmd_mmc.c debug, transfer print_part_header */
void dump_part_info(block_dev_desc_t * dev_desc)
{
#ifdef CONFIG_DOS_PARTITION
	if (dev_desc->part_type == PART_TYPE_DOS) {
		debugf("## Start to print GPT info for DOS partition ##\n");
		print_part_header ("DOS", dev_desc);
		debugf("## Start to print GPT info for DOS partition ##\n");
		print_part_dos (dev_desc);
		return;
	}
#endif

#ifdef CONFIG_EFI_PARTITION
	if (dev_desc->part_type == PART_TYPE_EFI) {
		debugf("** Start to print part header for EFI partition **\n");
		print_part_header ("EFI", dev_desc);
		debugf("## Start to print GPT info for EFI partition ##\n");
		dump_gpt_info (dev_desc);
		return;
	}
#endif
	debugf ("## Unknown partition table\n");
}

/**
 * Get disk part info:
 * get_partition_info_efi for efi type
 * get_partition_info_dos for dos type
*/
int get_partition_info(block_dev_desc_t *dev_desc, int part,
		       disk_partition_t *info)
{
#ifdef CONFIG_PARTITION_UUIDS
	/* Initialize UUID */
	info->uuid[0] = 0;
#endif

#ifdef CONFIG_DOS_PARTITION
	if (dev_desc->part_type == PART_TYPE_DOS) {
		if (get_partition_info_dos(dev_desc, part, info) == 0) {
			debugf("## Valid DOS partition found ##\n");
			return 0;
		}
	}
#endif

#ifdef CONFIG_EFI_PARTITION
	if (dev_desc->part_type == PART_TYPE_EFI) {
		if (get_partition_info_efi(dev_desc, part, info) == 0) {
			debugf("## Valid EFI partition found ##\n");
			return 0;
		}
	}
#endif
	/* No part_type match, return -1 */
	return -1;
}

/* Get partition info struct by partition name */
int get_partition_info_by_name(block_dev_desc_t *dev_desc, const char* partition_name,
		       disk_partition_t *info)
{
	switch(dev_desc->part_type){
#ifdef CONFIG_EFI_PARTITION
	case PART_TYPE_EFI:
		if (get_partition_info_by_part_name(dev_desc, partition_name, info)==0) {
			return (0);
		}
		break;
#endif
	default:
		break;
	}
	return(-1);
}
#endif /* HAVE_BLOCK_DEVICE */
