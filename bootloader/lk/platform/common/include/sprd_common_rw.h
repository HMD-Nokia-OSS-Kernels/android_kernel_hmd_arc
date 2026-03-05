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
#ifndef _SPRD_COMMON_RW_H_
#define _SPRD_COMMON_RW_H_
#include <sprd_common.h>
#include <part.h>
#include <lk/board.h>
#define SPRD_DISK_GUID_STR "11111111-2222-3333-4444-000000000000"

enum block_boot_part {
	USER_PART       = 0x00,
	BOOT_PART1      = 0x01,
	BOOT_PART2      = 0x02
};

#ifdef CONFIG_VERIFY_GPT
typedef enum falsh_ops {
	READ_OPS = 0,
	WRITE_OPS = 1,
	MAX_OPS = 2
}enFlashOps;
#endif

#ifdef CONFIG_EMMC_WP
#define ENABLE_WRITE_PROTECT	1
#define DISABLE_WRITE_PROTECT	0

int common_set_powp(char *part_name);
#endif
int common_raw_read(const char *part_name, uint64_t size, uint64_t offset, char *buf);
int common_raw_write(const char *part_name, uint64_t size, uint64_t updsize, uint64_t offset, char *buf);
int common_raw_erase(const char *part_name, uint64_t size, uint64_t offset);
int common_repartition(disk_partition_t *partitions, int parts_count);
int32_t common_get_lba_size(void);
int do_fs_file_read(const char *mpart, const char *filenm, void *buf, int len);
int do_fs_file_write(const char *mpart, const char *filenm, void *buf, int len);
int common_query_backstage(const char *part_name, uint32_t size, uchar *buf);
int common_write_backstage(const char *part_name, uint32_t size, uint64_t offset, uchar *buf);
int common_read_backstage(const char *part_name, uint64_t size, uint64_t offset, uchar *buf);
int common_read_query_backstage(const char *part_name, uint64_t size, uchar *buf);
int write_sparse_img(const char * partname, char* buf, unsigned long length);
void reset_sparse_status(void);
int get_partition_info_by_name(block_dev_desc_t *dev_desc, const char* partition_name, disk_partition_t *info);
int fdt_initrd_norsvmem(void *fdt, ulong initrd_start, ulong initrd_end, int force);
int boot_sprdisk(int offset, char *ramdisk_addr);
char *block_dev_get_name(void);

#ifdef CONFIG_VERIFY_GPT
int process_gpt_signdata(char *gpt_signdata, uint64_t size, enFlashOps ops, int backup);
int process_gpt_entry(char *gpt_entry, uint64_t size, enFlashOps ops, int backup);
int get_gpt_data(uint8_t *gpt_data_info, int backup);
#endif

#ifdef CONFIG_NAND_BOOT
int do_raw_data_write(char *part, u32 updsz, u32 size, u32 off, char *buf);
#endif
int get_img_partition_size(const char *part_name, uint64_t *size);
int get_img_partition_info(const char *part_name, disk_partition_t *part_info);
int get_partition_information(disk_partition_t *info, u32 *cnt);
int common_partition_sync(const char *part_name);
#endif /* _LOADER_COMMON_H_ */
