LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
        $(LOCAL_DIR)/include

MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_ufs.c	\
	$(LOCAL_DIR)/protocol_util.c	\
	$(LOCAL_DIR)/uiccmd.c

ifeq ($(SPRD_RPMB_SUPPORT),1)
MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_rpmb.c
endif

include make/module.mk
