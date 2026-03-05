/*
 * gadget.c - DesignWare USB3 DRD Controller Gadget Framework Link
 */

#include <malloc.h>
#include <asm/dma-mapping.h>
#include <lk/list.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/kernel.h>
#include "core.h"
#include "gadget.h"
#include "io.h"

/*
 * Enables USB2 Test Modes
 */
int dwc3_gadget_set_test_mode(struct dwc3 *dwc, int mode)
{
	u32 val;

	val = dwc3_readl(dwc->regs, SPRD_DWC3_DCTL);
	val &= ~SPRD_DWC3_DCTL_TSTCTRL_MASK;

	switch (mode) {
	case TEST_FORCE_EN:
	case TEST_SE0_NAK:
	case TEST_PACKET:
	case TEST_K:
	case TEST_J:
		val |= mode << 1;
		break;
	default:
		return -EINVAL;
	}

	dwc3_writel(dwc->regs, SPRD_DWC3_DCTL, val);

	return 0;
}

/*
 * Gets current state of USB Link
 */
int dwc3_gadget_get_link_state(struct dwc3 *dwc)
{
	u32 val;

	val = dwc3_readl(dwc->regs, SPRD_DWC3_DSTS);

	return SPRD_DWC3_DSTS_USBLNKST(val);
}

/*
 * Sets USB Link to a particular State
 */
int dwc3_gadget_set_link_state(struct dwc3 *dwc, enum dwc3_link_state state)
{
	int retries = 10000;
	u32 val;

	if (dwc->revision >= SPRD_DWC3_REVISION_194A) {
		while (--retries) {
			val = dwc3_readl(dwc->regs, SPRD_DWC3_DSTS);
			if (val & SPRD_DWC3_DSTS_DCNRD) {
				udelay(5);
			} else {
				break;
			}
		}

		if (retries <= 0) {
			return -ETIMEDOUT;
		}
	}

	val = dwc3_readl(dwc->regs, SPRD_DWC3_DCTL);
	val &= ~SPRD_DWC3_DCTL_ULSTCHNGREQ_MASK;

	/* set requested state */
	val |= SPRD_DWC3_DCTL_ULSTCHNGREQ(state);
	dwc3_writel(dwc->regs, SPRD_DWC3_DCTL, val);

	/*
	 * The following code is racy when called from
	 * dwc3_gadget_wakeup, and is not needed, at least
	 * on newer versions
	 */
	if (dwc->revision >= SPRD_DWC3_REVISION_194A) {
		return 0;
	}

	/* wait for a change in DSTS */
	retries = 10000;
	while (--retries) {
		val = dwc3_readl(dwc->regs, SPRD_DWC3_DSTS);

		if (SPRD_DWC3_DSTS_USBLNKST(val) == state) {
			return 0;
		}
		udelay(5);
	}

	dev_vdbg(dwc->dev, "link state change request timeout\n");

	return -ETIMEDOUT;
}

/*
 * reallocate fifo spaces for current use-case
 */
int dwc3_gadget_resize_tx_fifos(struct dwc3 *dwc)
{
	int fifo_depth_last = 0;
	int fifo_size;
	int mdwidth;
	int num;

	if (!dwc->needs_fifo_resize) {
		return 0;
	}

	/* change MDWIDTH bits to bytes */
	mdwidth = SPRD_DWC3_MDWIDTH(dwc->hwparams.hwparams0);
	mdwidth >>= 3;

	/*
	 * For now we will only allocate 1 wMaxPacketSize space for each
	 * enabled endpoint, later patches will come to improve this
	 * algorithm so that we better use the internal FIFO space
	 */
	for (num = 0; num < dwc->num_in_eps; num++) {
		/* bit0: direction, 1 means IN ep */
		struct dwc3_ep	*dep = dwc->eps[(num << 1) | 1];
		int temp = 0;
		int multi = 1;

		if (!(dep->flags & SPRD_DWC3_EP_ENABLED)) {
			continue;
		}
		if (usb_endpoint_xfer_bulk(dep->endpoint.desc)
				|| usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
			multi = 3;
		}

		/* REVISIT */
		temp = multi * (dep->endpoint.maxpacket + mdwidth);
		temp += mdwidth;

		fifo_size = DIV_ROUND_UP(temp, mdwidth);
		fifo_size |= (fifo_depth_last << 16);

		dev_vdbg(dwc->dev, "%s: FIFO addr %04x size %d\n",
				dep->name, fifo_depth_last, fifo_size & 0xffff);

		dwc3_writel(dwc->regs, SPRD_DWC3_GTXFIFOSIZ(num), fifo_size);

		fifo_depth_last += (fifo_size & 0xffff);
	}

	return 0;
}

/*
 * dwc3_gadget_giveback
 */
void dwc3_gadget_giveback(struct dwc3_ep *dep, struct dwc3_request *dreq,
		int status)
{
	struct dwc3 *dwc = dep->dwc;

	if (dreq->queued) {
		dep->busy_slot++;
		/* Skip LINK TRB */
		if (((dep->busy_slot & SPRD_DWC3_TRB_MASK) ==
			SPRD_DWC3_TRB_NUM- 1) &&
			usb_endpoint_xfer_isoc(dep->endpoint.desc))
			dep->busy_slot++;
		dreq->queued = false;
	}

	list_delete(&dreq->list);
	dreq->trb = NULL;

	if (-EINPROGRESS == dreq->request.status) {
		dreq->request.status = status;
	}

	if (dep->number == 0 && dwc->ep0_bounced) {
		dwc->ep0_bounced = false;
	} else {
		usb_gadget_unmap_request(&dwc->gadget, &dreq->request,
				dreq->direction);
	}

	dev_vdbg(dwc->dev, "request %p from %s completed %d/%d ===> %d\n",
			dreq, dep->name, dreq->request.actual,
			dreq->request.length, status);

	spin_unlock(&dwc->lock);
	usb_gadget_giveback_request(&dep->endpoint, &dreq->request);
	spin_lock(&dwc->lock);
}

/*
 * dwc3_send_gadget_generic_command
 */
int dwc3_send_gadget_generic_command(struct dwc3 *dwc, unsigned cmd, u32 param)
{
	u32 timeout = 500;
	u32 val;

	dwc3_writel(dwc->regs, SPRD_DWC3_DGCMDPAR, param);
	dwc3_writel(dwc->regs, SPRD_DWC3_DGCMD, cmd | SPRD_DWC3_DGCMD_CMDACT);

	do {
		val = dwc3_readl(dwc->regs, SPRD_DWC3_DGCMD);
		if (!(val & SPRD_DWC3_DGCMD_CMDACT)) {
			dev_vdbg(dwc->dev, "Dev Command %d Complete --> %d\n",
					cmd, SPRD_DWC3_DGCMD_STATUS(val));
			return 0;
		}

		/*
		 * We can't sleep here, for called from interrupt context.
		 */
		timeout--;
		if (!timeout) {
			return -ETIMEDOUT;
		}
		udelay(1);
	} while (1);
}

int dwc3_send_gadget_ep_cmd(struct dwc3 *dwc, unsigned ep,
		unsigned cmd, dwc3_cmd_params_t *params)
{
	u32 timeout = 500;
	u32 val;

	dwc3_writel(dwc->regs, SPRD_DWC3_DEPCMDPAR0(ep), params->param0);
	dwc3_writel(dwc->regs, SPRD_DWC3_DEPCMDPAR1(ep), params->param1);
	dwc3_writel(dwc->regs, SPRD_DWC3_DEPCMDPAR2(ep), params->param2);

	dwc3_writel(dwc->regs, SPRD_DWC3_DEPCMD(ep), cmd | SPRD_DWC3_DEPCMD_CMDACT);
	do {
		val = dwc3_readl(dwc->regs, SPRD_DWC3_DEPCMD(ep));
		if (!(val & SPRD_DWC3_DEPCMD_CMDACT)) {
			dev_vdbg(dwc->dev, "EP Command %d Complete --> %d\n",
					cmd, SPRD_DWC3_DEPCMD_STATUS(val));
			return 0;
		}

		/*
		 * We can't sleep here, for called from interrupt context.
		 */
		timeout--;
		if (!timeout) {
			return -ETIMEDOUT;
		}
		udelay(1);
	} while (1);
}

/*
 * dwc3_free_trb_pool
 */
static void dwc3_free_trb_pool(struct dwc3_ep *dep)
{
	dma_free_coherent(dep->trb_pool);
	dep->trb_pool = NULL;
	dep->trb_pool_dma = 0;
}

/*
 * dwc3_alloc_trb_pool
 */
static int dwc3_alloc_trb_pool(struct dwc3_ep *dep)
{
	if (dep->trb_pool || dep->number == 0 || dep->number == 1) {
		return 0;
	}

	dep->trb_pool = dma_alloc_coherent(sizeof(struct dwc3_trb) *
					   SPRD_DWC3_TRB_NUM,
					   (unsigned long *)&dep->trb_pool_dma);
	if (!dep->trb_pool) {
		dev_err(dep->dwc->dev, "fail to alloc trb pool for %s\n",
				dep->name);
		return -ENOMEM;
	}

	return 0;
}

/*
 * dwc3_trb_dma_offset
 */
static dma_addr_t dwc3_trb_dma_offset(struct dwc3_ep *dep,
		struct dwc3_trb *trb)
{
	u32 offset = (char *)trb - (char *)dep->trb_pool;
	return dep->trb_pool_dma + offset;
}

/*
 * dwc3_gadget_start_config
 */
static int dwc3_gadget_start_config(struct dwc3 *dwc, struct dwc3_ep *dep)
{
	dwc3_cmd_params_t dparams;
	u32 cmd;

	memset(&dparams, 0x00, sizeof(dparams));

	if (1 != dep->number) {
		cmd = SPRD_DWC3_DEPCMD_DEPSTARTCFG;
		/* 0 for ep0, and 2 for the remaining */
		if (dep->number > 1) {
			if (dwc->start_config_issued) {
				return 0;
			}
			cmd |= SPRD_DWC3_DEPCMD_PARAM(2);
			dwc->start_config_issued = true;
		}

		return dwc3_send_gadget_ep_cmd(dwc, 0, cmd, &dparams);
	}

	return 0;
}

/*
 * dwc3_gadget_set_ep_config
 */
