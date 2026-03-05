/*
 * usb fastboot driver
 */

#include <sprd_common.h>
#include <asm/arch/common.h>
#include <errno.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/composite.h>
#include <linux/usb/usb_uboot.h>
#include <asm/arch/check_reboot.h>
#include "gadget_chips.h"

#define atomic_read

#define INFO	1

unsigned packet_received, packet_sent;
static volatile int txn_done;
int txn_status;

#define DEV_CONFIG_CDC	1
#define GFP_ATOMIC ((gfp_t) 0)
#define GFP_KERNEL ((gfp_t) 0)

#define DRIVER_DESC		"fastboot Gadget"

static const char driver_desc[] = DRIVER_DESC;

/*-------------------------------------------------------------------------*/
struct fastboot_dev {
	struct usb_gadget *gadget;
	struct usb_request *req;	/* for control responses */
	struct usb_request *stat_req;	/* for cdc status */

	u8 config;
	struct usb_ep *in_ep, *out_ep;
	const struct usb_endpoint_descriptor
	*in, *out;

	struct usb_request *tx_req, *rx_req;

	unsigned suspended:1;
	unsigned network_started:1;
};

static struct fastboot_dev l_fbdev;
static struct usb_gadget_driver fastboot_driver;

#define	DEVSPEED	USB_SPEED_HIGH

#define FASTBOOT_VENDOR_NUM		0x18D1	/* Goolge VID */
#define FASTBOOT_PRODUCT_NUM		0x4ee0	/* Fastboot Product ID */


static ushort bcdDevice;

static char *iManufacturer = "Spreadtrum";

static char *iProduct;
static char *iSerialNumber = "20080823";
extern char *get_product_sn(void);

#define STRING_SERIALNUMBER		10
#define STRING_SUBSET			8
#define STRING_CDC			7
#define STRING_CONTROL			5
#define STRING_DATA			4
#define STRING_ETHADDR			3
#define STRING_PRODUCT			2
#define STRING_MANUFACTURER		1

// ning.wei@hmd++ for fastboot ui display sync from solo begin
extern void sprocomm_set_avb_display_background(int count, int is_need);
extern int bg_counts;
extern int key_listener_premssion;
// ning.wei@hmd++ for fastboot ui display sync from solo end

/* holds our biggest descriptor */
#define USB_BUFSIZ	256
#define DEV_CONFIG_VALUE	1	/* cdc or subset */
#define USB_MAX_TRANSFER_SIZE (2 * 1024 * 1024)


static struct usb_endpoint_descriptor hs_sink_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,

	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = __constant_cpu_to_le16(512),
};


/* ... but the "real" data interface has two bulk endpoints */

static const struct usb_interface_descriptor data_intf = {
	.bLength = sizeof data_intf,
	.bDescriptorType = USB_DT_INTERFACE,

	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass = 0x42,
	.bInterfaceProtocol = 3,
	.iInterface = STRING_DATA,
};

static struct usb_endpoint_descriptor fs_sink_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
};


static struct usb_config_descriptor fastboot_config = {
	.bLength = sizeof fastboot_config,
	.bDescriptorType = SPRD_USB_DT_CONFIG,

	/* compute wTotalLength on the fly */
	.bNumInterfaces = 1,
	.bConfigurationValue = DEV_CONFIG_VALUE,
	.iConfiguration = STRING_CDC,
	.bmAttributes = SPRD_USB_CONFIG_ATT_ONE,
	.bMaxPower = 0xfa,
};

static struct usb_qualifier_descriptor dev_qualifier = {
	.bLength = sizeof dev_qualifier,
	.bDescriptorType = USB_DT_DEVICE_QUALIFIER,

	.bcdUSB = __constant_cpu_to_le16(0x0200),
	.bDeviceClass = USB_CLASS_VENDOR_SPEC,

	.bNumConfigurations = 1,
};

/*
 * usb 2.0 devices need to expose both high speed and full speed
 * descriptors, unless they only run at full speed.
 */

static struct usb_endpoint_descriptor hs_source_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,

	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = __constant_cpu_to_le16(512),
};


static struct usb_device_descriptor device_desc = {
	.bLength = sizeof device_desc,
	.bDescriptorType = SPRD_USB_DT_DEVICE,

