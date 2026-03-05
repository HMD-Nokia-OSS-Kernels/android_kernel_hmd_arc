LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/include

MODULE_SRCS += \
	$(LOCAL_DIR)/pmic27xx_misc.c \
	$(LOCAL_DIR)/check_reboot.c \
	$(LOCAL_DIR)/reset.c \
	$(LOCAL_DIR)/sprd_glb.c \
	$(LOCAL_DIR)/uid_helper.c \
	$(LOCAL_DIR)/ap_secure_efuse.c \

include make/module.mk
