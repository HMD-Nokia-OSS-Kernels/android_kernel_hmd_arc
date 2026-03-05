LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/include

# shared platform code
MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_pwm.c 

include make/module.mk
