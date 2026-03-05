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

/* sysdumpdb */
#define SYSDUMPDB_PARTITION_NAME "sysdumpdb"
#define SYSDUMPDB_SIZE  (10 * 1024 * 1024)      /* 10M */
#define MINIDUMP_DATA_OFFSET    256     /* sizeof(struct dumpdb_header), minidump start offset in sysdumpdb partition */
#define MINIDUMP_DATA_SPACE	(SYSDUMPDB_SIZE - MINIDUMP_DATA_OFFSET -1)

/* 60M for sysdumpdb backup */
#define SYSDUMPDB_SIZE_MAX (60 * 1024 *1024)
#define MINIDUMP_DATA_SIZE_MAX (20 * 1024 * 1024)
#define MINIDUMP_DATA_SECOND_OFFSET	MINIDUMP_DATA_SIZE_MAX
#define MINIDUMP_DATA_THIRD_OFFSET	(MINIDUMP_DATA_SIZE_MAX + MINIDUMP_DATA_SIZE_MAX)

/* compress */
#define SRC_DATA_MAX (500 * 1024)
#define MALLOC_LIMIT (512 * 1024)
#define COMP_SUFFIX_MAX	10


/* minidump func */
#define MINIDUMP_FUNC_MAX 10       /* sysdump enter check func max num*/
typedef int (*MINIDUMP_FUNC) (void);

#define MINI_NOTE       0               /* note info type */
#define MINI_SECTION    1               /* section info type */

#define AREA_NUM_MAX            20      /* area max number for minidump */
#define SECTION_NUM_MAX         200     /* sections max number for each area:note, section */
#define NOTE_NUM_MAX		AREA_NUM_MAX
#define AREA_NAME_LEN_MAX	40
#define SECTION_NAME_LEN_MAX	40

#define MINIDUMP_DATA_MAGIC	"MINIDUMP"

/* for bootloader section */
#define NUM_MAX 50
#define PADDR_DDR 0
#define PADDR_OTHER 1
struct section_info_extend{
        char section_name[SECTION_NAME_LEN_MAX];        /* the name of the section */
        uint64_t paddr;                                 /* the start addr of the section */
        uint64_t size;                                  /* the size of the section */
};
struct section_info_bootloader{
        char section_name[SECTION_NAME_LEN_MAX];
        uint64_t paddr;
        uint64_t size;
        int type;
};
struct extend_info_bootloader{
        struct section_info_bootloader section_info[NUM_MAX];
        int total_num;
};
extern int sysdump_save_extend_info(struct section_info_extend *section_info_extend);

/* for kernel section */
#define REGS_NUM_MAX 50                 /* max dump regs num in minidump,real num in regs_info_item  */
#define KERNEL_MEM_MAX (REGS_NUM_MAX + SECTION_NUM_MAX + 1)
#define OSRELEASE_SIZE 300              /* Linux version 4.4.83+ (builder@shhud14) (gcc version 4.9.x 20150123 (prerelease) (GCC) ) #1 SMP PREEMPT Tue Sep 11 12:47:50 CST 2018*/
#define TIME_SIZE 50                    /* 2018-12-12-20-24-24 */
#define REBOOT_REASON_SIZE 50           /* kernel crash */
#define CONTENTS_DESC_SIZE (1024 - OSRELEASE_SIZE - TIME_SIZE - REBOOT_REASON_SIZE)
#define KERNEL_MAGIC "K2.0"
#define EXCEPTION_INFO_SIZE_SHORT 256
#define EXCEPTION_INFO_SIZE_MID 512
#define EXCEPTION_INFO_SIZE_LONG  0x2000
#define MAX_STACK_TRACE_DEPTH 32
struct minidump_data_desc{
        char osrelease[OSRELEASE_SIZE];
        char time[TIME_SIZE];
        char reboot_reason[REBOOT_REASON_SIZE];
        char minidump_data[CONTENTS_DESC_SIZE];
};
struct exception_info_item{
        char kernel_magic[8];  /* "K2.0" :make sure excep data valid */
        char exception_serialno[EXCEPTION_INFO_SIZE_SHORT];
        char exception_kernel_version[EXCEPTION_INFO_SIZE_MID];
        char exception_reboot_reason[EXCEPTION_INFO_SIZE_SHORT];
        char exception_panic_reason[EXCEPTION_INFO_SIZE_SHORT];
        char exception_time[EXCEPTION_INFO_SIZE_SHORT];
        char exception_file_info[EXCEPTION_INFO_SIZE_SHORT];
        int  exception_task_id;
        char exception_task_family[EXCEPTION_INFO_SIZE_SHORT];
        char exception_pc_symbol[EXCEPTION_INFO_SIZE_SHORT];
        char exception_stack_info[EXCEPTION_INFO_SIZE_LONG];
};

struct regs_info{
        int arch;       /* 32bit reg unsigned int and 64bit unsigned long, we need use type to mark*/
        int num;        /* the num of regs will save memery amount */
        unsigned long vaddr;    /* struct pt_regs vaddr */
        unsigned long paddr;    /* struct pt_regs paddr */
        int size;       /* sizeof(struct pt_regs)  */
        int size_comp;  /* size after compressed  */
};

