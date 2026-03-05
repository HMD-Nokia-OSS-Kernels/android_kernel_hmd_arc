#ifndef __ASM_SYSTEM_H
#define __ASM_SYSTEM_H

#include <arch/sprd_cache.h>

#define isb() __asm__ __volatile__ ("" : : : "memory")
#define nop() __asm__ __volatile__("mov\tr0,r0\t@ nop\n\t");
#define wfi() __asm__ __volatile__ ("wfi" : : : "memory")

#define TTB_SECT		(1 << 0)
#define TTB_SECT_MAIR(x)	((x & 0x7) << 2)
#define TTB_SECT_XN_MASK	(1ULL << 54)
#define TTB_SECT_AP		(1 << 6)
#define TTB_SECT_AF		(1 << 10)

enum dcache_option {
	DCACHE_OFF = TTB_SECT | TTB_SECT_MAIR(0) | TTB_SECT_XN_MASK,
	DCACHE_WRITETHROUGH = TTB_SECT | TTB_SECT_MAIR(1),
	DCACHE_WRITEBACK = TTB_SECT | TTB_SECT_MAIR(2),
	DCACHE_WRITEALLOC = TTB_SECT | TTB_SECT_MAIR(3),
};


enum {
	MMU_SECTION_SHIFT	= 21, /* 2MB */
	MMU_SECTION_SIZE	= 1 << MMU_SECTION_SHIFT,
};

#define PGTABLE_SIZE		(4096 * 4)
#define TTBCR_EAE		(1 << 31)
#define TTBCR_ORGN0_WBNWA	(3 << 10)
#define TTBCR_IRGN0_WBNWA	(3 << 8)

#define MEMORY_ATTRIBUTES	((0x00 << (0 * 8)) | (0x88 << (1 * 8)) | \
				 (0xFF << (2 * 8)) | (0xff << (3 * 8)))


#define TTB_PAGETABLE		(3 << 0)

static inline unsigned long get_cpsr(void)
{
	unsigned long cpsr;

	asm volatile("mrs %0, cpsr" : "=r"(cpsr): );
	return cpsr;
}

#define ARMSTRINGIFY(x) #x
#define ARMTOSTRING(x) ARMSTRINGIFY(x)

#define ARM_READ_SYSREG(reg) \
({ \
    uint64_t _val; \
    __asm__ volatile("mrs %0," ARMTOSTRING(reg) : "=r" (_val)); \
    _val; \
})

static inline int is_hyp_mode(void)
{
	/* check HYP mode for armv7 lage page*/
	return ((get_cpsr() & 0x1f) == 0x1a);
}

static inline unsigned int get_cr(void)
{
	unsigned int val;

	if (is_hyp_mode())
		asm volatile("mrc p15, 4, %0, c1, c0, 0	@ get CR" : "=r" (val)
			     :
			     : "cc");
	else
		asm volatile("mrc p15, 0, %0, c1, c0, 0	@ get CR" : "=r" (val)
			     :
			     : "cc");

	return val;
}

static inline void set_cr(unsigned int val)
{
	if (is_hyp_mode())
		asm volatile("mcr p15, 4, %0, c1, c0, 0	@ set CR"
			     : : "r" (val) : "cc");
	else
		asm volatile("mcr p15, 0, %0, c1, c0, 0	@ set CR"
			     : : "r" (val) : "cc");
	isb();
}

#define CR_M	(1 << 0)	/* MMU enable */
#define CR_C	(1 << 2)	/* Dcache enable */
#define CR_I	(1 << 12)	/* Icache enable */

static inline void cp_delay (void)
{
	volatile int i;

	/* copro seems to need some delay between reading and writing */
	for (i = 0; i < 100; i++)
		nop();
	asm volatile("" : : : "memory");
}



#endif
