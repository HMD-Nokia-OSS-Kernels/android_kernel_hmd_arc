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

/*
 * NOTE:
 *   when CONFIG_SYS_64BIT_LBA is not defined, lbaint_t is 32 bits; this
 *   limits the maximum size of addressable storage to < 2 Terra Bytes
 */

#include <stdio.h>
#include <sprd_common.h>
#include <inttypes.h>
#include <malloc.h>
#include <ctype.h>
#include <part_efi.h>
#include <part.h>
#include <string.h>
#include <linux/unaligned/le_byteshift.h>
#include <linux/byteorder/little_endian.h>
#include <linux/byteorder/generic.h>
#include <uuid.h>
#include <lib/cksum.h>
#include <errno.h>
#include <sprd_sizes.h>

#ifdef CONFIG_UFS
extern bool is_active_reconfig_ufs(void);
#endif

#ifdef CONFIG_VERIFY_GPT
#include <dl_cmd_proc.h>
#include <dl_operate.h>
#include <sprd_common_rw.h>
#include <lk_sec_drv.h>

extern int gpt_key_cmd;
extern gpt_cmd_data gpt_data;
#endif
//DECLARE_GLOBAL_DATA_PTR;
#define min(a, b) (((a) < (b)) ? (a) : (b))

#ifdef HAVE_BLOCK_DEVICE
/**
 * gpt_crc32() - crc32 function for GPT verify
 * @buf: data pointer to calculate crc32
 * @size - size of buf
 *
 * Description: Returns CRC32 value for @buf
 */

static inline u32 gpt_crc32(const void *buf, u32 size)
{
	return crc32(0, buf, size);
}

/*
 * Private function prototypes
 */

static int is_pmbr_part_validate(struct partition *part);
static int is_pmbr_validate(protective_mbr * mbr);
static int is_gpt_validate(block_dev_desc_t *dev_desc, u64 lba,
				gpt_header *pgpt_head, gpt_entry **pgpt_pte);
static gpt_entry *alloc_read_gpt_entries(block_dev_desc_t * dev_desc,
				gpt_header * pgpt_head);
static int is_pte_validate(gpt_entry * pte);
static int set_protective_mbr(block_dev_desc_t *dev_desc);

static char *print_ptename(gpt_entry *pte)
{
	static char part_name[PARTNAME_SZ + 1];
	u32 i;
	for (i = 0; i < PARTNAME_SZ; i++) {
		u8 partition_c;
		partition_c = pte->partition_name[i] & 0xff;
		partition_c = (partition_c && !isprint(partition_c)) ? '.' : partition_c;
		part_name[i] = partition_c;
	}
	part_name[PARTNAME_SZ] = 0;
	return part_name;
}

static efi_guid_t part_type_guid;

static gpt_header gpt_h_buf;
static u8 gpt_e_buf[GPT_ENTRY_NUMBERS * GPT_ENTRY_SIZE] = {0};

unsigned int is_gpt_h_buf = 0;
unsigned int is_gpt_e_buf = 0;

static inline int get_bootable_from_entry(gpt_entry *g_entry)
{
	int ret;
	part_type_guid = PARTITION_TYPE_GUID;

	ret = g_entry->attributes.params.legacy_bios_bootable ||
		!memcmp(&(g_entry->partition_type_guid), &part_type_guid,
			sizeof(efi_guid_t));
	return ret;
}

static int is_gpt_header_validate(gpt_header *g_header, lbaint_t lba,
		lbaint_t lastlba)
{
	uint32_t orig_header_crc32 = 0;
	uint32_t calc_header_crc32 = 0;

	/* Check GPT header signature value */
	if (le64_to_cpu(g_header->signat_value) != GPT_HEADER_SIGNATURE) {
		errorf("GPT Header check: signature error: 0x%llX != 0x%llX\n",
		       le64_to_cpu(g_header->signat_value), GPT_HEADER_SIGNATURE);
		return -1;
	}

	/* Check GPT header CRC32 */
	memcpy(&orig_header_crc32, &g_header->header_crc32, sizeof(orig_header_crc32));
	memset(&g_header->header_crc32, 0, sizeof(g_header->header_crc32));

	calc_header_crc32 = gpt_crc32((const void*)g_header, le32_to_cpu(g_header->header_size));

	memcpy(&g_header->header_crc32, &orig_header_crc32, sizeof(orig_header_crc32));

	if (le32_to_cpu(orig_header_crc32) != calc_header_crc32) {
		errorf("GPT Header check: CRC error: 0x%x(orig) != 0x%x(calc)\n",
		       le32_to_cpu(orig_header_crc32), calc_header_crc32);
		return -1;
	}

	/* Check my_lba entry whether points to the LBA that contains the GPT */
	if (le64_to_cpu(g_header->my_lba) != lba) {
		errorf("GPT header check: my_lba error: %llX(my_lba) != " LBAF "\n",
		       le64_to_cpu(g_header->my_lba),
		       lba);
		return -1;
	}

	/* Check the first_usable_lba within the disk. */
	if (le64_to_cpu(g_header->first_usable_lba) > lastlba) {
		errorf("GPT header check: first_usable_lba overlap: %llX > " LBAF "\n",
		       le64_to_cpu(g_header->first_usable_lba), lastlba);
		return -1;
	}
	if (le64_to_cpu(g_header->last_usable_lba) > lastlba) {
		errorf("GPT header check: last_usable_lba overlap: %llX > " LBAF "\n",
		       le64_to_cpu(g_header->last_usable_lba), lastlba);
		return -1;
	}
	/* Check numbers of entry in GPT header. */
	if (g_header->num_partition_entries > GPT_ENTRY_NUMBERS) {
        errorf("GPT Header check: num_partition_entries(%d) exceeds %d\n",
		       g_header->num_partition_entries, GPT_ENTRY_NUMBERS);
                return -1;
    }
	/*
	debugf("%s: first_usable_lba: %llX last_usable_lba: %llX last lba: "
	      LBAF "\n", __func__, le64_to_cpu(g_header->first_usable_lba),
	      le64_to_cpu(g_header->last_usable_lba), lastlba);
	*/
	return 0;
}

