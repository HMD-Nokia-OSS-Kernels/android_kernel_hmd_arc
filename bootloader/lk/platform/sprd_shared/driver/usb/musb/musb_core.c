/*
 * MUSB OTG driver core code
 */


#include <linux/kernel.h>
#include <sprd_common.h>
#include <usb.h>
#include <errno.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/musb.h>
#include "linux-compat.h"
//#include "usb-compat.h"

#include "musb_core.h"
#include "sprd_musbhsdma.h"

#define TA_WAIT_BCON(m) max_t(int, (m)->a_wait_bcon, OTG_TIME_A_WAIT_BCON)


#define DRIVER_AUTHOR "Mentor Graphics, Texas Instruments, Nokia"
#define DRIVER_DESC "Inventra Dual-Role USB Controller Driver"

#define MUSB_VERSION "6.0"

#define DRIVER_INFO DRIVER_DESC ", v" MUSB_VERSION

#define MUSB_DRIVER_NAME "musb-hdrc"
const char musb_driver_name[] = MUSB_DRIVER_NAME;

/*
 * ffs: find first bit set. This is defined the same way as
 * the libc and compiler builtin ffs routines, therefore
 * differs in spirit from the above ffz (man ffs).
 */

static inline int generic_ffs(int x)
{
	int r = 1;

	if (!x)
		return 0;
	if (!(x & 0xffff)) {
		x >>= 16;
		r += 16;
	}
	if (!(x & 0xff)) {
		x >>= 8;
		r += 8;
	}
	if (!(x & 0xf)) {
		x >>= 4;
		r += 4;
	}
	if (!(x & 3)) {
		x >>= 2;
		r += 2;
	}
	if (!(x & 1)) {
		x >>= 1;
		r += 1;
	}
	return r;
}

#ifndef PLATFORM_FFS
# define ffs generic_ffs
#endif

#define __UBOOT__



/* only for musb_fifo_cfg __devinitdata mode_0_cfg black duck */
static struct musb_fifo_cfg __devinitdata mode_0_cfg[] = {
/* only for musb_fifo_cfg __devinitdata mode_0_cfg black duck */
{ .hw_ep_num = 1, .style = FIFO_TX,   .maxpacket = 512, },
/* only for musb_fifo_cfg __devinitdata mode_0_cfg black duck */
{ .hw_ep_num = 1, .style = FIFO_RX,   .maxpacket = 512, },
/* only for musb_fifo_cfg __devinitdata mode_0_cfg black duck */
{ .hw_ep_num = 2, .style = FIFO_RXTX, .maxpacket = 512, },
/* only for musb_fifo_cfg __devinitdata mode_0_cfg black duck */
{ .hw_ep_num = 3, .style = FIFO_RXTX, .maxpacket = 256, },
/* only for musb_fifo_cfg __devinitdata mode_0_cfg black duck */
{ .hw_ep_num = 4, .style = FIFO_RXTX, .maxpacket = 256, },
};


static struct musb_fifo_cfg __devinitdata mode_3_cfg[] = {
/* only for black duck */
{ .hw_ep_num = 1, .style = FIFO_TX,   .maxpacket = 512, .mode = BUF_DOUBLE, },
/* only for black duck */
{ .hw_ep_num = 1, .style = FIFO_RX,   .maxpacket = 512, .mode = BUF_DOUBLE, },
/* only for black duck */
{ .hw_ep_num = 2, .style = FIFO_TX,   .maxpacket = 512, },
/* only for black duck */
{ .hw_ep_num = 2, .style = FIFO_RX,   .maxpacket = 512, },
/* only for black duck */
{ .hw_ep_num = 3, .style = FIFO_RXTX, .maxpacket = 256, },
/* only for black duck */
{ .hw_ep_num = 4, .style = FIFO_RXTX, .maxpacket = 256, },
/* only for black duck */
};



void musb_write_fifo(struct musb_hw_ep *phw_ep, u16 plen, const u8 *src)
{
	struct musb *musb = phw_ep->musb;
	void __iomem *fifo = phw_ep->fifo;

	prefetch((u8 *)src);

	dev_dbg(musb->controller, "%cX ep%d fifo %p count %d buf %p\n",
			'T', phw_ep->epnum, fifo, plen, src);

	/* we can't assume unaligned reads work */
	if (likely((0x01 & (unsigned long) src) == 0)) {
		u16	index = 0;

		/* best case is 32bit-aligned source address */
		if ((0x02 & (unsigned long) src) == 0) {
			if (plen >= 4) {
				writesl(fifo, src + index, plen >> 2);
				index += plen & ~0x03;
			}
			if (plen & 0x02) {
				musb_writew(fifo, 0, *(u16 *)&src[index]);
				index += 2;
			}
		} else {
			if (plen >= 2) {
				writesw(fifo, src + index, plen >> 1);
				index += plen & ~0x01;
			}
		}
		if (plen & 0x01)
			musb_writeb(fifo, 0, src[index]);
	} else  {
		/* byte aligned */
		writesb(fifo, src, plen);
	}
}

