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

#include <asm/types.h>
#include <arch/arm64/system.h>
#include <arch/arm64/sprd_mmu.h>
#include <arch/sprd_cache.h>
#include <lk/debug.h>

#ifndef SPRD_SYS_DCACHE_OFF
void set_pgtable_section(u64 *page_table, u64 index, u64 section,
			 u64 memory_type)
{
	u64 value;

	value = section | S_PMD_TYPE_SECT | S_PMD_SECT_AF;

	if (SMT_DEVICE_NGNRNE == memory_type)
		value |= S_PMD_SECT_PXN | S_PMD_SECT_UXN;
#if WITH_SMP
	else
		value |= S_PMD_SECT_INNER_SHARE;
#endif
	value |= S_PMD_ATTRINDX(memory_type);
	page_table[index] = value;
}

static void set_whole_ddr_pgtable_section(u64 *page_table, u64 index, u64 section)
{
	u64 value;

	value = section | S_PMD_TYPE_SECT | S_PMD_SECT_AF;
#if WITH_SMP
	value |= S_PMD_SECT_INNER_SHARE;
#endif
	value |= S_PMD_SECT_PXN | S_PMD_SECT_UXN;
	value |= S_PMD_ATTRINDX(SMT_NORMAL);
	page_table[index] = value;
}

/* to activate the MMU we need to set up virtual memory */
extern phys_size_t real_ram_size;
static void mmu_setup(void)
{
	u32 i, j, el;
	u64 bootloader_start = MEMBASE;
	u64 bootloader_end = MEMBASE+MEMSIZE + 0x00100000;
	u64 ddr_start = PHYS_SDRAM_1;
	u64 ddr_end = PHYS_SDRAM_1 + real_ram_size;
	u64 *page_table = (u64 *) (MEMBASE+MEMSIZE);
	u64 *page_table_sec = (u64 *) ((MEMBASE + MEMSIZE) + 0x1000);

	/* Setup an identity-mapping for all spaces */
	for (i = 0; i < (PGTABLE_SIZE >> 3); i++) {
		set_pgtable_section(page_table, i, (u64)i << SECTION_SHIFT,
				    SMT_DEVICE_NGNRNE);
	}

	/* Setup an identity-mapping for all RAM space */
	if (ddr_end % (1 << SECTION_SHIFT))
		ddr_end = ddr_end +(1 << SECTION_SHIFT);

	for (j = ddr_start >> SECTION_SHIFT;
			j < ddr_end >> SECTION_SHIFT; j++) {
		if (j == ddr_start >> SECTION_SHIFT)
			set_whole_ddr_pgtable_section(page_table, j, (u64)(page_table_sec) | 0x2);
		else
			set_whole_ddr_pgtable_section(page_table, j, (u64)j << SECTION_SHIFT);
	}

	/* 2nd page setup */
	for (j = 0; j < 0x40000000 >> SECTION_SHIFT_2M; j++) {
		set_whole_ddr_pgtable_section(page_table_sec, j,
			((((u64)j) << SECTION_SHIFT_2M) + ddr_start));
	}

	/* Setup an identity-mapping for UBOOT space */
	if (bootloader_end % (1 << SECTION_SHIFT_2M))
	{
		bootloader_end = bootloader_end + (1 << SECTION_SHIFT_2M);
	}

	for (j = (bootloader_start - ddr_start) >> SECTION_SHIFT_2M;
			j < (bootloader_end - ddr_start) >> SECTION_SHIFT_2M; j++) {
		set_pgtable_section(page_table_sec, j,
			((((u64)j) << SECTION_SHIFT_2M) + ddr_start), SMT_NORMAL);
	}

	/* load TTBR0 */
	el = current_el();
	switch (el ) {
		case 1:
			set_ttbr_tcr_mair(el, (u64)page_table,
				  S_TCR_FLAGS | S_TCR_EL1_IPS_BITS, MEMORY_ATTRIBUTES);
			break;
		case 2:
			set_ttbr_tcr_mair(el, (u64)page_table,
				  S_TCR_FLAGS | S_TCR_EL2_IPS_BITS, MEMORY_ATTRIBUTES);
			break;
		default :
			set_ttbr_tcr_mair(el, (u64)page_table,
				  S_TCR_FLAGS | S_TCR_EL3_IPS_BITS, MEMORY_ATTRIBUTES);
			break;
	}

	arm64_set_sctlr(arm64_get_sctlr() | CR_M);	/* set sctlr_el2 for enable the mmu */
}

/*
 * Invalidation data cache at all levels
 */
void invalidate_dcache_all(void)
{
	__arm64_invalidate_dcache_all();
}

/*
 * Clean & invalidation data cache at all levels.
 */