static int is_gpt_entry_validate(gpt_header *g_header, gpt_entry *g_entry)
{
	uint32_t calc_entry_crc32 = 0;
	uint32_t entry_size = 0;

	/* Check GPT Entries CRC32 */
	entry_size = le32_to_cpu(g_header->num_partition_entries) *
		le32_to_cpu(g_header->sizeof_partition_entry);
	calc_entry_crc32 = gpt_crc32((const void *)g_entry, entry_size);

	if (le32_to_cpu(g_header->partition_entry_array_crc32) != calc_entry_crc32) {
		errorf("GPT Entries CRC32 ERROR: 0x%x(orig) != 0x%x(cacl)\n",
		       le32_to_cpu(g_header->partition_entry_array_crc32),
		       calc_entry_crc32);
		return -1;
	}

	return 0;
}

static void backup_gpt_header_fill(block_dev_desc_t * dev_desc, gpt_header *g_header)
{
	uint32_t calc_header_crc32;
	uint64_t val;

	/* Fill value for the Backup GPT Header */
	val = le64_to_cpu(g_header->my_lba);
	g_header->my_lba = g_header->alternate_lba;
	g_header->alternate_lba = val;
	g_header->partition_entry_lba = le64_to_cpu(g_header->my_lba) -
		(GPT_ENTRY_NUMBERS * GPT_ENTRY_SIZE / dev_desc->blksz);
	g_header->header_crc32 = 0;

	calc_header_crc32 = gpt_crc32((const unsigned char *)g_header,
			       le32_to_cpu(g_header->header_size));
	g_header->header_crc32 = cpu_to_le32(calc_header_crc32);
}

#ifdef CONFIG_EFI_PARTITION

void dump_gpt_info(block_dev_desc_t * dev_desc)
{
	if (!dev_desc) {
		errorf("%s: Invalid dev_desc\n", __func__);
		return;
	}
	gpt_header *gpt_head = NULL;
	uint32_t size = 0;

	size = PAD_TO_BLOCKSIZE(sizeof(gpt_header), dev_desc);
	gpt_head = malloc_cache_aligned(size);
	if (gpt_head == NULL) {
		errorf("no enough heap memory for gpt_head\n");
		return;
	}
	memset(gpt_head, 0, size);
	gpt_entry *g_entry = NULL;
	u32 num = 0;
	char uuid[37];
	unsigned char *uuid_bin;

	/* Call is_gpt_validate function to check GPT header and PTE(include backup) */
	if (is_gpt_validate(dev_desc, GPT_PRIMARY_PARTITION_LBA,
			gpt_head, &g_entry) != 1) {
		errorf("%s: *** ERROR: Invalid GPT ***\n", __func__);
		memset(gpt_head, 0, size);
		if (is_gpt_validate(dev_desc, (dev_desc->lba - 1),
				gpt_head, &g_entry) == 1) {
			debugf("%s: *** Invalid GPT, Using Backup GPT ***\n",
			       __func__);
		} else {
			errorf("%s: *** ERROR: Invalid GPT and Backup GPT ***\n",
			       __func__);
			goto err;
		}
	}

	debugf("%s: GPT-Entry pointer at %p\n", __func__, g_entry);
	debugf("%s: gpt-entry at %p\n", __func__, g_entry);

	debugf("Partition\tStarting LBA\tEnding LBA\t\tName\n");
	debugf("\tAttributes\n");
	debugf("\tType GUID\n");
	debugf("\tPartition GUID\n");

	for (num = 0; num < le32_to_cpu(gpt_head->num_partition_entries); num++) {
		/* Stop at the first non valid PTE */
		if (!is_pte_validate(&g_entry[num]))
			break;

		debugf("%3d\t0x%08llx\t0x%08llx\t\"%s\"\n", (num + 1),
			le64_to_cpu(g_entry[num].starting_lba),
			le64_to_cpu(g_entry[num].ending_lba),
			print_ptename(&g_entry[num]));
		debugf("\tattrs:\t0x%016llx\n", g_entry[num].attributes.raw_data);
		uuid_bin = (unsigned char *)g_entry[num].partition_type_guid.b;
		uuid_bin_to_str(uuid_bin, uuid, UUID_STR_FORMAT_GUID);
		debugf("\ttype:\t%s\n", uuid);
		uuid_bin = (unsigned char *)g_entry[num].unique_partition_guid.b;
		uuid_bin_to_str(uuid_bin, uuid, UUID_STR_FORMAT_GUID);
		debugf("\tguid:\t%s\n", uuid);
	}

err:
	/* Remember to free pte when done process */
	if (g_entry) {
		free(g_entry);
		g_entry = NULL;
	}

	if (gpt_head) {
		free(gpt_head);
		gpt_head = NULL;
	}

	return;
}

/**
 * get_partition_info_efi() - Find the specified GPT partition table entry by part_num
 * dev_desc - block device descriptor
 * part - the part num in table entry
 * info - poniter to the disk partition info
 * return - '0' on success, '-1' for error
 */
int get_partition_info_efi(block_dev_desc_t * dev_desc, u32 part,
				disk_partition_t * info)
{
	uint32_t size = 0;
	int ret = -1;
	gpt_entry *g_entry = NULL;
	gpt_header *gpt_head = NULL;
	size = PAD_TO_BLOCKSIZE(sizeof(gpt_header), dev_desc);
	gpt_head = malloc_cache_aligned(size);
	if (gpt_head == NULL) {
		errorf("no enough heap memory for gpt_head\n");
		goto err;
	}
	memset(gpt_head, 0, size);

	/* "part" argument must be at least 1 */
	if (!dev_desc || !info || part < 1) {
		errorf("%s: Invalid Argument(s)\n", __func__);
		goto err;
	}

	/* Call is_gpt_validate function to check validates of GPT header and PTE */
	if (is_gpt_validate(dev_desc, GPT_PRIMARY_PARTITION_LBA,
			gpt_head, &g_entry) != 1) {
		debugf("%s: *** ERROR: Invalid GPT ***\n", __func__);
		memset(gpt_head, 0, size);
		if (is_gpt_validate(dev_desc, (dev_desc->lba - 1),
				 gpt_head, &g_entry) == 1) {
			debugf("%s: *** Using Backup GPT ***\n", __func__);
		} else {
			errorf("%s: *** ERROR: Invalid Backup GPT ***\n",
			       __func__);
			goto err;
		}
	}

	/* Check if this part num in table entry */
	if (part > le32_to_cpu(gpt_head->num_partition_entries) ||
	    !is_pte_validate(&g_entry[part - 1])) {
		debugf("%s: *** ERROR: Invalid partition number %d ***\n",
			__func__, part);
		goto err;
	}

	/* Starting lba of partition area, use CONFIG_SYS_64BIT_LBA to limit disk size */
	info->start_blk = (lbaint_t)le64_to_cpu(g_entry[part - 1].starting_lba);
	/* Calculate partition area size, add 1 to the ending LBA */
	info->blk_cnt = (lbaint_t)le64_to_cpu(g_entry[part - 1].ending_lba) + 1
		     - info->start_blk;
	/* Determined by driver block */
	info->blksz = dev_desc->blksz;
	info->bootable = get_bootable_from_entry(&g_entry[part - 1]);

	sprintf((char *)info->part_name, "%s",
			print_ptename(&g_entry[part - 1]));
	sprintf((char *)info->platform_type, "LK");
#ifdef CONFIG_PARTITION_UUIDS
	uuid_bin_to_str(g_entry[part - 1].unique_partition_guid.b, info->uuid,
			UUID_STR_FORMAT_GUID);
#endif

	debugf("%s: part_name %s, start 0x" LBAF ", blk_cnt 0x" LBAF "\n", __func__,
	      info->part_name, info->start_blk, info->blk_cnt);
	ret = 0;
err:
	/* Free pte when process done */
	if (g_entry) {
		free(g_entry);
		g_entry = NULL;
	}
	if (gpt_head) {
		free(gpt_head);
		gpt_head = NULL;
	}

	return ret;
}

