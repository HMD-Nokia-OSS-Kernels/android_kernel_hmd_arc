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

#include "ddr_memtest_types.h"
#include "memtest_interface.h"
#include <asm/arch/common.h>
#include <sprd_common.h>
#include <rand.h>
#include <lcd_console.h>

#define TEST_NARROW_WRITES

char progress[] = "-\\|/";
#define PROGRESSOFTEN 2500
#define ONE 0x00000001L

static unsigned lcd_col, lcd_row;
#ifdef UPDATE_PROGRESS
static ul progress_printed;
static ul progress_full;
#endif

static int compare_times = 0;
/* Function definitions. */

#define fprintf printf
//#define printf lcd_printf
//#define UPDATE_PROGRESS

union {
	unsigned char bytes[SPRD_DDR_UL_LENGTH / 8];
	ul val;
} mword8;

union {
	unsigned short u16s[SPRD_DDR_UL_LENGTH / 16];
	ul val;
} mword16;

static ulv *sr_addr;
static ulv *dst_addr;
typedef long off_tt;

static void clear_screen(void)
{
	unsigned i, j;

	/* clear console */
	// dprintf(INFO,CONSOLE_CMD_CURSOR_HOME);
	// dprintf(INFO,CONSOLE_CMD_CLR_SCREEN);

	/* clear lcd screen */
	lcd_position_cursor(0, 0);
	for (j = 0; j < lcd_row; j++) {
		for (i = 0; i < lcd_col; i++) {
			lcd_putc(' ');
		}
	}
	lcd_position_cursor(0, 0);
}

static void sprd_ddr_memtest_prepare(void)
{
	lcd_row = lcd_get_screen_rows();
	lcd_col = lcd_get_screen_columns();
}

static void sprd_ddr_memtest_progress_start(char *title, int pass)
{
#ifdef UPDATE_PROGRESS
	ul i;

	progress_printed = 0;
	progress_full = (ul)(lcd_col) * ((lcd_row >> 3) - 2);

	clear_screen();
	lcd_printf("\n");
	for (i = 0; i < progress_full; i++) {
		lcd_putc('.');
	}
#endif
	dprintf(INFO,"Please wait memtest runing.\n");
	lcd_printf("Please wait memtest runing.\n");
#ifdef UPDATE_PROGRESS
	lcd_position_cursor(0, 0);
#endif
	dprintf(INFO,"sprd_ddr_memtest: %s [%d]\n", title, pass);
	lcd_printf("sprd_ddr_memtest: %s [%d]\n", title, pass);
}

static void sprd_ddr_memtest_progress_end(void)
{
	clear_screen();
}

#ifdef UPDATE_PROGRESS
static void sprd_ddr_memtest_progress_step(ull curr, ull size, char c)
{
	ull cur_size = (curr * progress_full) / size;

	while (progress_printed < cur_size) {
		lcd_printf("%c", c);
		progress_printed++;
	}

	dprintf(INFO,"%llu%%\n", cur_size * 100ULL / progress_full);
}
#endif

int sprd_ddr_memtest_addressing(ulv *base_addr, size_t bytes)
{
	register ulv *p;
	size_t words = bytes / sizeof(ul);
	ul i, size_full = words << 1;

	p = base_addr;
	for (i = 0; i < words; i++) {
		*p = (ul)p;
		p++;
#ifdef UPDATE_PROGRESS
		if ((i & 0xffff) == 0)
			sprd_ddr_memtest_progress_step(i, size_full, '*');
#endif
	}

	p = base_addr;
	for (i = 0; i < size_full; i++) {
		if (*p != (ul)p) {
			lcd_position_cursor(0, 0);
			dprintf(INFO,"memtest addressing failed: %p contains %lu\n", p, *p);
			lcd_printf("memtest addressing failed: %p contains %lu\n", p, *p);
			return MEMTEST_FAIL;
		}
		p++;
#ifdef UPDATE_PROGRESS
		if ((i & 0xffff) == 0)
			sprd_ddr_memtest_progress_step(i, size_full, '*');
#endif
	}

	return MEMTEST_SUCCESS;
}

void sprd_ddr_memtest_fill_random(ulv *base_addr, size_t bytes)
{
	size_t step = MEMTEST_STEP_BYTE / sizeof(ul);
	size_t words = bytes / sizeof(ul) / 2;
	size_t iwords = words / step;
	register ulv *l1, *l2;
	ul i, j;
#ifdef CONFIG_ARM64
	ull rseed = sprd_ddr_rand64(), rout = 0;
#else
	ul rseed = sprd_ddr_rand32(), rout = 0;
#endif

	for (i = 0; i < step; i++) {
		l1 = base_addr + i;
		l2 = l1 + words;
		for (j = 0; j < iwords; j++) {
			/* get random value */
			rseed ^= rseed >> 12;
#ifdef CONFIG_ARM64
			rseed ^= rseed << 25;
			rseed ^= rseed >> 27;
			rout = rseed * sprd_ddr_rand64();
#else
			rout = rseed * sprd_ddr_rand32();
#endif
			/* fill */
			*l1 = *l2 = (ul)rout;
			l1 += step;
			l2 += step;
#ifdef UPDATE_PROGRESS
			if ((i & 0xffff) == 0)
				sprd_ddr_memtest_progress_step(i * iwords + j, words, '*');
#endif
		}
	}
}

void sprd_ddr_memtest_fill_value(ulv *base_addr, size_t bytes, ul v1, ul v2, char sym)
{
	size_t step = MEMTEST_STEP_BYTE / sizeof(ul);
	size_t words = bytes / sizeof(ul) / 2;
	size_t iwords = words / step;
	ulv *l1, *l2;
	ul i, j, v;

	for (i = 0; i < step; i++) {
		l1 = base_addr + i;
		l2 = l1 + words;
		v = (i & 1) ? v2 : v1;	// alternate filling of v1 and v2
		for (j = 0; j < iwords; j++) {
			*l1 = *l2 = v;
			l1 += step;
			l2 += step;
#ifdef UPDATE_PROGRESS
			if ((i & 0xffff) == 0)
				sprd_ddr_memtest_progress_step(i * iwords + j, words, sym);
#endif
		}
	}
}

