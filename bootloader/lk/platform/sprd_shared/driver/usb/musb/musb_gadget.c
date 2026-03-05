/*
 * MUSB OTG driver peripheral support
 */

#include <linux/kernel.h>
#include <sprd_common.h>
#include <linux/usb/ch9.h>
#include "linux-compat.h"

#include "musb_core.h"
#include <asm/dma-mapping.h>

/* ----------------------------------------------------------------------- */

#define is_buffer_mapped(req) (is_dma_capable() && \
					(req->map_state != UN_MAPPED))

/* Unmap the buffer from dma and maps it back to cpu */
static inline void unmap_dma_buffer(struct sprd_musb_request *req,
				struct musb *musb)
{
	if (!is_buffer_mapped(req))
		return;

	if (req->request.dma == DMA_ADDR_INVALID) {
		dev_vdbg(musb->controller,
				"not unmapping a never mapped buffer\n");
		return;
	}
	if (req->map_state == MUSB_MAPPED) {
		dma_unmap_single(
			req->request.dma,
			req->request.length,
			req->tx
				? DMA_TO_DEVICE
				: DMA_FROM_DEVICE);
		req->request.dma = DMA_ADDR_INVALID;
	}
	req->map_state = UN_MAPPED;
}


void sprd_musb_g_giveback(
	struct musb_ep		*ep,
	struct usb_request	*request,
	int			status)
__releases(ep->musb->lock)
__acquires(ep->musb->lock)
{
	struct sprd_musb_request	*tmp;
	struct musb		*musb;
	int			busy = ep->busy;

	tmp = to_sprd_musb_request(request);

	list_delete(&tmp->list);
	if (tmp->request.status == -EINPROGRESS)
		tmp->request.status = status;
	musb = tmp->musb;

	ep->busy = 1;
	spin_unlock(&musb->lock);
	unmap_dma_buffer(tmp, musb);
	if (request->status == 0)
		dev_vdbg(musb->controller, "%s done request %p,  %d/%d\n",
				ep->end_point.name, request,
				tmp->request.actual, tmp->request.length);
	else
		dev_vdbg(musb->controller, "%s request %p, %d/%d fault %d\n",
				ep->end_point.name, request,
				tmp->request.actual, tmp->request.length,
				request->status);
	tmp->request.complete(&tmp->ep->end_point, &tmp->request);
	spin_lock(&musb->lock);
	ep->busy = busy;
}


static inline int max_ep_writesize(struct musb *musb, struct musb_ep *ep)
{
	if (can_bulk_split(musb, ep->type))
		return ep->hw_ep->max_packet_sz_tx;
	else
		return ep->packet_sz;
}


static void nuke(struct musb_ep *p, const int status)
{
	struct musb		*musb = p->musb;
	struct sprd_musb_request	*tmp = NULL;
	void __iomem *epio = p->musb->endpoints[p->current_epnum].regs;

	p->busy = 1;

	if (is_dma_capable() && p->dma) {
        int ret;
		struct dma_controller	*pcontroller = p->musb->dma_controller;

		if (p->is_in) {
			musb_writew(epio, MUSB_TXCSR,
				    MUSB_TXCSR_DMAMODE | MUSB_TXCSR_FLUSHFIFO);
			musb_writew(epio, MUSB_TXCSR,
					0 | MUSB_TXCSR_FLUSHFIFO);
		} else {
			musb_writew(epio, MUSB_RXCSR,
					0 | MUSB_RXCSR_FLUSHFIFO);
			musb_writew(epio, MUSB_RXCSR,
					0 | MUSB_RXCSR_FLUSHFIFO);
		}

		ret = pcontroller->channel_abort(p->dma);
		dev_dbg(musb->controller, "%s: abort DMA --> %d\n",
				p->name, ret);
		pcontroller->channel_release(p->dma);
		p->dma = NULL;
	}

	while (!list_is_empty(&p->req_list)) {
		tmp = list_first_entry(&p->req_list, struct sprd_musb_request, list);
		sprd_musb_g_giveback(p, &tmp->request, status);
	}
}


static void txstate(struct musb *musb, struct sprd_musb_request *req)
{
	int			use_dma = 0;
	u16			fifo_count = 0, csr;
	u8			epnum = req->epnum;
	void __iomem		*epio = musb->endpoints[epnum].regs;
	struct usb_request	*tmp;
	struct musb_ep		*ep;

	ep = req->ep;

	/* Check if EP is disabled */
	if (!ep->desc) {
		dev_dbg(musb->controller, "ep:%s disabled - ignore request\n",
						ep->end_point.name);
		return;
	}

	if (MUSB_DMA_STATUS_BUSY == dma_channel_status(ep->dma)) {
		dev_dbg(musb->controller, "dma pending...\n");
		return;
	}

	csr = musb_readw(epio, MUSB_TXCSR);

	tmp = &req->request;
	fifo_count = min(max_ep_writesize(musb, ep),
			(int)(tmp->length - tmp->actual));

	if (csr & MUSB_TXCSR_TXPKTRDY) {
		dev_dbg(musb->controller, "%s old packet still ready , txcsr %03x\n",
				ep->end_point.name, csr);
		return;
	}

	if (csr & MUSB_TXCSR_P_SENDSTALL) {
		dev_dbg(musb->controller, "%s stalling, txcsr %03x\n",
				ep->end_point.name, csr);
		return;
	}

	dev_dbg(musb->controller, "hw_ep%d, maxpacket %d, fifo count %d, txcsr %03x\n",
			epnum, ep->packet_sz, fifo_count,
			csr);

	if (is_buffer_mapped(req)) {
		size_t request_size;

		/* setup DMA, then program endpoint CSR */
		request_size = min_t(size_t, tmp->length - tmp->actual,
					ep->dma->max_len);

		use_dma = (tmp->dma != DMA_ADDR_INVALID);
	}

	if (!use_dma) {
		/*
		 * Unmap the dma buffer back to cpu if dma channel
		 * programming fails
		 */
		unmap_dma_buffer(req, musb);

		musb_write_fifo(ep->hw_ep, fifo_count,
				(u8 *) (tmp->buf + tmp->actual));
		tmp->actual += fifo_count;
		csr |= MUSB_TXCSR_TXPKTRDY;
		csr &= ~MUSB_TXCSR_P_UNDERRUN;
		musb_writew(epio, MUSB_TXCSR, csr);
	}

	/* host may already have the data when this message shows... */
	dev_dbg(musb->controller, "%s TX/IN %s len %d/%d, txcsr %04x, fifo %d/%d\n",
			ep->end_point.name, use_dma ? "dma" : "pio",
			tmp->actual, tmp->length,
			musb_readw(epio, MUSB_TXCSR),
			fifo_count,
			musb_readw(epio, MUSB_TXMAXP));
}