static int dwc3_gadget_set_ep_config(struct dwc3 *dwc, struct dwc3_ep *dep,
		const struct usb_endpoint_descriptor *desc,
		const struct usb_ss_ep_comp_descriptor *comp_desc,
		bool ignore, bool restore)
{
	dwc3_cmd_params_t dparams;

	memset(&dparams, 0x00, sizeof(dparams));

	dparams.param0 = SPRD_DWC3_DEP_CFG_EP_TYPE(usb_endpoint_type(desc))
		| SPRD_DWC3_DEP_CFG_MAX_PACKET_SIZE(usb_endpoint_maxp(desc));

	/* only SuperSpeed mode need Burst */
	if (USB_SPEED_SUPER == dwc->gadget.speed) {
		u32 burst = dep->endpoint.maxburst - 1;
		dparams.param0 |= SPRD_DWC3_DEP_CFG_BURST_SIZE(burst);
	}

	if (ignore) {
		dparams.param0 |= SPRD_DWC3_DEP_CFG_IGN_SEQ_NUM;
	}

	if (restore) {
		dparams.param0 |= SPRD_DWC3_DEP_CFG_ACTION_RESTORE;
		dparams.param2 |= dep->saved_state;
	}

	dparams.param1 = SPRD_DWC3_DEP_CFG_XFER_COMPLETE_EN |
		SPRD_DWC3_DEP_CFG_XFER_NOT_READY_EN;

	if (usb_ss_max_streams(comp_desc) && usb_endpoint_xfer_bulk(desc)) {
		dparams.param1 |= SPRD_DWC3_DEP_CFG_STREAM_CAPABLE
			| SPRD_DWC3_DEP_CFG_STREAM_EVENT_EN;
		dep->stream_capable = true;
	}

	if (!usb_endpoint_xfer_control(desc)) {
		dparams.param1 |= SPRD_DWC3_DEP_CFG_XFER_IN_PROGRESS_EN;
	}

	/*
	 * 1:1 mapping for endpoints
	 * We consider the direction bit as part of the physical ep number.
	 * So USB endpoint 0x81 is 0x03.
	 */
	dparams.param1 |= SPRD_DWC3_DEP_CFG_EP_NUMBER(dep->number);

	/* Must use the lower 16 TX FIFOs even though HW might have more */
	if (dep->direction) {
		dparams.param0 |= SPRD_DWC3_DEP_CFG_FIFO_NUMBER(dep->number >> 1);
	}

	if (desc->bInterval) {
		dparams.param1 |= SPRD_DWC3_DEP_CFG_BINTERVAL_M1(desc->bInterval - 1);
		dep->interval = 1 << (desc->bInterval - 1);
	}

	return dwc3_send_gadget_ep_cmd(dwc, dep->number,
			SPRD_DWC3_DEPCMD_SETEPCONFIG, &dparams);
}

/*
 * dwc3_gadget_set_xfer_resource
 */
static int dwc3_gadget_set_xfer_resource(struct dwc3 *dwc, struct dwc3_ep *dep)
{
	dwc3_cmd_params_t dparams;

	memset(&dparams, 0x00, sizeof(dparams));
	dparams.param0 = SPRD_DWC3_DEPXFERCFG_NUM_XFER_RES(1);

	return dwc3_send_gadget_ep_cmd(dwc, dep->number,
			SPRD_DWC3_DEPCMD_SETTRANSFRESOURCE, &dparams);
}

/*
 * Initializes a HW endpoint
 */
static int dwc3_gadget_dep_enable(struct dwc3_ep *dep,
		const struct usb_endpoint_descriptor *desc,
		const struct usb_ss_ep_comp_descriptor *compdesc,
		bool ignore, bool restore)
{
	struct dwc3 *dwc = dep->dwc;
	int err;
	u32 val;

	dev_vdbg(dwc->dev, "enabling %s\n", dep->name);

	if (!(dep->flags & SPRD_DWC3_EP_ENABLED)) {
		err = dwc3_gadget_start_config(dwc, dep);
		if (err) {
			return err;
		}
	}

	err = dwc3_gadget_set_ep_config(dwc, dep, desc, compdesc, ignore,
			restore);
	if (err) {
		return err;
	}

	if (!(dep->flags & SPRD_DWC3_EP_ENABLED)) {
		struct dwc3_trb	*trb_end;
		struct dwc3_trb	*trb_first;

		err = dwc3_gadget_set_xfer_resource(dwc, dep);
		if (err) {
			return err;
		}

		dep->comp_desc = compdesc;
		dep->endpoint.desc = desc;
		dep->type = usb_endpoint_type(desc);
		dep->flags |= SPRD_DWC3_EP_ENABLED;

		val = dwc3_readl(dwc->regs, SPRD_DWC3_DALEPENA);
		val |= SPRD_DWC3_DALEPENA_EP(dep->number);
		dwc3_writel(dwc->regs, SPRD_DWC3_DALEPENA, val);

		if (!usb_endpoint_xfer_isoc(desc)) {
			return 0;
		}

		trb_first = &dep->trb_pool[0];
		trb_end = &dep->trb_pool[SPRD_DWC3_TRB_NUM - 1];
		memset(trb_end, 0, sizeof(*trb_end));

		trb_end->bph = upper_32_bits(dwc3_trb_dma_offset(dep, trb_first));
		trb_end->bpl = lower_32_bits(dwc3_trb_dma_offset(dep, trb_first));
		trb_end->ctrl |= SPRD_DWC3_TRB_CTRL_HWO;
		trb_end->ctrl |= SPRD_DWC3_TRBCTL_LINK_TRB;
	}

	return 0;
}

/*
 * dwc3_stop_active_transfer
 */
static void dwc3_stop_active_transfer(struct dwc3 *dwc, u32 epnum, bool force)
{
	dwc3_cmd_params_t params;
	struct dwc3_ep *dep;
	int ret;
	u32 cmd;

	dep = dwc->eps[epnum];

	if (!dep->resource_index) {
		return;
	}

	/* NOTICE */
	cmd = SPRD_DWC3_DEPCMD_ENDTRANSFER;
	cmd |= force ? SPRD_DWC3_DEPCMD_HIPRI_FORCERM : 0;
	cmd |= SPRD_DWC3_DEPCMD_CMDIOC;
	cmd |= SPRD_DWC3_DEPCMD_PARAM(dep->resource_index);

	memset(&params, 0, sizeof(params));
	ret = dwc3_send_gadget_ep_cmd(dwc, dep->number, cmd, &params);
	WARN_ON_ONCE(ret);
	dep->resource_index = 0;
	dep->flags &= ~SPRD_DWC3_EP_BUSY;
	udelay(100);
}

/*
 * dwc3_remove_requests
 */
static void dwc3_remove_requests(struct dwc3 *dwc, struct dwc3_ep *dep)
{
	struct dwc3_request *dreq;

	if (!list_is_empty(&dep->req_queued)) {
		dwc3_stop_active_transfer(dwc, dep->number, true);

		/* giveback all requests to gadget driver */
		while (!list_is_empty(&dep->req_queued)) {
			dreq = next_request(&dep->req_queued);
			dwc3_gadget_giveback(dep, dreq, -ESHUTDOWN);
		}
	}

	while (!list_is_empty(&dep->request_list)) {
		dreq = next_request(&dep->request_list);
		dwc3_gadget_giveback(dep, dreq, -ESHUTDOWN);
	}
}

/*
 * Disables a HW endpoint
 */
static int dwc3_gadget_dep_disable(struct dwc3_ep *dep)
{
	struct dwc3 *dwc = dep->dwc;
	u32 val;

	dwc3_remove_requests(dwc, dep);

	/* Make sure HW endpoint isn't stalled */
	if (dep->flags & SPRD_DWC3_EP_STALL) {
		dwc3_gadget_ep_set_halt_internal(dep, 0, false);
	}

	val = dwc3_readl(dwc->regs, SPRD_DWC3_DALEPENA);
	val &= ~SPRD_DWC3_DALEPENA_EP(dep->number);
	dwc3_writel(dwc->regs, SPRD_DWC3_DALEPENA, val);

	dep->comp_desc = NULL;
	dep->endpoint.desc = NULL;
	dep->stream_capable = false;
	dep->flags = 0;
	dep->type = 0;

	return 0;
}

/*
 * dwc3_gadget_ep0_disable
 */
static int dwc3_gadget_ep0_disable(struct usb_ep *ep)
{
	return -EINVAL;
}

/*
 * dwc3_gadget_ep0_enable
 */
static int dwc3_gadget_ep0_enable(struct usb_ep *ep,
		const struct usb_endpoint_descriptor *desc)
{
	return -EINVAL;
}

/*
 * dwc3_gadget_ep_disable
 */
static int dwc3_gadget_ep_disable(struct usb_ep *ep)
{
	struct dwc3_ep *dep;
	struct dwc3    *dwc;
	unsigned long flag;
	int ret;

	if (!ep) {
		pr_debug("dwc3: invalid parameter\n");
		return -EINVAL;
	}

	dep = to_dwc3_ep(ep);
	dwc = dep->dwc;

	if (!(dep->flags & SPRD_DWC3_EP_ENABLED)) {
		WARN(true, "%s is already disabled !\n", dep->name);
		return 0;
	}

	snprintf(dep->name, sizeof(dep->name), "ep%d%s",
			dep->number >> 1, (dep->number & 1) ? "in" : "out");

	spin_lock_irqsave(&dwc->lock, flag);
	ret = dwc3_gadget_dep_disable(dep);
	spin_unlock_irqrestore(&dwc->lock, flag);

	return ret;
}

/*
 * dwc3_gadget_ep_enable
 */
static int dwc3_gadget_ep_enable(struct usb_ep *ep,
		const struct usb_endpoint_descriptor *desc)
{
	struct dwc3_ep *dep;
	struct dwc3    *dwc;
	unsigned long flag;
	int ret;

	if (!ep || !desc || USB_DT_ENDPOINT != desc->bDescriptorType) {
		pr_debug("dwc3: invalid parameter !\n");
		return -EINVAL;
	}

	if (!get_unaligned(&desc->wMaxPacketSize)) {
		pr_debug("dwc3: missing wMaxPacketSize !\n");
		return -EINVAL;
	}

	dep = to_dwc3_ep(ep);
	dwc = dep->dwc;

	if (SPRD_DWC3_EP_ENABLED & dep->flags) {
		WARN(true, "%s is already enabled\n", dep->name);
		return 0;
	}

	switch (usb_endpoint_type(desc)) {
	case USB_ENDPOINT_XFER_INT:
		strlcat(dep->name, "-int", sizeof(dep->name));
		break;

	case USB_ENDPOINT_XFER_BULK:
		strlcat(dep->name, "-bulk", sizeof(dep->name));
		break;

	case USB_ENDPOINT_XFER_ISOC:
		strlcat(dep->name, "-isoc", sizeof(dep->name));
		break;

	case USB_ENDPOINT_XFER_CONTROL:
		strlcat(dep->name, "-control", sizeof(dep->name));
		break;

	default:
		dev_err(dwc->dev, "invalid endpoint transfer type\n");
	}

	spin_lock_irqsave(&dwc->lock, flag);
	ret = dwc3_gadget_dep_enable(dep, desc, ep->comp_desc, false, false);
	spin_unlock_irqrestore(&dwc->lock, flag);

	return ret;
}

