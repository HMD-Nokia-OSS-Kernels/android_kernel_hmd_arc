SEC_SPRD_DIR := $(GET_LOCAL_DIR)

MODULE_SRCS += \
	$(SEC_SPRD_DIR)/sec_string.c \
	$(SEC_SPRD_DIR)/sec_efuse_api.c \


ifneq (,$(filter $(PLATFORM), qogirn6l))
MODULE_SRCS += \
    $(SEC_SPRD_DIR)/sprd_verify_n6lite.c
else
MODULE_SRCS += \
    $(SEC_SPRD_DIR)/sprd_verify.c
endif