static void rxstate(struct musb *musb, struct sprd_musb_request *req)
{
    u8			use_mode_1;
	u16			len;
	unsigned		fifo_count = 0;
	struct musb_ep		*ep;
	const u8		epnum = req->epnum;
	struct usb_request	*tmp = &req->request;
	void __iomem		*epio = musb->endpoints[epnum].regs;
	u16			csr = musb_readw(epio, MUSB_RXCSR);
	struct musb_hw_ep	*hw_ep = &musb->endpoints[epnum];

	if (hw_ep->is_shared_fifo)
		ep = &hw_ep->ep_in;
	else
		ep = &hw_ep->ep_out;

	len = ep->packet_sz;

	/* Check if EP is disabled */
	if (!ep->desc) {
		dev_dbg(musb->controller, "ep:%s disabled - ignore request\n",
						ep->end_point.name);
		return;
	}

	/* We shouldn't get here while DMA is active, but we do... */
	if (MUSB_DMA_STATUS_BUSY == dma_channel_status(ep->dma)) {
		dev_dbg(musb->controller, "DMA pending...\n");
		return;
	}

	if (csr & MUSB_RXCSR_P_SENDSTALL) {
		dev_dbg(musb->controller, "%s stalling, RXCSR %04x\n",
		    ep->end_point.name, csr);
		return;
	}

	if (csr & MUSB_RXCSR_RXPKTRDY) {
		len = musb_readw(epio, MUSB_RXCOUNT);

		if (tmp->short_not_ok && len == ep->packet_sz)
			use_mode_1 = 1;
		else
			use_mode_1 = 0;

		if (tmp->actual < tmp->length) {
			fifo_count = tmp->length - tmp->actual;
			dev_dbg(musb->controller, "%s OUT/RX pio fifo %d/%d, maxpacket %d\n",
					ep->end_point.name,
					len, fifo_count,
					ep->packet_sz);

			fifo_count = min_t(unsigned, len, fifo_count);

			 if (is_buffer_mapped(req)) {
				unmap_dma_buffer(req, musb);
				csr &= ~(MUSB_RXCSR_DMAENAB | MUSB_RXCSR_AUTOCLEAR);
				musb_writew(epio, MUSB_RXCSR, csr);
			}

			musb_read_fifo(ep->hw_ep, fifo_count, (u8 *)
					(tmp->buf + tmp->actual));
			tmp->actual += fifo_count;

			/* ack the read! */
			csr |= MUSB_RXCSR_P_WZC_BITS;
			csr &= ~MUSB_RXCSR_RXPKTRDY;
			musb_writew(epio, MUSB_RXCSR, csr);
		}
	}

	if (tmp->actual == tmp->length || len < ep->packet_sz)
		sprd_musb_g_giveback(ep, tmp, 0);
}

void sprd_musb_g_tx(struct musb *musb, u8 epnum)
{
	struct sprd_dma_channel	*dma;
	u16			reg_csr;
	struct sprd_musb_request	*sprd_req;
	struct musb_ep		*ep = &musb->endpoints[epnum].ep_in;
	struct usb_request	*tmp;
	u8 __iomem		*mbase = musb->mregs;
	void __iomem		*epio = musb->endpoints[epnum].regs;

	musb_ep_select(mbase, epnum);
	sprd_req = sprd_next_request(ep);
	tmp = &sprd_req->request;

	reg_csr = musb_readw(epio, MUSB_TXCSR);
	dev_dbg(musb->controller, "<== %s, txcsr %04x\n", ep->end_point.name, reg_csr);

	dma = is_dma_capable() ? ep->dma : NULL;

	if (reg_csr & MUSB_TXCSR_P_SENTSTALL) {
		reg_csr |=	MUSB_TXCSR_P_WZC_BITS;
		reg_csr &= ~MUSB_TXCSR_P_SENTSTALL;
		musb_writew(epio, MUSB_TXCSR, reg_csr);
		return;
	}

	if (reg_csr & MUSB_TXCSR_P_UNDERRUN) {
		reg_csr |=	 MUSB_TXCSR_P_WZC_BITS;
		reg_csr &= ~(MUSB_TXCSR_P_UNDERRUN | MUSB_TXCSR_TXPKTRDY);
		musb_writew(epio, MUSB_TXCSR, reg_csr);
		dev_vdbg(musb->controller, "underrun on ep%d, req %p\n",
				epnum, tmp);
	}

	if (dma_channel_status(dma) == MUSB_DMA_STATUS_BUSY) {
		dev_dbg(musb->controller, "%s dma still busy?\n", ep->end_point.name);
		return;
	}

	if (tmp) {
		u8	is_dma = 0;

		if (dma && (reg_csr & MUSB_TXCSR_DMAENAB)) {
			is_dma = 1;
			reg_csr |= MUSB_TXCSR_P_WZC_BITS;
			reg_csr &= ~(MUSB_TXCSR_DMAENAB | MUSB_TXCSR_P_UNDERRUN |
				 MUSB_TXCSR_TXPKTRDY | MUSB_TXCSR_AUTOSET);
			musb_writew(epio, MUSB_TXCSR, reg_csr);
			reg_csr = musb_readw(epio, MUSB_TXCSR);
			tmp->actual += ep->dma->actual_len;
			dev_dbg(musb->controller, "TXCSR%d %04x, DMA off, len %zu, req %p\n",
				epnum, reg_csr, ep->dma->actual_len, tmp);
		}

		if ((tmp->zero && tmp->length
			&& (tmp->length % ep->packet_sz == 0)
			&& (tmp->actual == tmp->length))) {
			if (reg_csr & MUSB_TXCSR_TXPKTRDY)
				return;

			dev_dbg(musb->controller, "sending zero pkt\n");
			musb_writew(epio, MUSB_TXCSR, MUSB_TXCSR_MODE
					| MUSB_TXCSR_TXPKTRDY);
			tmp->zero = 0;
		}

		if (tmp->actual == tmp->length) {
			sprd_musb_g_giveback(ep, tmp, 0);
			musb_ep_select(mbase, epnum);
			sprd_req = ep->desc ? sprd_next_request(ep) : NULL;
			if (!sprd_req) {
				dev_dbg(musb->controller, "%s idle now\n",
					ep->end_point.name);
				return;
			}
		}

		txstate(musb, sprd_req);
	}
}

