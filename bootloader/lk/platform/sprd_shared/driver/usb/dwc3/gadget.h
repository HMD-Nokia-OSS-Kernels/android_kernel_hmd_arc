/**
 * gadget.h - DesignWare USB3 DRD Gadget Header
 */

#ifndef __DRIVERS_USB_SPRD_DWC3_GADGET_H__
#define __DRIVERS_USB_SPRD_DWC3_GADGET_H__

#include <linux/usb/gadget.h>
#include "io.h"
#include "sprd_usb3_def.h"

/* DEPCFG parameter 1 */
#define SPRD_DWC3_DEP_CFG_INT_NUM(n)          ((n) << 0)
#define SPRD_DWC3_DEP_CFG_XFER_COMPLETE_EN    (1 << 8)
#define SPRD_DWC3_DEP_CFG_XFER_IN_PROGRESS_EN (1 << 9)
#define SPRD_DWC3_DEP_CFG_XFER_NOT_READY_EN   (1 << 10)
#define SPRD_DWC3_DEP_CFG_FIFO_ERROR_EN       (1 << 11)
#define SPRD_DWC3_DEP_CFG_STREAM_EVENT_EN     (1 << 13)
#define SPRD_DWC3_DEP_CFG_BINTERVAL_M1(n)     ((n) << 16)
#define SPRD_DWC3_DEP_CFG_STREAM_CAPABLE      (1 << 24)
#define SPRD_DWC3_DEP_CFG_EP_NUMBER(n)        ((n) << 25)
#define SPRD_DWC3_DEP_CFG_BULK_BASED          (1 << 30)
#define SPRD_DWC3_DEP_CFG_FIFO_BASED          (1 << 31)

/* DEPCFG parameter 0 */
#define SPRD_DWC3_DEP_CFG_EP_TYPE(n)          ((n) << 1)
#define SPRD_DWC3_DEP_CFG_MAX_PACKET_SIZE(n)  ((n) << 3)
#define SPRD_DWC3_DEP_CFG_FIFO_NUMBER(n)      ((n) << 17)
#define SPRD_DWC3_DEP_CFG_BURST_SIZE(n)       ((n) << 22)
#define SPRD_DWC3_DEP_CFG_DATA_SEQ_NUM(n)     ((n) << 26)
/* This applies for core versions earlier than 1.94a */
#define SPRD_DWC3_DEP_CFG_IGN_SEQ_NUM         (1 << 31)
/* These apply for core versions 1.94a and later */
#define SPRD_DWC3_DEP_CFG_ACTION_INIT         (0 << 30)
#define SPRD_DWC3_DEP_CFG_ACTION_RESTORE      (1 << 30)
#define SPRD_DWC3_DEP_CFG_ACTION_MODIFY       (2 << 30)

/* DEPXFERCFG parameter 0 */
#define SPRD_DWC3_DEPXFERCFG_NUM_XFER_RES(n) ((n) & 0xffff)

/* -------------------------------------------------------------------------- */
struct dwc3;
#define gadget_to_dwc(gdt)     (container_of(gdt, struct dwc3, gadget))
#define to_dwc3_request(req)   (container_of(req, struct dwc3_request, request))
#define to_dwc3_ep(ep)         (container_of(ep, struct dwc3_ep, endpoint))

void dwc3_ep0_out_start(struct dwc3 *dwc);
void dwc3_ep0_interrupt(struct dwc3 *dwc, const struct dwc3_event_depevt *event);
void dwc3_gadget_giveback(struct dwc3_ep *dep, struct dwc3_request *req, int status);
void dwc3_gadget_uboot_handle_interrupt(struct dwc3 *dwc);
int dwc3_gadget_ep0_set_halt(struct usb_ep *ep, int value);
int dwc3_gadget_ep0_queue(struct usb_ep *ep, struct usb_request *req, gfp_t gfp_flags);
int dwc3_gadget_ep_set_halt_internal(struct dwc3_ep *dep, int value, int protocol);
int __dwc3_gadget_ep0_set_halt(struct usb_ep *ep, int value);

/*
 * Gets transfer index from HW, Caller should take care of locking
 * @dwc: dwc3 pointer
 * @num: dwc endpoint number
 */
static inline u32 dwc3_gadget_ep_get_transfer_index(struct dwc3 *dwc, u8 num)
{
	u32 id;

	id = dwc3_readl(dwc->regs, SPRD_DWC3_DEPCMD(num));

	return SPRD_DWC3_DEPCMD_GET_RSC_IDX(id);
}

static inline struct dwc3_request *next_request(struct list_head *list)
{
	if (list_is_empty(list)) {
		return NULL;
	}
	return list_first_entry(list, struct dwc3_request, list);
}

static inline void dwc3_gadget_move_request_queued(struct dwc3_request *request)
{
	struct dwc3_ep *dep = request->dep;

	request->queued = true;
	list_move_tail(&request->list, &dep->req_queued);
}

#endif /* __DRIVERS_USB_SPRD_DWC3_GADGET_H__ */
