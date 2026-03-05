ifndef OPENSSL_LIB

ifeq ($(ARCH), arm64)

MODULE_SRCS += \
	$(LOCAL_DIR)/hashes/sprd_sha256_core_armv8.S

else ifeq ($(ARCH), arm)
MODULE_SRCS += \
	$(LOCAL_DIR)/hashes/sha256-armv4.S

else
$(error ARCH = $(BSP_BOARD_ARCH) is not null)

endif

endif
MODULE_SRCS += \
	$(LOCAL_DIR)/hashes/sprd_digest.c \
	$(LOCAL_DIR)/hashes/sprdsha1.c \
	$(LOCAL_DIR)/hashes/sprd_sha256_armv8.c \
	$(LOCAL_DIR)/hashes/sprdsha256.c \