/**
 * match_partition_info_efi_by_part_name() - Find the specified GPT partition table entry
 *
 * dev_desc - block device descriptor
 * gpt_name - the specified table entry name
 * info - returns the disk partition info
 *
 * return - '0' on match, '-1' on no match, otherwise error
 */
int match_partition_info_efi_by_part_name(block_dev_desc_t *dev_desc,
	const char *name, disk_partition_t *info)
{
	int ret;
	int i;
	for (i = 1; i < GPT_ENTRY_NUMBERS; i++) {
		ret = get_partition_info_efi(dev_desc, i, info);
		if (ret != 0) {
			/* no this num entries in gpt table */
			return -1;
		}
		if (strcmp(name, (const char *)info->part_name) == 0) {
			/* matched */
			return 0;
		}
	}
	return -2;
}
int write_primary_gpt_table(block_dev_desc_t *dev_desc,
		gpt_header *gpt_h, gpt_entry *gpt_e)
{
	const unsigned int pte_blk_cnt = BLOCK_CNT((gpt_h->num_partition_entries
					   * sizeof(gpt_entry)), dev_desc);
	u32 calc_crc32;
	uint64_t orig_my_lba;

	debugf("max lba: %x\n", (u32) dev_desc->lba);
	/* Fill the Protective MBR */
	if (set_protective_mbr(dev_desc) < 0)
		goto err;

	/* recalculate the values for the Primary GPT Header */
	orig_my_lba = le64_to_cpu(gpt_h->my_lba);
	gpt_h->my_lba = gpt_h->alternate_lba;
	gpt_h->alternate_lba = cpu_to_le64(orig_my_lba);
	gpt_h->partition_entry_lba = 2;
	gpt_h->header_crc32 = 0;
	gpt_h->partition_entry_array_crc32 = 0;

	/* Generate CRC for the Primary GPT Header */
	calc_crc32 = gpt_crc32((const unsigned char *)gpt_e,
			      le32_to_cpu(gpt_h->num_partition_entries) *
			      le32_to_cpu(gpt_h->sizeof_partition_entry));
	gpt_h->partition_entry_array_crc32 = cpu_to_le32(calc_crc32);

	calc_crc32 = gpt_crc32((const unsigned char *)gpt_h,
			      le32_to_cpu(gpt_h->header_size));
	gpt_h->header_crc32 = cpu_to_le32(calc_crc32);

	/* Write the GPT header and pte to the device block */
	if (dev_desc->block_write(dev_desc->dev_num, 1, 1, gpt_h) != 1) {
		errorf("** Write GPT Header to device %d error **\n", dev_desc->dev_num);
		goto err;
	}

	if (dev_desc->block_write(dev_desc->dev_num, 2, pte_blk_cnt, gpt_e)
	    != pte_blk_cnt) {
		errorf("** Write GPT entry to device %d error **\n", dev_desc->dev_num);
		goto err;
	}

	debugf("%s:Write Primary GPT to device %d successfully!\n", __func__, dev_desc->dev_num);
	is_gpt_h_buf = 0;
	is_gpt_e_buf = 0;
	return 0;

err:
	is_gpt_h_buf = 0;
	is_gpt_e_buf = 0;
	return -1;
}

static void gpt_dump(unsigned char *data, int len)
{
	int i, j;

	for (i = 0; i < len; i += 16) {
		for (j = i; j < i + 16 && j < len; j += 4)
			debugf("%02x%02x%02x%02x ", data[j], data[j + 1], data[j + 2], data[j + 3]);
		debugf("\n");
	}
}