struct regs_memory_info{
        unsigned long reg_vaddr[REGS_NUM_MAX - 1];      /* if vaddr invalid set it as 0*/
        unsigned long reg_paddr[REGS_NUM_MAX - 1];      /* if paddr invalid set it as 0*/
        int per_reg_memory_size;                /* memory size amount reg */
        int per_mem_size_comp[REGS_NUM_MAX - 1];                        /* size after compressed  */
        int valid_reg_num;                      /* maybe some regs value not a valid addr */
        int size;                               /* memory size amount reg */
};
struct section_info{
        char section_name[SECTION_NAME_LEN_MAX];
        /*Get teh value in kernel use to record elfhdr info in uboot*/
        unsigned long section_start_vaddr;
        unsigned long section_end_vaddr;
        /*Get the value in kernel by __pa  use to get memory contents in uboot */
        unsigned long section_start_paddr;
        unsigned long section_end_paddr;
        int section_size;
        int section_size_comp;                  /* size after compressed  */
};
struct section_info_total{
        struct section_info section_info[SECTION_NUM_MAX];
        int total_size;
        int total_num;
};
struct kernel_info{
        char kernel_magic[6];                           /* make sure kernel data valid */
        struct regs_info regs_info;                     /* | struct pt_regs |                   */
        struct regs_memory_info regs_memory_info;       /* | memory amount regs |  , need paddr and size, if paddr invalid set it as 0  */
        struct section_info_total section_info_total;   /* | sections | , text,rodata,page_table ....,may be logbuf in here */
        int minidump_elfhdr_size;                       /* minidump elfhdr data size: update in uboot  */
        int minidump_elfhdr_size_comp;                  /* minidump elfhdr data size,after compressed  */
        struct minidump_data_desc  desc;                /* minidump contents description */
        int minidump_data_size;                         /* minidump data total size: regs_all_size + reg_memory_all_size + section_all_size  */
        int compressed;                                 /* indicate if minidump data compressed */
        struct exception_info_item exception_info;      /* exception info */
};


/* minidump data struct */
struct mini_data_header {
	char data_magic[12];	/* minidump: data valid */
	uint32_t area_num;      /* the number of area */
	uint32_t arch_info;      /* 0:arm, 1: arm64; 2:x86 */
	uint32_t h_size;                /* the size of area_info_header*/
	uint32_t ainfo_size;            /* the size of area_info */
};

struct mini_area_info {
	char a_name[AREA_NAME_LEN_MAX];                /* area name */
	uint8_t data_flag;              /* 0: area data invalid, 1: area data valid*/
	uint32_t area_type;		/* AREA_NOTE */
	uint32_t a_size;        /* area size */
	uint32_t a_offset;      /* area offset in sysdumpdb */
};

struct mini_area_header {
	uint32_t section_num;   	/* section number */
	uint32_t section_start_index;	/* valid section index */
	uint32_t ah_size;               /* the size of struct mini_section_info_header */
	uint32_t sinfo_size;            /* the size of struct mini_section_info */
	uint8_t comp_type;              /* enum compress_type */
	char comp_suffix[COMP_SUFFIX_MAX];
};

struct mini_section_info {
	char s_name[SECTION_NAME_LEN_MAX];                /* section name */
	uint64_t s_paddr;		/* physical address */
	uint64_t s_vaddr;		/* vritual address */
	uint32_t s_size;                /* section size */
	uint32_t s_offset;		/* offset in sysdumpdb partition */
};

/*area info */
#define AREA_NOTE_NAME			"notes"
#define AREA_BOOTLOADER_NAME		"bootloader"
#define AREA_KERENL_NAME		"kernel"
#define AREA_MODEM_NAME			"modem"
#define AREA_TOS_NAME			"trusty"

const char *area_name_g[] = {
	"notes",	/* AREA_NOTE */
	"bootloader",	/* AREA_BOOTLOADER */
	"kernel",	/* AREA_KERENL */
	"modem",	/* AREA_MODEM */
	"trusty",	/* AREA_TOS */
	"undefined"	/* AREA_END_NUM */
};

enum area_type {
	AREA_NOTE = 0,
	AREA_BOOTLOADER,
	AREA_KERNEL,
	AREA_MODEM,
	AREA_TOS,

	AREA_END_NUM,
};

#define FIRST_AREA_OFFSET (MINIDUMP_DATA_OFFSET + \
		sizeof(struct mini_data_header) + \
		AREA_END_NUM * sizeof(struct mini_area_info))


/* compressed type */
#define COMPRESS_GZIP_SUF	".gz"
#define COMPRESS_ZLIB_SUF	".zlib"

#define COMPRESS_RECORD_NAME	"compress_record_file"
#define TMP_SIZE (4 * 1024)
#define COMPRESSED_LEN_MALLOC_MAX   (SECTION_NAME_LEN_MAX * KERNEL_MEM_MAX + TMP_SIZE)

enum compress_type {
	NO_COMPRESS = 0,
	COMPRESS_GZIP,
	COMPRESS_ZLIB,
};

const char *compress_suffix_g[] = {
	"null",			/* NO_COMPRESS */
	COMPRESS_GZIP_SUF,	/* COMPRESS_GZIP */
	COMPRESS_ZLIB_SUF,	/* COMPRESS_ZLIB */
};

/* note info */
const char *note_file[NOTE_NUM_MAX] = {
	"dump_report.txt",
};

#define DUMP_TIME_LEN	32

enum error_code {
	COMP_ERROR,
	WRITE_ERROR,
	NO_SPACE_ERROR,

	NO_ERROR = 0,
};
