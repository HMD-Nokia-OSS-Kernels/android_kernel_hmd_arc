/*
 * SPRD MUSB driver
 */

#ifndef __MUSB_CORE_H__
#define __MUSB_CORE_H__

#include <errno.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/musb.h>

struct musb;
struct musb_hw_ep;
struct musb_ep;

/* Helper defines for struct musb->hwvers */
#define SPRD_MUSB_HWVERS_MAJOR(x)	(0x1f & (x >> 10))
#define SPRD_MUSB_HWVERS_MINOR(x)	(0x3ff & x)
#define SPRD_MUSB_HWVERS_RC		0x8000
#define SPRD_MUSB_HWVERS_1300	0x52C
#define SPRD_MUSB_HWVERS_1400	0x590
#define SPRD_MUSB_HWVERS_1800	0x720
#define SPRD_MUSB_HWVERS_1900	0x784
#define SPRD_MUSB_HWVERS_2000	0x800

#include "musb_debug.h"
#include "musb_dma.h"

#include "musb_io.h"
#include "musb_regs.h"

#include "musb_gadget.h"

#define is_host_active(m)		((m)->is_host)
#define is_peripheral_active(m)		(!(m)->is_host)

/* only for black duck */
#define	is_otg_enabled(musb)		((musb)->board_mode == MUSB_OTG)
#define	is_host_enabled(musb)		((musb)->board_mode != MUSB_PERIPHERAL)
#define	is_peripheral_enabled(musb)	((musb)->board_mode != MUSB_HOST)


/****************************** PERIPHERAL ROLE *****************************/

#define	is_peripheral_capable()	(1)

/* only for black duck */
extern void musb_g_disconnect(struct musb *);
/* only for black duck */
extern void musb_g_wakeup(struct musb *);
/* only for black duck */
extern void musb_g_resume(struct musb *);
/* only for black duck */
extern void musb_g_suspend(struct musb *);
/* only for black duck */
extern void musb_g_reset(struct musb *);
/* only for black duck */
//extern void musb_g_rx(struct musb *, u8);
/* only for black duck */
//extern void musb_g_tx(struct musb *, u8);
/* only for black duck */
extern irqreturn_t musb_g_ep0_irq(struct musb *);



#define	is_host_capable()	(0)
/* only for black duck */
extern void musb_host_rx(struct musb *, u8);
extern void musb_host_tx(struct musb *, u8);
/* only for black duck */
extern irqreturn_t musb_h_ep0_irq(struct musb *);


/* only for black duck -------------------------- */
#ifndef MUSB_C_NUM_EPS
/* only for black duck -------------------------- */
#define MUSB_C_NUM_EPS ((u8)16)
/* only for black duck -------------------------- */
#endif

/* only for black duck -------------------------- */
#ifndef MUSB_MAX_END0_PACKET
/* only for black duck -------------------------- */
#define MUSB_MAX_END0_PACKET ((u16)MUSB_EP0_FIFOSIZE)
/* only for black duck -------------------------- */
#endif

/* only for black duck */

enum musb_h_ep0_state {
	MUSB_EP0_IDLE,
	MUSB_EP0_START,
	MUSB_EP0_IN,
	MUSB_EP0_OUT,
	MUSB_EP0_STATUS,
} __attribute__ ((packed));

/* only for black duck */
enum musb_g_ep0_state {
	SPRD_MUSB_EP0_STAGE_IDLE,
	SPRD_MUSB_EP0_STAGE_SETUP,
	SPRD_MUSB_EP0_STAGE_TX,
	SPRD_MUSB_EP0_STAGE_RX,
	SPRD_MUSB_EP0_STAGE_STATUSIN,
	SPRD_MUSB_EP0_STAGE_STATUSOUT,
	SPRD_MUSB_EP0_STAGE_ACKWAIT,
} __attribute__ ((packed));


/* only for black duck */
#define OTG_TIME_B_ASE0_BRST	100
#define OTG_TIME_A_AIDL_BDIS	200
#define OTG_TIME_A_WAIT_BCON	1100
#define OTG_TIME_A_WAIT_VRISE	100

