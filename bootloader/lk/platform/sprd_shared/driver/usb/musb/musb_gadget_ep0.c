/*
 * unisoc 
 *
 */

#include <linux/kernel.h>
#include <sprd_common.h>
#include "linux-compat.h"

#include "musb_core.h"

/*  musb->endpoints[0].ep_in -ep0 is always  */
#define	sprd_next_ep0_request(musb)	sprd_next_in_request(&(musb)->endpoints[0])


static int sprd_service_tx_status_request(struct musb *musb,
		const struct usb_ctrlrequest *ctrlrequest)
{
	const u8 recip = ctrlrequest->bRequestType & USB_RECIP_MASK;
	void __iomem *pmbase = musb->mregs;
	int ret = 1;
	u8 results_arr[2], epcnt = 0;

	results_arr[1] = 0;

	switch (recip) {
	case USB_RECIP_DEVICE:
		results_arr[0] = musb->is_self_powered << USB_DEVICE_SELF_POWERED;
		results_arr[0] |= musb->may_wakeup << USB_DEVICE_REMOTE_WAKEUP;
		if (musb->g.is_otg) {
			results_arr[0] |= musb->g.b_hnp_enable << USB_DEVICE_B_HNP_ENABLE;
			results_arr[0] |= musb->g.a_alt_hnp_support << USB_DEVICE_A_ALT_HNP_SUPPORT;
			results_arr[0] |= musb->g.a_hnp_support << USB_DEVICE_A_HNP_SUPPORT;
		}
		break;

	case USB_RECIP_INTERFACE:
		results_arr[0] = 0;
		break;

	case USB_RECIP_ENDPOINT: {
		int		yes_in;
		struct musb_ep	*musb_ep;
		u16		tmp;
		void __iomem	*pregs;

		epcnt = (u8) ctrlrequest->wIndex;
		if (!epcnt) {
			results_arr[0] = 0;
			break;
		}

		yes_in = epcnt & USB_DIR_IN;
		if (yes_in) {
			epcnt &= 0x0f;
			musb_ep = &musb->endpoints[epcnt].ep_in;
		} else {
			musb_ep = &musb->endpoints[epcnt].ep_out;
		}
		pregs = musb->endpoints[epcnt].regs;

		if (epcnt >= MUSB_C_NUM_EPS || !musb_ep->desc) {
			ret = -EINVAL;
			break;
		}

		musb_ep_select(pmbase, epcnt);
		if (yes_in)
			tmp = musb_readw(pregs, MUSB_TXCSR) & MUSB_TXCSR_P_SENDSTALL;
		else
			tmp = musb_readw(pregs, MUSB_RXCSR) & MUSB_RXCSR_P_SENDSTALL;
		musb_ep_select(pmbase, 0);

		results_arr[0] = tmp ? 1 : 0;
		} break;

	default:
		ret = 0;
		break;
	}

	/* fill up the fifo; caller updates csr0 */
	if (ret > 0) {
		u16	length = le16_to_cpu(ctrlrequest->wLength);

		if (length > 2)
			length = 2;
		musb_write_fifo(&musb->endpoints[0], length, results_arr);
	}

	return ret;
}

static inline void sprd_musb_try_b_hnp_enable(struct musb *musb)
{
	void __iomem	*pmbase = musb->mregs;
	u8		devicectl;

	dev_dbg(musb->controller, "HNP: Setting HR\n");
	devicectl = musb_readb(pmbase, MUSB_DEVCTL);
	musb_writeb(pmbase, MUSB_DEVCTL, devicectl | MUSB_DEVCTL_HR);
}

static void sprd_musb_g_ep0_giveback(struct musb *musb, struct usb_request *req)
{
	sprd_musb_g_giveback(&musb->endpoints[0].ep_in, req, 0);
}

static int sprd_service_in_request(struct musb *musb, const struct usb_ctrlrequest *ctrlrequest)
{
	int ret = 0;	/* not handled */

	if (USB_TYPE_STANDARD
			== (ctrlrequest->bRequestType & USB_TYPE_MASK)) {
		switch (ctrlrequest->bRequest) {
		case USB_REQ_GET_STATUS:
			ret = sprd_service_tx_status_request(musb,
					ctrlrequest);
			break;

		default:
			break;
		}
	}
	return ret;
}

