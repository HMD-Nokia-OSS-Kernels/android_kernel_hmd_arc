LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/autosave_encode.c

ifeq ($(PLATFORM),qogirn6pro)
MODULE_SRCS += \
	$(LOCAL_DIR)/autosave_phy_qogirn6pro.c
endif

ifeq ($(PLATFORM),qogirl6)
MODULE_SRCS += \
	$(LOCAL_DIR)/autosave_phy_qogirl6.c
endif

MODULE_SRCS += \
	$(LOCAL_DIR)/autosave_hal.c

include make/module.mk