int sprd_ddr_memtest_compare(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	register ulv *l1 = base_addr, *l2 = base_addr + words;
	ul i;

	for (i = 0; i < words; i++) {
		if (*l1 != *l2) {
			lcd_position_cursor(0, 0);
			dprintf(INFO,"memtest compare failed: %p != %p (%lu vs %lu)\n", l1, l2, *l1, *l2);
			lcd_printf("memtest compare failed: %p != %p (%lu vs %lu)\n", l1, l2, *l1, *l2);
			return MEMTEST_FAIL;
		}
		l1++;
		l2++;
#ifdef UPDATE_PROGRESS
		if ((i & 0xffff) == 0)
			sprd_ddr_memtest_progress_step(i, words, '=');
#endif
	}

	return MEMTEST_SUCCESS;
}

int sprd_ddr_memtest_compare_times(ulv *base_addr, size_t bytes, int pass, unsigned int times)
{
	int errors = 0;

	while (times--) {
		sprd_ddr_memtest_progress_start("Compare", pass);
		errors += sprd_ddr_memtest_compare(base_addr, bytes);
		sprd_ddr_memtest_progress_end();
	}

	return errors;
}

int sprd_ddr_memtest_addr_walk(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul);
	register ulv *l1, *l2, *le = base_addr + words;
	ul mask1, mask2;
	ul expect, actual;
	ul invert;
	unsigned int j = 0;
#ifdef UPDATE_PROGRESS
	ull progress_size = MEMTEST_ADDR_WALK_ITERATIONS << 1;
#endif

addr_walk_start:
	invert = 0;
	/* walking one on our first address */
	mask1 = sizeof(ul);
	do {
		l1 = (ul*)((ul)base_addr | (ul)mask1);
		mask1 <<= 1;
		if (l1 >= le)
			break;
		expect = invert ^ (ul)l1;
		*l1 = expect;

		/* walking one on our second address */
		mask2 = sizeof(ul);
		do {
			l2 = (ul*)((ul)base_addr | (ul)mask2);
			mask2 <<= 1;
			if (l2 == l1)
				continue;
			if (l2 >= le)
				break;
			*l2 = ~invert ^ (ul)l2;

			actual = *l1;
			if (actual != expect) {
				lcd_position_cursor(0, 0);
				dprintf(INFO,"memtest addrwalk failed: %p (%lu vs %lu) when walking %p\n", l1, actual, expect, l2);
				lcd_printf("memtest addrwalk failed: %p (%lu vs %lu) when walking %p\n", l1, actual, expect, l2);
				return MEMTEST_FAIL;
			}
		} while (mask2);
	} while (mask1);
#ifdef UPDATE_PROGRESS
	sprd_ddr_memtest_progress_step((j << 1) + 1, progress_size, '*');
#endif

	invert = ~invert;
	/* walking one on our first address */
	mask1 = sizeof(ul);
	do {
		l1 = (ul*)((ul)base_addr | (ul)mask1);
		mask1 <<= 1;
		if (l1 >= le)
			break;
		expect = invert ^ (ul)l1;
		*l1 = expect;

		/* walking one on our second address */
		mask2 = sizeof(ul);
		do {
			l2 = (ul*)((ul)base_addr | (ul)mask2);
			mask2 <<= 1;
			if (l2 == l1)
				continue;
			if (l2 >= le)
				break;
			*l2 = ~invert ^ (ul)l2;

			actual = *l1;
			if (actual != expect) {
				lcd_position_cursor(0, 0);
				dprintf(INFO,"memtest addrwalk failed: %p (%lu vs %lu) when walking %p\n", l1, actual, expect, l2);
				lcd_printf("memtest addrwalk failed: %p (%lu vs %lu) when walking %p\n", l1, actual, expect, l2);
				return MEMTEST_FAIL;
			}
		} while (mask2);
	} while (mask1);
#ifdef UPDATE_PROGRESS
	sprd_ddr_memtest_progress_step((j << 1) + 2, progress_size, '*');
#endif

	if (j++ < MEMTEST_ADDR_WALK_ITERATIONS) {
		goto addr_walk_start;
	}

	return MEMTEST_SUCCESS;
}

int sprd_ddr_memtest_mov_inv_fixed(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul);
	register ulv *p = base_addr, *pe = base_addr + words;
	ul i;
#ifdef UPDATE_PROGRESS
	ull progress_size = (MEMTEST_MOV_INV_FIXED_ITERATIONS << 2) + 2;
#endif

	/* step1: fill and test pattern1 following pattern2 */
	while (p < pe) {
		*p = MEMTEST_MOV_INV_FIXED_PATTERN1;
		p++;
	}
	for (i = 0; i < MEMTEST_MOV_INV_FIXED_ITERATIONS; i++) {
		p = base_addr;
		while (p < pe) {
			if (*p != MEMTEST_MOV_INV_FIXED_PATTERN1) {
				lcd_position_cursor(0, 0);
				dprintf(INFO,"memtest movinvfixed failed: %p (act:%lu, exp:%lu)\n", p, *p, MEMTEST_MOV_INV_FIXED_PATTERN1);
				lcd_printf("memtest movinvfixed failed: %p (act:%lu, exp:%lu)\n", p, *p, MEMTEST_MOV_INV_FIXED_PATTERN1);
				return MEMTEST_FAIL;
			}
			*p = MEMTEST_MOV_INV_FIXED_PATTERN2;
			p++;
		}
#ifdef UPDATE_PROGRESS
		sprd_ddr_memtest_progress_step((i << 1) + 1, progress_size, '*');
#endif
		p = pe - 1;
		while (p >= base_addr) {
			if (*p != MEMTEST_MOV_INV_FIXED_PATTERN2) {
				lcd_position_cursor(0, 0);
				dprintf(INFO,"memtest movinvfixed failed: %p (act:%lu, exp:%lu)\n", p, *p, MEMTEST_MOV_INV_FIXED_PATTERN2);
				lcd_printf("memtest movinvfixed failed: %p (act:%lu, exp:%lu)\n", p, *p, MEMTEST_MOV_INV_FIXED_PATTERN2);
				return MEMTEST_FAIL;
			}
			*p = MEMTEST_MOV_INV_FIXED_PATTERN1;
			p--;
		}
#ifdef UPDATE_PROGRESS
		sprd_ddr_memtest_progress_step((i << 2) + 2, progress_size, '*');
#endif
	}

	/* step2: fill and test pattern2 following pattern1 */
	p = base_addr;
	while (p < pe) {
		*p = MEMTEST_MOV_INV_FIXED_PATTERN2;
		p++;
	}
	for (i = 0; i < MEMTEST_MOV_INV_FIXED_ITERATIONS; i++) {
		p = base_addr;
		while (p < pe) {
			if (*p != MEMTEST_MOV_INV_FIXED_PATTERN2) {
				lcd_position_cursor(0, 0);
				dprintf(INFO,"memtest movinvfixed failed: %p (act:%lu, exp:%lu)\n", p, *p, MEMTEST_MOV_INV_FIXED_PATTERN2);
				lcd_printf("memtest movinvfixed failed: %p (act:%lu, exp:%lu)\n", p, *p, MEMTEST_MOV_INV_FIXED_PATTERN2);
				return MEMTEST_FAIL;
			}
			*p = MEMTEST_MOV_INV_FIXED_PATTERN1;
			p++;
		}
#ifdef UPDATE_PROGRESS
		sprd_ddr_memtest_progress_step(((i + MEMTEST_MOV_INV_FIXED_ITERATIONS) << 1) + 2, progress_size, '*');
#endif
		p = pe - 1;
		while (p >= base_addr) {
			if (*p != MEMTEST_MOV_INV_FIXED_PATTERN1) {
				lcd_position_cursor(0, 0);
				dprintf(INFO,"memtest movinvfixed failed: %p (act:%lu, exp:%lu)\n", p, *p, MEMTEST_MOV_INV_FIXED_PATTERN1);
				lcd_printf("memtest movinvfixed failed: %p (act:%lu, exp:%lu)\n", p, *p, MEMTEST_MOV_INV_FIXED_PATTERN1);
				return MEMTEST_FAIL;
			}
			*p = MEMTEST_MOV_INV_FIXED_PATTERN2;
			p--;
		}
#ifdef UPDATE_PROGRESS
		sprd_ddr_memtest_progress_step(((i + MEMTEST_MOV_INV_FIXED_ITERATIONS) << 1) + 3, progress_size, '*');
#endif
	}

	return MEMTEST_SUCCESS;
}

