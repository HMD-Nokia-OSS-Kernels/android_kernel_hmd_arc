/*
* Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
* Licensed under the Unisoc General Software License, version 1.0 (the License);
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
* Software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* See the Unisoc General Software License, version 1.0 for more details.
*/

#include <linux/types.h>
#include <config.h>
#include <string.h>
#include <stdio.h>
#include <lk/debug.h>
#include "teecfg_parse.h"


#define TOS_COREDUMP_HEADER_OFFSET      8192
#define TOS_COREDUMP_HEADER_SIZE        4096

static void get_reserved_params(char **addr, unsigned int *num)
{
	if (strcmp(TEECFG_HEADER_MAGIC, (char *)TEECFG_MEM_ADDR) != 0) {
		panic("teecfg magic err!\n");
		//while(1);
	}

	tee_config_header_t *teecfg_header = (tee_config_header_t *)TEECFG_MEM_ADDR;
	unsigned int off = teecfg_header->res_seg_off;
	*addr = (char *)(TEECFG_MEM_ADDR + off);
	*num = teecfg_header->res_seg_num;
}

/*
 * the struct of reserved segment is organized as follow:
 * (tag|len|val)|(tag|len|val)|......(tag|len|val)
 * tag:a string whose length does NOT execced RESERVED_TAG_MAX_SIZE chars (including '\0')
 * len:unsigned char type, describes the length of value
 * val:the reserved data
 */
int teecfg_get_reserved_val(const char *id, void *val)
{
	char *reserved_str_addr;
	unsigned int reserved_str_num;
	unsigned int tag_len, val_len;
	char tag[RESERVED_TAG_MAX_SIZE];
	int i;

	/* get the address and number of reserved segment */
	get_reserved_params(&reserved_str_addr, &reserved_str_num);

	for (i = 0; i < reserved_str_num; i++) {
		/* get the tag from reserved segment */
		tag_len = strlen(reserved_str_addr) + 1;
		if (tag_len > RESERVED_TAG_MAX_SIZE) return -1;
		memcpy(tag, reserved_str_addr, tag_len);
		reserved_str_addr += tag_len; //adjust offset by adding "tag" length

		/* get the length of value */
		val_len = *((unsigned char *)reserved_str_addr);
		reserved_str_addr += RESERVED_LEN_SIZE; //adjust offset by adding "len" length

		/* compare the tag with id */
		if (0 == strcmp(id, tag)) {
			/* set the reserved data */
			memcpy((char *)val, reserved_str_addr, val_len);
			return 0;
		}
		reserved_str_addr += val_len; //adjust offset by adding "val" length
	}

	return -1;
}

int teecfg_get_sml_coredump_addr(void *val)
{
	return teecfg_get_reserved_val("tee_sml_addr", val);
}

int teecfg_get_sml_coredump_size(void *val)
{
	return teecfg_get_reserved_val("tee_sml_size", val);
}

int teecfg_get_tos_coredump_addr(void *val)
{
	return teecfg_get_reserved_val("tee_tos_addr", val);
}

int teecfg_get_tos_coredump_size(void *val)
{
	return teecfg_get_reserved_val("tee_tos_size", val);
}

int teecfg_get_tos_coredump_header_addr(void *val)
{
	unsigned long long addr = 0;
	unsigned long size = 0;

	if (teecfg_get_reserved_val("tee_tos_addr", &addr) < 0 || \
		teecfg_get_reserved_val("tee_tos_size", &size) < 0)
		return -1;

	*(unsigned long long *)val = (unsigned long long)(addr + size - TOS_COREDUMP_HEADER_OFFSET);

	return 0;
}

int teecfg_get_tos_coredump_header_size(void *val)
{
	*(unsigned long *)val = TOS_COREDUMP_HEADER_SIZE;

	return 0;
}