int get_partition_info_by_part_name(block_dev_desc_t * dev_desc, const char *partition_name, disk_partition_t *info)
{

	if (!dev_desc || !info || !partition_name) {
		errorf("%s: Invalid Argument(s)\n", __func__);
		return -1;
	}

	unsigned int valid_retry = 0, size = 0;
	static int dumped = 0;
	gpt_header *gpt_head = NULL;
	size = PAD_TO_BLOCKSIZE(sizeof(gpt_header), dev_desc);
	gpt_head = malloc_cache_aligned(size);
	if (gpt_head == NULL) {
		errorf("no enough heap memory for gpt_head\n");
		return -1;
	}
	gpt_entry *pgpt_pte = NULL;
	int ret = -1;
	unsigned int i, j, partition_nums = 0;
	uchar disk_partition[PARTNAME_SZ];
	/* This function validates primary GPT header and PTE */
	for (valid_retry = 0; valid_retry < GPT_VALIDATE_RETRY_NUM; valid_retry++ ) {
		pgpt_pte = NULL;
		memset(gpt_head, 0, size);
		if (is_gpt_validate(dev_desc, GPT_PRIMARY_PARTITION_LBA, gpt_head, &pgpt_pte) != 1) {
			errorf("%s: *** ERROR: Invalid Main GPT , %s, retry times is %u***\n", __func__, partition_name, valid_retry + 1);
		} else {
			//debugf("load primary GPT successfully \n");
			break;
		}
	}

	/* if primary GPT's validation is failed,then use alternate GPT */
	if (GPT_VALIDATE_RETRY_NUM == valid_retry) {
		errorf("%s: *** ERROR: Invalid Main GPT ***\n", __func__);
		if (!dumped) {
			gpt_dump(gpt_head, sizeof(gpt_header));
			if (pgpt_pte) {
				gpt_dump(pgpt_pte, le32_to_cpu(gpt_head->num_partition_entries) *
					le32_to_cpu(gpt_head->sizeof_partition_entry));
			}
		}

		pgpt_pte = NULL;
		if (is_gpt_validate(dev_desc, (dev_desc->lba - 1), gpt_head, &pgpt_pte) != 1) {
			errorf("%s: *** ERROR: Invalid alternate GPT ***\n", __func__);
			if (!dumped) {
				dumped = 1;
				gpt_dump(gpt_head, sizeof(gpt_header));
				if (pgpt_pte) {
					gpt_dump(pgpt_pte, le32_to_cpu(gpt_head->num_partition_entries) *
						le32_to_cpu(gpt_head->sizeof_partition_entry));
				}
			}
			goto err;
		} else {
			dumped = 0;
		#ifdef CONFIG_NO_GPT_REWRITE_BACKUP
			/* Jjust read backup gpt, no rewrite primary gpt. Come from AR.254.0188.2334.5383. */
			dprintf(INFO,"load alternate GPT successfully\n");
		#else
			/* Rewrite Primary GPT partition table with the value of Backup GPT */
			write_primary_gpt_table(dev_desc, gpt_head, pgpt_pte);
			debugf("Rewrite Primary GPT successfully\n");
		#endif
		}
	}

	/*Get partition info by loop */
	partition_nums=le32_to_cpu(gpt_head->num_partition_entries);
	for(i=0;i<partition_nums;i++) {
		for(j=0;j<PARTNAME_SZ;j++) {
			disk_partition[j]=pgpt_pte[i].partition_name[j]&0xFF;
		}
		if(0 == strcmp((char *)disk_partition,(char *)partition_name)) {
			/* The ulong casting  limits the maximum disk size to 2 TB */
			info->start_blk = (ulong)le64_to_cpu(pgpt_pte[i].starting_lba);
			/* The ending LBA is inclusive, to calculate size, add 1 to it */
			info->blk_cnt = (ulong)le64_to_cpu((pgpt_pte[i].ending_lba) + 1)
					- info->start_blk;
			info->blksz = dev_desc->blksz;
			info->part_num = i;

			sprintf((char *)info->part_name, "%s",print_ptename(&((pgpt_pte)[i])));
			sprintf((char *)info->platform_type, "LK");

#ifdef CONFIG_PARTITION_UUIDS
			uuid_bin_to_str(pgpt_pte[i].unique_partition_guid.b, info->uuid, UUID_STR_FORMAT_GUID);
#endif

			/*
			debugf("%s: partition name %s, start 0x" LBAF ", size 0x" LBAF "\n", __func__,
				info->name, info->start, info->size);
			*/
			ret = 0;
			break;
		}
	}
err:
	/* Remember to free pte */
	if(pgpt_pte) {
		free(pgpt_pte);
		pgpt_pte = NULL;
	}
	if (gpt_head) {
		free(gpt_head);
		gpt_head = NULL;
	}

	return ret;
}
int test_part_efi(block_dev_desc_t * dev_desc)
{
	protective_mbr *legacymbr = NULL;
	uint32_t size = 0;
	int ret = 0;
	size = PAD_TO_BLOCKSIZE(sizeof(protective_mbr), dev_desc);
	legacymbr = malloc_cache_aligned(size);
	if (legacymbr == NULL) {
		errorf("no enough heap memory for legacymbr\n");
		return -1;
	}
	memset(legacymbr, 0, size);

	/* Read legacy MBR from device block 0 and check it if validate */
	if ((dev_desc->block_read(dev_desc->dev_num, 0, 1, (ulong *)legacymbr) != 1)
		|| (is_pmbr_validate(legacymbr) != 1)) {
		ret = -1;
		goto err;
	}

err:
	if (legacymbr) {
		free(legacymbr);
		legacymbr = NULL;
	}
	return ret;
}

/**
 * set_protective_mbr(): Config the EFI protective MBR
 * return - 0 on success, otherwise error
 */
static int set_protective_mbr(block_dev_desc_t *dev_desc)
{
	/* Initialize the Protective MBR */
	ALLOC_CACHE_ALIGN_BUFFER(protective_mbr, p_mbr, 1);
	memset(p_mbr, 0, sizeof(*p_mbr));

	if (p_mbr == NULL) {
		errorf("%s: calloc failed!\n", __func__);
		return -1;
	}
	/* Append informatition for p_mbr, 0xEE for GPT_EFI_PMBR_SYSIND */
	p_mbr->signature = GPT_PMBR_SIGNATURE;
	p_mbr->partition_record[0].sys_ind = GPT_EFI_PMBR_SYSIND;
	p_mbr->partition_record[0].start_sect = 1;
	p_mbr->partition_record[0].num_sectors = (u32) dev_desc->lba - 1;

	/* Write sector of PMBR into MMC device */
	if (dev_desc->block_write(dev_desc->dev_num, 0, 1, p_mbr) != 1) {
		errorf("** Write protective_mbr to device %d error **\n",
			dev_desc->dev_num);
		return -1;
	}

	return 0;
}

/**
 * write_gpt_table() - Write the GUID Partition Table to disk.
 * return - zero on success, otherwise error
 */
