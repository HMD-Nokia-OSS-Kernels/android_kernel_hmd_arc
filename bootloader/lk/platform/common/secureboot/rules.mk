LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_DEPS += \
	platform/sprd_shared/trustzone \
	platform/sprd_shared/lib/crypto

MODULE_SRCS += \
	$(LOCAL_DIR)/sec_common.c \
	$(LOCAL_DIR)/sprd_imgversion_comm.c \
	$(LOCAL_DIR)/sprd_imgversion.c

include $(LOCAL_DIR)/sprd/rules.mk

include $(LOCAL_DIR)/avb/rules.mk

include make/module.mk
