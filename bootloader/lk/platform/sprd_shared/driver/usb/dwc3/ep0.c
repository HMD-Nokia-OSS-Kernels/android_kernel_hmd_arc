/**
 * ep0.c - DesignWare USB3 DRD Controller Endpoint 0 Handling
 */
#include <lk/list.h>
#include <linux/kernel.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/composite.h>
#include "core.h"
#include "gadget.h"
#include "io.h"

#ifndef ROUND_UP
#define ROUND_UP(a, b) (((a) + (b) - 1) / (b))
#endif

static void dwc3_ep0_do_control_data(struct dwc3 *dwc, struct dwc3_ep *dep, struct dwc3_request *req);
static void dwc3_ep0_stall_and_restart(struct dwc3 *dwc);
static int dwc3_ep0_delegate_req(struct dwc3 *dwc, struct usb_ctrlrequest *creq);
static void dwc3_ep0_set_sel_cmpl(struct usb_ep *ep, struct usb_request *req);

static int dwc3_ep0_start_transfer(struct dwc3 *dwc, u8 epnum, dma_addr_t buf_dma,
				u32 len, u32 type, unsigned chain)
{
	dwc3_cmd_params_t params;
	struct dwc3_ep  *dep;
	struct dwc3_trb *trb;
	int ret;

	dep = dwc->eps[epnum];
	if (dep->flags & SPRD_DWC3_EP_BUSY) {
		dev_vdbg(dwc->dev, "%s still busy\n", dep->name);
		return 0;
	}

	trb = &dwc->ep0_trb[dep->free_slot];

	if (chain) {
		dep->free_slot++;
	}

	trb->bph = upper_32_bits(buf_dma);
	trb->bpl = lower_32_bits(buf_dma);
	trb->ctrl = type;
	trb->size = len;

	trb->ctrl |= (SPRD_DWC3_TRB_CTRL_HWO | SPRD_DWC3_TRB_CTRL_ISP_IMI);

	if (chain) {
		trb->ctrl |= SPRD_DWC3_TRB_CTRL_CHN;
	} else {
		trb->ctrl |= (SPRD_DWC3_TRB_CTRL_IOC | SPRD_DWC3_TRB_CTRL_LST);
	}

	dwc3_flush_cache((unsigned long)trb, sizeof(*trb), 1);

	if (chain) {
		return 0;
	}

	memset(&params, 0, sizeof(params));
	params.param1 = lower_32_bits(dwc->ep0_trb_addr);
	params.param0 = upper_32_bits(dwc->ep0_trb_addr);

	ret = dwc3_send_gadget_ep_cmd(dwc, dep->number,
			SPRD_DWC3_DEPCMD_STARTTRANSFER, &params);
	if (ret < 0) {
		dev_dbg(dwc->dev, "%s STARTTRANSFER failed\n", dep->name);
		return ret;
	}

	dep->flags |= SPRD_DWC3_EP_BUSY;
	dep->resource_index = dwc3_gadget_ep_get_transfer_index(dwc,
			dep->number);

	dwc->ep0_next_event = SPRD_DWC3_EP0_COMPLETE;

	return 0;
}

static int __dwc3_ep0_start_control_status(struct dwc3_ep *dep)
{
	u32 type;
	struct dwc3 *dwc = dep->dwc;

	type = dwc->three_stage_setup ? SPRD_DWC3_TRBCTL_CONTROL_STATUS3
		: SPRD_DWC3_TRBCTL_CONTROL_STATUS2;

	return dwc3_ep0_start_transfer(dwc, dep->number,
			dwc->ctrl_req_addr, 0, type, 0);
}

static void __dwc3_ep0_do_control_status(struct dwc3 *dwc, struct dwc3_ep *dep)
{
	if (dwc->resize_fifos) {
		dev_dbg(dwc->dev, "Resizing FIFOs\n");
		dwc3_gadget_resize_tx_fifos(dwc);
		dwc->resize_fifos = 0;
	}

	WARN_ON(__dwc3_ep0_start_control_status(dep));
}

/*
 * __dwc3_gadget_ep0_queue
 */
