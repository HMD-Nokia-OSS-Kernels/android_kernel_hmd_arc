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

#include <minidump.h>
#include <sysdump_common.h>
#include <sysdump.h>
#include <stdio.h>
#include <lk/err.h>
#include <sprd_log.h>
#include <string.h>
#include <sprd_wdt.h>
#include <lib/miniz.h>
#include <sprd_common_rw.h>
#include <malloc.h>
#include <asm/arch/check_reboot.h>
#include <errno.h>
#ifdef CONFIG_TEECFG
#include <teecfg_parse.h>
#endif
#include <sprd_rtc_def.h>


static struct mini_data_header g_data_header;
static struct mini_area_info g_area_data_info[AREA_END_NUM];

static int minidump_data_end_g;
static int backup_flag;

struct extend_info_bootloader extend_info_bootloader;
extern int is_paddr_linux_memory(unsigned long paddr);
extern phys_size_t get_real_ram_size(void);

/* for fulldump save minidump_section */
int minidump_section_num;
struct mini_info_total *mini_info_total;


/* for soc_dump */
#ifdef CONFIG_ETB_DUMP
#define ETB_DATA "etb_data"
extern unsigned char etb_dump_mem[SZ_32K];
extern u32 etb_buf_size;
extern void sprd_etb_hw_dis(void);
extern void sprd_etb_dump (void);
#endif

/* for tos&sml mem */
#define TOS_MEM "tos_mem"
#define SML_MEM "sml_mem"

/* for bootloader mem */
#define BOOTLOADER_MEM "bootloader_mem"
#define BOOTLOADER_LAST_LOG "bootloader_last_log"
#define AON_IRAM "aon_iram"

/* for kernel data */
int reboot_mode;
struct sysdump_info info_g;
struct kernel_info *kernel_infop;
struct sysdump_mem *kernel_mem;
#if defined (CONFIG_ARM7_RAM_ACTIVE) && !defined (CONFIG_SPRD_SP_UART)
extern void pmic_arm7_RAM_active(void);
#endif

/* for note data */
extern const char *bootcause_cmdline;
int area_note_index;
char* txt_info = NULL;
struct mini_area_header sprd_note_header;
struct mini_section_info *sprd_note_info;
extern unsigned reboot_reg;
extern struct rtc_time get_time_by_sec(void);
char dump_time[32];
uint32_t s_length;

void get_time(void)
{
	struct rtc_time tm;
        tm = get_time_by_sec();
        snprintf(dump_time, 31, "%04d-%02d-%02d_%02d-%02d-%02d",
                        tm.tm_year, tm.tm_mon, tm.tm_mday, \
                        tm.tm_hour,tm.tm_min, tm.tm_sec);
}

static unsigned long get_partation_total_size(char *part_name)
{
	int dev_id = 0;
	char *ifname;
	unsigned long total_size = 0;
	disk_partition_t part_info;
	block_dev_desc_t *dev_desc;
	int ret;

	ifname = block_dev_get_name();

	dev_id = get_devnum_hwpart(ifname, USER_PART);
	dev_desc = get_dev_hwpart(ifname, dev_id, USER_PART);
	if (NULL == dev_desc) {
		errorf("invalid dev_desc!\n");
		return -ENODEV;
	}
	ret = get_partition_info_by_name(dev_desc, part_name, &part_info);
	if (0 == ret) {
		total_size = (uint64_t)part_info.blksz * (uint64_t)part_info.blk_cnt;
	} else {
		errorf("get total size error!,ret=%d\n", ret);
	}

	return total_size;
}
static int partition_data_move(char *part_name, int total_size, int start_pos, int target_pos)
{
	char *tmp_buf;
	int tmp_buf_size = 512 * 1024;
	int re_size = total_size;
	int start_offset = start_pos;
	int target_offset = target_pos;

	tmp_buf = malloc(tmp_buf_size);
	if (tmp_buf == NULL) {
		dump_loge("alloc tmp buffer fail, size:%d\n", tmp_buf_size);
		return -1;
	}
	while (re_size > tmp_buf_size) {
		if(common_raw_read(SYSDUMPDB_PARTITION_NAME, tmp_buf_size, start_offset, tmp_buf))
			dump_loge("read sysdumpdb failed, size:%d, offset:%d\n", tmp_buf_size, start_offset);

		if(common_raw_write(SYSDUMPDB_PARTITION_NAME, tmp_buf_size, tmp_buf_size, target_offset, tmp_buf))
			dump_loge("write sysdumpdb failed, size:%d, offset:%d\n", tmp_buf_size, target_offset);
		start_offset += tmp_buf_size;
		target_offset += tmp_buf_size;
		re_size -= tmp_buf_size;
	}
	if (common_raw_read(SYSDUMPDB_PARTITION_NAME, re_size, start_offset, tmp_buf))
		dump_loge("read sysdumpdb failed, size:%d, offset:%d\n", re_size, start_offset);
	if (common_raw_write(SYSDUMPDB_PARTITION_NAME, re_size, re_size, target_offset, tmp_buf))
		dump_loge("write sysdumpdb failed, size:%d, offset:%d\n", re_size, target_offset);

	free(tmp_buf);
	return 0;
}
static int minidump_data_backup_in_partition(void)
{
	struct mini_data_header data_header;
	int first_offset;
	int second_offset;
	int third_offset;
	int re_size;

	if(!backup_flag)
		return 0;

	memset(&data_header, 0, sizeof(struct mini_data_header));
	if(common_raw_read(SYSDUMPDB_PARTITION_NAME, (uint64_t)(sizeof(struct mini_data_header)),(uint64_t)MINIDUMP_DATA_OFFSET, (char *)&data_header)){
		dump_loge("read sysdumpdb partition failed!\n");
		return -1;
	}

	if(memcmp(MINIDUMP_DATA_MAGIC, data_header.data_magic, strlen(MINIDUMP_DATA_MAGIC))) {
		return 0;
	} else {
		dump_loga("MINIDUMP magic checked, need to backup data!!\n");
		re_size = minidump_data_end_g - MINIDUMP_DATA_OFFSET;
		second_offset = MINIDUMP_DATA_SECOND_OFFSET;
		third_offset = MINIDUMP_DATA_THIRD_OFFSET;
		/*2 to 3*/
		dump_loga("backup minidump 2 -> 3\n");
		partition_data_move(SYSDUMPDB_PARTITION_NAME, re_size, second_offset, third_offset);
		/*1 to 2*/
		dump_loga("backup minidump 1 -> 2\n");
		first_offset = MINIDUMP_DATA_OFFSET;
		partition_data_move(SYSDUMPDB_PARTITION_NAME, re_size, first_offset, second_offset);
		dump_loga("backup minidump data ok!\n");
	}

	return 0;
}
static void sysdumpdb_partition_init(void)
{
	unsigned long sysdumpdb_total_size;

	sysdumpdb_total_size = get_partation_total_size(SYSDUMPDB_PARTITION_NAME);

	dump_logd("sysdumpdb total size is %ld bytes\n", sysdumpdb_total_size);
	if(sysdumpdb_total_size < SYSDUMPDB_SIZE_MAX) {
		minidump_data_end_g = sysdumpdb_total_size;
		backup_flag = 0;
	} else {
		minidump_data_end_g = MINIDUMP_DATA_SIZE_MAX;
		backup_flag = 1;
	}
}

#if defined (CONFIG_ARM7_RAM_ACTIVE) && !defined (CONFIG_SPRD_SP_UART)
extern void pmic_arm7_RAM_active(void);
#endif

#define CM4_MEM "cm4_mem"
/**
 * save extend debug information of modules to section_info, such as: cm4, iram...
 * @section_info_extend:  the paddr of the struct section_info_extend
 *
 * Return: 0 means success, -1 means fail.
 *
 */