	.bcdUSB = __constant_cpu_to_le16(0x0200),

	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,

	.idVendor = __constant_cpu_to_le16(FASTBOOT_VENDOR_NUM),
	.idProduct = __constant_cpu_to_le16(FASTBOOT_PRODUCT_NUM),
	.iManufacturer = STRING_MANUFACTURER,
	.iProduct = STRING_PRODUCT,
	.bNumConfigurations = 1,
};

/*-------------------------------------------------------------------------*/

/* descriptors that are built on-demand */
static char product_desc[40] = DRIVER_DESC;
static char serial_number[128];
static char manufacturer[50];

static struct usb_endpoint_descriptor fs_source_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
};

/* static strings, in UTF-8 */
static struct usb_string strings[] = {
	{STRING_MANUFACTURER, manufacturer,},
	{STRING_PRODUCT, product_desc,},
	{STRING_SERIALNUMBER, serial_number,},
	{STRING_DATA, "fastboot Data",},
#ifdef	DEV_CONFIG_CDC
        {STRING_CDC, "fastboot Ethernet",},
        {STRING_CONTROL, "fastboot Communications Control",},
#endif
	{0, NULL}			/* end of list */
};


/*============================================================================*/
static u8 control_req[USB_BUFSIZ] __attribute__ ((aligned(4)));

static struct usb_gadget_strings stringtab = {
	.language = 0x0409,	/* en-us */
	.strings = strings,
};


/*============================================================================*/

static const struct usb_descriptor_header *fs_fastboot_function[] = {
        /* data interface, with altsetting */
        (struct usb_descriptor_header *)&data_intf,
        (struct usb_descriptor_header *)&fs_source_desc,
        (struct usb_descriptor_header *)&fs_sink_desc,
        NULL,
};

static const struct usb_descriptor_header *hs_fastboot_function[] = {
        (struct usb_descriptor_header *)&data_intf,
        (struct usb_descriptor_header *)&hs_source_desc,
        (struct usb_descriptor_header *)&hs_sink_desc,
        NULL,
};


/*
 * one config, two interfaces:  control, data.
 * complications: class descriptors, and an altsetting.
 */
static int config_buf(struct usb_gadget *g, u8 * buf, u8 type, unsigned index, int is_otg)
{
	int len;
	const struct usb_config_descriptor *config;
	const struct usb_descriptor_header **function;
	int hs = 0;

	if (gadget_is_dualspeed(g)) {
		hs = (g->speed == USB_SPEED_HIGH);
		if (type == USB_DT_OTHER_SPEED_CONFIG)
			hs = !hs;
	}
#define which_fn(t)	(hs ? hs_ ## t ## _function : fs_ ## t ## _function)

	debug("%s , type :%d, index %d\n", __func__, type, index);
	if (index >= device_desc.bNumConfigurations)
		return -EINVAL;

	config = &fastboot_config;
	function = which_fn(fastboot);

	len = sprd_usb_gadget_config_buf(config, buf, USB_BUFSIZ, function);
	if (len < 0)
		return len;
	((struct usb_config_descriptor *)buf)->bDescriptorType = type;
	return len;
}

/*-------------------------------------------------------------------------*/
static void fastboot_reset_config(struct fastboot_dev *dev)
{
	if (dev->config == 0)
		return;

	debug("%s\n", __func__);

	/*
	 * disable endpoints, forcing (synchronous) completion of
	 * pending i/o.  then free the requests.
	 */

	if (dev->in) {
		usb_ep_disable(dev->in_ep);
		if (dev->tx_req) {
			usb_ep_free_request(dev->in_ep, dev->tx_req);
			dev->tx_req = NULL;
		}
	}
	if (dev->out) {
		usb_ep_disable(dev->out_ep);
		if (dev->rx_req) {
			usb_ep_free_request(dev->out_ep, dev->rx_req);
			dev->rx_req = NULL;
		}
	}
	dev->config = 0;
}