static int sprd_service_zero_data_request(struct musb *pmusb,
		struct usb_ctrlrequest *ctrlrequest)
__releases(pmusb->lock)
__acquires(pmusb->lock)
{
	void __iomem *pmbase = pmusb->mregs;
	const u8 recip_type = ctrlrequest->bRequestType & USB_RECIP_MASK;
    int ret = -EINVAL;

	/* the gadget driver handles everything except what we MUST handle */
	if (USB_TYPE_STANDARD
			== (ctrlrequest->bRequestType & USB_TYPE_MASK)) {
		switch (ctrlrequest->bRequest) {
		case USB_REQ_SET_ADDRESS:
			/* change it after the status stage */
			ret = 1;
			pmusb->set_address = true;
			pmusb->address =
			      (u8) (ctrlrequest->wValue & 0x7f);
			break;

		case USB_REQ_CLEAR_FEATURE:
			switch (recip_type) {
			case USB_RECIP_DEVICE:
				if (USB_DEVICE_REMOTE_WAKEUP
						!= ctrlrequest->wValue)
					break;
				ret = 1;
				pmusb->may_wakeup = 0;
				break;
			case USB_RECIP_INTERFACE:
				break;
			case USB_RECIP_ENDPOINT:{
				const u8		epnum = ctrlrequest->wIndex & 0x0f;
				struct musb_ep		*musb_ep;
				struct musb_hw_ep	*hwep;
				struct sprd_musb_request	*sprd_request;
				void __iomem		*regs;
				int			yes_in;
				u16			csr_reg;

				if (epnum >= MUSB_C_NUM_EPS || epnum == 0 ||
				    USB_ENDPOINT_HALT != ctrlrequest->wValue)
					break;

				hwep = pmusb->endpoints + epnum;
				regs = hwep->regs;
				yes_in = ctrlrequest->wIndex & USB_DIR_IN;
				if (yes_in)
					musb_ep = &hwep->ep_in;
				else
					musb_ep = &hwep->ep_out;
				if (!musb_ep->desc)
					break;

				ret = 1;
				if (musb_ep->wedged)
					break;

				musb_ep_select(pmbase, epnum);
				if (yes_in) {
					csr_reg  = musb_readw(regs, MUSB_TXCSR);
					csr_reg |= MUSB_TXCSR_CLRDATATOG | MUSB_TXCSR_P_WZC_BITS;
					csr_reg &= ~(MUSB_TXCSR_P_SENDSTALL |
						 MUSB_TXCSR_P_SENTSTALL | MUSB_TXCSR_TXPKTRDY);
					musb_writew(regs, MUSB_TXCSR, csr_reg);
				} else {
					csr_reg  = musb_readw(regs, MUSB_RXCSR);
					csr_reg |= MUSB_RXCSR_CLRDATATOG | MUSB_RXCSR_P_WZC_BITS;
					csr_reg &= ~(MUSB_RXCSR_P_SENDSTALL | MUSB_RXCSR_P_SENTSTALL);
					musb_writew(regs, MUSB_RXCSR, csr_reg);
				}

				sprd_request = sprd_next_request(musb_ep);
				if (!musb_ep->busy && sprd_request) {
					dev_dbg(pmusb->controller, "restarting the request\n");
					sprd_musb_ep_restart(pmusb, sprd_request);
				}

				musb_ep_select(pmbase, 0);
				} break;
			default:
				ret = 0;
				break;
			}
			break;

		case USB_REQ_SET_FEATURE:
			switch (recip_type) {
			case USB_RECIP_DEVICE:
				ret = 1;
				switch (ctrlrequest->wValue) {
				case USB_DEVICE_REMOTE_WAKEUP:
					pmusb->may_wakeup = 1;
					break;
				case USB_DEVICE_TEST_MODE:
					if (USB_SPEED_HIGH != pmusb->g.speed)
						goto stall;
					if (ctrlrequest->wIndex & 0xff)
						goto stall;

					switch (ctrlrequest->wIndex >> 8) {
					case 4:
						pr_debug("TEST_PACKET\n");
						pmusb->test_mode_nr = MUSB_TEST_PACKET;
						break;
					case 3:
						pr_debug("TEST_SE0_NAK\n");
						pmusb->test_mode_nr = MUSB_TEST_SE0_NAK;
						break;
					case 2:
						pr_debug("TEST_K\n");
						pmusb->test_mode_nr = MUSB_TEST_K;
						break;
					case 1:
						pr_debug("TEST_J\n");
						pmusb->test_mode_nr = MUSB_TEST_J;
						break;

					case 0xc3:
						pr_debug("TEST_FORCE_HOST\n");
						pmusb->test_mode_nr = MUSB_TEST_FORCE_HOST;
						break;
					case 0xc2:
						pr_debug("TEST_FIFO_ACCESS\n");
						pmusb->test_mode_nr = MUSB_TEST_FIFO_ACCESS;
						break;
					case 0xc1:
						pr_debug("TEST_FORCE_FS\n");
						pmusb->test_mode_nr = MUSB_TEST_FORCE_FS;
						break;
					case 0xc0:
						pr_debug("TEST_FORCE_HS\n");
						pmusb->test_mode_nr = MUSB_TEST_FORCE_HS;
						break;
					default:
						goto stall;
					}

					if (ret > 0)
						pmusb->test_mode = true;
					break;
				case USB_DEVICE_A_HNP_SUPPORT:
					if (!pmusb->g.is_otg)
						goto stall;
					pmusb->g.a_hnp_support = 1;
					break;
				case USB_DEVICE_B_HNP_ENABLE:
					if (!pmusb->g.is_otg)
						goto stall;
					pmusb->g.b_hnp_enable = 1;
					sprd_musb_try_b_hnp_enable(pmusb);
					break;
				case USB_DEVICE_DEBUG_MODE:
					ret = 0;
					break;
				case USB_DEVICE_A_ALT_HNP_SUPPORT:
					if (!pmusb->g.is_otg)
						goto stall;
					pmusb->g.a_alt_hnp_support = 1;
					break;
stall:
				default:
					ret = -EINVAL;
					break;
				}
				break;

			case USB_RECIP_INTERFACE:
				break;

			case USB_RECIP_ENDPOINT:{
				const u8		epnum = ctrlrequest->wIndex & 0x0f;
				struct musb_ep		*musb_ep;
				struct musb_hw_ep	*hwep;
				void __iomem		*regs;
				int			yes_in;
				u16			csr_reg;

				if (epnum == 0 || epnum >= MUSB_C_NUM_EPS ||
				    USB_ENDPOINT_HALT != ctrlrequest->wValue)
					break;

				hwep = pmusb->endpoints + epnum;
				regs = hwep->regs;
				yes_in = ctrlrequest->wIndex & USB_DIR_IN;
				if (yes_in)
					musb_ep = &hwep->ep_in;
				else
					musb_ep = &hwep->ep_out;
				if (!musb_ep->desc)
					break;

				musb_ep_select(pmbase, epnum);
				if (yes_in) {
					csr_reg = musb_readw(regs, MUSB_TXCSR);
					if (csr_reg & MUSB_TXCSR_FIFONOTEMPTY)
						csr_reg |= MUSB_TXCSR_FLUSHFIFO;
					csr_reg |= MUSB_TXCSR_P_SENDSTALL | MUSB_TXCSR_CLRDATATOG
						| MUSB_TXCSR_P_WZC_BITS;
					musb_writew(regs, MUSB_TXCSR, csr_reg);
				} else {
					csr_reg = musb_readw(regs, MUSB_RXCSR);
					csr_reg |= MUSB_RXCSR_P_SENDSTALL | MUSB_RXCSR_FLUSHFIFO
						| MUSB_RXCSR_P_WZC_BITS | MUSB_RXCSR_CLRDATATOG;
					musb_writew(regs, MUSB_RXCSR, csr_reg);
				}

				musb_ep_select(pmbase, 0);
				ret = 1;
				} break;

			default:
				/* class, vendor, etc ... delegate */
				ret = 0;
				break;
			}
			break;
		default:
			/* delegate SET_CONFIGURATION, etc */
			ret = 0;
		}
	} else
		ret = 0;
	return ret;
}

