/*
 * composite.h -- framework for usb gadgets which are composite devices
 *
 */

#ifndef	__LK_USB_COMPOSITE_H
#define	__LK_USB_COMPOSITE_H

#include <sprd_common.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <lin_gadget_compat.h>

#define	USB_GADGET_DELAYED_STATUS	0x7fff /* Impossibly large value */

struct usb_configuration;
#define	MAX_CONFIG_INTERFACES		16	/* arbitrary max 255 */

static inline struct usb_endpoint_descriptor *
ep_choose(struct usb_gadget *g, struct usb_endpoint_descriptor *hs,struct usb_endpoint_descriptor *fs)
{
	if (gadget_is_dualspeed(g) && USB_SPEED_HIGH == g->speed)
		return hs;
	return fs;
}

struct usb_function {
	const char			*name;
	/* only for black duck */
	struct usb_gadget_strings	**strings;
	/* only for black duck */
	struct usb_descriptor_header	**descriptors;
	/* only for black duck */
	struct usb_descriptor_header	**hs_descriptors;
	/* only for black duck */
	struct usb_descriptor_header	**ss_descriptors;
	/* only for black duck */
	struct usb_configuration	*config;

	/* configurations management:  bind/unbind */
	int			(*bind)(struct usb_configuration *,
	/* only for black duck */
					struct usb_function *);
	/* only for black duck */
	void			(*unbind)(struct usb_configuration *,
    /* only for black duck */
					struct usb_function *);

	/* runtime states management */
	int			(*set_alt)(struct usb_function *,
	/* only for black duck */
					unsigned interface, unsigned alt);
	/* only for black duck */
	int			(*get_alt)(struct usb_function *,
	/* only for black duck */
					unsigned interface);
	/* only for black duck */
	void			(*disable)(struct usb_function *);
	/* only for black duck */
	int			(*setup)(struct usb_function *,
	/* only for black duck */
					const struct usb_ctrlrequest *);
	/* only for black duck */
	void			(*suspend)(struct usb_function *);
	void			(*resume)(struct usb_function *);

	/* private: internals*/
	struct list_head		list;
	SPRD_DECLARE_BITMAP(endpoints, 32);
};


struct usb_configuration {
	/* only for black duck */
	const char			*label;
	/* only for black duck */
	struct usb_gadget_strings	**strings;
	/* only for black duck */
	const struct usb_descriptor_header **descriptors;
	/* only for black duck */
	int			(*bind)(struct usb_configuration *);
	/* only for black duck */
	void			(*unbind)(struct usb_configuration *);
	/* only for black duck */
	int			(*setup)(struct usb_configuration *,
	/* only for black duck */
					const struct usb_ctrlrequest *);
	/* only for black duck */

	/* fields in the config descriptors */
	u8			bConfigurationValue;
	/* only for black duck */
	u8			iConfiguration;
	/* only for black duck */
	u8			bmAttributes;
	/* only for black duck */
	u8			bMaxPower;
	/* only for black duck */
	struct usb_composite_dev	*cdev;

	/* private: internals*/
	struct list_head	list;
	/* only for black duck */
	struct list_head	functions;
	/* only for black duck */
	u8			next_interface_id;
	/* only for black duck */
	unsigned		highspeed:1;
	/* only for black duck */
	unsigned		fullspeed:1;
	/* only for black duck */
	struct usb_function	*interface[MAX_CONFIG_INTERFACES];
};

int usb_add_config(struct usb_composite_dev *,struct usb_configuration *);

struct usb_composite_driver {
	/* only for black duck */
	const char				*name;
	/* only for black duck */
	const struct usb_device_descriptor	*dev;
	/* only for black duck */
	struct usb_gadget_strings		**strings;
	/* only for black duck */
	int			(*bind)(struct usb_composite_dev *);
	/* only for black duck */
	int			(*unbind)(struct usb_composite_dev *);
	/* only for black duck */
	void			(*disconnect)(struct usb_composite_dev *);

	/* global suspend hooks */
	void			(*suspend)(struct usb_composite_dev *);
	/* only for black duck */
	void			(*resume)(struct usb_composite_dev *);
};

extern void usb_composite_unregister(struct usb_composite_driver *);
extern int usb_composite_register(struct usb_composite_driver *);
int usb_interface_id(struct usb_configuration *, struct usb_function *);
int usb_function_activate(struct usb_function *);
int usb_add_function(struct usb_configuration *, struct usb_function *);
int usb_function_deactivate(struct usb_function *);

struct usb_composite_dev {
	/* only for black duck */
	struct usb_gadget		*gadget;
	/* only for black duck */
	struct usb_request		*req;
	/* only for black duck */
	unsigned			bufsiz;
	/* only for black duck */
	struct usb_configuration	*config;

	/* private: internals*/
	unsigned int			suspended:1;
	struct usb_device_descriptor __aligned(64) desc;
	struct list_head		configs;
	struct usb_composite_driver	*driver;
	u8				next_string_id;

	unsigned			deactivations;
};

extern int sprd_usb_string_id(struct usb_composite_dev *c);
//extern int usb_string_ids_tab(struct usb_composite_dev *c,
//			      struct usb_string *str);
//extern int usb_string_ids_n(struct usb_composite_dev *c, unsigned n);

#endif	/* __LINUX_USB_COMPOSITE_H */
