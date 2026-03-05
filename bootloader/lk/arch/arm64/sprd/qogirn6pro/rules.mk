LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/timer.c \
	$(LOCAL_DIR)/misc.c \
	$(LOCAL_DIR)/ddr_qos.c \
	$(LOCAL_DIR)/ap_qos.c \
	$(LOCAL_DIR)/pmu.c \
	$(LOCAL_DIR)/ufs_global_reg.c

include make/module.mk