/*
 * Data ready for a request; called from IRQ
 */
void sprd_musb_g_rx(struct musb *musb, u8 epnum)
{
	void __iomem		*mbase = musb->mregs;
	struct musb_ep		*pmusb_ep;
	void __iomem		*epio = musb->endpoints[epnum].regs;
	struct sprd_dma_channel	*dma;
	struct musb_hw_ep	*hw_ep = &musb->endpoints[epnum];
	struct usb_request	*request;
    struct sprd_musb_request	*req;
	u16			reg_csr;

	if (hw_ep->is_shared_fifo)
		pmusb_ep = &hw_ep->ep_in;
	else
		pmusb_ep = &hw_ep->ep_out;

	musb_ep_select(mbase, epnum);

	req = sprd_next_request(pmusb_ep);
	if (!req)
		return;

	request = &req->request;

	reg_csr = musb_readw(epio, MUSB_RXCSR);
	dma = is_dma_capable() ? pmusb_ep->dma : NULL;

	dev_dbg(musb->controller, "<== %s, rxcsr %04x%s %p\n", pmusb_ep->end_point.name,
			reg_csr, dma ? " (dma)" : "", request);

	if (reg_csr & MUSB_RXCSR_P_SENTSTALL) {
		reg_csr |= MUSB_RXCSR_P_WZC_BITS;
		reg_csr &= ~MUSB_RXCSR_P_SENTSTALL;
		musb_writew(epio, MUSB_RXCSR, reg_csr);
		return;
	}

	if (reg_csr & MUSB_RXCSR_P_OVERRUN) {
		reg_csr &= ~MUSB_RXCSR_P_OVERRUN;
		musb_writew(epio, MUSB_RXCSR, reg_csr);

		dev_dbg(musb->controller, "%s iso overrun on %p\n", pmusb_ep->name, request);
		if (request->status == -EINPROGRESS)
			request->status = -EOVERFLOW;
	}
	if (reg_csr & MUSB_RXCSR_INCOMPRX) {
		dev_dbg(musb->controller, "%s, incomprx\n", pmusb_ep->end_point.name);
	}

	if (MUSB_DMA_STATUS_BUSY == dma_channel_status(dma)) {
		dev_dbg(musb->controller, "%s busy, csr %04x\n",
			pmusb_ep->end_point.name, reg_csr);
		return;
	}

	if (dma && (reg_csr & MUSB_RXCSR_DMAENAB)) {
		reg_csr &= ~(MUSB_RXCSR_AUTOCLEAR
				| MUSB_RXCSR_DMAENAB
				| MUSB_RXCSR_DMAMODE);
		musb_writew(epio, MUSB_RXCSR,
			MUSB_RXCSR_P_WZC_BITS | reg_csr);

		request->actual += pmusb_ep->dma->actual_len;

		dev_dbg(musb->controller, "RXCSR%d %04x, dma off, %04x, len %zu, req %p\n",
			epnum, reg_csr,
			musb_readw(epio, MUSB_RXCSR),
			pmusb_ep->dma->actual_len, request);

		sprd_musb_g_giveback(pmusb_ep, request, 0);
		/*
		 * In the giveback function the MUSB lock is
		 * released and acquired after sometime. During
		 * this time period the INDEX register could get
		 * changed by the gadget_queue function especially
		 * on SMP systems. Reselect the INDEX to be sure
		 * we are reading/modifying the right registers
		 */
		musb_ep_select(mbase, epnum);

		req = sprd_next_request(pmusb_ep);
		if (!req)
			return;
	}
	/* Analyze request */
	rxstate(musb, req);
}

