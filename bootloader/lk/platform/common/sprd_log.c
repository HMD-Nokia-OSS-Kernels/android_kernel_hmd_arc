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

#include <boot_mode.h>
#include <malloc.h>
#include <string.h>
#include <chipram_env.h>
#include <sprd_common_rw.h>
#include <sprd_log.h>
#include <asm/arch/check_reboot.h>
#include <arch/arch_ops.h>
#include <fdtdec.h>
#include <config.h>
#include <arch/sprd_cache.h>
#include "fastboot.h"
#include <sprd_rtc_def.h>
#include <kernel/spinlock.h>

extern struct rtc_time get_time_by_sec(void);
extern spin_lock_t block_rw_lock;
extern spin_lock_t sprd_adi_lock;

uint32_t log_buffer_enabled_flag = 0x5a5a6543;
#ifdef CONFIG_DTS_MEM_LAYOUT
LOG_BUFFER *p_log_buffer = 0x82000000;
#else
LOG_BUFFER *p_log_buffer = (LOG_BUFFER *)LOG_RESERVED_ADDR;
#endif

//#define BOOTLOADER_LOG_DEBUG
#ifdef BOOTLOADER_LOG_DEBUG
#define log_debug(fmt, args...) do { dprintf(INFO,"BOOTLOADER_LOG: %s(): ", __func__);dprintf(INFO,fmt, ##args); } while (0)
#else
#define log_debug(fmt, args...)
#endif

LOG_REBOOT_TYPE_T lr_cause = LR_NORMAL;

unsigned long get_bootloader_log_addr(void)
{
	if (NULL != p_log_buffer)
		return LOG_BUFFER_START_ADDR;
	return 0;
}

uint32_t get_bootloader_log_len(void)
{
	if (NULL != p_log_buffer)
		return (p_log_buffer->end - (uint8_t*)LOG_BUFFER_START_ADDR);
	return 0;
}


#ifdef CONFIG_SPRD_LOG
extern int get_dl_cmd_log2pc_status(void);
static int lastn = 1;
static int flush_buf_flags = 0; // 1 : flushing buffer to storage. 0: no flush.

static int is_valid_addr(uint8_t* addr)
{
   return ((addr >= (uint8_t *)LOG_RESERVED_ADDR) && (addr < (uint8_t *)(LOG_RESERVED_ADDR+LOG_RESERVED_SIZE)));
}

static int check_valid_log_buf_addr(void)
{
	if(is_valid_addr(p_log_buffer->pointer)
		&& is_valid_addr(p_log_buffer->log_save_pointer)) {
		return 0;
	} else {
		return 1;
	}
}
__weak int log2pc(char *log, int len) { return -1;}
static int init_log_buffer(void)
{
#ifdef CONFIG_DTS_MEM_LAYOUT
	unsigned long buf_base = 0, buf_size = 0;

	log_buffer_enabled_flag = 0;

	if (get_buffer_base_size_from_dt("heap@8", &buf_base, &buf_size) < 0) {
		return -1;
	}

	p_log_buffer = (LOG_BUFFER *)buf_base;
	p_log_buffer->addr = (unsigned char *)ALIGN(buf_base + sizeof(LOG_BUFFER), 8);
	p_log_buffer->end = buf_base+buf_size;

	if (buf_size < LOG_BUFFER_SIZE + sizeof(LOG_BUFFER)) {
		errorf("log buf size in uboot dts is too small\n");
		return -1;
	}
#else
	p_log_buffer->addr = (uint8_t* )LOG_BUFFER_START_ADDR;
	p_log_buffer->end = (uint8_t* )(LOG_RESERVED_ADDR + LOG_RESERVED_SIZE - 1);
#endif
	/* Storge boot log in DDR by cycle */
	if ((p_log_buffer->magic == 0x31676f6c44525053) && (!check_valid_log_buf_addr())) {
		p_log_buffer->record_count ++;
		p_log_buffer->addr = p_log_buffer->log_save_pointer;
		p_log_buffer->size = 0;
		log_buffer_enabled_flag = 1;
		return 0;
	}

	p_log_buffer->pointer = p_log_buffer->addr;
	p_log_buffer->log_save_pointer = p_log_buffer->pointer;
	p_log_buffer->magic = 0x31676f6c44525053;
	p_log_buffer->record_count = 0;
	p_log_buffer->size = 0;

	log_buffer_enabled_flag = 1;

	return (0);
}