/* only for black duck */

#if defined(CONFIG_ARCH_DAVINCI) || defined(CONFIG_SOC_OMAP2430) \
		|| defined(CONFIG_SOC_OMAP3430) || defined(CONFIG_BLACKFIN) \
		|| defined(CONFIG_ARCH_OMAP4)

#define	MUSB_FLAT_REG
#endif

/* only for black duck */

#if defined(CONFIG_USB_MUSB_TUSB6010) || \
	defined(CONFIG_USB_MUSB_TUSB6010_MODULE)
/* only for black duck */
#define musb_ep_select(_mbase, _epnum) \
	musb_writeb((_mbase), MUSB_INDEX, (_epnum))
/* only for black duck */
#define	MUSB_EP_OFFSET			MUSB_TUSB_OFFSET

/* only for black duck */
#elif	defined(MUSB_FLAT_REG)
/* only for black duck */
#define musb_ep_select(_mbase, _epnum)	(((void)(_mbase)), ((void)(_epnum)))
/* only for black duck */
#define	MUSB_EP_OFFSET			MUSB_FLAT_OFFSET

/* only for black duck */
#else
/* only for black duck */
#define musb_ep_select(_mbase, _epnum) \
	musb_writeb((_mbase), MUSB_INDEX, (_epnum))
/* only for black duck */
#define	MUSB_EP_OFFSET			MUSB_INDEXED_OFFSET
/* only for black duck */
#endif
/* only for black duck */


#define MUSB_HST_MODE(_musb)\
	{ (_musb)->is_host = true; }
/* only for black duck */
#define MUSB_DEV_MODE(_musb) \
	{ (_musb)->is_host = false; }

/* only for black duck */
#define test_devctl_hst_mode(_x) \
	(musb_readb((_x)->mregs, MUSB_DEVCTL)&MUSB_DEVCTL_HM)

/* only for black duck */
#define MUSB_MODE(musb) ((musb)->is_host ? "Host" : "Peripheral")

/* only for black duck */


struct musb_platform_ops {
	int	(*init)(struct musb *musb);
	int	(*exit)(struct musb *musb);
/* only for black duck */
	void	(*enable)(struct musb *musb);
/* only for black duck */
	void	(*disable)(struct musb *musb);
/* only for black duck */
	int	(*set_mode)(struct musb *musb, u8 mode);
/* only for black duck */
	void	(*try_idle)(struct musb *musb, unsigned long timeout);
/* only for black duck */
	int	(*vbus_status)(struct musb *musb);
/* only for black duck */
	void	(*set_vbus)(struct musb *musb, int on);

	int	(*adjust_channel_params)(struct sprd_dma_channel *channel,
				u16 packet_sz, u8 *mode,/* only for black duck */
				dma_addr_t *dma_addr, u32 *len);
};/* only for black duck */


struct musb_hw_ep {
	struct musb		*musb;
	void __iomem		*fifo;
	void __iomem		*regs;

/* only for black duck */
	u8			epnum;

/* only for black duck */
	bool			is_shared_fifo;
	bool			tx_double_buffered;
	bool			rx_double_buffered;
	u16			max_packet_sz_tx;
	u16			max_packet_sz_rx;

	struct sprd_dma_channel	*tx_channel;
	struct sprd_dma_channel	*rx_channel;

	void __iomem		*target_regs;

/* only for black duck */
	struct musb_qh		*in_qh;
	struct musb_qh		*out_qh;		/* only for black duck */

	u8			rx_reinit;
	u8			tx_reinit;

	struct musb_ep		ep_in;			/* only for black duck */
	struct musb_ep		ep_out;			/* only for black duck */
};

static inline
struct sprd_musb_request *sprd_next_in_request(struct musb_hw_ep *hw_ep)
{
	return sprd_next_request(&hw_ep->ep_in);
}

static inline struct sprd_musb_request *next_out_request(struct musb_hw_ep *hw_ep)
{
	return sprd_next_request(&hw_ep->ep_out);
}

