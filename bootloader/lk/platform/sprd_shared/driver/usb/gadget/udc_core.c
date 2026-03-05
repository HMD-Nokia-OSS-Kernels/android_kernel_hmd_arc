/**
 * udc-core.c - Core UDC Framework
 */

#include <malloc.h>
#include <asm/dma-mapping.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

/*
 * struct usb_udc - describes one usb device controller
 * @driver - gadget driver pointer. For use by the class code
 * @dev - child device to the actual controller
 * @gadget - gadget. For use by the class code
 * @list - for use by the udc class driver
 *
 * This represents the internal data structure which is used by the
 * UDC-class to hold information about udc driver and gadget together.
 */
struct usb_udc {
	struct usb_gadget_driver *driver;
	struct usb_gadget        *gadget;
	struct device             dev;
	struct list_head          list;
};

static struct class *udc_class;
static struct list_node udc_list = LIST_INITIAL_VALUE(udc_list);
DEFINE_MUTEX(udc_lock);

extern void
invalidate_dcache_range(unsigned long start, unsigned long end);
extern void
flush_dcache_range(unsigned long start , unsigned long end);

int usb_gadget_map_request(struct usb_gadget *gadget,
		struct usb_request *req, int is_in)
{
	req->dma = req->buf;
	if (req->length == 0)
		return 0;
	if (is_in)
		flush_dcache_range((unsigned long)req->buf,
			(unsigned long)(req->buf + req->length));
	else
		invalidate_dcache_range((unsigned long)req->dma,
			(unsigned long)(req->dma + req->length));
	return 0;
}

void usb_gadget_unmap_request(struct usb_gadget *gadget,
		struct usb_request *req, int is_in)
{
	if (req->length == 0) {
		return;
	}
	if (is_in) {
		flush_dcache_range((unsigned long)req->buf,
			(unsigned long)(req->buf + req->length));
	} else {
		invalidate_dcache_range((unsigned long)req->dma,
			(unsigned long)(req->dma + req->length));
	}
}

void usb_gadget_set_state(struct usb_gadget *gadget, enum usb_device_state state)
{
	gadget->state = state;
}

/*
 * Give the request back to the gadget layer
 */
void usb_gadget_giveback_request(struct usb_ep *ep, struct usb_request *req)
{
	if (req != NULL && req->complete) {
		req->complete(ep, req);
	}
}

/*
 * tells usb device controller we don't need it anymore
 */
static inline void usb_gadget_udc_stop(struct usb_udc *udc)
{
	udc->gadget->ops->udc_stop(udc->gadget);
}

/*
 * tells usb device controller to start up
 */
static inline int usb_gadget_udc_start(struct usb_udc *udc)
{
	return udc->gadget->ops->udc_start(udc->gadget, udc->driver);
}

/*
 * notifies the udc core that bus reset occurs
 */
void usb_gadget_udc_reset(struct usb_gadget *gadget, struct usb_gadget_driver *dri)
{
	dri->reset(gadget);
	usb_gadget_set_state(gadget, USB_STATE_DEFAULT);
}

/*
 * release the usb_udc struct
 */
static void usb_udc_release(struct device *dev)
{
	struct usb_udc *udc;

	udc = container_of(dev, struct usb_udc, dev);
	kfree(udc);
}

/*
 * adds a new gadget to the udc class driver list
 */
int usb_add_gadget_udc(struct device *parent, struct usb_gadget *gadget)
{
	return usb_add_gadget_udc_release(parent, gadget, NULL);
}

/*
 * adds a new gadget to the udc class driver list
 */
int usb_add_gadget_udc_release(struct device *parent, struct usb_gadget *gadget,
		void (*release)(struct device *dev))
{
	struct usb_udc *udc;
	int ret = -ENOMEM;

	udc = kzalloc(sizeof(*udc), GFP_KERNEL);
	if (!udc) {
		return ret;
	}
	// dev_set_name(&gadget->dev, "gadget");
	gadget->dev.parent = parent;

	udc->dev.parent = parent;
	udc->dev.class = udc_class;
	udc->dev.release = usb_udc_release;

	udc->gadget = gadget;

	mutex_lock(&udc_lock);
	list_add_tail(&udc_list, &udc->list);

	usb_gadget_set_state(gadget, USB_STATE_NOTATTACHED);

	mutex_unlock(&udc_lock);

	return 0;
}

static void __gadget_remove_driver(struct usb_udc *udc)
{
	dev_dbg(&udc->dev, "unregister UDC driver [%s]...\n",
		udc->driver->function);

	usb_gadget_disconnect(udc->gadget);
	udc->driver->disconnect(udc->gadget);
	udc->driver->unbind(udc->gadget);
	usb_gadget_udc_stop(udc);

	udc->driver = NULL;
}

/*
 * deletes @udc from udc_list
 */
void usb_del_gadget_udc(struct usb_gadget *gadget)
{
	struct usb_udc *udc = NULL;

	mutex_lock(&udc_lock);
	list_for_each_entry(udc, &udc_list, list) {
		if (gadget == udc->gadget) {
			goto found;
		}
	}

	dev_err(gadget->dev.parent, "gadget is not registered.\n");
	mutex_unlock(&udc_lock);

	return;

found:
	dev_vdbg(gadget->dev.parent, "unregister gadget\n");

	list_delete(&udc->list);
	mutex_unlock(&udc_lock);

	if (udc->driver) {
		__gadget_remove_driver(udc);
	}
}

/*
 * unregister driver
 */
int usb_gadget_unregister_driver(struct usb_gadget_driver *dri)
{
	int ret = -ENODEV;
	struct usb_udc *udc = NULL;

	if (!dri || !dri->unbind) {
		return -EINVAL;
	}

	mutex_lock(&udc_lock);
	list_for_each_entry(udc, &udc_list, list) {
		if (udc->driver == dri) {
			__gadget_remove_driver(udc);
			usb_gadget_set_state(udc->gadget,
					USB_STATE_NOTATTACHED);
			ret = 0;
			break;
		}
	}

	mutex_unlock(&udc_lock);
	return ret;
}

/*
 * bind to driver
 */
static int __udc_bind_to_driver(struct usb_udc *udc, struct usb_gadget_driver *dri)
{
	int ret;

	dev_dbg(&udc->dev, "registering UDC driver %s\n", dri->function);

	udc->driver = dri;

	ret = dri->bind(udc->gadget);
	if (ret) {
		goto out1;
	}

	ret = usb_gadget_udc_start(udc);
	if (ret) {
		dri->unbind(udc->gadget);
		goto out1;
	}

	usb_gadget_connect(udc->gadget);

	return 0;
out1:
	//TODO
	//if (ret != -EISNAM)
		dev_err(&udc->dev, "failed to start %s: %d\n",
			udc->driver->function, ret);
	udc->driver = NULL;
	return ret;
}

/*
 * probe driver
 */
static int __gadget_probe_driver(struct usb_gadget_driver *dri)
{
	struct usb_udc *udc = NULL;
	int ret;

	if (!dri || !dri->setup || !dri->bind) {
		return -EINVAL;
	}

	mutex_lock(&udc_lock);
	list_for_each_entry(udc, &udc_list, list) {
		/* For now we take the first one */
		if (!udc->driver) {
			goto found;
		}
	}

	errorf("couldn't find an available UDC\n");
	mutex_unlock(&udc_lock);
	return -ENODEV;
found:
	ret = __udc_bind_to_driver(udc, dri);
	mutex_unlock(&udc_lock);
	return ret;
}

int usb_gadget_register_driver(struct usb_gadget_driver *dri)
{
	return __gadget_probe_driver(dri);
}

