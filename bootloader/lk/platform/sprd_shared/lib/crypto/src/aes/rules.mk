ifndef OPENSSL_LIB
MODULE_SRCS += \
	$(LOCAL_DIR)/aes/aes_core.c \
	$(LOCAL_DIR)/aes/cbc.c \
	$(LOCAL_DIR)/aes/ctr.c \
	$(LOCAL_DIR)/aes/mode_wrappers.c \

endif
MODULE_SRCS += \
	$(LOCAL_DIR)/aes/aes256_cbc_pkcs7.c \
	$(LOCAL_DIR)/aes/pkcs7_padding.c \

ifeq ($(strip $(WITH_CRYPTO_TEST)),true)
MODULE_SRCS += \
	$(LOCAL_DIR)/aes/aes_test.c
endif

include $(LOCAL_DIR)/aes/aes_gcm/src/rules.mk