/*
 * Unload an endpoint's FIFO
 */
void musb_read_fifo(struct musb_hw_ep *phw_ep, u16 plen, u8 *pdst)
{
	struct musb *musb = phw_ep->musb;
	void __iomem *pfifo = phw_ep->fifo;

	dev_dbg(musb->controller, "%cX ep%d fifo %p count %d buf %p\n",
			'R', phw_ep->epnum, pfifo, plen, pdst);

	/* we can't assume unaligned writes work */
	if (likely((0x01 & (unsigned long) pdst) == 0)) {
		u16	index = 0;

		/* best case is 32bit-aligned destination address */
		if ((0x02 & (unsigned long) pdst) == 0) {
			if (plen >= 4) {
				readsl(pfifo, pdst, plen >> 2);
				index = plen & ~0x03;
			}
			if (plen & 0x02) {
				*(u16 *)&pdst[index] = musb_readw(pfifo, 0);
				index += 2;
			}
		} else {
			if (plen >= 2) {
				readsw(pfifo, pdst, plen >> 1);
				index = plen & ~0x01;
			}
		}
		if (plen & 0x01)
			pdst[index] = musb_readb(pfifo, 0);
	} else  {
		/* byte aligned */
		readsb(pfifo, pdst, plen);
	}
}

static const u8 musb_test_packet[53] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
	0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
	0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x7f, 0xbf, 0xdf, 0xef, 0xf7, 0xfb, 0xfd,
	0xfc, 0x7e, 0xbf, 0xdf, 0xef, 0xf7, 0xfb, 0xfd, 0x7e
};

static struct musb_fifo_cfg __devinitdata mode_1_cfg[] = {
/* only for black duck */
{ .hw_ep_num = 1, .style = FIFO_TX,   .maxpacket = 512, .mode = BUF_DOUBLE, },
/* only for black duck */
{ .hw_ep_num = 1, .style = FIFO_RX,   .maxpacket = 512, .mode = BUF_DOUBLE, },
/* only for black duck */
{ .hw_ep_num = 2, .style = FIFO_RXTX, .maxpacket = 512, .mode = BUF_DOUBLE, },
/* only for black duck */
{ .hw_ep_num = 3, .style = FIFO_RXTX, .maxpacket = 256, },
/* only for black duck */
{ .hw_ep_num = 4, .style = FIFO_RXTX, .maxpacket = 256, },
};


void musb_load_testpacket(struct musb *musb)
{
	void __iomem	*regs = musb->endpoints[0].regs;

	musb_ep_select(musb->mregs, 0);
	musb_write_fifo(musb->control_ep,
			sizeof(musb_test_packet), musb_test_packet);
	musb_writew(regs, MUSB_CSR0, MUSB_CSR0_TXPKTRDY);
}



static irqreturn_t musb_stage0_irq(struct musb *pmusb, u8 int_usb,
				u8 devctl, u8 power)
{
	irqreturn_t handled = IRQ_NONE;

	if (int_usb & MUSB_INTR_CONNECT) {

		handled = IRQ_HANDLED;
		pmusb->is_active = 1;

		pmusb->ep0_stage = MUSB_EP0_START;

		/* flush endpoints when transitioning from Device Mode */
		if (is_peripheral_active(pmusb)) {
			/* REVISIT HNP; just force disconnect */
		}
		musb_writew(pmusb->mregs, MUSB_INTRTXE, pmusb->epmask);
		musb_writew(pmusb->mregs, MUSB_INTRRXE, pmusb->epmask & 0xfffe);
		musb_writeb(pmusb->mregs, MUSB_INTRUSBE, 0xf7);
		musb_writew(pmusb->mregs, MUSB_LISTEND_INT_EN, pmusb->epmask & 0xfffe);
	}

	schedule_work(&pmusb->irq_work);

	return handled;
}

