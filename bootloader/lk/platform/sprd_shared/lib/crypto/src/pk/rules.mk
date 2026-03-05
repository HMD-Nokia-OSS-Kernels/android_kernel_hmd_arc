MODULE_SRCS += \
	$(LOCAL_DIR)/pk/pk1.c \
	$(LOCAL_DIR)/pk/sprd_rsa_sw.c \

include $(LOCAL_DIR)/pk/pkcs1/rules.mk
include $(LOCAL_DIR)/pk/authentication/rules.mk
