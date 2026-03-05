#include <sprd_common.h>
#include <linux/usb/ch9.h>
#include <errno.h>
#include <linux/usb/gadget.h>
#include <sprd_unaligned.h>
#include <string.h>
#include <part_efi.h>
#include "gadget_chips.h"

#define isdigit(c)      ('0' <= (c) && (c) <= '9')

/* we must assign addresses for configurable endpoints (like net2280) */
static unsigned epnum;

#define SPRD_MANY_ENDPOINTS
#ifdef SPRD_MANY_ENDPOINTS
/* more than 15 configurable endpoints */
static unsigned in_epnum;
#endif

static int sprd_ep_matches(struct usb_gadget *gadget, struct usb_ep *ep,
			struct usb_endpoint_descriptor	*desc)
{
	u8		ep_type;
	const char	*temp;
	u16		max;

	if (NULL != ep->driver_data)
		return 0;

	ep_type = desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;
	if (USB_ENDPOINT_XFER_CONTROL == ep_type)
		return 0;

	if ('e' != ep->name[0])
		return 0;

	if ('-' != ep->name[2]) {
		temp = strrchr(ep->name, '-');
		if (temp) {
			switch (ep_type) {
			case USB_ENDPOINT_XFER_INT:
				if (temp[2] == 's')	/* "--iso" */
					return 0;
				if (gadget_is_pxa(gadget)
						&& temp[1] == 'i')
					return 0;
				break;
			case USB_ENDPOINT_XFER_BULK:
				if (temp[1] != 'b')	/* "--bulk" */
					return 0;
				break;
			case USB_ENDPOINT_XFER_ISOC:
				if (temp[2] != 's')	/* "--iso" */
					return 0;
			}
		} else {
			temp = ep->name + strlen(ep->name);
		}

		/* direction:  -in -out */
		temp--;
		if (!isdigit(*temp)) {
			if (USB_DIR_IN & desc->bEndpointAddress) {
				if (*temp != 'n')
					return 0;
			} else {
				if (*temp != 't')
					return 0;
			}
		}
	}

	max = le16_to_cpu(get_unaligned(&desc->wMaxPacketSize)) & 0x7ff;
	switch (ep_type) {
	case USB_ENDPOINT_XFER_INT:
		if (!gadget->is_dualspeed && max > 64)
			return 0;

	case USB_ENDPOINT_XFER_ISOC:
		if (!gadget->is_dualspeed && max > 1023)
			return 0;
		if (ep->maxpacket < max)
			return 0;

		if ((get_unaligned(&desc->wMaxPacketSize) &
					__constant_cpu_to_le16(3<<11))) {
			if (!gadget->is_dualspeed)
				return 0;
		}
		break;
	}

	/* report address */
	if (isdigit(ep->name[2])) {
		u8	num = simple_strtoul(&ep->name[2], NULL, 10);
		desc->bEndpointAddress |= num;
#ifdef	SPRD_MANY_ENDPOINTS
	} else if (desc->bEndpointAddress & USB_DIR_IN) {
		if (++in_epnum > 15)
			return 0;
		desc->bEndpointAddress = USB_DIR_IN | in_epnum;
#endif
	} else {
		if (++epnum > 15)
			return 0;
		desc->bEndpointAddress |= epnum;
	}

	/* bulk full speed maxpacket */
	if (ep_type == USB_ENDPOINT_XFER_BULK) {
		int pkt_size = ep->maxpacket;

		if (pkt_size > 64)
			pkt_size = 64;
		put_unaligned(cpu_to_le16(pkt_size), &desc->wMaxPacketSize);
	}
	ep->address = desc->bEndpointAddress;
	return 1;
}

struct usb_ep *sprd_find_ep(
	struct usb_gadget *gadget,
	const char *name
)
{
	struct usb_ep	*ep;

	list_for_each_entry(ep, &gadget->ep_list, ep_list) {
		if (!strcmp(ep->name, name))
			return ep;
	}
	return NULL;
}

struct usb_ep *usb_ep_autoconfig(
	struct usb_gadget		*gadget,
	struct usb_endpoint_descriptor	*desc
)
{
	struct usb_ep *ep = NULL;
	u8 type;

	type = desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;

	if (gadget_is_dwc3(gadget)) {
		/*
		 * First try standard, common configuration:
		 *     ep1in-bulk, ep2out-bulk, ep3in-int
		 * to match other udc drivers to avoid confusion in already
		 * deployed software (endpoint numbers hardcoded in userspace
		 * software/drivers)
		 */
		const char *name = NULL;

		if ((desc->bEndpointAddress & USB_DIR_IN) &&
		    type == USB_ENDPOINT_XFER_BULK) {
			name = "ep1in";
		} else if ((desc->bEndpointAddress & USB_DIR_IN) == 0 &&
			 type == USB_ENDPOINT_XFER_BULK) {
			name = "ep1out";
		} else if ((desc->bEndpointAddress & USB_DIR_IN) &&
			 type == USB_ENDPOINT_XFER_INT) {
			name = "ep2in";
		}
		if (name) {
			ep = sprd_find_ep(gadget, name);
		}
		if (ep && sprd_ep_matches(gadget, ep, desc)) {
			return ep;
		}
	}

	/* Second, look at endpoints until an unclaimed one looks usable */
	list_for_each_entry(ep, &gadget->ep_list, ep_list) {
		if (sprd_ep_matches(gadget, ep, desc))
			return ep;
	}

	/* Fail */
	return NULL;
}

void usb_ep_autoconfig_reset(struct usb_gadget *gadget)
{
	struct usb_ep *ep;

	list_for_each_entry(ep, &gadget->ep_list, ep_list) {
		ep->driver_data = NULL;
	}
#ifdef SPRD_MANY_ENDPOINTS
	in_epnum = 0;
#endif
	epnum = 0;
}