/*
 * dwc3_gadget_ep_free_request
 */
static void dwc3_gadget_ep_free_request(struct usb_ep *ep,
		struct usb_request *request)
{
	struct dwc3_request *req = to_dwc3_request(request);
	free(req);
}

/*
 * dwc3_gadget_ep_alloc_request
 */
static struct usb_request *dwc3_gadget_ep_alloc_request(struct usb_ep *ep,
	gfp_t gfp_flags)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3_request *dreq;

	dreq = kzalloc(sizeof(*dreq), gfp_flags);
	if (!dreq) {
		return NULL;
	}

	dreq->dep = dep;
	dreq->epnum = dep->number;

	return &dreq->request;
}

/*
 * setup one TRB from one request
 */
static void dwc3_prepare_one_trb(struct dwc3_ep *dep,
		struct dwc3_request *dreq, dma_addr_t dma,
		unsigned length, unsigned last, unsigned chain, unsigned node)
{
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_trb *dtrb;

	dev_vdbg(dwc->dev, "%s: req %p dma %08llx length %d%s%s\n",
			dep->name, dreq, (unsigned long long)dma,
			length, last ? " last" : "", chain ? " chain" : "");

	dtrb = &dep->trb_pool[dep->free_slot & SPRD_DWC3_TRB_MASK];

	if (!dreq->trb) {
		dwc3_gadget_move_request_queued(dreq);
		dreq->start_slot = dep->free_slot & SPRD_DWC3_TRB_MASK;
		dreq->trb_dma = dwc3_trb_dma_offset(dep, dtrb);
		dreq->trb = dtrb;
	}

	dep->free_slot++;

	/* Skip LINK-TRB on ISOC */
	if (((dep->free_slot & SPRD_DWC3_TRB_MASK) == SPRD_DWC3_TRB_NUM - 1) &&
			usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
		dep->free_slot++;
	}

	dtrb->bph = upper_32_bits(dma);
	dtrb->bpl = lower_32_bits(dma);
	dtrb->size = SPRD_DWC3_TRB_SIZE_LENGTH(length);

	switch (usb_endpoint_type(dep->endpoint.desc)) {
	case USB_ENDPOINT_XFER_INT:
	case USB_ENDPOINT_XFER_BULK:
		dtrb->ctrl = SPRD_DWC3_TRBCTL_NORMAL;
		break;

	case USB_ENDPOINT_XFER_ISOC:
		if (!node) {
			dtrb->ctrl = SPRD_DWC3_TRBCTL_ISOCHRONOUS_FIRST;
		} else {
			dtrb->ctrl = SPRD_DWC3_TRBCTL_ISOCHRONOUS;
		}
		break;

	case USB_ENDPOINT_XFER_CONTROL:
		dtrb->ctrl = SPRD_DWC3_TRBCTL_CONTROL_SETUP;
		break;

	default:
		BUG();
	}

	if (!dreq->request.no_interrupt && !chain) {
		dtrb->ctrl |= SPRD_DWC3_TRB_CTRL_IOC;
	}

	if (usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
		dtrb->ctrl |= SPRD_DWC3_TRB_CTRL_ISP_IMI;
		dtrb->ctrl |= SPRD_DWC3_TRB_CTRL_CSP;
	} else if (last) {
		dtrb->ctrl |= SPRD_DWC3_TRB_CTRL_LST;
	}

	if (chain) {
		dtrb->ctrl |= SPRD_DWC3_TRB_CTRL_CHN;
	}

	if (usb_endpoint_xfer_bulk(dep->endpoint.desc) && dep->stream_capable) {
		dtrb->ctrl |= SPRD_DWC3_TRB_CTRL_SID_SOFN(dreq->request.stream_id);
	}

	dtrb->ctrl |= SPRD_DWC3_TRB_CTRL_HWO;
	dwc3_flush_cache((unsigned long)dtrb, sizeof(*dtrb), 1);
}

/*
 * setup TRBs from requests
 */
static void dwc3_prepare_trbs(struct dwc3_ep *dep, bool starting)
{
	struct dwc3_request *dreq;
	struct dwc3_request *n;
	u32 trbs_left;
	u32 max_trbs;

	/* first request must not be queued */
	trbs_left = (dep->busy_slot - dep->free_slot) & SPRD_DWC3_TRB_MASK;

	/* can't wrap around on a non-isoc EP since there's no link TRB */
	if (!usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
		max_trbs = SPRD_DWC3_TRB_NUM - (dep->free_slot & SPRD_DWC3_TRB_MASK);
		if (trbs_left > max_trbs) {
			trbs_left = max_trbs;
		}
	}

	/*
	 * If busy & slot are equal than it is either full or empty.
	 * If we are starting to process requests then we are empty.
	 * Otherwise we are full and don't do anything.
	 */
	if (!trbs_left) {
		if (!starting) {
			return;
		}

		trbs_left = SPRD_DWC3_TRB_NUM;

		if (usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
			dep->free_slot = 1;
			dep->busy_slot = 1;
		} else {
			dep->free_slot = 0;
			dep->busy_slot = 0;
		}
	}

	/* Last TRB, a link TRB, is not used for transfer */
	if ((trbs_left <= 1) && usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
		return;
	}

	list_for_every_entry_safe(&dep->request_list, dreq, n, struct dwc3_request, list) {
		dwc3_prepare_one_trb(dep, dreq, dreq->request.dma,
					dreq->request.length, true, false, 0);
		break;
	}
}

/*
 * dwc3_gadget_kick_transfer_internal
 */
static int dwc3_gadget_kick_transfer_internal(struct dwc3_ep *dep, u16 cmd_param,
		int to_start_new)
{
	dwc3_cmd_params_t dparams;
	struct dwc3_request *dreq;
	struct dwc3 *dwc = dep->dwc;
	int err;
	u32 cmd;

	if (to_start_new && (dep->flags & SPRD_DWC3_EP_BUSY)) {
		dev_vdbg(dwc->dev, "%s: endpoint busy\n", dep->name);
		return -EBUSY;
	}
	dep->flags &= ~SPRD_DWC3_EP_PENDING_REQUEST;

	/*
	 * If we are getting here after a short-out-packet we don't
	 * enqueue any new requests as we try to set the IOC bit only
	 * on the last request.
	 */
	if (to_start_new) {
		if (list_is_empty(&dep->req_queued)) {
			dwc3_prepare_trbs(dep, to_start_new);
		}
		/* dreq points to the first request which will be sent */
		dreq = next_request(&dep->req_queued);
	} else {
		dwc3_prepare_trbs(dep, to_start_new);

		/*
		 * dreq points to the first request where HWO changed from 0 to 1
		 */
		dreq = next_request(&dep->req_queued);
	}
	if (!dreq) {
		dep->flags |= SPRD_DWC3_EP_PENDING_REQUEST;
		return 0;
	}

	memset(&dparams, 0, sizeof(dparams));

	if (to_start_new) {
		dparams.param0 = upper_32_bits(dreq->trb_dma);
		dparams.param1 = lower_32_bits(dreq->trb_dma);
		cmd = SPRD_DWC3_DEPCMD_STARTTRANSFER;
	} else {
		cmd = SPRD_DWC3_DEPCMD_UPDATETRANSFER;
	}

	cmd |= SPRD_DWC3_DEPCMD_PARAM(cmd_param);
	err = dwc3_send_gadget_ep_cmd(dwc, dep->number, cmd, &dparams);
	if (err < 0) {
		dev_dbg(dwc->dev, "fail to send cmd STARTTRANSFER\n");
		usb_gadget_unmap_request(&dwc->gadget, &dreq->request,
				dreq->direction);
		list_delete(&dreq->list);
		return err;
	}

	dep->flags |= SPRD_DWC3_EP_BUSY;

	if (to_start_new) {
		dep->resource_index = dwc3_gadget_ep_get_transfer_index(dwc,
				dep->number);
	}

	return 0;
}

/*
 * dwc3_gadget_start_isoc_internal
 */
static void dwc3_gadget_start_isoc_internal(struct dwc3 *dwc,
		struct dwc3_ep *dep, u32 uf)
{
	u32 tmp_uf;

	if (list_is_empty(&dep->request_list)) {
		dep->flags |= SPRD_DWC3_EP_PENDING_REQUEST;
		dev_vdbg(dwc->dev, "ISOC ep %s run out for req!\n", dep->name);
		return;
	}

	tmp_uf = uf + dep->interval * 4;
	dwc3_gadget_kick_transfer_internal(dep, tmp_uf, true);
}

/*
 * dwc3_gadget_start_isoc
 */
static void dwc3_gadget_start_isoc(struct dwc3 *dwc,
		struct dwc3_ep *dep, const struct dwc3_event_depevt *evt)
{
	u32 uf;
	u32 mask = ~(dep->interval - 1);

	uf = evt->parameters & mask;
	dwc3_gadget_start_isoc_internal(dwc, dep, uf);
}

/*
 * __dwc3_gadget_ep_queue
 */
static int __dwc3_gadget_ep_queue(struct dwc3_ep *dep, struct dwc3_request *dreq)
{
	struct dwc3 *dwc = dep->dwc;
	int err;

	dreq->epnum = dep->number;
	dreq->direction = dep->direction;
	dreq->request.status = -EINPROGRESS;
	dreq->request.actual = 0;

	/* Avoid hangs on OUT requests smaller than maxpacket size */
	if (0 == dep->direction &&
	    dreq->request.length < dep->endpoint.maxpacket) {
		dreq->request.length = dep->endpoint.maxpacket;
	}

	err = usb_gadget_map_request(&dwc->gadget, &dreq->request,
			dep->direction);
	if (err) {
		return err;
	}

	list_add_tail(&dep->request_list, &dreq->list);

	/* 1. XferNotReady with empty list of requests. */
	if (dep->flags & SPRD_DWC3_EP_PENDING_REQUEST) {
		if (usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
			if (list_is_empty(&dep->req_queued)) {
				dwc3_stop_active_transfer(dwc, dep->number, true);
				dep->flags = SPRD_DWC3_EP_ENABLED;
			}
			return 0;
		}

		err = dwc3_gadget_kick_transfer_internal(dep, 0, true);
		if (err && err != -EBUSY) {
			dev_dbg(dwc->dev, "%s: fail to kick transfers\n",
					dep->name);
		}
		return err;
	}

	/* 2. XferInProgress on Isoc EP with an active transfer. */
	if (usb_endpoint_xfer_isoc(dep->endpoint.desc) &&
			(dep->flags & SPRD_DWC3_EP_BUSY) &&
			!(dep->flags & SPRD_DWC3_EP_MISSED_ISOC)) {
		WARN_ON_ONCE(!dep->resource_index);
		err = dwc3_gadget_kick_transfer_internal(dep, dep->resource_index,
				false);
		if (err && err != -EBUSY) {
			dev_dbg(dwc->dev, "%s: fail to kick transfers\n",
					dep->name);
		}
		return err;
	}

	/* 4. Stream Capable Bulk Endpoints. */
	if (dep->stream_capable) {
		err = dwc3_gadget_kick_transfer_internal(dep, 0, true);
		if (err && err != -EBUSY) {
			dev_dbg(dwc->dev, "%s: fail to kick transfers\n",
					dep->name);
		}
	}

	return 0;
}

