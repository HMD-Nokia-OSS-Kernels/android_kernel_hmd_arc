LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/timer.c \
	$(LOCAL_DIR)/misc.c \
	$(LOCAL_DIR)/pmu.c \
	$(LOCAL_DIR)/ufs_cfg.c

include make/module.mk
