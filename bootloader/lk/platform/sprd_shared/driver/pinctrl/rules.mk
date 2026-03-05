LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/include

MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_pinctrl.c

ifeq ($(PLATFORM),sharkl5pro)
MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_pinctrl_sharkl5pro.c
else ifeq ($(PLATFORM),qogirl6)
MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_pinctrl_qogirl6.c
else ifeq ($(PLATFORM),sharkl5)
MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_pinctrl_sharkl5.c
endif

include make/module.mk
