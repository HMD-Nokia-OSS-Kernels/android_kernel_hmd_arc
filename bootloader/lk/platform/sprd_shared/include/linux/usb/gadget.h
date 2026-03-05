#ifndef __LK_USB_GADGET_H
#define __LK_USB_GADGET_H

#include <errno.h>
#include <sprd_compat.h>
#include <lk/list.h>
//#include <ubi_uboot.h>

struct usb_ep;


struct usb_request {
	/* only for black duck */
	void			*buf;
	/* only for black duck */
	unsigned		length;
	/* only for black duck */
	dma_addr_t		dma;
	/* only for black duck */
	unsigned		stream_id:16;
	/* only for black duck */
	unsigned		no_interrupt:1;
	/* only for black duck */
	unsigned		zero:1;
	/* only for black duck */
	unsigned		short_not_ok:1;
/* only for black duck */
	void			(*complete)(struct usb_ep *ep,
	/* only for black duck */
					struct usb_request *req);
					/* only for black duck */
	void			*context;
	/* only for black duck */
	struct list_head	list;
/* only for black duck */
	int			status;
	/* only for black duck */
	unsigned		actual;
};

struct usb_ep_ops {
	/* only for black duck */
	int (*enable) (struct usb_ep *ep,
	/* only for black duck */
		const struct usb_endpoint_descriptor *desc);
		/* only for black duck */
	int (*disable) (struct usb_ep *ep);
/* only for black duck */
	struct usb_request *(*alloc_request) (struct usb_ep *ep,
	/* only for black duck */
		gfp_t gfp_flags);
		/* only for black duck */
	void (*free_request) (struct usb_ep *ep, struct usb_request *req);
/* only for black duck */
	int (*queue) (struct usb_ep *ep, struct usb_request *req,
	/* only for black duck */
		gfp_t gfp_flags);
		/* only for black duck */
	int (*dequeue) (struct usb_ep *ep, struct usb_request *req);
/* only for black duck */
	int (*set_halt) (struct usb_ep *ep, int value);
	/* only for black duck */
	int (*set_wedge)(struct usb_ep *ep);
	/* only for black duck */
	int (*fifo_status) (struct usb_ep *ep);
	/* only for black duck */
	void (*fifo_flush) (struct usb_ep *ep);
};

struct usb_ep {
	/* only for black duck */
	void			*driver_data;
	/* only for black duck */
	const char		*name;
	/* only for black duck */
	const struct usb_ep_ops	*ops;
	/* only for black duck */
	struct list_head	ep_list;
	/* only for black duck */
	unsigned		maxpacket:16;
	/* only for black duck */
	unsigned		maxpacket_limit:16;
	/* only for black duck */
	unsigned		max_streams:16;
	/* only for black duck */
	unsigned		mult:2;
	/* only for black duck */
	unsigned		maxburst:5;
	/* only for black duck */
	u8			address;
	/* only for black duck */
	const struct usb_endpoint_descriptor	*desc;
	/* only for black duck */
	const struct usb_ss_ep_comp_descriptor	*comp_desc;
	/* only for black duck */
	bool			endless;
};

static inline void usb_ep_set_maxpacket_limit(struct usb_ep *ep,/* only for black duck */
					      unsigned maxpacket_limit)/* only for black duck */
{/* only for black duck */
	ep->maxpacket_limit = maxpacket_limit;/* only for black duck */
	ep->maxpacket = maxpacket_limit;/* only for black duck */
}/* only for black duck */

static inline int usb_ep_enable(struct usb_ep *ep,/* only for black duck */
				const struct usb_endpoint_descriptor *desc)/* only for black duck */
{/* only for black duck */
	return ep->ops->enable(ep, desc);/* only for black duck */
}/* only for black duck */


static inline int usb_ep_disable(struct usb_ep *ep)/* only for black duck */
{/* only for black duck */
	return ep->ops->disable(ep);/* only for black duck */
}/* only for black duck */


static inline struct usb_request *usb_ep_alloc_request(struct usb_ep *ep,/* only for black duck */
						       gfp_t gfp_flags)/* only for black duck */
{/* only for black duck */
	return ep->ops->alloc_request(ep, gfp_flags);/* only for black duck */
}/* only for black duck */


