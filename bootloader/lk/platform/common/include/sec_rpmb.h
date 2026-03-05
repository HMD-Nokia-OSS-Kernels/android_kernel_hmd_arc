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

#ifndef SEC_RPMB_H_
#define SEC_RPMB_H_

/*
*@buf  The data read from RPMB
*@len   The buffer length must be 256
*@blk_ind rpmb reserve block number [1 - 13]
*Return value: zero is ok
*you must call uboot_set_rpmb_size() berfor using this api.
*/
int sec_rpmb_read(void *buf, int len, int blk_ind);

/*
*@buf  The data to write to RPMB
*@len   The buffer length must be 256
*@blk_ind rpmb reserve block number [1 - 13]
*Return value: zero is ok
*you must call uboot_set_rpmb_size() berfor using this api.
*/
int sec_rpmb_write(void *buf, int len, int blk_ind);

/*
*@buf  The Secure Write Protect Configuration Block  write to RPMB
*@len   The buffer length must be 256
*Return value: zero is ok
*
*/
int sec_rpmb_swp_config_write(void *buf, int len);

/*
*@buf  The Secure Write Protect Block Configuration read from RPMB
*@len   The buffer length must be 256
*Return value: zero is ok
*/
int sec_rpmb_swp_config_read(void *buf, int len);

#endif
