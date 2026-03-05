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

#ifndef _INCLUDE_PART_EFI_H
#define _INCLUDE_PART_EFI_H

#include <linux/compiler.h>
#include <linux/types.h>

/* For GPT PMBR partition */
#define GPT_PMBR_SIGNATURE 0xAA55
#define GPT_EFI_PMBR_SYSIND 0xEE

/* For GPT header check */
#define GPT_HEADER_SIGNATURE 0x5452415020494645ULL
#define GPT_HEADER_REVISION 0x00010000
#define GPT_PRIMARY_PARTITION_LBA 1ULL

/* For GPT entry parameter */
#define GPT_ENTRY_NUMBERS		128
#define GPT_ENTRY_SIZE			128

#define GPT_VALIDATE_RETRY_NUM		5

typedef u16 efi_char16_t;
#define PARTNAME_SZ	(72 / sizeof(efi_char16_t))

typedef struct {
	u8 b[16];
} efi_guid_t;

#define EFI_GUID_CACL(a,b,c,d0,d1,d2,d3,d4,d5,d6,d7) \
	((efi_guid_t) \
	{{ (a) & 0xff, ((a) >> 8) & 0xff, ((a) >> 16) & 0xff, ((a) >> 24) & 0xff, \
		(b) & 0xff, ((b) >> 8) & 0xff, \
		(c) & 0xff, ((c) >> 8) & 0xff, \
		(d0), (d1), (d2), (d3), (d4), (d5), (d6), (d7) }})

#define PARTITION_TYPE_GUID \
	EFI_GUID_CACL( 0xC12A7328, 0xF81F, 0x11d2, \
		0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B)
#define LEGACY_MBR_PARTITION_GUID \
	EFI_GUID_CACL( 0x024DEE41, 0x33E7, 0x11d3, \
		0x9D, 0x69, 0x00, 0x08, 0xC7, 0x81, 0xF3, 0x9F)
#define PARTITION_TYPE_GUID_BASIC \
	EFI_GUID_CACL( 0xEBD0A0A2, 0xB9E5, 0x4433, \
		0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7)


/* based on linux/include/genhd.h */
typedef struct partition {
	u8 boot_ind;		/* 0x80 - active */
	u8 start_head;		/* starting head */
	u8 sector;		/* starting sector */
	u8 start_cyl;			/* starting cylinder */
	u8 sys_ind;		/* partition type */
	u8 end_head;		/* end head */
	u8 end_sector;		/* end sector */
	u8 end_cyl;		/* end cylinder */
	__le32 start_sect;	/* starting sector counting from 0, for pmbr initialize and check*/
	__le32 num_sectors;	/* nr of sectors in partition */
} __attribute__ ((packed))  partition;

/* based on linux/fs/partitions/efi.h */
typedef struct gpt_header {
	__le64 signat_value;	/* signature of gpt header */
	__le32 revision;	/* header revision of gpt header */
	__le32 header_size;	/* sizeof of gpt header */
	__le32 header_crc32;	/* cacl crc32 of gpt header */
	__le32 reserved1;
	__le64 my_lba;	/* primary or backup of my_lba */
	__le64 alternate_lba;
	__le64 first_usable_lba;	/* first lba can be used to store */
	__le64 last_usable_lba;	/* last lba can be used to store */
	efi_guid_t disk_guid;
	__le64 partition_entry_lba;	/* primary lba(2) or backup lba((g_header->my_lba) - 32) */
	__le32 num_partition_entries;	/* number entries of partition */
	__le32 sizeof_partition_entry;	/* sizeof(gpt_entry) */
	__le32 partition_entry_array_crc32;	/* calculate entry member crc32 */
} __attribute__ ((packed))  gpt_header;

typedef union gpt_entry_attributes {
	struct {
		u64 required_to_function:1;/* required to function */
		u64 lack_block_io_protocol:1;/* no block io setting */
		u64 legacy_bios_bootable:1;/* Used by get_bootable_from_entry */
		u64 reserved_area:45;/* Reserved */
		u64 type_guid_specific:16;/* specific guid for gpt_entry */
	} params;
	unsigned long long raw_data;
} __attribute__ ((packed))  gpt_entry_attributes;

typedef struct protective_mbr {
	u8 boot_code[440];
	__le32 unique_mbr_signature;
	__le16 unknown;
	struct partition partition_record[4];
	__le16 signature;
} __attribute__ ((packed))   protective_mbr;
typedef struct gpt_entry {
	efi_guid_t partition_type_guid;
	efi_guid_t unique_partition_guid;
	__le64 starting_lba;
	__le64 ending_lba;
	gpt_entry_attributes attributes;
	efi_char16_t partition_name[PARTNAME_SZ];
} __attribute__ ((packed))  gpt_entry;

unsigned long simple_strtoul(const char *cp, char **endp, unsigned int base);
unsigned long long simple_strtoull(const char *cp, char **endp, unsigned int base);

#endif	/* _DISK_PART_EFI_H */