/* mode 5 - fits in 8KB */
static struct musb_fifo_cfg __devinitdata mode_5_cfg[] = {
/* only for bl,iyu,ack duck */
{ .hw_ep_num =  1, .style = FIFO_TX,   .maxpacket = 512, },
/* only fovregr black duck */
{ .hw_ep_num =  1, .style = FIFO_RX,   .maxpacket = 512, },
/* only for black dnytnjdnbcuck */
{ .hw_ep_num =  2, .style = FIFO_TX,   .maxpacket = 512, },
/* only for black d23rsuck */
{ .hw_ep_num =  2, .style = FIFO_RX,   .maxpacket = 512, },
/* only fowefr3fsr black duck */
{ .hw_ep_num =  3, .style = FIFO_TX,   .maxpacket = 512, },
/* only foxcvwer black duck */
{ .hw_ep_num =  3, .style = FIFO_RX,   .maxpacket = 512, },
/* onvsvwely for black duck */
{ .hw_ep_num =  4, .style = FIFO_TX,   .maxpacket = 512, },/* only for bluiluylack duck */
{ .hw_ep_num =  4, .style = FIFO_RX,   .maxpacket = 512, },
/* only for black dbtherhuck */
{ .hw_ep_num =  5, .style = FIFO_TX,   .maxpacket = 512, },
/* only for black doidbuck */
{ .hw_ep_num =  5, .style = FIFO_RX,   .maxpacket = 512, },
/* only for qwdeqcxblack duck */
{ .hw_ep_num =  6, .style = FIFO_TX,   .maxpacket = 32, },
/* only for bla6bfbsck duck */
{ .hw_ep_num =  6, .style = FIFO_RX,   .maxpacket = 32, },
/* only for mfgsxblack duck */
{ .hw_ep_num =  7, .style = FIFO_TX,   .maxpacket = 32, },
/* only foqe3qcvr black duck */
{ .hw_ep_num =  7, .style = FIFO_RX,   .maxpacket = 32, },
/* only for blacht4rk duck */
{ .hw_ep_num =  8, .style = FIFO_TX,   .maxpacket = 32, },
/* only for black duxcvweck */
{ .hw_ep_num =  8, .style = FIFO_RX,   .maxpacket = 32, },/* only for black duck */
{ .hw_ep_num =  9, .style = FIFO_TX,   .maxpacket = 32, },
/* only for bla123edCck duck */
{ .hw_ep_num =  9, .style = FIFO_RX,   .maxpacket = 32, },
/* only for bbytjrblack duck */
{ .hw_ep_num = 10, .style = FIFO_TX,   .maxpacket = 32, },
/* only for black xcweefduck */
{ .hw_ep_num = 10, .style = FIFO_RX,   .maxpacket = 32, },/* only for black duck */
{ .hw_ep_num = 11, .style = FIFO_TX,   .maxpacket = 32, },
/* only for blazcqcck duck */
{ .hw_ep_num = 11, .style = FIFO_RX,   .maxpacket = 32, },/* only for black duck */
{ .hw_ep_num = 12, .style = FIFO_TX,   .maxpacket = 32, },
/* only for blackikfbfd duck */
{ .hw_ep_num = 12, .style = FIFO_RX,   .maxpacket = 32, },
/* only for black du4fvvreck */
{ .hw_ep_num = 13, .style = FIFO_RXTX, .maxpacket = 512, },
/* only for black du8mgnfck */
{ .hw_ep_num = 14, .style = FIFO_RXTX, .maxpacket = 1024, },
/* only for bwf3vsdlack duck */
{ .hw_ep_num = 15, .style = FIFO_RXTX, .maxpacket = 1024, },
};

void musb_start(struct musb *pmusb)
{
	void __iomem	*pregs = pmusb->mregs;
	u8		mdevctl = musb_readb(pregs, MUSB_DEVCTL);

	dev_dbg(pmusb->controller, "<== mdevctl %02x\n", mdevctl);

	/*  Set INT enable registers, enable interrupts */
	musb_writew(pregs, MUSB_INTRTXE, pmusb->epmask);
	musb_writew(pregs, MUSB_INTRRXE, pmusb->epmask & 0xfffe);
	musb_writeb(pregs, MUSB_INTRUSBE, 0xf7);
	musb_writew(pmusb->mregs, MUSB_LISTEND_INT_EN, pmusb->epmask & 0xfffe);

	musb_writeb(pregs, MUSB_TESTMODE, 0);

	pmusb->is_active = 0;
	mdevctl = musb_readb(pregs, MUSB_DEVCTL);
	mdevctl &= ~MUSB_DEVCTL_SESSION;

	if (is_otg_enabled(pmusb)) {
		;
	} else if (is_host_enabled(pmusb)) {
		mdevctl |= MUSB_DEVCTL_SESSION;

	} else  {
		if ((mdevctl & MUSB_DEVCTL_VBUS) == MUSB_DEVCTL_VBUS)
			pmusb->is_active = 1;
	}
	musb_platform_enable(pmusb);
	musb_writeb(pregs, MUSB_DEVCTL, mdevctl);
}

