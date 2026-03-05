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

#ifndef __ARCH_ARM64_SPRD_MMU_H
#define __ARCH_ARM64_SPRD_MMU_H
#include <arch/arm64.h>

#define UL(x)	x##UL

/***************************************************************/

#define VA_ADDR_BITS			(39)	/* 42 bits virtual address */

/*Memory types*/
#define SMT_DEVICE_NGNRNE	0
#define SMT_DEVICE_NGNRE		1
#define SMT_DEVICE_GRE		2
#define SMT_NORMAL_NC		3
#define SMT_NORMAL		4

#define MEMORY_ATTRIBUTES	((0x00 << (SMT_DEVICE_NGNRNE*8)) |	\
				(0x04 << (SMT_DEVICE_NGNRE*8)) |		\
				(0x0c << (SMT_DEVICE_GRE*8)) |		\
				(0x44 << (SMT_NORMAL_NC*8)) |		\
				(UL(0xff) << (SMT_NORMAL*8)))

/*
 * Hardware page table definitions.
 * Level 2 descriptor (PMD).
 */
#define S_PMD_TYPE_MASK		(3 << 0)
#define S_PMD_TYPE_FAULT	(0 << 0)
#define S_PMD_TYPE_TABLE	(3 << 0)
#define S_PMD_TYPE_SECT		(1 << 0)

/* Section */
#define S_PMD_SECT_OUTER_SHARE	(2 << 8)
#define S_PMD_SECT_INNER_SHARE	(3 << 8)
#define S_PMD_SECT_AF		(1 << 10)
#define S_PMD_SECT_NG		(1 << 11)
#define S_PMD_SECT_PXN		(UL(1) << 53)
#define S_PMD_SECT_UXN		(UL(1) << 54)

/*AttrIndx[2:0] */
#define S_PMD_ATTRINDX(t)	((t) << 2)
#define S_PMD_ATTRINDX_MASK		(7 << 2)

/* TCR flags. */
#define S_TCR_T0SZ(x)		((64 - (x)) << 0)
#define S_TCR_IRGN_NC		(0 << 8)
#define S_TCR_IRGN_WBWA		(1 << 8)
#define S_TCR_IRGN_WT		(2 << 8)
#define S_TCR_IRGN_WBNWA		(3 << 8)
#define S_TCR_IRGN_MASK		(3 << 8)
#define S_TCR_ORGN_NC		(0 << 10)
#define S_TCR_ORGN_WBWA		(1 << 10)
#define S_TCR_ORGN_WT		(2 << 10)
#define S_TCR_ORGN_WBNWA		(3 << 10)
#define S_TCR_ORGN_MASK		(3 << 10)
#define S_TCR_SHARED_NON		(0 << 12)
#define S_TCR_SHARED_OUTER	(2 << 12)
#define S_TCR_SHARED_INNER	(3 << 12)
#define S_TCR_TG0_4K		(0 << 14)
#define S_TCR_TG0_64K		(1 << 14)
#define S_TCR_TG0_16K		(2 << 14)
#define S_TCR_EL1_IPS_BITS	(UL(2) << 32)	/* 40 bits physical address */
#define S_TCR_EL2_IPS_BITS	(2 << 16)	/* 40 bits physical address */
#define S_TCR_EL3_IPS_BITS	(2 << 16)	/* 40 bits physical address */

/***************************************************************/

/* PAGE_SHIFT determines the page size */
#undef  PAGE_SIZE
#define PAGE_SHIFT		16
#define PAGE_SIZE		(1 << PAGE_SHIFT)
#define PAGE_MASK		(~(PAGE_SIZE-1))

/*
 * section address mask and size definitions.
 */
#define SECTION_SHIFT		30
#define SECTION_SHIFT_2M	21
#define SECTION_SIZE		(UL(1) << SECTION_SHIFT)
#define SECTION_MASK		(~(SECTION_SIZE-1))


/* PTWs cacheable, inner/outer WBWA and non-shareable */
#define S_TCR_FLAGS		(S_TCR_TG0_4K |		\
				(0x3 << 21)	|	\
				S_PMD_SECT_INNER_SHARE |	\
				S_TCR_ORGN_WBWA |		\
				S_TCR_IRGN_WBWA |		\
				S_TCR_T0SZ(VA_ADDR_BITS))

void set_pgtable_section(u64 *page_table, u64 index,
			 u64 section, u64 memory_type);

static inline void hang(void)
{
	for (;;)
		;
}

static inline void set_ttbr_tcr_mair(int el, u64 table, u64 tcr, u64 attr)
{
	DSB;
	switch(el) {
		case 1:
			ARM64_WRITE_SYSREG(ttbr0_el1, table);
			ARM64_WRITE_SYSREG(tcr_el1, tcr);
			ARM64_WRITE_SYSREG(mair_el1, attr);
			break;

		case 2:
			ARM64_WRITE_SYSREG(ttbr0_el2, table);
			ARM64_WRITE_SYSREG(tcr_el2, tcr);
			ARM64_WRITE_SYSREG(mair_el2, attr);
			break;

		case 3:
			ARM64_WRITE_SYSREG(ttbr0_el3, table);
			ARM64_WRITE_SYSREG(tcr_el3, tcr);
			ARM64_WRITE_SYSREG(mair_el3, attr);
			break;
		default :
			hang();
	}
	ISB;
}
#endif /* __ARCH_ARM64_SPRD_MMU_H */
