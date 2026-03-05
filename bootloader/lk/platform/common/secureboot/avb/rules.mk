AVB_DIR := $(GET_LOCAL_DIR)

MODULE_SRCS += \
	$(AVB_DIR)/avb_check.c \
	$(AVB_DIR)/uboot_avb_ops.c \
	$(AVB_DIR)/uboot_avb_util.c

include $(AVB_DIR)/libavb/rules.mk
