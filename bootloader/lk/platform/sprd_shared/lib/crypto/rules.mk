LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_INCLUDES := $(LOCAL_DIR)/inc

MODULE_SRCS += \

include $(LOCAL_DIR)/src/rules.mk

include make/module.mk
