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

#ifndef __SPRD_LOG_H__
#define __SPRD_LOG_H__
#include <sprd_common.h>
#include <stdlib.h>
#ifdef CONFIG_LOG_2_SD
#include "../common/loader/sysdump.h"
#endif
#ifdef CONFIG_SHOW_DEBUG_CRC
#include <lib/cksum.h>
#endif
#define LOG_BUFFER_START_ADDR	ALIGN(LOG_RESERVED_ADDR + sizeof(LOG_BUFFER), 8)
#ifndef LOG_BUFFER_SIZE
#define LOG_BUFFER_SIZE		0x040000 /* 256KB */
#endif

#define LOG_FOLDER_NUM		3
#define LOG_FOLDER_NAME		"ylog/ap/uboot"
#define LOG_AUTO_TEST		"ylog/uboot/uboot_log_auto_test.txt"

#define UBOOT_LOG_PARTITION	"uboot_log"
#define LOG_HEAD_MAGIC		0x000abcde
#define LOG_HEADER_SIZE		512

#ifndef LOG_PARTITION_SIZE
#define LOG_PARTITION_SIZE	0x400000
#endif

/* log body num */
#if LOG_PARTITION_SIZE >= 0x1000000
/* configs for 16MB log partion */
#define LOG_PANIC_BODY_NUM	28 //32
#define LOG_CBOOT_BODY_NUM	16
#define LOG_FASTBOOT_BODY_NUM	8
#define LOG_DOWNLD_BODY_NUM	8
#else
/* configs for 4MB log partion */
#define LOG_PANIC_BODY_NUM	3
#define LOG_CBOOT_BODY_NUM	5
#define LOG_FASTBOOT_BODY_NUM	2
#define LOG_DOWNLD_BODY_NUM	2
#endif

#define SUCESS		1
#define FAILED		0

#define LAST_LOG_PARTITION_OFFSET       (0)
/* log position for spl help lk write lk panic log*/
#define PANIC_SPL_HELP_SAVE_OFFSET		(LAST_LOG_PARTITION_OFFSET + LOG_BUFFER_SIZE * 2)
/* log position for panic */
#define PANIC_LOG_PARTITION_OFFSET		(PANIC_SPL_HELP_SAVE_OFFSET + LOG_BUFFER_SIZE * 2)
/* log position for cboot (0x200000) */
#define START_LOG_PARTITON_OFFSET		(PANIC_LOG_PARTITION_OFFSET + LOG_BUFFER_SIZE * LOG_PANIC_BODY_NUM)
/* log position for fastboot (0x300000) */
#define FASTBOOT_LOG_PARTITION_OFFSET		(START_LOG_PARTITON_OFFSET + LOG_BUFFER_SIZE * LOG_CBOOT_BODY_NUM)
/* log position for download (0x380000) */
#define DOWNLD_LOG_PARTITION_OFFSET		(FASTBOOT_LOG_PARTITION_OFFSET + LOG_BUFFER_SIZE * LOG_FASTBOOT_BODY_NUM)

/* check configs */
#define LOG_TOTAL_BODY_NUM (LOG_PANIC_BODY_NUM + LOG_CBOOT_BODY_NUM + LOG_FASTBOOT_BODY_NUM + LOG_DOWNLD_BODY_NUM)
STATIC_ASSERT(LOG_PARTITION_SIZE >= ((LOG_TOTAL_BODY_NUM) * LOG_BUFFER_SIZE ));

typedef enum {
	LR_NORMAL,
	LR_ABNORMAL,
	LR_LONG_PRESS,
	LR_UNKNOWN,
} LOG_REBOOT_TYPE_T;

typedef enum {
	PANIC_LOG_TYPE = 0,
	START_LOG_TYPE,
	FASTBOOT_LOG_TYPE,
	DOWNLD_LOG_TYPE,
	MAX_LOG_TYPE
} LOG_TYPE_T;

typedef struct _LOG_BUFFER {
	uint64_t magic;		/* 53 50 52 44 6c 6f 67 31 -- SPRDlog1*/
	uint8_t* addr;
	uint8_t* end;
	uint8_t* pointer;
	uint8_t* log_save_pointer;
	uint32_t record_count;
	uint64_t size;
} LOG_BUFFER;

