LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/include

GLOBAL_DEFINES += \
	CONFIG_ETB_DUMP

MODULE_SRCS += \
	$(LOCAL_DIR)/coresight-etb10.c

include make/module.mk