/* ------------------------------------------------------------ */
static int musb_gadget_disable(struct usb_ep *ep)
{
    int		status = 0;
	void __iomem	*epio;
	struct musb_ep	*pmusb_ep;
	u8		epnum;
	struct musb	*pmusb;
	unsigned long	flags;

	pmusb_ep = to_musb_ep(ep);
	pmusb = pmusb_ep->musb;
	epnum = pmusb_ep->current_epnum;
	epio = pmusb->endpoints[epnum].regs;

	spin_lock_irqsave(&pmusb->lock, flags);
	musb_ep_select(pmusb->mregs, epnum);

	/* zero the endpoint sizes */
	if (pmusb_ep->is_in) {
		u16 int_txe = musb_readw(pmusb->mregs, MUSB_INTRTXE);
		int_txe &= ~(1 << epnum);
		musb_writew(pmusb->mregs, MUSB_INTRTXE, int_txe);
		musb_writew(epio, MUSB_TXMAXP, 0);
	} else {
		u16 int_rxe = musb_readw(pmusb->mregs, MUSB_INTRRXE);
		int_rxe &= ~(1 << epnum);
		musb_writew(pmusb->mregs, MUSB_INTRRXE, int_rxe);
		musb_writew(epio, MUSB_RXMAXP, 0);
	}

	pmusb_ep->desc = NULL;

	/* abort all pending DMA and requests */
	nuke(pmusb_ep, -ESHUTDOWN);

	schedule_work(&pmusb->irq_work);

	spin_unlock_irqrestore(&(pmusb->lock), flags);

	dev_dbg(pmusb->controller, "%s\n", pmusb_ep->end_point.name);

	return status;
}

static int musb_gadget_enable(struct usb_ep *ep,
			const struct usb_endpoint_descriptor *desc)
{
	int		status = -EINVAL;
	unsigned	tmp;
	u16		reg_csr;
	u8		epnum;
	void __iomem	*pmbase;
	struct musb		*pmusb;
	void __iomem		*regs;
	struct musb_hw_ep	*phw_ep;
	struct musb_ep		*pmusb_ep;
	unsigned long		flags;

	if (!ep || !desc)
		return -EINVAL;

	pmusb_ep = to_musb_ep(ep);
	phw_ep = pmusb_ep->hw_ep;
	regs = phw_ep->regs;
	pmusb = pmusb_ep->musb;
	pmbase = pmusb->mregs;
	epnum = pmusb_ep->current_epnum;

	spin_lock_irqsave(&pmusb->lock, flags);

	if (pmusb_ep->desc) {
		status = -EBUSY;
		goto fail;
	}
	pmusb_ep->type = usb_endpoint_type(desc);

	/* check direction and (later) maxpacket size against endpoint */
	if (usb_endpoint_num(desc) != epnum)
		goto fail;

	/* REVISIT this rules out high bandwidth periodic transfers */
	tmp = usb_endpoint_maxp(desc);
	if (tmp & ~0x07ff) {
		int is_ok;

		if (usb_endpoint_dir_in(desc))
			is_ok = pmusb->hb_iso_tx;
		else
			is_ok = pmusb->hb_iso_rx;

		if (!is_ok) {
			dev_dbg(pmusb->controller, "no support for high bandwidth ISO\n");
			goto fail;
		}
		pmusb_ep->hb_mult = (tmp >> 11) & 3;
	} else {
		pmusb_ep->hb_mult = 0;
	}

	pmusb_ep->packet_sz = tmp & 0x7ff;
	tmp = pmusb_ep->packet_sz * (pmusb_ep->hb_mult + 1);

	musb_ep_select(pmbase, epnum);
	if (usb_endpoint_dir_in(desc)) {
		u16 txe = musb_readw(pmbase, MUSB_INTRTXE);

		if (phw_ep->is_shared_fifo)
			pmusb_ep->is_in = 1;
		if (!pmusb_ep->is_in)
			goto fail;

		if (tmp > phw_ep->max_packet_sz_tx) {
			dev_dbg(pmusb->controller, "packet size beyond hardware FIFO size\n");
			goto fail;
		}

		txe |= (1 << epnum);
		musb_writew(pmbase, MUSB_INTRTXE, txe);

		if (pmusb->double_buffer_not_ok)
			musb_writew(regs, MUSB_TXMAXP, phw_ep->max_packet_sz_tx);
		else
			musb_writew(regs, MUSB_TXMAXP, pmusb_ep->packet_sz
					| (pmusb_ep->hb_mult << 11));

		reg_csr = MUSB_TXCSR_MODE | MUSB_TXCSR_CLRDATATOG;
		if (musb_readw(regs, MUSB_TXCSR)
				& MUSB_TXCSR_FIFONOTEMPTY)
			reg_csr |= MUSB_TXCSR_FLUSHFIFO;
		if (pmusb_ep->type == USB_ENDPOINT_XFER_ISOC)
			reg_csr |= MUSB_TXCSR_P_ISO;

		musb_writew(regs, MUSB_TXCSR, reg_csr);
		musb_writew(regs, MUSB_TXCSR, reg_csr);

	} else {
		u16 rxe = musb_readw(pmbase, MUSB_INTRRXE);

		if (phw_ep->is_shared_fifo)
			pmusb_ep->is_in = 0;
		if (pmusb_ep->is_in)
			goto fail;

		if (tmp > phw_ep->max_packet_sz_rx) {
			dev_dbg(pmusb->controller, "packet size beyond hardware FIFO size\n");
			goto fail;
		}

		rxe |= (1 << epnum);
		musb_writew(pmbase, MUSB_INTRRXE, rxe);

		if (pmusb->double_buffer_not_ok)
			musb_writew(regs, MUSB_RXMAXP, phw_ep->max_packet_sz_tx);
		else
			musb_writew(regs, MUSB_RXMAXP, pmusb_ep->packet_sz | (pmusb_ep->hb_mult << 11));

		if (phw_ep->is_shared_fifo) {
			reg_csr = musb_readw(regs, MUSB_TXCSR);
			reg_csr &= ~(MUSB_TXCSR_MODE | MUSB_TXCSR_TXPKTRDY);
			musb_writew(regs, MUSB_TXCSR, reg_csr);
		}

		reg_csr = MUSB_RXCSR_FLUSHFIFO | MUSB_RXCSR_CLRDATATOG;
		if (pmusb_ep->type == USB_ENDPOINT_XFER_ISOC)
			reg_csr |= MUSB_RXCSR_P_ISO;
		else if (pmusb_ep->type == USB_ENDPOINT_XFER_INT)
			reg_csr |= MUSB_RXCSR_DISNYET;

		musb_writew(regs, MUSB_RXCSR, reg_csr);
		musb_writew(regs, MUSB_RXCSR, reg_csr);
	}

	if (is_dma_capable() && pmusb->dma_controller) {
		struct dma_controller	*controller = pmusb->dma_controller;

		pmusb_ep->dma = controller->channel_alloc(controller, phw_ep,
				(desc->bEndpointAddress & USB_DIR_IN));
	} else
		pmusb_ep->dma = NULL;

	pr_debug("%s periph: enabled %s for %s %s, %smaxpacket %d\n",
			musb_driver_name, pmusb_ep->end_point.name,
			({ char *s; switch (pmusb_ep->type) {
			case USB_ENDPOINT_XFER_BULK:	s = "bulk"; break;
			case USB_ENDPOINT_XFER_INT:	s = "int"; break;
			default:			s = "iso"; break;
			}; s; }),
			pmusb_ep->is_in ? "IN" : "OUT",
			pmusb_ep->dma ? "dma, " : "",
			pmusb_ep->packet_sz);

	status = 0;
	pmusb_ep->wedged = 0;
	pmusb_ep->busy = 0;
	pmusb_ep->desc = desc;

	schedule_work(&pmusb->irq_work);

fail:
	spin_unlock_irqrestore(&pmusb->lock, flags);
	return status;
}



