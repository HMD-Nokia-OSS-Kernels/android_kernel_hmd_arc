/*
 * Copyright (c) 2015 Steve White
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#pragma once

//#include <lib/bio.h>
#include <lib/bcache.h>
#include <part.h>

typedef struct {
    block_dev_desc_t *dev;
    bcache_t cache;

    uint32_t lba_start;
    uint64_t part_start;
    uint64_t total_part_size;
    uint64_t part_block_size;

    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_cluster;
    uint32_t reserved_sectors;
    uint32_t fat_bits;
    uint32_t fat_count;
    uint32_t sectors_per_fat;
    uint32_t total_sectors;
    uint32_t active_fat;
    uint32_t data_start;
    uint32_t total_clusters;
    uint32_t root_cluster;
    uint32_t root_entries;
    uint32_t root_start;
    uint32_t free_cluster;
} fat_fs_t;

typedef struct {
    fat_fs_t *fat_fs;
    uint32_t start_cluster;
    uint32_t length;
    uint8_t attributes;
    uint32_t father_cluster;
    uint32_t fclus_offset;
} fat_file_t;

typedef struct {
    fat_fs_t *fat_fs;
    uint32_t start_cluster;
    uint32_t read_offset;
} fat_dir_t;


typedef struct {
    char name[11];         /* Name and extension */
    uint8_t attr;             /* Attribute bits */
    uint8_t lcase;           /* Case for base and extension */
    uint8_t ctime_ms;    /* Creation time, milliseconds */
    uint16_t ctime;        /* Creation time */
    uint16_t cdate;        /* Creation date */
    uint16_t adate;       /* Last access date */
    uint16_t starthi;      /* High 16 bits of cluster in FAT32 */
    uint16_t time,date,start; /* Time, date and first cluster */
    uint32_t size;          /* File size in bytes */
} short_dir_entry;

typedef struct {
    uint8_t LDIR_ord;
    uint8_t name0[10];
    uint8_t lattr;
    uint8_t res0;
    uint8_t chksum;
    uint8_t name1[12];
    uint16_t res1;
    uint8_t name2[4];
} long_dir_entry;



typedef struct {
    uint8_t attr;
    uint32_t cluster;
    uint32_t offset;
    uint32_t lfn_sequences;
} fat_dircluster_info;


typedef enum {
    fat_attribute_read_only = 0x01,
    fat_attribute_hidden = 0x02,
    fat_attribute_system = 0x04,
    fat_attribute_volume_id = 0x08,
    fat_attribute_directory = 0x10,
    fat_attribute_archive = 0x20,
    fat_attribute_lfn = fat_attribute_read_only | fat_attribute_hidden | fat_attribute_system | fat_attribute_volume_id,
} fat_attributes;

#define fat_read32(buffer,off) \
(((uint8_t *)buffer)[(off)] + (((uint8_t *)buffer)[(off)+1] << 8) + \
(((uint8_t *)buffer)[(off)+2] << 16) + (((uint8_t *)buffer)[(off)+3] << 24))

#define fat_read16(buffer,off) \
(((uint8_t *)buffer)[(off)] + (((uint8_t *)buffer)[(off)+1] << 8))

