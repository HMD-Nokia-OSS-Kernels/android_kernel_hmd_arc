/*
 * MUSB OTG driver register I/O
 */

#ifndef __MUSB_LK_PLATFORM_ARCH_H__
#define __MUSB_LK_PLATFORM_ARCH_H__

#include <sprd_common.h>
#include <asm/arch/common.h>


#if !defined(CONFIG_ARM) && !defined(CONFIG_SUPERH) \
	&& !defined(CONFIG_AVR32) && !defined(CONFIG_PPC32) \
	&& !defined(CONFIG_PPC64) && !defined(CONFIG_BLACKFIN) \
	&& !defined(CONFIG_MIPS) && !defined(CONFIG_M68K)
static inline void readsl(const void __iomem *addr, void *buf, int len)
{
 	uint32_t *rbuf = (uint32_t *)buf;
	while(len--)
		*rbuf++ = __arch_getl(addr);
}
static inline void readsw(const void __iomem *addr, void *buf, int len)
{
	uint16_t *rbuf = (uint16_t *)buf;
	while(len--)
        	*rbuf++ = __arch_getw(addr);
}
static inline void readsb(const void __iomem *addr, void *buf, int len)
{
	uint8_t *rbuf = (uint8_t *)buf;
	while(len--)
		*rbuf++ = __arch_getb(addr);
}
static inline void writesl(const void __iomem *addr, const void *buf, int len)
{
	uint32_t *wbuf = (uint32_t *)buf;
	while(len--)
		__arch_putl(*wbuf++, addr);
}
static inline void writesw(const void __iomem *addr, const void *buf, int len)
{
	uint16_t *wbuf = (uint16_t *)buf;
	while(len--)
		__arch_putw(*wbuf++, addr);
}
static inline void writesb(const void __iomem *addr, const void *buf, int len)
{
	uint8_t *wbuf = (uint8_t *)buf;
	while(len--)
		__arch_putb(*wbuf++, addr);
}

#endif

/* NOTE:  these offsets are all in bytes */
static inline u16 musb_readw(const void __iomem *addr, unsigned offset)
	{ return readw(addr + offset); }

static inline u32 musb_readl(const void __iomem *addr, unsigned offset)
	{ return readl(addr + offset); }

static inline void musb_writew(void __iomem *addr, unsigned offset, u16 data)
	{ writew(data, addr + offset); }

static inline void musb_writel(void __iomem *addr, unsigned offset, u32 data)
	{ writel(data, addr + offset); }

static inline u8 musb_readb(const void __iomem *addr, unsigned offset)
	{ return readb(addr + offset); }

static inline void musb_writeb(void __iomem *addr, unsigned offset, u8 data)
	{ writeb(data, addr + offset); }
#endif
