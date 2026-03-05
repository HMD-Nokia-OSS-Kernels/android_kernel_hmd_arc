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
 **                   Edit    History                                         *
 **---------------------------------------------------------------------------*
 ** DATE          NAME            DESCRIPTION                                 *
 ** 9/1/2003      Daniel.Ding     Create.                                     *
 ******************************************************************************/
#ifndef _COMMON_H_
#define _COMMON_H_
/*----------------------------------------------------------------------------*
 **                         Dependencies                                      *
 **-------------------------------------------------------------------------- */

//#include "stdio.h"
#include "stdarg.h"
#include <string.h>
#include <linux/types.h>
#include "sprd_debug.h"
#include <lk/reg.h>

/**---------------------------------------------------------------------------*
 **                             Compiler Flag                                 *
 **---------------------------------------------------------------------------*/
#ifdef   __cplusplus
extern   "C"
{
#endif
/**----------------------------------------------------------------------------*
**                               Micro Define                                 **
**----------------------------------------------------------------------------*/
#define	LOCAL		static
#define PUBLIC
#define TRUE   1   /* Boolean true value. */
#define FALSE  0   /* Boolean false value. */

/*
    Bit define
*/
#define BIT_0               (1<<(0))
#define BIT_1               (1<<(1))
#define BIT_2               (1<<(2))
#define BIT_3               (1<<(3))
#define BIT_4               (1<<(4))
#define BIT_5               (1<<(5))
#define BIT_6               (1<<(6))
#define BIT_7               (1<<(7))
#define BIT_8               (1<<(8))
#define BIT_9               (1<<(9))
#define BIT_10              (1<<(10))
#define BIT_11              (1<<(11))
#define BIT_12              (1<<(12))
#define BIT_13              (1<<(13))
#define BIT_14              (1<<(14))
#define BIT_15              (1<<(15))
#define BIT_16              (1<<(16))
#define BIT_17              (1<<(17))
#define BIT_18              (1<<(18))
#define BIT_19              (1<<(19))
#define BIT_20              (1<<(20))
#define BIT_21              (1<<(21))
#define BIT_22              (1<<(22))
#define BIT_23              (1<<(23))
#define BIT_24              (1<<(24))
#define BIT_25              (1<<(25))
#define BIT_26              (1<<(26))
#define BIT_27              (1<<(27))
#define BIT_28              (1<<(28))
#define BIT_29              (1<<(29))
#define BIT_30              (1<<(30))
#define BIT_31              (1<<(31))

//Debug info Function and MacroS.

#define CHIP_DRV_ASSERT SCI_ASSERT
#define CHIP_DRV_PASSERT SCI_PASSERT

typedef enum{
TYPE_RESET = 0,           //bit[4:0]
TYPE_BACKLIGHT,         //bit[5]
TYPE_DSPEXCEPTION,  //bit[6]
TYPE_USB,                    //bit[7]
TYPE_MAX
}WDG_HW_FLAG_T;

typedef enum{
TDPLL_REF0 = 0,
TDPLL_REF1,
TDPLL_MAX,
}TDPLL_REF_T;

/**----------------------------------------------------------------------------*
**                             Data Prototype                                 **
**----------------------------------------------------------------------------*/

/**----------------------------------------------------------------------------*
**                         Local Function Prototype                           **
**----------------------------------------------------------------------------*/

/**----------------------------------------------------------------------------*
**                           Function Prototype                               **
**----------------------------------------------------------------------------*/

/**----------------------------------------------------------------------------*
**                         Compiler Flag                                      **
**----------------------------------------------------------------------------*/
#ifdef   __cplusplus
}
#endif
/**---------------------------------------------------------------------------*/
#endif
// End