int sprd_ddr_memtest_block_move(ulv *base_addr, size_t bytes)
{
	size_t words_16 = bytes / sizeof(ul) / 16;
	size_t words = bytes / sizeof(ul) / 2;
	register ulv *p = base_addr, *l1, *l2;
	ul pattern1 = 1, pattern2;
	ul i = 0;
	ulv *m, *n = base_addr + words;
#ifdef UPDATE_PROGRESS
	ull progress_size = (words << 1) * (MEMTEST_BLOCK_MOVE_ITERATIONS + 1) + words;
#endif

	if (words < 8) {
		lcd_position_cursor(0, 0);
		dprintf(INFO,"memtest blockmove failed: test length is too small\n");
		lcd_printf("memtest blockmove failed: test length is too small\n");
		return MEMTEST_FAIL;
	} else if ((bytes/sizeof(ul)) & 1) {
		lcd_position_cursor(0, 0);
		dprintf(INFO,"memtest blockmove failed: test length is non-standard\n");
		lcd_printf("memtest blockmove failed: test length is non-standard\n");
		return MEMTEST_FAIL;
	}

	/* step1: fill patterns */
	while (words_16 >= 1) {
		pattern2 = ~pattern1;
		*p++ = pattern1;	//0
		*p++ = pattern1;	//1
		*p++ = pattern1;	//2
		*p++ = pattern1;	//3
		*p++ = pattern2;	//4
		*p++ = pattern2;	//5
		*p++ = pattern1;	//6
		*p++ = pattern1;	//7
		*p++ = pattern1;	//8
		*p++ = pattern1;	//9
		*p++ = pattern2;	//10
		*p++ = pattern2;	//11
		*p++ = pattern1;	//12
		*p++ = pattern1;	//13
		*p++ = pattern2;	//14
		*p++ = pattern2;	//15
		pattern1 = pattern1 << 1 | pattern1 >> (8 * sizeof(ul) - 1);	// rotate left
#ifdef UPDATE_PROGRESS
		if ((i & 0xff) == 0)
			sprd_ddr_memtest_progress_step(i << 4, progress_size, '*');
#endif
		words_16--;
		i++;
	}

	/* step2: move blocks */
	for (i = 0; i < MEMTEST_BLOCK_MOVE_ITERATIONS; i++) {
		for (l1 = base_addr, l2 = n; l1 < n; l1++, l2++) {
			*l2 = *l1;
		}
#ifdef UPDATE_PROGRESS
		sprd_ddr_memtest_progress_step((words << 1) * (i + 1) + words, progress_size, '*');
#endif
		m = base_addr + ((i % 7 + 1) << 1);	// (m-base_addr) from {2,4,6,8,10,12,14}
		for (l1 = m, l2 = n; l1 < n; l1++, l2++) {
			*l1 = *l2;
		}
		for (l1 = base_addr; l1 < m; l1++, l2++) {
			*l1 = *l2;
		}
#ifdef UPDATE_PROGRESS
		sprd_ddr_memtest_progress_step((words << 1) * (i + 2), progress_size, '*');
#endif
	}

	/* step3: check the values of adjacent address */
	for (l1 = base_addr; l1 < n; l1 += 2) {
		if (*l1 != *(l1 + 1)) {
			lcd_position_cursor(0, 0);
			dprintf(INFO,"memtest blockmove failed: %p != %p (%lu vs %lu)\n", l1, l1 + 1, *l1, *(l1 + 1));
			lcd_printf("memtest blockmove failed: %p != %p (%lu vs %lu)\n", l1, l1 + 1, *l1, *(l1 + 1));
			return MEMTEST_FAIL;
		}
	}

	return MEMTEST_SUCCESS;
}

int sprd_ddr_memtest_modulo_n(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul);
	register ulv *p;
	register ulv *pend = base_addr + words;
	ul i;
	unsigned int j = 0;
#ifdef UPDATE_PROGRESS
	ull progress_size = words * MEMTEST_MODULO_N_ITERATIONS + words * MEMTEST_MODULO_N_ITERATIONS / MEMTEST_MODULO_N;
#endif

	if ((ul)pend >= (~0UL - MEMTEST_MODULO_N_PATTERN1))
		pend -= MEMTEST_MODULO_N_PATTERN1;

