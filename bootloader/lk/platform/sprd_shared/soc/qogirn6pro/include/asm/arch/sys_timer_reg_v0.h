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

/******************************************************************************
 ** File Name:    sys_timer_reg_v0.h                                            *
 ** Author:       Steve.Zhan                                                 *
 ** DATE:         06/05/2010                                                  *
 ** Copyright:    2010 Spreatrum, Incoporated. All Rights Reserved.           *
 ** Description:                                                              *
 ******************************************************************************/
/******************************************************************************
 **                   Edit    History                                         *
 **---------------------------------------------------------------------------*
 ** DATE          NAME            DESCRIPTION                                 *
 ** 06/12/2010    hao.liu    Create.                                     *
 ******************************************************************************/
#ifndef _SYS_TIMER_REG_V0_H_
#define _SYS_TIMER_REG_V0_H_

/**---------------------------------------------------------------------------*
**                               Micro Define                                **
**---------------------------------------------------------------------------*/
/*----------System Count----------*/
#include "sprd_reg.h"

#define SYSTIMER_BASE                   (SPRD_SYSCNT_PHYS)

#define SYS_ALM                         (SYSTIMER_BASE + 0x0000)
#define SYS_CNT0                        (SYSTIMER_BASE + 0x0004)
#define SYS_CTL                         (SYSTIMER_BASE + 0x0008)
#define SYS_VLAUE_SHDW                  (SYSTIMER_BASE + 0x000C)

#define SYSTEM_CURRENT_CLOCK (*((volatile u32 *)SYS_CNT0) & 0xFFFFFFFF)

/**---------------------------------------------------------------------------*/
#endif
// End
