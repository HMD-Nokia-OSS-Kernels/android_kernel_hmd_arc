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

#ifndef __SYSDUMP_H__
#define __SYSDUMP_H__

//#include <rtc.h>
//#include <config.h>
#include <linux/types.h>
//#include <elf.h>

#define SYSDUMP_CORE_NAME_FMT 	"sysdump.core.%02d"
#define SYSDUMP_DEVICE_NAME	"sd"
#define SYSDUMP_FOLDER_NAME "/ylog/sysdump"
#define SYSDUMP_DUMPINFO_NAME "dump_report.txt"
#define SYSDUMP_AUTO_TEST "/ylog/ap/sysdump/sysdump_auto_test.txt"
#define NR_KCORE_MEM		80
#define SYSDUMP_MAGIC		"SPRD_SYSDUMP_119"
#define MINIDUMP_MAGIC		"SPRD_MINIDUMP"
#define MAX_NUM_DUMP_MEM 40
#define DUMPINFO_FILE_SIZE (4 * 1024)

#ifndef SPRD_SYSDUMP_MAGIC
#define SPRD_SYSDUMP_MAGIC    RAMDISK_ADR
#endif

#ifdef CONFIG_ARM64
#define BOARD_ARCH "arm64"
#else
#define BOARD_ARCH "arm"
#endif


#ifdef CONFIG_X86
#define SYSDUMP_4GB   0xFFFFFFFF
#define SYSDUMP_256M  0x10000000
#define SYSDUMP_512M  0x20000000
#endif

#define PAGE_SHIFT 12
#ifndef PAGE_SIZE
#define PAGE_SIZE (1UL << PAGE_SHIFT)
#endif
#ifndef PAGE_ALIGN
#define PAGE_ALIGN(addr) ALIGN(addr, PAGE_SIZE)
#endif
#define SYSDUMP_FOLDER_NUM 3

#define VMCOREINFO_BYTES	(PAGE_SIZE * 2)

#ifdef CONFIG_X86
struct sysdump_mem {
	unsigned long long paddr;
	unsigned long long vaddr;
	unsigned long long soff;
	unsigned long long size;
	unsigned long long type;
};
#else
struct sysdump_mem {
	unsigned long paddr;
	unsigned long vaddr;
	unsigned long soff;
	unsigned long size;
	unsigned long type;
};
#endif

struct vmcore_info {
	uint64_t kaslr_offset;
	uint64_t kimage_voffset;
	uint64_t phys_offset;
	uint64_t vabits_actual;
};
struct minidump_info_record {
	char magic[20];
	uint64_t minidump_info_paddr;
	int minidump_info_size;
};
struct log_buf_info {
	uint64_t log_buf;
	uint32_t log_buf_len;
	uint64_t log_first_idx;
	uint64_t log_next_idx;
	uint64_t vmcoreinfo_size;
};
struct mmu_regs_info {
	uint32_t sprd_pcpu_offset;
	uint32_t cpu_id;
	uint32_t cpu_numbers;
	uint64_t paddr_mmu_regs_t;
};
struct core_regs_info {
	uint64_t pcpu_start_paddr;
	uint64_t paddr_core_regs_t;
};
struct sysdump_info {
	char magic[16];
	char time[32];
	char reason[32];
	char dump_path[128];
	int elfhdr_size;
	int mem_num;
#ifdef CONFIG_X86
	unsigned long long dump_mem_paddr;
#else
        unsigned long dump_mem_paddr;
#endif
	int crash_key;
	struct vmcore_info sprd_vmcoreinfo;
	struct minidump_info_record sprd_minidump_info;
	struct log_buf_info sprd_logbuf_info;
	struct mmu_regs_info sprd_mmuregs_info;
	struct core_regs_info sprd_coreregs_info;
};


enum sysdump_type {
	SYSDUMP_RAM,
	SYSDUMP_MODEM,
	SYSDUMP_IOMEM,
};

#define CORE_STR		"CORE"
#ifndef ELF_CORE_EFLAGS
#define ELF_CORE_EFLAGS	0
#endif

