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

#include <linux/types.h>
#include <part.h>
#include <sparse_format.h>
#include <sprd_common_rw.h>
#include <sparse_crc32.h>
#include <string.h>
#include <errno.h>
#ifdef CONFIG_WR_SPARSE
#include <fb_sparse.h>
#endif
#include <boot_mode.h>
#ifdef SPRD_SPARSE_SUPER_SPEEDUP
#include "dl_operate.h"
#include "dl_cmd_proc.h"
#include <sprd_sizes.h>

//#define DOWNLOAD_DEBUG 1
#ifdef DOWNLOAD_DEBUG
#define prt(fmt, args...) do { dprintf(INFO,"sparse_download %s(): ", __func__);dprintf(INFO,fmt, ##args); } while (0)
#else
#define prt(fmt, args...)
#endif

extern ALTER_BUFFER_ATTR* sparse_cur_buf;
extern ALTER_BUFFER_ATTR* sparse_temp_buf;
extern ALTER_BUFFER_ATTR* sparse2emmc_buf;
extern ALTER_BUFFER_ATTR* emmc_cur_buf;

uint64_t fill_chunk_temp = 0; //raw\fill type emmc buffer(128MB) not enough,for temp record
uint64_t sparse2emmc_temp = 0; //resolve emmc block/offset align
uint64_t have_dont_care = 0;
uint64_t dont_care_temp = 0;
extern int write_sparse2emmc_img(block_dev_desc_t *dev_desc);
#endif

#define COPY_BUF_SIZE (1024*1024)
#define SPARSE_HEADER_LEN       (sizeof(sparse_header_t))
#define CHUNK_HEADER_LEN (sizeof(chunk_header_t))

/* modify for download very big size ext4 image */
typedef enum EXT4_DL_STATUS_DEF
{
	START = 0,
	END
} EXT4_DL_STATUS_E;

typedef struct {
	chunk_header_t header;    /* chunk header */
	unsigned int idx;	  /* index of chunk */
	u64 saved_len;            /* length of chunk which was already saved */
} uncomplete_chunk_t;

static u8 copybuf[COPY_BUF_SIZE];
static unsigned int g_buf_index = 0;

#ifdef SPRD_SPARSE_SUPER_SPEEDUP
sparse_header_t sparse_header;
u32 total_blocks = 0;
#else
static sparse_header_t sparse_header;
static u32 total_blocks = 0;
#endif

static EXT4_DL_STATUS_E download_status = END;
static unsigned long current_chunks = 0;
static uint64_t sparse_offset = 0;
static uncomplete_chunk_t uncomplete_ck;/* save chunk relative information */

extern int get_ab_partition(char *);
#ifdef CONFIG_WR_SPARSE
#define DBG_HIST_MAX		128
typedef struct {
	u32 *chunk_addr;
	chunk_header_t hdr;
} sparse_img_dbg_t;
static sparse_img_dbg_t dbg_his[DBG_HIST_MAX];
static int his_c;
#endif

static int read_all(char *buf_src, u32 src_index, void *buf_dest, size_t len)
{
	memcpy(buf_dest, (void*)(buf_src + src_index), len);
	g_buf_index += len;
	return len;
}

static int preread_chunk(char *buf_src, u32 src_index, void *buf_dest, size_t len)
{
	memcpy(buf_dest, (void*)(buf_src + src_index), len);
	return len;
}


static int process_crc32_chunk(void *buf, u32 crc32)
{
	u32 file_crc32;
	int ret;

	ret = read_all(buf, g_buf_index, &file_crc32, 4);
	if (ret != 4) {
		errorf("read returned an error copying a crc32 chunk\n");
		return -1;
	}

	if (file_crc32 != crc32) {
		errorf("computed crc32 of 0x%8.8x, expected 0x%8.8x\n",
			 crc32, file_crc32);
		return -1;
	}

	return 0;
}

#ifdef CONFIG_WR_SPARSE
static void dbg_his_print(void)
{
	chunk_header_t *hdr;
	int i;
	u32 *p;

	dprintf(INFO,"%3s %4s %8s %8s\n", "cks", "type", "chunk_sz", "total_sz");
	dprintf(INFO,"--- ---- -------- --------\n");
	for (i = 0; i <= his_c && i < DBG_HIST_MAX; i++) {
		hdr = &dbg_his[i].hdr;
		dprintf(INFO,"%03d %04x %08x %08x\n", i, hdr->chunk_type, hdr->chunk_sz,
			hdr->total_sz);
	}

	for (i = 0; i <= his_c && i < DBG_HIST_MAX; i++) {
		p = dbg_his[i].chunk_addr - 4;
		dprintf(INFO,"%p: %08x %08x %08x %08x\n", p, *p, *(p + 1), *(p + 2), *(p + 3));
		p += 4;
		dprintf(INFO,"%p: %08x %08x %08x %08x\n", p, *p, *(p + 1), *(p + 2), *(p + 3));
		p += 4;
		dprintf(INFO,"%p: %08x %08x %08x %08x\n\n", p, *p, *(p + 1), *(p + 2), *(p + 3));
	}

	if (his_c && dbg_his[his_c].chunk_addr) {
		p = (u32 *)((u8 *)dbg_his[his_c].chunk_addr + dbg_his[his_c].hdr.total_sz) - 4;
		dprintf(INFO,"%p: %08x %08x %08x %08x\n", p, *p, *(p + 1), *(p + 2), *(p + 3));
		p += 4;
		dprintf(INFO,"%p: %08x %08x %08x %08x\n", p, *p, *(p + 1), *(p + 2), *(p + 3));
		p += 4;
		dprintf(INFO,"%p: %08x %08x %08x %08x\n\n", p, *p, *(p + 1), *(p + 2), *(p + 3));
	}
}
#endif