void bootloader_log_2_pc(void)
{
	log2pc(get_bootloader_log_addr(), get_bootloader_log_len());
}

void bootloader_log_insert_time(void)
{
	// log insert time log
	char tmp[12];
	if (lastn) {
		if (p_log_buffer->pointer + 11 >= p_log_buffer->end)
			p_log_buffer->pointer = (uint8_t*)LOG_BUFFER_START_ADDR;

		sprintf(tmp, "[%08d] ", SCI_GetTickCount() & 0x52FFFFFF);
		memcpy(p_log_buffer->pointer, (uint8_t *)tmp, 11);
		p_log_buffer->pointer += 11;
		p_log_buffer->size += 11;

		lastn = 0;
	}
}

void update_bootloader_log(char str)
{
#ifdef CONFIG_SPRD_LOG
	bootloader_log_insert_time();
	if (p_log_buffer->pointer +1 >= p_log_buffer->end)
		p_log_buffer->pointer = (uint8_t*)LOG_BUFFER_START_ADDR;

	if (str == '\n')
		lastn = 1;

	*(p_log_buffer->pointer) = str;
	p_log_buffer->pointer++;
	p_log_buffer->size ++;


	if (!get_dl_cmd_log2pc_status())
		bootloader_log_2_pc();

#endif
}

void check_bootloader_log_buffer(void)
{
#ifdef CONFIG_LOG_2_STORAGE
	if (block_rw_lock)
		return;

	//prevent circular buffers from overwriting log data. reserverd 64K size for cycles.
	if ((flush_buf_flags == 0) && (p_log_buffer->size > (p_log_buffer->end - (uint8_t*)LOG_BUFFER_START_ADDR -0x10000))) {
		flush_buf_flags = 1;
		flush_log_buffer();
		init_log_buffer();
		init_write_log();
		flush_buf_flags = 0;
	}
#endif
}

void sprd_log_cache_flush(void)
{
	flush_cache((unsigned long*)LOG_RESERVED_ADDR, (unsigned long*)LOG_RESERVED_SIZE);
}

#else
static int init_log_buffer(void) {return 0;}
void update_bootloader_log(char str) {}
void check_bootloader_log_buffer(void) {}
#endif

#if defined(CONFIG_LOG_2_STORAGE)
LOG_STRUCT log_st;
int reinit_flag = 0;

void init_log_struct(void)
{
	memset(&log_st, 0, sizeof(LOG_STRUCT));
}

int init_log_partition_hdr(void)
{
	uint8_t __aligned(ARCH_DMA_MINALIGN) hdr_buf[LOG_HEADER_SIZE];
	LOG_PARTITION_HEADER *p_hdr = (LOG_PARTITION_HEADER *)hdr_buf;
	const uint64_t log_p_offset[MAX_LOG_TYPE] = {
		PANIC_LOG_PARTITION_OFFSET,
		START_LOG_PARTITON_OFFSET,
		FASTBOOT_LOG_PARTITION_OFFSET,
		DOWNLD_LOG_PARTITION_OFFSET,
	};
	const int log_body_count[MAX_LOG_TYPE] = {
		LOG_PANIC_BODY_NUM,
		LOG_CBOOT_BODY_NUM,
		LOG_FASTBOOT_BODY_NUM,
		LOG_DOWNLD_BODY_NUM,
	};
	LOG_TYPE_T t;

	for (t = PANIC_LOG_TYPE; t < MAX_LOG_TYPE; t++) {
		debugf("init log type %d\n", t);

		memset(hdr_buf, 0, sizeof(hdr_buf));
		if (0 != common_raw_read(UBOOT_LOG_PARTITION, (uint64_t)LOG_HEADER_SIZE, log_p_offset[t], (char*)hdr_buf)) {
			errorf("read hdr error\n");
			log_st.flag = FAILED;
			return -1;
		}

		if ((p_hdr->magic != LOG_HEAD_MAGIC)) {
			debugf("reset log partition header\n");
			p_hdr->magic = LOG_HEAD_MAGIC;
			p_hdr->len = LOG_HEADER_SIZE;
			p_hdr->boot_count = 1;
			p_hdr->body_num = log_body_count[t];
			p_hdr->type = t;
			p_hdr->body.p_offset = LOG_HEADER_SIZE;
			p_hdr->body.p_end = (uint64_t)p_hdr->body_num*LOG_BUFFER_SIZE - 1;
			p_hdr->body.b_offset = LOG_HEADER_SIZE;
			p_hdr->body.size = 0;

			if (0 != common_raw_write(UBOOT_LOG_PARTITION, (uint64_t)LOG_HEADER_SIZE, (uint64_t)0,
									  (uint64_t)log_p_offset[t], (char*)hdr_buf)) {
				errorf("write hdr error\n");
				log_st.flag = FAILED;
				return -1;
			}
		} else if (reinit_flag == 0) {
			p_hdr->body.p_offset = p_hdr->body.b_offset;
			p_hdr->body.size = 0;
			p_hdr->boot_count++;
		}

		log_st.log_type_p_offset[t] = log_p_offset[t];
		memcpy(&log_st.log_par_hdr[t], p_hdr, sizeof(LOG_PARTITION_HEADER));
		if (t == START_LOG_TYPE)
			dprintf(ALWAYS, "start type init log success on %d times\n", p_hdr->boot_count);
		else if (t == PANIC_LOG_TYPE)
			dprintf(ALWAYS, "panic type boot count %d\n", p_hdr->boot_count);
		else if (t == FASTBOOT_LOG_TYPE)
			dprintf(ALWAYS, "fastboot type boot count %d\n", p_hdr->boot_count);
		else if (t == DOWNLD_LOG_TYPE)
			dprintf(ALWAYS, "download type boot count %d\n", p_hdr->boot_count);
	}

	log_st.flag = SUCESS; /* set sucess flag */
	return 0;
}