/* MMU REGS in vmcoreinfo */
#ifdef CONFIG_ARM64
struct sprd_debug_mmu_reg_t {
        unsigned long sctlr_el1;
        unsigned long ttbr0_el1;
        unsigned long ttbr1_el1;
        unsigned long tcr_el1;
        unsigned long mair_el1;
        unsigned long amair_el1;
        unsigned long contextidr_el1;
};
#elif (defined CONFIG_X86)
struct sprd_debug_mmu_reg_t {
        struct desc_ptrg idt;
        struct desc_ptrg gdt;
};
#else
struct sprd_debug_mmu_reg_t {
	int SCTLR;
	int TTBR0;
	int TTBR1;
	int TTBCR;
	int DACR;
	int DFSR;
	int DFAR;
	int IFSR;
	int IFAR;
	int DAFSR;
	int IAFSR;
	int PMRRR;
	int NMRRR;
	int FCSEPID;
	int CONTEXT;
	int URWTPID;
	int UROTPID;
	int POTPIDR;
};
#endif

/* An ELF note in memory */
struct memelfnote
{
	const char *name;
	int type;
	unsigned int datasz;
	void *data;
};

#define SETUP_NOTE 0

#if SETUP_NOTE
struct task_struct;

typedef unsigned long elf_greg_t;
typedef unsigned long elf_freg_t[3];

#define ELF_NGREG (sizeof (struct pt_regs) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

struct timeval {
	long tv_sec;     /* seconds */
	long tv_usec;    /* microseconds */
};


struct elf_siginfo
{
	int	si_signo;			/* signal number */
	int	si_code;			/* extra code */
	int	si_errno;			/* errno */
};
#endif

/* from asm/elf.h */

#ifdef CONFIG_ARM64
#define EM_ARM	0xb7
#else
#define EM_ARM	0x28
#endif
#define EM_X86_64	62

#define R_ARM_NONE		0
#define R_ARM_PC24		1
#define R_ARM_ABS32		2
#define R_ARM_CALL		28
#define R_ARM_JUMP24		29
#define R_ARM_V4BX		40
#define R_ARM_PREL31		42
#define R_ARM_MOVW_ABS_NC	43
#define R_ARM_MOVT_ABS		44

#define R_ARM_THM_CALL		10
#define R_ARM_THM_JUMP24	30
#define R_ARM_THM_MOVW_ABS_NC	47
#define R_ARM_THM_MOVT_ABS	48

/*
 * These are used to set parameters in the core dumps.
 */

#define PHYS_OFFSET    CONFIG_SYS_SDRAM_BASE

#ifdef CONFIG_X86
#define ELF_CLASS      ELFCLASS64
#define Elf_Off        Elf64_Off
#define PAGE_OFFSET    (unsigned long long)(0xffff880000000000)
#define KERNEL_TEXT    (unsigned long long)(0xffffffff80000000)

static inline unsigned long long __va( unsigned long long x ) {
	if(((unsigned long long)(x)) >= (KERNEL_BASE) && ((unsigned long long)(x)) < (KERNEL_BASE+0x20000000)){
		return ((unsigned long long)(x)) + KERNEL_TEXT - KERNEL_BASE;
	} else {
		return ((unsigned long long)(x)) + PAGE_OFFSET;
	}
}
#else
#ifdef CONFIG_ARM64
#define ELF_CLASS      ELFCLASS64
#define Elf_Off	       Elf64_Off
#define VA_BITS        39
#define PAGE_OFFSET    ((unsigned long)(0xffffffffffffffffUL) << (VA_BITS - 1))
#define __va(x)        ((unsigned long)((x) - PHYS_OFFSET + PAGE_OFFSET))
#else
#define ELF_CLASS       ELFCLASS32
#define Elf_Off	       Elf32_Off
#define PAGE_OFFSET    (0xC0000000UL)
#define __va(x)        ((unsigned long)((x) - PHYS_OFFSET + PAGE_OFFSET))
#endif
#endif

#ifdef __ARMEB__
#define ELF_DATA	ELFDATA2MSB
#else
#define ELF_DATA	ELFDATA2LSB
#endif

#ifdef CONFIG_X86
#define ELF_ARCH	EM_X86_64
#else
#define ELF_ARCH	EM_ARM
#endif


#define PT_GNU_STACK	(PT_LOOS + 0x474e551)

/*
 * Extended Numbering
 *
 * If the real number of program header table entries is larger than
 * or equal to PN_XNUM(0xffff), it is set to sh_info field of the
 * section header at index 0, and PN_XNUM is set to e_phnum
 * field. Otherwise, the section header at index 0 is zero
 * initialized, if it exists.
 *
 * Specifications are available in:
 *
 * - Sun microsystems: Linker and Libraries.
 *   Part No: 817-1984-17, September 2008.
 *   URL: http://docs.sun.com/app/docs/doc/817-1984
 *
 * - System V ABI AMD64 Architecture Processor Supplement
 *   Draft Version 0.99.,
 *   May 11, 2009.
 *   URL: http://www.x86-64.org/
 */