int sysdump_save_extend_info(struct section_info_extend *section_info_extend)
{
        int i, j, index;
        char str_name[SECTION_NAME_LEN_MAX];
        char tmp;

        /* check the name of section */
        if (section_info_extend == NULL){
                dump_loge("%s: %s The address of section_info is NULLL!\n", SYSDUMPDB_LOG_TAG, __FUNCTION__);
                return -1;
        }
	dump_logd("the section name is %s \n", section_info_extend->section_name);
        memset(str_name, 0, SECTION_NAME_LEN_MAX);
        memcpy(str_name, section_info_extend->section_name, strlen(section_info_extend->section_name));
        if (strlen(str_name) > (SECTION_NAME_LEN_MAX - 2)) {
                dump_loge("%s: %s The length of name is too long!!, add extend section fail!!\n", SYSDUMPDB_LOG_TAG, __FUNCTION__);
                return -1;
        }
        if (!strlen(str_name)) {
                dump_loge("%s: %s The name is empty, invalid!!\n", SYSDUMPDB_LOG_TAG, __FUNCTION__);
                return -1;
        }
        for (j = 0; j < strlen(str_name); j++) {
                tmp = str_name[j];
                if (tmp == '?' || tmp == '*' || tmp == '/' || tmp == '>' || tmp == '<'
                                || tmp == '"' || tmp == '|') {
                        dump_loge("the name=%s include special character:%c that not supported!!\n",
                                        str_name, tmp);
                        return -1;
                }
        }

        /* check insert repeatly and acquire total seciton num before insert new section */
        for (i = 0; i < NUM_MAX; i++) {
                if (!memcmp(str_name, extend_info_bootloader.section_info[i].section_name, strlen(str_name))) {
                        dump_loge("same name, add new section fail!!\n");
                        return -1;
                }
	}
        /* check insert repeatly and acquire total seciton num before insert new section */
        for (i = 0; i < NUM_MAX; i++) {
                if (!memcmp(str_name, extend_info_bootloader.section_info[i].section_name, strlen(str_name))) {
                        dump_loge("same name, add new section fail!!\n");
                        return -1;
                }
                if (!strlen(extend_info_bootloader.section_info[i].section_name))
                        break;
        }
        extend_info_bootloader.total_num = i;
        index = extend_info_bootloader.total_num;
        if (index >= (NUM_MAX - 1)) {
                dump_loge("No space for new section.\n");
                return -1;
        }
	/* check the paddr of the section */
	if (!section_info_extend->paddr){
		dump_loge("The addr of the section is NULL! .\n");
		return -1;
	}
        /* add new section to section_info */
        snprintf(extend_info_bootloader.section_info[index].section_name, SECTION_NAME_LEN_MAX - 1, "%s", str_name);
        extend_info_bootloader.section_info[index].paddr = section_info_extend->paddr;
        extend_info_bootloader.section_info[index].size = section_info_extend->size;
	if(is_paddr_linux_memory(section_info_extend->paddr))
		extend_info_bootloader.section_info[index].type = PADDR_DDR;
	else
		extend_info_bootloader.section_info[index].type = PADDR_OTHER;
        extend_info_bootloader.total_num++;
        return 0;

}
int add_minidump_section_to_fulldump(struct mini_area_header *area_header,
				struct mini_section_info *section_info)
{
	int i,start_index;
	if (!mini_info_total){
		dump_loga("mini_info_total is NULL\n");
		return 0;
	}
	if (area_header->comp_type == COMPRESS_ZLIB)
		start_index = 1;
	else
		start_index = 0;
	for(i=start_index;i<area_header->section_num;i++){
		memset(mini_info_total->mini_info[minidump_section_num].s_name, 0, MINI_SECTION_NAME_LEN_MAX);
		memcpy(mini_info_total->mini_info[minidump_section_num].s_name, section_info[i].s_name, sizeof(section_info[i].s_name));
		mini_info_total->mini_info[minidump_section_num].s_size = section_info[i].s_size;
		mini_info_total->mini_info[minidump_section_num].s_paddr = section_info[i].s_paddr;
		mini_info_total->mini_info[minidump_section_num].s_vaddr = section_info[i].s_vaddr;
		minidump_section_num++;
	}
	dump_logd("minidump_section_num = %d\n", minidump_section_num);
	mini_info_total->mini_section_num = minidump_section_num;
	return 0;
}
void show_extend_info(void)
{
        int i;
        dump_logd("extend_info_bootloader total_num:              %d \n", extend_info_bootloader.total_num);
        for(i=0;i<extend_info_bootloader.total_num;i++){
                dump_logd("section_name: %s \n", extend_info_bootloader.section_info[i].section_name);
                dump_logd("paddr: 0x%llx \n", extend_info_bootloader.section_info[i].paddr);
                dump_logd("size: 0x%llx \n", extend_info_bootloader.section_info[i].size);
		dump_logd("type: %d \n", extend_info_bootloader.section_info[i].type);
        }
}

void mini_data_init(void)
{
	memset(&g_data_header, 0, sizeof(struct mini_data_header));
	memset(&g_area_data_info, 0, AREA_END_NUM * sizeof(struct mini_area_info));
	g_data_header.area_num = 0;
#ifdef COMFIG_ARM
	g_data_header.arch_info = 0;
#endif
#ifdef COMFIG_ARM64
	g_data_header.arch_info = 1;
#endif
#ifdef COMFIG_X86
	g_data_header.arch_info = 2;
#endif
	g_data_header.h_size = sizeof(struct mini_data_header);

	g_data_header.ainfo_size = sizeof(struct mini_area_info);

	return;
}

uint32_t get_area_offset(int area_index)
{
	int i;
	uint32_t area_offset = 0;

	area_offset += FIRST_AREA_OFFSET;

	for(i = 0; i < area_index; i++) {
		area_offset += g_area_data_info[i].a_size;
	}

	return area_offset;
}
static int compress_data(void *des, unsigned long *outlen, unsigned char *src, unsigned long srclen)
{
	int ret = 0;
	dump_logd(" %s: before compress  des : %p srclen : 0x%lx\n", __func__, des, srclen);
	memset(des, 0, *outlen);
	/* when gzip return 0 ,means compressed ok */
	ret = mz_compress(des, outlen, src, srclen);
	if(ret){
		dump_logd(" compress data  fail ..\n");
		return -1;
	}
	dump_logd(" %s: after compress outlen : 0x%lx\n", __func__, *outlen);
	return ret;
}
static int sysdumpdb_write(char *part_name, uint32_t *size, uint32_t updsize, uint32_t offset, uint64_t buf)
{
	uint32_t total_size = *size;
	uint32_t free_size;
	int ret = 0;

	free_size = minidump_data_end_g - offset;

	if(offset < minidump_data_end_g && (offset + total_size) > minidump_data_end_g) {
#ifdef CONFIG_NAND_BOOT
		/* int do_raw_data_write(char *part, u32 updsz, u32 size, u32 off, char *buf) */
		ret = do_raw_data_write(part_name, free_size, free_size, offset, buf);
#else
		dump_logd("sysdumpdb_write: free_size:0x%x, offset:0x%x\n", free_size, offset);
		/* int common_raw_write(char *part_name, uint64_t size, uint64_t updsize, uint64_t offset, char *buf) */
		ret = common_raw_write(part_name, (uint64_t)free_size, (uint64_t)free_size, (uint64_t)offset, buf);
#endif
		*size = free_size;
		return NO_SPACE_ERROR;
	} else if(offset >= minidump_data_end_g) {
		*size = 0;
		return NO_SPACE_ERROR;
	} else {
#ifdef CONFIG_NAND_BOOT
		/* int do_raw_data_write(char *part, u32 updsz, u32 size, u32 off, char *buf) */
		ret = do_raw_data_write(part_name, total_size, total_size,  offset, buf);
#else
		dump_logd("sysdumpdb_write: total_size:0x%x, offset:0x%x\n", total_size, offset);
		/* int common_raw_write(char *part_name, uint64_t size, uint64_t updsize, uint64_t offset, char *buf) */
		ret = common_raw_write(part_name, total_size, total_size, offset, buf);
#endif
	}
	return ret;
}

int minidump_data_write(char* compressed_buf,
		char* compress_record_buf,
		int *record_length,
		uint32_t *size,
		uint64_t addr,
		uint32_t offset,
		char* name)
{
	unsigned long fix_len = 0;
	int compress_counter;
	int step_size = 0;
	uint32_t src_data_max = SRC_DATA_MAX; /* 12K smaller than compress buf len */
	int comp_length = *record_length;
	char *string;
	uint32_t file_size = *size;
	uint32_t update_offset;
	int re_length;
	unsigned long compressed_size;
	int malloc_limit = MALLOC_LIMIT;

	update_offset = offset;
	comp_length = *record_length;
	string = compress_record_buf;

	if(compressed_buf) {
		dump_logd("%s:need compress-----total data size: 0x%x, offset:0x%x\n", __FUNCTION__, file_size, offset);
		/* if the data length more than malloc_limit ,we need compresse step by step */
		compress_counter = step_size = 0;  /* record circle num as src data index  */
		*size = step_size;
		while (file_size > src_data_max) {
			fix_len = malloc_limit; /* compress malloc_limit data every time, src data pointor need have a offset too. */
			if(compress_data(compressed_buf, &fix_len, (char*)addr + compress_counter * src_data_max, src_data_max)){
				return COMP_ERROR;
			}
			if (update_offset+ fix_len > minidump_data_end_g) {
				*size = step_size;
				dump_loge("over size in compressing\n");
				return NO_SPACE_ERROR;
			}

			/* after compressed, we record circle number, rest data length, and compressed output length */
			if (sysdumpdb_write(name, &fix_len, fix_len, update_offset, compressed_buf)) {
				dump_loge(" write %s error.\n", SYSDUMPDB_PARTITION_NAME);
				return WRITE_ERROR;
			}
			if(string) {
				re_length = COMPRESSED_LEN_MALLOC_MAX - comp_length;
				if (re_length >= 0)
					comp_length += snprintf(string + comp_length, re_length, "%lu ", fix_len);
				*record_length = comp_length;
			}
			compress_counter++; /* circle number */
			update_offset += fix_len;
			file_size -= src_data_max;
			step_size += fix_len;
		}
		*size = step_size;
		compressed_size = malloc_limit;
		if(compress_data(compressed_buf, &compressed_size, (void*)addr + compress_counter * src_data_max, file_size)){
			return COMP_ERROR;
		}
		if (update_offset + fix_len > minidump_data_end_g) {
			return NO_SPACE_ERROR;
		}
		if (sysdumpdb_write(name, &compressed_size, compressed_size, update_offset, compressed_buf)) {
			dump_loge(" write %s error.\n", SYSDUMPDB_PARTITION_NAME);
			return WRITE_ERROR;
		}
		if(string) {
			re_length = COMPRESSED_LEN_MALLOC_MAX - comp_length;
			dump_logd("re_length = 0x%x \n", re_length);
			if (re_length >= 0)
				comp_length += snprintf(string + comp_length, re_length, "%lu ", compressed_size);
			*record_length = comp_length;
			dump_logd("comp_length = 0x%x \n", comp_length);
		}
		*size = step_size + compressed_size;
	} else {
		if (sysdumpdb_write(name, &file_size, file_size, offset, addr)) {
			dump_loge(" write %s error.\n", SYSDUMPDB_PARTITION_NAME);
		}
	}
	return NO_ERROR;

}
void update_save_flag(enum area_type area, int flag)
{
	g_area_data_info[area].data_flag = flag;
}