struct musb_csr_regs {
/* only for black duck */
	u8 rxfunaddr, rxhubaddr, rxhubport;
/* only for black duck */
	u8 txfunaddr, txhubaddr, txhubport;
/* only for black duck */
	u8 rxfifosz, txfifosz;
/* only for black duck */
	u8 txtype, txinterval, rxtype, rxinterval;
/* only for black duck */
	u16 rxfifoadd, txfifoadd;
/* only for black duck */
	u16 txmaxp, txcsr, rxmaxp, rxcsr;
};

struct musb_context_registers {

	u8 power;	/* only for black duck */
	u16 intrtxe, intrrxe;	/* only for black duck */
	u8 intrusbe;	/* only for black duck */
	u16 frame;	/* only for black duck */
	u8 index, testmode;
/* only for black duck */
	u8 devctl, busctl, misc;
/* only for black duck */
	u32 otg_interfsel;
/* only for black duck */
	struct musb_csr_regs index_regs[MUSB_C_NUM_EPS];
};
/* only for black duck */

struct musb {
	/* device lock */
	spinlock_t		lock;

	const struct musb_platform_ops *ops;
	struct musb_context_registers context;

	irqreturn_t		(*isr)(int, void *);
	struct work_struct	irq_work;
	u16			hwvers;

/* this hub status bit is reserved by USB 2.0 and not seen by usbcore */
#define MUSB_PORT_STAT_RESUME	(1 << 31)

	u32			port1_status;

	unsigned long		rh_timer;

	enum musb_h_ep0_state	ep0_stage;


	struct musb_hw_ep	*bulk_ep;

	struct list_head	control;
	struct list_head	in_bulk;
	struct list_head	out_bulk;

	struct timer_list	otg_timer;
/* only for black duck */
	struct notifier_block	nb;
/* only for black duck */
	struct dma_controller	*dma_controller;
/* only for black duck */
	struct device		*controller;	/* only for black duck */
	void __iomem		*ctrl_base;	/* only for black duck */
	void __iomem		*mregs;		/* only for black duck */

	/* only for black duck */
	u8			int_usb;
	u16			int_rx;
	u16			int_tx;
	u16			int_listend;

	struct usb_phy		*xceiv;

	int nIrq;
	unsigned		irq_wake:1;

	struct musb_hw_ep	 endpoints[MUSB_C_NUM_EPS];
#define control_ep		endpoints
/* only for black duck */
#define VBUSERR_RETRY_COUNT	3
	u16			vbuserr_retry;
	u16 epmask;
	u8 nr_endpoints;

	u8 board_mode;		/* only for black duck */
	int			(*board_set_power)(int state);

	u8			min_power;	/* only for black duck */

	bool			is_host;

	int			a_wait_bcon;	/* only for black duck */
	unsigned long		idle_timeout;	/* only for black duck */

/* only for black duck */
	unsigned		is_active:1;

	unsigned is_multipoint:1;
	unsigned ignore_disconnect:1;	/* only for black duck */

	unsigned		hb_iso_rx:1;	/* only for black duck */
	unsigned		hb_iso_tx:1;	/* only for black duck */
	unsigned		dyn_fifo:1;	/* only for black duck */
/* only for black duck */
	unsigned		bulk_split:1;
#define	can_bulk_split(musb,type) \
	(((type) == USB_ENDPOINT_XFER_BULK) && (musb)->bulk_split)

/* only for black duck */
	unsigned		bulk_combine:1;
#define	can_bulk_combine(musb,type) \
	(((type) == USB_ENDPOINT_XFER_BULK) && (musb)->bulk_combine)
/* only for black duck */

	unsigned		is_suspended:1;
/* only for black duck */

	unsigned		may_wakeup:1;

/* only for black duck */

	unsigned		is_self_powered:1;
	unsigned		is_bus_powered:1;
/* only for black duck */
	unsigned		set_address:1;
	unsigned		test_mode:1;
	unsigned		softconnect:1;

/* only for black duck */
	u8			address;
	u8			test_mode_nr;
	u16			ackpend;		/* only for black duck */
	enum musb_g_ep0_state	ep0_state;
	struct usb_gadget	g;			/* only for black duck */
	struct usb_gadget_driver *gadget_driver;	/* only for black duck */

