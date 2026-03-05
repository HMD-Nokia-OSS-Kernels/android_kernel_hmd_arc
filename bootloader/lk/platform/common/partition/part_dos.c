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
#include "part_dos.h"
#include <part.h>
#include <string.h>

#ifdef HAVE_BLOCK_DEVICE

#define DOS_PART_DEFAULT_SECTOR 512

/* boot_ind active 0x80 */
static inline int is_dos_bootable(dos_partition_t *p)
{
	return p->boot_ind == 0x80;
}

static inline int is_extended(int part_type)
{
    return (part_type == 0x5 ||
	    part_type == 0xf ||
	    part_type == 0x85);
}

/* Convert little endian char[4] to host format integer */
static inline int le32_to_int(unsigned char *le32)
{
    return ((le32[3] << 24) +
	    (le32[2] << 16) +
	    (le32[1] << 8) +
	     le32[0]
	   );
}

static int test_block_type(unsigned char *dos_buffer)
{
	int slot;
	struct dos_partition *part;

	/* Check dos part magic */
	if((dos_buffer[DOS_PART_MAGIC_OFFSET + 0] != 0x55) ||
	    (dos_buffer[DOS_PART_MAGIC_OFFSET + 1] != 0xaa) ) {
		return (-1); /* no DOS Signature at all */
	}

	part = (struct dos_partition *)&dos_buffer[DOS_PART_TBL_OFFSET];
	for (slot = 0; slot < 3; slot++) {
		if (part->boot_ind != 0 && part->boot_ind != 0x80) {
			if (!slot &&
			    (strncmp((char *)&dos_buffer[DOS_PBR_FSTYPE_OFFSET],
				     "FAT", 3) == 0 ||
			     strncmp((char *)&dos_buffer[DOS_PBR32_FSTYPE_OFFSET],
				     "FAT32", 5) == 0)) {
				return DOS_PBR; /* Dos PBR type */
			} else {
				return -1;
			}
		}
	}
	return DOS_MBR;	    /* Dos MBR type */
}

int test_part_dos(block_dev_desc_t *dev_desc)
{
	ALLOC_CACHE_ALIGN_BUFFER(unsigned char, buffer, dev_desc->blksz);

	if (dev_desc->block_read(dev_desc->dev_num, 0, 1, (ulong *)buffer) != 1) {
		errorf("%s:Read dos part from dev %d failed!\n", __func__, dev_desc->dev_num);
		return -1;
	}

	if (test_block_type(buffer) != DOS_MBR) {
		errorf("%s:This part type is not DOS_MBR!\n", __func__);
		return -1;
	}

	return 0;
}

/* Print part info according offset by ext_part_sector */
static void print_part_info(dos_partition_t *part, int ext_part_sector,
			   int part_num, unsigned int disksig)
{
	int lba_start = ext_part_sector + le32_to_int(part->start);
	int lba_size  = le32_to_int(part->size);

	debugf("%3d\t%-10d\t%-10d\t%08x-%02x\t%02x%s%s\n",
		part_num, lba_start, lba_size, disksig, part_num, part->sys_ind,
		(is_extended(part->sys_ind) ? " Extd" : ""),
		(is_dos_bootable(part) ? " Boot" : ""));
}

/* Print partitions info according to Extended partition table
 * Format: Part | Start | Sector | Num | Sectors | UUID | Type
*/
static void print_extended_partition(block_dev_desc_t *dev_desc,
				     int ext_sector_num, int offset,
				     int part_num, unsigned int disksig)
{
	int i, ret;
	dos_partition_t *part;
	ALLOC_CACHE_ALIGN_BUFFER(unsigned char, ext_buffer, dev_desc->blksz);

	/* Read extended partition sector */
	if (dev_desc->block_read(dev_desc->dev_num, ext_sector_num, 1, (ulong *) ext_buffer) != 1) {
		errorf("** Can't read extended partition table from dev %d on %d sector **\n",
			dev_desc->dev_num, ext_sector_num);
		return;
	}

	ret = test_block_type(ext_buffer);
	if (ret != DOS_MBR) {
		errorf("Error DOS_MBR sector signature 0x%02x%02x\n",
			ext_buffer[DOS_PART_MAGIC_OFFSET],
			ext_buffer[DOS_PART_MAGIC_OFFSET + 1]);
		return;
	}

	if (!ext_sector_num)
		disksig = le32_to_int(&ext_buffer[DOS_PART_DISKSIG_OFFSET]);

	/* print partition info */
	part = (dos_partition_t *) (ext_buffer + DOS_PART_TBL_OFFSET);
	for (i = 0; i < 4; i++, part++) {
		/* show extended partitions info that in MBR */
		if ((part->sys_ind != 0) &&
		    (ext_sector_num == 0 || !is_extended(part->sys_ind)) ) {
			print_part_info(part, ext_sector_num, part_num, disksig);
		}

		/* Reverse engr the fdisk part# assignment rule! */
		if ((ext_sector_num == 0) ||
		    (part->sys_ind != 0 && !is_extended(part->sys_ind)) ) {
			part_num++;
		}
	}

	/* Follows the extended partitions */
	part = (dos_partition_t *) (ext_buffer + DOS_PART_TBL_OFFSET);
	for (i = 0; i < 4; i++, part++) {
		if (is_extended(part->sys_ind)) {
			int lba_start = le32_to_int(part->start) + offset;

			print_extended_partition(dev_desc, lba_start,
				ext_sector_num == 0  ? lba_start : offset,
				part_num, disksig);
		}
	}

	return;
}