#define PN_XNUM 0xffff

#define elfhdr		elf64_hdr
#define elf_phdr	elf64_phdr
#define elf_shdr	elf64_shdr
#define elf_note	elf64_note
#define elf_addr_t	Elf64_Off
#define Elf_Half	Elf64_Half

//#endif

#ifdef CONFIG_X86
#define LLX			"0x%08llx"
#define SYSDUMP_LONG		unsigned long long
#define U32_T_U64(val1,val2)	(u64)((((u64)(val1))<<32)|((u64)(val2)))
#else
#define LLX			"0x%08lx"
#define SYSDUMP_LONG		unsigned long
#endif

#if (defined CONFIG_ARM64) || (defined CONFIG_X86)
typedef uint64_t mem_addr_t;
typedef uint64_t mem_size_t;
#else
typedef uint32_t mem_addr_t;
typedef uint32_t mem_size_t;
#endif

struct sysdump_memory {
	SYSDUMP_LONG addr;
	SYSDUMP_LONG size;
};


typedef struct smp_header{
	unsigned int header;
	unsigned short smp_length;
	unsigned char  lcn;
	unsigned char type;
	unsigned short reserved;
	unsigned short check_sum;
	unsigned int diag_sn;
	unsigned short diag_length;
	unsigned char diag_type;
	unsigned char diag_subtype;
	unsigned int sub_cmd_type;
}smp_header_t;

#define MAX_DUMP_PKT_SIZE	0x10000 /* max packet size including header */
#define DUMP_USB_ENUM_MS	300000
#define DUMP_USB_IO_MS		100000
#define FN_MAX_SIZE		128
#define PKT_SZ_40K		0xA000

/* sysdump2.0 start*/
#define BIT(x)                          (1 << x)
#define ORIG_STATUS			BIT(0)
#define AP_FULL_DUMP_ENABLE		BIT(1)
#define AP_MINI_DUMP_ENABLE		BIT(2)
#define AP_FULLDUMP_INTERNAL		BIT(4)
#define BOOT_FROM_DUMP_STATUS           BIT(9)
#define DUMP_FINISH_AUTO_REBOOT		BIT(15)
#define SIZEOF_UNSIGNED_LONG		BIT(16)
#define IS_STRUCT_PACKET		BIT(17)
#define SYSDUMP_STATUS_MASK		0xffffff00	/* init sysdump status in low bits every time but record high bits value */
#define IS_ORIG_STATUS(x)		(x & BIT(0) ? 0: 1)   /* x & BIT(0) = 1  means not orig status . or the value must be 0 .*/
#define IS_BOOT_FROM_DUMP(x)		(x & BIT(9) ? 1: 0)   /* x & BIT(3) = 1  means boot from dump. Not boot from the value must be 0 .*/
#define IS_FULLDUMP_INTERNAL(x)		(x & BIT(4) ? 1: 0)   /* x & BIT(4) = 1  means fulldump internal supoort */
#define IS_DUMPFINISH_AUTOREBOOT(x)	(x & BIT(15) ? 1: 0)   /* x & BIT(15) = 1  means need auto reboot when dump finish. */
#define SYSDUMPDB_PARTITION_DUMP_FLAG_OFFSET	(16) /* bit 16 ~23  'Y' */
#define CHECK_FUNC_MAX 10       /* sysdump enter check func max num*/
#define SYSDUMP_FILE_MAX 50     /* sysdump file number max */
#define DUMP_FILE_NAME_SIZE 60 /* sysdump file name size max */
#define ERROR_FNISH	0xff
#define CHECKSUM_DATA_LEN 0x1000
#define MALLOC_LIMIT (512 * 1024)
/*	need smaller than MALLOC_LIMIT,	avoid "Deflate need more space to compress left 512 bytes " error */
#define SRC_DATA_MAX (500 * 1024)


typedef  int (*CHECK_FUNC) (void);

/*	check result all (des)*/
struct dt_info{
	unsigned char *dt_addr;
	int dt_fail;
	int dts_version ;       /* 0:old version, memory region transfer by DTS;  0xA1: auto calculate linux memory region */
	int nodeoffset ;
	int ofs_remem ;
};

/* sdcard file system */
#define MOUNT_FLAG	"/fs_sdcard"
#define DEVICE_SD	"sd"
#define FS_FAT32	"fat32"
#define FS_EXFAT	"exfat"

