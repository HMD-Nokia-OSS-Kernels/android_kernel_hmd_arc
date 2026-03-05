LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_div64.c \
	$(LOCAL_DIR)/uuid.c

include make/module.mk
