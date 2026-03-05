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

/*
	I2C2 -> AP_IIC2-config 2
*/
struct sensor_phypara sensor_board_phylist[]={
	{0x4C, 0x18, 0xA4 ,"mc3419", 2},
	{0x18, 0x0F, 0x11 ,"sc7a20", 2},
	{0x26, 0x01, 0x13 ,"mir3da", 2},
	{0x23, 0xB3, 0x9C ,"ltr569als", 2},
	{0x54, 0x20, 0x12 ,"ls98xx", 2},
};
#endif //_SENSOR_BOARD_INFO_H_
