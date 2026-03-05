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

#include <fb_sparse.h>
#include <sprd_sizes.h>
#include <lk/list.h>
#include <asm/byteorder.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <dl_operate.h>
#include <malloc.h>
#include <sprd_common_rw.h>
#include <lk/debug.h>
#include <sparse_format.h>
#include <string.h>
#include <sprd_list.h>

static wr_sparse_mgt_t *g_wr_sparse;
static LIST_HEAD(unp_chunk_list);		/* unprocessed chunk list */
static LIST_HEAD(unp_free_list);		/* unprocessed free list */

#ifdef CONFIG_WR_SPARSE

ALTER_BUFFER_ATTR fb_alter_buffer1;
ALTER_BUFFER_ATTR fb_alter_buffer2;
ALTER_BUFFER_ATTR* fb_current_buffer;

#ifdef CONFIG_WRBG_SPARSE
static int wrbg_sparse_raw(wr_sparse_mgt_t *mgt, char *pname,
	u64 size, u64 offset, char *buf)
{
	u32 blk_count = 0;
	u32 tot_buf_size = fb_current_buffer->size;
	u32 last_size = 0;

	if (fb_current_buffer->spare > size) {
		memcpy(fb_current_buffer->pointer, buf, size);
		fb_current_buffer->used += size;
		fb_current_buffer->spare -= size;
		fb_current_buffer->pointer += size;
	} else {
		if (fb_current_buffer->spare)
			memcpy(fb_current_buffer->pointer, buf, fb_current_buffer->spare);

		last_size = size - fb_current_buffer->spare;
		if (BUFFER_DIRTY == fb_current_buffer->next->status) {
			if (0 != common_query_backstage(pname,
						tot_buf_size, fb_current_buffer->next->addr)) {
				errorf("%s query bg fail, next->addr=%p\n",
					pname, fb_current_buffer->next->addr);
				return -2;
			}
		}

		wr_raw_dbg("%s wr bg write, fb_current_buffer->addr:%p wr_offset:0x%llx, tot_buf_size:%llx\n",
			pname, fb_current_buffer->addr, mgt->wr_offset, tot_buf_size);
		if (0 != common_write_backstage(pname,
			tot_buf_size, mgt->wr_offset, fb_current_buffer->addr)) {
			errorf("%s write bg fail, wr_offset %llx addr %p\n", pname, mgt->wr_offset,
				fb_current_buffer->addr);
			return -3;
		}

		mgt->wr_offset += tot_buf_size;

		if (last_size > fb_current_buffer->size) {
			errorf("%s write bg fail, last_size %x fb_current_buffer->size %x\n",
				pname, last_size, fb_current_buffer->size);
			return -4;
		} else if (last_size) {
			memcpy(fb_current_buffer->next->addr, buf + fb_current_buffer->spare,
				last_size);
		}

		fb_current_buffer->used = 0;
		fb_current_buffer->spare = fb_current_buffer->size;
		fb_current_buffer->pointer = fb_current_buffer->addr;
		fb_current_buffer->status = BUFFER_DIRTY;

		fb_current_buffer = fb_current_buffer->next;
		fb_current_buffer->pointer = fb_current_buffer->addr + last_size;
		fb_current_buffer->used = last_size;
		fb_current_buffer->spare = fb_current_buffer->size - last_size;
	}

	return 0;
}

static void wrbg_sparse_rest_alt(void)
{
#ifdef SPRD_DTS_MEM_LAYOUT
	fb_alter_buffer1.addr = g_wr_sparse->wr_addr;
	fb_alter_buffer1.size = sizeof(g_wr_sparse->wr_addr) / 2;

	fb_alter_buffer2.addr = g_wr_sparse->wr_addr + fb_alter_buffer1.size;
	fb_alter_buffer2.size = fb_alter_buffer1.size;
#else
	fb_alter_buffer1.addr = DL_ALT1_BUF_ADDR;
	fb_alter_buffer1.size = DL_ALT1_BUF_SIZE;

	fb_alter_buffer2.addr = DL_ALT2_BUF_ADDR;
	fb_alter_buffer2.size = DL_ALT2_BUF_SIZE;
#endif

	fb_alter_buffer1.pointer = fb_alter_buffer1.addr;
	fb_alter_buffer1.used = 0;
	fb_alter_buffer1.spare = fb_alter_buffer1.size;
	fb_alter_buffer1.status = BUFFER_CLEAN;
	fb_alter_buffer1.next = &fb_alter_buffer2;

	fb_alter_buffer2.pointer = fb_alter_buffer2.addr;
	fb_alter_buffer2.used = 0;
	fb_alter_buffer2.spare = fb_alter_buffer2.size;
	fb_alter_buffer2.status = BUFFER_CLEAN;
	fb_alter_buffer2.next = &fb_alter_buffer1;
	fb_current_buffer = &fb_alter_buffer1;
}

