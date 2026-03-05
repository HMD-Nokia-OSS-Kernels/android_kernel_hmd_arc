/*
 * <sprd_bitops.h>
 * Copyright (C) 2021 Unisoc Communications Inc.
 * History:
 * 	<2021> <sprd_bitops.h>
 *	Create a contiguous bitmask starting at bit position.
 */

#ifndef __SPRD_BITOPS_H
#define __SPRD_BITOPS_H

#define BIT_MASK(nr)		(1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)

#define __set_bit generic_set_bit

static inline void generic_set_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

	*p  |= mask;
}

static inline int test_bit(int nr, const void * addr)
{
    return ((unsigned char *) addr)[nr >> 3] & (1U << (nr & 7));
}

#endif
