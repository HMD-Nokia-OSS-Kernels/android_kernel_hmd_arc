LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += target/$(TARGET)

MODULE_SRCS += \
	$(LOCAL_DIR)/init.c

include make/module.mk

