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

#include "dl_common.h"

unsigned short fdl_calc_checksum(unsigned char *data, unsigned long len)
{
	unsigned short num = 0;
	unsigned long chkSum = 0;
	while(len>1) {
		num = (unsigned short)(*data);
		data++;
		num |= (((unsigned short)(*data))<<8);
		data++;
		chkSum += (unsigned long)num;
		len -= 2;
	}
	if(len) {
		chkSum += *data;
	}
	chkSum = (chkSum >> 16) + (chkSum & 0xffff);
	chkSum += (chkSum >> 16);
	return (~chkSum);
}

unsigned short fdl_calc_checksum32(unsigned char *data, unsigned long len) {
  unsigned short num = 0;
  uint32_t chkSum = 0;
  while (len > 1) {
    num = (unsigned short)(*data);
    data++;
    num |= (((unsigned short)(*data)) << 8);
    data++;
    chkSum += (uint32_t)num;
    len -= 2;
  }
  if (len) {
    chkSum += *data;
  }
  chkSum = (chkSum >> 16) + (chkSum & 0xffff);
  chkSum += (chkSum >> 16);
  return (~chkSum);
}

unsigned short fdl_calc_checksum64(unsigned char *data, unsigned long len) {
  unsigned short num = 0;
  uint64_t chkSum = 0;
  while (len > 1) {
    num = (unsigned short)(*data);
    data++;
    num |= (((unsigned short)(*data)) << 8);
    data++;
    chkSum += (uint64_t)num;
    len -= 2;
  }
  if (len) {
    chkSum += *data;
  }
  chkSum = (chkSum >> 16) + (chkSum & 0xffff);
  chkSum += (chkSum >> 16);
  return (~chkSum);
}

unsigned char fdl_check_crc(char* buf, uint32_t size,uint32_t checksum)
{
	uint16_t crc;
	uint16_t len = 0;
	static uint8_t check_flag = 0x0;

	if(!check_flag) {
		crc = fdl_calc_checksum(buf,size);
		debugf("fdl_check_crc  calcout = 0x%x,org = 0x%x\n",crc,checksum);
		if(crc != (uint16_t)checksum)
		{
			len = sizeof(unsigned long);
			if(len == sizeof(uint32_t)) {
				crc = fdl_calc_checksum64(buf,size);
				check_flag = 0x1;
			} else {
				crc = fdl_calc_checksum32(buf,size);
				check_flag = 0x2;
			}
			if(crc != (uint16_t)checksum) {
				check_flag = 0x0;
			}
		}
	} else if(check_flag == 0x1) {
		crc = fdl_calc_checksum64(buf,size);
	} else {
		crc = fdl_calc_checksum32(buf,size);
	}
	debugf("fdl_check_crc  calcout = 0x%x,org = 0x%x,len=0x%x,chkflag=0x%x\n",crc,checksum,len,check_flag);
	return (crc == (uint16_t)checksum);
}

//This param src must be 4 byte aligned.
unsigned long Get_CheckSum (const unsigned char *src, int len)
{
    unsigned long sum =0;

    while (len > 3)
    {
        //sum += *((unsigned long *)src)++;
        sum += *src++;
        sum += *src++;
        sum += *src++;
        sum += *src++;

        len-=4;
    }

    while (len)
    {
        sum +=*src++;
        len--;
    }

    return sum;
}

#ifdef NV_CHECK_WITH_SHA256
extern void sha256_csum_wd_sw(const unsigned char *input,unsigned int ilen,unsigned char *output,unsigned int chunk_sz);
extern void sprd_hexdump(uint32_t title, uint8_t * data, int len);
int fdl_check_sha256(char* buf, uint32_t size,uint8_t* auth256) {
    uint8_t sha256[HASH_SHA256_BUF_LEN] = {0};
    sha256_csum_wd_sw(buf, size, sha256, 0);
#ifdef PRINT_NV_DATA
    sprd_hexdump(1, auth256, 32);
    sprd_hexdump(2, sha256, 32);
#endif
    if(0 != memcmp(sha256, auth256, HASH_SHA256_BUF_LEN)){
        debugf("fdl_check_sha256 fail");
        return 0;
    }else{
        debugf("fdl_check_sha256 pass");
        return 1;
    }
}

int fdl_get_sha256(char* buf, uint32_t size, uint8_t* output) {
    uint8_t sha256[HASH_SHA256_BUF_LEN] = {0};
    sha256_csum_wd_sw(buf, size, sha256, 0);
#ifdef PRINT_NV_DATA
    debugf("fdl_get_sha256 size:0x%x", size);
    sprd_hexdump(2, sha256, HASH_SHA256_BUF_LEN);
#endif
    memcpy(output, sha256, HASH_SHA256_BUF_LEN);
    return 1;
}
#endif