modulo_n_start:
	p = base_addr + MEMTEST_MODULO_OFFSET;
	i = 0;
	/* step1: fill pattern1 */
	while (p < pend) {
		*p = MEMTEST_MODULO_N_PATTERN1;
		p += MEMTEST_MODULO_N;
		i++;
#ifdef UPDATE_PROGRESS
		if ((i & 0xffff) == 0)
			sprd_ddr_memtest_progress_step(i + j * words + j * words / MEMTEST_MODULO_N, progress_size, '*');
#endif
	}

	/* step2: fill pattern2 */
	p = base_addr;
	i = 0;
	while (p < pend) {
		if (i != MEMTEST_MODULO_OFFSET)
			*p = MEMTEST_MODULO_N_PATTERN2;
		i++;
		p++;
		if (i == MEMTEST_MODULO_N)
			i = 0;
#ifdef UPDATE_PROGRESS
		if (((ul)p & 0xffff) == 0)
			sprd_ddr_memtest_progress_step((p - base_addr) + (j + 1) * words / MEMTEST_MODULO_N + j * words, progress_size, '*');
#endif
	}

	/* step3: check memories with pattern1 */
	p = base_addr + MEMTEST_MODULO_OFFSET;
	i = 0;
	while (p < pend) {
		if (*p != MEMTEST_MODULO_N_PATTERN1) {
			lcd_position_cursor(0, 0);
			dprintf(INFO,"memtest modulo_n failed: %p (act:%lu, exp:%lu)\n", p, *p, MEMTEST_MODULO_N_PATTERN1);
			lcd_printf("memtest modulo_n failed: %p (act:%lu, exp:%lu)\n", p, *p, MEMTEST_MODULO_N_PATTERN1);
			return MEMTEST_FAIL;
		}
		p += MEMTEST_MODULO_N;
		i++;
#ifdef UPDATE_PROGRESS
		if ((i & 0xffff) == 0)
			sprd_ddr_memtest_progress_step((j + 1) * words + j * words / MEMTEST_MODULO_N + i, progress_size, 'c');
#endif
	}

	if (j++ < MEMTEST_MODULO_N_ITERATIONS) {
		goto modulo_n_start;
	}

	return MEMTEST_SUCCESS;
}

void sprd_ddr_readRepeat(ulv *pointer1, ulv *pointer2, ulv *buffer_1,
			 ulv *buffer_2, size_t cnt, int n)
{
	int i = 0;
	int ret = NULL;

	for (i = 0; i < n; i++) {
		ret = (*pointer1 == *pointer2) ? 0 : -1;
		if (ret) {
			compare_times++;
		}
	}
}

int sprd_ddr_compare_regions(ulv *buffer_1, ulv *buffer_2, size_t cnt, ul v_base, ul v_full)
{
	ul i;
	register ulv *pointer1 = buffer_1;
	register ulv *pointer2 = buffer_2;

	for (i = 0; i < cnt; i++, pointer1++, pointer2++) {
		if (*pointer1 != *pointer2) {
			lcd_position_cursor(0, 0);
			dprintf(INFO,"memtest compare failed: %p != %p (%lu vs %lu)\n", pointer1, pointer2, *pointer1, *pointer2);
			lcd_printf("memtest compare failed: %p != %p (%lu vs %lu)\n", pointer1, pointer2, *pointer1, *pointer2);
			return MEMTEST_FAIL;
		}
#ifdef UPDATE_PROGRESS
		if ((i & 0xffff) == 0)
			sprd_ddr_memtest_progress_step(i + v_base, v_full, '=');
#endif
	}

	return MEMTEST_SUCCESS;
}

int sprd_ddr_test_stuck_address(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul);
	register ulv *buffer_1 = base_addr;
	ulv *pointer1 = buffer_1;
	unsigned int j;
	size_t i;
#ifdef UPDATE_PROGRESS
	ull progress_size = (words << 1) * MEMTEST_STUCK_ADDRESS_ITERATIONS;
#endif

	for (j = 0; j < MEMTEST_STUCK_ADDRESS_ITERATIONS; j++) {
		pointer1 = (ulv *) buffer_1;
		for (i = 0; i < words; i++) {
			*pointer1 = ((j + i) & 1) == 0 ? (ul) pointer1 : ~((ul) pointer1);
			pointer1++;
#ifdef UPDATE_PROGRESS
			if ((i & 0xffff) == 0)
				sprd_ddr_memtest_progress_step((j << 1) * words + i, progress_size, '*');
#endif
		}
		pointer1 = (ulv *) buffer_1;
		for (i = 0; i < words; i++, pointer1++) {
			if (*pointer1 != (((j + i) % 2) == 0 ? (ul) pointer1 : ~((ul) pointer1))) {
				lcd_position_cursor(0, 0);
				dprintf(INFO,"memtest stuckaddress failed: %p contains %lu\n", pointer1, *pointer1);
				lcd_printf("memtest stuckaddress failed: %p contains %lu\n", pointer1, *pointer1);
				return MEMTEST_FAIL;
			}
#ifdef UPDATE_PROGRESS
			if ((i & 0xffff) == 0)
				sprd_ddr_memtest_progress_step(((j << 1) + 1) * words + i, progress_size, '*');
#endif
		}
	}

	return MEMTEST_SUCCESS;
}

int sprd_ddr_test_random_value(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	ulv *buffer_1 = base_addr;
	ulv *buffer_2 = base_addr + words;
	register ulv *pointer1 = buffer_1;
	register ulv *pointer2 = buffer_2;
	size_t i;

	for (i = 0; i < words; i++) {
		*pointer1++ = *pointer2++ = sprd_ddr_rand_ul();
#ifdef UPDATE_PROGRESS
		if ((i & 0xffff) == 0)
			sprd_ddr_memtest_progress_step(i, words << 1, '*');
#endif
	}

	return sprd_ddr_compare_regions(buffer_1, buffer_2, words, words, words << 1);
}

int sprd_ddr_test_xor_comparison(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	ulv *buffer_1 = base_addr;
	ulv *buffer_2 = base_addr + words;
	register ulv *pointer1 = buffer_1;
	register ulv *pointer2 = buffer_2;
	size_t i;
	ul val = sprd_ddr_rand_ul();

	for (i = 0; i < words; i++) {
		*pointer1++ ^= val;
		*pointer2++ ^= val;
#ifdef UPDATE_PROGRESS
		if ((i & 0xffff) == 0)
			sprd_ddr_memtest_progress_step(i, words << 1, '*');
#endif
	}

	return sprd_ddr_compare_regions(buffer_1, buffer_2, words, words, words << 1);
}

int sprd_ddr_test_sub_comparison(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	ulv *buffer_1 = base_addr;
	ulv *buffer_2 = base_addr + words;
	register ulv *pointer1 = buffer_1;
	register ulv *pointer2 = buffer_2;
	size_t i;
	ul val = sprd_ddr_rand_ul();

	for (i = 0; i < words; i++) {
		*pointer1++ -= val;
		*pointer2++ -= val;
#ifdef UPDATE_PROGRESS
		if ((i & 0xffff) == 0)
			sprd_ddr_memtest_progress_step(i, words << 1, '*');
#endif
	}

	return sprd_ddr_compare_regions(buffer_1, buffer_2, words, words, words << 1);
}

