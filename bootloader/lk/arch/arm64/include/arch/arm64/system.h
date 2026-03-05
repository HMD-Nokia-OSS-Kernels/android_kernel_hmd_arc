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

#ifndef __ARCH_ARM64_SYSTEM_H
#define __ARCH_ARM64_SYSTEM_H
#include <arch/arm64.h>

/*
 * SCTLR_EL1/SCTLR_EL2/SCTLR_EL3 bits definitions
 */
 /* MMU enable	*/
#define CR_M		(1 << 0)
/* Alignment abort enable	*/
#define CR_A		(1 << 1)
/* Dcache enable		*/
#define CR_C		(1 << 2)
/* Stack Alignment Check Enable*/
#define CR_SA		(1 << 3)
/* Icache enable*/
#define CR_I		(1 << 12)
/* Write Permision Imply XN*/
#define CR_WXN		(1 << 19)
/* Exception (Big) Endian*/
#define CR_EE		(1 << 25)

#define PGTABLE_SIZE	(0x1000)

static inline unsigned int current_el(void)
{
	unsigned int el;
	el = ARM64_READ_SYSREG(CurrentEL)>> 2;

	return el ;
}

static inline unsigned int arm64_get_sctlr(void)
{
	unsigned int el, val;

	/*Get the el mode.*/
	el = current_el();
	switch(el) {
		case 1:
			val = ARM64_READ_SYSREG(sctlr_el1);
			break;
		case 2:
			val = ARM64_READ_SYSREG(sctlr_el2);
			break;
		default :
			val = ARM64_READ_SYSREG(sctlr_el3);
			break;
	}

	return val;
}

static inline void arm64_set_sctlr(unsigned int val)
{
	unsigned int el;

	el = current_el();
	switch(el) {
		case 1:
			ARM64_WRITE_SYSREG(sctlr_el1, val);
			break;
		case 2:
			ARM64_WRITE_SYSREG(sctlr_el2, val);
			break;
		default :
			ARM64_WRITE_SYSREG(sctlr_el3, val);
			break;
	}
}

void __arm64_flush_dcache_all(void);
void __arm64_invalidate_dcache_all(void);
void __arm64_flush_dcache_range(u64 start, u64 end);
void __arm64_invalidate_tlb_all(void);
void __arm64_invalidate_dcache_range(unsigned long start, unsigned long stop);
void __arm64_invalidate_icache_all(void);
int __arm64_flush_l3_cache(void);
#endif /*__ARCH_ARM64_SYSTEM_H*/
