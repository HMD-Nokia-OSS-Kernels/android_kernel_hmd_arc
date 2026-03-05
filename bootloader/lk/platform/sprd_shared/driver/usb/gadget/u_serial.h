#ifndef __LK_U_SERIAL_H
#define __LK_U_SERIAL_H

#include <linux/usb/composite.h>
#include <linux/usb/cdc.h>

struct gserial {
	struct usb_function	func;

	/* port is managed by gserial_connect/gserial_disconnect */
	struct gs_port		*ioport;

	struct usb_ep		*in;
	struct usb_ep		*out;
	struct usb_endpoint_descriptor	*in_desc;
	struct usb_endpoint_descriptor	*out_desc;

	/* REVISIT avoid this CDC-ACM support harder ... */
	struct usb_cdc_line_coding port_line_coding;

	void (*connect)(struct gserial *pgser);
	void (*disconnect)(struct gserial *pgser);
	int (*send_break)(struct gserial *pgser, int duration);
};

/* handled by gadget driver -- port setup/clearup  */
void sprd_gserial_cleanup(void);
int sprd_gserial_setup(struct usb_gadget *u_gadget, unsigned count);

/* handled by individual functions -- gser connect/disconnect */
void sprd_gserial_disconnect(struct gserial *gser);
int sprd_gserial_connect(struct gserial *gser, u8 port_num);

/* gser functions bound to configurations by a config or gadget driver */
int sprd_gser_bind_config(struct usb_configuration *c, u8 port_num);

#endif /* __LK_U_SERIAL_H */