/* common func */
int area_init(enum area_type a_type)
{
	struct mini_area_info area_info;
	int area_index = g_data_header.area_num;

	memset(&area_info, 0, sizeof(struct mini_area_info));
	memcpy(area_info.a_name, area_name_g[a_type], strlen(area_name_g[a_type]));
	area_info.data_flag = 1;
	area_info.area_type = a_type;
	area_info.a_size = sizeof(struct mini_area_header);
	area_info.a_offset = get_area_offset(area_index);

	memcpy(&g_area_data_info[area_index], &area_info, sizeof(struct mini_area_info));

	g_data_header.area_num ++;

	return 0;
}
int show_area_info(int area, uint32_t section_num, struct mini_section_info *section_info)
{
	int i;

	dump_logd("area_num is:      		%d\n", g_data_header.area_num);
	dump_logd("arch_info is:      		%d\n", g_data_header.arch_info);
	dump_logd("data header size is:      	0x%x\n", g_data_header.h_size);
	dump_logd("area_info struct size is:    0x%x\n", g_data_header.ainfo_size);

	dump_logd("area name is: 	%s\n", g_area_data_info[area].a_name);
	dump_logd("data_flag is: 	%d\n", g_area_data_info[area].data_flag);
	dump_logd("area type is: 	%d\n", g_area_data_info[area].area_type);
	dump_logd("area size is: 	0x%x\n", g_area_data_info[area].a_size);
	dump_logd("area offset is: 	0x%x\n", g_area_data_info[area].a_offset);
	for(i = 0; i < section_num; i++) {
		dump_logd("section %d:\n", i);
		dump_logd("name is	%s\n", section_info[i].s_name);
		dump_logd("size is      0x%x\n", section_info[i].s_size);
		dump_logd("paddr is	0x%llx\n", section_info[i].s_paddr);
		dump_logd("vaddr is	0x%llx\n", section_info[i].s_vaddr);
		dump_logd("offset is	0x%x\n", section_info[i].s_offset);
	}
	return 0;
}
int update_area_data(int area,
		uint32_t section_num,
		struct mini_section_info *section_info)
{
	int i;
	uint32_t offset;
	uint32_t size;
	uint32_t tmp_size = 0;

	offset = section_num * sizeof(struct mini_section_info) +
		sizeof(struct mini_area_header) +
		g_area_data_info[area].a_offset;

	size = sizeof(struct mini_area_header) + section_num * sizeof(struct mini_section_info);

	for(i = 0; i < section_num; i++) {
		section_info[i].s_offset = offset + tmp_size;
		tmp_size += section_info[i].s_size;

		size += section_info[i].s_size;
	}

	g_area_data_info[area].a_size = size;

	return 0;

}

int save_section_data(char* compress_buf,
		char* compress_record_buf,
		struct mini_area_header *area_header,
		struct mini_section_info *section_info)
{
	int i;
	uint32_t offset;
	char *comp_buf;
	char *comp_record_buf;
	int record_length = 0;
	int ret = 0;
	uint32_t size;
	uint64_t addr;
	int re_length;

	comp_buf = compress_buf;
	comp_record_buf = compress_record_buf;
	if (comp_buf) {
		dump_logd("save section with compressed\n");
		dump_logd("section_start_index: %d, secion num: %d\n", area_header->section_start_index, area_header->section_num);
		for (i = area_header->section_start_index; i < area_header->section_num; i++) {
			size = section_info[i].s_size;
			offset = section_info[i].s_offset;
			addr = section_info[i].s_paddr;
			dump_logd("name: %s,size:0x%x, offset:0x%x, addr:0x%llx, before compress\n", section_info[i].s_name, size, offset, addr);
			if (compress_record_buf) {

				record_length += snprintf(compress_record_buf + record_length, COMPRESSED_LEN_MALLOC_MAX,
						"%s:", section_info[i].s_name);
				ret = minidump_data_write(comp_buf, comp_record_buf, &record_length,
						&size, addr, offset, SYSDUMPDB_PARTITION_NAME);
				re_length = COMPRESSED_LEN_MALLOC_MAX - record_length;
				if (re_length >= 0)
					record_length += snprintf(compress_record_buf + record_length, re_length, "-1 \n");
			} else {
				ret = minidump_data_write(comp_buf, NULL, &record_length, &size, addr, offset, SYSDUMPDB_PARTITION_NAME);
			}
			/* update the section size after compressed */
			dump_logd("size:0x%x, after compress\n", size);
			section_info[i].s_size = size;
			/* update offset of the next section in sysdumpdb */
			if (i < area_header->section_num - 1) {
				section_info[i + 1].s_offset = offset + size;
				dump_logd("name:%s, offset:0x%x\n", section_info[i + 1].s_name, section_info[i + 1].s_offset);
			}
			if (ret < 0) {
				area_header->section_num = i + 1;
				break;
			}

		}
		if (compress_record_buf) {
			/* save compress record buf last */
			size = section_info[0].s_size;
			offset = section_info[0].s_offset;
			addr = section_info[0].s_paddr;
			sysdumpdb_write(SYSDUMPDB_PARTITION_NAME, &size, size, offset, addr);
		}
	} else {
		dump_logd("save section no compress\n");
		for (i = area_header->section_start_index; i < area_header->section_num; i++) {
			size = section_info[i].s_size;
			offset = section_info[i].s_offset;
			addr = section_info[i].s_paddr;
			dump_logd("addr:0x%llx, size:0x%x, offset:0x%x\n", addr, size, offset);
			ret = sysdumpdb_write(SYSDUMPDB_PARTITION_NAME, &size, size, offset, addr);
			if (ret == NO_SPACE_ERROR) {
				/* update the section size */
				section_info[i].s_size = size;
				area_header->section_num = i + 1;

				return NO_SPACE_ERROR;
			}
		}

	}

	return ret;

}
/* common func */
int save_area_data(int area,
		struct mini_area_header *area_header,
		struct mini_section_info *section_info)
{
	int i;
	uint32_t size, offset;
	uint64_t addr;
	uint8_t comp_type;
	uint32_t section_num;
	int ret;
	int malloc_limit = MALLOC_LIMIT;
	void *compress_buf = NULL;
	void *compress_record_buf = NULL;

	comp_type = area_header->comp_type;

	dump_logd("comp_type = %d\n", comp_type);

	/* get section first */
	section_num = area_header->section_num;
	/* save section data with compressed if needed */
	switch(comp_type) {
	case COMPRESS_GZIP:
		dump_logd("compress gzip\n");
		compress_buf = (void*)malloc(malloc_limit);
		if(compress_buf == NULL) {
			dump_loge("malloc compressed buffer failed, save no compress!\n");
			ret = save_section_data(NULL, NULL, area_header, section_info);
		} else {
			memcpy(area_header->comp_suffix, compress_suffix_g[comp_type], strlen(compress_suffix_g[comp_type]));
			ret = save_section_data(compress_buf, NULL, area_header, section_info);
			free(compress_buf);
		}
		break;
	case COMPRESS_ZLIB:
		dump_logd("compress zlib\n");
		compress_buf = (void*)malloc(malloc_limit);
		if (!compress_buf) {
			dump_loge("malloc compress buf faild!\n");
			return -1;
		}
		compress_record_buf = (void*)malloc(COMPRESSED_LEN_MALLOC_MAX);
		if (!compress_record_buf) {
			dump_loge("malloc compress record buf faild!\n");
			free(compress_buf);
			return -1;
		}
		memset(compress_record_buf, 0, COMPRESSED_LEN_MALLOC_MAX);
		/* compress record buf is the first section */
		section_info[0].s_paddr = compress_record_buf;

		area_header->section_start_index = 1;

		memcpy(area_header->comp_suffix, compress_suffix_g[comp_type], strlen(compress_suffix_g[comp_type]));
		ret = save_section_data(compress_buf, compress_record_buf, area_header, section_info);

		free(compress_buf);
		free(compress_record_buf);
		break;
	case NO_COMPRESS:
		dump_logd("no compress\n");
		ret = save_section_data(NULL, NULL, area_header, section_info);
		break;
	default:
		dump_loge("compress type not init, do nothing\n");
		return 0;
	}

	update_area_data(area, section_num, section_info);

	/* save header */
	offset = MINIDUMP_DATA_OFFSET;
	addr = &g_data_header;
	size = sizeof(struct mini_data_header);
	sysdumpdb_write((char *)SYSDUMPDB_PARTITION_NAME, &size, size, offset, addr);