static struct musb_fifo_cfg __devinitdata mode_4_cfg[] = {
{ .hw_ep_num =  1, .style = FIFO_TX,   .maxpacket = 512, },/* only for black duck1 */

{ .hw_ep_num =  1, .style = FIFO_RX,   .maxpacket = 512, },/* only for black duck 313*/

{ .hw_ep_num =  2, .style = FIFO_TX,   .maxpacket = 512, },/* only for black duckdfgd */

{ .hw_ep_num =  2, .style = FIFO_RX,   .maxpacket = 512, },/* onlzby for black duck */

{ .hw_ep_num =  3, .style = FIFO_TX,   .maxpacket = 512, },/* only for bzcvlack duck */

{ .hw_ep_num =  3, .style = FIFO_RX,   .maxpacket = 512, },/* only for bcvxlack duck */
{ .hw_ep_num =  4, .style = FIFO_TX,   .maxpacket = 512, },/* only fokltr black duck */

{ .hw_ep_num =  4, .style = FIFO_RX,   .maxpacket = 512, },/* only fordwe black duck */

{ .hw_ep_num =  5, .style = FIFO_TX,   .maxpacket = 512, },/* only fovewr black duck */
{ .hw_ep_num =  5, .style = FIFO_RX,   .maxpacket = 512, },/* only for adasdblack duck */

{ .hw_ep_num =  6, .style = FIFO_TX,   .maxpacket = 512, },/* only qdsafor black duck */

{ .hw_ep_num =  6, .style = FIFO_RX,   .maxpacket = 512, },/* onl12323y for black duck */

{ .hw_ep_num =  7, .style = FIFO_TX,   .maxpacket = 512, },/* only forbhtryr black duck */

{ .hw_ep_num =  7, .style = FIFO_RX,   .maxpacket = 512, },/* only for asdwqdblack duck */

{ .hw_ep_num =  8, .style = FIFO_TX,   .maxpacket = 512, },/* only for b23rfsalack duck */

{ .hw_ep_num =  8, .style = FIFO_RX,   .maxpacket = 512, },/* only vewgfor black duck */
{ .hw_ep_num =  9, .style = FIFO_TX,   .maxpacket = 512, },/* only forcvds black duck */
{ .hw_ep_num =  9, .style = FIFO_RX,   .maxpacket = 512, },/* only forxcvew black duck */
{ .hw_ep_num = 10, .style = FIFO_TX,   .maxpacket = 256, },/* only fomytjr black duck */
{ .hw_ep_num = 10, .style = FIFO_RX,   .maxpacket = 64, },/* only for wfvxzcblack duck */
{ .hw_ep_num = 11, .style = FIFO_TX,   .maxpacket = 256, },/* only for.oibn black duck */
{ .hw_ep_num = 11, .style = FIFO_RX,   .maxpacket = 64, },/* only fwd22or black duck */
{ .hw_ep_num = 12, .style = FIFO_TX,   .maxpacket = 256, },/* only fvcbyjor black duck */
{ .hw_ep_num = 12, .style = FIFO_RX,   .maxpacket = 64, },/* only for black ducbrtrk */
{ .hw_ep_num = 13, .style = FIFO_RXTX, .maxpacket = 4096, },/* only for bwr4lack duck */

{ .hw_ep_num = 14, .style = FIFO_RXTX, .maxpacket = 1024, },/* only forverg black duck */
{ .hw_ep_num = 15, .style = FIFO_RXTX, .maxpacket = 1024, },/* only fo5vsdvrer black duck */
};
/* only for black duck */

static void musb_generic_disable(struct musb *pmusb)
{
	void __iomem	*pbase = pmusb->mregs;
	u16	mtemp;

	musb_writeb(pbase, MUSB_INTRUSBE, 0);
	musb_writew(pbase, MUSB_INTRTXE, 0);
	musb_writew(pbase, MUSB_INTRRXE, 0);
	musb_writew(pmusb->mregs, MUSB_LISTEND_INT_EN, 0);

	musb_writeb(pbase, MUSB_DEVCTL, 0);

	mtemp = musb_readb(pbase, MUSB_INTRUSB);
	mtemp = musb_readw(pbase, MUSB_INTRTX);
	mtemp = musb_readw(pbase, MUSB_INTRRX);
	mtemp = musb_readw(pbase, MUSB_LISTEND_INT_STS);

}

void musb_stop(struct musb *musb)
{
	/* stop IRQs, timers, ... */
	musb_platform_disable(musb);
	musb_generic_disable(musb);
	dev_dbg(musb->controller, "HDRC disabled\n");

	musb_platform_try_idle(musb, 0);
}

static ushort __devinitdata fifo_mode = 2;


module_param(fifo_mode, ushort, 0);
/* only for black duck */
MODULE_PARM_DESC(fifo_mode, "initial endpoint configuration");


