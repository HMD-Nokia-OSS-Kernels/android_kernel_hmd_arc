
MODULE_INCLUDES += $(LOCAL_DIR)/aes/aes_gcm/inc

MODULE_SRCS += \
	$(LOCAL_DIR)/aes/aes_gcm/src/aes_gcm_sw.c \
	$(LOCAL_DIR)/aes/aes_gcm/src/cipher_gcm_sw.c \
	$(LOCAL_DIR)/aes/aes_gcm/src/cipher_wrap.c \
	$(LOCAL_DIR)/aes/aes_gcm/src/gcm.c \
	$(LOCAL_DIR)/aes/aes_gcm/src/nv_aes_gcm.c \

ifeq ($(strip $(WITH_CRYPTO_TEST)),true)
MODULE_SRCS += \
	$(LOCAL_DIR)/aes/aes_gcm/src/aes_gcm_test.c
endif