void musb_free_request(struct usb_ep *ep, struct usb_request *req)
{
	kfree(to_sprd_musb_request(req));
}


struct free_record {
	struct list_head	list;
	struct device		*dev;
	unsigned		bytes;
	dma_addr_t		dma;
};

struct usb_request *musb_alloc_request(struct usb_ep *ep, gfp_t gfp_flags)
{
	struct musb_ep		*musb_ep = to_musb_ep(ep);
	struct musb		*musb = musb_ep->musb;
	struct sprd_musb_request	*request = NULL;

	request = kzalloc(sizeof *request, gfp_flags);
	if (!request) {
		dev_dbg(musb->controller, "not enough memory\n");
		return NULL;
	}

	request->request.dma = DMA_ADDR_INVALID;
	request->epnum = musb_ep->current_epnum;
	request->ep = musb_ep;

	return &request->request;
}

/*
 * Context: controller locked, IRQs blocked.
 */
void sprd_musb_ep_restart(struct musb *musb, struct sprd_musb_request *req)
{
	struct dma_controller *c = musb->dma_controller;
	struct musb_ep *musb_ep;
	struct sprd_dma_channel *channel;
	struct usb_request *request = &req->request;

	dev_vdbg(musb->controller, "sprd_musb_ep_restart %s len %u on hw_ep%d\n",
			req->tx ? "TX/IN" : "RX/OUT",
			req->request.length, req->epnum);

	musb_ep_select(musb->mregs, req->epnum);
	musb_ep = req->ep;
	channel = musb_ep->dma;
	c->channel_program(channel,
			musb_ep->packet_sz,
			req->tx,
			request->buf+request->actual,
			request->length - request->actual);
}

/* Maps the buffer to dma  */
static inline void map_dma_buffer(struct sprd_musb_request *sprd_request,
			struct musb *musb, struct musb_ep *musb_ep)
{
	int compatible = true;
	struct dma_controller *dma = musb->dma_controller;

	sprd_request->map_state = UN_MAPPED;

	if (!is_dma_capable() || !musb_ep->dma)
		return;

	/* Check if DMA engine can handle this request.
	 * DMA code must reject the USB request explicitly.
	 * Default behaviour is to map the request.
	 */
	if (dma->is_compatible)
		compatible = dma->is_compatible(musb_ep->dma,
				musb_ep->packet_sz, sprd_request->request.buf,
				sprd_request->request.length);
	if (!compatible)
		return;

	if (sprd_request->request.dma == DMA_ADDR_INVALID) {
		sprd_request->request.dma = dma_map_single(
				sprd_request->request.buf,
				sprd_request->request.length,
				sprd_request->tx
					? DMA_TO_DEVICE
					: DMA_FROM_DEVICE);
		sprd_request->map_state = MUSB_MAPPED;
	}
}


static int musb_gadget_dequeue(struct usb_ep *ep, struct usb_request *request)
{
	int			status = 0;
	unsigned long		flags;
	struct sprd_musb_request	*r;
	struct musb_ep		*pmusb_ep = to_musb_ep(ep);
	struct sprd_musb_request	*req = to_sprd_musb_request(request);
	struct musb		*musb = pmusb_ep->musb;

	if (!ep || !request || to_sprd_musb_request(request)->ep != pmusb_ep)
		return -EINVAL;

	spin_lock_irqsave(&musb->lock, flags);

	list_for_each_entry(r, &pmusb_ep->req_list, list) {
		if (r == req)
			break;
	}
	if (r != req) {
		dev_dbg(musb->controller, "request %p not queued to %s\n", request, ep->name);
		status = -EINVAL;
		goto done;
	}

	/* if the hardware doesn't have the request, easy ... */
	if (pmusb_ep->req_list.next != &req->list || pmusb_ep->busy)
		sprd_musb_g_giveback(pmusb_ep, request, -ECONNRESET);

	/* ... else abort the dma transfer ... */
	else if (is_dma_capable() && pmusb_ep->dma) {
		struct dma_controller	*controller = musb->dma_controller;

		musb_ep_select(musb->mregs, pmusb_ep->current_epnum);
		if (controller->channel_abort)
			status = controller->channel_abort(pmusb_ep->dma);
		else
			status = -EBUSY;
		if (status == 0)
			sprd_musb_g_giveback(pmusb_ep, request, -ECONNRESET);
	} else {
		/* NOTE: by sticking to easily tested hardware/driver states,
		 * we leave counting of in-flight packets imprecise.
		 */
		sprd_musb_g_giveback(pmusb_ep, request, -ECONNRESET);
	}

done:
	spin_unlock_irqrestore(&musb->lock, flags);
	return status;
}