/* reset status when flash sparse failed */
void reset_sparse_status(void) {
	download_status = END;
}

/* -1 : error; x : buf is finished to write */
int write_sparse_img(const char * partname, char* buf, unsigned long length)
{
	unsigned int i;
	unsigned int j;
	chunk_header_t chunk_header;
	u32 crc32 = 0;
	int ret;
	uint64_t chunk_len = 0;
	int chunk;
	u32 fill_val;
	u32 *fillbuf;
	u64 sparse_sz, part_sz;
	const char temp[32] = {0};
	int v_ab_flag = !get_ab_partition(temp);
	int succ;

	g_buf_index = 0;

	if (download_status == END) {
		memset(&uncomplete_ck, 0, sizeof(uncomplete_ck));
		uncomplete_ck.idx = -1;
		current_chunks = 0;
		total_blocks = 0;
		sparse_offset = 0;
		memset(&sparse_header, 0, sizeof(sparse_header_t));
		ret = read_all(buf, g_buf_index, &sparse_header, sizeof(sparse_header));
		debugf("sparse_header file_hdr_sz = %d chunk_hdr_sz = %d blk_sz = %d total_blks = %d total_chunks = %d\n",
			sparse_header.file_hdr_sz, sparse_header.chunk_hdr_sz, sparse_header.blk_sz,

			sparse_header.total_blks, sparse_header.total_chunks);
		if (ret != sizeof(sparse_header)) {
			errorf("Error reading sparse file header\n");
			goto fail;
		}
		if (sparse_header.magic != SPARSE_HEADER_MAGIC) {
			errorf("Bad magic\n");
			goto fail;
		}
		if (sparse_header.major_version != SPARSE_HEADER_MAJOR_VER) {
			errorf("Unknown major version number\n");
			goto fail;
		}

		if (sparse_header.file_hdr_sz > SPARSE_HEADER_LEN) {
			/* Skip the remaining bytes in a header that is longer than
			* we expected.
			 */
			g_buf_index += (sparse_header.file_hdr_sz - SPARSE_HEADER_LEN);
		}
		download_status = START;
#ifdef CONFIG_WR_SPARSE
		memset(dbg_his, 0, sizeof(dbg_his));
		his_c = 0;
#endif
	}

	for (i = current_chunks; i < sparse_header.total_chunks; i++) {
		memset(&chunk_header, 0, sizeof(chunk_header_t));
		if ((g_buf_index + sizeof(chunk_header)) > length) {
			debugf("bufferindex(%d) + sizeof(chunk_header)(%lu) exceed length\n", g_buf_index, sizeof(chunk_header));
			current_chunks = i;
			break;
		}

		if (((!i) && (uncomplete_ck.idx == -1)) || (i != uncomplete_ck.idx)) {
			preread_chunk((void*)buf, g_buf_index, &chunk_header, sizeof(chunk_header));
			//debugf("chunk_header.total_sz(%d)\n", chunk_header.total_sz);

			/*
			 * fixup for "fastboot flash -S 200M system system.img"
			 *   1. first block(200M): the first chunk type must not be 0xCAC3
			 *   2. other blocks: the first chuck type is 0xCAC3 and chunk_len was offset continue
			 *   from last block
			 */
			if (!sparse_offset && !current_chunks
				&& (chunk_header.chunk_type != CHUNK_TYPE_DONT_CARE)
				&& strcmp(partname, "userdata")) {
				/* erase partition before spare image write */
				sparse_sz = (u64)sparse_header.total_blks * (u64)sparse_header.blk_sz;
				if (
#ifdef CONFIG_WR_SPARSE
					(NULL != g_env_bootmode && !strcmp(g_env_bootmode, "download")) &&
#endif
				sparse_sz ) {
					if ((get_img_partition_size(partname, &part_sz) >= 0)
							&& (part_sz > 0)) {
						if (sparse_sz > part_sz) {
							errorf("fail: sparse sz(0x%llx) large than partition sz(0x%llx)\n", sparse_sz, part_sz);
							goto fail;
						} else {
							if (!strcmp(partname, "super")) {
								dprintf(INFO,"do not need erase super partition\n");
							} else {
								debugf("??? sparse img write, erase partition %s sz 0x%llx\n", partname, sparse_sz);
								if (common_raw_erase(partname, sparse_sz > part_sz ? part_sz : sparse_sz, sparse_offset)) {
									errorf("sparse image write, erase partition %s fail\n", partname);
									goto fail;
								}
							}
						}
					}
				}
			}

			ret = 0;
			switch (chunk_header.chunk_type) {
			case CHUNK_TYPE_RAW:
			case CHUNK_TYPE_FILL:
			case CHUNK_TYPE_DONT_CARE:
			case CHUNK_TYPE_CRC32:
				if ((g_buf_index + chunk_header.total_sz) > length) {
					debugf("bufferindex(%d)+chunk_header.total_sz(%d) exceed  length(%lu) \n", g_buf_index, chunk_header.total_sz, length);
					ret = 1;
				}
				break;
			default:
				errorf("Unknown chunk type 0x%4.4x\n", chunk_header.chunk_type);
#ifdef CONFIG_WR_SPARSE
				{
					int k;
					u32 *p = (u32 *)((u8 *)buf + g_buf_index) - 4;
					errorf("%p: %08x %08x %08x %08x\n", p, *(p), *(p + 1), *(p + 2), *(p + 3));
					p += 4;
					errorf("%p: %08x %08x %08x %08x\n", p, *(p), *(p + 1), *(p + 2), *(p + 3));
					p += 4;
					errorf("%p: %08x %08x %08x %08x\n", p, *(p), *(p + 1), *(p + 2), *(p + 3));
				}
				dbg_his_print();
#endif
				goto fail;
			}

			if (ret == 1) {
				/* save uncomplete chunk header only for type RAW */
				current_chunks = i;
				if (chunk_header.chunk_type == CHUNK_TYPE_RAW) {
					memcpy(&uncomplete_ck.header, &chunk_header, sizeof(chunk_header_t));
					uncomplete_ck.idx = i;
					uncomplete_ck.saved_len = 0;
				} else if ((chunk_header.chunk_type == CHUNK_TYPE_FILL)
							|| (chunk_header.chunk_type == CHUNK_TYPE_DONT_CARE)) {
					debugf("chunk_type %x current_chunks %lu g_buf_index %x, continue for next\n",
						chunk_header.chunk_type, current_chunks, g_buf_index);
					break;
				} else {
					errorf("no support type chunk_type %x\n", chunk_header.chunk_type);
#ifdef CONFIG_WR_SPARSE
					dbg_his_print();
#endif
					goto fail;
				}
			}

			memset(&chunk_header, 0, sizeof(chunk_header_t));
			ret = read_all((void*)buf, g_buf_index, &chunk_header, sizeof(chunk_header));

			if (ret != sizeof(chunk_header)) {
				errorf("Error reading chunk header\n");
				goto fail;
			}

#ifdef CONFIG_WR_SPARSE
			if ((i < DBG_HIST_MAX)
				&& !dbg_his[i].chunk_addr) {
				memcpy(&dbg_his[i].hdr, &chunk_header, sizeof(chunk_header));
				dbg_his[i].chunk_addr = (u32 *)((u8 *)buf + g_buf_index);
				his_c = i;
			}
#endif

			if (sparse_header.chunk_hdr_sz > CHUNK_HEADER_LEN) {
				/* Skip the remaining bytes in a header that is longer than
				 * we expected.*/
				g_buf_index += sparse_header.chunk_hdr_sz - CHUNK_HEADER_LEN;
			}

			/* write current part of this chunk to flash */
			if ((chunk_header.chunk_type == CHUNK_TYPE_RAW)
				&& (i == uncomplete_ck.idx)) {
				chunk_len = length - g_buf_index;
			} else
				chunk_len = (uint64_t)chunk_header.chunk_sz * sparse_header.blk_sz;
		} else {
			memcpy(&chunk_header, &uncomplete_ck.header, sizeof(chunk_header_t));

			chunk_len = (uint64_t)chunk_header.total_sz - sizeof(chunk_header_t)
							- uncomplete_ck.saved_len;
			if (chunk_len > (length - g_buf_index))
				chunk_len = length - g_buf_index;
		}

		switch (chunk_header.chunk_type) {
		case CHUNK_TYPE_RAW:
			if ((i != uncomplete_ck.idx)
				&& (chunk_header.total_sz != (sparse_header.chunk_hdr_sz + chunk_len))) {
				errorf("Bogus chunk size for chunk %d, type Raw\n", i);
				goto fail;
			}
			succ = 0;
#ifdef CONFIG_WR_SPARSE
			if (NULL != g_env_bootmode && !strcmp(g_env_bootmode, "fastboot")) {
				wr_raw_dbg("RAW(CAC1) trunk_len=0x%x, sparse_offset=0x%llx!\n", chunk_len, sparse_offset);
				ret = wr_sparse_raw((char *)partname, chunk_len, sparse_offset, buf + g_buf_index);
				if (ret < -1) {
					errorf("Raw fail, chunk_len%llx! sparse_offset%llx\n",chunk_len,
						sparse_offset);
					goto fail;
				} else if (ret == 0)
					succ = 1;/* wr success */
				else {/* not enable, try raw write */
					 if (0 != common_raw_write(partname, chunk_len, (uint64_t)0, sparse_offset, buf + g_buf_index)) {
						 errorf("Raw chunk fail, trunk_len=0x%x!\n", chunk_len);
						 goto fail;
					 } else
						 succ = 1;
				}
			}else {/* download mode */
					 if (0 != common_raw_write(partname, chunk_len, (uint64_t)0, sparse_offset, buf + g_buf_index)) {
						 errorf("Raw chunk fail, trunk_len=0x%x!\n", chunk_len);
						 goto fail;
					 } else
						 succ = 1;
			}
#else
			if (0 != common_raw_write(partname, chunk_len, (uint64_t)0, sparse_offset, (char*)(buf + g_buf_index))) {
				errorf("Write raw chunk fail, trunk_len=0x%llx!\n", chunk_len);
				goto fail;
			} else {
				/* download complete, nothing to do here. */
				succ = 1;
			}
#endif
			if ((NULL != g_env_bootmode && !strcmp(g_env_bootmode, "download")) && v_ab_flag) {
				debugf("download slot: %s\n", temp);
				if (0 != common_raw_write(temp, chunk_len, (uint64_t)0, sparse_offset, (char*)(buf + g_buf_index))) {
					errorf("Write raw chunk fail, trunk_len=0x%llx!\n", chunk_len);
					goto fail;
				}
			}

			//crc32 = sparse_crc32(crc32, buf + g_buf_index, chunk_len);
			if (succ) {
				g_buf_index += chunk_len;
				sparse_offset += chunk_len;
				if (i == uncomplete_ck.idx) {
					uncomplete_ck.saved_len += chunk_len;
					if (uncomplete_ck.saved_len == (uint64_t)chunk_header.total_sz - sizeof(chunk_header_t)) {
						total_blocks += chunk_header.chunk_sz;
						memset(&uncomplete_ck, 0, sizeof(uncomplete_ck));
						uncomplete_ck.idx = -1;
					} else
						goto out;
				} else
					total_blocks += chunk_header.chunk_sz;
			}

			break;
		case CHUNK_TYPE_FILL:
			if (chunk_header.total_sz != (sparse_header.chunk_hdr_sz + sizeof(fill_val)) ) {
				errorf("Bogus chunk size for chunk %d, type Fill\n", i);
				goto fail;
			}
			/* Fill copy_buf with the fill value */
			ret = read_all(buf, g_buf_index, &fill_val, sizeof(fill_val));
#ifdef CONFIG_WR_SPARSE
			if (NULL != g_env_bootmode && !strcmp(g_env_bootmode, "fastboot")) {
				wr_fill_dbg("FILL(CAC2) trunk_len=0x%llx, sparse_offset=0x%llx!\n", chunk_len, sparse_offset);

				ret = wr_sparse_fill(partname, chunk_header.chunk_type,
							chunk_len, sparse_offset, fill_val);
				if (ret < -1) { /* fail */
					errorf("Fill fail, chunk_len %llx, sparse_offset %llx, fill_val %x\n",
						chunk_len, sparse_offset, fill_val);
					goto fail;
				} else if (ret == 0) {/* succ */
					sparse_offset += chunk_len;
					chunk_len = 0;
				}/* else not enable */
			}
#endif
			if (chunk_len) {
				fillbuf = (u32 *)copybuf;
				for (j = 0; j < (COPY_BUF_SIZE / sizeof(fill_val)); j++)
					fillbuf[j] = fill_val;
			}

			while (chunk_len) {
				chunk = (chunk_len > COPY_BUF_SIZE) ? COPY_BUF_SIZE : chunk_len;
				if (0 != common_raw_write(partname, (uint64_t)chunk, (uint64_t)0, sparse_offset, (char*)copybuf)) {
					errorf("Write fill chunk fail, trunk_len=0x%llx!\n", chunk_len);
					goto fail;
				}
				else {
					/* download complete, nothing to do here. */
					succ = 1;
				}

				/* write partition b only if in download mode */
				if ((NULL != g_env_bootmode && !strcmp(g_env_bootmode, "download")) && v_ab_flag) {
					debugf("download slot: %s\n", temp);
					if (0 != common_raw_write(temp, (uint64_t)chunk, (uint64_t)0, sparse_offset, (char*)copybuf)) {
						errorf("Write raw chunk fail, trunk_len=0x%llx!\n", chunk_len);
						goto fail;
					}
				}

				//crc32 = sparse_crc32(crc32, copybuf, chunk);
				sparse_offset += chunk;
				chunk_len -= chunk;
			}
			total_blocks += chunk_header.chunk_sz;
			break;
		case CHUNK_TYPE_DONT_CARE:
#ifdef CONFIG_WR_SPARSE
			wr_dontcare_dbg("DONTCARE(CAC3) trunk_len=0x%llx, sparse_offset=0x%llx!\n", chunk_len, sparse_offset);
			if (NULL != g_env_bootmode && !strcmp(g_env_bootmode, "fastboot")) {
				if (!sparse_offset && !current_chunks) { /* first chunk */
					ret = wr_sparse_dontcare((char *)partname, chunk_len, sparse_offset);
					if (ret < -1) { /* fail */
						errorf("First packet dont care fail, chunk_len %llx, sparse_offset %llx\n",
							chunk_len, sparse_offset);
						goto fail;
					}
				} else if (!(i == (sparse_header.total_chunks - 1))) { /* except last chunk */
					fill_val = 0;
					ret = wr_sparse_fill(partname, chunk_header.chunk_type,
								chunk_len, sparse_offset, fill_val);
					if (ret < -1) { /* fail */
						errorf("Dont care fail, chunk_len %llx, sparse_offset %llx, fill_val %x\n",
							chunk_len, sparse_offset, fill_val);
						goto fail;
					}
				}
			}
#endif
			if (chunk_header.total_sz != sparse_header.chunk_hdr_sz) {
				errorf("Bogus chunk size for chunk %d, type Dont Care\n", i);
				goto fail;
			}
			sparse_offset += chunk_len;
			total_blocks += chunk_header.chunk_sz;
			break;
		case CHUNK_TYPE_CRC32:
			if (process_crc32_chunk((void*)buf, crc32) != 0) {
				goto fail;
			}
			break;
		default:
			errorf("Unknown chunk type 0x%4.4x\n", chunk_header.chunk_type);
#ifdef CONFIG_WR_SPARSE
			dbg_his_print();
#endif
			goto fail;
		}

	}

out:
	if (sparse_header.total_blks != total_blocks) {
		return g_buf_index;
	} else {
		debugf("image download end\n");
		download_status = END;
		current_chunks = 0;
		sparse_offset = 0;
		total_blocks = 0;
		return 0;
	}

fail:
	download_status = END;
	current_chunks = 0;
	sparse_offset = 0;
	total_blocks = 0;
	return -1;
}