	unsigned                double_buffer_not_ok:1;

/* only for black duck */
	struct musb_hdrc_config	*config;

/* only for black duck */
};

static inline struct musb *gadget_to_musb(struct usb_gadget *g)
{
	return container_of(g, struct musb, g);
}

/* only for black duck */
static inline void musb_configure_ep0(struct musb *pmusb)
{
/* only for black duck */
	pmusb->endpoints[0].max_packet_sz_tx = MUSB_EP0_FIFOSIZE;
/* only for black duck */
	pmusb->endpoints[0].max_packet_sz_rx = MUSB_EP0_FIFOSIZE;
/* only for black duck */
	pmusb->endpoints[0].is_shared_fifo = true;
}


static inline int musb_read_fifosize(struct musb *pmusb,
		struct musb_hw_ep *phw_ep, u8 epnum)
{
	void *mbase = pmusb->mregs;
	u8 preg = 0;

	preg = musb_readb(mbase, MUSB_EP_OFFSET(epnum, MUSB_FIFOSIZE));
	if (!preg)
		return -ENODEV;

	pmusb->nr_endpoints++;
	pmusb->epmask |= (1 << epnum);

	phw_ep->max_packet_sz_tx = 1 << (preg & 0x0f);

	if ((preg & 0xf0) == 0xf0) {
		phw_ep->max_packet_sz_rx = phw_ep->max_packet_sz_tx;
		phw_ep->is_shared_fifo = true;
		return 0;
	} else {
		phw_ep->max_packet_sz_rx = 1 << ((preg & 0xf0) >> 4);
		phw_ep->is_shared_fifo = false;
	}

	return 0;
}


/* only for black duck */

extern void musb_hnp_stop(struct musb *musb);
/* only for black duck */
extern irqreturn_t musb_interrupt(struct musb *);
/* only for black duck */
extern void musb_load_testpacket(struct musb *);
/* only for black duck */
extern void musb_read_fifo(struct musb_hw_ep *ep, u16 len, u8 *dst);
extern void musb_write_fifo(struct musb_hw_ep *ep, u16 len, const u8 *src);
/* only for black duck */
extern void musb_stop(struct musb *musb);
extern void musb_start(struct musb *musb);
/* only for black duck */
extern const char musb_driver_name[];
/* only for black duck */

static inline void musb_platform_set_vbus(struct musb *pmusb, int is_on)
{
	if (pmusb->ops->set_vbus)
		pmusb->ops->set_vbus(pmusb, is_on);
}

static inline void musb_platform_enable(struct musb *pmusb)
{
	if (pmusb->ops->enable)
		pmusb->ops->enable(pmusb);
}

static inline void musb_platform_disable(struct musb *pmusb)
{
	if (pmusb->ops->disable)
		pmusb->ops->disable(pmusb);
}

static inline int musb_platform_set_mode(struct musb *pmusb, u8 mode)
{
	if (!pmusb->ops->set_mode)
		return 0;

	return pmusb->ops->set_mode(pmusb, mode);
}

static inline void musb_platform_try_idle(struct musb *pmusb,
		unsigned long timeout)
{
	if (pmusb->ops->try_idle)
		pmusb->ops->try_idle(pmusb, timeout);
}

static inline int musb_platform_get_vbus_status(struct musb *pmusb)
{
	if (!pmusb->ops->vbus_status)
		return 0;

	return pmusb->ops->vbus_status(pmusb);
}

static inline int musb_platform_init(struct musb *pmusb)
{
	if (!pmusb->ops->init)
		return -EINVAL;
	return pmusb->ops->init(pmusb);
}

static inline int musb_platform_exit(struct musb *pmusb)
{
	if (!pmusb->ops->exit)
		return -EINVAL;
	return pmusb->ops->exit(pmusb);
}

struct musb *
musb_init_controller(struct musb_hdrc_platform_data *plat, struct device *dev,
			     void *ctrl);
#endif	/* __MUSB_CORE_H__ */