static int musb_gadget_queue(struct usb_ep *ep, struct usb_request *req,
			gfp_t gfp_flags)
{
	unsigned long		lockflags;
	int			status = 0;
	struct musb		*pmusb;
	struct musb_ep		*musb_ep;
	struct sprd_musb_request	*sprd_request;

	if (!ep || !req)
		return -EINVAL;
	if (!req->buf)
		return -ENODATA;

	musb_ep = to_musb_ep(ep);
	pmusb = musb_ep->musb;

	sprd_request = to_sprd_musb_request(req);
	sprd_request->musb = pmusb;

	if (sprd_request->ep != musb_ep)
		return -EINVAL;

	//dev_dbg(musb->controller, "<== to %s request=%p\n", ep->name, req);

	/* request is mine now... */
	sprd_request->request.actual = 0;
	sprd_request->request.status = -EINPROGRESS;
	sprd_request->epnum = musb_ep->current_epnum;
	sprd_request->tx = musb_ep->is_in;

	map_dma_buffer(sprd_request, pmusb, musb_ep);

	spin_lock_irqsave(&pmusb->lock, lockflags);

	/* don't queue if the ep is down */
	if (!musb_ep->desc) {
		dev_dbg(pmusb->controller, "req %p queued to %s while ep %s\n",
				req, ep->name, "disabled");
		status = -ESHUTDOWN;
		goto cleanup;
	}

	/* add request to the list */
	list_add_tail(&musb_ep->req_list, &sprd_request->list);

	/* it this is the head of the queue, start i/o ... */
	if (!musb_ep->busy && &sprd_request->list == musb_ep->req_list.next)
		sprd_musb_ep_restart(pmusb, sprd_request);

cleanup:
	spin_unlock_irqrestore(&pmusb->lock, lockflags);
	return status;
}

static void musb_gadget_fifo_flush(struct usb_ep *ep)
{
	u16		reg_csr, txe;
	unsigned long	flags;
	void __iomem	*mbase;
	struct musb_ep	*musb_ep = to_musb_ep(ep);
	struct musb	*musb = musb_ep->musb;
	u8		epnum = musb_ep->current_epnum;
	void __iomem	*epio = musb->endpoints[epnum].regs;

	mbase = musb->mregs;

	spin_lock_irqsave(&musb->lock, flags);
	musb_ep_select(mbase, (u8) epnum);

	/* disable interrupts */
	txe = musb_readw(mbase, MUSB_INTRTXE);
	musb_writew(mbase, MUSB_INTRTXE, txe & ~(1 << epnum));

	if (musb_ep->is_in) {
		reg_csr = musb_readw(epio, MUSB_TXCSR);
		if (reg_csr & MUSB_TXCSR_FIFONOTEMPTY) {
			reg_csr |= MUSB_TXCSR_FLUSHFIFO | MUSB_TXCSR_P_WZC_BITS;
			reg_csr &= ~MUSB_TXCSR_TXPKTRDY;
			musb_writew(epio, MUSB_TXCSR, reg_csr);
			musb_writew(epio, MUSB_TXCSR, reg_csr);
		}
	} else {
		reg_csr = musb_readw(epio, MUSB_RXCSR);
		reg_csr |= MUSB_RXCSR_FLUSHFIFO | MUSB_RXCSR_P_WZC_BITS;
		musb_writew(epio, MUSB_RXCSR, reg_csr);
		musb_writew(epio, MUSB_RXCSR, reg_csr);
	}

	/* re-enable interrupt */
	musb_writew(mbase, MUSB_INTRTXE, txe);
	spin_unlock_irqrestore(&musb->lock, flags);
}

static int musb_gadget_fifo_status(struct usb_ep *ep)
{
	int			retval = -EINVAL;
	struct musb_ep		*pmusb_ep = to_musb_ep(ep);
	void __iomem		*epio = pmusb_ep->hw_ep->regs;

	if (pmusb_ep->desc && !pmusb_ep->is_in) {
		struct musb		*musb = pmusb_ep->musb;
		int			epnum = pmusb_ep->current_epnum;
		void __iomem		*mbase = musb->mregs;
		unsigned long		flags;

		spin_lock_irqsave(&musb->lock, flags);

		musb_ep_select(mbase, epnum);
		/* FIXME return zero unless RXPKTRDY is set */
		retval = musb_readw(epio, MUSB_RXCOUNT);

		spin_unlock_irqrestore(&musb->lock, flags);
	}
	return retval;
}