#ifdef SPRD_SPARSE_SUPER_SPEEDUP
int sparse_download_process(uint32_t size, char *buf)
{
	uint32_t i, j;
	chunk_header_t chunk_header;
	u32 crc32 = 0;
	int ret;
	uint64_t offset = 0, sparse_start = 0;
	unsigned long length = 0;
	uint64_t chunk_len = 0, chunk, chunt_temp;
	u32 fill_val;
	u32 *fillbuf;
	u64 sparse_sz, part_sz;
	unsigned char *partname = "userdata";
	g_buf_index = 0;	//offset for whole buffer
	char *ifname;
	block_dev_desc_t *dev_desc;
	int dev_id = 0;
	ifname = block_dev_get_name();
	dev_id = get_devnum_hwpart(ifname, 0);
	dev_desc = get_dev_hwpart(ifname, dev_id, USER_PART);
	if (NULL == dev_desc) {
		errorf("invalid dev_desc!\n");
		return -ENODEV;
	}
	prt("ready to handle sparse_cur_buf->addr:0x%x, sparse_cur_buf->used:0x%x, sparse2emmc_temp=0x%llx\n", sparse_cur_buf->addr, sparse_cur_buf->used, sparse2emmc_temp);

	if(sparse2emmc_temp != 0) {
		prt("need copy temp_length 0x%llx to sparse2emmc_buf start at:0x%x,sparse2emmc_buf->fixed will be 0x%llx\n", sparse2emmc_temp, (sparse2emmc_buf->addr+sparse2emmc_buf->fixed), (sparse2emmc_buf->fixed+sparse2emmc_temp));
		memcpy(sparse2emmc_buf->addr+sparse2emmc_buf->fixed, sparse_temp_buf->addr, sparse2emmc_temp);
		sparse2emmc_buf->fixed += sparse2emmc_temp;
		offset += sparse2emmc_temp;
	}

	if (download_status == END) {
		dprintf(ALWAYS, "enter speedup sparse download!\n");
		memset(&uncomplete_ck, 0, sizeof(uncomplete_ck));
		uncomplete_ck.idx = -1;
		current_chunks = 0;
		total_blocks = 0;
		offset = 0;
		memset(&sparse_header, 0, sizeof(sparse_header_t));
		ret = read_all(sparse_cur_buf->addr, g_buf_index, &sparse_header, sizeof(sparse_header));
		prt("sparse_cur_buf->addr:0x%x\n",sparse_cur_buf->addr);
		debugf("sparse_header file_hdr_sz = %d chunk_hdr_sz = %d blk_sz = %d total_blks = %d total_chunks = %d\n",
			sparse_header.file_hdr_sz, sparse_header.chunk_hdr_sz, sparse_header.blk_sz,
			sparse_header.total_blks, sparse_header.total_chunks);
		if (ret != sizeof(sparse_header)) {
			errorf("Error reading sparse file header\n");
			goto fail;
		}
		if (sparse_header.magic != SPARSE_HEADER_MAGIC) {
			errorf("Bad magic\n");
			goto fail;
		}
		if (sparse_header.major_version != SPARSE_HEADER_MAJOR_VER) {
			errorf("Unknown major version number\n");
			goto fail;
		}

		if (sparse_header.file_hdr_sz > SPARSE_HEADER_LEN) {
		/* Skip the remaining bytes in a header that is longer than
		* we expected.
		*/
			g_buf_index += (sparse_header.file_hdr_sz - SPARSE_HEADER_LEN);
		}
		download_status = START;
		//sparse_num == 0;
	}

	length = sparse_cur_buf->used;
	prt("handle sparse and copy to emmc:sparse2emmc_buf->addr:0x%x,buf start at:0x%x\n", sparse2emmc_buf->addr, (sparse2emmc_buf->addr+offset));
	for (i = current_chunks; i < sparse_header.total_chunks; i++) {
		prt("current_chunk=%d\n",i);
		memset(&chunk_header, 0, sizeof(chunk_header_t));
		if ((g_buf_index + sizeof(chunk_header)) > length) {
			prt("bufferindex(%d) + sizeof(chunk_header)(%d) exceed length\n", g_buf_index, sizeof(chunk_header));
			current_chunks = i;
			break;
		}
		if (!i || (i != uncomplete_ck.idx)) {
			prt("ready to read chunk_header,g_buf_index=%d\n", g_buf_index);
			preread_chunk((void*)sparse_cur_buf->addr, g_buf_index, &chunk_header, sizeof(chunk_header));
			prt("chunk_header.total_sz(%d)\n", chunk_header.total_sz);

			/*
			* fixup for "fastboot flash -S 200M system system.img"
			*   1. first block(200M): the first chunk type must not be 0xCAC3
			*   2. other blocks: the first chuck type is 0xCAC3 and chunk_len was offset continue
			*   from last block
			*/
			if (!offset && !current_chunks
				&& (chunk_header.chunk_type != CHUNK_TYPE_DONT_CARE)
				&& strcmp(partname, "userdata")) {
				/* erase partition before spare image write */
				errorf("=============================================");
				sparse_sz = (u64)sparse_header.total_blks * (u64)sparse_header.blk_sz;
				if (sparse_sz) {
					if ((get_img_partition_size(partname, &part_sz) >= 0)
							&& (part_sz > 0)) {
						if (sparse_sz > part_sz) {
							errorf("fail: sparse sz(0x%llx) large than partition sz(0x%llx)\n", sparse_sz, part_sz);
							goto fail;
						} else {
							debugf("??? sparse img write, erase partition %s sz 0x%llx\n", partname, sparse_sz);
							if (common_raw_erase(partname, sparse_sz > part_sz ? part_sz : sparse_sz, offset)) {
								errorf("sparse image write, erase partition %s fail\n", partname);
								goto fail;
							}
						}
					}
				}
			}

			ret = 0;
			switch (chunk_header.chunk_type) {
			case CHUNK_TYPE_RAW:
			case CHUNK_TYPE_FILL:
			case CHUNK_TYPE_DONT_CARE:
			case CHUNK_TYPE_CRC32:
				if ((g_buf_index + chunk_header.total_sz) > length) {
					prt("bufferindex(%d)+chunk_header.total_sz(%d) exceed  length(%d),chunk=%d \n", g_buf_index, chunk_header.total_sz, length, i);
					ret = 1;
				}
				break;
			default:
				errorf("Unknown chunk type 0x%4.4x\n", chunk_header.chunk_type);
			}

			if (ret == 1) {
				/* save uncomplete chunk header only for type RAW */
				current_chunks = i;
				if (chunk_header.chunk_type == CHUNK_TYPE_RAW) {
					memcpy(&uncomplete_ck.header, &chunk_header, sizeof(chunk_header_t));
					uncomplete_ck.idx = i;
					uncomplete_ck.saved_len = 0;
				} else
					break;
			}

			memset(&chunk_header, 0, sizeof(chunk_header_t));
			ret = read_all((void*)sparse_cur_buf->addr, g_buf_index, &chunk_header, sizeof(chunk_header));

			if (ret != sizeof(chunk_header)) {
				errorf("Error reading chunk header\n");
			}

			if (sparse_header.chunk_hdr_sz > CHUNK_HEADER_LEN) {
			/* Skip the remaining bytes in a header that is longer than
				* we expected.*/
				g_buf_index += sparse_header.chunk_hdr_sz - CHUNK_HEADER_LEN;
			}

			/* write current part of this chunk to flash */
			if ((chunk_header.chunk_type == CHUNK_TYPE_RAW)
				&& i && (i == uncomplete_ck.idx)) {
				chunk_len = length - g_buf_index;
			} else
				chunk_len = (uint64_t)chunk_header.chunk_sz * sparse_header.blk_sz;
		} else {
			memcpy(&chunk_header, &uncomplete_ck.header, sizeof(chunk_header_t));

			chunk_len = (uint64_t)chunk_header.total_sz - sizeof(chunk_header_t)
						- uncomplete_ck.saved_len;
			if (chunk_len > (length - g_buf_index))
				chunk_len = length - g_buf_index;
		}
		prt("chunk_type:0x%x,chunk_len: 0x%llx\n", chunk_header.chunk_type, chunk_len);
		switch (chunk_header.chunk_type) {
		case CHUNK_TYPE_RAW:
			if ((i != uncomplete_ck.idx)
				&& (chunk_header.total_sz != (sparse_header.chunk_hdr_sz + chunk_len))) {
				errorf("Bogus chunk size for chunk %d, type Raw\n", i);
			}
			if ((sparse2emmc_buf->fixed + chunk_len) > sparse2emmc_buf->size) {
				errorf("type_raw not fixed yet!!!!!!!!!!!!sparse2emmc_buf->fixed:0x%x, chunk_len:0x%llx\n", sparse2emmc_buf->fixed, chunk_len);
				fill_chunk_temp = sparse2emmc_buf->size - sparse2emmc_buf->fixed;
				memcpy(sparse2emmc_buf->addr + offset, sparse_cur_buf->addr + g_buf_index, fill_chunk_temp);
				g_buf_index += fill_chunk_temp;
				chunk_len -= fill_chunk_temp;
				sparse2emmc_buf->fixed += fill_chunk_temp;
				prt("raw:sparse2emmc_buf->fixed=0x%x, chunk_len=0x%llx, chunk=0x%llx,type_fill not fixed yet,yield to emmc!\n", sparse2emmc_buf->fixed, chunk_len, chunk);
				write_sparse2emmc_img(dev_desc);
				fill_chunk_temp = 0;
				sparse2emmc_buf = sparse2emmc_buf->next;
				offset = 0;
				prt("raw:data will copy start at:0x%x\n", (sparse2emmc_buf->addr + offset));
				memcpy(sparse2emmc_buf->addr + offset, sparse_cur_buf->addr + g_buf_index, chunk_len);
				//crc32 = sparse_crc32(crc32, sparse_cur_buf->addr + g_buf_index, chunk_len);
				g_buf_index += chunk_len;
				offset += chunk_len;
				sparse2emmc_buf->fixed += chunk_len;
				prt("raw:sparse2emmc_buf->fixed=0x%x,chunk_len=0x%llx\n", sparse2emmc_buf->fixed, chunk_len);
			} else {
				prt("raw:data will copy start at:0x%x\n",(sparse2emmc_buf->addr + offset));
				memcpy(sparse2emmc_buf->addr + offset, sparse_cur_buf->addr + g_buf_index, chunk_len);
				//crc32 = sparse_crc32(crc32, sparse_cur_buf->addr + g_buf_index, chunk_len);
				g_buf_index += chunk_len;
				offset += chunk_len;
				sparse2emmc_buf->fixed += chunk_len;
				prt("raw:sparse2emmc_buf->fixed=0x%x,chunk_len=0x%llx\n", sparse2emmc_buf->fixed, chunk_len);
			}
			if (i == uncomplete_ck.idx) {
				uncomplete_ck.saved_len += chunk_len;
				if (uncomplete_ck.saved_len == (uint64_t)chunk_header.total_sz - sizeof(chunk_header_t)) {
					total_blocks += chunk_header.chunk_sz;
					memset(&uncomplete_ck, 0, sizeof(uncomplete_ck));
					uncomplete_ck.idx = -1;
				} else {
					prt("raw:this chunk not fixed yet,current_chunk=%d!\n",i);
					i = i-1;
					goto out;
				}

			} else
				total_blocks += chunk_header.chunk_sz;

			break;
		case CHUNK_TYPE_FILL:
			if (chunk_header.total_sz != (sparse_header.chunk_hdr_sz + sizeof(fill_val)) ) {
				errorf("Bogus chunk size for chunk %d, type Fill\n", i);
			}
			/* Fill copy_buf with the fill value */
			ret = read_all(sparse_cur_buf->addr, g_buf_index, &fill_val, sizeof(fill_val));
			prt("fill_val=%x,sizeof(fill_val)=%u\n",fill_val,sizeof(fill_val));
			fillbuf = (u32 *)copybuf;
			for (j = 0; j < (COPY_BUF_SIZE / sizeof(fill_val)); j++)
				fillbuf[j] = fill_val;
			while (chunk_len)
			{
				chunk = (chunk_len > COPY_BUF_SIZE) ? COPY_BUF_SIZE : chunk_len;
				if ((sparse2emmc_buf->fixed + chunk) <= sparse2emmc_buf->size) {
					prt("fill:data will copy start at:0x%x\n", (sparse2emmc_buf->addr + offset));
					memcpy(sparse2emmc_buf->addr + offset, copybuf, chunk);
					offset += chunk;
					chunk_len -= chunk;
					sparse2emmc_buf->fixed += chunk;
					prt("fill:sparse2emmc_buf->fixed=0x%x,chunk_len=0x%llx,chunk=0x%llx\n", sparse2emmc_buf->fixed, chunk_len, chunk);
				} else {
					fill_chunk_temp = sparse2emmc_buf->size - sparse2emmc_buf->fixed;
					memcpy(sparse2emmc_buf->addr + offset, copybuf, fill_chunk_temp);
					chunk_len -= fill_chunk_temp;
					sparse2emmc_buf->fixed += fill_chunk_temp;
					prt("fill:sparse2emmc_buf->fixed=0x%x,chunk_len=0x%llx,chunk_temp=0x%llx,type_fill not fixed yet,yield to emmc!\n", sparse2emmc_buf->fixed, chunk_len, chunk);
					write_sparse2emmc_img(dev_desc);
					fill_chunk_temp = 0;
					sparse2emmc_buf = sparse2emmc_buf->next;
					offset = 0;
				}
			}
			total_blocks += chunk_header.chunk_sz;
			break;
		case CHUNK_TYPE_DONT_CARE:
			if (chunk_header.total_sz != sparse_header.chunk_hdr_sz) {
				errorf("Bogus chunk size for chunk %d, type Dont Care\n", i);
			}

			while (chunk_len > 0) {
				if ((sparse2emmc_buf->fixed + chunk_len) <= sparse2emmc_buf->size) {
						prt("dont care:data will copy start at:0x%x\n", (sparse2emmc_buf->addr + offset));
						memset(sparse2emmc_buf->addr + offset, 0, chunk_len);
						offset += chunk_len;
						sparse2emmc_buf->fixed += chunk_len;
						chunk_len -= chunk_len;
						have_dont_care = 0;
						prt("dont care can write once:sparse2emmc_buf->fixed=0x%x,remain chunk_len=0x%llx\n", sparse2emmc_buf->fixed, chunk_len);
				} else {
					dont_care_temp = sparse2emmc_buf->size - sparse2emmc_buf->fixed;
					chunk_len -= dont_care_temp;
					prt("dont care:sparse2emmc_buf->fixed=0x%x,remain chunk_len=0x%llx,chunk_temp=0x%llx,type_dont_care not fixed yet,yield to emmc!\n", sparse2emmc_buf->fixed, chunk_len, fill_chunk_temp);
					have_dont_care = 1;
					write_sparse2emmc_img(dev_desc);
					sparse2emmc_buf = sparse2emmc_buf->next;
					offset = 0;
				}
			}

			total_blocks += chunk_header.chunk_sz;
			break;
		case CHUNK_TYPE_CRC32:
			if (process_crc32_chunk((void*)sparse_cur_buf->addr, crc32) != 0) {
				errorf("CRC32 error\n");
			}
			break;
		default:
			errorf("Unknown chunk type 0x%4.4x\n", chunk_header.chunk_type);
		}
	}//for loop
out:
		sparse_cur_buf->used = 0;

		if (sparse_header.total_blks == total_blocks) {
			prt("total_blocks=%lld,current_chunk=%d,sparse enter wait\n",total_blocks, i);
			download_status = END;
			current_chunks = 0;
			offset = 0;
			sparse2emmc_temp = 0;
			prt("sparse2emmc_buf->fixed:0x%x\n", sparse2emmc_buf->fixed);
			prt("all sparse handle, exit\n");
			ret = write_sparse2emmc_img(dev_desc);
		} else {
			if(sparse2emmc_buf->fixed % dev_desc->blksz != 0) {
				sparse2emmc_temp = sparse2emmc_buf->fixed;
				sparse2emmc_buf->fixed = ((uint32_t)(sparse2emmc_buf->fixed/dev_desc->blksz)) * dev_desc->blksz;
				sparse2emmc_temp = sparse2emmc_temp - sparse2emmc_buf->fixed;
				prt("sparse_temp_buf->addr:0x%x\n", sparse_temp_buf->addr);
				memcpy(sparse_temp_buf->addr,sparse2emmc_buf->addr+offset-sparse2emmc_temp,sparse2emmc_temp);
				prt("this sparse2emmc buf can not write align %ld!!copy from:0x%x,copy to:0x%x,last:0x%llx\n", dev_desc->blksz, (sparse2emmc_buf->addr+offset-sparse2emmc_temp), sparse_temp_buf->addr, sparse2emmc_temp);
			} else {
				prt("this sparse2emmc buf can wirte align!\n");
				sparse2emmc_temp = 0;
			}
			ret = write_sparse2emmc_img(dev_desc);
			sparse_cur_buf = sparse_cur_buf->next;
			sparse2emmc_buf = sparse2emmc_buf->next;
		}
		return 1;

fail:
        return -1;
}
#endif