/*
 * dwc3_gadget_ep_queue
 */
static int dwc3_gadget_ep_queue(struct usb_ep *ep, struct usb_request *req,
	gfp_t gfp_flags)
{
	struct dwc3_request *dreq = to_dwc3_request(req);
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	unsigned long flag;
	int err;

	spin_lock_irqsave(&dwc->lock, flag);
	if (!dep->endpoint.desc) {
		dev_dbg(dwc->dev, "trying to queue req %p to disabled %s\n",
				req, ep->name);
		err = -ESHUTDOWN;
		goto out;
	}

	if (dreq->dep != dep) {
		WARN(true, "req %p belongs to '%s'\n", req, dreq->dep->name);
		err = -EINVAL;
		goto out;
	}

	dev_vdbg(dwc->dev, "queing req %p to %s length %d\n",
			req, ep->name, req->length);

	err = __dwc3_gadget_ep_queue(dep, dreq);

out:
	spin_unlock_irqrestore(&dwc->lock, flag);

	return err;
}

/*
 * dwc3_gadget_ep_dequeue
 */
static int dwc3_gadget_ep_dequeue(struct usb_ep *ep,
		struct usb_request *request)
{
	struct dwc3_request *dreq = to_dwc3_request(request);
	struct dwc3_request *tmp = NULL;
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3 *dwc = dep->dwc;
	unsigned long flag;
	int err = 0;

	spin_lock_irqsave(&dwc->lock, flag);

	list_for_each_entry(tmp, &dep->request_list, list) {
		if (tmp == dreq) {
			break;
		}
	}

	if (tmp != dreq) {
		list_for_each_entry(tmp, &dep->req_queued, list) {
			if (tmp == dreq) {
				break;
			}
		}
		if (tmp == dreq) {
			/* until it is processed */
			dwc3_stop_active_transfer(dwc, dep->number, true);
			goto out1;
		}
		dev_err(dwc->dev, "request %p was not queued to %s\n",
				request, ep->name);
		err = -EINVAL;
		goto out0;
	}

out1:
	dwc3_gadget_giveback(dep, dreq, -ECONNRESET);
out0:
	spin_unlock_irqrestore(&dwc->lock, flag);

	return err;
}

/*
 * dwc3_gadget_ep_set_halt_internal
 */
int dwc3_gadget_ep_set_halt_internal(struct dwc3_ep *dep, int val, int protocol)
{
	dwc3_cmd_params_t dparams;
	struct dwc3 *dwc = dep->dwc;
	int err;

	if (usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
		dev_err(dwc->dev, "%s is of isoc type\n", dep->name);
		return -EINVAL;
	}

	memset(&dparams, 0x00, sizeof(dparams));

	if (val) {
		if (!protocol && ((dep->direction && dep->flags & SPRD_DWC3_EP_BUSY) ||
				(!list_is_empty(&dep->req_queued) ||
				 !list_is_empty(&dep->request_list)))) {
			dev_dbg(dwc->dev, "%s: pending request, cannot halt\n",
					dep->name);
			return -EAGAIN;
		}

		err = dwc3_send_gadget_ep_cmd(dwc, dep->number,
					SPRD_DWC3_DEPCMD_SETSTALL, &dparams);
		if (err) {
			dev_err(dwc->dev, "fail to set STALL on %s\n",
					dep->name);
		} else {
			dep->flags |= SPRD_DWC3_EP_STALL;
		}
	} else {
		err = dwc3_send_gadget_ep_cmd(dwc, dep->number,
					SPRD_DWC3_DEPCMD_CLEARSTALL, &dparams);
		if (err) {
			dev_err(dwc->dev, "fail to clear STALL on %s\n",
					dep->name);
		} else {
			dep->flags &= ~(SPRD_DWC3_EP_STALL | SPRD_DWC3_EP_WEDGE);
		}
	}

	return err;
}

/*
 * dwc3_gadget_ep_set_halt
 */
static int dwc3_gadget_ep_set_halt(struct usb_ep *ep, int val)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3    *dwc = dep->dwc;
	unsigned long flag;
	int ret;

	spin_lock_irqsave(&dwc->lock, flag);
	ret = dwc3_gadget_ep_set_halt_internal(dep, val, false);
	spin_unlock_irqrestore(&dwc->lock, flag);

	return ret;
}

/*
 * dwc3_gadget_ep_set_wedge
 */
static int dwc3_gadget_ep_set_wedge(struct usb_ep *ep)
{
	struct dwc3_ep *dep = to_dwc3_ep(ep);
	struct dwc3    *dwc = dep->dwc;
	unsigned long flag;
	int ret;

	spin_lock_irqsave(&dwc->lock, flag);
	dep->flags |= SPRD_DWC3_EP_WEDGE;

	if (dep->number == 0 || dep->number == 1) {
		ret = __dwc3_gadget_ep0_set_halt(ep, 1);
	} else {
		ret = dwc3_gadget_ep_set_halt_internal(dep, 1, false);
	}
	spin_unlock_irqrestore(&dwc->lock, flag);

	return ret;
}

/*
 * dwc3_gadget_get_frame
 */
static int dwc3_gadget_get_frame(struct usb_gadget *gadget)
{
	struct dwc3 *dwc = gadget_to_dwc(gadget);
	u32 val = dwc3_readl(dwc->regs, SPRD_DWC3_DSTS);
	return SPRD_DWC3_DSTS_SOFFN(val);
}

/*
 * dwc3_gadget_wakeup
 */
static int dwc3_gadget_wakeup(struct usb_gadget *g)
{
	struct dwc3   *dwc = gadget_to_dwc(g);
	unsigned long flag;
	unsigned long timeout;
	int ret = 0;
	u32 val;
	u8 link_state;
	u8 speed;

	spin_lock_irqsave(&dwc->lock, flag);

	val = dwc3_readl(dwc->regs, SPRD_DWC3_DSTS);

	speed = val & SPRD_DWC3_DSTS_CONNECTSPD;
	if (speed == SPRD_DWC3_DSTS_SUPERSPEED) {
		ret = -EINVAL;
		dev_dbg(dwc->dev, "no wakeup on SuperSpeed\n");
		goto out;
	}

	link_state = SPRD_DWC3_DSTS_USBLNKST(val);

	switch (link_state) {
	case SPRD_DWC3_LINK_STATE_U3:	  /* means SUSPEND in HS */
	case SPRD_DWC3_LINK_STATE_RX_DET: /* Early Suspend in HS */
		break;
	default:
		ret = -EINVAL;
		dev_dbg(dwc->dev, "can't wakeup from link-state %d\n",
				link_state);
		goto out;
	}

	ret = dwc3_gadget_set_link_state(dwc, SPRD_DWC3_LINK_STATE_RECOV);
	if (ret < 0) {
		dev_err(dwc->dev, "fail to put link in Recovery\n");
		goto out;
	}

	if (dwc->revision < SPRD_DWC3_REVISION_194A) {
		val = dwc3_readl(dwc->regs, SPRD_DWC3_DCTL);
		val &= ~SPRD_DWC3_DCTL_ULSTCHNGREQ_MASK;
		dwc3_writel(dwc->regs, SPRD_DWC3_DCTL, val);
	}

	/* poll until Link State ON */
	timeout = 1000;

	while (timeout--) {
		val = dwc3_readl(dwc->regs, SPRD_DWC3_DSTS);

		/* means ON in HS*/
		if (SPRD_DWC3_DSTS_USBLNKST(val) == SPRD_DWC3_LINK_STATE_U0) {
			break;
		}
	}

	if (SPRD_DWC3_DSTS_USBLNKST(val) != SPRD_DWC3_LINK_STATE_U0) {
		ret = -EINVAL;
		dev_err(dwc->dev, "fail to send remote wakeup\n");
	}

out:
	spin_unlock_irqrestore(&dwc->lock, flag);

	return ret;
}

/*
 * dwc3_gadget_set_selfpowered
 */
static int dwc3_gadget_set_selfpowered(struct usb_gadget *gadget,
		int selfpowered)
{
	unsigned long flag;
	struct dwc3 *dwc = gadget_to_dwc(gadget);

	spin_lock_irqsave(&dwc->lock, flag);
	dwc->is_selfpowered = !!selfpowered;
	spin_unlock_irqrestore(&dwc->lock, flag);

	return 0;
}

/*
 * dwc3_gadget_run_stop
 */
static int dwc3_gadget_run_stop(struct dwc3 *dwc, int is_on, int suspend)
{
	u32 timeout = 500;
	u32 val = 0;

	val = dwc3_readl(dwc->regs, SPRD_DWC3_DCTL);
	if (is_on) {
		if (dwc->revision <= SPRD_DWC3_REVISION_187A) {
			val &= ~SPRD_DWC3_DCTL_TRGTULST_MASK;
			val |= SPRD_DWC3_DCTL_TRGTULST_RX_DET;
		}

		if (dwc->revision >= SPRD_DWC3_REVISION_194A) {
			val &= ~SPRD_DWC3_DCTL_KEEP_CONNECT;
		}
		val |= SPRD_DWC3_DCTL_RUN_STOP;

		if (dwc->has_hibernation) {
			val |= SPRD_DWC3_DCTL_KEEP_CONNECT;
		}
		dwc->pullups_connected = true;
	} else {
		val &= ~SPRD_DWC3_DCTL_RUN_STOP;

		if (dwc->has_hibernation && !suspend) {
			val &= ~SPRD_DWC3_DCTL_KEEP_CONNECT;
		}
		dwc->pullups_connected = false;
	}

	dwc3_writel(dwc->regs, SPRD_DWC3_DCTL, val);

	do {
		val = dwc3_readl(dwc->regs, SPRD_DWC3_DSTS);
		if (is_on) {
			if (!(val & SPRD_DWC3_DSTS_DEVCTRLHLT)) {
				break;
			}
		} else {
			if (val & SPRD_DWC3_DSTS_DEVCTRLHLT) {
				break;
			}
		}
		timeout--;
		if (!timeout) {
			return -ETIMEDOUT;
		}
		udelay(1);
	} while (1);

	dev_vdbg(dwc->dev, "gadget %s data soft-%s\n",
		dwc->gadget_driver ? dwc->gadget_driver->function : "no-function",
		is_on ? "connect" : "disconnect");
	return 0;
}