int write_gpt_table(block_dev_desc_t *dev_desc,
		gpt_header *g_header, gpt_entry *g_entry)
{
	u32 calc_crc32;
	u32 entry_size;
	const u64 pte_blk_cnt = (u64) BLOCK_CNT((g_header->num_partition_entries
					   * sizeof(gpt_entry)), dev_desc);

	debugf("max lba of GPT: %x\n", (u32) dev_desc->lba);
	/* Setup the Protective MBR */
	if (set_protective_mbr(dev_desc) < 0)
		goto err;

	/* Generate CRC for the Primary GPT Header */
	entry_size = le32_to_cpu(g_header->num_partition_entries) *
			      le32_to_cpu(g_header->sizeof_partition_entry);
	calc_crc32 = gpt_crc32((const unsigned char *)g_entry, entry_size);
	g_header->partition_entry_array_crc32 = cpu_to_le32(calc_crc32);

	calc_crc32 = gpt_crc32((const unsigned char *)g_header,
			      le32_to_cpu(g_header->header_size));
	g_header->header_crc32 = cpu_to_le32(calc_crc32);

	/* Write the First GPT to the block right after the Legacy MBR */
	if (dev_desc->block_write(dev_desc->dev_num, 1, 1, g_header) != 1) {
		errorf("** %s Can't write gpt_header to lba1 for device %d **\n",
				__func__, dev_desc->dev_num);
		goto err;
	}

	if (dev_desc->block_write(dev_desc->dev_num, 2, pte_blk_cnt, g_entry)
	    != pte_blk_cnt) {
		errorf("** %s Can't write gpt_entry to lba2 for device %d **\n",
				__func__, dev_desc->dev_num);
		goto err;
	}

	backup_gpt_header_fill(dev_desc, g_header);

	if (dev_desc->block_write(dev_desc->dev_num,
				  (lbaint_t)le64_to_cpu(g_header->partition_entry_lba),
				  pte_blk_cnt, g_entry) != pte_blk_cnt) {
		errorf("** Can't write to device %d **\n", dev_desc->dev_num);
		goto err;
	}

	if (dev_desc->block_write(dev_desc->dev_num,
					(lbaint_t)le64_to_cpu(g_header->my_lba), 1,
					g_header) != 1) {
		errorf("** Can't write to device %d **\n", dev_desc->dev_num);
		goto err;
	}

	debugf("GPT successfully written to block device!\n");
	is_gpt_h_buf = 0;
	is_gpt_e_buf = 0;
	return 0;

err:
	is_gpt_h_buf = 0;
	is_gpt_e_buf = 0;
	return -1;
}

/**
 * gpt_pte_fill(): Fill GPT partition table entry.
 * return zero on success
 */
int gpt_pte_fill(gpt_header *g_header, gpt_entry *g_entry,
		disk_partition_t *partitions, int parts)
{
	lbaint_t lba_start;
	lbaint_t lba_offset = (lbaint_t)le64_to_cpu(g_header->first_usable_lba);
	lbaint_t last_usable_lba = (lbaint_t)le64_to_cpu(g_header->last_usable_lba);
	int i;
	unsigned int k;
	size_t gptname_len, partname_len;

#ifdef CONFIG_PARTITION_UUIDS
	char *str_uuid;
	unsigned char *bin_uuid;
#endif

	for (i = 0; i < parts; i++) {
		/* partition[i] start at lba num */
		lba_start = partitions[i].start_blk;

		if (lba_start) {
			if (lba_start && (lba_start < lba_offset)) {
				errorf("Overlap exist partition area!\n");
				return -1;
			} else {
				/* For start lba and partition lba offset */
				g_entry[i].starting_lba = cpu_to_le64(lba_start);
				lba_offset = lba_start + partitions[i].blk_cnt;
			}
		} else {
			/* No partition area info, shoule start by first_usable_lba */
			g_entry[i].starting_lba = cpu_to_le64(lba_offset);
			lba_offset += partitions[i].blk_cnt;
		}
		if (lba_offset >= last_usable_lba) {
			errorf("Partitions lab_offset exceds disk size\n");
			return -1;
		}
		/* Last partition[i] end at lba num */
		if ((i == parts - 1) && (partitions[i].blk_cnt == 0)) {
			/* config the last partition(always userdata) to maximuim lba */
#ifdef CONFIG_VERIFY_GPT
			if (gpt_key_cmd) {
				g_entry[i].ending_lba = 0;//g_header->last_usable_lba;
			} else {
				g_entry[i].ending_lba = g_header->last_usable_lba;
			}
#else
			g_entry[i].ending_lba = g_header->last_usable_lba;
#endif
		} else
			g_entry[i].ending_lba = cpu_to_le64(lba_offset - 1);

		/* Copy partition type_guidReserve  for each entry */
		memcpy(g_entry[i].partition_type_guid.b,
			&PARTITION_TYPE_GUID_BASIC, 16);

#ifdef CONFIG_PARTITION_UUIDS
		/* uuid string transfer to bin */
		str_uuid = partitions[i].uuid;
		bin_uuid = g_entry[i].unique_partition_guid.b;

		if (uuid_str_to_bin(str_uuid, bin_uuid, UUID_STR_FORMAT_GUID)) {
			errorf("Partition num. %d with invalid guid: %s\n",
				i, str_uuid);
			return -1;
		}
#endif

		/* Reserved attributes area for each entry */
		memset(&g_entry[i].attributes, 0, sizeof(gpt_entry_attributes));

		/* Loop for filling partition name */
		gptname_len = sizeof(g_entry[i].partition_name)
			/ sizeof(efi_char16_t);
		partname_len = sizeof(partitions[i].part_name);

		memset(g_entry[i].partition_name, 0,
		       sizeof(g_entry[i].partition_name));

		for (k = 0; k < min(partname_len, gptname_len); k++)
			g_entry[i].partition_name[k] =
				(efi_char16_t)(partitions[i].part_name[k]);

		debugf("%s: partition name: %s lba_offset[%d]: 0x" LBAF
		      " size[%d]: 0x" LBAF "\n",
		      __func__, partitions[i].part_name, i,
		      lba_offset, i, partitions[i].blk_cnt);
	}

	return 0;
}

/**
 * gpt_header_fill(): Fill the GPT header
 *
 * dev_desc - block device descriptor
 * gpt_h - GPT header representation
 * str_guid - disk guid string representation
 * parts_count - number of partitions
 *
 * return - error on str_guid conversion error
 */
int gpt_header_fill(block_dev_desc_t *dev_desc, gpt_header *g_header,
		char *str_guid, int parts_count)
{
	g_header->signat_value = cpu_to_le64(GPT_HEADER_SIGNATURE);
	g_header->revision = cpu_to_le32(GPT_HEADER_REVISION);
	g_header->header_size = cpu_to_le32(sizeof(gpt_header));
	g_header->my_lba = cpu_to_le64(1);
	g_header->alternate_lba = cpu_to_le64(dev_desc->lba - 1);
	g_header->first_usable_lba = cpu_to_le64(2 + (GPT_ENTRY_NUMBERS * GPT_ENTRY_SIZE / dev_desc->blksz));
	g_header->last_usable_lba = cpu_to_le64(dev_desc->lba - 1 - (SZ_2M / dev_desc->blksz));
	g_header->partition_entry_lba = cpu_to_le64(2);
	g_header->num_partition_entries = cpu_to_le32(GPT_ENTRY_NUMBERS);
	g_header->sizeof_partition_entry = cpu_to_le32(sizeof(gpt_entry));
	g_header->header_crc32 = 0;
	g_header->partition_entry_array_crc32 = 0;

	if (uuid_str_to_bin(str_guid, g_header->disk_guid.b, UUID_STR_FORMAT_GUID))
		return -1;

	return 0;
}

