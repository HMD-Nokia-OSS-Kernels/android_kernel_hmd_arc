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

#include <asm/system.h>
#include <sys/types.h>
#include <lk/debug.h>
#include <asm/types.h>

extern phys_size_t real_ram_size;
__attribute__((noinline))void arm_init_before_mmu(void)
{
	invalidate_dcache_all();
	v7_inval_tlb();
}
void arm_init_domains(void)
{
}

__attribute__((noinline)) void set_pgtable_section_lpae(int section, enum dcache_option option)
{
	u64 *page_table = MEMBASE+MEMSIZE;
	u64 value = TTB_SECT_AP | TTB_SECT_AF;

	value |= ((u64)section << MMU_SECTION_SHIFT);
	value |= option;
	page_table[section] = value;
}

/* Enable MMU and set up virtual memory: use 2M areas */
void mmu_setup(void)
{
	int i,j;
	u32 reg;
	u64 *page_table = (u64 *)(MEMBASE+MEMSIZE);
	u64 *page_table_sec = (u64 *)((MEMBASE + MEMSIZE)+ PGTABLE_SIZE);
	u64 ddr_start_sec = (CONFIG_SYS_SDRAM_BASE >> MMU_SECTION_SHIFT);
	u64 ddr_end_sec = (CONFIG_SYS_SDRAM_BASE >> MMU_SECTION_SHIFT)
		+ (real_ram_size >> MMU_SECTION_SHIFT);

	arm_init_before_mmu();
	//Set  identity-mapping for all 4GB, rw for all
	for (i = 0;
		i < ((4096ULL * 1024 * 1024) >> MMU_SECTION_SHIFT); i++)
		set_pgtable_section_lpae(i, DCACHE_OFF);

	for (j = ddr_start_sec; j < ddr_end_sec; j++) {
		set_pgtable_section_lpae(j , DCACHE_WRITEBACK);
	}

	//Set 4 PTE entries pointing to 1GB page tables
	for (i = 0; i < 4; i++) {
		u64 tpt = (MEMBASE + MEMSIZE) + (4096 * i);
		page_table_sec[i] = tpt | TTB_PAGETABLE;
	}

	reg = TTBCR_EAE;
	reg |= TTBCR_ORGN0_WBNWA | TTBCR_IRGN0_WBNWA;

	if (is_hyp_mode()) {
		/* Set HCTR Register enable LPAE */
		asm volatile("mcr p15, 4, %0, c2, c0, 2" : : "r" (reg) : "memory");
		asm volatile("mcrr p15, 4, %0, %1, c2" : : "r"(page_table_sec), "r"(0)
			: "memory"); /* Set HTTBR0 */
		asm volatile("mcr p15, 4, %0, c10, c2, 0"	: : "r" (MEMORY_ATTRIBUTES)
			: "memory"); /* Set HMAIR */
	} else {
		/* Set TTBCR Register enable LPAE */
		asm volatile("mcr p15, 0, %0, c2, c0, 2" : : "r" (reg) : "memory");
		asm volatile("mcrr p15, 0, %0, %1, c2" : : "r"(page_table_sec), "r"(0)
			: "memory");/* Set 64-bit TTBR0 */
		asm volatile("mcr p15, 0, %0, c10, c2, 0" : : "r" (MEMORY_ATTRIBUTES)
			: "memory");/* Set MAIR */
	}

	/* Set the access control to all-supervisor */
	asm volatile("mcr p15, 0, %0, c3, c0, 0" : : "r" (~0));
	arm_init_domains();

	/* and enable the mmu */
	reg = get_cr();
	cp_delay();
	set_cr(reg | CR_M);
}

int mmu_enabled(void)
{
	//enable mmu for CR_M bit
	return get_cr() & CR_M;
}

void cache_enable(uint32_t cache_bit)
{
	uint32_t reg;

	reg = get_cr();	/* get control reg. */
	cp_delay();
	set_cr(reg | cache_bit);
}

void cache_disable(uint32_t cache_bit)
{
	uint32_t reg;

	reg = get_cr();
	cp_delay();

	if (cache_bit == CR_C) {
		if ((reg & CR_C) != CR_C)
			return;
		cache_bit |= CR_M;
	}
	reg = get_cr();
	cp_delay();
	if (cache_bit == (CR_C | CR_M))
		flush_dcache_all();
	set_cr(reg & ~cache_bit);
}

void icache_enable(void)
{
	cache_enable(CR_I);
}

void icache_disable(void)
{
	cache_disable(CR_I);
}

int icache_status(void)
{
	return (get_cr() & CR_I) != 0;
}

void dcache_enable(void)
{
	cache_enable(CR_C);
}

void enable_caches(void)
{
	icache_enable();
	dcache_enable();
}

void dcache_disable(void)
{
	cache_disable(CR_C);
}

int dcache_status(void)
{
	return (get_cr() & CR_C) != 0;
}