/*
 * dwc3_gadget_pullup
 */
static int dwc3_gadget_pullup(struct usb_gadget *gadget, int is_on)
{
	struct dwc3 *dwc = gadget_to_dwc(gadget);
	unsigned long flag;
	int on = !!is_on;
	int ret;

	spin_lock_irqsave(&dwc->lock, flag);
	ret = dwc3_gadget_run_stop(dwc, on, false);
	spin_unlock_irqrestore(&dwc->lock, flag);

	return ret;
}

static void dwc3_gadget_enable_irq(struct dwc3 *dwc)
{
	u32 val;

	/* enable all but Start and End of Frame IRQs */
	val = (/*SPRD_DWC3_DEVTEN_VNDRDEVTSTRCVEDEN |
			SPRD_DWC3_DEVTEN_EVNTOVERFLOWEN |
			SPRD_DWC3_DEVTEN_CMDCMPLTEN |*/
			SPRD_DWC3_DEVTEN_ERRTICERREN |
			/*SPRD_DWC3_DEVTEN_WKUPEVTEN |
			SPRD_DWC3_DEVTEN_ULSTCNGEN |*/
			SPRD_DWC3_DEVTEN_CONNECTDONEEN |
			SPRD_DWC3_DEVTEN_USBRSTEN /*|
			SPRD_DWC3_DEVTEN_DISCONNEVTEN */);

	dwc3_writel(dwc->regs, SPRD_DWC3_DEVTEN, val);
}

/* mask all interrupts */
static void dwc3_gadget_disable_irq(struct dwc3 *dwc)
{
	dwc3_writel(dwc->regs, SPRD_DWC3_DEVTEN, 0x00);
}

static struct usb_endpoint_descriptor dwc3_gadget_ep0_desc = {
	.bmAttributes    = USB_ENDPOINT_XFER_CONTROL,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bLength         = USB_DT_ENDPOINT_SIZE,
};

static const struct usb_ep_ops dwc3_gadget_ep0_ops = {
	.disable	= dwc3_gadget_ep0_disable,
	.enable		= dwc3_gadget_ep0_enable,
	.free_request	= dwc3_gadget_ep_free_request,
	.alloc_request	= dwc3_gadget_ep_alloc_request,
	.dequeue	= dwc3_gadget_ep_dequeue,
	.queue		= dwc3_gadget_ep0_queue,
	.set_wedge	= dwc3_gadget_ep_set_wedge,
	.set_halt	= dwc3_gadget_ep0_set_halt,
};

static const struct usb_ep_ops dwc3_gadget_ep_ops = {
	.disable	= dwc3_gadget_ep_disable,
	.enable		= dwc3_gadget_ep_enable,
	.free_request	= dwc3_gadget_ep_free_request,
	.alloc_request	= dwc3_gadget_ep_alloc_request,
	.dequeue	= dwc3_gadget_ep_dequeue,
	.queue		= dwc3_gadget_ep_queue,
	.set_wedge	= dwc3_gadget_ep_set_wedge,
	.set_halt	= dwc3_gadget_ep_set_halt,
};

/*
 * dwc3_gadget_start
 */
static int dwc3_gadget_start(struct usb_gadget *g,
		struct usb_gadget_driver *driver)
{
	struct dwc3 *dwc = gadget_to_dwc(g);
	struct dwc3_ep *dep;
	unsigned long flag;
	int ret = 0;
	u32 val;

	spin_lock_irqsave(&dwc->lock, flag);

	if (dwc->gadget_driver) {
		dev_err(dwc->dev, "%s is already bound to %s\n",
				dwc->gadget.name,
				dwc->gadget_driver->function);
		ret = -EBUSY;
		goto out1;
	}

	dwc->gadget_driver = driver;

	val = dwc3_readl(dwc->regs, SPRD_DWC3_DCFG);
	val &= ~(SPRD_DWC3_DCFG_SPEED_MASK);

	/* WORKAROUND for revision < 2.20a */
	if (dwc->revision < SPRD_DWC3_REVISION_220A) {
		val |= SPRD_DWC3_DCFG_SUPERSPEED;
	} else {
		switch (dwc->maximum_speed) {
		case USB_SPEED_HIGH:
			val |= SPRD_DWC3_DSTS_HIGHSPEED;
			break;

		case USB_SPEED_FULL:
			val |= SPRD_DWC3_DSTS_FULLSPEED2;
			break;

		case USB_SPEED_LOW:
			val |= SPRD_DWC3_DSTS_LOWSPEED;
			break;

		case USB_SPEED_UNKNOWN:	/* FALLTHROUGH */
		case USB_SPEED_SUPER:	/* FALLTHROUGH */
		default:
			val |= SPRD_DWC3_DSTS_SUPERSPEED;
		}
	}

	dwc3_writel(dwc->regs, SPRD_DWC3_DCFG, val);
	dwc->start_config_issued = false;

	/* Start with SuperSpeed Default */
	dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(512);

	dep = dwc->eps[0];
	ret = dwc3_gadget_dep_enable(dep, &dwc3_gadget_ep0_desc, NULL, false,
			false);
	if (ret) {
		dev_err(dwc->dev, "fail to enable %s\n", dep->name);
		goto out2;
	}

	dep = dwc->eps[1];
	ret = dwc3_gadget_dep_enable(dep, &dwc3_gadget_ep0_desc, NULL, false,
			false);
	if (ret) {
		dev_err(dwc->dev, "fail to enable %s\n", dep->name);
		goto out3;
	}

	/* begin to receive SETUP packets */
	dwc->ep0state = SPRD_DWC3_EP0_STATE_SETUP_PHASE;
	dwc3_ep0_out_start(dwc);

	dwc3_gadget_enable_irq(dwc);

	spin_unlock_irqrestore(&dwc->lock, flag);

	return 0;

out3:
	dwc3_gadget_dep_disable(dwc->eps[0]);

out2:
	dwc->gadget_driver = NULL;

out1:
	spin_unlock_irqrestore(&dwc->lock, flag);

	return ret;
}

/*
 * dwc3_gadget_stop
 */
static int dwc3_gadget_stop(struct usb_gadget *g)
{
	struct dwc3 *dwc = gadget_to_dwc(g);
	unsigned long flag;

	spin_lock_irqsave(&dwc->lock, flag);

	dwc3_gadget_disable_irq(dwc);
	dwc3_gadget_dep_disable(dwc->eps[0]);
	dwc3_gadget_dep_disable(dwc->eps[1]);

	dwc->gadget_driver = NULL;

	spin_unlock_irqrestore(&dwc->lock, flag);

	return 0;
}

/*
 * dwc3_gadget_init_hw_endpoints
 */
static int dwc3_gadget_init_hw_endpoints(struct dwc3 *dwc,
		u8 num, u32 direction)
{
	struct dwc3_ep *dep;
	u8 i;

	for (i = 0; i < num; i++) {
		u8 epnum = (i << 1) | (!!direction);

		dep = kzalloc(sizeof(*dep), GFP_KERNEL);
		if (!dep) {
			return -ENOMEM;
		}

		dep->direction = !!direction;
		dep->number = epnum;
		dwc->eps[epnum] = dep;
		dep->dwc = dwc;

		snprintf(dep->name, sizeof(dep->name), "ep%d%s", epnum >> 1,
				(epnum & 1) ? "in" : "out");

		dep->endpoint.name = dep->name;
		if (0 == epnum || 1 == epnum) {
			usb_ep_set_maxpacket_limit(&dep->endpoint, 512);
			dep->endpoint.ops = &dwc3_gadget_ep0_ops;
			dep->endpoint.maxburst = 1;
			if (0 == epnum) {
				dwc->gadget.ep0 = &dep->endpoint;
			}
		} else {
			int err;

			usb_ep_set_maxpacket_limit(&dep->endpoint, 512);
			dep->endpoint.ops = &dwc3_gadget_ep_ops;
			dep->endpoint.max_streams = 15;
			list_add_tail(&dwc->gadget.ep_list, &dep->endpoint.ep_list);

			err = dwc3_alloc_trb_pool(dep);
			if (err) {
				return err;
			}
		}

		INIT_LIST_HEAD(&dep->request_list);
		INIT_LIST_HEAD(&dep->req_queued);
	}

	return 0;
}

/*
 * dwc3_gadget_init_endpoints
 */
static int dwc3_gadget_init_endpoints(struct dwc3 *dwc)
{
	int err;

	INIT_LIST_HEAD(&dwc->gadget.ep_list);

	err = dwc3_gadget_init_hw_endpoints(dwc, dwc->num_out_eps, 0);
	if (err < 0) {
		dev_vdbg(dwc->dev, "fail to allocate OUT endpoints\n");
		return err;
	}

	err = dwc3_gadget_init_hw_endpoints(dwc, dwc->num_in_eps, 1);
	if (err < 0) {
		dev_vdbg(dwc->dev, "fail to allocate IN endpoints\n");
		return err;
	}

	return 0;
}

/*
 * dwc3_gadget_free_endpoints
 */
static void dwc3_gadget_free_endpoints(struct dwc3 *dwc)
{
	struct dwc3_ep *dep;
	u8 num;

	for (num = 0; num < SPRD_DWC3_ENDPOINTS_NUM; num++) {
		dep = dwc->eps[num];
		if (!dep) {
			continue;
		}

		if (0 != num && 1 != num) {
			dwc3_free_trb_pool(dep);
			list_delete(&dep->endpoint.ep_list);
		}
		free(dep);
	}
}

/*
 * __dwc3_cleanup_done_trbs
 */