/**
 * gpt_info_fill(): Restore GPT partition table
 *
 * dev_desc - block device descriptor
 * str_disk_guid - disk GUID
 * partitions - list of partitions
 * parts - number of partitions
 *
 * return - 0 on success, others fail
 */
int gpt_info_fill(block_dev_desc_t *dev_desc, char *str_disk_guid,
		disk_partition_t *partitions, int parts_count)
{
	int ret = -1;
	uint32_t gpt_size = 0;
	gpt_header *gpt_h = NULL;
	gpt_entry *gpt_e = NULL;

#ifdef CONFIG_VERIFY_GPT
	char *gpt_sign_data = NULL;
	OPERATE_STATUS status = OPERATE_SUCCESS;
#endif

	gpt_size = PAD_TO_BLOCKSIZE(sizeof(gpt_header), dev_desc);
	gpt_h = malloc_cache_aligned(gpt_size);
	if (gpt_h == NULL) {
		errorf("%s: malloc gpt_header failed!\n", __func__);
		return -1;
	}
	memset(gpt_h, 0, gpt_size);

	gpt_size = PAD_TO_BLOCKSIZE(GPT_ENTRY_NUMBERS * sizeof(gpt_entry), dev_desc);
	gpt_e = malloc_cache_aligned(gpt_size);
	if (gpt_e == NULL) {
		errorf("%s: malloc gpt_entry failed!\n", __func__);
		goto err;
	}
	memset(gpt_e, 0, gpt_size);

	/* Generate Positive GPT header (LBA1), partition table entries and GPT partition table */
	ret = gpt_header_fill(dev_desc, gpt_h, str_disk_guid, parts_count);
	if (ret)
		goto err;

	/* Generate partition entries */
	ret = gpt_pte_fill(gpt_h, gpt_e, partitions, parts_count);
	if (ret)
		goto err;

#ifdef CONFIG_VERIFY_GPT
	if (gpt_key_cmd) {
		gpt_sign_data = memalign(SZ_4K, 0x1000);
		if (gpt_sign_data == NULL) {
			errorf("malloc gpt_sign_data error!\n");
			ret = -1;
			goto gpterr;
		}
		memset(gpt_sign_data, 0, 0x1000);

		memcpy(gpt_data.entry, gpt_e, SZ_E);
		memcpy(gpt_sign_data, gpt_data.header, SZ_H);
		memcpy(gpt_sign_data + SZ_H, gpt_data.key, SZ_K);

		status = dl_secboot_verify(0, "gpt", 0, 0, &gpt_data);
		if (status != OPERATE_SUCCESS) {
			ret = -1;
			goto gpterr;
		}

		ret = process_gpt_signdata(gpt_sign_data, SZ_H+SZ_K, 1, 0);
		if (ret!=0) {
			errorf("process signdata error!\n");
			goto gpterr;
		}

		ret = process_gpt_signdata(gpt_sign_data, SZ_H+SZ_K, 1, 1);
		if (ret!=0) {
			errorf("process backup signdata error!\n");
			goto gpterr;
		}
		gpt_e[parts_count-1].ending_lba = gpt_h->last_usable_lba;
	} else {
		dprintf(INFO, "GPT efuse bit not enable, do not need verify gpt!\n");
	}
#endif

	/* Write GPT partition table */
	ret = write_gpt_table(dev_desc, gpt_h, gpt_e);

#ifdef CONFIG_VERIFY_GPT
gpterr:
	if (gpt_sign_data) {
		free(gpt_sign_data);
		gpt_sign_data = NULL;
	}
#endif

err:
	if (gpt_e) {
		free(gpt_e);
		gpt_e = NULL;
	}
	if (gpt_h) {
		free(gpt_h);
		gpt_h = NULL;
	}
	return ret;
}

/**
 * is_gpt_mem_validate() - Ensure that the Primary GPT information is valid.
 * return - '0' on success, otherwise error
 */
int is_gpt_mem_validate(block_dev_desc_t *dev_desc, void *buf)
{
	gpt_header *g_header;
	gpt_entry *g_entry;

	/* Get GPT Header with buffer and primary table offset */
	g_header = buf + (GPT_PRIMARY_PARTITION_LBA *
		       dev_desc->blksz);
	if (is_gpt_header_validate(g_header, GPT_PRIMARY_PARTITION_LBA,
				dev_desc->lba))
		return -1;

	/* Get GPT Entries with buffer and entry_lba offset */
	g_entry = buf + (le64_to_cpu(g_header->partition_entry_lba) * dev_desc->blksz);
	if (is_gpt_entry_validate(g_header, g_entry))
		return -1;

	return 0;
}

/**
 * write_mbr_and_gpt_partitions() - write MBR, Primary GPT and Backup GPT.
 * return - 0 on success, otherwise error
 */