static int __dwc3_gadget_ep0_queue(struct dwc3_ep *dep, struct dwc3_request *dreq)
{
	struct dwc3 *dwc = dep->dwc;
	unsigned direction;

	dreq->request.actual = 0;
	dreq->request.status = -EINPROGRESS;
	dreq->epnum = dep->number;

	list_add_tail(&dep->request_list, &dreq->list);

	/*
	 * Gadget driver might not be quick enough to queue a request before we
	 * get a Transfer Not Ready event on this endpoint.
	 *
	 * Set SPRD_DWC3_EP_PENDING_REQUEST flag, it's telling us that as soon as
         * Gadget queues the required request, we should kick the transfer
         * here because the IRQ we were waiting for is long gone.
	 */
	if (dep->flags & SPRD_DWC3_EP_PENDING_REQUEST) {
		direction = !!(dep->flags & SPRD_DWC3_EP0_DIR_IN);

		if (dwc->ep0state != SPRD_DWC3_EP0_STATE_DATA_PHASE) {
			dev_WARN(dwc->dev, "Unexpected pending request\n");
			return 0;
		}

		dwc3_ep0_do_control_data(dwc, dwc->eps[direction], dreq);

		dep->flags &= ~(SPRD_DWC3_EP_PENDING_REQUEST | SPRD_DWC3_EP0_DIR_IN);

		return 0;
	}

	/*
	 * If gadget driver asked us to delay the STATUS phase, handle it here.
	 */
	if (dwc->delayed_status) {
		dwc->delayed_status = false;
		direction = !dwc->ep0_expect_in;
		usb_gadget_set_state(&dwc->gadget, USB_STATE_CONFIGURED);

		if (dwc->ep0state == SPRD_DWC3_EP0_STATE_STATUS_PHASE) {
			__dwc3_ep0_do_control_status(dwc, dwc->eps[direction]);
		} else {
			dev_dbg(dwc->dev, "too early for delayed status\n");
		}
		return 0;
	}

	if (dwc->three_stage_setup) {
		direction = dwc->ep0_expect_in;
		dwc->ep0state = SPRD_DWC3_EP0_STATE_DATA_PHASE;

		dwc3_ep0_do_control_data(dwc, dwc->eps[direction], dreq);

		dep->flags &= ~SPRD_DWC3_EP0_DIR_IN;
	}

	return 0;
}

static const char *dwc3_get_ep0_state_string(enum dwc3_ep0_state state)
{
	switch (state) {
	case SPRD_DWC3_EP0_STATE_SETUP_PHASE:
		return "Setup Phase";
	case SPRD_DWC3_EP0_STATE_STATUS_PHASE:
		return "Status Phase";
	case SPRD_DWC3_EP0_STATE_DATA_PHASE:
		return "Data Phase";
	case SPRD_DWC3_EP0_STATE_UNCONNECTED:
		return "Unconnected";
	default:
		return "UNKNOWN";
	}
}

int __dwc3_gadget_ep0_set_halt(struct usb_ep *ep, int value)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3    *dwc = dep->dwc;

	dwc3_ep0_stall_and_restart(dwc);

	return 0;
}

/*
 * dwc3_gadget_ep0_set_halt
 */
int dwc3_gadget_ep0_set_halt(struct usb_ep *ep, int value)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&dwc->lock, flags);
	ret = __dwc3_gadget_ep0_set_halt(ep, value);
	spin_unlock_irqrestore(&dwc->lock, flags);

	return ret;
}

/*
 * dwc3_wIndex_to_dwc3ep(dwc, wIndex_le)
 */
static struct dwc3_ep *dwc3_wIndex_to_dwc3ep(struct dwc3 *dwc, __le16 wIndex_le)
{
	struct dwc3_ep *dep;
	u32 epnum = 0;
	u32 idx = le16_to_cpu(wIndex_le);

	epnum = (idx & USB_ENDPOINT_NUMBER_MASK) << 1;
	if (USB_DIR_IN == (idx & USB_ENDPOINT_DIR_MASK)) {
		epnum |= 1;
	}
	dep = dwc->eps[epnum];
	if (dep->flags & SPRD_DWC3_EP_ENABLED) {
		return dep;
	}
	return NULL;
}

/*
 * dwc3_ep0_stall_and_restart
 */
static void dwc3_ep0_stall_and_restart(struct dwc3 *dwc)
{
	struct dwc3_request *dreq;
	struct dwc3_ep *dep;

	/* physical ep1 reinitialize */
	dep = dwc->eps[1];
	dep->flags = SPRD_DWC3_EP_ENABLED;

	/* Always issue Stall on EP0 */
	dep = dwc->eps[0];
	dwc3_gadget_ep_set_halt_internal(dep, 1, false);
	dwc->delayed_status = false;
	dep->flags = SPRD_DWC3_EP_ENABLED;

	if (!list_is_empty(&dep->request_list)) {
		dreq = next_request(&dep->request_list);
		dwc3_gadget_giveback(dep, dreq, -ECONNRESET);
	}

	dwc->ep0state = SPRD_DWC3_EP0_STATE_SETUP_PHASE;
	dwc3_ep0_out_start(dwc);
}

/*
 * dwc3_ep0_out_start
 */
