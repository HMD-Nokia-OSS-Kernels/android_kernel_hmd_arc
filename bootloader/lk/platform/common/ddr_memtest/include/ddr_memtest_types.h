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

#ifndef _DDR_MEMTEST_TYPES_H_
#define _DDR_MEMTEST_TYPES_H_

#include <sprd_common.h>
#include <linux/types.h>
#include <asm/arch/common.h>
#include <rand.h>

typedef unsigned long ul;
typedef unsigned long long ull;
typedef unsigned long volatile ulv;
typedef unsigned char volatile u8v;
typedef unsigned short volatile u16v;

#define sprd_ddr_rand32() ((unsigned int) rand() | ( (unsigned int) rand() << 16))

#ifndef CONFIG_ARM64
	#define sprd_ddr_rand_ul() sprd_ddr_rand32()
	#define SPRD_DDR_UL_ONEBITS 0xffffffff
	#define SPRD_DDR_UL_LENGTH 32
	#define SPRD_DDR_CHK1 0x55555555
	#define SPRD_DDR_CHK2 0xaaaaaaaa
	#define SPRD_DDR_UL_BYTE(x) ((x | x << 8 | x << 16 | x << 24))
#else
	#define sprd_ddr_rand64() (((ul) sprd_ddr_rand32()) << 32 | ((ul) sprd_ddr_rand32()))
	#define sprd_ddr_rand_ul() sprd_ddr_rand64()
	#define SPRD_DDR_UL_ONEBITS 0xffffffffffffffffUL
	#define SPRD_DDR_UL_LENGTH 64
	#define SPRD_DDR_CHK1 0x5555555555555555
	#define SPRD_DDR_CHK2 0xaaaaaaaaaaaaaaaa
	#define SPRD_DDR_UL_BYTE(x) (((ul)x | (ul)x<<8 | (ul)x<<16 | (ul)x<<24 | (ul)x<<32 | (ul)x<<40 | (ul)x<<48 | (ul)x<<56))
#endif

#define MEMTEST_SUCCESS				0
#define MEMTEST_FAIL				1

#define MEMTEST_STEP_BYTE			4096
#define MEMTEST_ADDR_WALK_ITERATIONS		32
#ifdef CONFIG_ARM64
#define MEMTEST_MOV_INV_FIXED_PATTERN1		0x8421CAA9653EDB70
#define MEMTEST_MOV_INV_FIXED_PATTERN2		0x7BDE35569AC1248F
#else
#define MEMTEST_MOV_INV_FIXED_PATTERN1		0x653EDB70
#define MEMTEST_MOV_INV_FIXED_PATTERN2		0x9AC1248F
#endif
#define MEMTEST_MOV_INV_FIXED_ITERATIONS	32
#define MEMTEST_BLOCK_MOVE_ITERATIONS		32
#define MEMTEST_MODULO_N_ITERATIONS		32
#define MEMTEST_MODULO_N			7
#define MEMTEST_MODULO_OFFSET			3	//assert MEMTEST_MODULO_OFFSET<MEMTEST_MODULO_N
#ifdef CONFIG_ARM64
#define MEMTEST_MODULO_N_PATTERN1		0x5555555555555555
#define MEMTEST_MODULO_N_PATTERN2		0xAAAAAAAAAAAAAAAA
#else
#define MEMTEST_MODULO_N_PATTERN1		0x55555555
#define MEMTEST_MODULO_N_PATTERN2		0xAAAAAAAA
#endif
#define MEMTEST_STUCK_ADDRESS_ITERATIONS	16
#define MEMTEST_SOLIDBITS_ITERATIONS		32
#define MEMTEST_CHECKERBOARD_ITERATIONS		32
#define MEMTEST_BLOCKSEQ_ITERATIONS		32

#define CONSOLE_CMD_CURSOR_HOME			"\x1b[H"
#define CONSOLE_CMD_CLR_SCREEN			"\x1b[2J"
#define CONSOLE_CMD_CLR_CUR_LINE		"\x1b[2K"

struct sprd_memtest_list {
	char *name;
	int (*sprd_memtest_func)(ulv*, size_t);
};

typedef struct refresh_t_ {
	ulv *bufa;
	ulv *bufb;
	size_t count;
	int time;
	int use;
	int do_mlock;
}refresh_t, *lprefresh_t;

#define MODE_NORMAL 1
#define MODE_REFRESH 2

#endif