/* Obtain partition info from part num, according to its Extended partition table */
static int get_partition_info_extended (block_dev_desc_t *dev_desc, int ext_part_sector,
				 int relative, int part_num,
				 int which_part, disk_partition_t *info,
				 unsigned int disksig)
{
	int i;
	int dos_type;
	dos_partition_t *part;
	ALLOC_CACHE_ALIGN_BUFFER(unsigned char, buffer, dev_desc->blksz);

	if (dev_desc->block_read(dev_desc->dev_num, ext_part_sector, 1, (ulong *) buffer) != 1) {
		errorf("** Can't read extended partition table from dev %d on %d sector **\n",
			dev_desc->dev_num, ext_part_sector);
		return -1;
	}
	/* Check dos part magic */
	if (buffer[DOS_PART_MAGIC_OFFSET] != 0x55 ||
		buffer[DOS_PART_MAGIC_OFFSET + 1] != 0xaa) {
		errorf("Error MBR signature 0x%02x%02x\n",
			buffer[DOS_PART_MAGIC_OFFSET],
			buffer[DOS_PART_MAGIC_OFFSET + 1]);
		return -1;
	}

#ifdef CONFIG_PARTITION_UUIDS
	if (!ext_part_sector)
		disksig = le32_to_int(&buffer[DOS_PART_DISKSIG_OFFSET]);
#endif

	/* Fill all primary/logical partitions info */
	part = (dos_partition_t *) (buffer + DOS_PART_TBL_OFFSET);
	for (i = 0; i < 4; i++, part++) {
		/* fdisk show the extended partitions in the MBR */
		if (((part->boot_ind & ~0x80) == 0) &&
		    (part->sys_ind != 0) &&
		    (part_num == which_part) &&
		    (is_extended(part->sys_ind) == 0)) {
			info->blksz = DOS_PART_DEFAULT_SECTOR;
			info->start_blk = (lbaint_t)(ext_part_sector +
					le32_to_int(part->start));
			info->blk_cnt  = (lbaint_t)le32_to_int(part->size);
			/* Fill part_name */
			if ( (dev_desc->api_type == API_TYPE_IDE)
					|| (dev_desc->api_type == API_TYPE_SATA)
					|| (dev_desc->api_type == API_TYPE_ATAPI) ) {
				sprintf((char *)info->part_name, "hd%c%d",
					'a' + dev_desc->dev_num, part_num);
			} else if (dev_desc->api_type == API_TYPE_SCSI) {
				sprintf((char *)info->part_name, "sd%c%d",
					'a' + dev_desc->dev_num, part_num);
			} else if (dev_desc->api_type == API_TYPE_USB) {
				sprintf((char *)info->part_name, "usbd%c%d",
					'a' + dev_desc->dev_num, part_num);
			} else if (dev_desc->api_type == API_TYPE_DOC) {
				sprintf((char *)info->part_name, "docd%c%d",
					'a' + dev_desc->dev_num, part_num);
			} else {
				sprintf ((char *)info->part_name, "xx%c%d",
					'a' + dev_desc->dev_num, part_num);
			}
			/* sprintf(info->platform_type, "%d, part->sys_ind); */
			sprintf((char *)info->platform_type, "LK");
			info->bootable = is_dos_bootable(part);
#ifdef CONFIG_PARTITION_UUIDS
			sprintf(info->uuid, "%08x-%02x", disksig, part_num);
#endif
			return 0;
		}

		/* Reverse engr the fdisk part# assignment rule! */
		if ((ext_part_sector == 0) ||
		    (part->sys_ind != 0 && !is_extended(part->sys_ind)) ) {
			part_num++;
		}
	}

	/* Follows the extended partitions */
	part = (dos_partition_t *) (buffer + DOS_PART_TBL_OFFSET);
	for (i = 0; i < 4; i++, part++) {
		if (is_extended (part->sys_ind)) {
			int lba_start = le32_to_int (part->start) + relative;

			return get_partition_info_extended (dev_desc, lba_start,
				 ext_part_sector == 0 ? lba_start : relative,
				 part_num, which_part, info, disksig);
		}
	}

	dos_type = test_block_type(buffer);

	if (dos_type == DOS_PBR) {
		info->start_blk = 0;
		info->blk_cnt = dev_desc->lba;
		info->blksz = DOS_PART_DEFAULT_SECTOR;
		info->bootable = 0;
		sprintf ((char *)info->platform_type, "LK");
#ifdef CONFIG_PARTITION_UUIDS
		info->uuid[0] = 0;
#endif
		return 0;
	}

	return -1;
}

/* print extended partition info for dos type */
void print_part_dos(block_dev_desc_t *dev_desc)
{
	debugf("Part\tStart Sector\tNum of Sector\tUUID\t\tType\n");
	print_extended_partition(dev_desc, 0, 0, 1, 0);
}

/* Obtain dos extended partition info by part num */
int get_partition_info_dos(block_dev_desc_t *dev_desc, int part, disk_partition_t * info)
{
	return get_partition_info_extended(dev_desc, 0, 0, 1, part, info, 0);
}

#endif