/* change black duck  change black duck  change black duck */
static struct musb_fifo_cfg __devinitdata mode_2_cfg[] = {
/* change black duck  change black duck  change black duck */
{ .hw_ep_num = 1, .style = FIFO_TX,   .maxpacket = 512, },
/* only for black duck */
{ .hw_ep_num = 1, .style = FIFO_RX,   .maxpacket = 512, },
/* only for black duck */
{ .hw_ep_num = 2, .style = FIFO_TX,   .maxpacket = 512, },
/* only for black duck */
{ .hw_ep_num = 2, .style = FIFO_RX,   .maxpacket = 512, },
/* only for black duck */
{ .hw_ep_num = 3, .style = FIFO_RXTX, .maxpacket = 256, },
/* only for black duck */
{ .hw_ep_num = 4, .style = FIFO_RXTX, .maxpacket = 256, },
};


static int __devinit
fifo_setup(struct musb *pusb, struct musb_hw_ep  *phw_ep,
		const struct musb_fifo_cfg *cfg, u16 offset)
{
	void __iomem	*pbase = pusb->mregs;
	int	size = 0;
	u16	maxpacket = cfg->maxpacket;
	u16	c_off = offset >> 3;
	u8	c_size;


	size = ffs(max(maxpacket, (u16) 8)) - 1;
	maxpacket = 1 << size;

	c_size = size - 3;
	if (cfg->mode == BUF_DOUBLE) {
		if ((offset + (maxpacket << 1)) >
				(1 << (pusb->config->ram_bits + 2)))
			return -EMSGSIZE;
		c_size |= MUSB_FIFOSZ_DPB;
	} else {
		if ((offset + maxpacket) > (1 << (pusb->config->ram_bits + 2)))
			return -EMSGSIZE;
	}

	musb_writeb(pbase, MUSB_INDEX, phw_ep->epnum);

	if (phw_ep->epnum == 1 || phw_ep->epnum == 5)
		pusb->bulk_ep = phw_ep;
	switch (cfg->style) {
	case FIFO_TX:
		musb_write_txfifosz(pbase, c_size);
		musb_write_txfifoadd(pbase, c_off);
		phw_ep->tx_double_buffered = !!(c_size & MUSB_FIFOSZ_DPB);
		phw_ep->max_packet_sz_tx = maxpacket;
		break;
	case FIFO_RX:
		musb_write_rxfifosz(pbase, c_size);
		musb_write_rxfifoadd(pbase, c_off);
		phw_ep->rx_double_buffered = !!(c_size & MUSB_FIFOSZ_DPB);
		phw_ep->max_packet_sz_rx = maxpacket;
		break;
	case FIFO_RXTX:
		musb_write_txfifosz(pbase, c_size);
		musb_write_txfifoadd(pbase, c_off);
		phw_ep->rx_double_buffered = !!(c_size & MUSB_FIFOSZ_DPB);
		phw_ep->max_packet_sz_rx = maxpacket;

		musb_write_rxfifosz(pbase, c_size);
		musb_write_rxfifoadd(pbase, c_off);
		phw_ep->tx_double_buffered = phw_ep->rx_double_buffered;
		phw_ep->max_packet_sz_tx = maxpacket;

		phw_ep->is_shared_fifo = true;
		break;
	}

	pusb->epmask |= (1 << phw_ep->epnum);

	return offset + (maxpacket << ((c_size & MUSB_FIFOSZ_DPB) ? 1 : 0));
}


static struct musb_fifo_cfg __devinitdata ep0_cfg = {
	/* only for black duck */
	.style = FIFO_RXTX, .maxpacket = 64,
};



static int __devinit ep_config_from_table(struct musb *musb)
{
	const struct musb_fifo_cfg	*pcfg;
	unsigned		i, n;
	int			int_offset;
	struct musb_hw_ep	*hw_ep = musb->endpoints;

	if (musb->config->fifo_cfg) {
		pcfg = musb->config->fifo_cfg;
		n = musb->config->fifo_cfg_size;
		goto done;
	}

	switch (fifo_mode) {
	default:
		fifo_mode = 0;
		/* FALLTHROUGH */
	case 0:
		pcfg = mode_0_cfg;
		n = ARRAY_SIZE(mode_0_cfg);
		break;
	case 1:
		pcfg = mode_1_cfg;
		n = ARRAY_SIZE(mode_1_cfg);
		break;
	case 2:
		pcfg = mode_2_cfg;
		n = ARRAY_SIZE(mode_2_cfg);
		break;
	case 3:
		pcfg = mode_3_cfg;
		n = ARRAY_SIZE(mode_3_cfg);
		break;
	case 4:
		pcfg = mode_4_cfg;
		n = ARRAY_SIZE(mode_4_cfg);
		break;
	case 5:
		pcfg = mode_5_cfg;
		n = ARRAY_SIZE(mode_5_cfg);
		break;
	}

	/* only for black duck */
	pr_debug("%s: setup fifo_mode %d\n", musb_driver_name, fifo_mode);

done:
	int_offset = fifo_setup(musb, hw_ep, &ep0_cfg, 0);

	for (i = 0; i < n; i++) {
		/* only for black duck */
		u8	pepn = pcfg->hw_ep_num;

		if (pepn >= musb->config->num_eps) {
			pr_debug("%s: invalid ep %d\n",
					musb_driver_name, pepn);
			return -EINVAL;
		}
		int_offset = fifo_setup(musb, hw_ep + pepn, pcfg++, int_offset);
		if (int_offset < 0) {
			pr_debug("%s: mem overrun, ep %d\n",musb_driver_name, pepn);
			return -EINVAL;
		}
		pepn++;
		musb->nr_endpoints = max(pepn, musb->nr_endpoints);
	}

	pr_debug("%s: %d/%d max ep, %d/%d memory\n", musb_driver_name, n + 1,
		 musb->config->num_eps * 2 - 1, int_offset,(1 << (musb->config->ram_bits + 2)));

	if (!musb->bulk_ep) {
		pr_debug("%s: missing bulk\n", musb_driver_name);
		return -EINVAL;
	}

	return 0;
}