int sprd_ddr_test_mul_comparison(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	ulv *buffer_1 = base_addr;
	ulv *buffer_2 = base_addr + words;
	register ulv *pointer1 = buffer_1;
	register ulv *pointer2 = buffer_2;
	size_t i;
	ul val = sprd_ddr_rand_ul();

	for (i = 0; i < words; i++) {
		*pointer1++ *= val;
		*pointer2++ *= val;
#ifdef UPDATE_PROGRESS
		if ((i & 0xffff) == 0)
			sprd_ddr_memtest_progress_step(i, words << 1, '*');
#endif
	}

	return sprd_ddr_compare_regions(buffer_1, buffer_2, words, words, words << 1);
}

int sprd_ddr_test_div_comparison(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	ulv *buffer_1 = base_addr;
	ulv *buffer_2 = base_addr + words;
	register ulv *pointer1 = buffer_1;
	register ulv *pointer2 = buffer_2;
	size_t i;
	ul val = sprd_ddr_rand_ul();

	for (i = 0; i < words; i++) {
		if (!val)
			val++;
		*pointer1++ /= val;
		*pointer2++ /= val;
#ifdef UPDATE_PROGRESS
		if ((i & 0xffff) == 0)
			sprd_ddr_memtest_progress_step(i, words << 1, '*');
#endif
	}

	return sprd_ddr_compare_regions(buffer_1, buffer_2, words, words, words << 1);
}

int sprd_ddr_test_or_comparison(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	ulv *buffer_1 = base_addr;
	ulv *buffer_2 = base_addr + words;
	register ulv *pointer1 = buffer_1;
	register ulv *pointer2 = buffer_2;
	size_t i;
	ul val = sprd_ddr_rand_ul();

	for (i = 0; i < words; i++) {
		*pointer1++ |= val;
		*pointer2++ |= val;
#ifdef UPDATE_PROGRESS
		if ((i & 0xffff) == 0)
			sprd_ddr_memtest_progress_step(i, words << 1, '*');
#endif
	}

	return sprd_ddr_compare_regions(buffer_1, buffer_2, words, words, words << 1);
}

int sprd_ddr_test_and_comparison(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	ulv *buffer_1 = base_addr;
	ulv *buffer_2 = base_addr + words;
	register ulv *pointer1 = buffer_1;
	register ulv *pointer2 = buffer_2;
	size_t i;
	ul val = sprd_ddr_rand_ul();

	for (i = 0; i < words; i++) {
		*pointer1++ &= val;
		*pointer2++ &= val;
#ifdef UPDATE_PROGRESS
		if ((i & 0xffff) == 0)
			sprd_ddr_memtest_progress_step(i, words << 1, '*');
#endif
	}

	return sprd_ddr_compare_regions(buffer_1, buffer_2, words, words, words<<1);
}

int sprd_ddr_test_seqinc_comparison(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	ulv *buffer_1 = base_addr;
	ulv *buffer_2 = base_addr + words;
	register ulv *pointer1 = buffer_1;
	register ulv *pointer2 = buffer_2;
	size_t i;
	ul val = sprd_ddr_rand_ul();

	for (i = 0; i < words; i++) {
		*pointer1++ = *pointer2++ = (i + val);
#ifdef UPDATE_PROGRESS
		if ((i & 0xffff) == 0)
			sprd_ddr_memtest_progress_step(i, words << 1, '*');
#endif
	}

	return sprd_ddr_compare_regions(buffer_1, buffer_2, words, words, words << 1);
}

int sprd_ddr_test_solidbits_comparison(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	ulv *buffer_1 = base_addr;
	ulv *buffer_2 = base_addr + words;
	register ulv *pointer1 = buffer_1;
	register ulv *pointer2 = buffer_2;
	unsigned int j;
	ul val;
	size_t i;
#ifdef UPDATE_PROGRESS
	ull progress_size = (words << 1) * MEMTEST_SOLIDBITS_ITERATIONS;
#endif

	for (j = 0; j < MEMTEST_SOLIDBITS_ITERATIONS; j++) {
		val = (j % 2) == 0 ? SPRD_DDR_UL_ONEBITS : 0;
		pointer1 = (ulv *) buffer_1;
		pointer2 = (ulv *) buffer_2;
		for (i = 0; i < words; i++) {
			*pointer1++ = *pointer2++ = (i % 2) == 0 ? val : ~val;
#ifdef UPDATE_PROGRESS
			if ((i & 0xffff) == 0)
				sprd_ddr_memtest_progress_step(i + j * (words << 1), progress_size, '*');
#endif
		}
		if (sprd_ddr_compare_regions(buffer_1, buffer_2, words, j * (words << 1) + words, (words << 1) * MEMTEST_SOLIDBITS_ITERATIONS)) {
			return MEMTEST_FAIL;
		}
	}

	return MEMTEST_SUCCESS;
}

int sprd_ddr_test_checkerboard_comparison(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	ulv *buffer_1 = base_addr;
	ulv *buffer_2 = base_addr + words;
	register ulv *pointer1 = buffer_1;
	register ulv *pointer2 = buffer_2;
	unsigned int j;
	ul val;
	size_t i;
#ifdef UPDATE_PROGRESS
	ull progress_size = (words << 1) * MEMTEST_CHECKERBOARD_ITERATIONS;
#endif

	for (j = 0; j < MEMTEST_CHECKERBOARD_ITERATIONS; j++) {
		val = (j % 2) == 0 ? SPRD_DDR_CHK1 : SPRD_DDR_CHK2;
		pointer1 = (ulv *) buffer_1;
		pointer2 = (ulv *) buffer_2;
		for (i = 0; i < words; i++) {
			*pointer1++ = *pointer2++ = (i % 2) == 0 ? val : ~val;
#ifdef UPDATE_PROGRESS
			if ((i & 0xffff) == 0)
				sprd_ddr_memtest_progress_step(i + j * (words << 1), progress_size, '*');
#endif
		}
		if (sprd_ddr_compare_regions(buffer_1, buffer_2, words, j * (words << 1) + words, (words << 1) * MEMTEST_CHECKERBOARD_ITERATIONS)) {
			return MEMTEST_FAIL;
		}
	}

	return MEMTEST_SUCCESS;
}