static int __dwc3_cleanup_done_trbs(struct dwc3 *dwc,
		struct dwc3_ep *dep,
		struct dwc3_request *dreq,
		struct dwc3_trb *trb,
		const struct dwc3_event_depevt *evt,
		int status)
{
	unsigned int trb_status, count, short_pkt=0;

	if ((trb->ctrl & SPRD_DWC3_TRB_CTRL_HWO) &&
		-ESHUTDOWN != status) {
		dev_err(dwc->dev, "%s's TRB (%p) still owned by HW !!\n",
				dep->name, trb);
	}

	count = trb->size & SPRD_DWC3_TRB_SIZE_MASK;

	/* Direction */
	if (dep->direction) {
		if (count) {
			trb_status = SPRD_DWC3_TRB_SIZE_TRBSTS(trb->size);
			if (trb_status == SPRD_DWC3_TRBSTS_MISSED_ISOC) {
				dep->flags |= SPRD_DWC3_EP_MISSED_ISOC;
				dev_dbg(dwc->dev, "incomplete IN transfer %s\n",
						dep->name);
			} else {
				status = -ECONNRESET;
				dev_err(dwc->dev, "incomplete IN transfer %s\n",
						dep->name);
			}
		} else {
			dep->flags &= ~SPRD_DWC3_EP_MISSED_ISOC;
		}
	} else {
		if (count && (evt->status & DEPEVT_STATUS_SHORT)) {
			short_pkt = 1;
		}
	}

	dreq->request.actual += dreq->request.length - count;
	if (short_pkt) {
		return 1;
	}
	if ((DEPEVT_STATUS_LST & evt->status) &&
		(trb->ctrl & (SPRD_DWC3_TRB_CTRL_LST | SPRD_DWC3_TRB_CTRL_HWO))) {
		return 1;
	}
	if ((DEPEVT_STATUS_IOC & evt->status) &&
		(trb->ctrl & SPRD_DWC3_TRB_CTRL_IOC)) {
		return 1;
	}
	return 0;
}

/*
 * dwc3_cleanup_done_reqs
 */
static int dwc3_cleanup_done_reqs(struct dwc3 *dwc, struct dwc3_ep *dep,
		const struct dwc3_event_depevt *event, int status)
{
	struct dwc3_request *dreq;
	struct dwc3_trb *trb;
	unsigned int slot;

	dreq = next_request(&dep->req_queued);
	if (!dreq) {
		WARN_ON_ONCE(1);
		return 1;
	}

	slot = dreq->start_slot;
	if ((slot == SPRD_DWC3_TRB_NUM - 1) &&
	    usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
		slot++;
	}
	slot %= SPRD_DWC3_TRB_NUM;
	trb = &dep->trb_pool[slot];

	dwc3_flush_cache((unsigned long)trb, sizeof(*trb), 0);
	__dwc3_cleanup_done_trbs(dwc, dep, dreq, trb, event, status);
	dwc3_gadget_giveback(dep, dreq, status);

	if (usb_endpoint_xfer_isoc(dep->endpoint.desc) &&
			list_is_empty(&dep->req_queued)) {
		if (list_is_empty(&dep->request_list)) {
			dep->flags = SPRD_DWC3_EP_PENDING_REQUEST;
		} else {
			dwc3_stop_active_transfer(dwc, dep->number, true);
			dep->flags = SPRD_DWC3_EP_ENABLED;
		}
		return 1;
	}

	return 1;
}
/*
 * dwc3_endpoint_transfer_complete
 */
static void dwc3_endpoint_transfer_complete(struct dwc3 *dwc,
		struct dwc3_ep *dep, const struct dwc3_event_depevt *event)
{
	unsigned stat = 0;
	int clear_busy;

	if (DEPEVT_STATUS_BUSERR & event->status) {
		stat = -ECONNRESET;
	}

	clear_busy = dwc3_cleanup_done_reqs(dwc, dep, event, stat);
	if (clear_busy) {
		dep->flags &= ~SPRD_DWC3_EP_BUSY;
	}

	/* WORKAROUND: This is the 2nd half of U1/U2 -> U0 workaround */
	if (dwc->revision < SPRD_DWC3_REVISION_183A) {
		int num;
		u32 val;

		for (num = 0; num < SPRD_DWC3_ENDPOINTS_NUM; num++) {
			dep = dwc->eps[num];

			if (!(dep->flags & SPRD_DWC3_EP_ENABLED)) {
				continue;
			}
			if (!list_is_empty(&dep->req_queued)) {
				return;
			}
		}

		val = dwc3_readl(dwc->regs, SPRD_DWC3_DCTL);
		val |= dwc->u1u2;
		dwc3_writel(dwc->regs, SPRD_DWC3_DCTL, val);
		dwc->u1u2 = 0;
	}
}

/*
 * dwc3_endpoint_interrupt
 */
static void dwc3_endpoint_interrupt(struct dwc3 *dwc,
		const struct dwc3_event_depevt *evt)
{
	struct dwc3_ep *dep;
	u8 num = evt->endpoint_number;

	dep = dwc->eps[num];
	if (!(dep->flags & SPRD_DWC3_EP_ENABLED)) {
		return;
	}

	/* EP0 or EP1 */
	if (0 == num || 1 == num) {
		dwc3_ep0_interrupt(dwc, evt);
		return;
	}

	switch (evt->endpoint_event) {
	/* DEPEVT_XFERINPROGRESS */
	case SPRD_DWC3_DEPEVT_XFERINPROGRESS:
		dwc3_endpoint_transfer_complete(dwc, dep, evt);
		break;

	/* DEPEVT_XFERCOMPLETE */
	case SPRD_DWC3_DEPEVT_XFERCOMPLETE:
		dep->resource_index = 0;
		if (usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
			dev_dbg(dwc->dev, "%s is an isoc endpoint\n", dep->name);
			return;
		}
		dwc3_endpoint_transfer_complete(dwc, dep, evt);
		break;

	/* DEPEVT_EPCMDCMPLT */
	case SPRD_DWC3_DEPEVT_EPCMDCMPLT:
		dev_vdbg(dwc->dev, "EP Command Complete\n");
		break;

	/* DEPEVT_RXTXFIFOEVT */
	case SPRD_DWC3_DEPEVT_RXTXFIFOEVT:
		dev_dbg(dwc->dev, "%s FIFO Overrun\n", dep->name);
		break;

	/* DEPEVT_XFERNOTREADY */
	case SPRD_DWC3_DEPEVT_XFERNOTREADY:
		if (usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
			dwc3_gadget_start_isoc(dwc, dep, evt);
		} else {
			int err;
			dev_vdbg(dwc->dev, "%s: reason Transfer %s\n", dep->name,
					evt->status & DEPEVT_STATUS_TRANSFER_ACTIVE
					? "Active" : "Not Active");

			err = dwc3_gadget_kick_transfer_internal(dep, 0, 1);
			if (!err || -EBUSY == err) {
				return;
			}
			dev_dbg(dwc->dev, "%s: fail to kick transfers\n", dep->name);
		}
		break;

	/* DEPEVT_STREAMEVT */
	case SPRD_DWC3_DEPEVT_STREAMEVT:
		if (!usb_endpoint_xfer_bulk(dep->endpoint.desc)) {
			dev_err(dwc->dev, "stream event for non-Bulk %s\n",
					dep->name);
			return;
		}

		if (evt->status == DEPEVT_STREAMEVT_FOUND) {
			dev_vdbg(dwc->dev, "stream %d found and started\n",
					evt->parameters);
		} else {
			dev_dbg(dwc->dev, "couldn't find suitable stream\n");
		}
		break;
	}
}

/*
 * dwc3_disconnect_gadget
 */
static void dwc3_disconnect_gadget(struct dwc3 *dwc)
{
	if (!dwc->gadget_driver || !dwc->gadget_driver->disconnect) {
		return;
	}
	spin_unlock(&dwc->lock);
	dwc->gadget_driver->disconnect(&dwc->gadget);
	spin_lock(&dwc->lock);
}

/*
 * dwc3_suspend_gadget
 */
static void dwc3_suspend_gadget(struct dwc3 *dwc)
{
	if (!dwc->gadget_driver || !dwc->gadget_driver->suspend) {
		return;
	}
	spin_unlock(&dwc->lock);
	dwc->gadget_driver->suspend(&dwc->gadget);
	spin_lock(&dwc->lock);
}

/*
 * dwc3_resume_gadget
 */
static void dwc3_resume_gadget(struct dwc3 *dwc)
{
	if (!dwc->gadget_driver || !dwc->gadget_driver->resume) {
		return;
	}
	spin_unlock(&dwc->lock);
	dwc->gadget_driver->resume(&dwc->gadget);
	spin_lock(&dwc->lock);
}

/*
 * dwc3_reset_gadget
 */
static void dwc3_reset_gadget(struct dwc3 *dwc)
{
	if (!dwc->gadget_driver || !dwc->gadget_driver->reset) {
		return;
	}
	if (USB_SPEED_UNKNOWN != dwc->gadget.speed) {
		spin_unlock(&dwc->lock);
		usb_gadget_udc_reset(&dwc->gadget, dwc->gadget_driver);
		spin_lock(&dwc->lock);
	}
}

/*
 * dwc3_stop_active_transfers
 */
static void dwc3_stop_active_transfers(struct dwc3 *dwc)
{
	struct dwc3_ep *dep;
	u32 epnum;

	for (epnum = 2; epnum < SPRD_DWC3_ENDPOINTS_NUM; epnum++) {
		dep = dwc->eps[epnum];
		if (!dep || !(dep->flags & SPRD_DWC3_EP_ENABLED)) {
			continue;
		}
		dwc3_remove_requests(dwc, dep);
	}
}

/*
 * dwc3_clear_stall_all_ep
 */
static void dwc3_clear_stall_all_ep(struct dwc3 *dwc)
{
	dwc3_cmd_params_t dparams;
	struct dwc3_ep *dep;
	u32 epnum;
	int ret;

	for (epnum = 1; epnum < SPRD_DWC3_ENDPOINTS_NUM; epnum++) {
		dep = dwc->eps[epnum];
		if (!dep || !(dep->flags & SPRD_DWC3_EP_STALL)) {
			continue;
		}
		dep->flags &= ~SPRD_DWC3_EP_STALL;

		memset(&dparams, 0, sizeof(dparams));
		ret = dwc3_send_gadget_ep_cmd(dwc, dep->number,
				SPRD_DWC3_DEPCMD_CLEARSTALL, &dparams);
		WARN_ON_ONCE(ret);
	}
}

/*
 * dwc3_gadget_disconnect_interrupt
 */
static void dwc3_gadget_disconnect_interrupt(struct dwc3 *dwc)
{
	int val;

	val = dwc3_readl(dwc->regs, SPRD_DWC3_DCTL);
	val &= ~SPRD_DWC3_DCTL_INITU1ENA;
	dwc3_writel(dwc->regs, SPRD_DWC3_DCTL, val);

	val &= ~SPRD_DWC3_DCTL_INITU2ENA;
	dwc3_writel(dwc->regs, SPRD_DWC3_DCTL, val);

	dwc3_disconnect_gadget(dwc);
	dwc->start_config_issued = false;

	dwc->setup_packet_pending = false;
	dwc->gadget.speed = USB_SPEED_UNKNOWN;
	usb_gadget_set_state(&dwc->gadget, USB_STATE_NOTATTACHED);
}