void flush_log_buffer(void)
{
	uint8_t hdr_buf[LOG_HEADER_SIZE];
	uint8_t *buf = NULL;
	uint64_t buf_size = 0;
	uint8_t *buf2 = NULL;
	uint64_t buf2_size = 0;
	LOG_BUFFER *p_log;
	LOG_PARTITION_HEADER *p_hdr;
	LOG_BODY *body;
	boot_mode_t mode;
	const char *boot_mode;

	show_current_time(); // save uboot_log partition show rtc timer.
	if (log_st.flag != SUCESS) {
		return;
	}

	/* choose the right log partition */
	if (lr_cause == LR_ABNORMAL || lr_cause == LR_LONG_PRESS
			|| lr_cause == LR_UNKNOWN)
		p_hdr = &log_st.log_par_hdr[PANIC_LOG_TYPE];
	else {
#ifdef CONFIG_ZEBU
		mode = BOOTLOADER_MODE_LOAD;
#else
		mode = get_boot_role();
#endif
		boot_mode = g_env_bootmode;
		if (mode == BOOTLOADER_MODE_DOWNLOAD
			|| (boot_mode && (!strcmp(boot_mode, "download") || !strcmp(boot_mode, "autodloader")))
			)
			p_hdr = &log_st.log_par_hdr[DOWNLD_LOG_TYPE];
		else {
			if (boot_mode && !strcmp(boot_mode, "fastboot"))
				p_hdr = &log_st.log_par_hdr[FASTBOOT_LOG_TYPE];
			else
				p_hdr = &log_st.log_par_hdr[START_LOG_TYPE];
		}
	}

	if (p_hdr->type >= MAX_LOG_TYPE) {
		log_debug("error log type %d\n", p_hdr->type);
		return;
	}

	body = &p_hdr->body;
	p_log = p_log_buffer;
	log_debug("p_log_buffer address p_log->addr = %p, p_log->pointer= %p , p_log->log_save_pointer =%p, body->size =%d. \n",
		p_log->addr, p_log->pointer, p_log->log_save_pointer, body->size);

	p_log->log_save_pointer = p_log->pointer;
	buf = (p_log->addr + body->size < p_log->end) ? (p_log->addr + body->size)
		:((p_log->addr + body->size) - (p_log->end) + (uint8_t*)LOG_BUFFER_START_ADDR);

	if (p_log->pointer > buf) {
		buf_size = p_log->pointer - buf;
	} else if (p_log->pointer < buf) {
		buf_size = p_log->end - buf;
		buf2 = (uint8_t *)LOG_BUFFER_START_ADDR;
		buf2_size = (p_log->pointer - (uint8_t*)LOG_BUFFER_START_ADDR);
	}

	log_debug("body->p_offset = %llu, body->b_offset = %llu, body->size = %d\n", body->p_offset, body->b_offset , body->size);

	body->b_offset = (body->b_offset +buf_size + buf2_size >= body->p_end)
		? LOG_HEADER_SIZE : body->b_offset;

	log_debug("body->p_offset = %llu, body->b_offset = %llu, body->p_end = %llu\n", body->p_offset ,body->b_offset, body->p_end);
	log_debug("log_st.log_type_p_offset[p_hdr->type] = %llu, p_hdr->type = %d \n", log_st.log_type_p_offset[p_hdr->type], p_hdr->type);
	log_debug("update body->b_offset = %llu, buf_size = %llu, buf2_size =  %llu \n", body->b_offset + buf_size + buf2_size, buf_size, buf2_size);

	if (((buf_size) > (body->p_end - body->b_offset))
		|| ((buf2_size) > (body->p_end - body->b_offset))
		|| ((buf_size + buf2_size) > (body->p_end - body->b_offset))) {
			log_debug("p_log->addr = %p, p_log->pointer= %p, p_log->log_save_pointer =%p, body->size = %d \n",
				p_log->addr, p_log->pointer, p_log->log_save_pointer, body->size);
			log_debug("Log Type: %d, write log buf buf_size = %llu, buf2_size = %llu, overlay current body size\n",
				p_hdr->type, buf_size, buf2_size);
		return;
	}

	/* write log */
	if (buf2 == NULL) {
		if (0 != common_raw_write(UBOOT_LOG_PARTITION, (uint64_t)buf_size, (uint64_t)0,
								  log_st.log_type_p_offset[p_hdr->type] + body->b_offset, (char*)buf)) {
			log_debug("write log 2 storage failed\n");
			return;
		}
	} else {
		if (0 != common_raw_write(UBOOT_LOG_PARTITION, (uint64_t)buf_size, (uint64_t)0,
								  log_st.log_type_p_offset[p_hdr->type] + body->b_offset, (char*)buf)) {
			log_debug("write log1 2 storage failed\n");
			return;
		}

		if (0 != common_raw_write(UBOOT_LOG_PARTITION, (uint64_t)buf2_size, (uint64_t)0,
								  log_st.log_type_p_offset[p_hdr->type] + body->b_offset + buf_size ,
								  (char*)buf2)) {
			log_debug("write log2 2 storage failed\n");
			return;
		}
	}

	/* must place this sentence before any printf until wirte to emmc */
	body->b_offset += buf_size + buf2_size;
	body->size += buf_size + buf2_size;
	if (body->size >= (p_log->end - (uint8_t*)LOG_BUFFER_START_ADDR))
		body->size = 0;

	memset((void *)hdr_buf, 0, LOG_HEADER_SIZE);
	memcpy((void *)hdr_buf, p_hdr, sizeof(LOG_PARTITION_HEADER));
	/* write back to header */
	if (0 != common_raw_write(UBOOT_LOG_PARTITION, (uint64_t)LOG_HEADER_SIZE, (uint64_t)0,
							  log_st.log_type_p_offset[p_hdr->type], (char*)hdr_buf)) {
		log_debug("write header 2 storage failed\n");
		return;
	}
}