inline void flush_dcache_all(void)
{
	int ret;

	__arm64_flush_dcache_all();
	ret = __arm64_flush_l3_cache();
	if (ret)
		dprintf(INFO, "flushing dcache returns 0x%x\n", ret);
}

/*
 * Invalidates D-cache/unified cache at all levels.
 */
void invalidate_dcache_range(unsigned long start, unsigned long stop)
{
	__arm64_invalidate_dcache_range(start, stop);
}

/*
 * Flush range(clean & invalidate) D-cache/unified cache at all levels
 */
void flush_dcache_range(unsigned long start, unsigned long stop)
{
	__arm64_flush_dcache_range(start, stop);
}

void dcache_enable(void)
{
	if (!(arm64_get_sctlr() & CR_M)) {
		invalidate_dcache_all();
		__arm64_invalidate_tlb_all();
		mmu_setup();
	}

	/*enable Dcache*/
	arm64_set_sctlr(arm64_get_sctlr() | CR_C);
}

void dcache_disable(void)
{
	uint32_t sctlr;

	sctlr = arm64_get_sctlr();

	if (!(sctlr & CR_C))
		return;

	/*Disable mmu and Dcache*/
	arm64_set_sctlr(sctlr & ~(CR_C|CR_M));

	flush_dcache_all();
	__arm64_invalidate_tlb_all();

}

int dcache_status(void)
{
	/*Get Dcache status*/
	return (arm64_get_sctlr() & CR_C) != 0;
}

void invalid_dcache_range(unsigned long start, unsigned long stop)
{
	__arm64_invalidate_dcache_range(start, stop);
}

#else	/* SPRD_SYS_DCACHE_OFF */

void invalidate_dcache_all(void)
{
}

void flush_dcache_all(void)
{
}

void invalidate_dcache_range(unsigned long start, unsigned long stop)
{
}

void flush_dcache_range(unsigned long start, unsigned long stop)
{
}

void dcache_enable(void)
{
}

void dcache_disable(void)
{
}

int dcache_status(void)
{
	return 0;
}

void invalid_dcache_range(unsigned long start, unsigned long stop)
{
}

#endif	/* SPRD_SYS_DCACHE_OFF */

#ifndef SPRD_SYS_ICACHE_OFF

void icache_enable(void)
{
	__arm64_invalidate_icache_all();
	arm64_set_sctlr(arm64_get_sctlr() | CR_I);
}

void icache_disable(void)
{
	arm64_set_sctlr(arm64_get_sctlr() & ~CR_I);
}

int icache_status(void)
{
	return (arm64_get_sctlr() & CR_I) != 0;
}

void invalidate_icache_all(void)
{
	__arm64_invalidate_icache_all();
}

#else	/* SPRD_SYS_ICACHE_OFF */

void icache_enable(void)
{
}

void icache_disable(void)
{
}

int icache_status(void)
{
	return 0;
}

void invalidate_icache_all(void)
{
}

#endif	/* SPRD_SYS_ICACHE_OFF */

/*
 * Enable dCache & iCache, whether cache is actually enabled
 * depend on SPRD_SYS_DCACHE_OFF and SPRD_SYS_ICACHE_OFF
 */
void enable_caches(void)
{
	icache_enable();
	dcache_enable();
}

#if WITH_SMP
void secondary_cache_enable(void)
{
	u64 *page_table = (u64 *) (MEMBASE+MEMSIZE);
	u32 el;

	icache_enable();
	__arm64_invalidate_tlb_all();
	/* load TTBR0 */
	el = current_el();
	switch (el) {
		case 1:
			set_ttbr_tcr_mair(el, (u64)page_table,
				  S_TCR_FLAGS | S_TCR_EL1_IPS_BITS, MEMORY_ATTRIBUTES);
			break;
		case 2:
			set_ttbr_tcr_mair(el, (u64)page_table,
				  S_TCR_FLAGS | S_TCR_EL2_IPS_BITS, MEMORY_ATTRIBUTES);
			break;
		default :
			set_ttbr_tcr_mair(el, (u64)page_table,
				  S_TCR_FLAGS | S_TCR_EL3_IPS_BITS, MEMORY_ATTRIBUTES);
			break;
	}

	arm64_set_sctlr(arm64_get_sctlr() | CR_M);	/* set sctlr_el2 for enable the mmu */
	arm64_set_sctlr(arm64_get_sctlr() | CR_C);
}
#endif

/*
 * Flush range from all levels of d-cache/unified-cache
 */
void flush_cache(unsigned long start, unsigned long size)
{
	flush_dcache_range(start, start + size);
}

void invalid_cache(unsigned long start, unsigned long size)
{
	invalidate_dcache_range(start, start + size);
}
