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

#include <sprd_common.h>
#include <linux/types.h>
#include <string.h>
#include "dl_root_inspect.h"
#include "boot_parse.h"
#include "sprd_common_rw.h"
#include <malloc.h>

u32 get_rootflag(root_stat_t *stat)
{
	u32 result = 0;

	if (0 != common_raw_read(PRODUCTINFO_FILE_PATITION, (uint64_t)sizeof(root_stat_t), ROOT_OFFSET, (char *)stat)) {
		errorf("read miscdata error.\n");
		return result;
	}

	if (stat->magic == ROOT_MAGIC) {
		debugf("ROOT_MAGIC matched\n");
		debugf("stat.root_flag = %d\n", stat->root_flag);
		result = stat->root_flag;
	}
	return result;
}

u32 erase_rootflag(root_stat_t *stat)
{
	unsigned long erasesize = 0;

	erasesize = sizeof(root_stat_t);
	char *buf = malloc(erasesize);
	if (buf == NULL) {
		errorf("erase_rootflag malloc failed!\n");
		return -1;
	}

	memset(buf, 0, erasesize);
	debugf("Erase miscdata partition.\n");

	if (0 != common_raw_write(PRODUCTINFO_FILE_PATITION, (uint64_t)erasesize, (uint64_t)0, ROOT_OFFSET, buf)) {
		errorf("erase miscdata error.\n");
		free(buf);
		return -1;
	}

	memset(stat, 0, erasesize);
	free(buf);

	return 0;
}
