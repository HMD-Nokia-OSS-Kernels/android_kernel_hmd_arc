#ifndef __LINUX_COMPAT_H__
#define __LINUX_COMPAT_H__

#include <malloc.h>
#include <lk/list.h>

#if ARCH_ARM
#define __arch_getb(a)			(*(volatile unsigned char *)(a))
#define __arch_getw(a)			(*(volatile unsigned short *)(a))
#define __arch_getl(a)                  (*(volatile unsigned int *)(a))
#define __arch_getq(a)                  (*(volatile unsigned long long *)(a))

#define __arch_putb(v,a)                (*(volatile unsigned char *)(a) = (v))
#define __arch_putw(v,a)                (*(volatile unsigned short *)(a) = (v))
#define __arch_putl(v,a)                (*(volatile unsigned int *)(a) = (v))
#define __arch_putq(v,a)                (*(volatile unsigned long long *)(a) = (v))

static inline void readsl(unsigned long addr, void *buf, int len)
{
 	uint32_t *rbuf = (uint32_t *)buf;
	while(len--)
		*rbuf++ = __arch_getl(addr);
 }
static inline void readsw(unsigned long addr, void *buf, int len)
{
	uint16_t *rbuf = (uint16_t *)buf;
	while(len--)
        	*rbuf++ = __arch_getw(addr);
	}
static inline void readsb(unsigned long addr, void *buf, int len)
{
	uint8_t *rbuf = (uint8_t *)buf;
	while(len--)
		*rbuf++ = __arch_getb(addr);
}
static inline void writesl(unsigned long addr, const void *buf, int len)
{
	uint32_t *wbuf = (uint32_t *)buf;
	while(len--)
		__arch_putl(*wbuf++, addr);
}
static inline void writesw(unsigned long addr, const void *buf, int len)
{
	uint16_t *wbuf = (uint16_t *)buf;
	while(len--)
		__arch_putw(*wbuf++, addr);
}
static inline void writesb(unsigned long addr, const void *buf, int len)
{
	uint8_t *wbuf = (uint8_t *)buf;
	while(len--)
		__arch_putb(*wbuf++, addr);
}
#endif

#if 0
#define writesl(a, d, s) __raw_writesl((unsigned long)a, d, s)
#define readsl(a, d, s) __raw_readsl((unsigned long)a, d, s)
#define writesw(a, d, s) __raw_writesw((unsigned long)a, d, s)
#define readsw(a, d, s) __raw_readsw((unsigned long)a, d, s)
#define writesb(a, d, s) __raw_writesb((unsigned long)a, d, s)
#define readsb(a, d, s) __raw_readsb((unsigned long)a, d, s)
#endif

#define device_init_wakeup(dev, a) do {} while (0)

#define platform_data device_data

#ifndef wmb
#define wmb()			asm volatile (""   : : : "memory")
#endif

#define msleep(a)	udelay(a * 1000)

/*
 * Map U-Boot config options to Linux ones
 */
#ifdef CONFIG_OMAP34XX
#define CONFIG_SOC_OMAP3430
#endif

#endif /* __LINUX_COMPAT_H__ */