typedef struct _LOG_BODY {
	uint64_t p_offset;	/* offset in bootloader log body */
	uint64_t p_end;	/* offset in bootloader log body end */
	uint64_t b_offset;	/* offset in log body, it also indicates the log size in emmc */
	uint32_t size;
} LOG_BODY;

typedef struct _LOG_PARTITION_HEADER {
	uint32_t magic;
	uint32_t len;
	uint32_t boot_count;
	uint32_t body_num;
	uint32_t type;
	LOG_BODY body;
} LOG_PARTITION_HEADER;

typedef struct _LOG_STRUCT {
	int32_t flag;
	uint64_t log_type_p_offset[MAX_LOG_TYPE];
	LOG_PARTITION_HEADER log_par_hdr[MAX_LOG_TYPE];
} LOG_STRUCT;

unsigned long get_bootloader_log_addr(void);
uint32_t get_bootloader_log_len(void);
#if defined(CONFIG_LOG_2_STORAGE)
void init_log_struct(void);
int init_log_partition_hdr(void);
void write_bootloader_last_log(void);
#endif
void flush_log_buffer(void);
extern void cleanup_cache_environment(void);
void sprd_log_cache_flush(void);

#ifdef CONFIG_SPRD_LOG
#if DEBUG
#if defined(CONFIG_LOG_2_STORAGE)
extern int reinit_flag;
#define reinit_write_log() do {	\
	reinit_flag = 1;	\
	init_log_partition_hdr();	\
	flush_log_buffer();	\
	reinit_flag = 0;	\
} while(0)

#define init_write_log() do {		\
	init_log_struct();		\
	init_log_partition_hdr();	\
} while(0)

#define write_log() flush_log_buffer()
#define write_log_before_entry_kernel() do {		\
	flush_log_buffer();		\
	write_bootloader_last_log(); \
} while(0)

#define write_log_last() do {		\
	flush_log_buffer();		\
	write_bootloader_last_log(); \
	sprd_log_cache_flush();	\
} while(0)

# else
#define write_log_last()
#define write_log_before_entry_kernel()
# endif

#else //DEBUG
#define reinit_write_log()
#define init_write_log()
#define write_log()

# if defined(CONFIG_LOG_2_STORAGE)
#define write_log_before_entry_kernel() do {		\
	init_log_struct();		\
	init_log_partition_hdr();	\
	flush_log_buffer();		\
	write_bootloader_last_log();	\
} while(0)

#define write_log_last() do {		\
	init_log_struct();		\
	init_log_partition_hdr();	\
	flush_log_buffer();		\
	write_bootloader_last_log();	\
	sprd_log_cache_flush();	\
} while(0)
# else
#define write_log_last()
#define write_log_before_entry_kernel()
# endif

#endif //DEBUG
#else //CONFIG_SPRD_LOG
#define init_write_log()
#define reinit_write_log()
#define write_log()
#define write_log_last()
#define write_log_before_entry_kernel()
#endif //CONFIG_SPRD_LOG

typedef enum {
    INIT_LOG_BUFFER = 0,     // init for log buffer
    INIT_LOG_WRITER,         // init before writing emmc/ufs/sd
} sprdlog_init_stage_t;

extern LOG_STRUCT log_st;
extern LOG_BUFFER *p_log_buffer;
#ifdef CONFIG_DTS_MEM_LAYOUT
extern uint32_t log_buffer_enabled_flag;
#endif
extern LOG_REBOOT_TYPE_T lr_cause;

void update_sprdlog_buffer(const char *printbuffer, int len);
void update_bootloader_log(char str);
void check_bootloader_log_buffer(void);
void sprdlog_panic_finish(void);
void init_sprdlog(sprdlog_init_stage_t level);
void show_current_time(void);
int log_save(const char *fmt, ...);
#ifdef CONFIG_SHOW_DEBUG_CRC
void show_crc_key_string(const void *buf, u32 size);
#endif

#endif //__SPRD_LOG_H__