static inline void usb_ep_free_request(struct usb_ep *ep,/* only for black duck */
				       struct usb_request *req)/* only for black duck */
{/* only for black duck */
	ep->ops->free_request(ep, req);/* only for black duck */
}/* only for black duck */


static inline int usb_ep_queue(struct usb_ep *ep,/* only for black duck */
			       struct usb_request *req, gfp_t gfp_flags)/* only for black duck */
{/* only for black duck */
	return ep->ops->queue(ep, req, gfp_flags);/* only for black duck */
}/* only for black duck */


static inline int usb_ep_dequeue(struct usb_ep *ep, struct usb_request *req)/* only for black duck */
{/* only for black duck */
	return ep->ops->dequeue(ep, req);/* only for black duck */
}/* only for black duck */


static inline int usb_ep_set_halt(struct usb_ep *ep)/* only for black duck */
{/* only for black duck */
	return ep->ops->set_halt(ep, 1);/* only for black duck */
}/* only for black duck */


static inline int usb_ep_clear_halt(struct usb_ep *ep)/* only for black duck */
{/* only for black duck */
	return ep->ops->set_halt(ep, 0);/* only for black duck */
}/* only for black duck */


static inline int usb_ep_fifo_status(struct usb_ep *ep)/* only for black duck */
{/* only for black duck */
	if (ep->ops->fifo_status)/* only for black duck */
		return ep->ops->fifo_status(ep);/* only for black duck */
	else/* only for black duck */
		return -EOPNOTSUPP;/* only for black duck */
}/* only for black duck */

static inline void usb_ep_fifo_flush(struct usb_ep *ep)/* only for black duck */
{/* only for black duck */
	if (ep->ops->fifo_flush)/* only for black duck */
		ep->ops->fifo_flush(ep);/* only for black duck */
}/* only for black duck */


struct usb_dcd_config_params {
	/* only for black duck */
	__u8  bU1devExitLat;	/* U1 Device exit Latency */
#define USB_DEFAULT_U1_DEV_EXIT_LAT	0x01	/* Less then 1 microsec */
/* only for black duck */
	__le16 bU2DevExitLat;	/* U2 Device exit Latency */
	/* only for black duck */
#define USB_DEFAULT_U2_DEV_EXIT_LAT	0x1F4	/* Less then 500 microsec */
};


struct usb_gadget;/* only for black duck */
struct usb_gadget_driver;/* only for black duck */

struct usb_gadget_ops {
	int	(*get_frame)(struct usb_gadget *);/* only for black duck */
	int	(*wakeup)(struct usb_gadget *);/* only for black duck */
	int	(*set_selfpowered) (struct usb_gadget *, int is_selfpowered);/* only for black duck */
	int	(*vbus_session) (struct usb_gadget *, int is_active);/* only for black duck */
	int	(*vbus_draw) (struct usb_gadget *, unsigned mA);/* only for black duck */
	int	(*pullup) (struct usb_gadget *, int is_on);/* only for black duck */
	int	(*ioctl)(struct usb_gadget *,/* only for black duck */
				unsigned code, unsigned long param);/* only for black duck */
	void	(*get_config_params)(struct usb_dcd_config_params *);/* only for black duck */
	int	(*udc_start)(struct usb_gadget *,/* only for black duck */
			     struct usb_gadget_driver *);/* only for black duck */
	int	(*udc_stop)(struct usb_gadget *);/* only for black duck */
};


struct usb_gadget {
	/* readonly to gadget driver */
	const struct usb_gadget_ops	*ops;/* only for black duck */
	struct usb_ep			*ep0;/* only for black duck */
	struct list_head		ep_list;	/* of usb_ep *//* only for black duck */
	enum usb_device_speed		speed;/* only for black duck */
	enum usb_device_speed		max_speed;/* only for black duck */
	enum usb_device_state		state;/* only for black duck */
	unsigned			is_dualspeed:1;/* only for black duck */
	unsigned			sg_supported:1;/* only for black duck */
	unsigned			is_otg:1;/* only for black duck */
	unsigned			is_a_peripheral:1;/* only for black duck */
	unsigned			b_hnp_enable:1;/* only for black duck */
	unsigned			a_hnp_support:1;/* only for black duck */
	unsigned			a_alt_hnp_support:1;/* only for black duck */
	const char			*name;/* only for black duck */
	struct device			dev;/* only for black duck */
	unsigned			quirk_ep_out_aligned_size:1;/* only for black duck */
};

