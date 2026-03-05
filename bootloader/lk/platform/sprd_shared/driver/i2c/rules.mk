LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/include

# shared platform code
MODULE_SRCS += \
	$(LOCAL_DIR)/i2c_core.c \
	$(LOCAL_DIR)/sprd_i2c.c

ifeq ($(SPRD_I2C_HW_SUPPORT),1)
MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_i2c_hw.c
endif

ifeq ($(SPRD_I2C_HW_V2_SUPPORT),1)
MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_i2c_hw_v2.c
endif

GLOBAL_DEFINES += SPRD_I2C=1

include make/module.mk