int write_mbr_and_gpt_partitions(block_dev_desc_t *dev_desc, void *buf)
{
	gpt_header *g_header;
	gpt_entry *g_entry;
	u32 gpt_e_blk_cnt, cnt, i, calc_crc32;
	uchar dpart_name[36] = {0};
	uint32_t entry_size;
	lbaint_t lba;

	if (is_gpt_mem_validate(dev_desc, buf))
		return -1;

	/* determine start of GPT Header in the buffer  GPT_PRIMARY_PARTITION_LBA == 1ULL */
	g_header = buf + (GPT_PRIMARY_PARTITION_LBA * dev_desc->blksz);
	/* determine start of GPT Entry in the buffer */
	g_entry = buf + (le64_to_cpu(g_header->partition_entry_lba) *
			dev_desc->blksz);

	entry_size = le32_to_cpu(g_header->num_partition_entries) *
				le32_to_cpu(g_header->sizeof_partition_entry);
	gpt_e_blk_cnt = BLOCK_CNT(entry_size, dev_desc);

	g_header->alternate_lba = cpu_to_le64(dev_desc->lba - 1);
	g_header->last_usable_lba = cpu_to_le64(dev_desc->lba - 1 - (SZ_2M / dev_desc->blksz));

	/* Resize userdata end-lba */
	while (g_entry->starting_lba != 0) {
		for (i = 0; i < strlen("userdata"); i++) {
			dpart_name[i] = (efi_char16_t)g_entry->partition_name[i];
		}

		if (!strcmp(dpart_name, "userdata")) {
			g_entry->ending_lba = g_header->last_usable_lba;
			break;
		}
		g_entry += 1;
		memset(dpart_name, 0, 36);
	}

	if (0 == g_entry->starting_lba) {
		errorf("GPT table is lack of userdata partition\n");
		return 1;
	}

	g_entry = buf + (le64_to_cpu(g_header->partition_entry_lba) *
				dev_desc->blksz);

	/* Generate CRC for the Primary GPT Header */
	calc_crc32 = gpt_crc32((const unsigned char *)g_entry, entry_size);
	g_header->partition_entry_array_crc32 = cpu_to_le32(calc_crc32);
	g_header->header_crc32 = 0;
	calc_crc32 = gpt_crc32((const unsigned char *)g_header,
					le32_to_cpu(g_header->header_size));
	g_header->header_crc32 = cpu_to_le32(calc_crc32);

	/* write MBR, location is always at 0 (1 block) */
	lba = 0;
	cnt = 1;
	if (dev_desc->block_write(dev_desc->dev_num, lba, cnt, buf) != cnt) {
		errorf("%s: failed writing MBR (%d blks at 0x" LBAF ")\n",
		       __func__, cnt, lba);
		return 1;
	}

	/* write Primary GPT */
	lba = GPT_PRIMARY_PARTITION_LBA;/*primary table use lba1 */
	cnt = 1;	/* GPT Header use 1 block */
	if (dev_desc->block_write(dev_desc->dev_num, lba, cnt, g_header) != cnt) {
		errorf("%s: failed writing '%s' (%d blks at 0x" LBAF ")\n",
		       __func__, "Primary GPT Header", cnt, lba);
		is_gpt_h_buf = 0;
		return 1;
	}
	is_gpt_h_buf = 0;
	lba = le64_to_cpu(g_header->partition_entry_lba);
	cnt = gpt_e_blk_cnt;
	if (dev_desc->block_write(dev_desc->dev_num, lba, cnt, g_entry) != cnt) {
		errorf("%s: failed writing '%s' (%d blks at 0x" LBAF ")\n",
		       __func__, "Primary GPT Entries", cnt, lba);
		is_gpt_e_buf = 0;
		return 1;
	}
	is_gpt_e_buf = 0;
	backup_gpt_header_fill(dev_desc, g_header);

	/* write Backup GPT */
	lba = le64_to_cpu(g_header->partition_entry_lba);
	cnt = gpt_e_blk_cnt;
	if (dev_desc->block_write(dev_desc->dev_num, lba, cnt, g_entry) != cnt) {
		errorf("%s: failed writing '%s' (%d blks at 0x" LBAF ")\n",
		       __func__, "Backup GPT Entries", cnt, lba);
		return 1;
	}

	lba = le64_to_cpu(g_header->my_lba);
	cnt = 1;	/* GPT Header (1 block) */
	if (dev_desc->block_write(dev_desc->dev_num, lba, cnt, g_header) != cnt) {
		errorf("%s: failed writing Backup GPT Header (%d blks at 0x" LBAF ")\n",
		       __func__, cnt, lba);
		return 1;
	}
	/* Update gpt buffer */
	memcpy(&gpt_h_buf, g_header, sizeof(gpt_header));
	memcpy(gpt_e_buf, g_entry, GPT_ENTRY_NUMBERS * GPT_ENTRY_SIZE);

	return 0;
}
#endif

/*
 * is_pmbr_part_validate(): Check for EFI partition signature
 * GPT_EFI_PMBR_SYSIND = 0xEE
 */
static int is_pmbr_part_validate(struct partition *part)
{
	if (part->sys_ind == GPT_EFI_PMBR_SYSIND &&
		get_unaligned_le32(&part->start_sect) == 1UL) {
		return 1;
	}

	return 0;
}

/*
 * is_pmbr_validate(): check Protective MBR whether validity
 *
 * return - 1 if PMBR is valid, 0 otherwise.
 */
static int is_pmbr_validate(protective_mbr * pmbr)
{
	int i = 0;

	if (!pmbr || le16_to_cpu(pmbr->signature) != GPT_PMBR_SIGNATURE)
		return 0;

	for (i = 0; i < 4; i++) {
		if (is_pmbr_part_validate(&pmbr->partition_record[i])) {
			return 1;
		}
	}
	return 0;
}

/**
 * is_gpt_validate() - Check GPT header and Partition Table Entries whether valid or not.
 * pg_header: point to GPT header
 * pg_entries: point to Partition entry
 * return: 1 on success, otherwise error.
 */
static int is_gpt_validate(block_dev_desc_t *dev_desc, u64 lba,
			gpt_header *pg_header, gpt_entry **pg_entry)
{
	if (!dev_desc || !pg_header) {
		errorf("%s: Invalid Argument(s)\n", __func__);
		return 0;
	}
	if (lba != 1 || is_gpt_h_buf == 0) {
		/* Read GPT Header from device descriptor and lba */
		if (dev_desc->block_read(dev_desc->dev_num, (lbaint_t)lba, 1, pg_header)
				!= 1) {
			errorf("*** ERROR: Can't read GPT header from device %d***\n", dev_desc->dev_num);
			is_gpt_h_buf = 0;
			is_gpt_e_buf = 0;
			return 0;
		}
		/* only copy original GPT Header once */
		if (lba == 1 && is_gpt_h_buf == 0) {
			memcpy(&gpt_h_buf, pg_header, sizeof(gpt_header));
			is_gpt_h_buf = 1;
		}
	} else {
		memcpy(pg_header, &gpt_h_buf, sizeof(gpt_header));
	}

	if (is_gpt_header_validate(pg_header, (lbaint_t)lba, dev_desc->lba))
		return 0;

	/* Allocate Partition Table Entries and check it */
	*pg_entry = alloc_read_gpt_entries(dev_desc, pg_header);
	if (*pg_entry == NULL) {
		errorf("GPT: Failed to allocate memory for PTE\n");
		return 0;
	}

	if (is_gpt_entry_validate(pg_header, *pg_entry)) {
		free(*pg_entry);
		*pg_entry = NULL;
		return 0;
	}

	/* GPT all's validate */
	return 1;
}

/**
 * alloc_read_gpt_entries(): reads partition entries from disk.
 * pg_header - pointer to GPT header
 */
