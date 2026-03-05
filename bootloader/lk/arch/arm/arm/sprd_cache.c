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

#include <arch/sprd_cache.h>
#include <sys/types.h>
#include <arch/ops.h>

#define CR_I	(1 << 12)	 /* Icache enable */
#define CR_C	(1 << 2)	/* Dcache enable			*/

extern void arch_invalidate_cache_range(addr_t start, size_t len);
extern void arch_clean_invalidate_cache_range(addr_t start, size_t len);



void invalidate_dcache_range(unsigned long start, unsigned long stop)
{

	size_t size = stop - start;
	arch_invalidate_cache_range(start, size);
}

void flush_dcache_range(unsigned long start, unsigned long stop)
{
	size_t size = stop - start;
	arch_clean_invalidate_cache_range(start, size);
}

extern void v7_invalidate_dcache_all(void);
extern void v7_flush_dcache_all(void);

void invalidate_dcache_all(void)
{
	v7_invalidate_dcache_all();
}

void flush_dcache_all(void)
{
	v7_flush_dcache_all();
}

void invalidate_icache_all(void)
{
	asm volatile ("mcr p15, 0, %0, c7, c5, 0" : : "r" (0));
	asm volatile ("mcr p15, 0, %0, c7, c5, 6" : : "r" (0));
	DSB;
	ISB;
}

void v7_inval_tlb(void)
{
	/* Invalidate entire unified TLB */
	asm volatile ("mcr p15, 0, %0, c8, c7, 0" : : "r" (0));
	/* Invalidate entire data TLB */
	asm volatile ("mcr p15, 0, %0, c8, c6, 0" : : "r" (0));
	/* Invalidate entire instruction TLB */
	asm volatile ("mcr p15, 0, %0, c8, c5, 0" : : "r" (0));
	/* Full system DSB - make sure that the invalidation is complete */
	DSB;
	/* Full system ISB - make sure the instruction stream sees it */
	ISB;
}

/**
 * Register an update to the page tables, and flush the TLB
 *
 * \param start		start address of update in page table
 * \param stop		stop address of update in page table
 */
void mmu_page_table_flush(unsigned long start, unsigned long stop)
{
	flush_dcache_range(start, stop);
	v7_inval_tlb();
}


/*
 * Flush range from all levels of d-cache/unified-cache
 */
void flush_cache(unsigned long start, unsigned long size)
{
	flush_dcache_range(start, start + size);
}