	/* save area info */
	offset = MINIDUMP_DATA_OFFSET + sizeof(struct mini_data_header);
	addr = &g_area_data_info;
	size = sizeof(struct mini_area_info) * AREA_END_NUM;
	sysdumpdb_write((char *)SYSDUMPDB_PARTITION_NAME, &size, size, offset, addr);

	/* save area header */
	offset = g_area_data_info[area].a_offset;
	addr = area_header;
	size = sizeof(struct mini_area_header);
	sysdumpdb_write((char *)SYSDUMPDB_PARTITION_NAME, &size, size, offset, addr);

	/* save section info */
	offset = g_area_data_info[area].a_offset + sizeof(struct mini_area_header);
	addr = section_info;
	size = sizeof(struct mini_section_info) * section_num;
	sysdumpdb_write((char *)SYSDUMPDB_PARTITION_NAME, &size, size, offset, addr);

	dump_logd("save area ok, ret= %d\n", ret);

	return ret;
}
#ifdef CONFIG_ETB_DUMP
void save_soc_dump_to_minidump(void)
{
	struct section_info_extend section_info_extend;
	int ret;

	dump_logd ("Start to dump ETB trace data to minidump\n");
	sprd_etb_hw_dis();
	sprd_etb_dump();
	section_info_extend.paddr = &etb_dump_mem[0];
	section_info_extend.size = etb_buf_size * 4;
	memset(section_info_extend.section_name, 0, SECTION_NAME_LEN_MAX);
	memcpy(section_info_extend.section_name, ETB_DATA, strlen(ETB_DATA));
	dump_logd("section_name: %s \n", section_info_extend.section_name);
	ret = sysdump_save_extend_info(&section_info_extend);
	if (ret)
		dump_loge("save soc_dump to bootloader section failed ! \n");
}
#endif

#ifdef LK_BACKUP_FOR_MINIDUMP_ADR
void save_bootloader_mem_to_minidump(void)
{
        struct section_info_extend section_info_extend;
        int ret;

        dump_logd ("add bootloader mem section to minidump\n");
        section_info_extend.paddr = LK_BACKUP_FOR_MINIDUMP_ADR;
        section_info_extend.size = 0x1000000;
        memset(section_info_extend.section_name, 0, SECTION_NAME_LEN_MAX);
        memcpy(section_info_extend.section_name, BOOTLOADER_MEM, strlen(BOOTLOADER_MEM));
        dump_logd("section_name: %s \n", section_info_extend.section_name);
        ret = sysdump_save_extend_info(&section_info_extend);
        if (ret)
                dump_loge("save bootloader mem to bootloader section failed ! \n");
}
#endif

void save_bootloader_last_log_to_minidump(void)
{
	struct section_info_extend section_info_extend;
        int ret;

        dump_logd ("Start to dump bootloader last log to minidump\n");
        section_info_extend.paddr = get_bootloader_log_addr();
        section_info_extend.size = get_bootloader_log_len();
        memset(section_info_extend.section_name, 0, SECTION_NAME_LEN_MAX);
        memcpy(section_info_extend.section_name, BOOTLOADER_LAST_LOG, strlen(BOOTLOADER_LAST_LOG));
        dump_logd("section_name: %s \n", section_info_extend.section_name);
        ret = sysdump_save_extend_info(&section_info_extend);
        if (ret)
                dump_loge("save bootloader last log to bootloader section failed ! \n");

}
#ifdef AON_IRAM_ADDR
void save_aon_iram_to_minidump(void)
{
	struct section_info_extend section_info_extend;
	int ret;
	dump_logd ("Start to add aon_iram to minidump\n");
	section_info_extend.paddr = AON_IRAM_ADDR;
	section_info_extend.size = AON_IRAM_SIZE;
	memset(section_info_extend.section_name, 0, SECTION_NAME_LEN_MAX);
	memcpy(section_info_extend.section_name, AON_IRAM, strlen(AON_IRAM));
	dump_logd("section_name: %s \n", section_info_extend.section_name);
	ret = sysdump_save_extend_info(&section_info_extend);
	if (ret)
		dump_loge("add aon_iram to bootloader section failed ! \n");
}
#endif
int prepare_bootloader_data(struct mini_area_header *area_header,
                struct mini_section_info *section_info)
{
	struct mini_area_header *bootloader_area_header;
        int index = 0;
	int i,j;

        bootloader_area_header = area_header;

