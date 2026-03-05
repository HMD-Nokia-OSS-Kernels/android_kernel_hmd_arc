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

#ifndef _PLATFORM_IRQ_H
#define _PLATFORM_IRQ_H
#define SPRD_IRQ_TYPE_NONE		0
#define SPRD_IRQ_TYPE_EDGE_RISING	1
#define SPRD_IRQ_TYPE_EDGE_FALLING	2
#define SPRD_IRQ_TYPE_EDGE_BOTH	(SPRD_IRQ_TYPE_EDGE_RISING | SPRD_IRQ_TYPE_EDGE_FALLING)
#define SPRD_IRQ_TYPE_LEVEL_HIGH	4
#define SPRD_IRQ_TYPE_LEVEL_LOW	8
#endif //_PLATFORM_IRQ_H