/*
 * transmitting to the host (IN), this code might be called from IRQ
 * and from kernel thread.
 *
 * Context:  caller holds controller lock
 */
static void ep0_txstate(struct musb *musb)
{
	u8			fifo_cnt;
	u8			*fifo_src;
	u16			csr_reg = MUSB_CSR0_TXPKTRDY;
	struct usb_request	*tmp;
	struct sprd_musb_request	*req = sprd_next_ep0_request(musb);
	void __iomem		*regs = musb->control_ep->regs;

	if (!req) {
		/* WARN_ON(1); */
		dev_dbg(musb->controller, "odd; csr0 %04x\n", musb_readw(regs, MUSB_CSR0));
		return;
	}

	tmp = &req->request;

	/* load the data */
	fifo_src = (u8 *) tmp->buf + tmp->actual;
	fifo_cnt = min((unsigned) MUSB_EP0_FIFOSIZE,
		tmp->length - tmp->actual);
	musb_write_fifo(&musb->endpoints[0], fifo_cnt, fifo_src);
	tmp->actual += fifo_cnt;

	/* update the flags */
	if (fifo_cnt < MUSB_MAX_END0_PACKET
			|| (tmp->actual == tmp->length && !tmp->zero)) {
		musb->ep0_state = SPRD_MUSB_EP0_STAGE_STATUSOUT;
		csr_reg |= MUSB_CSR0_P_DATAEND;
	} else
		tmp = NULL;

	/* send it out, triggering a "txpktrdy cleared" irq */
	musb_ep_select(musb->mregs, 0);
	musb_writew(regs, MUSB_CSR0, csr_reg);

	/* report completions as soon as the fifo's loaded; there's no
	 * win in waiting till this last packet gets acked.  (other than
	 * very precise fault reporting, needed by USB TMC; possible with
	 * this hardware, but not usable from portable gadget drivers.)
	 */
	if (tmp) {
		musb->ackpend = csr_reg;
		sprd_musb_g_ep0_giveback(musb, tmp);
		if (!musb->ackpend)
			return;
		musb->ackpend = 0;
	}
}