        /* if need compress, locate the compress buffer in the first section */
        if (bootloader_area_header->comp_type == COMPRESS_ZLIB) {
                memcpy(section_info[0].s_name, COMPRESS_RECORD_NAME, strlen(COMPRESS_RECORD_NAME));
                section_info[0].s_paddr = 0;
                section_info[0].s_vaddr = 0;
                section_info[0].s_size = COMPRESSED_LEN_MALLOC_MAX;

                bootloader_area_header->section_num ++;
                index ++;
        }
	for (i=0;i<extend_info_bootloader.total_num;i++){
		if (!is_paddr_linux_memory(extend_info_bootloader.section_info[i].paddr)
				&& (extend_info_bootloader.section_info[i].type != PADDR_OTHER)){
			dump_loge("the addr of the section is NULL!\n");
			continue;
		}
		memcpy(section_info[index].s_name, extend_info_bootloader.section_info[i].section_name,
				strlen(extend_info_bootloader.section_info[i].section_name));
        	section_info[index].s_paddr = extend_info_bootloader.section_info[i].paddr;
		section_info[index].s_vaddr = 0;
		section_info[index].s_size = extend_info_bootloader.section_info[i].size;

		bootloader_area_header->section_num ++;
		index ++;
	}
	return 0;
}
int handle_bootloader_data(void)
{
	struct mini_area_header sprd_bootloader_header;
        struct mini_section_info *sprd_bootloader_info;
        int ret;
        uint32_t section_num;
        int area_index = 0;

	dump_loga("handle bootloader data start!\n");
#ifdef CONFIG_ETB_DUMP
	/* save soc dump data */
	save_soc_dump_to_minidump();
#endif
	/* save bootloader_last_log to minidump */
	save_bootloader_last_log_to_minidump();
#ifdef AON_IRAM_ADDR
	/* add aon_iram to minidump */
	save_aon_iram_to_minidump();
#endif
	/* area init */
        area_init(AREA_BOOTLOADER);

        /* bootloader area header init */
        memset(&sprd_bootloader_header, 0, sizeof(struct mini_area_header));
        sprd_bootloader_header.comp_type = COMPRESS_ZLIB;
        sprd_bootloader_header.ah_size = sizeof(struct mini_area_header);
        sprd_bootloader_header.sinfo_size = sizeof(struct mini_section_info);


        sprd_bootloader_info = malloc(sizeof(struct mini_section_info) * SECTION_NUM_MAX);
        if(sprd_bootloader_info == NULL) {
                dump_loge("malloc error\n");
                return -1;
        }

        memset(sprd_bootloader_info, 0, sizeof(struct mini_section_info) * SECTION_NUM_MAX);

        /* bootloader data need be saved */
        prepare_bootloader_data(&sprd_bootloader_header, sprd_bootloader_info);

//	add_minidump_section_to_fulldump(&sprd_bootloader_header, sprd_bootloader_info);

        /* update section info and area info */
        section_num = sprd_bootloader_header.section_num;
        if (g_data_header.area_num > 0)
                area_index = g_data_header.area_num -1;

        update_area_data(area_index, section_num, sprd_bootloader_info);

        show_area_info(area_index, section_num, sprd_bootloader_info);
        /* save all area data */
        ret = save_area_data(area_index, &sprd_bootloader_header, sprd_bootloader_info);

        update_save_flag(area_index, 1);

        free(sprd_bootloader_info);
        dump_loga("handle bootloader data end!\n");
	return 0;

}
void get_kernel_info_paddr(void)
{
	unsigned long sprd_sysdump_magic = SPRD_SYSDUMP_MAGIC;
	memset(&info_g, 0, sizeof(struct sysdump_info));
	memcpy(&info_g, (struct sysdump_info *)sprd_sysdump_magic, sizeof(struct sysdump_info));

        /* check header magic */
        if(memcmp(MINIDUMP_MAGIC, info_g.sprd_minidump_info.magic, strlen(MINIDUMP_MAGIC))){
                dump_loge("no minidump magic. do nothing\n");
                return;
        } else {
		kernel_infop = (struct kernel_info *)(info_g.sprd_minidump_info.minidump_info_paddr);
                dump_logd("%s: kernel_info paddr : 0x%llx\n", __FUNCTION__, info_g.sprd_minidump_info.minidump_info_paddr);
                dump_logd("%s: kernel_info size : 0x%x\n", __FUNCTION__, info_g.sprd_minidump_info.minidump_info_size);
        }

}
void show_kernel_info(void)
{
        int i;

        dump_logd("kernel_magic: %s  \n", kernel_infop->kernel_magic);
#if 0
        dump_logd("---     regs_info       ---  \n");
        dump_logd("arch:              %d \n", kernel_infop->regs_info.arch);
        dump_logd("num:               %d \n", kernel_infop->regs_info.num);
        dump_logd("paddr:         0x%lx \n", kernel_infop->regs_info.paddr);
        dump_logd("size:          %d \n", kernel_infop->regs_info.size);

        dump_logd("---     regs_memory_info        ---  \n");
        for(i=0;i<kernel_infop->regs_info.num;i++){
                dump_logd("reg[%d] paddr:          0x%lx \n", i, kernel_infop->regs_memory_info.reg_paddr[i]);
        }
        dump_logd("per_reg_memory_size:    %d \n", kernel_infop->regs_memory_info.per_reg_memory_size);
        dump_logd("valid_reg_num:          %d \n", kernel_infop->regs_memory_info.valid_reg_num);
        dump_logd("reg_memory_all_size:    %d \n", kernel_infop->regs_memory_info.size);

        dump_logd("---     section_info_total        ---  \n");
        dump_logd("Here are %d sections, Total size : %d \n", kernel_infop->section_info_total.total_num, kernel_infop->section_info_total.total_size);
        dump_logd("total_num:        %d \n", kernel_infop->section_info_total.total_num);
        dump_logd("total_size        %d \n", kernel_infop->section_info_total.total_size);
	for(i=0;i<kernel_infop->section_info_total.total_num;i++){
                dump_logd("section_name:           %s \n", kernel_infop->section_info_total.section_info[i].section_name);
                dump_logd("section_start_vaddr:    0x%lx \n", kernel_infop->section_info_total.section_info[i].section_start_vaddr);
                dump_logd("section_end_vaddr:      0x%lx \n", kernel_infop->section_info_total.section_info[i].section_end_vaddr);
                dump_logd("section_start_paddr:    0x%lx \n", kernel_infop->section_info_total.section_info[i].section_start_paddr);
                dump_logd("section_end_paddr:      0x%lx \n", kernel_infop->section_info_total.section_info[i].section_end_paddr);
                dump_logd("section_size:           0x%x \n", kernel_infop->section_info_total.section_info[i].section_size);
        }

        dump_logd("minidump_data_size:     0x%x \n", kernel_infop->minidump_data_size);
#endif
        dump_logd("-------------------------          exception_info  ----------------------------- \n");
        dump_logd(" struct minidump_info size : 0x%x \n", sizeof(struct kernel_info));
        dump_logd("kernel_magic:             %s  \n", kernel_infop->exception_info.kernel_magic);
        dump_logd("exception_serialno:  %s  \n", kernel_infop->exception_info.exception_serialno);
        dump_logd("exception_kernel_version: %s  \n", kernel_infop->exception_info.exception_kernel_version);
        dump_logd("exception_reboot_reason:  %s  \n", kernel_infop->exception_info.exception_reboot_reason);
        dump_logd("exception_panic_reason:   %s  \n", kernel_infop->exception_info.exception_panic_reason);
        dump_logd("exception_time:           %s  \n", kernel_infop->exception_info.exception_time);
        dump_logd("exception_file_info:      %s  \n", kernel_infop->exception_info.exception_file_info);
        dump_logd("exception_task_id:        %d  \n", kernel_infop->exception_info.exception_task_id);
        dump_logd("exception_task_family:      %s  \n", kernel_infop->exception_info.exception_task_family);
        dump_logd("exception_pc_symbol:      %s  \n", kernel_infop->exception_info.exception_pc_symbol);
        dump_logd("exception_stack_info:      %s  \n", kernel_infop->exception_info.exception_stack_info);

}
int update_except_note_info(void)
{
	if (txt_info == NULL){
		dump_loge("txt_info is NULL\n");
		return 0;
	}
#if SPRD_SYSDUMP_MAGIC != RAMDISK_ADR
#ifdef CONFIG_ARM64
        /* record kernel vmcore info to txt file when sysdump-mem is reserved to (0x80001000) */
        s_length += snprintf(txt_info + s_length, DUMPINFO_FILE_SIZE - s_length,"\n%s\n-%s%llx\n-%s%llx\n-%s%llx\n-%s%lld\n\n",
                        "Vmcoreinfo:",
                        "kimage_voffset = 0x",
                        info_g.sprd_vmcoreinfo.kimage_voffset,
                        "phys_offset = 0x",
                        info_g.sprd_vmcoreinfo.phys_offset,
                        "kaslr_offset = 0x",
                        info_g.sprd_vmcoreinfo.kaslr_offset,
                        "vabits_actual = ",
                        info_g.sprd_vmcoreinfo.vabits_actual);
#endif
#endif
	s_length += snprintf(txt_info + s_length, DUMPINFO_FILE_SIZE - s_length, "-%s%s\n-%s%s\n-%s%s\n-%s%d\n-%s%s\n-%s%s\n-%s%s\n",
			"exception_kernel_version:",
			kernel_infop->exception_info.exception_kernel_version,
                        "exception_panic_reason:",
                        kernel_infop->exception_info.exception_panic_reason,
                        "exception_file_infoi:",
                        kernel_infop->exception_info.exception_file_info,
			"exception_task_id:",
			kernel_infop->exception_info.exception_task_id,
			"exception_task_family:",
			kernel_infop->exception_info.exception_task_family,
			"exception_pc_symbol:",
			kernel_infop->exception_info.exception_pc_symbol,
			"exception_stack_info:",
			kernel_infop->exception_info.exception_stack_info);

	return 0;
}

int prepare_kernel_data(struct mini_area_header *area_header,
                struct mini_section_info *section_info)
{
	struct mini_area_header *kernel_area_header;
        int index = 0;
        int i,j,size;
	int zlib_flag = 0;
	int kernel_mem_num = 0;
	unsigned long addr,vaddr;

	uint64_t ddr_end = CONFIG_SYS_SDRAM_BASE + get_real_ram_size();
        kernel_area_header = area_header;

        /* if need compress, locate the compress buffer in the first section */
        if (kernel_area_header->comp_type == COMPRESS_ZLIB) {
		zlib_flag = 1;
                memcpy(section_info[0].s_name, COMPRESS_RECORD_NAME, strlen(COMPRESS_RECORD_NAME));
                section_info[0].s_paddr = 0;
                section_info[0].s_vaddr = 0;
                section_info[0].s_size = COMPRESSED_LEN_MALLOC_MAX;
                kernel_area_header->section_num ++;
                index ++;
		kernel_mem = (void*)malloc(sizeof(struct sysdump_mem) * KERNEL_MEM_MAX);
	        if (kernel_mem == NULL) {
                	dump_logd("%s: %s malloc compress buf error!\n", SYSDUMPDB_LOG_TAG, __FUNCTION__);
                	return -1;
        	}
        }
	/* save cpu_regs to minidump section */
	if(!is_paddr_linux_memory(kernel_infop->regs_info.paddr)) {
		dump_loge("invalid reg paddr !\n");
	} else {
		snprintf(section_info[index].s_name, SECTION_NAME_LEN_MAX, "%03d_minidump_%s", index, "regs");
		section_info[index].s_paddr = kernel_infop->regs_info.paddr;
                section_info[index].s_vaddr = kernel_infop->regs_info.vaddr;
                section_info[index].s_size = kernel_infop->regs_info.size;
		kernel_area_header->section_num ++;
		index ++;
		if (zlib_flag){
			kernel_mem[kernel_mem_num].paddr = kernel_infop->regs_info.paddr;
			kernel_mem[kernel_mem_num].vaddr = kernel_infop->regs_info.vaddr;
			kernel_mem[kernel_mem_num].size = kernel_infop->regs_info.size;
			kernel_mem_num ++;
		}
	}
	/* save regs_memory to minidump */
	for (i=0;i<kernel_infop->regs_memory_info.valid_reg_num;i++){
		size = kernel_infop->regs_memory_info.per_reg_memory_size;
                addr = kernel_infop->regs_memory_info.reg_paddr[i];
                vaddr = kernel_infop->regs_memory_info.reg_vaddr[i];
		if(!is_paddr_linux_memory(addr) ) {
                        dump_logd("%s: invalid addr skip it  . \n", __FUNCTION__);
                        continue;
                }
		snprintf(section_info[index].s_name, SECTION_NAME_LEN_MAX, "%03d_minidump_%s_%d", index, "regs_memory", i);
		section_info[index].s_paddr = addr;
                section_info[index].s_vaddr = vaddr;
                section_info[index].s_size = size;
		kernel_area_header->section_num ++;
                index ++;
		if (zlib_flag){
                        kernel_mem[kernel_mem_num].paddr = addr;
                        kernel_mem[kernel_mem_num].vaddr = vaddr;
                        kernel_mem[kernel_mem_num].size = size;
                        kernel_mem_num ++;
                }
		dump_logd("save reg_memory OK!\n");
	}
	/* save sections to minidump */
	for(i=0;i<kernel_infop->section_info_total.total_num;i++){
		size = kernel_infop->section_info_total.section_info[i].section_size;
                addr = kernel_infop->section_info_total.section_info[i].section_start_paddr;
                vaddr = kernel_infop->section_info_total.section_info[i].section_start_vaddr;
		if((addr == 0) || (addr >= ddr_end)){
			dump_logd("%s: invalid section addr skip it  . \n", __FUNCTION__);
			continue;
		}
		snprintf(section_info[index].s_name, SECTION_NAME_LEN_MAX, "%03d_minidump_%s", index,
					kernel_infop->section_info_total.section_info[i].section_name);

		section_info[index].s_paddr = addr;
                section_info[index].s_vaddr = vaddr;
                section_info[index].s_size = size;
		kernel_area_header->section_num ++;
                index ++;
                if (zlib_flag){
                        kernel_mem[kernel_mem_num].paddr = addr;
                        kernel_mem[kernel_mem_num].vaddr = vaddr;
                        kernel_mem[kernel_mem_num].size = size;
                        kernel_mem_num ++;
                }
		dump_logd("save kernel sections OK!\n");
	}
	/* save kernel_mem to minidump */
	if (zlib_flag){
		addr = kernel_mem;
		size = sizeof(struct sysdump_mem) * KERNEL_MEM_MAX;
		snprintf(section_info[index].s_name, SECTION_NAME_LEN_MAX, "%03d_minidump_%s", index, "kernel_mem_info");
		section_info[index].s_paddr = addr;
                section_info[index].s_vaddr = 0;
                section_info[index].s_size = size;
		kernel_area_header->section_num ++;
	}
	return 0;
}

