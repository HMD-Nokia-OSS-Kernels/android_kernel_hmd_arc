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

#ifndef _SECURE_EFUSE_H
#define _SECURE_EFUSE_H

#define PROG_OK     0
#define SHA1_ERR   -1
#define AES256_ERR -2
#define SHA256_ERR -3
#define SANSA_EFUSE_PRO_ERR -4
#define SANSA_GET_LCS_ERR   -5
#define SANSA_SET_RMA_ERR   -6
#define SANSA_KCE_PRO_ERR   -7

int secure_efuse_program_native(void);
int secure_efuse_program_sansa(void);

int read_master_key(void);

int sansa_compute_socid(void *socid);
int sansa_get_lcs_cmd(void);
int sansa_set_rma_cmd(void);
int sansa_kce_program(uint32_t *kcePtr);
void sansa_calc_hbk(void);

#endif
