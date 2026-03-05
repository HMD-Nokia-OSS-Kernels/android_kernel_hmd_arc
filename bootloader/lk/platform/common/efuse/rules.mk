SEC_EFUSE_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

ifneq (,$(filter $(PLATFORM), pike2))
MODULE_SRCS += \
	$(SEC_EFUSE_DIR)/sec_efuse_pike2.c \
	$(SEC_EFUSE_DIR)/sec_efuse_pike2_drv.c
endif

ifneq (,$(filter $(PLATFORM), sharkle))
MODULE_SRCS += \
	$(SEC_EFUSE_DIR)/sec_efuse_sharkle.c \
	$(SEC_EFUSE_DIR)/sec_efuse_sharkle_drv.c
endif

ifneq (,$(filter $(PLATFORM), sharkl3))
MODULE_SRCS += \
	$(SEC_EFUSE_DIR)/sec_efuse_sharkl3.c \
	$(SEC_EFUSE_DIR)/sec_efuse_sharkl3_drv.c
endif

ifneq (,$(filter $(PLATFORM), sharkl5))
MODULE_SRCS += \
        $(SEC_EFUSE_DIR)/sec_efuse_sharkl5.c \
        $(SEC_EFUSE_DIR)/sec_efuse_sharkl5_drv.c
endif

ifneq (,$(filter $(PLATFORM), sharkl5pro))
MODULE_SRCS += \
        $(SEC_EFUSE_DIR)/sec_efuse_sharkl5pro.c \
        $(SEC_EFUSE_DIR)/sec_efuse_sharkl5pro_drv.c
endif

ifneq (,$(filter $(PLATFORM), qogirl6))
MODULE_SRCS += \
        $(SEC_EFUSE_DIR)/sec_efuse_sharkl6.c \
        $(SEC_EFUSE_DIR)/sec_efuse_sharkl6_drv.c
endif

ifneq (,$(filter $(PLATFORM), qogirn6pro))
MODULE_SRCS += \
        $(SEC_EFUSE_DIR)/sec_efuse_sharkl6pro.c \
        $(SEC_EFUSE_DIR)/sec_efuse_sharkl6pro_drv.c
endif

ifneq (,$(filter $(PLATFORM), qogirn6l))
MODULE_SRCS += \
        $(SEC_EFUSE_DIR)/sec_efuse_qogirn6l.c \
        $(SEC_EFUSE_DIR)/sec_efuse_qogirn6l_drv.c
endif