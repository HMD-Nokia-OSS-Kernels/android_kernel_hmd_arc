#ifndef _LINUX_KERNEL_H
#define _LINUX_KERNEL_H


#include <linux/types.h>
#include <stdlib.h>

#ifndef SIZE_MAX
#define SIZE_MAX       (~(size_t)0)
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define __ALIGN_MASK(x,mask)   (((x)+(mask))&~(mask))
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define DIV_ROUND_CLOSEST(x, divisor)(			\
{							\
	typeof(x) __x = x;				\
	typeof(divisor) __d = divisor;			\
	(((typeof(x))-1) > 0 ||				\
	 ((typeof(divisor))-1) > 0 || (__x) > 0) ?	\
		(((__x) + ((__d) / 2)) / (__d)) :	\
		(((__x) - ((__d) / 2)) / (__d));	\
}							\
)

#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

#define max(x, y) ({				\
	typeof(x) _max1 = (x);			\
	typeof(y) _max2 = (y);			\
	(void) (&_max1 == &_max2);		\
	_max1 > _max2 ? _max1 : _max2; })

#define min_t(type, x, y) ({			\
	type a = (x);			\
	type b = (y);			\
	a < b ? a: b; })

#define max_t(type, x, y) ({			\
	type a = (x);			\
	type b = (y);			\
	a > b ? a: b; })


#endif
