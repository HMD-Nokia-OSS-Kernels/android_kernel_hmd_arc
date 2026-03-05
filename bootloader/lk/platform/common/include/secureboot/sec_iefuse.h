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

#ifndef _IWHALE2_EFUSE_DRIVE_H
#define _IWHALE2_EFUSE_DRIVE_H

/* secure efuse block id definitions */
#define HUK_BLOCK_START      (0)
#define HUK_BLOCK_END        (7)
#define KCE_BLOCK_START      (8)
#define KCE_BLOCK_END        (15)
#define ROTPK0_BLOCK_START   (16)
#define ROTPK0_BLOCK_END     (23)
#define SEC_VERSION_BLOCK    (24)
#define NSEC_VER_BLOCK_START (25)
#define NSEC_VER_BLOCK_END   (31)
#define ROTPK1_BLOCK_START   (32)
#define ROTPK1_BLOCK_END     (39)
#define ENDORKEY_BLOCK_START (40)
#define ENDORKEY_BLOCK_END   (47)
#define RESERVED_BLOCK_START (48)
#define RESERVED_BLOCK_END   (61)
#define CYCLE_STATE_BLOCK    (62)
#define LOCK_BIT_BLOCK       (63)

#define PUBLIC_EFUSE_BLOCK2  (2)

#define RMA_MODE_BIT         (0)

typedef enum {
	EFUSE_RESULT_SUCCESS = 0,
	EFUSE_RD_ERROR,
	EFUSE_WR_ERROR,
	EFUSE_ID_ERROR,
	EFUSE_PARAM_ERROR
} Efuse_Result_Ret;

Efuse_Result_Ret sprd_ce_efuse_read(unsigned int block_id, unsigned int *pReadData);

Efuse_Result_Ret sprd_ce_efuse_program(unsigned int blocked_id, unsigned int pWriteData);

Efuse_Result_Ret sprd_ce_efuse_huk_program(void);

Efuse_Result_Ret sprd_get_lock_bits(unsigned int start_id, unsigned int end_id, unsigned int *bits_data, unsigned int *bits_data1);

unsigned int sprd_get_secure_boot_enable(void);

#endif
