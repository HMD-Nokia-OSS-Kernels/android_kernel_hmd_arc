/*
 * debug define --MUSB OTG driver
 */

#ifndef __MUSB_LK_DEBUG_H__
#define __MUSB_LK_DEBUG_H__

#define yprintk(facility, format, args...) \
	do { printk(facility "%s %d: " format , \
	__func__, __LINE__ , ## args); } while (0)
#define WARNING(fmt, args...) yprintk(KERN_WARNING, fmt, ## args)
//#define INFO(fmt, args...) yprintk(KERN_INFO, fmt, ## args)
#define ERR(fmt, args...) yprintk(KERN_ERR, fmt, ## args)

#endif	/* __MUSB_LK_DEBUG_H__ */
