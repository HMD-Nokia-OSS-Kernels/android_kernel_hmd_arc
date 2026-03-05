/*
 * Copyright (C) 2021 Unisoc Communications Inc.
 */

#ifndef _SPRD_DIV64_H
#define _SPRD_DIV64_H

#include <linux/types.h>

extern uint32_t div64_32(uint64_t *dividend, uint32_t divisor);

/* The unnecessary pointer compare is there
 * to check for type safety (n must be 64bit)
 */
# define do_div(n,base) ({				\
	uint32_t tem_base = (base);			\
	uint32_t tem_rem;					\
	(void)(((typeof((n)) *)0) == ((uint64_t *)0));	\
	if (((n) >> 32) == 0) {			\
		tem_rem = (uint32_t)(n) % tem_base;		\
		(n) = (uint32_t)(n) / tem_base;		\
	} else						\
		tem_rem = div64_32(&(n), tem_base);	\
	tem_rem;						\
 })

/* Wrapper for do_div(). Doesn't modify dividend and returns
 * the result, not reminder.
 */
static inline uint64_t lldiv(uint64_t dividend, uint32_t divisor)
{
	uint64_t sprd_res = dividend;
	do_div(sprd_res, divisor);
	return(sprd_res);
}

#endif /* _SPRD_DIV64_H */
