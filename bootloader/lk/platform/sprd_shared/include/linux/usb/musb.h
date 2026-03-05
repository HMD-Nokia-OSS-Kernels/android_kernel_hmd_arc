/*
 *
 */

#ifndef __LK_USB_MUSB_H
#define __LK_USB_MUSB_H

/*
#ifndef __deprecated
#define __deprecated
#endif
*/

#include <sprd_compat.h>

enum musb_mode {
	MUSB_UNDEFINED = 0,
	MUSB_HOST,		/* host --A device */
	MUSB_PERIPHERAL,	/* device --B device */
	MUSB_OTG
};

enum musb_buf_mode {
	BUF_SINGLE,
	BUF_DOUBLE
} __attribute__ ((packed));

enum musb_fifo_style {
	FIFO_RXTX,
	FIFO_TX,
	FIFO_RX
} __attribute__ ((packed));

struct musb_fifo_cfg {
	u8			hw_ep_num;
	enum musb_fifo_style	style;
	enum musb_buf_mode	mode;
	u16			maxpacket;
};

#define SPRD_MUSB_EP_FIFO(musb_ep, st, md, max_pkt)		\
{						\
	.hw_ep_num	= musb_ep,			\
	.style		= st,			\
	.mode		= md,			\
	.maxpacket	= max_pkt,			\
}

/* musb double fifo */
#define SPRD_MUSB_EP_FIFO_DOUBLE(musb_ep, st, pkt)	\
	SPRD_MUSB_EP_FIFO(musb_ep, st, BUF_DOUBLE, pkt)

/* musb single fifo */
#define SPRD_MUSB_EP_FIFO_SINGLE(musb_ep, st, pkt)	\
	SPRD_MUSB_EP_FIFO(musb_ep, st, BUF_SINGLE, pkt)

struct musb_hdrc_config {
	struct musb_fifo_cfg	*fifo_cfg;
	unsigned		fifo_cfg_size;

	unsigned	multipoint:1;
	unsigned	dyn_fifo:1 __deprecated;

	u8		num_eps;
	u8		dyn_fifo_size;
	u8		ram_bits;
};

struct musb_hdrc_platform_data {
	u8 mode;	/* MUSB_HOST, MUSB_PERIPHERAL, or MUSB_OTG */

	const char *clock;	/* for clk_get() */

	int (*set_vbus)(struct device *dev, int is_on);	/* (HOST or OTG) switch VBUS on/off */

	u8 power;	/* HOST/OTG mA/2 power supplied on (default 8mA) */

	u8 min_power;	/* (PERIPHERAL) mA/2 max power consumed (default 100mA) */

	u8 potpgt;	/* HOST/OTG msec/2 after VBUS on till power good */

	unsigned extvbus:1;	/* HOST/OTG program PHY for external Vbus */

	int (*set_power)(int state); /* Power the device on or off */

	struct musb_hdrc_config	*config; /* MUSB configuration-specific details */

	void *board_data;	/* Architecture specific board data */

	const void *platform_ops;	/* Platform specific struct musb_ops pointer */
};


/*
 * U-Boot specfic stuff
 */
int musb_register(struct musb_hdrc_platform_data *plat, void *bdata,
			void *ctl_regs);

#endif /* __LK_USB_MUSB_H */