static gpt_entry *alloc_read_gpt_entries(block_dev_desc_t * dev_desc,
					 gpt_header * pg_header)
{
	size_t entry_size = 0, blk_cnt;
	size_t buffer_size = 0;
	gpt_entry *g_entry = NULL;

	if (!dev_desc || !pg_header) {
		errorf("%s: Invalid Argument(s)\n", __func__);
		return NULL;
	}

	entry_size = le32_to_cpu(pg_header->num_partition_entries) *
		le32_to_cpu(pg_header->sizeof_partition_entry);
	/*
	debugf("%s: size of partition entries = %u(num) * %u(sizeof) = %zu\n", __func__,
	      (u32) le32_to_cpu(pg_header->num_partition_entries),
	      (u32) le32_to_cpu(pg_header->sizeof_partition_entry), entry_size);
	*/
	/* Apply memory for Partition Table Entries */
	buffer_size = PAD_TO_BLOCKSIZE(entry_size, dev_desc);
	if (entry_size != 0) {
		g_entry = malloc_cache_aligned(buffer_size);
		if (g_entry != NULL) {
			memset(g_entry, 0, buffer_size);
		} else {
			errorf("no enough memory for g_entry\n");
			return NULL;
		}
	} else {
		errorf("%s: ERROR: Can't allocate 0x%zX "
		       "bytes for GPT Entries\n",
			__func__, entry_size);
		return NULL;
	}

	/* Read GPT Entries from device */
	blk_cnt = BLOCK_CNT(entry_size, dev_desc);
	if (le64_to_cpu(pg_header->partition_entry_lba) != 2 || is_gpt_e_buf == 0) {
	/* Read and allocate Partition Table Entries */
		if (dev_desc->block_read (dev_desc->dev_num,
			(lbaint_t)le64_to_cpu(pg_header->partition_entry_lba),
			(lbaint_t) (blk_cnt), g_entry) != blk_cnt) {
			errorf("%s: ERROR: Can't read GPT Entries from device %d ***\n",
				__func__, dev_desc->dev_num);
			is_gpt_h_buf = 0;
			is_gpt_e_buf = 0;
			if (g_entry) {
				free(g_entry);
				g_entry = NULL;
			}
			return NULL;
		}
		/* only copy original Partition Table Entries once */
		if (le64_to_cpu(pg_header->partition_entry_lba) == 2 && is_gpt_e_buf == 0) {
			memcpy(gpt_e_buf, g_entry, buffer_size);
			is_gpt_e_buf = 1;
		}
	} else {
		memcpy(g_entry, gpt_e_buf, buffer_size);
	}

	return g_entry;
}

/**
 * is_pte_validate(): Verify Partition Table Entry
 * returns - 1 if valid,  0 on error.
 */
static int is_pte_validate(gpt_entry * g_entry)
{
	efi_guid_t unused_guid;

	if (!g_entry) {
		errorf("%s: Invalid Argument(s)\n", __func__);
		return 0;
	}

	/* Only one validation for now:
	 * The GUID Partition Type != Unused Entry (ALL-ZERO)
	 */
	memset(unused_guid.b, 0, sizeof(unused_guid.b));

	if (memcmp(g_entry->partition_type_guid.b, unused_guid.b,
		sizeof(unused_guid.b)) == 0) {
		errorf("%s: Found an unused PTE GUID at 0x%08X\n", __func__,
		      (unsigned int)(uintptr_t)g_entry);
		return 0;
	} else {
		return 1;
	}
}

//Fixme:move these functions to lib/libc/atoi.c, then include stdlib.h
unsigned long simple_strtoul(const char *cp, char **endp,
				unsigned int base)
{
	int neg = 0;
    unsigned long ret = 0;

    if (base == 1 || base > 36) {
        return EINVAL;
    }

    while (isspace(*cp)) {
        cp++;
    }

    if (*cp == '+') {
        cp++;
    } else if (*cp == '-') {
        neg = 1;
        cp++;
    }

    if ((base == 0 || base == 16) && cp[0] == '0' && cp[1] == 'x') {
        base = 16;
        cp += 2;
    } else if (base == 0 && cp[0] == '0') {
        base = 8;
        cp++;
    } else if (base == 0) {
        base = 10;
    }

    for (;;) {
        char c = *cp;
        int v = -1;
        unsigned long new_ret;

        if (c >= 'A' && c <= 'Z') {
            v = c - 'A' + 10;
        } else if (c >= 'a' && c <= 'z') {
            v = c - 'a' + 10;
        } else if (c >= '0' && c <= '9') {
            v = c - '0';
        }

        if (v < 0 || (unsigned int)v >= base) {
            if (endp) {
                *endp = (char *) cp;
            }
            break;
        }

        new_ret = ret * base;
        if (new_ret / base != ret ||
                new_ret + v < new_ret ||
                ret == ULONG_MAX) {
            ret = ULONG_MAX;
            return ERANGE;
        } else {
            ret = new_ret + v;
        }

        cp++;
    }

    if (neg && ret != ULONG_MAX) {
        ret = -ret;
    }

    return ret;
}

unsigned long long simple_strtoull(const char *cp, char **endp,
					unsigned int base)
{
    int neg = 0;
    unsigned long long ret = 0;

    if (base == 1 || base > 36) {
        return EINVAL;
    }

    while (isspace(*cp)) {
        cp++;
    }

    if (*cp == '+') {
        cp++;
    } else if (*cp == '-') {
        neg = 1;
        cp++;
    }

    if ((base == 0 || base == 16) && cp[0] == '0' && cp[1] == 'x') {
        base = 16;
        cp += 2;
    } else if (base == 0 && cp[0] == '0') {
        base = 8;
        cp++;
    } else if (base == 0) {
        base = 10;
    }

    for (;;) {
        char c = *cp;
        int v = -1;
        unsigned long long new_ret;

        if (c >= 'A' && c <= 'Z') {
            v = c - 'A' + 10;
        } else if (c >= 'a' && c <= 'z') {
            v = c - 'a' + 10;
        } else if (c >= '0' && c <= '9') {
            v = c - '0';
        }

        if (v < 0 || (unsigned int)v >= base) {
            if (endp) {
                *endp = (char *) cp;
            }
            break;
        }

        new_ret = ret * base;
        if (new_ret / base != ret ||
                new_ret + v < new_ret ||
                ret == ULONG_MAX) {
            ret = ULONG_MAX;
            return ERANGE;
        } else {
            ret = new_ret + v;
        }

        cp++;
    }

    if (neg && ret != ULONG_MAX) {
        ret = -ret;
    }

    return ret;
}
#endif
