/*
 * usb lk gadget serial driver
 */
#include <sprd_common.h>
#include <errno.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

#include "u_serial.h"
#include "gadget_chips.h"


/* serial defines */
#define SPRD_GS_LONG_NAME			"Sprd Gadget Serial"
#define SPRD_GS_VERSION_NAME			SPRD_GS_LONG_NAME
#define SPRD_GS_VERSION_NUM			0x2400

#include "composite.c"
//#include "usbstring.c"
//#include "config.c"
//#include "epautoconf.c"

extern struct usb_endpoint_descriptor *__init
sprd_usb_find_endpoint(
	struct usb_descriptor_header **src,
	struct usb_descriptor_header **copy,
	struct usb_endpoint_descriptor *match
);
//#include "f_acm.c"
#include "f_serial.c"
#include "u_serial.c"

#define pr_warning(args...) dprintf(INFO,##args)

#define SPRD_GS_VENDOR_ID			0x1782/* SPREADTRUM*/
#define SPRD_GS_PRODUCT_ID			0x4d00/* Linux-USB Serial Gadget */

/* string IDs are assigned dynamically */
#define SPRD_STRING_MANUFACTURER_IDX		0x00
#define SPRD_STRING_PRODUCT_IDX		0x01
#define SPRD_STRING_DESCRIPTION_IDX		0x02

static char sprd_manufacturer[50];
void  usb_serial_cleanup(void);

static struct usb_string sprd_strings_dev[] = {
	[SPRD_STRING_MANUFACTURER_IDX].s = sprd_manufacturer,
	[SPRD_STRING_PRODUCT_IDX].s = SPRD_GS_VERSION_NAME,
	[SPRD_STRING_DESCRIPTION_IDX].s = NULL,
	{0, NULL}
};

static struct usb_gadget_strings sprd_stringtab_dev = {
	.language	= 0x0409,
	.strings	= sprd_strings_dev,
};

static struct usb_gadget_strings *sprd_dev_strings[] = {
	&sprd_stringtab_dev,
	NULL,
};

static struct usb_device_descriptor sprd_device_desc = {
	.bLength =		SPRD_USB_DT_DEVICE_SIZE,
	.bDescriptorType =	SPRD_USB_DT_DEVICE,
	.bcdUSB =		cpu_to_le16(0x0200),
	.bDeviceSubClass =	0,
	.bDeviceProtocol =	0,
	.idVendor =		cpu_to_le16(SPRD_GS_VENDOR_ID),
	.bNumConfigurations =	1,
};

MODULE_DESCRIPTION(SPRD_GS_VERSION_NAME);

static unsigned int n_ports = 0;

static int __init sprd_serial_bind_config(struct usb_configuration *c)
{
	unsigned i;
	int status = 0;

	for (i = 0; i < n_ports && status == 0; i++) {
		status = sprd_gser_bind_config(c, i);
	}
	return status;
}

static struct usb_configuration sprd_serial_config_driver = {
	.bind		= sprd_serial_bind_config,
	.bmAttributes	= SPRD_USB_CONFIG_ATT_SELFPOWER,
};

static int __init sprd_gs_bind(struct usb_composite_dev *cdev)
{
	int			gctl_num;
	struct usb_gadget	*gadget = cdev->gadget;
	int			sprd_status;

	sprd_status = sprd_gserial_setup(cdev->gadget, n_ports);
	if (sprd_status < 0)
		return sprd_status;

	/* device description : -manufacturer, -product */
	sprintf(sprd_manufacturer, "%s with %s", "spreadtrum", gadget->name);
	sprd_status = sprd_usb_string_id(cdev);
	if (sprd_status < 0)
		goto fail;
	sprd_strings_dev[SPRD_STRING_MANUFACTURER_IDX].id = sprd_status;

	sprd_device_desc.iManufacturer = sprd_status;

	sprd_status = sprd_usb_string_id(cdev);
	if (sprd_status < 0)
		goto fail;
	sprd_strings_dev[SPRD_STRING_PRODUCT_IDX].id = sprd_status;

	sprd_device_desc.iProduct = sprd_status;

	/* config description */
	sprd_status = sprd_usb_string_id(cdev);
	if (sprd_status < 0)
		goto fail;
	sprd_strings_dev[SPRD_STRING_DESCRIPTION_IDX].id = sprd_status;

	sprd_serial_config_driver.iConfiguration = sprd_status;

	/* set up other descriptors */
	gctl_num = usb_gadget_controller_number(gadget);
	if (gctl_num >= 0)
		sprd_device_desc.bcdDevice = cpu_to_le16(SPRD_GS_VERSION_NUM | gctl_num);
	else {
		dprintf(INFO,"sprd_gs_bind: controller '%s' not recognized\n",
			gadget->name);
		sprd_device_desc.bcdDevice =
			cpu_to_le16(SPRD_GS_VERSION_NUM | 0x0099);
	}

	/* register our configuration */
	sprd_status = usb_add_config(cdev, &sprd_serial_config_driver);
	if (sprd_status < 0)
		goto fail;

	debugf("%s\n", SPRD_GS_VERSION_NAME);

	return 0;

fail:
	sprd_gserial_cleanup();
	return sprd_status;
}

static int sprd_gs_unbind(struct usb_composite_dev *cdev)
{
	struct usb_gadget *gadget = cdev->gadget;

	free(cdev->config);
	cdev->config = NULL;
	debug("sprd_gs_unbind: calling usb_gadget_disconnect for "
			"controller '%s'\n", gadget->name);
	usb_gadget_disconnect(gadget);

	return 0;
}


static struct usb_composite_driver gserial_driver = {
	.name		= "g_serial",
	.dev		= &sprd_device_desc,
	.strings	= sprd_dev_strings,
	.bind		= sprd_gs_bind,
	.unbind		= sprd_gs_unbind,
};

int __init usb_serial_init(void)
{
	debugf("%s entered!\r\n", __func__);
	/* set n_ports 1 while init */

	if (n_ports) {
		usb_serial_cleanup();
	}
	n_ports = 1;
	sprd_serial_config_driver.label = "Generic Serial config";
	sprd_serial_config_driver.bConfigurationValue = 1;
	sprd_device_desc.bDeviceClass = USB_CLASS_VENDOR_SPEC;
	sprd_device_desc.idProduct = cpu_to_le16(SPRD_GS_PRODUCT_ID);

	sprd_strings_dev[SPRD_STRING_DESCRIPTION_IDX].s = sprd_serial_config_driver.label;

	return usb_composite_register(&gserial_driver);
}
module_init(usb_serial_init);

void  usb_serial_cleanup(void)
{
	debugf("%s entered!\r\n", __func__);
	usb_composite_unregister(&gserial_driver);
	sprd_gserial_cleanup();
	n_ports = 0;
}
module_exit(cleanup);