/* we have an ep0out data packet
 * Context:  caller holds controller lock
 */
static void sprd_ep0_rxstate(struct musb *musb)
{
	u16			cnt, csr_reg;
	struct usb_request	*tmp;
	struct sprd_musb_request	*request;
	void __iomem		*regs = musb->control_ep->regs;

	request = sprd_next_ep0_request(musb);
	tmp = &request->request;

	/* read packet and ack; or stall because of gadget driver bug:
	 * should have provided the rx buffer before setup() returned.
	 */
	if (tmp) {
		void		*buf = tmp->buf + tmp->actual;
		unsigned	len = tmp->length - tmp->actual;

		/* read the buffer */
		cnt = musb_readb(regs, MUSB_COUNT0);
		if (cnt > len) {
			tmp->status = -EOVERFLOW;
			cnt = len;
		}
		musb_read_fifo(&musb->endpoints[0], cnt, buf);
		tmp->actual += cnt;
		csr_reg = MUSB_CSR0_P_SVDRXPKTRDY;
		if (cnt < 64 || tmp->actual == tmp->length) {
			musb->ep0_state = SPRD_MUSB_EP0_STAGE_STATUSIN;
			csr_reg |= MUSB_CSR0_P_DATAEND;
		} else
			tmp = NULL;
	} else
		csr_reg = MUSB_CSR0_P_SVDRXPKTRDY | MUSB_CSR0_P_SENDSTALL;


	/* Completion handler may choose to stall, e.g. because the
	 * message just received holds invalid data.
	 */
	if (tmp) {
		musb->ackpend = csr_reg;
		sprd_musb_g_ep0_giveback(musb, tmp);
		if (!musb->ackpend)
			return;
		musb->ackpend = 0;
	}
	musb_ep_select(musb->mregs, 0);
	musb_writew(regs, MUSB_CSR0, csr_reg);
}

