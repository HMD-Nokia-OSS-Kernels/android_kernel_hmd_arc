LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_DEPS += \
	lib/fs \

MODULE_SRCS += \
	$(LOCAL_DIR)/exfat.c \
	$(LOCAL_DIR)/file.c \
	$(LOCAL_DIR)/exfat_format.c \

include make/module.mk
