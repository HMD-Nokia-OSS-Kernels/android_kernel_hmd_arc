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
	{0x69 , 0x75, 0x11 ,"icm20600", 2},
	{0xC, 0x00, 0x48 ,"akm09918", 2},
	{0x23, 0x86, 0x92 ,"ltr553als", 2},
};
#endif //_SENSOR_BOARD_INFO_H_
