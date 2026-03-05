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


#ifndef TEECFG_PARSE_H
#define TEECFG_PARSE_H


#define TEECFG_HEADER_MAGIC      "TEE_CONFIG_HEADER"
#define SEG_NAME_MAX_SIZE        24
#define SEG_DESC_MAX_SIZE        100

#define RESERVED_TAG_MAX_SIZE      72
#define RESERVED_LEN_SIZE          sizeof(unsigned char)

typedef struct
{
	unsigned char name[SEG_NAME_MAX_SIZE];
	unsigned int offset;
	unsigned int size;
} __attribute__((packed)) seg_desc_t;

typedef struct
{
	unsigned char name[SEG_NAME_MAX_SIZE]; //"TEE_CONFIG_HEADER"
	unsigned int version; //v1.0
	unsigned int size; //sizeof(tee_config_header_t)
	unsigned int seg_desc_off; //the first seg_desc offset in teecfg file
	unsigned int seg_desc_num; //seg_desc numbers
	unsigned int res_seg_off; //offset of reserved value from the file header
	unsigned int res_seg_num; //number of reserved value
	unsigned int reserved0; //reserved
	unsigned int reserved1; //reserved
	unsigned int reserved2; //reserved
	unsigned int reserved3; //reserved
	unsigned int reserved4; //reserved
	unsigned int reserved5; //reserved
	unsigned int reserved6; //reserved
	unsigned int reserved7; //reserved
	unsigned int reserved8; //reserved
	unsigned int reserved9; //reserved
	unsigned int reserved10; //reserved
	unsigned int reserved11; //reserved
	seg_desc_t seg_desc[SEG_DESC_MAX_SIZE]; //seg descriptor infos
} __attribute__((packed)) tee_config_header_t;

int teecfg_get_reserved_val(const char *id, void *val);
int teecfg_get_sml_coredump_addr(void *val);
int teecfg_get_sml_coredump_size(void *val);
int teecfg_get_tos_coredump_addr(void *val);
int teecfg_get_tos_coredump_size(void *val);
int teecfg_get_tos_coredump_header_addr(void *val);
int teecfg_get_tos_coredump_header_size(void *val);

#endif
