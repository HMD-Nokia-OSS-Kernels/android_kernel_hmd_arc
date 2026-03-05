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

#include <sprd_common.h>
#include <uid_helper.h>
#include <chipram_env.h>

#ifndef UID_START
#define UID_START	0
#endif
#ifndef UID_END
#define UID_END		1
#endif
#ifndef UID_DOUBLE
#define UID_DOUBLE	1
#endif

#define LOT_MSK		0x3F
#define TO_ASIC		48

int sprd_get_chip_uid(char *buf)
{
	u32 blocks[2] = {0};
	u32 LOTID_0, LOTID_1, LOTID_2, LOTID_3, LOTID_4, LOTID_5, x, y, wafer_id;
	if (!buf)
		return -1;

#if defined (CONFIG_SPRD_AP_NORMAL_EFUSE)
	if(get_chipram_env()->mode == BOOTLOADER_MODE_DOWNLOAD){
		if (sprd_secure_efuse_read(UID_START, UID_START, &blocks[1], UID_DOUBLE)) {
			return -2;
		}
		if (sprd_secure_efuse_read(UID_END, UID_END, &blocks[0], UID_DOUBLE)) {
			return -2;
           }
	}else{
		blocks[1] = sprd_efuse_double_read(UID_START,UID_DOUBLE);
		blocks[0] = sprd_efuse_double_read(UID_END,UID_DOUBLE);
	}
#else
	blocks[0]= sprd_ap_efuse_read(UID_START);
	blocks[1]= sprd_ap_efuse_read(UID_END);
#endif

	y = blocks[1] & 0x7F;
	x = (blocks[1] >> 7) & 0x7F;
	wafer_id = (blocks[1] >> 14) & 0x1F;
	LOTID_0 = ((blocks[1] >> 19) & LOT_MSK) + TO_ASIC;
	LOTID_1 = ((blocks[1] >> 25) & LOT_MSK) + TO_ASIC;
	LOTID_2 = (blocks[0] & 0x3F) + TO_ASIC;
	LOTID_3 = ((blocks[0] >> 6) & LOT_MSK) + TO_ASIC;
	LOTID_4 = ((blocks[0] >> 12) & LOT_MSK) + TO_ASIC;
	LOTID_5 = ((blocks[0] >> 18) & LOT_MSK) + TO_ASIC;

	debugf("uid start id 0x%x,end id 0x%x\n", blocks[0], blocks[1]);
	sprintf(buf, "%c%c%c%c%c%c_%d_%d_%d", LOTID_5, LOTID_4,
		LOTID_3, LOTID_2, LOTID_1, LOTID_0, wafer_id, x, y);
	return 0;
}

int sprd_get_chip_hex_uid(char *buf)
{
	u32 blocks[2] = {0};

#if defined (CONFIG_SPRD_AP_NORMAL_EFUSE)
	if(get_chipram_env()->mode == BOOTLOADER_MODE_DOWNLOAD){
		if (sprd_secure_efuse_read(UID_START, UID_START, &blocks[1], UID_DOUBLE)) {
			return -2;
		}
		if (sprd_secure_efuse_read(UID_END, UID_END, &blocks[0], UID_DOUBLE)) {
			return -2;
           }
	}else{
		blocks[1] = sprd_efuse_double_read(UID_START,UID_DOUBLE);
		blocks[0] = sprd_efuse_double_read(UID_END,UID_DOUBLE);
	}
#else
	blocks[0]= sprd_ap_efuse_read(UID_START);
	blocks[1]= sprd_ap_efuse_read(UID_END);
#endif

	sprintf(buf, "%08x%08x", blocks[0], blocks[1]);
	return 0;
}

void sprd_uid_bin_info(void)
{
	char uid[25] = {0};

	sprd_get_chip_uid(uid);
	debugf("uid is %s\n", uid);
}
