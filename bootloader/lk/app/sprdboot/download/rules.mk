LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/dl_packet.c \
	$(LOCAL_DIR)/dl_stdio.c \
	$(LOCAL_DIR)/dl_channel.c \
	$(LOCAL_DIR)/dl_cmd_proc.c \
	$(LOCAL_DIR)/dl_root_inspect.c \
	$(LOCAL_DIR)/dl_operate.c \
	$(LOCAL_DIR)/dl_common.c \
	$(LOCAL_DIR)/sparse_img.c \
	$(LOCAL_DIR)/sparse_crc32.c \
	$(LOCAL_DIR)/fb_sparse.c

include make/module.mk