static int musb_gadget_set_halt(struct usb_ep *ep, int value)
{
	int			ret = 0;
	struct sprd_musb_request	*sprd_request;
	u16			reg_csr;
	unsigned long		flags;
	void __iomem		*mbase;
	struct musb_ep		*pmusb_ep = to_musb_ep(ep);
	u8			epnum = pmusb_ep->current_epnum;
	struct musb		*pmusb = pmusb_ep->musb;
	void __iomem		*epio = pmusb->endpoints[epnum].regs;

	if (!ep)
		return -EINVAL;
	mbase = pmusb->mregs;

	spin_lock_irqsave(&pmusb->lock, flags);

	if ((USB_ENDPOINT_XFER_ISOC == pmusb_ep->type)) {
		ret = -EINVAL;
		goto done;
	}

	musb_ep_select(mbase, epnum);

	sprd_request = sprd_next_request(pmusb_ep);
	if (value) {
		if (sprd_request) {
			dev_dbg(pmusb->controller, "request in progress, cannot halt %s\n", ep->name);
			ret = -EAGAIN;
			goto done;
		}
		/* Cannot portably stall with non-empty FIFO */
		if (pmusb_ep->is_in) {
			reg_csr = musb_readw(epio, MUSB_TXCSR);
			if (reg_csr & MUSB_TXCSR_FIFONOTEMPTY) {
				dev_dbg(pmusb->controller, "FIFO busy, cannot halt %s\n", ep->name);
				ret = -EAGAIN;
				goto done;
			}
		}
	} else
		pmusb_ep->wedged = 0;

	dev_dbg(pmusb->controller, "%s: %s stall\n", ep->name, value ? "set" : "clear");
	if (pmusb_ep->is_in) {
		reg_csr = musb_readw(epio, MUSB_TXCSR);
		reg_csr |= MUSB_TXCSR_CLRDATATOG | MUSB_TXCSR_P_WZC_BITS;
		if (value)
			reg_csr |= MUSB_TXCSR_P_SENDSTALL;
		else
			reg_csr &= ~(MUSB_TXCSR_P_SENDSTALL | MUSB_TXCSR_P_SENTSTALL);
		reg_csr &= ~MUSB_TXCSR_TXPKTRDY;
		musb_writew(epio, MUSB_TXCSR, reg_csr);
	} else {
		reg_csr = musb_readw(epio, MUSB_RXCSR);
		reg_csr |= MUSB_RXCSR_CLRDATATOG | MUSB_RXCSR_FLUSHFIFO | MUSB_RXCSR_P_WZC_BITS;
		if (value)
			reg_csr |= MUSB_RXCSR_P_SENDSTALL;
		else
			reg_csr &= ~(MUSB_RXCSR_P_SENDSTALL
				| MUSB_RXCSR_P_SENTSTALL);
		musb_writew(epio, MUSB_RXCSR, reg_csr);
	}

	/* maybe start the first request in the queue */
	if (!pmusb_ep->busy && !value && sprd_request) {
		dev_dbg(pmusb->controller, "restarting the request\n");
		sprd_musb_ep_restart(pmusb, sprd_request);
	}

done:
	spin_unlock_irqrestore(&pmusb->lock, flags);
	return ret;
}



/* ----------------------------------------------------------------------- */
static int musb_gadget_wakeup(struct usb_gadget *gadget)
{
	return 0;
}

static int musb_gadget_get_frame(struct usb_gadget *gadget)
{
	struct musb	*pmusb = gadget_to_musb(gadget);

	return (int)musb_readw(pmusb->mregs, MUSB_FRAME);
}


static int musb_gadget_set_self_powered(struct usb_gadget *gadget, int is_selfpowered)
{
	struct musb	*pmusb = gadget_to_musb(gadget);

	pmusb->is_self_powered = !!is_selfpowered;
	return 0;
}

static void musb_pullup(struct musb *pmusb, int is_on)
{
	u8 power;

	power = musb_readb(pmusb->mregs, MUSB_POWER);
	if (is_on)
		power |= MUSB_POWER_SOFTCONN;
	else
		power &= ~MUSB_POWER_SOFTCONN;

	/* FIXME if on, HdrcStart; if off, HdrcStop */

	dev_dbg(pmusb->controller, "gadget D+ pullup %s\n",
		is_on ? "on" : "off");
	musb_writeb(pmusb->mregs, MUSB_POWER, power);
}

static int musb_gadget_pullup(struct usb_gadget *gadget, int is_on)
{
	struct musb	*pmusb = gadget_to_musb(gadget);
	unsigned long	flags;

	is_on = !!is_on;

	pm_runtime_get_sync(pmusb->controller);

	spin_lock_irqsave(&pmusb->lock, flags);
	if (is_on != pmusb->softconnect) {
		pmusb->softconnect = is_on;
		musb_pullup(pmusb, is_on);
	}
	spin_unlock_irqrestore(&pmusb->lock, flags);

	pm_runtime_put(pmusb->controller);

	return 0;
}

static int musb_gadget_vbus_draw(struct usb_gadget *gadget, unsigned mA)
{
	return 0;
}

static const struct usb_gadget_ops musb_gadget_operations = {
	.get_frame		= musb_gadget_get_frame,
	.wakeup			= musb_gadget_wakeup,
	.set_selfpowered	= musb_gadget_set_self_powered,
	.vbus_draw		= musb_gadget_vbus_draw,
	.pullup			= musb_gadget_pullup,
};

static const struct usb_ep_ops musb_ep_ops = {
	.enable		= musb_gadget_enable,
	.disable	= musb_gadget_disable,
	.alloc_request	= musb_alloc_request,
	.free_request	= musb_free_request,
	.queue		= musb_gadget_queue,
	.dequeue	= musb_gadget_dequeue,
	.set_halt	= musb_gadget_set_halt,
	.fifo_status	= musb_gadget_fifo_status,
	.fifo_flush	= musb_gadget_fifo_flush
};

static void __devinit init_peripheral_ep(struct musb *musb, struct musb_ep *p_ep, u8 epnum, int is_in)
{
	struct musb_hw_ep	*phw_ep = musb->endpoints + epnum;

	memset(p_ep, 0, sizeof *p_ep);

	p_ep->current_epnum = epnum;
	p_ep->musb = musb;
	p_ep->hw_ep = phw_ep;
	p_ep->is_in = is_in;

	INIT_LIST_HEAD(&p_ep->req_list);

	sprintf(p_ep->name, "ep%d%s", epnum,
			(!epnum || phw_ep->is_shared_fifo) ? "" : (
				is_in ? "in" : "out"));
	p_ep->end_point.name = p_ep->name;
	INIT_LIST_HEAD(&p_ep->end_point.ep_list);
	if (!epnum) {
		p_ep->end_point.maxpacket = 64;
		p_ep->end_point.ops = &sprd_musb_g_ep0_ops;
		musb->g.ep0 = &p_ep->end_point;
	} else {
		if (is_in)
			p_ep->end_point.maxpacket = phw_ep->max_packet_sz_tx;
		else
			p_ep->end_point.maxpacket = phw_ep->max_packet_sz_rx;
		p_ep->end_point.ops = &musb_ep_ops;
		list_add_tail(&musb->g.ep_list, &p_ep->end_point.ep_list);
	}
}