void write_bootloader_last_log(void)
{
	boot_mode_t mode;
	uint8_t *buf;
	uint64_t buf_size;
	uint8_t *buf2 = NULL;
	uint64_t buf2_size = 0;

	if (log_st.flag != SUCESS) {
		return;
	}

	mode = get_boot_role();
	if (mode == BOOTLOADER_MODE_DOWNLOAD)
		return;

	if (p_log_buffer->pointer > p_log_buffer->addr) {
		buf = p_log_buffer->addr;
		buf_size = (p_log_buffer->pointer - buf);
	} else {
		buf = p_log_buffer->addr;
		buf_size = (p_log_buffer->end - p_log_buffer->addr);
		buf2 = (uint8_t *)LOG_BUFFER_START_ADDR;
		buf2_size = (p_log_buffer->pointer - buf2);
	}

	if ((buf_size + buf2_size > LOG_BUFFER_SIZE*2)
		|| (buf_size > LOG_BUFFER_SIZE*2)
		|| ( buf2_size > LOG_BUFFER_SIZE*2)) {
		log_debug("write_bootloader_last_log p_log_buffer->addr = %p, p_log_buffer->pointer = %p.\n", p_log_buffer->addr , p_log_buffer->pointer);
		log_debug("write_bootloader_last_log buf_size = %llu, buf2_size = %llu, overlay log buffer size\n", buf_size , buf2_size);
		return;
	}

	/* write current log to last log location*/
	if (buf2==NULL) {
		if (0 != common_raw_write(UBOOT_LOG_PARTITION, (uint64_t)buf_size, (uint64_t)0,
								  (uint64_t)LAST_LOG_PARTITION_OFFSET, (char*)buf)) {
			log_debug("write log 2 storage failed\n");
			return;
		}
	} else {
		if (0 != common_raw_write(UBOOT_LOG_PARTITION, (uint64_t)buf_size, (uint64_t)0,
								  (uint64_t)LAST_LOG_PARTITION_OFFSET, (char*)buf)) {
			log_debug("write log1 2 storage failed\n");
			return;
		}

		if (0 != common_raw_write(UBOOT_LOG_PARTITION, (uint64_t)buf2_size, (uint64_t)0,
								  (uint64_t)LAST_LOG_PARTITION_OFFSET + buf_size ,
								  (char*)buf2)) {
			log_debug("write log2 2 storage failed\n");
			return;
		}
	}
}
#ifdef LK_BACKUP_FOR_MINIDUMP_ADR
extern void save_bootloader_mem_to_minidump(void);
#endif
extern int save_minidump(int reset_mode, char *reason);
extern void system_reboot(u32 cmd);
#ifdef CONFIG_SPRD_SOCDUMP
extern void wdt_save_dbg_info(void);
#endif

