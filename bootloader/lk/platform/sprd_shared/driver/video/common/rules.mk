LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/cmd_bmp.c \
	$(LOCAL_DIR)/lcd.c \
	$(LOCAL_DIR)/lcd_console.c \
	$(LOCAL_DIR)/splash.c

include make/module.mk