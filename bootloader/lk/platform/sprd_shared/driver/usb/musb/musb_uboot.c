#include <sprd_common.h>
//#include <watchdog.h>
#include <errno.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

//#include <usb.h>
#include "linux-compat.h"
//#include "usb-compat.h"
#include "musb_core.h"
#include "musb_gadget.h"


#define CONFIG_MUSB_GADGET

static struct musb *gadget;
void musb_free(struct musb *musb);

int usb_gadget_handle_interrupts(void)
{
//yang	WATCHDOG_RESET();
	if (!gadget || !gadget->isr)
		return -EINVAL;

	return gadget->isr(0, gadget);
}

int usb_gadget_register_driver(struct usb_gadget_driver *gadget_driver)
{
	int ret;

	if (!gadget_driver || gadget_driver->speed < USB_SPEED_FULL || !gadget_driver->bind ||
	    !gadget_driver->setup) {
		errorf("bad parameter.\n");
		return -EINVAL;
	}

	if (!gadget) {
		errorf("Controller uninitialized\n");
		return -ENXIO;
	}

	ret = sprd_musb_gadget_start(&gadget->g, gadget_driver);
	if (ret < 0) {
		errorf("gadget_start failed with %d\n", ret);
		return ret;
	}

	ret = gadget_driver->bind(&gadget->g);
	if (ret < 0) {
		errorf("bind failed with %d\n", ret);
		return ret;
	}

	return 0;
}

int usb_gadget_unregister_driver(struct usb_gadget_driver *gadget_driver)
{
	if (gadget_driver->disconnect)
		gadget_driver->disconnect(&gadget->g);
	if (gadget_driver->unbind)
		gadget_driver->unbind(&gadget->g);
	return 0;
}

int musb_register(struct musb_hdrc_platform_data *plat_data, void *bdata,
			void *ctl_regs)
{
	struct musb **musbpr;

	switch (plat_data->mode) {
	case MUSB_PERIPHERAL:
		musbpr = &gadget;
		break;
	default:
		return -EINVAL;
	}
	if (gadget != NULL) {
		musb_free(gadget);
		gadget = NULL;
	}
	*musbpr = musb_init_controller(plat_data, (struct device *)bdata, ctl_regs);
	if (!(*musbpr)) {
		errorf("Musb init the controller failed\n");
		return -EIO;
	}

	return 0;
}