static void musb_read_setup(struct musb *musb, struct usb_ctrlrequest *ctrlreq)
{
	void __iomem		*regs = musb->control_ep->regs;
	struct sprd_musb_request	*sprd_req;

	musb_read_fifo(&musb->endpoints[0], sizeof *ctrlreq, (u8 *)ctrlreq);

	dev_dbg(musb->controller, "SETUP req%02x.%02x v%04x i%04x l%d\n",
		ctrlreq->bRequestType,
		ctrlreq->bRequest,
		le16_to_cpu(ctrlreq->wValue),
		le16_to_cpu(ctrlreq->wIndex),
		le16_to_cpu(ctrlreq->wLength));

	/* clean up any leftover transfers */
	sprd_req = sprd_next_ep0_request(musb);
	if (sprd_req)
		sprd_musb_g_ep0_giveback(musb, &sprd_req->request);

	musb->set_address = false;
	musb->ackpend = MUSB_CSR0_P_SVDRXPKTRDY;
	if (ctrlreq->wLength == 0) {
		if (ctrlreq->bRequestType & USB_DIR_IN)
			musb->ackpend |= MUSB_CSR0_TXPKTRDY;
		musb->ep0_state = SPRD_MUSB_EP0_STAGE_ACKWAIT;
	} else if (ctrlreq->bRequestType & USB_DIR_IN) {
		musb->ep0_state = SPRD_MUSB_EP0_STAGE_TX;
		musb_writew(regs, MUSB_CSR0, MUSB_CSR0_P_SVDRXPKTRDY);
		while (0 != (musb_readw(regs, MUSB_CSR0) & MUSB_CSR0_RXPKTRDY))
			cpu_relax();
		musb->ackpend = 0;
	} else
		musb->ep0_state = SPRD_MUSB_EP0_STAGE_RX;
}

static int
forward_to_driver(struct musb *pmusb, const struct usb_ctrlrequest *ctrlrequest)
__releases(pmusb->lock)
__acquires(pmusb->lock)
{
	int ret;
	if (!pmusb->gadget_driver)
		return -EOPNOTSUPP;
	spin_unlock(&pmusb->lock);
	ret = pmusb->gadget_driver->setup(&pmusb->g, ctrlrequest);
	spin_lock(&pmusb->lock);
	return ret;
}

static char *sprd_decode_ep0_stage(u8 usb_ep0_stage)
{
	switch (usb_ep0_stage) {
	case SPRD_MUSB_EP0_STAGE_IDLE:
		return "idle";
	case SPRD_MUSB_EP0_STAGE_SETUP:
		return "setup";
	case SPRD_MUSB_EP0_STAGE_TX:
		return "in";
	case SPRD_MUSB_EP0_STAGE_RX:
		return "out";
	case SPRD_MUSB_EP0_STAGE_ACKWAIT:
		return "wait";
	case SPRD_MUSB_EP0_STAGE_STATUSIN:
		return "in/status";
	case SPRD_MUSB_EP0_STAGE_STATUSOUT:
		return "out/status";
	default:
		return "?";
	}
}

static int musb_g_ep0_disable(struct usb_ep *e)
{
	return -EINVAL;
}

/*
 * Handle peripheral ep0 interrupt
 *
 * Context: irq handler; we won't re-enter the driver that way.
 */