void update_except_kernel_info(void)
{
        struct rtc_time tm;
        memcpy(kernel_infop->exception_info.exception_reboot_reason, GET_RST_MODE(reboot_mode), strlen(GET_RST_MODE(reboot_mode)));
        tm = get_time_by_sec();
        snprintf(kernel_infop->exception_info.exception_time, 31, "%04d-%02d-%02d_%02d-%02d-%02d",
                        tm.tm_year, tm.tm_mon, tm.tm_mday, \
                        tm.tm_hour,tm.tm_min, tm.tm_sec);
//      memcpy(minidump_infop->exception_info.arch_info, BOARD_ARCH, strlen(BOARD_ARCH));
//      sprintf(minidump_infop->exception_info.exception_serialno, "ro.boot.serialno=%s", get_product_sn());
        if(reboot_mode == CMD_UNKNOW_REBOOT_MODE) {
                memcpy(kernel_infop->exception_info.exception_panic_reason, GET_RST_MODE(reboot_mode), strlen(GET_RST_MODE(reboot_mode)));
                memcpy(kernel_infop->exception_info.exception_kernel_version, GET_RST_MODE(reboot_mode), strlen(GET_RST_MODE(reboot_mode)));
                memcpy(kernel_infop->exception_info.exception_file_info, GET_RST_MODE(reboot_mode), strlen(GET_RST_MODE(reboot_mode)));
                memcpy(kernel_infop->exception_info.exception_task_family, GET_RST_MODE(reboot_mode), strlen(GET_RST_MODE(reboot_mode)));
                memcpy(kernel_infop->exception_info.exception_pc_symbol, GET_RST_MODE(reboot_mode), strlen(GET_RST_MODE(reboot_mode)));
                memcpy(kernel_infop->exception_info.exception_stack_info, GET_RST_MODE(reboot_mode), strlen(GET_RST_MODE(reboot_mode)));

        }
}

int handle_kernel_data(void)
{
	struct mini_area_header sprd_kernel_header;
        struct mini_section_info *sprd_kernel_info;
        int ret;
        uint32_t section_num;
        int area_index = 0;

	dump_loga("handle kernel data start!\n");

	get_kernel_info_paddr();

        if(!is_paddr_linux_memory(kernel_infop)){
                dump_loge("kernel info address is invalid!!, do nothing\n");
                return -1;
        }
        /*      check kernel info data kernel  magic ,make sure it is valid   */
        if(memcmp(KERNEL_MAGIC, kernel_infop->kernel_magic, strlen(KERNEL_MAGIC))){
                /*      no magic means ddr data is corruption  ,do nothing      */
                dump_loge("%s: no  kerenl magic ,minidump indo is no valid .do nothing \n",SYSDUMPDB_LOG_TAG);
                return -1;
        }
	update_except_kernel_info();
	show_kernel_info();
	update_except_note_info();
        /* area init */
        area_init(AREA_KERNEL);

        /* kernel area header init */
        memset(&sprd_kernel_header, 0, sizeof(struct mini_area_header));
        sprd_kernel_header.comp_type = COMPRESS_ZLIB;
        sprd_kernel_header.ah_size = sizeof(struct mini_area_header);
        sprd_kernel_header.sinfo_size = sizeof(struct mini_section_info);


        sprd_kernel_info = malloc(sizeof(struct mini_section_info) * KERNEL_MEM_MAX);
        if(sprd_kernel_info == NULL) {
                dump_loge("malloc error\n");
                return -1;
        }

        memset(sprd_kernel_info, 0, sizeof(struct mini_section_info) * KERNEL_MEM_MAX);

#ifdef CONFIG_ARM7_RAM_ACTIVE
	pmic_arm7_RAM_active();
#endif
        /* kernel data need be saved */
        prepare_kernel_data(&sprd_kernel_header, sprd_kernel_info);
//	add_minidump_section_to_fulldump(&sprd_kernel_header, sprd_kernel_info);

        /* update section info and area info */
        section_num = sprd_kernel_header.section_num;
        if (g_data_header.area_num > 0)
                area_index = g_data_header.area_num -1;

        update_area_data(area_index, section_num, sprd_kernel_info);

        show_area_info(area_index, section_num, sprd_kernel_info);
        /* save all area data */
        ret = save_area_data(area_index, &sprd_kernel_header, sprd_kernel_info);

        update_save_flag(area_index, 1);

        free(sprd_kernel_info);
	if (kernel_mem)
		free(kernel_mem);
        dump_loga("handle kernel data end!\n");

	return 0;
}

int save_modem_mem_to_minidump(struct mini_area_header *area_header,
		struct mini_section_info *section_info, int index)
{
#ifdef CONFIG_ARM7_RAM_ACTIVE
        pmic_arm7_RAM_active();
#endif

#ifdef SP_IRAM_ADDR
	dump_logd("save_modem_mem_to_minidump start!\n");
	memcpy(section_info[index].s_name, CM4_MEM, strlen(CM4_MEM));
	section_info[index].s_paddr = SP_IRAM_ADDR;
	section_info[index].s_vaddr = 0;
	section_info[index].s_size = SP_IRAM_SIZE;
	area_header->section_num ++;
        dump_logd("save_modem_mem_to_minidump end!\n");
	return 0;
#else
	dump_loga("SP_IRAM_ADDR not defined, do nothing!\n");
	return -1;
#endif

}

int prepare_modem_data(struct mini_area_header *area_header,
                struct mini_section_info *section_info)
{
	struct mini_area_header *modem_area_header;
        int index = 0;
	uint64_t paddr = 0;
	uint32_t size = 0;
	int ret_modem;

        modem_area_header = area_header;

        /* if need compress, locate the compress buffer in the first section */
        if (modem_area_header->comp_type == COMPRESS_ZLIB) {
                memcpy(section_info[0].s_name, COMPRESS_RECORD_NAME, strlen(COMPRESS_RECORD_NAME));
                section_info[0].s_paddr = 0;
                section_info[0].s_vaddr = 0;
                section_info[0].s_size = COMPRESSED_LEN_MALLOC_MAX;

                modem_area_header->section_num ++;
                index ++;
        }

	/* add modem mem to minidump section */
	ret_modem = save_modem_mem_to_minidump(modem_area_header,section_info,index);
	if (ret_modem){
		dump_loge("save modem_mem to minidump failed!\n");
	} else {
		dump_logd("save modem_mem to minidump success!\n");
		index ++;
	}
	return ret_modem;
}

int handle_modem_data(void)
{
	struct mini_area_header sprd_modem_header;
        struct mini_section_info *sprd_modem_info;
        int ret;
        uint32_t section_num;
        int area_index = 0;

	dump_loga("handle cm4 iram data start!\n");
	/* area init */
	area_init(AREA_MODEM);

	/* bootloader area header init */
	memset(&sprd_modem_header, 0, sizeof(struct mini_area_header));
	sprd_modem_header.comp_type = COMPRESS_ZLIB;
	sprd_modem_header.ah_size = sizeof(struct mini_area_header);
	sprd_modem_header.sinfo_size = sizeof(struct mini_section_info);

	sprd_modem_info = malloc(sizeof(struct mini_section_info) * SECTION_NUM_MAX);
	if(sprd_modem_info == NULL) {
		dump_loge("malloc error\n");
		return -1;
	}

	memset(sprd_modem_info, 0, sizeof(struct mini_section_info) * SECTION_NUM_MAX);

	/* bootloader data need be saved */
        prepare_modem_data(&sprd_modem_header, sprd_modem_info);
//	add_minidump_section_to_fulldump(&sprd_modem_header, sprd_modem_info);
        /* update section info and area info */
        section_num = sprd_modem_header.section_num;
        if (g_data_header.area_num > 0)
                area_index = g_data_header.area_num -1;

        update_area_data(area_index, section_num, sprd_modem_info);

        show_area_info(area_index, section_num, sprd_modem_info);
        /* save all area data */
        ret = save_area_data(area_index, &sprd_modem_header, sprd_modem_info);

        update_save_flag(area_index, 1);

        free(sprd_modem_info);
	dump_loga("handle modem_ data end!\n");
	return 0;
}
#ifdef CONFIG_TEECFG
int save_tos_mem_to_minidump(struct mini_area_header *area_header,
		struct mini_section_info *section_info, int index)
{
	uint64_t paddr = 0;
	uint32_t size = 0;
	int ret;
	ret = teecfg_get_tos_coredump_addr(&paddr);
	if (ret){
		dump_loge("get the addr of tos mem failed. \n");
		return ret;
	}
	ret = teecfg_get_tos_coredump_size(&size);
	if (ret){
                dump_loge("get the size of tos mem failed. \n");
                return ret;
        }
	memcpy(section_info[index].s_name, TOS_MEM, strlen(TOS_MEM));
	section_info[index].s_paddr = paddr;
	section_info[index].s_vaddr = 0;
	section_info[index].s_size = size;
	area_header->section_num ++;