/*
 * dwc3_gadget_reset_interrupt
 */
static void dwc3_gadget_reset_interrupt(struct dwc3 *dwc)
{
	u32 val;

	/*
	 * WORKAROUND: Revisions <1.88a have an issue which would cause
	 * a missing Disconnect Event if there's a pending Setup Packet
	 * in the FIFO.
	 */
	if (dwc->revision < SPRD_DWC3_REVISION_188A) {
		if (dwc->setup_packet_pending) {
			dwc3_gadget_disconnect_interrupt(dwc);
		}
	}

	if (dwc->gadget.state >= USB_STATE_ADDRESS) {
		dwc3_reset_gadget(dwc);
	}

	/* set to DEFAULT */
	usb_gadget_set_state(&dwc->gadget, USB_STATE_DEFAULT);

	val = dwc3_readl(dwc->regs, SPRD_DWC3_DCTL);
	val &= ~SPRD_DWC3_DCTL_TSTCTRL_MASK;
	dwc3_writel(dwc->regs, SPRD_DWC3_DCTL, val);
	dwc->test_mode = false;

	dwc3_stop_active_transfers(dwc);
	dwc3_clear_stall_all_ep(dwc);
	dwc->start_config_issued = false;

	/* reset device address to 0 */
	val = dwc3_readl(dwc->regs, SPRD_DWC3_DCFG);
	val &= ~(SPRD_DWC3_DCFG_DEVADDR_MASK);
	dwc3_writel(dwc->regs, SPRD_DWC3_DCFG, val);
}

/*
 * dwc3_update_ram_clk_sel
 */
static void dwc3_update_ram_clk_sel(struct dwc3 *dwc, u32 speed)
{
	u32 val;
	u32 usb30_clk = SPRD_DWC3_GCTL_CLK_BUS;

	/*
	 * We change the clock only at SS
	 * But I dunno why I would want to do this. Maybe it becomes part of
	 * the power saving plan.
	 */
	if (speed != SPRD_DWC3_DSTS_SUPERSPEED) {
		return;
	}
	/*
	 * RAMClkSel is reset to 0 after USB reset
	 * so it must be reprogrammed each time on Connect Done.
	 */
	if (!usb30_clk) {
		return;
	} else {
		val = dwc3_readl(dwc->regs, SPRD_DWC3_GCTL);
		val |= SPRD_DWC3_GCTL_RAMCLKSEL(usb30_clk);
		dwc3_writel(dwc->regs, SPRD_DWC3_GCTL, val);
	}
}

/*
 * dwc3_gadget_conndone_interrupt
 */
static void dwc3_gadget_conndone_interrupt(struct dwc3 *dwc)
{
	struct dwc3_ep *dep;
	u32 val;
	int ret;
	u8 speed;

	val = dwc3_readl(dwc->regs, SPRD_DWC3_DSTS);
	speed = val & SPRD_DWC3_DSTS_CONNECTSPD;
	dwc->speed = speed;

	dwc3_update_ram_clk_sel(dwc, speed);

	switch (speed) {
	case SPRD_DWC3_DCFG_SUPERSPEED:
		/*
		 * WORKAROUND: Revisions <1.90a have an issue which would cause
		 * a missing USB3 Reset event.
		 *
		 * In such situations, we should force a USB3 Reset event by
		 * calling our dwc3_gadget_reset_interrupt() routine.
		 */
		if (dwc->revision < SPRD_DWC3_REVISION_190A) {
			dwc3_gadget_reset_interrupt(dwc);
		}

		dwc->gadget.speed = USB_SPEED_SUPER;
		dwc->gadget.ep0->maxpacket = 512;
		dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(512);
		break;
	case SPRD_DWC3_DCFG_HIGHSPEED:
		dwc->gadget.speed = USB_SPEED_HIGH;
		dwc->gadget.ep0->maxpacket = 64;
		dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(64);
		break;
	case SPRD_DWC3_DCFG_FULLSPEED2:
	case SPRD_DWC3_DCFG_FULLSPEED1:
		dwc->gadget.speed = USB_SPEED_FULL;
		dwc->gadget.ep0->maxpacket = 64;
		dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(64);
		break;
	case SPRD_DWC3_DCFG_LOWSPEED:
		dwc->gadget.speed = USB_SPEED_LOW;
		dwc->gadget.ep0->maxpacket = 8;
		dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(8);
		break;
	}

	/* enable USB2 LPM Capability */
	if ((dwc->revision > SPRD_DWC3_REVISION_194A)
			&& (speed != SPRD_DWC3_DCFG_SUPERSPEED)) {
		val = dwc3_readl(dwc->regs, SPRD_DWC3_DCFG);
		val |= SPRD_DWC3_DCFG_LPM_CAP;
		dwc3_writel(dwc->regs, SPRD_DWC3_DCFG, val);

		val = dwc3_readl(dwc->regs, SPRD_DWC3_DCTL);
		val &= ~(SPRD_DWC3_DCTL_HIRD_THRES_MASK | SPRD_DWC3_DCTL_L1_HIBER_EN);

		val |= SPRD_DWC3_DCTL_HIRD_THRES(dwc->hird_threshold);

		/*
		 * Revisions >= 2.40a, LPM Erratum is enabled and DCFG.LPMCap is set,
		 * core responses with an ACK and the BESL value in the LPM token is
		 * less than or equal to LPM NYET threshold.
		 */
		if (dwc->revision < SPRD_DWC3_REVISION_240A && dwc->has_lpm_erratum) {
			WARN(true, "LPM Erratum not available on dwc3 < 2.40a\n");
		}
		if (dwc->has_lpm_erratum && dwc->revision >= SPRD_DWC3_REVISION_240A) {
			val |= SPRD_DWC3_DCTL_LPM_ERRATA(dwc->lpm_nyet_threshold);
		}
		dwc3_writel(dwc->regs, SPRD_DWC3_DCTL, val);
	} else {
		val = dwc3_readl(dwc->regs, SPRD_DWC3_DCTL);
		val &= ~SPRD_DWC3_DCTL_HIRD_THRES_MASK;
		dwc3_writel(dwc->regs, SPRD_DWC3_DCTL, val);
	}

	dep = dwc->eps[0];
	ret = dwc3_gadget_dep_enable(dep, &dwc3_gadget_ep0_desc, NULL, true, false);
	if (ret) {
		dev_err(dwc->dev, "fail to enable %s\n", dep->name);
		return;
	}

	dep = dwc->eps[1];
	ret = dwc3_gadget_dep_enable(dep, &dwc3_gadget_ep0_desc, NULL, true, false);
	if (ret) {
		dev_err(dwc->dev, "fail to enable %s\n", dep->name);
		return;
	}

}

/*
 * dwc3_gadget_wakeup_interrupt
 */
static void dwc3_gadget_wakeup_interrupt(struct dwc3 *dwc)
{
	/* TODO
	 * take core out of low power mode when that's implemented.
	 */
	dwc->gadget_driver->resume(&dwc->gadget);
}

/*
 * dwc3_gadget_linksts_change_interrupt
 */
static void dwc3_gadget_linksts_change_interrupt(struct dwc3 *dwc,
		unsigned int evtinfo)
{
	enum dwc3_link_state next = evtinfo & SPRD_DWC3_LINK_STATE_MASK;
	unsigned int pwropt;

	/*
	 * WORKAROUND:
	 * Revisions < 2.50a have an issue when configured without Hibernation
	 * mode enabled which would show up when device detects host-initiated
	 * U3 exit.
	 */
	pwropt = SPRD_DWC3_GHWPARAMS1_EN_PWROPT(dwc->hwparams.hwparams1);
	if ((dwc->revision < SPRD_DWC3_REVISION_250A) &&
			(pwropt != SPRD_DWC3_GHWPARAMS1_EN_PWROPT_HIB)) {
		if ((dwc->link_state == SPRD_DWC3_LINK_STATE_U3) &&
				(next == SPRD_DWC3_LINK_STATE_RESUME)) {
			dev_vdbg(dwc->dev, "ignoring transition U3 -> Resume\n");
			return;
		}
	}

	/*
	 * WORKAROUND: Revisions <1.83a have an issue which, depending on the
	 * link partner, the USB session might do multiple entry/exit of low
	 * power states before a transfer takes place.
	 */
	if (dwc->revision < SPRD_DWC3_REVISION_183A) {
		if (next == SPRD_DWC3_LINK_STATE_U0) {
			u32 u1u2;
			u32 val;

			switch (dwc->link_state) {
			case SPRD_DWC3_LINK_STATE_U1:
			case SPRD_DWC3_LINK_STATE_U2:
				val = dwc3_readl(dwc->regs, SPRD_DWC3_DCTL);
				u1u2 = val & (SPRD_DWC3_DCTL_INITU2ENA
						| SPRD_DWC3_DCTL_ACCEPTU2ENA
						| SPRD_DWC3_DCTL_INITU1ENA
						| SPRD_DWC3_DCTL_ACCEPTU1ENA);

				if (!dwc->u1u2) {
					dwc->u1u2 = val & u1u2;
				}
				val &= ~u1u2;

				dwc3_writel(dwc->regs, SPRD_DWC3_DCTL, val);
				break;
			default:
				/* do nothing */
				break;
			}
		}
	}

	switch (next) {
	case SPRD_DWC3_LINK_STATE_RESUME:
		dwc3_resume_gadget(dwc);
		break;
	case SPRD_DWC3_LINK_STATE_U1:
		if (dwc->speed == USB_SPEED_SUPER) {
			dwc3_suspend_gadget(dwc);
		}
		break;
	case SPRD_DWC3_LINK_STATE_U2:
	case SPRD_DWC3_LINK_STATE_U3:
		dwc3_suspend_gadget(dwc);
		break;
	default:
		break;
	}

	dwc->link_state = next;
}

/*
 * dwc3_gadget_hibernation_interrupt
 */
static void dwc3_gadget_hibernation_interrupt(struct dwc3 *dwc,
		unsigned int eventinfo)
{
	unsigned int is_ss = eventinfo & (1UL << 4);

	/*
	 * WORKAROUND: revison 2.20a with hibernation support have a known
	 * issue which can cause USB CV TD.9.23 to fail randomly.
	 */
	if (is_ss ^ (dwc->speed == USB_SPEED_SUPER)) {
		return;
	}
	/* here, enter hibernation*/
}