irqreturn_t musb_g_ep0_irq(struct musb *musb)
{
	irqreturn_t	ret = IRQ_NONE;
	void __iomem	*regs = musb->endpoints[0].regs;
	void __iomem	*mbase = musb->mregs;
	u16		length;
	u16		csr_reg;

	musb_ep_select(mbase, 0);	/* select ep0 */
	csr_reg = musb_readw(regs, MUSB_CSR0);
	length = musb_readb(regs, MUSB_COUNT0);

	dev_dbg(musb->controller, "csr %04x, count %d, myaddr %d, ep0stage %s\n",
			csr_reg, length,
			musb_readb(mbase, MUSB_FADDR),
			sprd_decode_ep0_stage(musb->ep0_state));

	if (csr_reg & MUSB_CSR0_P_DATAEND) {
		/*
		 * If DATAEND is set we should not call the callback,
		 * hence the status stage is not complete.
		 */
		return IRQ_HANDLED;
	}

	/* I sent a stall.. need to acknowledge it now.. */
	if (csr_reg & MUSB_CSR0_P_SENTSTALL) {
		musb_writew(regs, MUSB_CSR0,
				csr_reg & ~MUSB_CSR0_P_SENTSTALL);
		ret = IRQ_HANDLED;
		musb->ep0_state = SPRD_MUSB_EP0_STAGE_IDLE;
		csr_reg = musb_readw(regs, MUSB_CSR0);
	}

	/* request ended "early" */
	if (csr_reg & MUSB_CSR0_P_SETUPEND) {
		musb_writew(regs, MUSB_CSR0, MUSB_CSR0_P_SVDSETUPEND);
		ret = IRQ_HANDLED;
		/* Transition into the early status phase */
		switch (musb->ep0_state) {
		case SPRD_MUSB_EP0_STAGE_TX:
			musb->ep0_state = SPRD_MUSB_EP0_STAGE_STATUSOUT;
			break;
		case SPRD_MUSB_EP0_STAGE_RX:
			musb->ep0_state = SPRD_MUSB_EP0_STAGE_STATUSIN;
			break;
		default:
			ERR("SetupEnd came in a wrong ep0stage %s\n",
			    sprd_decode_ep0_stage(musb->ep0_state));
		}
		csr_reg = musb_readw(regs, MUSB_CSR0);
		/* NOTE:  request may need completion */
	}

	/* docs from Mentor only describe tx, rx, and idle/setup states.
	 * we need to handle nuances around status stages, and also the
	 * case where status and setup stages come back-to-back ...
	 */
	switch (musb->ep0_state) {

	case SPRD_MUSB_EP0_STAGE_TX:
		/* irq on clearing txpktrdy */
		if ((csr_reg & MUSB_CSR0_TXPKTRDY) == 0) {
			ep0_txstate(musb);
			ret = IRQ_HANDLED;
		}
		break;

	case SPRD_MUSB_EP0_STAGE_RX:
		/* irq on set rxpktrdy */
		if (csr_reg & MUSB_CSR0_RXPKTRDY) {
			sprd_ep0_rxstate(musb);
			ret = IRQ_HANDLED;
		}
		break;

	case SPRD_MUSB_EP0_STAGE_STATUSIN:
		if (musb->set_address) {
			musb->set_address = false;
			musb_writeb(mbase, MUSB_FADDR, musb->address);
		}

		/* enter test mode if needed (exit by reset) */
		else if (musb->test_mode) {
			dev_dbg(musb->controller, "entering TESTMODE\n");

			if (MUSB_TEST_PACKET == musb->test_mode_nr)
				musb_load_testpacket(musb);

			musb_writeb(mbase, MUSB_TESTMODE,
					musb->test_mode_nr);
		}
		/* FALLTHROUGH */

	case SPRD_MUSB_EP0_STAGE_STATUSOUT:
		/* end of sequence #1: write to host (TX state) */
		{
			struct sprd_musb_request	*req;

			req = sprd_next_ep0_request(musb);
			if (req)
				sprd_musb_g_ep0_giveback(musb, &req->request);
		}

		if (csr_reg & MUSB_CSR0_RXPKTRDY)
			goto setup;

		ret = IRQ_HANDLED;
		musb->ep0_state = SPRD_MUSB_EP0_STAGE_IDLE;
		break;

	case SPRD_MUSB_EP0_STAGE_IDLE:
		ret = IRQ_HANDLED;
		musb->ep0_state = SPRD_MUSB_EP0_STAGE_SETUP;
		/* FALLTHROUGH */

	case SPRD_MUSB_EP0_STAGE_SETUP:
setup:
		if (csr_reg & MUSB_CSR0_RXPKTRDY) {
			struct usb_ctrlrequest	setup;
			int			handled = 0;

			if (length != 8) {
				ERR("SETUP packet len %d != 8 ?\n", length);
				break;
			}
			musb_read_setup(musb, &setup);
			ret = IRQ_HANDLED;

			if (unlikely(USB_SPEED_UNKNOWN == musb->g.speed)) {
				u8	powerval;

				printk(KERN_NOTICE "%s: peripheral reset "
						"irq lost!\n",musb_driver_name);
				powerval = musb_readb(mbase, MUSB_POWER);
				musb->g.speed = (powerval & MUSB_POWER_HSMODE) ? USB_SPEED_HIGH : USB_SPEED_FULL;
			}

			switch (musb->ep0_state) {
			case SPRD_MUSB_EP0_STAGE_TX:
				handled = sprd_service_in_request(musb, &setup);
				if (handled > 0) {
					musb->ackpend = MUSB_CSR0_TXPKTRDY
						| MUSB_CSR0_P_DATAEND;
					musb->ep0_state =
						SPRD_MUSB_EP0_STAGE_STATUSOUT;
				}
				break;

			case SPRD_MUSB_EP0_STAGE_ACKWAIT:
				handled = sprd_service_zero_data_request(
						musb, &setup);

				musb->ackpend |= MUSB_CSR0_P_DATAEND;

				/* status stage might be immediate */
				if (handled > 0)
					musb->ep0_state =
						SPRD_MUSB_EP0_STAGE_STATUSIN;
				break;

			default:		/* SPRD_MUSB_EP0_STAGE_RX */
				break;
			}

			dev_dbg(musb->controller, "handled %d, csr %04x, ep0stage %s\n",
				handled, csr_reg,
				sprd_decode_ep0_stage(musb->ep0_state));

			if (handled < 0)
				goto stall;
			else if (handled > 0)
				goto finish;

			handled = forward_to_driver(musb, &setup);
			if (handled < 0) {
				musb_ep_select(mbase, 0);
stall:
				dev_dbg(musb->controller, "stall (%d)\n", handled);
				musb->ackpend |= MUSB_CSR0_P_SENDSTALL;
				musb->ep0_state = SPRD_MUSB_EP0_STAGE_IDLE;
finish:
				musb_writew(regs, MUSB_CSR0,
						musb->ackpend);
				musb->ackpend = 0;
			}
		}
		break;

	case SPRD_MUSB_EP0_STAGE_ACKWAIT:
		/* This should not happen. But happens with tusb6010 with
		 * g_file_storage and high speed. Do nothing.
		 */
		ret = IRQ_HANDLED;
		break;

	default:
		/* "can't happen" */
		WARN_ON(1);
		musb_writew(regs, MUSB_CSR0, MUSB_CSR0_P_SENDSTALL);
		musb->ep0_state = SPRD_MUSB_EP0_STAGE_IDLE;
		break;
	}

	return ret;
}