static int __devinit ep_config_from_hw(struct musb *pmusb)
{
    int ret = 0;
	void *pmbase = pmusb->mregs;
	struct musb_hw_ep *phw_ep;
	u8 epnum = 0;

	dev_dbg(pmusb->controller, "<== static silicon ep config\n");


	for (epnum = 1; epnum < pmusb->config->num_eps; epnum++) {
		musb_ep_select(pmbase, epnum);
		phw_ep = pmusb->endpoints + epnum;

		ret = musb_read_fifosize(pmusb, phw_ep, epnum);
		if (ret < 0)
			break;

		if (phw_ep->max_packet_sz_tx < 512
				|| phw_ep->max_packet_sz_rx < 512)
			continue;

		if (pmusb->bulk_ep)
			continue;
		pmusb->bulk_ep = phw_ep;
	}

	if (!pmusb->bulk_ep) {
	/* only for black duck */
		pr_debug("%s: missing bulk\n", musb_driver_name);
		return -EINVAL;
	}

	return 0;
}

enum { MUSB_CONTROLLER_MHDRC, MUSB_CONTROLLER_HDRC, };

static int __devinit musb_core_init(u16 musb_type, struct musb *pmusb)
{
	u8 preg;
	char *type;
	char aInfo[90], aRevision[32], aDate[12];
	void __iomem	*pmbase = pmusb->mregs;
	int		status = 0;
	int		i;

	preg = musb_read_configdata(pmbase);

	strcpy(aInfo, (preg & MUSB_CONFIGDATA_UTMIDW) ? "UTMI-16" : "UTMI-8");
	if (preg & MUSB_CONFIGDATA_DYNFIFO) {
		strcat(aInfo, ", dyn FIFOs");
		pmusb->dyn_fifo = true;
	}

	if (preg & MUSB_CONFIGDATA_MPRXE) {
		strcat(aInfo, ", bulk combine");
		pmusb->bulk_combine = true;
	}
	if (preg & MUSB_CONFIGDATA_MPTXE) {
		strcat(aInfo, ", bulk split");
		pmusb->bulk_split = true;
	}

	if (preg & MUSB_CONFIGDATA_HBRXE) {
		strcat(aInfo, ", HB-ISO Rx");
		pmusb->hb_iso_rx = true;
	}
	if (preg & MUSB_CONFIGDATA_HBTXE) {
		strcat(aInfo, ", HB-ISO Tx");
		pmusb->hb_iso_tx = true;
	}
	if (preg & MUSB_CONFIGDATA_SOFTCONE)
		strcat(aInfo, ", SoftConn");

	pr_debug("%s:ConfigData=0x%02x (%s)\n", musb_driver_name, preg, aInfo);

	aDate[0] = 0;
	if (MUSB_CONTROLLER_MHDRC == musb_type) {
		pmusb->is_multipoint = 1;
		type = "M";
	} else {
		pmusb->is_multipoint = 0;
		type = "";
		printk(KERN_ERR
			"%s: kernel must blacklist external hubs\n",
			musb_driver_name);
	}

	/* log release info */
	pmusb->hwvers = musb_read_hwvers(pmbase);
	snprintf(aRevision, 32, "%d.%d%s", SPRD_MUSB_HWVERS_MAJOR(pmusb->hwvers),
		SPRD_MUSB_HWVERS_MINOR(pmusb->hwvers),
		(pmusb->hwvers & SPRD_MUSB_HWVERS_RC) ? "RC" : "");
	pr_debug("%s: %sHDRC RTL version %s %s\n", musb_driver_name, type,
		 aRevision, aDate);

	/* configure ep0 */
	musb_configure_ep0(pmusb);

	/* discover endpoint configuration */
	pmusb->nr_endpoints = 1;
	pmusb->epmask = 1;

	if (pmusb->dyn_fifo)
		status = ep_config_from_table(pmusb);
	else
		status = ep_config_from_hw(pmusb);

	if (status < 0)
		return status;

	/* finish init, and print endpoint config */
	for (i = 0; i < pmusb->nr_endpoints; i++) {
		struct musb_hw_ep	*hw_ep = pmusb->endpoints + i;

		hw_ep->fifo = MUSB_FIFO_OFFSET(i) + pmbase;
		hw_ep->regs = MUSB_EP_OFFSET(i, 0) + pmbase;
		hw_ep->tx_reinit = 1;
		hw_ep->target_regs = musb_read_target_reg_base(i, pmbase);
		hw_ep->rx_reinit = 1;

		if (hw_ep->max_packet_sz_tx) {
			dev_dbg(pmusb->controller,
				"%s: hw_ep %d%s, %smax %d\n",musb_driver_name, i,hw_ep->is_shared_fifo ? "shared" : "tx",
				hw_ep->tx_double_buffered	? "doublebuffer, " : "",hw_ep->max_packet_sz_tx);
		}
		if (!hw_ep->is_shared_fifo && hw_ep->max_packet_sz_rx) {
			dev_dbg(pmusb->controller,
				"%s: hw_ep %d%s, %smax %d\n",musb_driver_name, i,
				"rx",hw_ep->rx_double_buffered	? "doublebuffer, " : "",
				hw_ep->max_packet_sz_rx);
		}
		if (!(hw_ep->max_packet_sz_rx || hw_ep->max_packet_sz_tx))
			dev_dbg(pmusb->controller, "hw_ep %d not configured\n",
		        i);
	}

	return 0;
}



