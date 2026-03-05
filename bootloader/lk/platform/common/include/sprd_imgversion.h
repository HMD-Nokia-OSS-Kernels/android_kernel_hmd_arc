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

#ifndef _SPRDIMGVERSION_H
#define _SPRDIMGVERSION_H

#define IMGVER_DEBUG 1

typedef enum enAntiRBImageType {
	IMAGE_VBMETA = 0,
	IMAGE_BOOT,
	IMAGE_RECOVERY,
	IMAGE_SYSTEM,
	IMAGE_VENDOR,
	IMAGE_L_MODEM,
	IMAGE_L_LDSP,
	IMAGE_L_LGDSP,
	IMAGE_PM_SYS,
	IMAGE_AGDSP,
	IMAGE_WCN,
	IMAGE_GPS,
	IMAGE_GPU,
	IMAGE_SOCKO,
	IMAGE_ODMKO,
	IMAGE_SPARE,
	IMAGE_SPARES,
	IMAGE_TYPE_END
} antirb_image_type;

/*
*@imgType   The image which need to get the version
*@swVersion return image version
*Return value: zero is ok
*
*/

//get a_slot imgversion
int sprd_get_imgversion(int imgType, unsigned int* swVersion);

//get b_slot imgversion
int sprd_get_b_slot_imgversion(int imgType, unsigned int* swVersion);

int sprd_init_all_imgversion(uint8_t *rpmb_key);

#endif //_SPRDIMGVERSION_H
