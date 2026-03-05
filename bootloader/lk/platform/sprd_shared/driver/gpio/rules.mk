LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/include

MODULE_SRCS += \
	$(LOCAL_DIR)/eic.c

ifeq ($(SPRD_GPIO_SUPPORT),1)
MODULE_SRCS += \
	$(LOCAL_DIR)/gpio.c \
	$(LOCAL_DIR)/gpio_phy.c
endif

ifeq ($(SPRD_GPIO_PLUS_SUPPORT),1)
MODULE_SRCS += \
	$(LOCAL_DIR)/gpio_plus.c
endif

include make/module.mk
