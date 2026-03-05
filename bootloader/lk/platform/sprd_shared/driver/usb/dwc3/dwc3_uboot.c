/**
 * dwc3_uboot.c - DesignWare USB3 DRD Module glue file
 */

#include <malloc.h>
#include <asm/dma-mapping.h>
#include <asm/arch/sprd_reg.h>
#include <sprd_regulator.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include "core.h"
#include "gadget.h"
#include "io.h"
#include "sprd_usb3_def.h"

static struct dwc3_device dwc3_dev = {
	.base = SPRD_USB_BASE,
	.maximum_speed = USB_SPEED_HIGH,
	.dis_u3_susphy_quirk = 1,
	.dis_u2_susphy_quirk = 1,
#ifdef CONFIG_USB_SPRD_DWC31_PHY
	.hsphy_mode = USBPHY_INTERFACE_MODE_UTMI,
#else

	.hsphy_mode = USBPHY_INTERFACE_MODE_UNKNOWN,
#endif
};

#define LDO_LDO_USB "vddusb33"
void usb_phy_enable(u32 is_on);
int usb_phy_enabled(void);

static inline
void usb_ldo_switch(int flag)
{
	if(flag){
		regulator_enable(LDO_LDO_USB);
	} else {
		regulator_disable(LDO_LDO_USB);
	}
}

int usb_driver_init(unsigned int max_speed)
{
#ifndef CONFIG_FPGA
	usb_ldo_switch(1);
#endif
	usb_phy_enable(1);

	if (max_speed <= USB_SPEED_HIGH &&
		max_speed >= USB_SPEED_FULL)
		dwc3_dev.maximum_speed = max_speed;

	return dwc3_uboot_init(&dwc3_dev);
}

void usb_driver_exit(void)
{
	mdelay(50);
	/* deinit dwc3 first, if it's inited */
	if (usb_phy_enabled()) {
		dwc3_uboot_exit(&dwc3_dev);
		usb_phy_enable(0);
	}
#ifndef CONFIG_FPGA
	usb_ldo_switch(0);
#endif
	mdelay(50);
}

int usb_gadget_handle_interrupts(void)
{
	dwc3_uboot_handle_interrupt(&dwc3_dev);
	return 0;
}