int sprd_ddr_test_blockseq_comparison(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	ulv *buffer_1 = base_addr;
	ulv *buffer_2 = base_addr + words;
	register ulv *pointer1 = buffer_1;
	register ulv *pointer2 = buffer_2;
	unsigned int j;
	size_t i;
#ifdef UPDATE_PROGRESS
	ull progress_size = (words << 1) * MEMTEST_BLOCKSEQ_ITERATIONS;
#endif

	for (j = 0; j < MEMTEST_BLOCKSEQ_ITERATIONS; j++) {
		pointer1 = (ulv *) buffer_1;
		pointer2 = (ulv *) buffer_2;
		for (i = 0; i < words; i++) {
			*pointer1++ = *pointer2++ = (ul) SPRD_DDR_UL_BYTE(j);
#ifdef UPDATE_PROGRESS
			if ((i & 0xffff) == 0)
				sprd_ddr_memtest_progress_step(i + j * (words << 1), progress_size, '*');
#endif
		}
		if (sprd_ddr_compare_regions(buffer_1, buffer_2, words, j * (words << 1) + words, (words << 1) * MEMTEST_BLOCKSEQ_ITERATIONS)) {
			return MEMTEST_FAIL;
		}
	}

	return MEMTEST_SUCCESS;
}

int sprd_ddr_test_walkbits0_comparison(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	ulv *buffer_1 = base_addr;
	ulv *buffer_2 = base_addr + words;
	register ulv *pointer1 = buffer_1;
	register ulv *pointer2 = buffer_2;
	unsigned int j;
	size_t i;
#ifdef UPDATE_PROGRESS
	ull progress_size = (words << 1) * (SPRD_DDR_UL_LENGTH << 1);
#endif

	for (j = 0; j < (SPRD_DDR_UL_LENGTH << 1); j++) {
		pointer1 = (ulv *) buffer_1;
		pointer2 = (ulv *) buffer_2;
		for (i = 0; i < words; i++) {
			if (j < SPRD_DDR_UL_LENGTH) { /* Walk it up. */
				*pointer1++ = *pointer2++ = ONE << j;
			} else { /* Walk it back down. */
				*pointer1++ = *pointer2++ = ONE << (SPRD_DDR_UL_LENGTH * 2 - j - 1);
			}
#ifdef UPDATE_PROGRESS
			if ((i & 0xffff) == 0)
				sprd_ddr_memtest_progress_step(i + j * (words << 1), progress_size, '*');
#endif
		}
		if (sprd_ddr_compare_regions(buffer_1, buffer_2, words, j * (words << 1) + words, (words << 1) * (SPRD_DDR_UL_LENGTH << 1))) {
			return MEMTEST_FAIL;
		}
	}

	return MEMTEST_SUCCESS;
}

int sprd_ddr_test_walkbits1_comparison(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	ulv *buffer_1 = base_addr;
	ulv *buffer_2 = base_addr + words;
	register ulv *pointer1 = buffer_1;
	register ulv *pointer2 = buffer_2;
	unsigned int j;
	size_t i;
#ifdef UPDATE_PROGRESS
	ull progress_size = (words << 1) * (SPRD_DDR_UL_LENGTH < 1);
#endif

	for (j = 0; j < (SPRD_DDR_UL_LENGTH << 1); j++) {
		pointer1 = (ulv *) buffer_1;
		pointer2 = (ulv *) buffer_2;
		for (i = 0; i < words; i++) {
			if (j < SPRD_DDR_UL_LENGTH) { /* Walk it up. */
				*pointer1++ = *pointer2++ = SPRD_DDR_UL_ONEBITS ^ (ONE << j);
			} else { /* Walk it back down. */
				*pointer1++ = *pointer2++ = SPRD_DDR_UL_ONEBITS ^ (ONE << (SPRD_DDR_UL_LENGTH * 2 - j - 1));
			}
#ifdef UPDATE_PROGRESS
			if ((i & 0xffff) == 0)
				sprd_ddr_memtest_progress_step(i + j * (words << 1), progress_size, '*');
#endif
		}
		if (sprd_ddr_compare_regions(buffer_1, buffer_2, words, j * (words << 1) + words, (words << 1) * (SPRD_DDR_UL_LENGTH << 1))) {
			return MEMTEST_FAIL;
		}
	}

	return MEMTEST_SUCCESS;
}

int sprd_ddr_test_bitspread_comparison(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	ulv *buffer_1 = base_addr;
	ulv *buffer_2 = base_addr + words;
	register ulv *pointer1 = buffer_1;
	register ulv *pointer2 = buffer_2;
	unsigned int j;
	size_t i;
#ifdef UPDATE_PROGRESS
	ull progress_size = (words << 1) * (SPRD_DDR_UL_LENGTH << 1);
#endif

	for (j = 0; j < SPRD_DDR_UL_LENGTH * 2; j++) {
		pointer1 = (ulv *) buffer_1;
		pointer2 = (ulv *) buffer_2;
		for (i = 0; i < words; i++) {
			if (j < SPRD_DDR_UL_LENGTH) { /* Walk it up. */
				*pointer1++ = *pointer2++ = (i % 2 == 0)
					? (ONE << j) | (ONE << (j + 2))
					: SPRD_DDR_UL_ONEBITS ^ ((ONE << j)
							| (ONE << (j + 2)));
			} else { /* Walk it back down. */
				*pointer1++ = *pointer2++ = (i % 2 == 0)
					? ((ONE << (SPRD_DDR_UL_LENGTH * 2 - 1 - j))
					   | (ONE << (SPRD_DDR_UL_LENGTH * 2 + 1 - j)))
					: SPRD_DDR_UL_ONEBITS ^ (ONE << (SPRD_DDR_UL_LENGTH * 2 - 1 - j)
						| (ONE << (SPRD_DDR_UL_LENGTH * 2 + 1 - j)));
			}
#ifdef UPDATE_PROGRESS
			if ((i & 0xffff) == 0)
				sprd_ddr_memtest_progress_step(i + j * (words << 1), progress_size, '*');
#endif
		}
		if (sprd_ddr_compare_regions(buffer_1, buffer_2, words, j * (words << 1) + words, (words << 1) * (SPRD_DDR_UL_LENGTH << 1))) {
			return MEMTEST_FAIL;
		}
	}

	return MEMTEST_SUCCESS;
}