void dwc3_ep0_out_start(struct dwc3 *dwc)
{
	int ret;

	/* FIXME:
	 * When u-boot cache flush function behaves right, flush cache in
	 * transfer complete event rather than before Start Transfer Command.
	 */
	dwc3_flush_cache(dwc->ctrl_req_addr, 8, 1);

	ret = dwc3_ep0_start_transfer(dwc, 0, dwc->ctrl_req_addr, 8,
				   SPRD_DWC3_TRBCTL_CONTROL_SETUP, 0);
	WARN_ON(ret < 0);
}

/* dwc3_ep0_status_cmpl: Do Nothing */
static void dwc3_ep0_status_cmpl(struct usb_ep *ep, struct usb_request *req)
{
}

/*
 * dwc3_ep0_handle_status
 */
static int dwc3_ep0_handle_status(struct dwc3 *dwc,
		struct usb_ctrlrequest *ctrl)
{
	struct dwc3_ep *dep;
	__le16 *resp_pkt;
	u16 usb_status = 0;
	u32 recip;
	u32 val;

	recip = ctrl->bRequestType & USB_RECIP_MASK;

	switch (recip) {
	case USB_RECIP_ENDPOINT:
		dep = dwc3_wIndex_to_dwc3ep(dwc, ctrl->wIndex);
		if (!dep) {
			return -EINVAL;
		}
		if (dep->flags & SPRD_DWC3_EP_STALL) {
			usb_status = 1 << USB_ENDPOINT_HALT;
		}
		break;

	case USB_RECIP_INTERFACE:
		/* Remote Wake Capable D0, Remote Wakeup D1 */
		break;

	case USB_RECIP_DEVICE:
		/* LTM will be set once we know how to set this in HW */
		usb_status |= dwc->is_selfpowered << USB_DEVICE_SELF_POWERED;

		if (dwc->speed == SPRD_DWC3_DSTS_SUPERSPEED) {
			val = dwc3_readl(dwc->regs, SPRD_DWC3_DCTL);
			if (val & SPRD_DWC3_DCTL_INITU1ENA) {
				usb_status |= 1 << USB_DEV_STAT_U1_ENABLED;
			}
			if (val & SPRD_DWC3_DCTL_INITU2ENA) {
				usb_status |= 1 << USB_DEV_STAT_U2_ENABLED;
			}
		}

		break;

	default:
		return -EINVAL;
	}

	resp_pkt = (__le16 *) dwc->setup_buf;
	*resp_pkt = cpu_to_le16(usb_status);

	dwc->ep0_usb_req.request.buf = dwc->setup_buf;
	dwc->ep0_usb_req.request.length = sizeof(*resp_pkt);
	dwc->ep0_usb_req.request.complete = dwc3_ep0_status_cmpl;
	dep = dwc->eps[0];
	dwc->ep0_usb_req.dep = dep;

	return __dwc3_gadget_ep0_queue(dep, &dwc->ep0_usb_req);
}

/*
 * dwc3_ep0_delegate_req
 */
static int dwc3_ep0_delegate_req(struct dwc3 *dwc, struct usb_ctrlrequest *creq)
{
	int ret = 0;

	spin_unlock(&dwc->lock);
	ret = dwc->gadget_driver->setup(&dwc->gadget, creq);
	spin_lock(&dwc->lock);
	return ret;
}

/*
 * dwc3_ep0_handle_feature
 */
static int dwc3_ep0_handle_feature(struct dwc3 *dwc,
		struct usb_ctrlrequest *ctrl, int set)
{
	struct dwc3_ep *dep;
	enum usb_device_state state;
	u32 recip;
	u32 reg;
	u32 wIndex;
	u32 wValue;
	int ret;

	wIndex = le16_to_cpu(ctrl->wIndex);
	wValue = le16_to_cpu(ctrl->wValue);
	state = dwc->gadget.state;
	recip = ctrl->bRequestType & USB_RECIP_MASK;

