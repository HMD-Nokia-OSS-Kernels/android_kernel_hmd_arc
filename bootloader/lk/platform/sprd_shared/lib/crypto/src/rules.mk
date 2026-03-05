LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/base64.c \
	$(LOCAL_DIR)/sprd_crypto_sw.c \
	$(LOCAL_DIR)/rngs/rand_sw/sha1_rng.c \

include $(LOCAL_DIR)/aes/rules.mk
include $(LOCAL_DIR)/pk/rules.mk
include $(LOCAL_DIR)/hashes/rules.mk