static int musb_g_ep0_enable(struct usb_ep *ep, const struct usb_endpoint_descriptor *desc)
{
	return -EINVAL;
}

static int musb_g_ep0_queue(struct usb_ep *e, struct usb_request *r, gfp_t gfp_flags)
{
	void __iomem		*regs;
	unsigned long		lockflags;
	int			ret;
	struct musb		*pmusb;
	struct sprd_musb_request	*sprd_req;
	struct musb_ep		*ep;

	if (!e || !r)
		return -EINVAL;

	ep = to_musb_ep(e);
	pmusb = ep->musb;
	regs = pmusb->control_ep->regs;

	sprd_req = to_sprd_musb_request(r);
	sprd_req->musb = pmusb;
	sprd_req->request.actual = 0;
	sprd_req->request.status = -EINPROGRESS;
	sprd_req->tx = ep->is_in;

	spin_lock_irqsave(&pmusb->lock, lockflags);

	if (!list_is_empty(&ep->req_list)) {
		ret = -EBUSY;
		goto cleanup;
	}

	switch (pmusb->ep0_state) {
	case SPRD_MUSB_EP0_STAGE_RX:		/* control-OUT data */
	case SPRD_MUSB_EP0_STAGE_TX:		/* control-IN data */
	case SPRD_MUSB_EP0_STAGE_ACKWAIT:	/* zero-length data */
		ret = 0;
		break;
	default:
		dev_dbg(pmusb->controller, "ep0 request queued in state %d\n",
				pmusb->ep0_state);
		ret = -EINVAL;
		goto cleanup;
	}

	/* add request to the list */
	list_add_tail(&ep->req_list, &sprd_req->list);

	dev_dbg(pmusb->controller, "queue to %s (%s), length=%d\n",
			ep->name, ep->is_in ? "IN/TX" : "OUT/RX",
			sprd_req->request.length);

	musb_ep_select(pmusb->mregs, 0);

	if (pmusb->ep0_state == SPRD_MUSB_EP0_STAGE_TX)
		ep0_txstate(pmusb);

	else if (pmusb->ep0_state == SPRD_MUSB_EP0_STAGE_ACKWAIT) {
		if (sprd_req->request.length)
			ret = -EINVAL;
		else {
			pmusb->ep0_state = SPRD_MUSB_EP0_STAGE_STATUSIN;
			musb_writew(regs, MUSB_CSR0,
					pmusb->ackpend | MUSB_CSR0_P_DATAEND);
			pmusb->ackpend = 0;
			sprd_musb_g_ep0_giveback(ep->musb, r);
		}

	} else if (pmusb->ackpend) {
		musb_writew(regs, MUSB_CSR0, pmusb->ackpend);
		pmusb->ackpend = 0;
	}

cleanup:
	spin_unlock_irqrestore(&pmusb->lock, lockflags);
	return ret;
}

