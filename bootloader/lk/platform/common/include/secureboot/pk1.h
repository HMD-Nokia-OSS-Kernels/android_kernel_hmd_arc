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

#ifndef PK1_H
#define PK1_H

void invert_char(unsigned char *src,int len);

int get_rand_bytes(unsigned char *buf, int num);

int padding_add_PKCS1_type_1(unsigned char *to, int tlen,
	     const unsigned char *from, int flen);

int padding_check_PKCS1_type_1(unsigned char *to, int tlen,
	     const unsigned char *from, int flen, int num);

int padding_add_PKCS1_type_2(unsigned char *to, int tlen,
	     const unsigned char *from, int flen);

int padding_check_PKCS1_type_2(unsigned char *to, int tlen,
	     const unsigned char *from, int flen, int num);

#endif
