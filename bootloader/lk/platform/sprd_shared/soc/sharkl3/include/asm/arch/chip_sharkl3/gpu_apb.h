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

#ifndef __GPU_APB_H____
#define __GPU_APB_H____

/* Some defs, in case these are not defined elsewhere */
#ifndef SCI_IOMAP
#define SCI_IOMAP(_b_) ( (_b_) )
#endif

#ifndef SCI_ADDR
#define SCI_ADDR(_b_, _o_) ( (_b_) + (_o_) )
#endif

#ifndef CTL_GPU_APB_BASE
#define CTL_GPU_APB_BASE                SCI_IOMAP(0x60100000)
#endif

/* registers definitions for CTL_GPU_APB, 0x60100000 */
#define REG_GPU_APB_APB_RST                                 SCI_ADDR(CTL_GPU_APB_BASE, 0x0000)

/* bits definitions for REG_GPU_APB_APB_RST, [0x60100000] */
#define BIT_GPU_APB_GPU_SOFT_RST                            ( BIT(0) )

/* vars definitions for controller CTL_GPU_APB */


#endif /* __GPU_APB_H____ */
