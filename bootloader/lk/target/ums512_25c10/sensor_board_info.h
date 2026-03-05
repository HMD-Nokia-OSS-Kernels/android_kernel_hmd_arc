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

#ifndef _SENSOR_BOARD_INFO_H_
#define _SENSOR_BOARD_INFO_H_

/*I2C2 -> AP_IIC0-config 0
  I2C5 -> AP_IIC1-config 1
*/
struct sensor_phypara sensor_board_phylist[]={
	{0x2C , 0x00, 0x80 ,"qmc6308", 0},
	{0x30 , 0x39, 0x10 ,"mmc5603", 0},
};

#endif //_SENSOR_BOARD_INFO_H_