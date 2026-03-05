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

#ifndef _SPRD_DEBUG_H_
#define _SPRD_DEBUG_H_

#define SPRD_ERROR 0
#define SPRD_WARNING 1
#define SPRD_INFO 2
#define SPRD_DEBUG 3

#if (SPRD_DEBUG_LEVEL > SPRD_ERROR)
#define sprd_err(fmt, args...) do {printf("err:%s(): "fmt, __func__, ##args); } while (0)
#else
#define sprd_err(fmt, args...) do {} while (0)
#endif

#if (SPRD_DEBUG_LEVEL > SPRD_WARNING)
#define sprd_warning(fmt, args...) do {printf("warning:%s(): "fmt, __func__, ##args); } while (0)
#else
#define sprd_warning(fmt, args...) do {} while (0)
#endif

#if (SPRD_DEBUG_LEVEL > SPRD_INFO)
#define sprd_info(fmt, args...) do {printf("info:%s(): "fmt, __func__, ##args); } while (0)
#else
#define sprd_info(fmt, args...) do {} while (0)
#endif

#if (SPRD_DEBUG_LEVEL > SPRD_DEBUG)
#define sprd_dbg(fmt, args...) do {printf("dbg:%s(): "fmt, __func__, ##args); } while (0)
#else
#define sprd_dbg(fmt, args...) do {} while (0)
#endif

#endif	/*end of _SPRD_DEBUG_H_*/