	return 0;
}
int save_sml_mem_to_minidump(struct mini_area_header *area_header,
		struct mini_section_info *section_info, int index)
{
        uint64_t paddr = 0;
        uint32_t size = 0;
        int ret;
        ret = teecfg_get_sml_coredump_addr(&paddr);
        if (ret){
                dump_loge("get the addr of tos mem failed. \n");
                return ret;
        }
        ret = teecfg_get_sml_coredump_size(&size);
        if (ret){
                dump_loge("get the size of tos mem failed. \n");
                return ret;
        }
        memcpy(section_info[index].s_name, SML_MEM, strlen(SML_MEM));
        section_info[index].s_paddr = paddr;
        section_info[index].s_vaddr = 0;
        section_info[index].s_size = size;
        area_header->section_num ++;

        return 0;
}
int prepare_tos_data(struct mini_area_header *area_header,
                struct mini_section_info *section_info)
{
	struct mini_area_header *tos_area_header;
        int index = 0;
	uint64_t paddr = 0;
	uint32_t size = 0;
	int ret_tos,ret_sml;

        tos_area_header = area_header;

        /* if need compress, locate the compress buffer in the first section */
        if (tos_area_header->comp_type == COMPRESS_ZLIB) {
                memcpy(section_info[0].s_name, COMPRESS_RECORD_NAME, strlen(COMPRESS_RECORD_NAME));
                section_info[0].s_paddr = 0;
                section_info[0].s_vaddr = 0;
                section_info[0].s_size = COMPRESSED_LEN_MALLOC_MAX;

                tos_area_header->section_num ++;
                index ++;
        }

	/* add tos mem to minidump section */
	if (reboot_mode == CMD_TOS_PANIC_MODE){
		ret_tos = save_tos_mem_to_minidump(tos_area_header,section_info,index);
		if (ret_tos){
			dump_loge("save tos_mem to minidump failed!\n");
		} else {
			dump_logd("save tos_mem to minidump success!\n");
			index ++;
		}
	}
	/* add sml mem to minidump section */
	if (reboot_mode == CMD_SML_PANIC_MODE){
		ret_sml = save_sml_mem_to_minidump(tos_area_header,section_info,index);
		if (ret_sml){
			dump_loge("save sml_mem to minidump failed!\n");
		} else {
			dump_logd("save sml_mem to minidump success!\n");
			index ++;
		}
	}
	return 0;
}
#endif
int handle_tos_data(void)
{
	struct mini_area_header sprd_tos_header;
	struct mini_section_info *sprd_tos_info;
	int ret;
	uint32_t section_num;
	int area_index = 0;
#ifdef CONFIG_TEECFG
	if((reboot_mode != CMD_TOS_PANIC_MODE) && (reboot_mode != CMD_SML_PANIC_MODE)){
		dump_loga("reboot_mode = 0x%x,not sml_panic or tos_panic,skip save tos_data\n",reboot_mode);
		return 0;
	}
	dump_loga("handle tos data start!\n");
	/* area init */
	area_init(AREA_TOS);

	/* bootloader area header init */
	memset(&sprd_tos_header, 0, sizeof(struct mini_area_header));
	sprd_tos_header.comp_type = COMPRESS_ZLIB;
	sprd_tos_header.ah_size = sizeof(struct mini_area_header);
	sprd_tos_header.sinfo_size = sizeof(struct mini_section_info);

	sprd_tos_info = malloc(sizeof(struct mini_section_info) * SECTION_NUM_MAX);
	if(sprd_tos_info == NULL) {
		dump_loge("malloc error\n");
		return -1;
	}
	memset(sprd_tos_info, 0, sizeof(struct mini_section_info) * SECTION_NUM_MAX);

	/* tos data need be saved */
	prepare_tos_data(&sprd_tos_header, sprd_tos_info);
//	add_minidump_section_to_fulldump(&sprd_tos_header, sprd_tos_info);
	/* update section info and area info */
	section_num = sprd_tos_header.section_num;
	if (g_data_header.area_num > 0)
		area_index = g_data_header.area_num -1;

	update_area_data(area_index, section_num, sprd_tos_info);

	show_area_info(area_index, section_num, sprd_tos_info);
	/* save all area data */
	ret = save_area_data(area_index, &sprd_tos_header, sprd_tos_info);

	update_save_flag(area_index, 1);

	free(sprd_tos_info);
	dump_loga("handle tos data end!\n");
#endif
	return 0;
}
void prepare_note_data(struct mini_area_header *area_header,
		struct mini_section_info *section_info,
		char *txt_info)
{
	struct mini_area_header *note_area_header;
	int index = 0;
	int note_index = 0;

	note_area_header = area_header;

	/* if need compress, locate the compress buffer in the first section */
	if (note_area_header->comp_type == COMPRESS_ZLIB) {
		memcpy(section_info[0].s_name, COMPRESS_RECORD_NAME, strlen(COMPRESS_RECORD_NAME));
		section_info[0].s_paddr = 0;
		section_info[0].s_vaddr = 0;
		section_info[0].s_size = COMPRESSED_LEN_MALLOC_MAX;

		note_area_header->section_num ++;
		index ++;
	}
	get_time();

	/* note 1 start */

	s_length += snprintf(txt_info + s_length, DUMPINFO_FILE_SIZE - s_length, "-%s%s\n-%s%x\n-%s%x\n-%s%s\n",
			"dump time is ",
			dump_time,
			"reboot reg is ",
			reboot_reg,
			"reset mode is ",
			reboot_mode,
			"reason is ",
			GET_RST_MODE(reboot_mode));
	 s_length += snprintf(txt_info + s_length, DUMPINFO_FILE_SIZE - s_length, "-%s%s\n",
			 "bootcause_cmdline is ",
			 bootcause_cmdline);

	 s_length += snprintf(txt_info + s_length, DUMPINFO_FILE_SIZE - s_length, "\n-%s%s\n",
                         "exception_reboot_reason: ",
			 GET_RST_MODE(reboot_mode));

	memcpy(section_info[index].s_name, note_file[note_index], strlen(note_file[note_index]));

	section_info[index].s_paddr = (uint64_t)txt_info;
	section_info[index].s_vaddr = 0;
	section_info[index].s_size = DUMPINFO_FILE_SIZE;

	note_area_header->section_num ++;
	index ++;
	note_index ++;

	/* note 1 end */
}
int handle_note_data(int rst_mode)
{
	uint32_t section_num;

	dump_logd("handle note data start!\n");
	/* area init */
	area_init(AREA_NOTE);

	/* note area header init */
	memset(&sprd_note_header, 0, sizeof(struct mini_area_header));
	sprd_note_header.comp_type = NO_COMPRESS;
	sprd_note_header.ah_size = sizeof(struct mini_area_header);
	sprd_note_header.sinfo_size = sizeof(struct mini_section_info);


	sprd_note_info = malloc(sizeof(struct mini_section_info) * SECTION_NUM_MAX);
	if(sprd_note_info == NULL) {
		dump_loge("malloc error\n");
		return -1;
	}
	txt_info = malloc(DUMPINFO_FILE_SIZE);
	if(txt_info == NULL) {
		dump_loge("malloc error\n");
		return -1;
	}

	memset(sprd_note_info, 0, sizeof(struct mini_section_info) * SECTION_NUM_MAX);

	/* prepare note data need be saved */
	prepare_note_data(&sprd_note_header, sprd_note_info, txt_info);

	/* update section info and area info */
	section_num = sprd_note_header.section_num;
	if (g_data_header.area_num > 0)
		area_note_index = g_data_header.area_num -1;

	update_area_data(area_note_index, section_num, sprd_note_info);

	show_area_info(area_note_index, section_num, sprd_note_info);
	return 0;
}
int update_note_data(void)
{
	dump_logd("%s -----in\n", __func__);
	int ret;
	/* save all area data */
	ret = save_area_data(area_note_index, &sprd_note_header, sprd_note_info);

	update_save_flag(area_note_index, 1);

	free(sprd_note_info);
	free(txt_info);

	dump_logd("handle note data end!\n");
	return ret;
}

MINIDUMP_FUNC minidump_save_func_list[MINIDUMP_FUNC_MAX] = {
	handle_note_data,
	handle_bootloader_data,
	handle_kernel_data,
	handle_modem_data,
	handle_tos_data,
};

static struct handle_function_node {
	int (*handle_func_p)(void);
	struct handle_function_node *next;
};
static struct handle_function_node head = {
	.handle_func_p = NULL,
	.next = &head,
};

