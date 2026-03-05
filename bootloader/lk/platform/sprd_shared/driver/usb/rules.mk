LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/include

MODULE_SRCS += \
	$(LOCAL_DIR)/gadget/serial.c \
	$(LOCAL_DIR)/gadget/usbstring.c \
	$(LOCAL_DIR)/gadget/epautoconf.c \
	$(LOCAL_DIR)/gadget/config.c \
	$(LOCAL_DIR)/gadget/u_dloader.c \
	$(LOCAL_DIR)/gadget/f_fastboot.c


WITH_USB_SPRD_DWC3 ?= 0

ifeq ($(WITH_USB_SPRD_DWC3),1)
#================SPRD_DWC3===================
MODULE_SPRD_DWC3_SRCS := \
	$(LOCAL_DIR)/dwc3/core.c \
	$(LOCAL_DIR)/dwc3/gadget.c \
	$(LOCAL_DIR)/dwc3/ep0.c \
	$(LOCAL_DIR)/dwc3/dwc3_uboot.c \
	$(LOCAL_DIR)/dwc3/sprd_usb3_driver.c \
	$(LOCAL_DIR)/gadget/udc_core.c

GLOBAL_DEFINES += \
    CONFIG_USB_SPRD_DWC3_GADGET \
    CONFIG_USB_SPRD_DWC3

ifeq ($(PLATFORM), qogirn6pro)
MODULE_SRCS += \
	$(MODULE_SPRD_DWC3_SRCS) \
	$(LOCAL_DIR)/dwc3/sprd_usb31_qogirn6pro_phy.c

GLOBAL_DEFINES += \
    CONFIG_USB_SPRD_DWC31_PHY
endif

else
# ===============MUSB==================
MODULE_MUSB_SRCS := \
	$(LOCAL_DIR)/musb/sprd_musbhsdma.c \
	$(LOCAL_DIR)/musb/musb_gadget.c \
	$(LOCAL_DIR)/musb/musb_core.c \
	$(LOCAL_DIR)/musb/musb_uboot.c \
	$(LOCAL_DIR)/musb/musb_gadget_ep0.c \
	$(LOCAL_DIR)/musb/sprd_musb2_driver.c

ifeq ($(PLATFORM), sharkl3)
MODULE_SRCS += \
	$(MODULE_MUSB_SRCS) \
	$(LOCAL_DIR)/musb/sharkl3_usb_phy.c
else ifeq ($(PLATFORM), pike2)
MODULE_SRCS += \
	$(MODULE_MUSB_SRCS) \
	$(LOCAL_DIR)/musb/pike2_usb_phy.c
else ifeq ($(PLATFORM), sharkle)
MODULE_SRCS += \
	$(MODULE_MUSB_SRCS) \
	$(LOCAL_DIR)/musb/sharkle_usb_phy.c
else ifeq ($(PLATFORM), sharkl5)
MODULE_SRCS += \
	$(MODULE_MUSB_SRCS) \
	$(LOCAL_DIR)/musb/sharkl5_usb_phy.c
else ifeq ($(PLATFORM), sharkl5pro)
MODULE_SRCS += \
	$(MODULE_MUSB_SRCS) \
	$(LOCAL_DIR)/musb/sharkl5pro_usb_phy.c
else ifeq ($(PLATFORM), qogirl6)
MODULE_SRCS += \
	$(MODULE_MUSB_SRCS) \
	$(LOCAL_DIR)/musb/qogirl6_usb_phy.c
else ifeq ($(PLATFORM), qogirn6l)
MODULE_SRCS += \
	$(MODULE_MUSB_SRCS) \
	$(LOCAL_DIR)/musb/qogirn6l_usb_phy.c
endif

endif

include make/module.mk
