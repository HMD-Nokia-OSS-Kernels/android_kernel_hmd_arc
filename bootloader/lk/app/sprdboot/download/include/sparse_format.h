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

#ifndef _SPARSE_FORMAT_H_
#define _SPARSE_FORMAT_H_

#include <linux/types.h>

/* sparse header magic to verify */
#define SPARSE_HEADER_MAGIC	0xed26ff3a
#define SPARSE_HEADER_MAJOR_VER 1

/* chunk type in sparse struct */
#define CHUNK_TYPE_RAW		0xCAC1
#define CHUNK_TYPE_FILL		0xCAC2
#define CHUNK_TYPE_DONT_CARE	0xCAC3
#define CHUNK_TYPE_CRC32    0xCAC4

typedef struct sparse_header {
  __le32	magic;		/* magic to verify sparse header (0xed26ff3a) */
  __le16	major_version;	/* (0x1) - only suppoort 0x1 major versions sparse image */
  __le16	minor_version;	/* (0x0) - allow higer minor versions sparse images with */
  __le16	file_hdr_sz;	/* sparse image header size, used 28 bytes */
  __le16	chunk_hdr_sz;	/* each trunk header size in sparse header struct, used 12 bytes */
  __le32	blk_sz;		/* must be a multiple of 4 (4096), block size in sparse image(unit: bytes) */
  __le32	total_blks;	/* sparse image generate total blocks to raw image */
  __le32	total_chunks;	/* total chunks in sparse image */
  __le32	image_checksum; /* CRC32 checksum of the original data, counting "don't care" */
				/* as 0. Standard 802.3 polynomial, use a Public Domain */
				/* table implementation */
} sparse_header_t;

#ifdef SPRD_SPARSE_SUPER_SPEEDUP
extern sparse_header_t sparse_header;
extern u32 total_blocks;
#endif

/* Following a Raw or Fill or CRC32 chunk is data, Dont care type need skip.
 *  As for Raw type chunk, it's fill with data, data size = chunk_sz * blk_sz.
 *  As for Fill type chunk, it's 4 bytes of the fill data.
 *  As for Dont care type chunk, it's no data, need skip total chunk size in flash.
 *  As for CRC32 type chunk, it's 4 bytes of CRC32.
 */
typedef struct chunk_header {
  __le16	chunk_type;	/* raw; fill; don't care; crc32 */
  __le16	reserved1;	/* reserved area */
  __le32	chunk_sz;	/* number of block contain in curent chunk */
  __le32	total_sz;	/* total size(in bytes) of curent chunk info in chunk header */
} chunk_header_t;

#endif