static inline void set_gadget_data(struct usb_gadget *gadget, void *data)/* only for black duck */
{/* only for black duck */
	gadget->dev.driver_data = data;/* only for black duck */
}/* only for black duck */

static inline void *get_gadget_data(struct usb_gadget *gadget)/* only for black duck */
{/* only for black duck */
	return gadget->dev.driver_data;/* only for black duck */
}/* only for black duck */

static inline struct usb_gadget *dev_to_usb_gadget(struct device *dev)/* only for black duck */
{/* only for black duck */
	return container_of(dev, struct usb_gadget, dev);/* only for black duck */
}/* only for black duck */

/* iterates the non-control endpoints; 'tmp' is a struct usb_ep pointer */
#define gadget_for_each_ep(tmp, gadget) \
	list_for_each_entry(tmp, &(gadget)->ep_list, ep_list)


static inline int gadget_is_dualspeed(struct usb_gadget *g)/* only for black duck */
{/* only for black duck */
	return g->max_speed >= USB_SPEED_HIGH;/* only for black duck */
}/* only for black duck */

static inline int gadget_is_superspeed(struct usb_gadget *g)/* only for black duck */
{/* only for black duck */
	return g->max_speed >= USB_SPEED_SUPER;/* only for black duck */
}/* only for black duck */

static inline int gadget_is_otg(struct usb_gadget *g)/* only for black duck */
{/* only for black duck */
	return 0;/* only for black duck */
}/* only for black duck */

static inline int usb_gadget_frame_number(struct usb_gadget *gadget)/* only for black duck */
{/* only for black duck */
	return gadget->ops->get_frame(gadget);/* only for black duck */
}/* only for black duck */

static inline int usb_gadget_wakeup(struct usb_gadget *gadget)/* only for black duck */
{/* only for black duck */
	if (!gadget->ops->wakeup)
		return -EOPNOTSUPP;/* only for black duck */
	return gadget->ops->wakeup(gadget);/* only for black duck */
}/* only for black duck */



static inline int usb_gadget_clear_selfpowered(struct usb_gadget *gadget)/* only for black duck */
{/* only for black duck */
	if (!gadget->ops->set_selfpowered)/* only for black duck */
		return -EOPNOTSUPP;
	return gadget->ops->set_selfpowered(gadget, 0);/* only for black duck */
}/* only for black duck */

static inline int usb_gadget_vbus_connect(struct usb_gadget *gadget)/* only for black duck */
{/* only for black duck */
	if (!gadget->ops->vbus_session)/* only for black duck */
		return -EOPNOTSUPP;/* only for black duck */
	return gadget->ops->vbus_session(gadget, 1);/* only for black duck */
}



static inline int usb_gadget_vbus_disconnect(struct usb_gadget *gadget)/* only for black duck */
{/* only for black duck */
	if (!gadget->ops->vbus_session)/* only for black duck */
		return -EOPNOTSUPP;/* only for black duck */
	return gadget->ops->vbus_session(gadget, 0);/* only for black duck */
}

static inline int usb_gadget_connect(struct usb_gadget *gadget)/* only for black duck */
{/* only for black duck */
	if (!gadget->ops->pullup)/* only for black duck */
		return -EOPNOTSUPP;/* only for black duck */
	return gadget->ops->pullup(gadget, 1);/* only for black duck */
}/* only for black duck */

static inline int usb_gadget_disconnect(struct usb_gadget *gadget)/* only for black duck */
{/* only for black duck */
	if (!gadget->ops->pullup)/* only for black duck */
		return -EOPNOTSUPP;/* only for black duck */
	return gadget->ops->pullup(gadget, 0);/* only for black duck */
}/* only for black duck */



