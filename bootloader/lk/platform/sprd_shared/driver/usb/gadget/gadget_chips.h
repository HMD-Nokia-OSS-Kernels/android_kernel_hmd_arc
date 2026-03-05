#ifndef _GADGET_CHIPS_H_
#define _GADGET_CHIPS_H_
/*
 * USB device controllers have lots of quirks.  Use these macros in
 * gadget drivers or other code that needs to deal with them, and which
 * autoconfigures instead of using early binding to the hardware.
 * Ported to U-boot by: Thomas Smits <ts.smits@gmail.com> and
 *                      Remy Bohmer <linux@bohmer.net>
 */
#define	gadget_is_net2280(g)	0
#define	gadget_is_amd5536udc(g)	0
#define	gadget_is_dummy(g)	0

#define	gadget_is_pxa(g)	0

#define	gadget_is_goku(g)	0

#define	gadget_is_sh(g)		0

#define	gadget_is_sa1100(g)	0

#define	gadget_is_lh7a40x(g)	0

#define	gadget_is_mq11xx(g)	0

#define	gadget_is_omap(g)	0

#define	gadget_is_n9604(g)	0

#define	gadget_is_pxa27x(g)	0

#define gadget_is_atmel_usba(g)	0

#define gadget_is_s3c2410(g)    0

#define gadget_is_at91(g)	0

#define gadget_is_imx(g)	0

#define gadget_is_fsl_usb2(g)	0

#define gadget_is_musbhsfc(g)	0

/* Mentor high speed "dual role" controller, in peripheral role */
#define gadget_is_musbhdrc(g)	(!strcmp("musb-hdrc", (g)->name))

#define gadget_is_mpc8272(g)	0

#define	gadget_is_m66592(g)	0

#define	gadget_is_sprd_otg(g)	0

#ifdef CONFIG_USB_SPRD_DWC3_GADGET
#define gadget_is_dwc3(g)	(!strcmp("dwc3-gadget", (g)->name))
#else
#define gadget_is_dwc3(g)	0
#endif

static inline int usb_gadget_controller_number(struct usb_gadget *gadget)
{
	if (gadget_is_musbhdrc(gadget))
		return 0x16;
	else if (gadget_is_dwc3(gadget))
		return 0x30;
	return -ENOENT;
}

#endif
