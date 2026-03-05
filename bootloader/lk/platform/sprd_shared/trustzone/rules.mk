LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/include

MODULE_SRCS += \
	$(LOCAL_DIR)/lk_drv_api.c \
	$(LOCAL_DIR)/tee_smc_call.c \
	$(LOCAL_DIR)/lk_drv_rpmb_comm.c \
	$(LOCAL_DIR)/lk_drv_rpmb.c \
	$(LOCAL_DIR)/sec_rpmb.c

include $(LOCAL_DIR)/$(ARCH)/rules.mk

include $(LOCAL_DIR)/trusty-ql-tipc/rules.mk

include $(LOCAL_DIR)/keymint/rules.mk

ifeq ($(strip $(BSP_BOARD_TEECFG_CUSTOM)),true)
include $(LOCAL_DIR)/teecfg/rules.mk
endif

include make/module.mk