	switch (recip) {
	case USB_RECIP_DEVICE:
		switch (wValue) {
		case USB_DEVICE_REMOTE_WAKEUP:
			break;

		case USB_DEVICE_LTM_ENABLE:
			return -EINVAL;
		/*
		 * 9.4.1 says only only for SS, in AddressState only for
		 * default control pipe
		 */
		case USB_DEVICE_U1_ENABLE:
			if (state != USB_STATE_CONFIGURED) {
				return -EINVAL;
			}
			if (dwc->speed != SPRD_DWC3_DSTS_SUPERSPEED) {
				return -EINVAL;
			}
			reg = dwc3_readl(dwc->regs, SPRD_DWC3_DCTL);
			if (set) {
				reg |= SPRD_DWC3_DCTL_INITU1ENA;
			} else {
				reg &= ~SPRD_DWC3_DCTL_INITU1ENA;
			}
			dwc3_writel(dwc->regs, SPRD_DWC3_DCTL, reg);
			break;

		case USB_DEVICE_U2_ENABLE:
			if (state != USB_STATE_CONFIGURED) {
				return -EINVAL;
			}
			if (dwc->speed != SPRD_DWC3_DSTS_SUPERSPEED) {
				return -EINVAL;
			}
			reg = dwc3_readl(dwc->regs, SPRD_DWC3_DCTL);
			if (set) {
				reg |= SPRD_DWC3_DCTL_INITU2ENA;
			} else {
				reg &= ~SPRD_DWC3_DCTL_INITU2ENA;
			}
			dwc3_writel(dwc->regs, SPRD_DWC3_DCTL, reg);
			break;

		case USB_DEVICE_TEST_MODE:
			if ((wIndex & 0xff) != 0) {
				return -EINVAL;
			}
			if (!set) {
				return -EINVAL;
			}
			dwc->test_mode = true;
			dwc->test_mode_nr = wIndex >> 8;
			break;
		default:
			return -EINVAL;
		}
		break;

	case USB_RECIP_ENDPOINT:
		switch (wValue) {
		case USB_ENDPOINT_HALT:
			dep = dwc3_wIndex_to_dwc3ep(dwc, wIndex);
			if (!dep) {
				return -EINVAL;
			}
			if (set == 0 && (dep->flags & SPRD_DWC3_EP_WEDGE)) {
				break;
			}
			ret = dwc3_gadget_ep_set_halt_internal(dep, set, true);
			if (ret) {
				return -EINVAL;
			}
			break;
		default:
			return -EINVAL;
		}
		break;

	case USB_RECIP_INTERFACE:
		switch (wValue) {
		case USB_INTRF_FUNC_SUSPEND:
			break;
		default:
			return -EINVAL;
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int dwc3_ep0_set_config(struct dwc3 *dwc, struct usb_ctrlrequest *creq)
{
	enum usb_device_state state = dwc->gadget.state;
	int ret;
	u32 cfg;
	u32 reg;

	cfg = le16_to_cpu(creq->wValue);
	dwc->start_config_issued = false;

	switch (state) {
	case USB_STATE_CONFIGURED:
		ret = dwc3_ep0_delegate_req(dwc, creq);
		if (!ret && !cfg) {
			usb_gadget_set_state(&dwc->gadget, USB_STATE_ADDRESS);
		}
		break;

	case USB_STATE_ADDRESS:
		ret = dwc3_ep0_delegate_req(dwc, creq);
		/* if cfg matches and cfg is non zero */
		if (cfg && (!ret || (ret == USB_GADGET_DELAYED_STATUS))) {
			if (ret == 0) {
				usb_gadget_set_state(&dwc->gadget,
						USB_STATE_CONFIGURED);
			}

			reg = dwc3_readl(dwc->regs, SPRD_DWC3_DCTL);
			reg |= (SPRD_DWC3_DCTL_ACCEPTU1ENA | SPRD_DWC3_DCTL_ACCEPTU2ENA);
			dwc3_writel(dwc->regs, SPRD_DWC3_DCTL, reg);

			dwc->resize_fifos = true;
			dev_dbg(dwc->dev, "resize FIFOs flag SET\n");
		}
		break;

	case USB_STATE_DEFAULT:
		return -EINVAL;

	default:
		ret = -EINVAL;
	}
	return ret;
}

static int dwc3_ep0_set_address(struct dwc3 *dwc, struct usb_ctrlrequest *creq)
{
	enum usb_device_state state = dwc->gadget.state;
	u32 reg = 0;
	u32 addr;

	addr = le16_to_cpu(creq->wValue);
	if (addr > 127) {
		dev_dbg(dwc->dev, "invalid device address %d\n", addr);
		return -EINVAL;
	}

	if (state == USB_STATE_CONFIGURED) {
		dev_dbg(dwc->dev, "trying to set address when configured\n");
		return -EINVAL;
	}

	reg = dwc3_readl(dwc->regs, SPRD_DWC3_DCFG);
	reg &= ~(SPRD_DWC3_DCFG_DEVADDR_MASK);
	reg |= SPRD_DWC3_DCFG_DEVADDR(addr);
	dwc3_writel(dwc->regs, SPRD_DWC3_DCFG, reg);

	if (addr) {
		usb_gadget_set_state(&dwc->gadget, USB_STATE_ADDRESS);
	} else {
		usb_gadget_set_state(&dwc->gadget, USB_STATE_DEFAULT);
	}

	return 0;
}

static int dwc3_ep0_set_isoch_delay(struct dwc3 *dwc, struct usb_ctrlrequest *creq)
{
	u16 wValue;
	u16 wIndex;
	u16 wLength;

	wValue = le16_to_cpu(creq->wValue);
	wIndex = le16_to_cpu(creq->wIndex);
	wLength = le16_to_cpu(creq->wLength);

	if (wIndex || wLength) {
		return -EINVAL;
	}
	/*
	 * REVISIT It's unclear from Databook what to do with this value.
	 * For now, just cache it.
	 */
	dwc->isoch_delay = wValue;

	return 0;
}

/*
 * dwc3_ep0_set_sel
 */
static int dwc3_ep0_set_sel(struct dwc3 *dwc, struct usb_ctrlrequest *creq)
{
	enum usb_device_state state = dwc->gadget.state;
	struct dwc3_ep *dep;
	u16 wLength;

	if (USB_STATE_DEFAULT == state) {
		return -EINVAL;
	}

	wLength = le16_to_cpu(creq->wLength);

	if (wLength != 6) {
		dev_err(dwc->dev, "set SEL should be 6 bytes, got %d\n", wLength);
		return -EINVAL;
	}

	/*
	 * To handle Set SEL we need to receive 6 bytes from Host.
	 * So let's queue a usb_request for 6 bytes.
	 *
	 * Though, this controller can't handle non-wMaxPacketSize aligned
	 * transfers on the OUT direction, so we queue a request for
	 * wMaxPacketSize instead.
	 */
	dep = dwc->eps[0];
	dwc->ep0_usb_req.dep = dep;
	dwc->ep0_usb_req.request.buf = dwc->setup_buf;
	dwc->ep0_usb_req.request.length = dep->endpoint.maxpacket;
	dwc->ep0_usb_req.request.complete = dwc3_ep0_set_sel_cmpl;

	return __dwc3_gadget_ep0_queue(dep, &dwc->ep0_usb_req);
}

/*
 * dwc3_ep0_set_sel_cmpl
 */
static void dwc3_ep0_set_sel_cmpl(struct usb_ep *ep, struct usb_request *req)
{
	int ret = 0;
	u32 reg = 0;
	u32 param = 0;
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3    *dwc = dep->dwc;

	struct timing {
		u8  u1sel;
		u8  u1pel;
		u16 u2sel;
		u16 u2pel;
	} __packed timing;

	memcpy(&timing, req->buf, sizeof(timing));

	dwc->u2sel = le16_to_cpu(timing.u2sel);
	dwc->u2pel = le16_to_cpu(timing.u2pel);
	dwc->u1sel = timing.u1sel;
	dwc->u1pel = timing.u1pel;

	reg = dwc3_readl(dwc->regs, SPRD_DWC3_DCTL);
	if (reg & SPRD_DWC3_DCTL_INITU2ENA) {
		param = dwc->u2pel;
	}
	if (reg & SPRD_DWC3_DCTL_INITU1ENA) {
		param = dwc->u1pel;
	}
	/*
	 * according to Synopsys Databook, if parameter is greater than 125
	 * a value of zero should be programmed in the register.
	 */
	if (param > 125) {
		param = 0;
	}

	/* issue DGCMD Set Sel */
	ret = dwc3_send_gadget_generic_command(dwc,
			SPRD_DWC3_DGCMD_SET_PERIODIC_PAR, param);
	WARN_ON(ret < 0);
}

/*
 * dwc3_ep0_standard_req
 */
static int dwc3_ep0_standard_req(struct dwc3 *dwc, struct usb_ctrlrequest *creq)
{
	int ret;

	switch (creq->bRequest) {
	case USB_REQ_SET_FEATURE:
		ret = dwc3_ep0_handle_feature(dwc, creq, 1);
		break;
	case USB_REQ_CLEAR_FEATURE:
		ret = dwc3_ep0_handle_feature(dwc, creq, 0);
		break;
	case USB_REQ_GET_STATUS:
		ret = dwc3_ep0_handle_status(dwc, creq);
		break;
	case USB_REQ_SET_ADDRESS:
		ret = dwc3_ep0_set_address(dwc, creq);
		break;
	case USB_REQ_SET_SEL:
		ret = dwc3_ep0_set_sel(dwc, creq);
		break;
	case USB_REQ_SET_CONFIGURATION:
		ret = dwc3_ep0_set_config(dwc, creq);
		break;
	case USB_REQ_SET_ISOCH_DELAY:
		ret = dwc3_ep0_set_isoch_delay(dwc, creq);
		break;
	default:
		ret = dwc3_ep0_delegate_req(dwc, creq);
		break;
	}

	return ret;
}

/*
 * dwc3_ep0_inspect_setup
 */
static void dwc3_ep0_inspect_setup(struct dwc3 *dwc)
{
	struct usb_ctrlrequest *creq = dwc->ctrl_req;
	u32 length = 0;
	int ret = -EINVAL;

	if (!dwc->gadget_driver) {
		goto out;
	}

	length = le16_to_cpu(creq->wLength);
	if (!length) {
		dwc->ep0_next_event = SPRD_DWC3_EP0_NRDY_STATUS;
		dwc->ep0_expect_in = false;
		dwc->three_stage_setup = false;
	} else {
		dwc->ep0_next_event = SPRD_DWC3_EP0_NRDY_DATA;
		dwc->ep0_expect_in = !!(creq->bRequestType & USB_DIR_IN);
		dwc->three_stage_setup = true;
	}

	if ((creq->bRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD) {
		ret = dwc3_ep0_standard_req(dwc, creq);
	} else {
		ret = dwc3_ep0_delegate_req(dwc, creq);
	}
	if (ret == USB_GADGET_DELAYED_STATUS) {
		dwc->delayed_status = true;
	}
out:
	if (ret < 0) {
		dwc3_ep0_stall_and_restart(dwc);
	}
}

/*
 * dwc3_ep0_complete_status
 */
static void dwc3_ep0_complete_status(struct dwc3 *dwc,
		const struct dwc3_event_depevt *event)
{
	struct dwc3_request *req;
	struct dwc3_trb	*trb;
	struct dwc3_ep  *dep;
	u32 status;

	trb = dwc->ep0_trb;
	dep = dwc->eps[0];

	if (!list_is_empty(&dep->request_list)) {
		req = next_request(&dep->request_list);
		dwc3_gadget_giveback(dep, req, 0);
	}

	if (dwc->test_mode) {
		int ret;

		ret = dwc3_gadget_set_test_mode(dwc, dwc->test_mode_nr);
		if (ret < 0) {
			dev_dbg(dwc->dev, "Invalid Test#%d\n", dwc->test_mode_nr);
			dwc3_ep0_stall_and_restart(dwc);
			return;
		}
	}

	status = SPRD_DWC3_TRB_SIZE_TRBSTS(trb->size);
	if (status == SPRD_DWC3_TRBSTS_SETUP_PENDING) {
		dev_dbg(dwc->dev, "Setup Pending received\n");
	}
	dwc->ep0state = SPRD_DWC3_EP0_STATE_SETUP_PHASE;
	dwc3_ep0_out_start(dwc);
}

/*
 * dwc3_ep0_complete_data
 */
static void dwc3_ep0_complete_data(struct dwc3 *dwc,
		const struct dwc3_event_depevt *event)
{
	struct usb_request  *ureq;
	struct dwc3_request *dreq = NULL;
	struct dwc3_trb     *trb;
	struct dwc3_ep      *ep0;
	unsigned xfer_size = 0;
	unsigned maxp;
	void *buf;
	u32 transferred = 0;
	u32 length;
	u32 status;
	u8 epnum;

	trb = dwc->ep0_trb;
	ep0 = dwc->eps[0];
	epnum = event->endpoint_number;
	dwc->ep0_next_event = SPRD_DWC3_EP0_NRDY_STATUS;

	dreq = next_request(&ep0->request_list);
	if (!dreq) {
		return;
	}

	dwc3_flush_cache((unsigned long)trb, sizeof(*trb), 0);

	status = SPRD_DWC3_TRB_SIZE_TRBSTS(trb->size);
	if (status == SPRD_DWC3_TRBSTS_SETUP_PENDING) {
		dev_dbg(dwc->dev, "Setup Pending received\n");

		if (dreq) {
			dwc3_gadget_giveback(ep0, dreq, -ECONNRESET);
		}
		return;
	}

	ureq = &dreq->request;
	buf = ureq->buf;

	length = trb->size & SPRD_DWC3_TRB_SIZE_MASK;

	maxp = ep0->endpoint.maxpacket;

	if (dwc->ep0_bounced) {
		/*
		 * Handle the first TRB before handling the bounce buffer if
		 * the request length is greater than the bounce buffer size.
		 */
		if (ureq->length > SPRD_DWC3_EP0_BOUNCE_SIZE) {
			xfer_size = (ureq->length / maxp) * maxp;
			transferred = xfer_size - length;
			buf = (u8 *)buf + transferred;
			ureq->actual += transferred;

			trb++;
			dwc3_flush_cache((unsigned long)trb, sizeof(*trb), 0);
			length = trb->size & SPRD_DWC3_TRB_SIZE_MASK;

			ep0->free_slot = 0;
		}

		xfer_size = ROUND_UP((ureq->length - xfer_size), maxp);
		transferred = min_t(u32, ureq->length - transferred, xfer_size - length);
		dwc3_flush_cache((unsigned long)dwc->ep0_bounce, SPRD_DWC3_EP0_BOUNCE_SIZE, 0);
		memcpy(buf, dwc->ep0_bounce, transferred);
	} else {
		transferred = ureq->length - length;
	}

	ureq->actual += transferred;

	if ((epnum & 1) && ureq->actual < ureq->length) {
		/* for some reason, we did not get everything out */
		dwc3_ep0_stall_and_restart(dwc);
	} else {
		dwc3_gadget_giveback(ep0, dreq, 0);

		if (IS_ALIGNED(ureq->length, ep0->endpoint.maxpacket) &&
				ureq->length && ureq->zero) {
			int ret;

			dwc->ep0_next_event = SPRD_DWC3_EP0_COMPLETE;

			ret = dwc3_ep0_start_transfer(dwc, epnum,
					dwc->ctrl_req_addr, 0,
					SPRD_DWC3_TRBCTL_CONTROL_DATA, 0);
			WARN_ON(ret < 0);
		}
	}
}

/*
 * dwc3_ep0_xfer_complete
 */
static void dwc3_ep0_xfer_complete(struct dwc3 *dwc,
			const struct dwc3_event_depevt *evt)
{
	struct dwc3_ep *dep = dwc->eps[evt->endpoint_number];

	dep->flags &= ~SPRD_DWC3_EP_BUSY;
	dep->resource_index = 0;
	dwc->setup_packet_pending = false;

	switch (dwc->ep0state) {
	case SPRD_DWC3_EP0_STATE_DATA_PHASE:
		dwc3_ep0_complete_data(dwc, evt);
		break;

	case SPRD_DWC3_EP0_STATE_SETUP_PHASE:
		dwc3_ep0_inspect_setup(dwc);
		break;

	case SPRD_DWC3_EP0_STATE_STATUS_PHASE:
		dwc3_ep0_complete_status(dwc, evt);
		break;
	default:
		WARN(true, "UNKNOWN ep0state %d\n", dwc->ep0state);
	}
}

/*
 * dwc3_ep0_do_control_data
 */
static void dwc3_ep0_do_control_data(struct dwc3 *dwc,
		struct dwc3_ep *dep, struct dwc3_request *dreq)
{
	unsigned dreq_len = dreq->request.length;
	u32 max_packet = dep->endpoint.maxpacket;
	u32 xfer_size = 0;
	u8 depnum = dep->number;
	int ret;

	dreq->direction = !!depnum;

	if (dreq_len == 0) {
		ret = dwc3_ep0_start_transfer(dwc, depnum,
					   dwc->ctrl_req_addr, 0,
					   SPRD_DWC3_TRBCTL_CONTROL_DATA, 0);
	} else if (!IS_ALIGNED(dreq_len, max_packet) && (depnum == 0)) {
		ret = usb_gadget_map_request(&dwc->gadget, &dreq->request,
				depnum);
		if (ret) {
			dev_dbg(dwc->dev, "failed to map request\n");
			return;
		}

		/* TODO: remove if values never be changed */
		max_packet = dep->endpoint.maxpacket;
		dreq_len = dreq->request.length;

		if (dreq_len > SPRD_DWC3_EP0_BOUNCE_SIZE) {
			xfer_size = (dreq_len / max_packet) * max_packet;
			ret = dwc3_ep0_start_transfer(dwc, depnum,
						   dreq->request.dma,
						   xfer_size,
						   SPRD_DWC3_TRBCTL_CONTROL_DATA, 1);
		}

		xfer_size = ROUND_UP((dreq->request.length - xfer_size), max_packet);

		dwc->ep0_bounced = true;

		/*
		 * REVIST If request length > WC3_EP0_BOUNCE_SIZE
		 * we will need two chained TRBs to handle the transfer
		 */
		ret = dwc3_ep0_start_transfer(dwc, depnum,
					   dwc->ep0_bounce_addr, xfer_size,
					   SPRD_DWC3_TRBCTL_CONTROL_DATA, 0);
	} else {
		ret = usb_gadget_map_request(&dwc->gadget, &dreq->request, depnum);
		if (ret) {
			dev_dbg(dwc->dev, "failed to map request\n");
			return;
		}

		ret = dwc3_ep0_start_transfer(dwc, depnum, dreq->request.dma,
					   dreq->request.length,
					   SPRD_DWC3_TRBCTL_CONTROL_DATA, 0);
	}

	WARN_ON(ret < 0);
}

static void __dwc3_ep0_end_control_data(struct dwc3 *dwc, struct dwc3_ep *dep)
{
	dwc3_cmd_params_t params;
	int ret;
	u32 cmd;

	if (!dep->resource_index) {
		return;
	}

	memset(&params, 0, sizeof(params));

	cmd = (SPRD_DWC3_DEPCMD_ENDTRANSFER | SPRD_DWC3_DEPCMD_CMDIOC);
	cmd |= SPRD_DWC3_DEPCMD_PARAM(dep->resource_index);
	ret = dwc3_send_gadget_ep_cmd(dwc, dep->number, cmd, &params);

	WARN_ON_ONCE(ret);
	dep->resource_index = 0;
}

static void dwc3_ep0_do_control_status(struct dwc3 *dwc,
				const struct dwc3_event_depevt *evt)
{
	struct dwc3_ep *dep = dwc->eps[evt->endpoint_number];
	__dwc3_ep0_do_control_status(dwc, dep);
}

/*
 * dwc3_ep0_xfer_notready
 */
static void dwc3_ep0_xfer_notready(struct dwc3 *dwc,
		const struct dwc3_event_depevt *evt)
{
	dwc->setup_packet_pending = true;

	switch (evt->status) {
	case DEPEVT_STATUS_CONTROL_STATUS:
		if (dwc->ep0_next_event != SPRD_DWC3_EP0_NRDY_STATUS) {
			return;
		}
		dev_vdbg(dwc->dev, "Control Status\n");

		dwc->ep0state = SPRD_DWC3_EP0_STATE_STATUS_PHASE;

		if (dwc->delayed_status) {
			dev_vdbg(dwc->dev, "Delayed Status\n");
			WARN_ON_ONCE(evt->endpoint_number != 1);
			return;
		}

		dwc3_ep0_do_control_status(dwc, evt);
		break;

	case DEPEVT_STATUS_CONTROL_DATA:
		dev_vdbg(dwc->dev, "Control Data\n");

		if (dwc->ep0_expect_in != evt->endpoint_number) {
			struct dwc3_ep *dep = dwc->eps[dwc->ep0_expect_in];

			dev_vdbg(dwc->dev, "Wrong direction for Data phase\n");
			__dwc3_ep0_end_control_data(dwc, dep);
			dwc3_ep0_stall_and_restart(dwc);
			return;
		}

		break;

	default:
		break;
	}
}

/*
 * dwc3_ep0_interrupt
 */
void dwc3_ep0_interrupt(struct dwc3 *dwc, const struct dwc3_event_depevt *evt)
{
	u8 epnum = evt->endpoint_number;

	dev_vdbg(dwc->dev, "%s while ep%d%s in state '%s'\n",
			dwc3_get_ep_event_string(evt->endpoint_event),
			epnum >> 1, (epnum & 1) ? "in" : "out",
			dwc3_get_ep0_state_string(dwc->ep0state));

	dwc3_flush_cache(dwc->ctrl_req_addr, 8, 1);
	switch (evt->endpoint_event) {
	case SPRD_DWC3_DEPEVT_EPCMDCMPLT:
	case SPRD_DWC3_DEPEVT_RXTXFIFOEVT:
	case SPRD_DWC3_DEPEVT_STREAMEVT:
	case SPRD_DWC3_DEPEVT_XFERINPROGRESS:
		break;

	case SPRD_DWC3_DEPEVT_XFERNOTREADY:
		dwc3_ep0_xfer_notready(dwc, evt);
		break;

	case SPRD_DWC3_DEPEVT_XFERCOMPLETE:
		dwc3_ep0_xfer_complete(dwc, evt);
		break;
	}
}

int dwc3_gadget_ep0_queue(struct usb_ep *ep, struct usb_request *request,
		gfp_t gfp_flags)
{
	struct dwc3_request *dreq = to_dwc3_request(request);
	struct dwc3_ep	    *dep = to_dwc3_ep(ep);
	struct dwc3	    *dwc = dep->dwc;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&dwc->lock, flags);
	if (!dep->endpoint.desc) {
		dev_dbg(dwc->dev, "trying to queue request %p to disabled %s\n",
				request, dep->name);
		ret = -ESHUTDOWN;
		goto out;
	}

	/* we share one TRB for ep0/1 */
	if (!list_is_empty(&dep->request_list)) {
		ret = -EBUSY;
		goto out;
	}

	dev_vdbg(dwc->dev, "queueing request %p to %s length %d state '%s'\n",
			request, dep->name, request->length,
			dwc3_get_ep0_state_string(dwc->ep0state));

	ret = __dwc3_gadget_ep0_queue(dep, dreq);

out:
	spin_unlock_irqrestore(&dwc->lock, flags);

	return ret;
}

