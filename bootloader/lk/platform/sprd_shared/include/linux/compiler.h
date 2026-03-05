#ifndef __LINUX_COMPILER_H
#define __LINUX_COMPILER_H

#define __sprd_null

#ifdef __CHECKER__
# define __sprd_user __attribute__((noderef, address_space(1)))
# define __sprd_force    __attribute__((force))
# define __sprd_iomem    __attribute__((noderef, address_space(2)))
# define __sprd_acquires(x)  __attribute__((context(x,0,1)))
# define __sprd_releases(x)  __attribute__((context(x,1,0)))
# define __user		__sprd_user
# define __force	__sprd_force
# define __iomem	__sprd_iomem
# define __acquires(x)	__sprd_acquires(x)
# define __releases(x)	__sprd_releases(x)
#else
//
# define __user   __sprd_null
# define __force  __sprd_null
# define __iomem  __sprd_null
# define __acquires(x)  __sprd_null
# define __releases(x)  __sprd_null
#endif

#if (!defined(__maybe_unused))
/* unimplemented */
# define __maybe_unused __sprd_null
#endif


#ifdef __KERNEL__

#if defined(__SPRD_YES) || defined(__GNUC__)
#include <linux/compiler-gcc.h>
#endif

#if (!defined(__used))
/* unimplemented */
# define __used __sprd_null
#endif

#endif /* __LINUX_COMPILER_H */

#endif 

