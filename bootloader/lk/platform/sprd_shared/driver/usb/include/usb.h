#ifndef _USB_H_
#define _USB_H_

#include <linux/usb/ch9.h>
#include <part.h>

#define USB_MAX_INTERFACES	8
#define USB_MAX_ENDPOINTS	16
#define USB_MAX_CHILDREN	8

#define USB_TIMEOUT_MS(usb_pipe) (usb_pipebulk(usb_pipe) ? 5000 : 1000)

/* usb device requests */
struct usb_dev_request {
	__u8	requesttype;
	__u8	request;
	__le16	value;
	__le16	index;
	__le16	length;
} __attribute__ ((packed));

/* usb interface info */
struct usb_interfaces {
	struct usb_interface_descriptor desc;

	__u8    no_of_ep;
	__u8    num_altsetting;
	__u8    act_altsetting;

	struct usb_endpoint_descriptor ep_desc[USB_MAX_ENDPOINTS];
	struct usb_ss_ep_comp_descriptor ss_ep_comp_desc[USB_MAX_ENDPOINTS];
} __attribute__ ((packed));

/* usb configuration information */
struct usb_config {
	struct usb_config_descriptor desc;

	/* number of usb interface */
	__u8	no_of_if;
	/* usb interface info string */
	struct usb_interfaces if_desc[USB_MAX_INTERFACES];
} __attribute__ ((packed));

struct usb_device {
	int	devnum;	
	int	speed;
	char	mf[32];	
	char	prod[32];
	char	serial[32];

	int maxpacketsize;
	unsigned int toggle[2];
	unsigned int halted[2];
	int epmaxpacketin[16];	
	int epmaxpacketout[16];	

	int configno;
	/* Device Descriptor */
	struct usb_device_descriptor descriptor
		__attribute__((aligned(ARCH_DMA_MINALIGN)));
	struct usb_config config; 

	int have_langid;
	int string_langid;
	int (*irq_handle)(struct usb_device *dev);
	unsigned long irq_status;
	int irq_act_len;
	void *privptr;
	unsigned long status;
	unsigned long int_pending;
	int act_len;
	int maxchild;
	int portnr;

	struct usb_device *parent;
	struct usb_device *children[USB_MAX_CHILDREN];
	void *controller;

	unsigned int slot_id;
};

struct int_queue;

/*
 * enum usb type
 */
enum usb_init_type {
	USB_INIT_HOST,
	USB_INIT_DEVICE
};

#endif /*_USB_H_ */
