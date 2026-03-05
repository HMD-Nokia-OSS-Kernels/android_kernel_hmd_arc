LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/include

MODULE_SRCS += \
	$(LOCAL_DIR)/pmic_efuse.c \
	$(LOCAL_DIR)/ap_public_efuse.c \
	$(LOCAL_DIR)/high_refresh_efuse.c

include make/module.mk