int sprd_ddr_test_bitflip_comparison(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	ulv *buffer_1 = base_addr;
	ulv *buffer_2 = base_addr + words;
	register ulv *pointer1 = buffer_1;
	register ulv *pointer2 = buffer_2;
	unsigned int j, k;
	ul val;
	size_t i;
#ifdef UPDATE_PROGRESS
	ull progress_size = (words << 1) * (SPRD_DDR_UL_LENGTH << 3);
#endif

	for (k = 0; k < SPRD_DDR_UL_LENGTH; k++) {
		val = ONE << k;
		for (j = 0; j < 8; j++) {
			val = ~val;
			pointer1 = (ulv *) buffer_1;
			pointer2 = (ulv *) buffer_2;
			for (i = 0; i < words; i++) {
				*pointer1++ = *pointer2++ = (i % 2) == 0 ? val : ~val;
#ifdef UPDATE_PROGRESS
				if ((i & 0xffff) == 0)
					sprd_ddr_memtest_progress_step(i + j * (words << 1) + k * (words << 3), progress_size, '*');
#endif
			}
			if (sprd_ddr_compare_regions(buffer_1, buffer_2, words, k * (words << 3) + j * (words << 1) + words, (words << 1) * (SPRD_DDR_UL_LENGTH << 3))) {
				return MEMTEST_FAIL;
			}
		}
	}

	return MEMTEST_SUCCESS;
}

void sprd_ddr_switch_for_tiger(ulv *p)
{
	char *r0;
	char *r1;
	char new_r0[4], new_r1[4];
	ulv *p_r = p;
	int i;

#define copy_byte(__x,__y) do{\
    __y = __x;}while(0)

/*Change method:
SW_original:
Word data:  0x77665544
Address 0x4:    7   6   5   4
Byte data:  0x77    0x66    0x55    0x44

Word data:  0x33221100
Address 0x0:    3   2   1   0
Byte data:  0x33    0x22    0x11    0x00

    |
    |
    V

SW_Swap:
Word data:  0x33772266
Address 0x4:    7   6   5   4
Byte data:  0x33    0x77    0x22    0x66

Word data:  0x11550044
Addrees 0x0:    3   2   1   0
Byte data:  0x11    0x55    0x00    0x44

r0_0 -> new_r0_1
r0_1 -> new_r0_3
r0_2 -> new_r1_1
r0_3 -> new_r1_3
r1_0 -> new_r0_0
r1_1 -> new_r0_2
r1_2 -> new_r1_0
r1_3 -> new_r1_2*/

	/*switch p,p-1*/
	r1 = (char *)(p_r--);
	r0 = (char *)p_r;
	/*dprintf(INFO,"r1=0x%08lx,*r1=0x%08lx\n",r1,*(ulv*)r1);
	dprintf(INFO,"r0=0x%08lx,*r0=0x%08lx\n",r0,*(ulv*)r0);*/
	copy_byte(r0[0],new_r0[1]);
	copy_byte(r0[1],new_r0[3]);
	copy_byte(r0[2],new_r1[1]);
	copy_byte(r0[3],new_r1[3]);
	copy_byte(r1[0],new_r0[0]);
	copy_byte(r1[1],new_r0[2]);
	copy_byte(r1[2],new_r1[0]);
	copy_byte(r1[3],new_r1[2]);
	for(i = 0; i < 4; i++) {
		r0[i] = new_r0[i]; /*write back*/
		r1[i] = new_r1[i];
	}
	/*dprintf(INFO,"r1=0x%08lx,*r1=0x%08lx\n",r1,*(ulv*)r1);
	dprintf(INFO,"r0=0x%08lx,*r0=0x%08lx\n",r0,*(ulv*)r0);*/
}

int sprd_ddr_test_bitflip_comparison_tiger(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	ulv *buffer_1 = base_addr;
	ulv *buffer_2 = base_addr + words;
	register ulv *pointer1 = buffer_1;
	register ulv *pointer2 = buffer_2;
	/*ulv *temp;*/
	unsigned int j, k;
	ul val;
	size_t i;
#ifdef UPDATE_PROGRESS
	ull progress_size = (words << 1) * (SPRD_DDR_UL_LENGTH << 3);
#endif

	//dprintf(INFO,"           ");
	if (sizeof(ulv) != 4) {
		dprintf(INFO,"This test only support for 32bit Architecture");
		return -1;
	}

	for (k = 0; k < SPRD_DDR_UL_LENGTH; k++) {
		val = ONE << k;
		for (j = 0; j < 8; j++) {
			//dprintf(INFO,"\b\b\b\b\b\b\b\b\b\b\b");
			val = ~val;
			//dprintf(INFO,"setting %3u", k * 8 + j);
			pointer1 = (ulv *) buffer_1;
			pointer2 = (ulv *) buffer_2;
			for (i = 0; i < words; i++) {
				*pointer1++ = *pointer2++ = (i % 2) == 0 ? val : ~val;
				if ((i > 0) && ((i % 2) == 1)) {/*switch for tiger*/
					/* *temp = 0x33221100;
					*pointer1 = 0x77665544;
					//dprintf(INFO,"\nbefore switch: 0x%8X 0x%8X\n",*(ulv *)temp,*(ulv *)pointer1);
					//dprintf(INFO,"\nbefore switch pointer1 i=%d : 0x%08lX 0x%08lX\n",i,*(ulv *)(pointer1-1),*(ulv *)pointer1);
					//dprintf(INFO,"\nbefore switch pointer2 i=%d : 0x%08lX 0x%08lX\n",i,*(ulv *)(pointer2-1),*(ulv *)pointer2);*/
					sprd_ddr_switch_for_tiger(pointer1-1);
					sprd_ddr_switch_for_tiger(pointer2-1);
					/*dprintf(INFO,"\nafter switch pointer1 i=%d : 0x%08lX 0x%08lX\n",i,*(ulv *)(pointer1-1),*(ulv *)pointer1);
					dprintf(INFO,"\nafter switch pointer2 i=%d : 0x%08lX 0x%08lX\n",i,*(ulv *)(pointer2-1),*(ulv *)pointer2);
				} else {
					temp = pointer1;*/
				}
#ifdef UPDATE_PROGRESS
				if ((i & 0xffff) == 0)
					sprd_ddr_memtest_progress_step(i + j * (words << 1) + k * (words << 3), progress_size, '*');
#endif
			}
			//dprintf(INFO,"\b\b\b\b\b\b\b\b\b\b\b");
			//dprintf(INFO,"testing %3u", k * 8 + j);
			if (sprd_ddr_compare_regions(buffer_1, buffer_2, words, k * (words << 3) + j * (words << 1) + words, (words << 1) * (SPRD_DDR_UL_LENGTH << 3))) {
				return MEMTEST_FAIL;
			}
		}
	}

	//dprintf(INFO,"\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b");
	return MEMTEST_SUCCESS;
}