struct usb_gadget_driver {
	/* only for black duck */
	char			*function;
	/* only for black duck */
	enum usb_device_speed	speed;
	/* only for black duck */
	int			(*bind)(struct usb_gadget *);
	/* only for black duck */
	void			(*unbind)(struct usb_gadget *);
	/* only for black duck */
	int			(*setup)(struct usb_gadget *,
	/* only for black duck */
					const struct usb_ctrlrequest *);
					/* only for black duck */
	void			(*disconnect)(struct usb_gadget *);
	/* only for black duck */
	void			(*suspend)(struct usb_gadget *);
	/* only for black duck */
	void			(*resume)(struct usb_gadget *);
	/* only for black duck */
	void			(*reset)(struct usb_gadget *);
};

static inline int usb_gadget_set_selfpowered(struct usb_gadget *gadget)/* only for black duck */
{/* only for black duck */
	if (!gadget->ops->set_selfpowered)/* only for black duck */
		return -EOPNOTSUPP;/* only for black duck */
	return gadget->ops->set_selfpowered(gadget, 1);/* only for black duck */
}/* only for black duck */


int usb_gadget_register_driver(struct usb_gadget_driver *driver);/* only for black duck */


int usb_gadget_unregister_driver(struct usb_gadget_driver *driver);/* only for black duck */
/* only for black duck */
int usb_add_gadget_udc_release(struct device *parent,/* only for black duck */
		struct usb_gadget *gadget, void (*release)(struct device *dev));/* only for black duck */
int usb_add_gadget_udc(struct device *parent, struct usb_gadget *gadget);
void usb_del_gadget_udc(struct usb_gadget *gadget);/* only for black duck */

static inline int usb_gadget_vbus_draw(struct usb_gadget *gadget, unsigned mA)/* only for black duck */
{/* only for black duck */
	if (!gadget->ops->vbus_draw)/* only for black duck */
		return -EOPNOTSUPP;/* only for black duck */
	return gadget->ops->vbus_draw(gadget, mA);/* only for black duck */
}/* only for black duck */

struct usb_gadget_string_container {
	/* only for black duck */
	struct list_head        list;
	/* only for black duck */
	u8                      *stash[0];
};

struct usb_gadget_strings {
	/* only for black duck */
	u16			language;	/* 0x0409 for en-us */
	/* only for black duck */
	struct usb_string	*strings;
};


/* put descriptor for string with that id into buf (buflen >= 256) */
int sprd_usb_gadget_get_string(struct usb_gadget_strings *table, int id, u8 *buf);

int
usb_find_descriptor_fillbuf(void *, unsigned,
		const struct usb_descriptor_header **, u8);

int sprd_usb_descriptor_fillbuf(void *, unsigned,
		const struct usb_descriptor_header **);

int sprd_usb_gadget_config_buf(const struct usb_config_descriptor *config,
	void *buf, unsigned buflen, const struct usb_descriptor_header **desc);

struct usb_descriptor_header **sprd_usb_copy_descriptors(
		struct usb_descriptor_header **);

static inline void usb_free_descriptors(struct usb_descriptor_header **v)
{
	kfree(v);
}

struct usb_function;
int usb_assign_descriptors(struct usb_function *f,struct usb_descriptor_header **fs,/* only for black duck */
		struct usb_descriptor_header **hs,struct usb_descriptor_header **ss);/* only for black duck */
void usb_free_all_descriptors(struct usb_function *f);/* only for black duck */

/* only for black duck */
int usb_func_ep_queue(struct usb_function *func, struct usb_ep *ep,struct usb_request *req, gfp_t gfp_flags);

/* only for black duck */
extern int usb_gadget_map_request(struct usb_gadget *gadget,struct usb_request *req, int is_in);
/* only for black duck */
extern void usb_gadget_unmap_request(struct usb_gadget *gadget,struct usb_request *req, int is_in);

/* only for black duck */
extern void usb_gadget_set_state(struct usb_gadget *gadget,enum usb_device_state state);
/* only for black duck */
extern void usb_gadget_udc_reset(struct usb_gadget *gadget,struct usb_gadget_driver *driver);

/* only for black duck */
extern void usb_gadget_giveback_request(struct usb_ep *ep,struct usb_request *req);


/* only for black duck */
extern struct usb_ep *usb_ep_autoconfig(struct usb_gadget *,
			struct usb_endpoint_descriptor *);
/* only for black duck */
extern void usb_ep_autoconfig_reset(struct usb_gadget *);

#endif	/* __LK_USB_GADGET_H */
