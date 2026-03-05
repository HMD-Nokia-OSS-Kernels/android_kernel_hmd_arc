#ifndef __LINUX_COMPILER_H
#error "Please don't include <linux/compiler-gcc.h> directly, include <linux/compiler.h> instead."
#endif

#define sprd_attribute(x)  __attribute__(x)
#define __sprd_aligned(x)            sprd_attribute((aligned(x)))

#if !defined(__weak)
#define __sprd_weak            sprd_attribute((weak))
#define __weak				   __sprd_weak
#endif

#define __sprd___deprecated     sprd_attribute((deprecated))
#define __deprecated			__sprd___deprecated
#ifndef __aligned
#define __aligned(x)			__sprd_aligned(x)
#endif

#if !defined(__packed)
#define __sprd_packed           sprd_attribute((packed))
#define __packed			    __sprd_packed
#endif