void sprdlog_panic_finish(void)
{
	/* allow messages to go out */
	udelay(100000);
	flush_dcache_range(MEMBASE, 0x1000000);
	errorf("bootloader panic, save log ...\n");

	sprd_spin_unlock(&block_rw_lock);
	sprd_spin_unlock(&sprd_adi_lock);
	write_log_last();
	FTL_Panic_Flag_Write(0, LOG_RESERVED_ADDR);
#ifdef LK_BACKUP_FOR_MINIDUMP_ADR
	if(!(g_last_step & ( (1 << PHASE_MINIDUMP_MODE) | (1 << PHASE_FULLDUMP_MODE)))) {
#ifdef CONFIG_SPRD_SOCDUMP
		wdt_save_dbg_info();
#endif
		memcpy((void *)LK_BACKUP_FOR_MINIDUMP_ADR, (const void *)MEMBASE, 0x1000000); //copy lk mem for save
		save_bootloader_mem_to_minidump();
		save_minidump(CMD_BOOTLOADER_PANIC_MODE, "bootloader_panic");
	}
#endif
#if DEBUG
	if(g_last_step & (1 << PHASE_BLOCK_DONE)){
		fail_and_enter_fastboot_mode();
	}
#endif
	errorf("bootloader panic, reboot ...\n");
	system_reboot(HWRST_STATUS_BOOTLOADER_PANIC);
}

#else
/* Do Nothing */
void flush_log_buffer(void) {}
void sprdlog_panic_finish(void) {}
#endif

#ifdef CONFIG_SHOW_DEBUG_CRC
void show_crc_key_string(const void *buf, u32 size)
{
    uint32_t crc;

    crc = crc32(0, buf, size);

    dprintf(INFO,"show crc addr:0x%x, size: %d\n", buf, size);
    dprintf(INFO,"CRC key: 0x%x\n", crc);
}
#endif

void show_current_time(void)
{
    struct rtc_time tm;
    char time[32];

    tm = get_time_by_sec();
    snprintf(time, 31, "%04d-%02d-%02d_%02d-%02d-%02d",
                        tm.tm_year, tm.tm_mon, tm.tm_mday, \
                        tm.tm_hour,tm.tm_min, tm.tm_sec);
    dprintf(ALWAYS, "LK time is %s\n",time);
    return;
}

void init_sprdlog(sprdlog_init_stage_t level)
{
	switch(level) {
	case INIT_LOG_BUFFER:
		init_log_buffer();
		break;
	case INIT_LOG_WRITER:
		init_write_log();
		break;
	default:
		break;
	}
}

