/*
 * <div64.c> - <Generic C version of 64bit/32bit division and modulo>
 *
 * Copyright (C) 2021 Unisoc Communications Inc.
 * History:
 * 	<2021> <div64.c>
 *	Generic C version of 64bit/32bit division and modulo, with
 *  64bit result and 32bit remainder
 */

#include <sprd_div64.h>
#include <linux/types.h>

uint32_t div64_32(uint64_t *n, uint32_t base)
{
	uint64_t rem = *n;
	uint64_t b = base;
	uint64_t res, d = 1;
	uint32_t high = rem >> 32;

	/* Reduce the thing a bit first */
	res = 0;
	if (high >= base) {
		high /= base;
		res = (uint64_t) high << 32;
		rem -= (uint64_t) (high*base) << 32;
	}

	while ((int64_t)b > 0 && b < rem) {
		b = b+b;
		d = d+d;
	}

	do {
		if (rem >= b) {
			rem -= b;
			res += d;
		}
		b >>= 1;
		d >>= 1;
	} while (d);

	*n = res;
	return rem;
}