int wrbg_sparse_flush(char *pname)
{
	wr_sparse_mgt_t *mgt = g_wr_sparse;

	if (!mgt)
		return -1;

	if (BUFFER_DIRTY == fb_current_buffer->next->status) {
		if (0 != common_query_backstage(pname,
					fb_current_buffer->size, fb_current_buffer->next->addr)) {
			errorf("%s flush query backstage fail, fb_current_buffer->size: %x,"
				"fb_current_buffer->next->addr %p\n",
				pname, fb_current_buffer->size, fb_current_buffer->next->addr);
			return -2;
		}
	}

	if (fb_current_buffer->used) {
		if (0 != common_raw_write(pname,
					(u64)(fb_current_buffer->used), (u64)0, mgt->wr_offset,
					fb_current_buffer->addr)) {
			errorf("%s last cross write fail, fb_current_buffer->used: %x"
				"wr_offset:%llx fb_current_buffer->addr:%p\n",
				pname, fb_current_buffer->used, mgt->wr_offset, fb_current_buffer->addr);
			return -3;
		}

		/* update wr_offset */
		mgt->wr_offset += fb_current_buffer->used;

	}
	/* reset alt buffer */
	wrbg_sparse_rest_alt();
	return 0;
}
#endif

int wr_sparse_raw(char *pname, u64 chunk_len, u64 offset, char *buf)
{
	wr_sparse_mgt_t *mgt = g_wr_sparse;
#ifdef CONFIG_WRBG_SPARSE
	const int wr_align_sz = SZ_2M;
	u64 align_offset = 0, sz = 0;
#else
	int spare;
#endif

	if (!mgt)
		return -1;

#ifdef CONFIG_WRBG_SPARSE
	if ((!fb_alter_buffer1.used && !fb_alter_buffer2.used) /* alt buffer is empty */
			&& (offset & (wr_align_sz - 1))) {
		/* flush backstage write */
		if (wrbg_sparse_flush(pname) < 0) {
			return -2;
		}

		align_offset = ALIGN(offset, wr_align_sz);
		sz = chunk_len > align_offset - offset ? align_offset - offset : chunk_len;

		wr_raw_dbg("wr align chunk_len %llx offset %llx wr_align_sz %x "
			"align_offset %llx sz %llx\n",
			chunk_len, offset, wr_align_sz, align_offset, sz);
		if (0 != common_raw_write(pname, sz, (u64)0, offset, buf)) {
			errorf("wr write fail, chunk_len:%llx sz:%llx offset:0x%llx, buf:%p\n",
				chunk_len, sz, offset, buf);
			return -3;
		}

		/* update wr_offset */
		mgt->wr_offset += sz;

		chunk_len -= sz;
		offset += sz;
	}

	if (chunk_len) {
		if (wrbg_sparse_raw(mgt, pname, chunk_len, offset, buf + sz) < 0) {
			errorf("%s chunk_len %llx offset %llx wr_offset %llx buf %p\n",
				pname, chunk_len, offset, mgt->wr_offset, buf + sz);
			return -4;
		}
	}
#else
	spare = mgt->wr_lmax - mgt->wr_len;
	if (spare < 0) {
		errorf("wr_lmax %lx < wr_len %lx\n", mgt->wr_lmax, mgt->wr_len);
		return -2;
	}

	if (mgt->wr_len + chunk_len > mgt->wr_lmax) {
		memcpy(mgt->wr_addr + mgt->wr_len, buf, spare);
		mgt->wr_len += spare;
		chunk_len -= spare;
	} else {
		memcpy(mgt->wr_addr + mgt->wr_len, buf, chunk_len);
		mgt->wr_len += chunk_len;
		chunk_len = 0;
	}

	if (mgt->wr_len == mgt->wr_lmax) {
		wr_raw_dbg("wr write, wr_addr:%p wr_offset:0x%llx, wr_len:%lx offset:%llx chunk_len:%llx\n",
				mgt->wr_addr, mgt->wr_offset, mgt->wr_len, offset, chunk_len);
		if (0 != common_raw_write(pname, mgt->wr_len, (uint64_t)0,
				mgt->wr_offset, mgt->wr_addr)) {
			errorf("wr write fail, wr_addr:%p wr_offset:0x%llx, wr_len:%lx\n",
				mgt->wr_addr, mgt->wr_offset, mgt->wr_len);
			return -3;
		}
		mgt->wr_offset += mgt->wr_len;
		mgt->wr_len = 0;
	}

	if (chunk_len) {
		if (chunk_len > mgt->wr_lmax) {
			errorf("wr write fail, chunk_len %llx > wr_lmax:%lx\n",
				chunk_len, mgt->wr_lmax);
			return -4;
		}
		memcpy(mgt->wr_addr, buf + spare, chunk_len);
		mgt->wr_len += chunk_len;
	}
#endif
	return 0;
}

