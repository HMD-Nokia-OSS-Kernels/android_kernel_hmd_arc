LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/include

MODULE_SRCS += \
	$(LOCAL_DIR)/part.c \
	$(LOCAL_DIR)/part_efi.c \
	$(LOCAL_DIR)/part_dos.c

include make/module.mk