#ifdef TEST_NARROW_WRITES
int sprd_ddr_test_8bit_wide_random(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	ulv *buffer_1 = base_addr;
	ulv *buffer_2 = base_addr + words;
	register u8v *pointer1, *t;
	register ulv *pointer2;
	int attempt;
	unsigned int b, j = 0;
	size_t i;

	for (attempt = 0; attempt < 2;  attempt++) {
		if (attempt & 1) {
			pointer1 = (u8v *) buffer_1;
			pointer2 = buffer_2;
		} else {
			pointer1 = (u8v *) buffer_2;
			pointer2 = buffer_1;
		}

		for (i = 0; i < words; i++) {
			t = mword8.bytes;
			*pointer2++ = mword8.val = sprd_ddr_rand_ul();
			for (b = 0; b < SPRD_DDR_UL_LENGTH / 8; b++) {
				*pointer1++ = *t++;
			}
			if (!(i % PROGRESSOFTEN)) {
			}
#ifdef UPDATE_PROGRESS
			if ((i & 0xffff) == 0)
				sprd_ddr_memtest_progress_step(i + attempt * (words << 1), words << 2, '*');
#endif
		}
		if (sprd_ddr_compare_regions(buffer_1, buffer_2, words, attempt * (words << 1) + words, words << 2)) {
			return MEMTEST_FAIL;
		}
	}

	return MEMTEST_SUCCESS;
}

int sprd_ddr_test_16bit_wide_random(ulv *base_addr, size_t bytes)
{
	size_t words = bytes / sizeof(ul) / 2;
	ulv *buffer_1 = base_addr;
	ulv *buffer_2 = base_addr + words;
	register u16v *pointer1, *t;
	register ulv *pointer2;
	int attempt;
	unsigned int b, j = 0;
	size_t i;

	for (attempt = 0; attempt < 2; attempt++) {
		if (attempt & 1) {
			pointer1 = (u16v *) buffer_1;
			pointer2 = buffer_2;
		} else {
			pointer1 = (u16v *) buffer_2;
			pointer2 = buffer_1;
		}

		for (i = 0; i < words; i++) {
			t = mword16.u16s;
			*pointer2++ = mword16.val = sprd_ddr_rand_ul();
			for (b = 0; b < SPRD_DDR_UL_LENGTH / 16; b++) {
				*pointer1++ = *t++;
			}
			if (!(i % PROGRESSOFTEN)) {
			}
#ifdef UPDATE_PROGRESS
			if ((i & 0xffff) == 0)
				sprd_ddr_memtest_progress_step(i + attempt * (words << 1), words << 2, '*');
#endif
		}
		if (sprd_ddr_compare_regions(buffer_1, buffer_2, words, attempt * (words << 1) + words, words << 2)) {
			return MEMTEST_FAIL;
		}
	}

	return MEMTEST_SUCCESS;
}
#endif

void sprd_ddr_readRepeat_tag(ulv *pointer1, ulv *pointer2, ulv *buffer_1,
			     ulv *buffer_2, size_t cnt, int n_repeat)
{
	int i = 0;
	char *pchar = NULL;

	for(i = 0; i < n_repeat; i++)
	{
		pchar = (*pointer1 == *pointer2) ? "=" :"!";
		fprintf("ReadRepeat(%d): 0x%lx %s= 0x%lx \n",
			i, (ul) *pointer1, pchar, (ul) *pointer2);
	}
}

int sprd_ddr_compare_regions_const(ulv *buffer_1, size_t cnt, int value)
{
	ulv *pointer1 = buffer_1;
	size_t i;
	int res = 0;
	for (i = 0; i < cnt; i++, pointer1++)
	{
		if (*pointer1 != value)
		{
			res = -1;
		}
	}

	return res;
}

static struct sprd_memtest_list tests[] = {
	//{ "Addressing", sprd_ddr_memtest_addressing },
	{ "Addr Walk", sprd_ddr_memtest_addr_walk },
	{ "Mov Inv Fixed", sprd_ddr_memtest_mov_inv_fixed },
	{ "Block Move", sprd_ddr_memtest_block_move },
	{ "Modulo N", sprd_ddr_memtest_modulo_n },
	{ "Stuck address", sprd_ddr_test_stuck_address},
	{ "Random Value", sprd_ddr_test_random_value },
	{ "Compare XOR", sprd_ddr_test_xor_comparison },
	{ "Compare SUB", sprd_ddr_test_sub_comparison },
	{ "Compare MUL", sprd_ddr_test_mul_comparison },
	{ "Compare DIV", sprd_ddr_test_div_comparison },
	{ "Compare OR", sprd_ddr_test_or_comparison },
	{ "Compare AND", sprd_ddr_test_and_comparison },
	{ "Compare seqinc", sprd_ddr_test_seqinc_comparison },
	{ "Compare solidbits", sprd_ddr_test_solidbits_comparison },
	{ "Compare checkerboard", sprd_ddr_test_checkerboard_comparison },
	{ "Compare blockseq", sprd_ddr_test_blockseq_comparison },
	{ "Compare walkbits0", sprd_ddr_test_walkbits0_comparison },
	{ "Compare walkbits1", sprd_ddr_test_walkbits1_comparison },
	{ "Compare bitspread", sprd_ddr_test_bitspread_comparison },
	{ "Compare bitflip", sprd_ddr_test_bitflip_comparison },
#ifdef TEST_NARROW_WRITES
	{ "Compare 8bit wide random", sprd_ddr_test_8bit_wide_random },
	{ "Compare 16bit wide random", sprd_ddr_test_16bit_wide_random },
#endif
};

int ddr_mem_test(ulong dram_base, ulong test_len, ulong is_loop)
{
	volatile ulong loop = is_loop;
	ul length;
	int regval;

	sprd_ddr_memtest_prepare();

	length = sizeof(tests)/sizeof(struct sprd_memtest_list);
	for (regval=0; regval < length; regval++) {
		sprd_ddr_memtest_progress_start(tests[regval].name, regval);
		if (tests[regval].sprd_memtest_func((ulv*)dram_base, test_len) != MEMTEST_SUCCESS) {
			dprintf(INFO,"memtest failed in: %s\n", tests[regval].name);
			lcd_printf("memtest failed in: %s\n", tests[regval].name);
			while (loop);
			return -1;
		}
		sprd_ddr_memtest_progress_end();
	}

	return 0;

}