int wr_sparse_fill(char *pname, s16 chunk_type, u64 size, u64 offset,
	u32 fill_val)
{
	wr_sparse_mgt_t *mgt = g_wr_sparse;
	u32 spare;
	wr_unp_ck_t *uc = NULL;
	int i, j;
	u32 *fillbuf = NULL;
	u32 filllen;

	if (!mgt)
		return -1;

#ifdef CONFIG_WRBG_SPARSE
	/* fill one alter buffer only */
	if (fb_current_buffer->spare){
		spare = fb_current_buffer->spare;
		debugf("[%s]: spare = 0x%x \n", __func__, spare);
	}
	else {
		wr_fill_dbg("fb_current_buffer.spare %x,"
			"fb_alter_buffer1.size %x fb_alter_buffer1.used %x,"
			"fb_alter_buffer2.size %x fb_alter_buffer2.used %x\n",
			fb_current_buffer->spare,
			fb_alter_buffer1.size, fb_alter_buffer1.used,
			fb_alter_buffer2.size, fb_alter_buffer2.used);
		if (wrbg_sparse_flush(pname) < 0) {
			return -2;
		}
		spare = 0;
	}
#else
	if (mgt->wr_offset + mgt->wr_len != offset) {
		errorf("wr_offset + wr_len (%llx + %lx) != fill offset %llx\n",
			mgt->wr_offset, mgt->wr_len, offset);
		return -2;
	}

	spare = mgt->wr_lmax - mgt->wr_len;
	if (spare < 0) {
		errorf("wr_lmax %lx < wr_len %lx\n", mgt->wr_lmax,
			mgt->wr_len);
		return -3;
	}
#endif

	fillbuf = (u32 *)mgt->wr_tmp;
	filllen = 0;
	if (spare >= size) {
		for (i = 0; i < size / sizeof(fill_val); i++) {
			fillbuf[i] = fill_val;
		}
		filllen = size;
		size = 0;
	} else if (spare) {
		for (i = 0; i < spare / sizeof(fill_val); i++) {
			fillbuf[i] = fill_val;
		}
		filllen = spare;
		size -= spare;
	}

	if (filllen) {
		//flush wr buffer
		if (wr_sparse_raw(pname, filllen, offset, fillbuf)) {
			errorf("fill write fill buf%p fail, offset %llx filllen %x\n",
				fillbuf, offset, filllen);
			return -4;
		}
	}

	if (size) {
#ifdef CONFIG_WRBG_SPARSE
		if (fb_alter_buffer1.used && fb_alter_buffer2.used) {
			errorf("err fb_alter_buffer1.used %x or fb_alter_buffer2.used %x not zero\n",
				fb_alter_buffer1.used, fb_alter_buffer2.used);
			return -5;
		}
#else
		if (mgt->wr_len != 0) {
			errorf("err wr_len%x not zero\n", mgt->wr_len);
			return -5;
		}
#endif

		if (mgt->free_cnt <= 0) {
			errorf("unprocess node was exhausted, free_cnt%d\n", mgt->free_cnt);
			return -6;
		}

		uc = list_first_entry(unp_free_list.next, wr_unp_ck_t, node);
		list_delete(&uc->node);
		mgt->free_cnt--;

		if ((chunk_type == (s16)CHUNK_TYPE_FILL) && (fill_val == 0)) {
			chunk_type = (s16)CHUNK_TYPE_DONT_CARE;
		}

		uc->type = chunk_type;
		uc->offset = offset;
		uc->chunk_len = size;
		uc->fill_val = fill_val;

		list_add_tail(&unp_chunk_list, &uc->node);

		//update wr_offset
		mgt->wr_offset += size;
	}
	return 0;
}

int wr_sparse_dontcare(char *pname, u64 size, u64 offset)
{
	wr_sparse_mgt_t *mgt = g_wr_sparse;

	if (!mgt)
		return -1;

#ifdef CONFIG_WRBG_SPARSE
	if (wrbg_sparse_flush(pname) < 0) {
		return -2;
	}
#else
	if (mgt->wr_len) {
		wr_raw_dbg("wr write, wr_addr:%p wr_offset:0x%llx, wr_len:%lx\n",
				mgt->wr_addr, mgt->wr_offset, mgt->wr_len);
		if (0 != common_raw_write(pname, mgt->wr_len, (uint64_t)0,
				mgt->wr_offset, mgt->wr_addr)) {
			errorf("wr flush before dont care fail"
				"wr_addr:%p wr_offset:0x%llx, wr_len:%lx\n",
				mgt->wr_addr, mgt->wr_offset, mgt->wr_len);
			return -2;
		}
		mgt->wr_offset += mgt->wr_len;
		mgt->wr_len = 0;
	}
#endif

	mgt->wr_offset = offset + size;
	wr_dontcare_dbg("%s dont care reset wr_offset %llx\n", pname, mgt->wr_offset);
	return 0;
}