#define FS_INVALID	0
#define FS_VALID	1

#define FS_NAME_LEN	12
#define FILE_NAME_MAX	128

/*	check result all (des)*/
struct check_dump_status_result{
        int full_dump_enable;
	int mini_dump_enable;
	int fulldump_internal_enbale;
        int full_dump_allow; /* enable and reset mode allow */
	int mini_dump_allow; /* enable and reset mode allow */
        int reset_mode;
	int sdcard_valid;
	char fs_type[FS_NAME_LEN];
	int path_pc;
	unsigned long sprd_sysdump_magic;
	int dump_to_pc_flag ;
	int dump_to_sd_fail ;
	int is_auto_reboot ;
};

int check_path_valid(void);
int check_dump_enable_status(void);

/*      sysdump target type 	在init&check storage path阶段判断*/
typedef enum {
        DUMP_TO_SDCARD = 0,
	DUMP_TO_PC = 1	,
	DUMP_TO_DATA=2, /**/
        INVALID ,
} sysdump_target;
struct dump_file_info {
        char name[DUMP_FILE_NAME_SIZE];				/*	file name，include path when to sd , not when to PC*/
        void *src_data_addr;					/*	src data paddr*/
        int src_data_size;					/*	src data size */
        int dump_level;						/*	dump contents priority，to control dump contens*/
        int dump_src_type;					/*	src data type like mem ，io，etc*/
};
/*      sysdump output contents description     */
struct dump_output_contents_info {
        struct dump_file_info file_info[SYSDUMP_FILE_MAX];
        int sysdump_target;         				/*	target ,dump to where, like sd, pc, internal */
        int fs_type;						/*	sd need this */
};

/*      dump resource contents description      */
struct dump_input_contents_info {
	int sprd_dump_mem_num;					/* 	Total dump mem block number*/
	int sprd_ptload_mem_num;				/* 	Total pt_load mem block number in elf_header*/
	struct sysdump_mem sprd_dump_mem[MAX_NUM_DUMP_MEM];	/*	All dump mem blocks */
	struct sysdump_mem sprd_ptload_mem[MAX_NUM_DUMP_MEM];	/*	elfheader pt_load memory block,exclude not linux kernel memroy in sprd_dump_mem*/
	struct sysdump_info *infop;
	int elfhdr_size;					/*	elf file header size */
	SYSDUMP_LONG mem_total_size ;				/*	elf file mem total size*/
	SYSDUMP_LONG file_total_size ;				/*	elf file file total size*/
	struct pt_regs *regs;
	unsigned char sysdump_checksum[CHECKSUM_DATA_LEN];
	int chk_length ;
};
enum {
	FULLDUMP_INTER_UNDO,
	FULLDUMP_INTER_INITED,
	FULLDUMP_INTER_DOING,
	FULLDUMP_INTER_ABORT,
	FULLDUMP_INTER_FINISH
};
enum {
	EXT4_FORMAT_FULLDUMP,
	EXT4_FORMAT_MINIDUMP
};
#define FULLDUMP_INFO_STRING "fulldump_info"
struct fulldump_file_info {
        char file_name[DUMP_FILE_NAME_SIZE];				/*	file name*/
        int file_offset;					/*	file offset in partition  */
        int file_size;					/*	file offset in partition  */
};
struct fulldump_to_internal_info {
	/*	here is fulldumpdata info after header
		[ file name(60Bytes) + file offset(4Bytes) ] * SYSDUMP_FILE_MAX(32) = 2048Bytes.
		First item is check data , all 64 Bytes is 'w'
		This is a fixed size.
	*/
	struct fulldump_file_info fulldump_fileinfo[SYSDUMP_FILE_MAX];
	int save_status;
	int data_start;
	int data_cur;
	int data_cur_index;
	void *comp_buf;

};

/* struct minidump_info */
#define MINI_SECTION_NUM_MAX 250
#define MINI_SECTION_NAME_LEN_MAX 40
struct mini_info {
        char s_name[MINI_SECTION_NAME_LEN_MAX];                /* section name */
        uint64_t s_paddr;               /* physical address */
        uint64_t s_vaddr;               /* vritual address */
        uint32_t s_size;                /* section size */
};
struct mini_info_total {
	struct mini_info mini_info[MINI_SECTION_NUM_MAX];
	int mini_section_num;
};

/* sysdump2.0 end*/
#endif //__SYSDUMP_H__