void sprd_musb_gadget_cleanup(struct musb *musb)
{
}

static inline void __devinit musb_g_init_endpoints(struct musb *musb)
{
	unsigned		cnt = 0;
	u8			epnum;
	struct musb_hw_ep	*phw_ep;

	/* initialize endpoint list just once */
	INIT_LIST_HEAD(&(musb->g.ep_list));

	for (epnum = 0, phw_ep = musb->endpoints;
			epnum < musb->nr_endpoints;
			epnum++, phw_ep++) {
		if (phw_ep->is_shared_fifo /* || !epnum */) {
			init_peripheral_ep(musb, &phw_ep->ep_in, epnum, 0);
			cnt++;
		} else {
			if (phw_ep->max_packet_sz_tx) {
				init_peripheral_ep(musb, &phw_ep->ep_in,epnum, 1);
				cnt++;
			}
			if (phw_ep->max_packet_sz_rx) {
				init_peripheral_ep(musb, &phw_ep->ep_out,epnum, 0);
				cnt++;
			}
		}
	}
}

int __devinit sprd_musb_gadget_setup(struct musb *musb)
{
	int status;

	/* REVISIT minor race:  if (erroneously) setting up two
	 * musb peripherals at the same time, only the bus lock
	 * is probably held.
	 */

	musb->g.ops = &musb_gadget_operations;
	musb->g.max_speed = USB_SPEED_HIGH;
	musb->g.speed = USB_SPEED_UNKNOWN;

	musb->g.name = musb_driver_name;

	musb_g_init_endpoints(musb);

	musb->is_active = 0;
	musb_platform_try_idle(musb, 0);

	return 0;
}


/*
 * Register the gadget driver. Used by gadget drivers when
 * registering themselves with the controller.
 *
 * -EINVAL something went wrong (not driver)
 * -EBUSY another gadget is already using the controller
 * -ENOMEM no memory to perform the operation
 *
 * @param driver the gadget driver
 * @return <0 if error, 0 if everything is fine
 */
int sprd_musb_gadget_start(struct usb_gadget *g,
		struct usb_gadget_driver *driver)
{
	struct musb		*musb = gadget_to_musb(g);
	unsigned long		flags;
	int			retval = -EINVAL;

	pm_runtime_get_sync(musb->controller);

	musb->softconnect = 0;
	musb->gadget_driver = driver;

	spin_lock_irqsave(&musb->lock, flags);
	musb->is_active = 1;

	musb_start(musb);

	spin_unlock_irqrestore(&musb->lock, flags);

	return 0;
}

void musb_g_resume(struct musb *musb)
{
}

/* called when SOF packets stop for 3+ msec */
void musb_g_suspend(struct musb *musb)
{
}

/* called when VBUS drops below session threshold, and in other cases */
void musb_g_disconnect(struct musb *pmusb)
{
	void __iomem	*mregs = pmusb->mregs;
	u8	devctl = musb_readb(mregs, MUSB_DEVCTL);

	dev_dbg(pmusb->controller, "devctl %02x\n", devctl);

	/* clear HR */
	musb_writeb(mregs, MUSB_DEVCTL, devctl & MUSB_DEVCTL_SESSION);

	/* don't draw vbus until new b-default session */
	(void) musb_gadget_vbus_draw(&pmusb->g, 0);

	pmusb->g.speed = USB_SPEED_UNKNOWN;
	if (pmusb->gadget_driver && pmusb->gadget_driver->disconnect) {
		spin_unlock(&pmusb->lock);
		pmusb->gadget_driver->disconnect(&pmusb->g);
		spin_lock(&pmusb->lock);
	}

	pmusb->is_active = 0;
}

/* Called during SRP */
void musb_g_wakeup(struct musb *musb)
{
	musb_gadget_wakeup(&musb->g);
}


void musb_g_reset(struct musb *musb)
__releases(musb->lock)
__acquires(musb->lock)
{
	void __iomem	*pmbase = musb->mregs;
	u8		devctl = musb_readb(pmbase, MUSB_DEVCTL);
	u8		power;

	/* report disconnect, if we didn't already (flushing EP state) */
	if (musb->g.speed != USB_SPEED_UNKNOWN)
		musb_g_disconnect(musb);

	/* clear HR */
	else if (devctl & MUSB_DEVCTL_HR)
		musb_writeb(pmbase, MUSB_DEVCTL, MUSB_DEVCTL_SESSION);


	/* what speed did we negotiate? */
	power = musb_readb(pmbase, MUSB_POWER);
	musb->g.speed = (power & MUSB_POWER_HSMODE)
			? USB_SPEED_HIGH : USB_SPEED_FULL;

	/* start in USB_STATE_DEFAULT */
	musb->is_suspended = 0;
	musb->is_active = 1;
	MUSB_DEV_MODE(musb);
	musb->ep0_state = SPRD_MUSB_EP0_STAGE_SETUP;
	musb->address = 0;

	musb->g.a_hnp_support = 0;
	musb->g.a_alt_hnp_support = 0;
	musb->g.b_hnp_enable = 0;
	musb->may_wakeup = 0;
}