static int set_fastboot_config(struct fastboot_dev *dev, gfp_t gfp_flags)
{
	int result = 0;
	struct usb_gadget *gadget = dev->gadget;

	dev->in = ep_choose(gadget, &hs_source_desc, &fs_source_desc);
	dev->in_ep->driver_data = dev;

	dev->out = ep_choose(gadget, &hs_sink_desc, &fs_sink_desc);
	dev->out_ep->driver_data = dev;

	result = usb_ep_enable(dev->in_ep, dev->in);
	if (result != 0) {
		debug("enable %s --> %d\n", dev->in_ep->name, result);
		goto done;
	}

	result = usb_ep_enable(dev->out_ep, dev->out);
	if (result != 0) {
		debug("enable %s --> %d\n", dev->out_ep->name, result);
		goto done;
	}

done:
	/* on error, disable any endpoints  */
	if (result < 0) {
		(void)usb_ep_disable(dev->in_ep);
		(void)usb_ep_disable(dev->out_ep);
		dev->in = NULL;
		dev->out = NULL;
	}

	/* caller is responsible for cleanup on error */
	return result;
}

/*-------------------------------------------------------------------------*/
static void fastboot_setup_complete(struct usb_ep *ep, struct usb_request *req)
{
	if (req->status || req->actual != req->length)
		debug("setup complete --> %d, %d/%d\n",
			req->status, req->actual, req->length);
}


/*
 * change our operational config.  must agree with the code
 * that returns config descriptors, and altsetting code.
 */
static int fastboot_set_config(struct fastboot_dev *dev, unsigned number, gfp_t gfp_flags)
{
	int result = 0;
	struct usb_gadget *gadget = dev->gadget;

	switch (number) {
	case DEV_CONFIG_VALUE:
		result = set_fastboot_config(dev, gfp_flags);
		break;
	default:
		result = -EINVAL;
		/* FALL THROUGH */
	case 0:
		break;
	}

	if (result) {
		if (number)
			fastboot_reset_config(dev);
		usb_gadget_vbus_draw(dev->gadget, gadget_is_otg(dev->gadget) ? 8 : 100);
	} else {
		char *speed;
		unsigned power;

		power = 2 * fastboot_config.bMaxPower;
		usb_gadget_vbus_draw(dev->gadget, power);

		switch (gadget->speed) {
		case USB_SPEED_FULL:
			speed = "full";
			break;
		case USB_SPEED_HIGH:
			speed = "high";
			break;
		default:
			speed = "?";
			break;
		}

		dev->config = number;
		debug("%s speed config #%d: %d mA, %s\n", speed, number, power, driver_desc);
	}
	return result;
}