struct handle_function_node *reset_mode_handle_func[MINIDUMP_FUNC_MAX] = {0};

static int minidump_func_list_append(struct handle_function_node *new)
{
	struct handle_function_node *list_head;

	list_head = &head;

	if (list_head->handle_func_p == NULL) {
		list_head->handle_func_p = new->handle_func_p;
		return 0;
	}

	struct handle_function_node *p = list_head;
	while (p->next != list_head) {
		p = p->next;
	}
	p->next = new;
	new->next = list_head;
	return 0;
}
/* This function is used to init the minidump function list */
void minidump_func_list_free(void)
{
	struct handle_function_node *p = NULL;
	struct handle_function_node *cur_head = NULL;

	if (head.next == &head) {
		dump_logd("no need to free.\n");
		return;
	} else {
		dump_logd("free the minidump_func_list .\n");
	}

	p = &head;

	/* free the middle node of the fuc_list */
	while(p->next != &head) {
		cur_head = p;
		p = p->next;
		if (cur_head != &head) {
			free(cur_head);
			cur_head = NULL;
		}
		/* check whether the addr of p is invalid */
		if(!is_paddr_linux_memory((unsigned long)p)) {
			dump_loge("the addr is invalid !!!\n");
			goto error_list_free;
		}
	}

	/* free the last node of the func_list */
	free(p);
	p = NULL;

error_list_free:

	/* init the head of the fuc_list */
	head.next=&head;
	head.handle_func_p = NULL;

	return;
}


int handle_func_list_init(void)
{
	struct handle_function_node *func_node;
	int i;

	for (i = 1; i < MINIDUMP_FUNC_MAX; i ++) {
		func_node = malloc(sizeof(struct handle_function_node));
		if (func_node == NULL) {
			dump_loge("malloc error!!\n");
			return -1;
		}

		memset(func_node, 0, sizeof(struct handle_function_node));
		if (minidump_save_func_list[i] == NULL) {
			free(func_node);
			return 0;
		}
		func_node->handle_func_p = minidump_save_func_list[i];
		minidump_func_list_append(func_node);
		if (i == 1) {
			reset_mode_handle_func[i] = &head;
			free(func_node);
		} else {
			reset_mode_handle_func[i] = func_node;
		}
	}

	return 0;
}
static void update_minidump_magic(void)
{
	int size = sizeof(struct mini_data_header);
	int ret;

	memcpy(g_data_header.data_magic, MINIDUMP_DATA_MAGIC, strlen(MINIDUMP_DATA_MAGIC));
	ret = common_raw_write(SYSDUMPDB_PARTITION_NAME, size, size, MINIDUMP_DATA_OFFSET, &g_data_header);
	if (ret)
		dump_loge("minidump data_magic updated failed!!\n");
	else
		dump_logd("minidump data_magic updated ok!, %s\n", g_data_header.data_magic);
}
int save_minidump(int reset_mode, char *reason)
{
	int ret;

	if (reason != NULL)
		dump_loga("minidump saving by %s\n", reason);

	if (reset_mode < 0 || reset_mode >= CMD_MAX_MODE) {
		reset_mode = 0;
	}

	reboot_mode = reset_mode;

	sysdumpdb_partition_init();
	minidump_data_backup_in_partition();

	ret = handle_func_list_init();
	if (ret) {
		dump_loge("handle func_list init error!\n");
		minidump_func_list_free();
		return -1;
	}

	mini_data_init();

	/* note data will be saved firstly */
	handle_note_data(reset_mode);

	struct handle_function_node *start_node;
	struct handle_function_node *p;

	if ((reset_mode == CMD_TOS_PANIC_MODE) || (reset_mode == CMD_SML_PANIC_MODE))
		p = start_node = reset_mode_handle_func[AREA_TOS];
	else if (reset_mode == CMD_PANIC_REBOOT)
		p = start_node = reset_mode_handle_func[AREA_KERNEL];
	else if (reset_mode == CMD_WATCHDOG_REBOOT || reset_mode == CMD_AP_WATCHDOG_REBOOT)
		p = start_node = reset_mode_handle_func[AREA_MODEM];
	else
		p = start_node = &head;
	if (!is_paddr_linux_memory((unsigned long)p)) {
		dump_loge("The addr of p is invalid !!!\n");
		return -1;
	}
	while (p->next != start_node) {
		if (!is_paddr_linux_memory((unsigned long)p->handle_func_p)) {
			dump_loge("no func to excute, do nothing!\n");
			p = p->next;
			continue;
		}
		p->handle_func_p();
		p = p->next;
	}

	if (is_paddr_linux_memory((unsigned long)p->handle_func_p))
		p->handle_func_p();
	update_note_data();
	update_minidump_magic();

	/* free minidump_handle_list */
	minidump_func_list_free();

	return 0;
}
int is_dump_allow(int rst_mode, int status)
{
	int exc_mode;

	exc_mode = is_sysdump_boot_mode(rst_mode);

	if(status && exc_mode)
		return 1;

	return 0;
}
int init_dump_status(struct dumpdb_header *dump_status_header, int *minidump_status,
					int *fulldump_status, int rst_mode)
{
	if((IS_ORIG_STATUS(dump_status_header->dump_flag)) || (rst_mode == CMD_RECOVERY_MODE)) {
		dump_status_header->dump_flag &= SYSDUMP_STATUS_MASK;
		dump_logd("dump status is orig:  (0x%x)\n", dump_status_header->dump_flag);
#if DEBUG
		dump_status_header->dump_flag |= AP_FULL_DUMP_ENABLE;
		dump_logd("Debug mode , set ap full dump enable default . (0x%x) \n", dump_status_header->dump_flag);
#else
		dump_status_header->dump_flag &= (~AP_FULL_DUMP_ENABLE);
		dump_logd("user mode , set ap full dump disenable default .  (0x%x)\n", dump_status_header->dump_flag);
#endif
		dump_status_header->dump_flag |= AP_MINI_DUMP_ENABLE;
		dump_logd("Always set ap mini dump enable default .  (0x%x) \n", dump_status_header->dump_flag);
                /* record the status of dumpdb_header struct */
		record_dumpdb_header_status(dump_status_header);
                /*      updadte dump flag data */
		if (common_raw_write(SYSDUMPDB_PARTITION_NAME, (uint64_t)(sizeof(struct dumpdb_header)), (uint64_t)(sizeof(struct dumpdb_header)), (uint64_t)0, (char*)dump_status_header)) {
			dump_loge(" update dump flag  %s error.\n", SYSDUMPDB_PARTITION_NAME);
			return -1;
		}
		*minidump_status = !!(dump_status_header->dump_flag & AP_MINI_DUMP_ENABLE);
		*fulldump_status = !!(dump_status_header->dump_flag & AP_FULL_DUMP_ENABLE);
	} else {
		dump_logd("...Status Changed... , read saved status (0x%x)\n", dump_status_header->dump_flag);
		*minidump_status = !!(dump_status_header->dump_flag & AP_MINI_DUMP_ENABLE);
		*fulldump_status = !!(dump_status_header->dump_flag & AP_FULL_DUMP_ENABLE);
	}
	return 0;
}
void handle_minidump_before_boot(int rst_mode)
{
	struct dumpdb_header dump_status_header;
	int minidump_status,fulldump_status;
	int minidump_allow,fulldump_allow;

	if (common_raw_read(SYSDUMPDB_PARTITION_NAME, (uint64_t)(sizeof(struct dumpdb_header)), (uint64_t)0,(char *)&dump_status_header)){
		dump_loge("%s: read header from %s error.do nothing \n", SYSDUMPDB_LOG_TAG, SYSDUMPDB_PARTITION_NAME);
		return;
	}
	if (init_dump_status(&dump_status_header, &minidump_status, &fulldump_status, rst_mode)){
		dump_loge("init_dump_status failed.\n");
		return;
	}
	minidump_allow = is_dump_allow(rst_mode, minidump_status);
	fulldump_allow = is_dump_allow(rst_mode, fulldump_status);

	dump_loga("fulldump_status = %d\n", fulldump_status);
	dump_loga("minidump_status = %d\n", minidump_status);
	dump_loga("fulldump_allow =  %d\n", fulldump_allow);
	dump_loga("minidump_allow =  %d\n", minidump_allow);
	dump_loga("reset_mode:      (%d)-(%s)\n", rst_mode, GET_RST_MODE(rst_mode));

	if (!minidump_allow) {
		dump_loga("no need minidump\n");
		return;
	}
	FTL_Savepoint_Private(PHASE_MINIDUMP_MODE);
	dump_loga("now enter in minidump!\n");
#ifdef LK_BACKUP_FOR_MINIDUMP_ADR
	if (rst_mode == CMD_BOOTLOADER_PANIC_MODE) {
		dump_loga("minidump has beed saved by bootloader panic, not need to save again!\n");
		return;
	}
#endif
	if(!save_minidump(rst_mode, NULL))
		dump_loga("save minidump ok!\n");
	else
		dump_loge("save minidump fail!\n");

	if (!fulldump_allow) {
		dump_loga("reboot device in normal mode!\n");
		write_log_last();
		system_reboot(HWRST_STATUS_SYSDUMP);
	}

	return;
}