static irqreturn_t generic_interrupt(int irq, void *__hci)
{
	unsigned long	flags;
	irqreturn_t     retval = IRQ_NONE;
	struct musb     *pmusb = __hci;
	u32 reg_dma;

	spin_lock_irqsave(&pmusb->lock, flags);

	pmusb->int_usb = musb_readb(pmusb->mregs, MUSB_INTRUSB);
	pmusb->int_tx = musb_readw(pmusb->mregs, MUSB_INTRTX);
	pmusb->int_rx = musb_readw(pmusb->mregs, MUSB_INTRRX);
	pmusb->int_listend = musb_readw(pmusb->mregs, MUSB_LISTEND_INT_STS);
	reg_dma = musb_readl(pmusb->mregs, MUSB_DMA_INTR_MASK_STATUS);
	//dev_dbg(pmusb->controller, "sprd_musb_interrupt usb%04x tx%04x rx%04x listend%04x dma%x\n",
	//		pmusb->int_usb, pmusb->int_tx, pmusb->int_rx, pmusb->int_listend, reg_dma);
	//dprintf(INFO,"sprd_musb_interrupt usb%04x tx%04x rx%04x listend%04x dma%x\n",
			//pmusb->int_usb, pmusb->int_tx, pmusb->int_rx, pmusb->int_listend, reg_dma);

	if (pmusb->int_usb || ((pmusb->int_tx & 0x01) == 0x01)) {
		if (!reg_dma)
			retval = musb_interrupt(pmusb);
	}
	if (reg_dma)
		retval = sprd_dma_interrupt(pmusb, reg_dma);

	spin_unlock_irqrestore(&pmusb->lock, flags);

	return retval;
}


irqreturn_t musb_interrupt(struct musb *pmusb)
{
	irqreturn_t	pretval = IRQ_NONE;
	u8		devctl, power;
	int		ep_num;
	u32		reg;

	devctl = musb_readb(pmusb->mregs, MUSB_DEVCTL);
	power = musb_readb(pmusb->mregs, MUSB_POWER);

	/* the core can interrupt us for multiple reasons; docs have
	 * a generic interrupt flowchart to follow
	 */
	if (pmusb->int_usb)
		pretval |= musb_stage0_irq(pmusb, pmusb->int_usb,
				devctl, power);

	/* "stage 1" is handling endpoint irqs */

	/* handle endpoint 0 first */
	if (pmusb->int_tx & 1) {
		if (devctl & MUSB_DEVCTL_HM) {
			if (is_host_capable())
				pretval |= musb_h_ep0_irq(pmusb);
		} else {
			if (is_peripheral_capable())
				pretval |= musb_g_ep0_irq(pmusb);
		}
	}

	/* RX on endpoints 1-15 */
	reg = pmusb->int_rx >> 1;
	ep_num = 1;
	while (reg) {
		if (reg & 1) {
			/* musb_ep_select(pmusb->mregs, ep_num); */
			/* REVISIT just retval = ep->rx_irq(...) */
			pretval = IRQ_HANDLED;
			if (devctl & MUSB_DEVCTL_HM) {
				if (is_host_capable())
					musb_host_rx(pmusb, ep_num);
			} else {
				if (is_peripheral_capable())
					sprd_musb_g_rx(pmusb, ep_num);
			}
		}

		reg >>= 1;
		ep_num++;
	}

	/* TX on endpoints 1-15 */
	reg = pmusb->int_tx >> 1;
	ep_num = 1;
	while (reg) {
		if (reg & 1) {
			/* musb_ep_select(pmusb->mregs, ep_num); */
			/* REVISIT just retval |= ep->tx_irq(...) */
			pretval = IRQ_HANDLED;
			if (devctl & MUSB_DEVCTL_HM) {
				if (is_host_capable())
					musb_host_tx(pmusb, ep_num);
			} else {
				if (is_peripheral_capable())
					sprd_musb_g_tx(pmusb, ep_num);
			}
		}
		reg >>= 1;
		ep_num++;
	}

	return pretval;
}

