/**
 * io.h - DesignWare USB3 DRD IO Header
 */
#ifndef __DRIVERS_USB_SPRD_DWC3_IO_H
#define __DRIVERS_USB_SPRD_DWC3_IO_H

#include <lk/reg.h>
#define CONFIG_SYS_CACHELINE_SIZE 64
#define	CACHELINE_SIZE		CONFIG_SYS_CACHELINE_SIZE

extern void
invalidate_dcache_range(unsigned long start, unsigned long end );
extern void
flush_dcache_range(unsigned long start , unsigned long end);

static inline u32 dwc3_readl(void __iomem *base, u32 offset)
{
	u32 ofs = offset - SPRD_DWC3_GLOBALS_REGS_START;
	u32 value = 0;
	value = readl(base + ofs);
	return value;
}

static inline void dwc3_writel(void __iomem *base, u32 offset, u32 value)
{
	u32 ofs = offset - SPRD_DWC3_GLOBALS_REGS_START;
	writel(value, base + ofs);
}

static inline void dwc3_flush_cache(unsigned long addr, int size, int dir)
{
	if(dir) {
		flush_dcache_range(addr, addr + size);
	} else {
		invalidate_dcache_range(addr, addr + size);
	}
}
#endif /* __DRIVERS_USB_SPRD_DWC3_IO_H */