/*
 * dwc3_gadget_interrupt
 */
static void dwc3_gadget_interrupt(struct dwc3 *dwc,
		const struct dwc3_event_devt *event)
{
	dev_vdbg(dwc->dev, "Device Event %d occurred\n", event->type);
	switch (event->type) {
	case SPRD_DWC3_DEVICE_EVENT_RESET:
		dwc3_gadget_reset_interrupt(dwc);
		break;

	case SPRD_DWC3_DEVICE_EVENT_DISCONNECT:
		dwc3_gadget_disconnect_interrupt(dwc);
		break;

	case SPRD_DWC3_DEVICE_EVENT_WAKEUP:
		dwc3_gadget_wakeup_interrupt(dwc);
		break;

	case SPRD_DWC3_DEVICE_EVENT_CONNECT_DONE:
		dwc3_gadget_conndone_interrupt(dwc);
		break;

	case SPRD_DWC3_DEVICE_EVENT_LINK_STATUS_CHANGE:
		dwc3_gadget_linksts_change_interrupt(dwc, event->event_info);
		break;

	case SPRD_DWC3_DEVICE_EVENT_HIBER_REQ:
		if (!dwc->has_hibernation) {
			WARN(1 ,"unexpected hibernation event\n");
			break;
		}
		dwc3_gadget_hibernation_interrupt(dwc, event->event_info);
		break;

	case SPRD_DWC3_DEVICE_EVENT_SOF:
		dev_vdbg(dwc->dev, "Start of Periodic Frame\n");
		break;

	case SPRD_DWC3_DEVICE_EVENT_EOPF:
		dev_vdbg(dwc->dev, "End of Periodic Frame\n");
		break;

	case SPRD_DWC3_DEVICE_EVENT_OVERFLOW:
		dev_vdbg(dwc->dev, "Overflow\n");
		break;

	case SPRD_DWC3_DEVICE_EVENT_CMD_CMPL:
		dev_vdbg(dwc->dev, "Command Complete\n");
		break;

	case SPRD_DWC3_DEVICE_EVENT_ERRATIC_ERROR:
		dev_vdbg(dwc->dev, "Erratic Error\n");
		break;

	default:
		dev_dbg(dwc->dev, "UNKNOWN IRQ %d\n", event->type);
	}
}

/*
 * dwc3_process_event_entry
 */
static void dwc3_process_event_entry(struct dwc3 *dwc,
		const union dwc3_event *evt)
{
	/* EP IRQ, handle it and return early */
	if (evt->type.is_devspec == 0) {
		return dwc3_endpoint_interrupt(dwc, &evt->depevt);
	}

	switch (evt->type.type) {
	case SPRD_DWC3_EVENT_TYPE_DEV:
		dwc3_gadget_interrupt(dwc, &evt->devt);
		break;
	default:
		dev_err(dwc->dev, "UNKNOWN IRQ type %d\n", evt->raw);
	}
}

/*
 * dwc3_process_event_buf
 */
static irqreturn_t dwc3_process_event_buf(struct dwc3 *dwc, u32 buf)
{
	struct dwc3_event_buffer *evtbuf;
	irqreturn_t ret = IRQ_NONE;
	int left;
	u32 val;

	evtbuf = dwc->ev_buffs[buf];
	left = evtbuf->count;

	if (!(evtbuf->flags & SPRD_DWC3_EVENT_PENDING)) {
		return IRQ_NONE;
	}

	while (left > 0) {
		union dwc3_event evt;

		evt.raw = *(u32 *) (evtbuf->buf + evtbuf->lpos);
		dwc3_process_event_entry(dwc, &evt);

		/*
		 * FIXME we wrap around correctly to the next entry as almost
		 * all entries are 4 bytes in size.
		 */
		evtbuf->lpos = (evtbuf->lpos + 4) % SPRD_DWC3_EVENT_BUFFERS_SIZE;
		left -= 4;

		dwc3_writel(dwc->regs, SPRD_DWC3_GEVNTCOUNT(buf), 4);
	}

	evtbuf->flags &= ~SPRD_DWC3_EVENT_PENDING;
	evtbuf->count = 0;
	ret = IRQ_HANDLED;

	/* unmask interrupt */
	val = dwc3_readl(dwc->regs, SPRD_DWC3_GEVNTSIZ(buf));
	val &= ~SPRD_DWC3_GEVNTSIZ_INTMASK;
	dwc3_writel(dwc->regs, SPRD_DWC3_GEVNTSIZ(buf), val);

	return ret;
}

/*
 * dwc3_thread_interrupt
 */
static irqreturn_t dwc3_thread_interrupt(int irq, void *_dwc)
{
	struct dwc3 *dwc = _dwc;
	unsigned long flag;
	irqreturn_t ret = IRQ_NONE;
	int i;

	spin_lock_irqsave(&dwc->lock, flag);

	for (i = 0; i < dwc->num_event_buffers; i++) {
		ret |= dwc3_process_event_buf(dwc, i);
	}

	spin_unlock_irqrestore(&dwc->lock, flag);

	return ret;
}

/*
 * dwc3_check_event_buf
 */
static irqreturn_t dwc3_check_event_buf(struct dwc3 *dwc, u32 buf)
{
	struct dwc3_event_buffer *evt;
	u32 count;
	u32 val;

	evt = dwc->ev_buffs[buf];

	count = dwc3_readl(dwc->regs, SPRD_DWC3_GEVNTCOUNT(buf));
	count &= SPRD_DWC3_GEVNTCOUNT_MASK;
	if (!count) {
		return IRQ_NONE;
	}

	evt->count = count;
	evt->flags |= SPRD_DWC3_EVENT_PENDING;

	/* Mask interrupt */
	val = dwc3_readl(dwc->regs, SPRD_DWC3_GEVNTSIZ(buf));
	val |= SPRD_DWC3_GEVNTSIZ_INTMASK;
	dwc3_writel(dwc->regs, SPRD_DWC3_GEVNTSIZ(buf), val);

	return IRQ_WAKE_THREAD;
}

/*
 * dwc3_interrupt
 */
static irqreturn_t dwc3_interrupt(int irq, void *_dwc)
{
	struct dwc3 *dwc = _dwc;
	irqreturn_t ret = IRQ_NONE;
	int i;

	spin_lock(&dwc->lock);

	for (i = 0; i < dwc->num_event_buffers; i++) {
		irqreturn_t status;

		status = dwc3_check_event_buf(dwc, i);
		if (status == IRQ_WAKE_THREAD) {
			ret = status;
		}
	}

	spin_unlock(&dwc->lock);

	return ret;
}

static const struct usb_gadget_ops dwc3_gadget_ops = {
	.wakeup			= dwc3_gadget_wakeup,
	.get_frame		= dwc3_gadget_get_frame,
	.pullup			= dwc3_gadget_pullup,
	.set_selfpowered	= dwc3_gadget_set_selfpowered,
	.udc_stop		= dwc3_gadget_stop,
	.udc_start		= dwc3_gadget_start,
};

/*
 * Initializes gadget related registers
 */
int dwc3_gadget_init(struct dwc3 *dwc)
{
	int err;

	dwc->ctrl_req = dma_alloc_coherent(sizeof(*dwc->ctrl_req),
					(unsigned long *)&dwc->ctrl_req_addr);
	if (!dwc->ctrl_req) {
		err = -ENOMEM;
		dev_err(dwc->dev, "fail to allocate ctrl request\n");
		goto out0;
	}

	dwc->ep0_trb = dma_alloc_coherent(sizeof(*dwc->ep0_trb) * 2,
					  (unsigned long *)&dwc->ep0_trb_addr);
	if (!dwc->ep0_trb) {
		err = -ENOMEM;
		dev_err(dwc->dev, "fail to allocate ep0 trb\n");
		goto out1;
	}

	dwc->setup_buf = memalign(CONFIG_SYS_CACHELINE_SIZE,
				  SPRD_DWC3_EP0_BOUNCE_SIZE);
	if (!dwc->setup_buf) {
		err = -ENOMEM;
		goto out2;
	}

	dwc->ep0_bounce = dma_alloc_coherent(SPRD_DWC3_EP0_BOUNCE_SIZE,
					(unsigned long *)&dwc->ep0_bounce_addr);
	if (!dwc->ep0_bounce) {
		err = -ENOMEM;
		dev_err(dwc->dev, "fail to allocate ep0 bounce buffer\n");
		goto out3;
	}

	dwc->gadget.name = "dwc3-gadget";
	dwc->gadget.speed = USB_SPEED_UNKNOWN;
	dwc->gadget.max_speed = USB_SPEED_HIGH;
	dwc->gadget.ops = &dwc3_gadget_ops;
	dwc->gadget.is_a_peripheral = 1;
	dwc->gadget.sg_supported = 1;
	dwc->gadget.is_dualspeed = 1;

	/* needs buffer size to be aligned to MaxPacketSize on ep out. */
	dwc->gadget.quirk_ep_out_aligned_size = true;

	/* REVISIT */
	err = dwc3_gadget_init_endpoints(dwc);
	if (err) {
		goto out4;
	}

	err = usb_add_gadget_udc(dwc->dev, &dwc->gadget);
	if (err) {
		dev_err(dwc->dev, "fail to register udc\n");
		goto out4;
	}

	return 0;

out4:
	dwc3_gadget_free_endpoints(dwc);
	dma_free_coherent(dwc->ep0_bounce);
out3:
	free(dwc->setup_buf);
out2:
	dma_free_coherent(dwc->ep0_trb);
out1:
	dma_free_coherent(dwc->ctrl_req);
out0:
	return err;
}

/*
 * dwc3_gadget_exit
 */
void dwc3_gadget_exit(struct dwc3 *dwc)
{
	usb_del_gadget_udc(&dwc->gadget);
	dwc3_gadget_free_endpoints(dwc);
	dma_free_coherent(dwc->ep0_bounce);
	free(dwc->setup_buf);
	dma_free_coherent(dwc->ep0_trb);
	dma_free_coherent(dwc->ctrl_req);
}

/*
 * Handles ep0 and gadget interrupt
 */
void dwc3_gadget_uboot_handle_interrupt(struct dwc3 *dwc)
{
	int ret = dwc3_interrupt(0, dwc);

	if (ret == IRQ_WAKE_THREAD) {
/*
		int i;
		struct dwc3_event_buffer *evt;
		for (i = 0; i < dwc->num_event_buffers; i++) {
			evt = dwc->ev_buffs[i];
			dwc3_flush_cache((unsigned long)evt->buf, evt->length, 0);
		}
*/
		dwc3_thread_interrupt(0, dwc);
	}
}