static int fastboot_setup(struct usb_gadget *gadget, const struct usb_ctrlrequest *ctrl)
{
	int is_otg = 0;
	struct fastboot_dev *dev = get_gadget_data(gadget);
	struct usb_request *req = dev->req;

	u16 wLength = le16_to_cpu(ctrl->wLength);
	u16 wValue = le16_to_cpu(ctrl->wValue);
	int ret = -EOPNOTSUPP;
	u16 wIndex = le16_to_cpu(ctrl->wIndex);


	switch (ctrl->bRequest) {

	case USB_REQ_GET_CONFIGURATION:
		if (ctrl->bRequestType != USB_DIR_IN)
			break;
		*(u8 *) req->buf = dev->config;
		ret = min(wLength, (u16) 1);
		break;

	case USB_REQ_GET_DESCRIPTOR:
		if (ctrl->bRequestType != USB_DIR_IN)
			break;
		switch (wValue >> 8) {

		case USB_DT_STRING:
			ret = sprd_usb_gadget_get_string(&stringtab, wValue & 0xff, req->buf);

			if (ret >= 0)
				ret = min(wLength, (u16) ret);

			break;

		case USB_DT_DEVICE_QUALIFIER:
			if (!gadget_is_dualspeed(gadget))
				break;
			/* assumes ep0 uses the same ret for both speeds ... */
			dev_qualifier.bMaxPacketSize0 = device_desc.bMaxPacketSize0;
			ret = min(wLength, (u16) sizeof dev_qualifier);
			memcpy(req->buf, &dev_qualifier, ret);
			break;

		case USB_DT_OTHER_SPEED_CONFIG:
			if (!gadget_is_dualspeed(gadget))
				break;
			/* FALLTHROUGH */
		case SPRD_USB_DT_CONFIG:
			is_otg = gadget_is_otg(gadget);
			ret = config_buf(gadget, req->buf,
				wValue >> 8, wValue & 0xff, is_otg);
			if (ret >= 0)
				ret = min(wLength, (u16) ret);
			break;
		case SPRD_USB_DT_DEVICE:
			device_desc.bMaxPacketSize0 = gadget->ep0->maxpacket;
			ret = min(wLength, (u16) sizeof device_desc);
			memcpy(req->buf, &device_desc, ret);
			break;
		}
		break;

	case USB_REQ_SET_CONFIGURATION:
		if (ctrl->bRequestType != 0)
			break;
		ret = fastboot_set_config(dev, wValue, GFP_ATOMIC);
		l_fbdev.network_started = 1;
		break;

	case USB_REQ_SET_INTERFACE:
		if (ctrl->bRequestType != USB_RECIP_INTERFACE
			|| !dev->config || wIndex > 1)
			break;
		/*
		 * FIXME this is wrong, as is the assumption that
		 * all non-PXA hardware talks real CDC ...
		 */
		debug("set_interface ignored!\n");

done_set_intf:
		break;
	case USB_REQ_GET_INTERFACE:
		if (ctrl->bRequestType != (USB_DIR_IN | USB_RECIP_INTERFACE)
		    || !dev->config || wIndex > 1)
			break;

		/* for CDC, iff carrier is on, data interface is active. */
		if (wIndex != 1)
			*(u8 *) req->buf = 0;
		else {
			/* *(u8 *)req->buf = netif_carrier_ok (dev->net) ? 1 : 0; */
			/* carrier always ok ... */
			*(u8 *) req->buf = 1;
		}
		ret = min(wLength, (u16) 1);
		break;
	default:
		debug("unknown control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest, wValue, wIndex, wLength);
	}

	/* respond with data transfer before status phase? */
	if (ret >= 0) {
		req->length = ret;
		req->zero = ret < wLength;
		ret = usb_ep_queue(gadget->ep0, req, GFP_ATOMIC);
		if (ret < 0) {
			debug("ep_queue --> %d\n", ret);
			req->status = 0;
			fastboot_setup_complete(gadget->ep0, req);
		}
	}

	/* host either stalls (ret < 0) or reports success */
	return ret;
}


static void tx_complete(struct usb_ep *ep, struct usb_request *req)
{
	debug("%s: status %s\n", __func__, (req->status) ? "failed" : "ok");
	packet_sent = 1;
}

static void req_complete(struct usb_ep *ep, struct usb_request *req)
{
	if (req->status || req->actual != req->length)
		debug("req complete --> %d, %d/%d\n",
			req->status, req->actual, req->length);

	txn_status = req->status;
	txn_done = 1;
}

static void fastboot_unbind(struct usb_gadget *gadget)
{
	struct fastboot_dev *dev = get_gadget_data(gadget);

	debug("%s...\n", __func__);

	/* we've already been disconnected ... no i/o is active */
	if (dev->req) {
		usb_ep_free_request(gadget->ep0, dev->req);
		dev->req = NULL;
	}
	set_gadget_data(gadget, NULL);
}

static void fastboot_disconnect(struct usb_gadget *gadget)
{
	fastboot_reset_config(get_gadget_data(gadget));
}

static void fastboot_suspend(struct usb_gadget *gadget)
{
	/* Not used */
}

static void fastboot_resume(struct usb_gadget *gadget)
{
	/* Not used */
}

/*-------------------------------------------------------------------------*/

static int fastboot_bind(struct usb_gadget *gadget)
{
	struct fastboot_dev *dev = &l_fbdev;
	u8 cdc = 1, zlp = 1;
	struct usb_ep *in_ep, *out_ep;
	int gcnum;
	u8 tmp[7];

	dprintf(INFO, "%s controller :%s recognized\n", __func__, gadget->name);
	gcnum = usb_gadget_controller_number(gadget);
	if (gcnum >= 0)
		device_desc.bcdDevice = cpu_to_le16(0x0300 + gcnum);
	else {
		/*
		 * can't assume CDC works.  don't want to default to
		 * anything less functional on CDC-capable hardware,
		 * so we fail in this case.
		 */
		errorf("controller '%s' not recognized", gadget->name);
		return -ENODEV;
	}

	if (iProduct)
		strlcpy(product_desc, iProduct, sizeof product_desc);
	if (iManufacturer)
		strlcpy(manufacturer, iManufacturer, sizeof manufacturer);
	if (bcdDevice)
		device_desc.bcdDevice = cpu_to_le16(bcdDevice);

	iSerialNumber = get_product_sn();
	device_desc.iSerialNumber = STRING_SERIALNUMBER,
	strlcpy(serial_number, iSerialNumber, sizeof serial_number);

	/* all we really need is bulk IN/OUT */
	usb_ep_autoconfig_reset(gadget);
	in_ep = usb_ep_autoconfig(gadget, &fs_source_desc);
	if (!in_ep) {
autoconf_fail:
		errorf("can't autoconfigure on %s\n", gadget->name);
		return -ENODEV;
	}
	in_ep->driver_data = in_ep;	/* claim */

	out_ep = usb_ep_autoconfig(gadget, &fs_sink_desc);
	if (!out_ep)
		goto autoconf_fail;
	out_ep->driver_data = out_ep;	/* claim */

	usb_gadget_set_selfpowered(gadget);

	if (gadget_is_dualspeed(gadget)) {

		/* and that all endpoints are dual-speed */
		hs_source_desc.bEndpointAddress = fs_source_desc.bEndpointAddress;
		hs_sink_desc.bEndpointAddress = fs_sink_desc.bEndpointAddress;
	}

	dev->network_started = 0;
	dev->in_ep = in_ep;
	dev->out_ep = out_ep;

	/* preallocate control message data and buffer */
	dev->req = usb_ep_alloc_request(gadget->ep0, GFP_KERNEL);
	if (!dev->req)
		goto fail;
	dev->req->buf = control_req;
	dev->req->complete = fastboot_setup_complete;
	dev->tx_req = usb_ep_alloc_request(dev->in_ep, GFP_KERNEL);
	dev->tx_req->complete = req_complete;
	dev->rx_req = usb_ep_alloc_request(dev->out_ep, GFP_KERNEL);
	dev->rx_req->complete = req_complete;

	/* ... and maybe likewise for status transfer */

	/* finish hookup to lower layer ... */
	dev->gadget = gadget;
	set_gadget_data(gadget, dev);
	gadget->ep0->driver_data = dev;

	debug("bind controller with the driver\n");
	/*
	 * two kinds of host-initiated state changes:
	 *  - iff DATA transfer is active, carrier is "on"
	 *  - tx queueing enabled if open *and* carrier is "on"
	 */
	return 0;

fail:
	dprintf(INFO, "%s failed", __func__);
	fastboot_unbind(gadget);
	return -ENOMEM;
}

static int usb_fastboot_start(void)
{
	struct fastboot_dev *dev = &l_fbdev;

	while (!dev->network_started)
		usb_gadget_handle_interrupts();

/*
	if (packet_received) {
		debug("%s: packet received\n", __func__);
		if (dev->rx_req) {
			NetReceive(NetRxPackets[0], dev->rx_req->length);
			packet_received = 0;

			rx_submit(dev, dev->rx_req, 0);
		} else
			error("dev->rx_req invalid");
	}
*/
	return 0;
}

static struct usb_gadget_driver fastboot_driver = {
	.speed = DEVSPEED,

	.bind = fastboot_bind,
	.unbind = fastboot_unbind,

	.setup = fastboot_setup,
	.disconnect = fastboot_disconnect,

	.suspend = fastboot_suspend,
	.resume = fastboot_resume,
};

#define mdelay(n)	udelay((n)*1000)

#define BASE_ADDR	0x00000000
#define DEFAULT_CMDLINE	"mem=128M console=null";

#define TAGS_ADDR	(BASE_ADDR + 0x00000100)
#define KERNEL_ADDR	(BASE_ADDR + 0x00800000)
#define RAMDISK_ADDR	(BASE_ADDR + 0x01000000)
//(BASE_ADDR + 0x02000000)


int fb_usb_read(void *_buf, unsigned len)
{
	int r;
	unsigned xfer;
	unsigned char *buf = _buf;
	int count = 0;
	struct usb_request *req = l_fbdev.rx_req;
	struct usb_ep *ep = l_fbdev.out_ep;

	dprintf(INFO, "usb_read(address = 0x%lx,len=0x%x)\n",(long)_buf, len);
	while (len > 0) {
		xfer = (len > USB_MAX_TRANSFER_SIZE) ?
			USB_MAX_TRANSFER_SIZE : ALIGN(len, ep->maxpacket);
		req->buf = buf;
		req->length = xfer;
		//r = udc_request_queue(out, req);
		r = usb_ep_queue(ep, req, GFP_ATOMIC);
		if (r < 0) {
			dprintf(INFO, "usb_read() queue failed\n");
			goto oops;
		}
		//event_wait(&txn_done);
		txn_done = 0;
		while (!txn_done) {
			if (fastboot_charger_connected()) {
				// ning.wei@hmd++ for fastboot ui display sync from solo begin
				if (key_listener_premssion == 1) {
					sprocomm_set_avb_display_background(bg_counts, 0);
				}
				// ning.wei@hmd++ for fastboot ui display sync from solo end
				usb_gadget_handle_interrupts();
			} else {
				/* usb has disconnected */
				return -ESHUTDOWN;
			}
		}

		if (txn_status < 0) {
			dprintf(INFO, "usb_read() transaction failed\n");
			goto oops;
		}
		if ((count % 0x100000) == 0)
			dprintf(INFO, "remained size = 0x%x\n", len);

		count += req->actual;
		buf += req->actual;
		len -= req->actual;

		/* short transfer? */
		if (req->actual != xfer)
			break;
	}

	return count;

oops:
	return -1;
}

int fb_usb_read_triger(void *_buf, unsigned len)
{
	int r;
	unsigned xfer;
	unsigned char *buf = _buf;
	struct usb_request *req = l_fbdev.rx_req;
	struct usb_ep *ep = l_fbdev.out_ep;

	if (len > USB_MAX_TRANSFER_SIZE) {
		dprintf(INFO, "triger fail, len %d > MAX %d\n", len, USB_MAX_TRANSFER_SIZE);
		return -1;
	}

	xfer = (len > USB_MAX_TRANSFER_SIZE) ?
			USB_MAX_TRANSFER_SIZE : ALIGN(len, ep->maxpacket);
	req->buf = buf;
	req->length = xfer;
	r = usb_ep_queue(ep, req, GFP_ATOMIC);
	if (r < 0) {
		dprintf(INFO, "usb read triger queue failed\n");
		return -2;
	}

	return 0;
}

int fb_read_usb_query_finish(void)
{
	struct usb_request *req = l_fbdev.rx_req;
	txn_done = 0;

	while (!txn_done) {
		usb_gadget_handle_interrupts();
	}

	return (int)req->actual;
}

int fb_usb_write(void *buf, unsigned len)
{
	int r;
	struct usb_request *req = l_fbdev.tx_req;
	struct usb_ep *ep = l_fbdev.in_ep;

	req->buf = buf;
	req->length = len;
	txn_done = 0;
	//r = udc_request_queue(in, req);
	r = usb_ep_queue(ep, req, GFP_ATOMIC);
	if (r < 0) {
		dprintf(INFO, "usb_write() queue failed\n");
		goto oops;
	}
	//event_wait(&txn_done);
	while (!txn_done)
		usb_gadget_handle_interrupts();
	if (txn_status < 0) {
		dprintf(INFO, "usb_write() transaction failed\n");
		goto oops;
	}
	return req->actual;

oops:
	return -1;
}


int usb_fastboot_init(void)
{
	int status = 0;

	usb_driver_init(USB_SPEED_HIGH);

	status = usb_gadget_register_driver(&fastboot_driver);
	if (status < 0)
		goto fail;
	dprintf(INFO, "usb gadget register driver ok!\n");
	usb_fastboot_start();
	dprintf(INFO, "usb fastboot start finish!\n");
	return 0;

fail:
	dprintf(INFO, "%s failed. error = %d", __func__, status);
	return status;
}

int usb_fastboot_exit(void)
{
	int status = 0;

	dprintf(INFO, "%s \r\n", __func__);
	usb_driver_exit();
	return 0;
}

