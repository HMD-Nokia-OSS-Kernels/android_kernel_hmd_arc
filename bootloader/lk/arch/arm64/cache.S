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

#include <lk/asm.h>
#include <arch/arm64/sprd_macro.h>

/*
 * void __asm_flush_dcache_level(level)
 *
 * clean and invalidate one level cache.
 *
 *  x12 <- cache level << 1
 *  x2 <- line length offset
 *  x3 <- number of cache ways - 1
 *  x4 <- number of cache sets - 1
 *  x5 <- bit position of #ways
 *  x1:
 *	0 flush & invalidate
 *	1 invalidate only
 *
 */
FUNCTION(__asm_flush_dcache_level)
	lsl	x12, x0, #1
	msr	csselr_el1, x12
	isb
	mrs	x6, ccsidr_el1
	and	x2, x6, #7
	add	x2, x2, #4
	mov	x3, #0x3ff
	and	x3, x3, x6, lsr #3
	clz	w5, w3
	mov	x4, #0x7fff
	and	x4, x4, x6, lsr #13

loop_set:
	mov	x6, x3
loop_way:
	lsl	x7, x6, x5
	orr	x9, x12, x7
	lsl	x7, x4, x2
	orr	x9, x9, x7
	tbz	w1, #0, 1f
	dc	isw, x9
	b	2f
1:	dc	cisw, x9
2:	subs	x6, x6, #1
	b.ge	loop_way
	subs	x4, x4, #1
	b.ge	loop_set

	ret

/*
 * void __arm64_flush_dcache_all(int invalidate_only)
 *
 * x0:
 * 	:0 flush & invalidate
 * 	:1 invalidate only
 * x15 <- return address
 * clean and invalidate all data cache.
 */
FUNCTION(__asm_dcache_all)
	mov	x1, x0
	dsb	sy
	mrs	x10, clidr_el1
	lsr	x11, x10, #24
	and	x11, x11, #0x7
	cbz	x11, finished
	mov	x15, lr
	mov	x0, #0

loop_level:
	lsl	x12, x0, #1
	add	x12, x12, x0
	lsr	x12, x10, x12
	and	x12, x12, #7
	cmp	x12, #2
	b.lt	skip
	bl	__asm_flush_dcache_level
skip:
	add	x0, x0, #1
	cmp	x11, x0
	b.gt	loop_level

	mov	x0, #0
	msr	csselr_el1, x0
	dsb	sy
	isb
	mov	lr, x15

finished:
	ret

FUNCTION(__arm64_flush_dcache_all)
	mov	x16, lr
	mov	x0, #0
	bl	__asm_dcache_all
	mov	lr, x16
	ret

FUNCTION(__arm64_invalidate_dcache_all)
	mov	x16, lr
	mov	x0, #0xffff
	bl	__asm_dcache_all
	mov	lr, x16
	ret

/*
 * void __arm64_flush_dcache_range(start, end)
 *
 * clean & invalidate data cache in the range
 *
 * x0: start address
 * x1: end address
 */
FUNCTION(__arm64_flush_dcache_range)
	mrs	x3, ctr_el0
	lsr	x3, x3, #16
	and	x3, x3, #0xf
	mov	x2, #4
	lsl	x2, x2, x3		/* cache line size */

	/* x2 <- minimal cache line size in cache system */
	sub	x3, x2, #1
	bic	x0, x0, x3
1:	dc	civac, x0	/* clean & invalidate data or unified cache */
	add	x0, x0, x2
	cmp	x0, x1
	b.lo	1b
	dsb	sy
	ret

/*
 * invalidate data cache in the range
 *
 * x0: start address
 * x1: end address
 */
FUNCTION(__arm64_invalidate_dcache_range)
	mrs	x3, ctr_el0
	lsr	x3, x3, #16
	and	x3, x3, #0xf
	mov	x2, #4
	lsl	x2, x2, x3		/* cache line size */

	/* x2 <- minimal cache line size in cache system */
	sub	x3, x2, #1
	bic	x0, x0, x3
1:	dc	ivac, x0	/* clean & invalidate data or unified cache */
	add	x0, x0, x2
	cmp	x0, x1
	b.lo	1b
	dsb	sy
	ret

/*
 * void __arm64_invalidate_icache_all(void)
 *
 * invalidate all tlb entries.
 */
FUNCTION(__arm64_invalidate_icache_all)
	ic	ialluis
	isb	sy
	ret

FUNCTION(__arm64_flush_l3_cache)
	mov	x0, #0			/* return status as success */
	ret
