LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += arch/arm64/include/arch/ \
                   $(LOCAL_DIR)/include

MODULE_SRCS += \
        $(LOCAL_DIR)/sprd_crypto_special.c \
        $(LOCAL_DIR)/sec_memcpy_invert.c

ifneq (,$(filter $(PLATFORM), pike2))
MODULE_INCLUDES += $(LOCAL_DIR)/r2p0lite/inc
include $(LOCAL_DIR)/r2p0lite/rules.mk
endif

ifneq (,$(filter $(PLATFORM), sharkle))
MODULE_INCLUDES += $(LOCAL_DIR)/r2p0lite/inc
include $(LOCAL_DIR)/r2p0lite/rules.mk
endif

ifneq (,$(filter $(PLATFORM), sharkl3))
MODULE_INCLUDES += $(LOCAL_DIR)/r2p0lite/inc
include $(LOCAL_DIR)/r2p0lite/rules.mk
endif

ifneq (,$(filter $(PLATFORM), sharkl5))
MODULE_INCLUDES += $(LOCAL_DIR)/r2p0lite/inc
include $(LOCAL_DIR)/r2p0lite/rules.mk
endif

ifneq (,$(filter $(PLATFORM), sharkl5pro))
MODULE_INCLUDES += $(LOCAL_DIR)/r2p0lite/inc
include $(LOCAL_DIR)/r2p0lite/rules.mk
endif

ifneq (,$(filter $(PLATFORM), qogirl6 qogirn6pro qogirn6l))
MODULE_INCLUDES += $(LOCAL_DIR)/r5p0lite/inc
include $(LOCAL_DIR)/r5p0lite/rules.mk
endif

include make/module.mk
