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

#ifndef __SPRD_VERIFY_H
#define __SPRD_VERIFY_H

#include "sprdsec_header.h"
//#include<crypto/sprdrsa.h>

int sprd_secure_process_flow(uint8_t *name, uint8_t * header, uint8_t *code);
void dumpHex(const char *title, uint8_t * data, int len);
void cal_sha256(uint8_t * input, uint32_t bytes_num, uint8_t * output);
bool sprd_verify_cert(uint8_t * hash_key_precert, uint8_t * hash_data, uint8_t * certptr);
uint8_t * sprd_get_sechdr_addr(uint8_t * buf);
uint8_t * sprd_get_code_addr(uint8_t * buf);
uint8_t * sprd_get_cert_addr(uint8_t * buf);
void sprd_get_pubk(uint8_t *load_buf, uint8_t *pubk, int sprd_keycert_ver);
bool sprd_verify_img(uint8_t * hash_key_precert, uint8_t * imgbuf);
bool sprd_verify_wcn_gps(uint8_t * hash_key_precert, uint8_t * imgbuf, uint8_t flag);
bool sprd_verify_cert_wcn_gps(uint8_t * hash_key_precert, uint8_t * hash_data, uint8_t * certptr, uint8_t flag);
void sprd_secure_check(uint8_t * current_img_addr, uint8_t * data_header);
void sprd_dl_check(uint8_t * hash, uint8_t * data_header);
void sprd_dl_verify(uint8_t *name, uint8_t * header,uint8_t *code);
void sprd_fdl2_dl_verify(uint8_t *name,uint8_t * header,uint8_t *code);
int reload_spl_to_tos(void);
void sprd_calc_hbk(void);
void sprd_get_hash_key(uint8_t * load_buf,uint8_t *hash_key);
int8_t check_sprdimgheader(uint8_t *header, uint8_t *payload);
void sprd_fb_secure_verify(uint8_t *name,uint8_t * header,uint8_t *code);
void sprd_fb_secure_vboot_verify(uint8_t *name,uint8_t *partition_name,uint8_t * header,uint8_t *code);
void sprd_dl_vboot_verify(uint8_t *name,uint8_t *tocheck_name,uint8_t * header,uint8_t *code);
uint8_t sprd_dl_write_efuse(void);
#endif	/* !HEADER_AES_H */
