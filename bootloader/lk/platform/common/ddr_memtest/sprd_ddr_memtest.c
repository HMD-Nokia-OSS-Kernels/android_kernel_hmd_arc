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

#include <chipram_env.h>
#include "memtest_interface.h"
#include "sprd_ddr_memtest.h"
#include "lcd_console.h"
#include <asm/arch/sprd_reg.h>

ulong ram_size = 0;

#define LK_SIZE_RESERVED (0x40000000)//need consider lcd buffer address
#define MEMTEST_COUNT (0x80000f04)//need write by chipram ddr init

#if defined(SPRD_PUB_PHYS)
#define DDR_DMC_BASE SPRD_PUB_PHYS
#elif defined(SPRD_PUB_RF_DDR_CTRL_BASE)
#define DDR_DMC_BASE SPRD_PUB_RF_DDR_CTRL_BASE
#elif defined(SPRD_PUB_DMC_PHYS)
#define DDR_DMC_BASE SPRD_PUB_DMC_PHYS
#endif

extern void modem_entry(void);
extern phys_size_t real_ram_size;

void get_ddr_size(void)
{
#if 0
	chipram_env_t * env = CHIPRAM_ENV_LOCATION;
	if (CHIPRAM_ENV_MAGIC != env->magic) {
		dprintf(INFO,"Chipram magic wrong , ddr data may be broken\n");
		lcd_printf("Chipram magic wrong , ddr data may be broken\n");
		return 0;
	}

	ram_size = 0;

	if (env->cs_number == 1) {
		ram_size += env->cs0_size;
		debugf("dram cs0 size %lx\n",env->cs0_size);
		lcd_printf("dram cs0 size %lx\n",env->cs0_size);
	} else if (env->cs_number == 2) {
		ram_size += env->cs0_size;
		ram_size += env->cs1_size;
		debugf("dram cs0 size 0x%lx dram cs1 size 0x%lx\n",env->cs0_size, env->cs1_size);
		lcd_printf("dram cs0 size 0x%lx dram cs1 size 0x%lx\n",env->cs0_size, env->cs1_size);
	}
#else
	ram_size = real_ram_size;
#endif
}

int ddr_memtester(ulong base_addr, ulong test_len, ulong test_times, ulong is_loop)
{
	int i, ret = 0;
	volatile u32 loop = is_loop;/*for check log, if not need set 0*/
	u32 ddr_freq, test_time;
	ulong ram_reserved_size = 0;
	dprintf(INFO,"DDR mem test start\n");
	lcd_printf("DDR mem test start\n");

#ifdef DDR_DMC_BASE
	ddr_freq = (readl(DDR_DMC_BASE + 0x12c) >> 8) & 0x7;
	dprintf(INFO,"DDR freq:%d\n", ddr_freq);
	lcd_printf("DDR freq:%d\n", ddr_freq);
#endif

#ifdef MEMTEST_COUNT
	test_time = readl(MEMTEST_COUNT);
	dprintf(INFO,"test_time:%d\n", test_time);
	lcd_printf("test_time:%d\n", test_time);
#endif

	get_ddr_size();
	if (ram_size <= LK_SIZE_RESERVED) {
		lcd_printf("DDR size:0x%lx or less than lk_used_size\n", ram_size);
		dprintf(INFO,"DDR size:0x%lx less than lk_used_size\n", ram_size);
		ret = -1;
		goto test_end;
	}

	ram_reserved_size = ram_size - LK_SIZE_RESERVED;
	if (ram_reserved_size < test_len) {
		lcd_printf("DDR reserved size:0x%lx less than test_len:0x%lx\n", ram_reserved_size, test_len);
		dprintf(INFO,"DDR reserved size:0x%lx less than test_len:0x%lx", ram_reserved_size, test_len);
		ret = -1;
		goto test_end;
	}

	for (i = 0; i < test_times; i++) {
		if (ddr_mem_test(base_addr, test_len, is_loop) == 0) {
			lcd_printf("DDR mem test pass, times: %d\n", i + 1);
			dprintf(INFO,"DDR mem test pass, times: %d\n", i + 1);
		} else {
			lcd_printf("DDR mem test fail, times: %d\n", i + 1);
			dprintf(INFO,"DDR mem test fail, times: %d\n", i + 1);
			ret++;
		}
	}
	ret = ret ? -1 : 0;

test_end:
	while(loop);

	return ret;
}
