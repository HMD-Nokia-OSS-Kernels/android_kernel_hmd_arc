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

#ifndef __FDL_STDIO_H_
#define __FDL_STDIO_H_

#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void*)0)
#endif /* __cplusplus */
#endif /* NULL */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/******************************************************************************
 * FDL_memcpy
 ******************************************************************************/
void *FDL_memcpy (void *dst, const void *src, unsigned int count);

/******************************************************************************
 * FDL_memset
 ******************************************************************************/
void *FDL_memset (void *dst, int c, unsigned int count);
unsigned short EndianConv_16 (unsigned short value);
unsigned int EndianConv_32 (unsigned int value);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* FDL_STDIO_H */