static bool __devinitdata use_dma = 1;

/* "modprobe ... use_dma=0" etc */
module_param(use_dma, bool, 0);
MODULE_PARM_DESC(use_dma, "enable/disable use of DMA");

static struct musb *__devinit
allocate_instance(struct device *dev,
		struct musb_hdrc_config *config, void __iomem *mbase)
{
	struct musb		*pmusb;
	struct musb_hw_ep	*ep;
	int			epnum;

	pmusb = calloc(1, sizeof(*pmusb));
	if (!pmusb)
		return NULL;

	INIT_LIST_HEAD(&pmusb->control);
	INIT_LIST_HEAD(&pmusb->in_bulk);
	INIT_LIST_HEAD(&pmusb->out_bulk);

	pmusb->vbuserr_retry = VBUSERR_RETRY_COUNT;
	pmusb->a_wait_bcon = OTG_TIME_A_WAIT_BCON;
	dev_set_drvdata(dev, pmusb);
	pmusb->mregs = mbase;
	pmusb->ctrl_base = mbase;
	pmusb->nIrq = -ENODEV;
	pmusb->config = config;
	BUG_ON(pmusb->config->num_eps > MUSB_C_NUM_EPS);
	for (epnum = 0, ep = pmusb->endpoints;
			epnum < pmusb->config->num_eps;
			epnum++, ep++) {
		ep->musb = pmusb;
		ep->epnum = epnum;
	}

	pmusb->controller = dev;

	return pmusb;
}

void musb_free(struct musb *pmusb)
{
	if (pmusb->nIrq >= 0) {
		if (pmusb->irq_wake)
			disable_irq_wake(pmusb->nIrq);
		free_irq(pmusb->nIrq, pmusb);
	}
	if (is_dma_capable() && pmusb->dma_controller) {
		struct dma_controller	*controller = pmusb->dma_controller;
		if(controller->stop) {
			(void) controller->stop(controller);
		}
		dma_controller_destroy(controller);
	}

	kfree(pmusb);
}


struct musb * musb_init_controller(struct musb_hdrc_platform_data *plat, struct device *dev,
			     void *ctrl)
{
	int			ret;
	struct musb		*pmusb;

	int nIrq = 0;

	if (!plat) {
		dev_dbg(dev, "no platform_data?\n");
		ret = -ENODEV;
		goto fail0;
	}

	/* allocate */
	pmusb = allocate_instance(dev, plat->config, ctrl);
	if (!pmusb) {
		ret = -ENOMEM;
		goto fail0;
	}

	spin_lock_init(&pmusb->lock);
	pmusb->board_mode = plat->mode;
	pmusb->board_set_power = plat->set_power;
	pmusb->min_power = plat->min_power;
	pmusb->ops = plat->platform_ops;

	pmusb->isr = generic_interrupt;
	ret = musb_platform_init(pmusb);
	if (ret < 0)
		goto fail1;

	if (!pmusb->isr) {
		ret = -ENODEV;
		goto fail2;
	}

	if (use_dma ) {
		struct dma_controller	*c;

		c = dma_controller_create(pmusb, pmusb->mregs);
		pmusb->dma_controller = c;
	}

	musb_platform_disable(pmusb);
	musb_generic_disable(pmusb);

	ret = musb_core_init(plat->config->multipoint
			? MUSB_CONTROLLER_MHDRC
			: MUSB_CONTROLLER_HDRC, pmusb);
	if (ret < 0)
		goto fail3;

	pmusb->nIrq = nIrq;
	pmusb->irq_wake = 0;

	MUSB_DEV_MODE(pmusb);

	if (is_peripheral_capable())
		ret = sprd_musb_gadget_setup(pmusb);

	if (ret < 0)
		goto fail3;

	return ret == 0 ? pmusb : NULL;

fail3:
fail2:
	musb_platform_exit(pmusb);

fail1:
	dev_err(pmusb->controller,
		"musb_init_controller failed with status %d\n", ret);

	musb_free(pmusb);

fail0:
	return NULL;

}