int wr_sparse_flush(void)
{
	wr_unp_ck_t *pos, *n;
	int i;
	u32 *fillbuf = NULL;
	u32 fill_val, filllen;
	u64 offset;
	wr_sparse_mgt_t *mgt = g_wr_sparse;
	if (!mgt)
		 return -1;
	char *pname = mgt->pname;
	if (!strlen(pname))
		return -1;

#ifdef CONFIG_WRBG_SPARSE
	if (wrbg_sparse_flush(pname) < 0) {
		return -2;
	}
#else
	if (mgt->wr_len) {
		wr_raw_dbg("%s flush wr buffer, wr_addr %llx wr_offset %llx wr_len %x\n",
					pname, mgt->wr_addr, mgt->wr_offset, mgt->wr_len);
		if (common_raw_write(pname, mgt->wr_len, (uint64_t)0,
				mgt->wr_offset, mgt->wr_addr)) {
			errorf("%s wr write fail, wr_addr:%p offset:0x%llx, wr_len:%lx\n",
				pname, mgt->wr_addr, mgt->wr_offset, mgt->wr_len);
			return -2;
		}
		mgt->wr_offset += mgt->wr_len;
		mgt->wr_len = 0;
	}
#endif

	list_for_each_entry_safe(pos, n, &unp_chunk_list, node) {
		switch (pos->type) {
		case CHUNK_TYPE_FILL:
			wr_fill_dbg("%s flush fill, chunk_len %llx offset %llx\n",
				pname, pos->chunk_len, pos->offset);
			fillbuf = (u32 *)mgt->wr_tmp;
			for (i = 0; i < (mgt->wr_lmax / sizeof(pos->fill_val)); i++)
				fillbuf[i] = pos->fill_val;

			filllen = pos->chunk_len;
			offset = pos->offset;
			while (filllen) {
				i = (filllen > mgt->wr_lmax) ? mgt->wr_lmax : filllen;
				if (0 != common_raw_write(pname, (uint64_t)i, (uint64_t)0,
					offset, fillbuf)) {
					errorf("%s flush write fill fail, i:%x, offset:%llx,"
						"fillbuf:%p\n", pname, i, offset, fillbuf);
					return -3;
				}
				offset += i;
				filllen -= i;
			}
			break;
		case CHUNK_TYPE_DONT_CARE:
			wr_dontcare_dbg("%s flush dont care, chunk_len %llx offset %llx\n",
				pname, pos->chunk_len, pos->offset);
			if (0 != common_raw_erase(pname, pos->chunk_len, pos->offset)) {
				errorf("%s flush erase fail, chunk_len %llx, offset %llx\n",
					pname, pos->chunk_len, pos->offset);
				return -4;
			}
			break;
		default:
			break;
		}

		list_delete(&pos->node);
		list_add_tail(&unp_free_list, &pos->node);
		mgt->free_cnt++;
	}

	wr_dbg("%s flush free_cnt %d\n", pname, mgt->free_cnt);
	return 0;
}

void wr_sparse_rest(char *pname)
{
	wr_sparse_mgt_t *mgt = g_wr_sparse;
	int i;

	if (!mgt)
		return;

	INIT_LIST_HEAD(&unp_chunk_list);
	INIT_LIST_HEAD(&unp_free_list);

	memset(mgt, 0, sizeof(*mgt));
	mgt->wr_lmax = WR_LMAX_LIMIT;
	mgt->free_cnt = WR_UNP_CK_MAX;

	for (i = mgt->free_cnt - 1; i > 0; i--)
		list_add_tail(&unp_free_list, &mgt->cks[i].node);

	if (pname)
		strncpy(mgt->pname, pname, PARTNAME_SZ-1);

#ifdef CONFIG_WRBG_SPARSE
	wrbg_sparse_rest_alt();
#endif
	wr_dbg("rest pname %s\n", mgt->pname);
}

int wr_sparse_prepare(u8 *base_address, u64 *max_size)
{
	wr_sparse_mgt_t *mgt;
	u64 sz = *max_size;
	const u32 lmin = SZ_128M;


	if (sz < lmin + sizeof(*mgt)) {
		return -1;
	}

	mgt = (wr_sparse_mgt_t *)(base_address + lmin);
	g_wr_sparse = mgt;

	wr_sparse_rest(NULL);

	*max_size = lmin;
	return 0;
}

#endif

