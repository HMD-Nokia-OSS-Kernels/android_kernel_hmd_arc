#ifndef ___SPRD_COMMON_H_
#define ___SPRD_COMMON_H_

#include <lk/debug.h>
#include <sprd_log_point.h>
#include "sprd_div64.h"
#include "delay.h"

#define debugf(x...) dprintf(INFO, x)
#define errorf(x...) dprintf(CRITICAL, x)

#ifndef BUG
#define BUG() do { \
	printf("BUG at %s:%d!\n", __FILE__, __LINE__); \
} while (0)

#define BUG_ON(condition) do { if (condition) BUG(); } while(0)
#endif /* BUG */

#define WARN_ON(x) if (x) {debugf("WARNING in %s line %d\n" \
				  , __FILE__, __LINE__); }

#define WARN(condition, fmt, args...) ({	\
	int ret_warn = !!condition;		\
	if (ret_warn)				\
		dprintf(INFO, fmt, ##args);		\
	ret_warn; })

#define dev_WARN(dev, format, arg...)	debug(format, ##arg)
#define WARN_ON_ONCE(val)		debug("Error %d\n", val)

#if DEBUG
#define _DEBUG  1
#else
#define _DEBUG  0
#endif

#define debug(x...)   dprintf(INFO, x)
#define ROUND(a,b)		(((a) + (b) - 1) & ~((b) - 1))
#define pr_err(fmt, args...) do { dprintf(ALWAYS, "[sprdlk][%s] ", __func__); dprintf(CRITICAL, fmt, ##args); } while (0)
#define pr_info(fmt, args...) do { dprintf(INFO, "[sprdlk][%s] ", __func__); dprintf(INFO, fmt, ##args);} while (0)
#define pr_emerg(fmt, args...) do { dprintf(CRITICAL, "[sprdlk][%s]", __func__); dprintf(ALWAYS, fmt, ##args); } while (0)
#define pr_debug(fmt, args...) do { } while (0)

//arch/arm64/sprd/<soc>/time.c
void udelay(unsigned long usec);
void mdelay(unsigned long msec);

#define MAX_ERRNO 4095

#define ERROR(d, fmt, args...)   do { } while (0)
#define VDBG(d, fmt, args...)    debugf(fmt , ## args)
#define DBG(d, fmt, args...)     debugf(fmt , ## args)

#define pr_fmt(fmt) fmt

#define debug_cond(cond, fmt, args...)          \
        do {                                    \
                if (cond)                       \
                        debugf(pr_fmt(fmt), ##args);    \
        } while (0)

#define dev_err(dev, fmt, args...)		\
	debugf(fmt, ##args)
#define dev_vdbg(dev, fmt, args...) do { } while (0) /* \
		debug(fmt, ##args) */
#define dev_dbg(dev, fmt, args...)		\
		debug(fmt, ##args)
#define printk	debugf
#define printk_once	debugf

#endif
