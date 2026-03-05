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
 ******************************************************************************
 **                        Edit History                                       *
 ** ------------------------------------------------------------------------- *
 ** DATE           NAME             DESCRIPTION                               *
 ** 10/22/2001     Jakle zhu     Create.                                      *
 ******************************************************************************/
#ifndef SCI_TYPES_H
#define SCI_TYPES_H

/**---------------------------------------------------------------------------*
 **                         Compiler Flag                                     *
 **---------------------------------------------------------------------------*/
#ifdef __cplusplus
    extern   "C"
    {
#endif

//#include "migrate.h"
/* ------------------------------------------------------------------------
** Constants
** ------------------------------------------------------------------------ */

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#define TRUE   1   /* Boolean true value. */
#define FALSE  0   /* Boolean false value. */

#define SCI_TRUE                    TRUE       // Boolean true value
#define SCI_FALSE                   FALSE       // Boolean false value

#ifndef NULL
  #define NULL  0
#endif

/* -----------------------------------------------------------------------
** Standard Types
** ----------------------------------------------------------------------- */
typedef unsigned char			BOOLEAN;
typedef unsigned char			uint8;
typedef unsigned short			uint16;
typedef unsigned int		    uint32;

#if 0
#ifndef _X_64
typedef unsigned __int64		uint64;
#else
typedef struct _X_UN_64_T {
	uint32 hiDWORD;
	uint32 loDWORD;
}X_UN_64_T;
typedef X_UN_64_T uint64;
typedef uint64 __uint64;
#endif
#endif

typedef signed char				int8;
typedef signed short			int16;
typedef signed long int			int32;

#ifndef _X_64
//typedef __int64				int64;
#else
typedef struct _X_64_T {
	int32 hiDWORD;
	int32 loDWORD;
}X_64_T;
typedef X_64_T int64;
typedef int64 __int64;
#endif


#ifdef WIN_UNIT_TEST
#define LOCAL
#define CONST
#else
#define	LOCAL		static
#define	CONST		const
#endif  //WIN_UNIT_TEST

#define	VOLATILE	volatile


#define PNULL		0

//Added by Xueliang.Wang on 28/03/2002
#define PUBLIC
//#define	PRIVATE		static
#define	SCI_CALLBACK

// @Xueliang.Wang moved it from os_api.h(2002-12-30)
// Thread block ID.
typedef uint32          BLOCK_ID;

/*
	Bit define
*/
#ifndef BIT_0
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
#endif

#ifdef WIN32
    #define PACK
#else
    #define PACK __packed    /* Byte alignment for communication structures.*/
#endif

/* some usefule marcos */
#define Bit(_i)              ((u32) 1<<(_i))

//#define  MAX( _x, _y ) ( ((_x) > (_y)) ? (_x) : (_y) )

//#define  MIN( _x, _y ) ( ((_x) < (_y)) ? (_x) : (_y) )
#define  WORD_LO(_xxx)  ((uint8) ((int16)(_xxx)))
#define  WORD_HI(_xxx)  ((uint8) ((int16)(_xxx) >> 8))

#define RND8( _x )       ((((_x) + 7) / 8 ) * 8 ) /*rounds a number up to the nearest multiple of 8 */


#define  UPCASE( _c ) ( ((_c) >= 'a' && (_c) <= 'z') ? ((_c) - 0x20) : (_c) )

#define  DECCHK( _c ) ((_c) >= '0' && (_c) <= '9')

#define  DTMFCHK( _c ) ( ((_c) >= '0' && (_c) <= '9') ||\
                       ((_c) >= 'A' && (_c) <= 'D') ||\
                       ((_c) >= 'a' && (_c) <= 'd') ||\
					   ((_c) == '*') ||\
					   ((_c) == '#'))

#define  HEXCHK( _c ) ( ((_c) >= '0' && (_c) <= '9') ||\
                       ((_c) >= 'A' && (_c) <= 'F') ||\
                       ((_c) >= 'a' && (_c) <= 'f') )

#define  ARR_SIZE( _a )  ( sizeof( (_a) ) / sizeof( (_a[0]) ) )

/*
    @Lin.liu Added.(2002-11-19)
*/
#define  BCD_EXT    uint8


/**---------------------------------------------------------------------------*
 **                         Compiler Flag                                     *
 **---------------------------------------------------------------------------*/
#ifdef __cplusplus
    }
#endif

#endif  /* SCI_TYPES_H */