static int musb_g_ep0_dequeue(struct usb_ep *ep, struct usb_request *req)
{
	/* we just won't support this */
	return -EINVAL;
}

static int musb_g_ep0_halt(struct usb_ep *e, int value)
{
	u16			csr_reg;
	int			ret;
	unsigned long		flags;
	void __iomem		*base, *regs;
	struct musb		*pmusb;
	struct musb_ep		*musb_ep;

	if (!value || !e)
		return -EINVAL;

	musb_ep = to_musb_ep(e);
	pmusb = musb_ep->musb;
	base = pmusb->mregs;
	regs = pmusb->control_ep->regs;
	ret = 0;

	spin_lock_irqsave(&pmusb->lock, flags);

	if (!list_is_empty(&musb_ep->req_list)) {
		ret = -EBUSY;
		goto cleanup;
	}

	musb_ep_select(base, 0);
	csr_reg = pmusb->ackpend;

	switch (pmusb->ep0_state) {

	case SPRD_MUSB_EP0_STAGE_TX:		/* control-IN data */
	case SPRD_MUSB_EP0_STAGE_ACKWAIT:	/* STALL for zero-length data */
	case SPRD_MUSB_EP0_STAGE_RX:		/* control-OUT data */
		csr_reg = musb_readw(regs, MUSB_CSR0);
		/* FALLTHROUGH */

	case SPRD_MUSB_EP0_STAGE_STATUSIN:	/* control-OUT status */
	case SPRD_MUSB_EP0_STAGE_STATUSOUT:	/* control-IN status */

		csr_reg |= MUSB_CSR0_P_SENDSTALL;
		musb_writew(regs, MUSB_CSR0, csr_reg);
		pmusb->ep0_state = SPRD_MUSB_EP0_STAGE_IDLE;
		pmusb->ackpend = 0;
		break;
	default:
		dev_dbg(pmusb->controller, "ep0 can't halt in state %d\n", pmusb->ep0_state);
		ret = -EINVAL;
	}

cleanup:
	spin_unlock_irqrestore(&pmusb->lock, flags);
	return ret;
}

const struct usb_ep_ops sprd_musb_g_ep0_ops = {
	.enable		= musb_g_ep0_enable,
	.disable	= musb_g_ep0_disable,
	.alloc_request	= musb_alloc_request,
	.free_request	= musb_free_request,
	.queue		= musb_g_ep0_queue,
	.dequeue	= musb_g_ep0_dequeue,
	.set_halt	= musb_g_ep0_halt,
};
