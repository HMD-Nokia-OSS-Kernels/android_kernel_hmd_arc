/*
 * unisoc musb driver
 */
#ifndef __LK_MUSB_GADGET_H
#define __LK_MUSB_GADGET_H

#include <lk/list.h>
//#include <asm/byteorder.h>
#include <errno.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

enum sprd_buffer_map_state {
	UN_MAPPED = 0,
	PRE_MAPPED,
	MUSB_MAPPED
};

struct sprd_musb_request {
	struct usb_request	request;
	struct list_head	list;
	struct musb_ep		*ep;
	struct musb		*musb;
	u8 tx;			/* endpoint direction */
	u8 epnum;
	enum sprd_buffer_map_state map_state;
};

extern void sprd_musb_ep_restart(struct musb *, struct sprd_musb_request *);

extern void sprd_musb_g_giveback(struct musb_ep *, struct usb_request *, int);

extern const struct usb_ep_ops sprd_musb_g_ep0_ops;

extern void sprd_musb_gadget_cleanup(struct musb *);
extern int sprd_musb_gadget_setup(struct musb *);

extern void sprd_musb_g_rx(struct musb *musb, u8 epnum);
extern void sprd_musb_g_tx(struct musb *musb, u8 epnum);

extern struct usb_request * musb_alloc_request(struct usb_ep *ep, gfp_t gfp_flags);
extern void musb_free_request(struct usb_ep *ep, struct usb_request *usb_req);

int sprd_musb_gadget_start(struct usb_gadget *g, struct usb_gadget_driver *driver);

static inline struct sprd_musb_request *to_sprd_musb_request(struct usb_request *usb_req)
{
	if (usb_req)
		return container_of(usb_req, struct sprd_musb_request, request);
	else
		return NULL;
}

struct musb_ep {
	/* part1 */
	struct usb_ep	end_point;
	char		name[12];
	struct musb_hw_ep	*hw_ep;
	struct musb	*musb;
	/* part4 */
	u8		wedged;
	u8		busy;
	u8		hb_mult;
	/* part3 */
	const struct usb_endpoint_descriptor	*desc;
	struct sprd_dma_channel		*dma;
	struct list_head		req_list;
	/* part2 */
	u8		current_epnum;
	u8		type;
	u8		is_in;
	u16		packet_sz;
};

static inline struct sprd_musb_request *sprd_next_request(struct musb_ep *musb_ep)
{
	struct list_head *queue = &musb_ep->req_list;

	if (list_is_empty(queue))
		return NULL;

	return container_of(queue->next, struct sprd_musb_request, list);
}

static inline struct musb_ep *to_musb_ep(struct usb_ep *usb_ep)
{
	if (usb_ep) 
	       return container_of(usb_ep, struct musb_ep, end_point);
	else
		return NULL;
}

#endif		/* __LK_MUSB_GADGET_H */
