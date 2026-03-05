LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_ddr_debug.c

ifneq (,$(filter $(PLATFORM), sharkl3))
MODULE_SRCS += \
	$(LOCAL_DIR)/ddr_dvfs_sharkl3.c
endif

ifneq (,$(filter $(PLATFORM), sharkl5))
MODULE_SRCS += \
	$(LOCAL_DIR)/ddr_dvfs_sharkl5.c
endif

ifneq (,$(filter $(PLATFORM), sharkl5pro))
MODULE_SRCS += \
	$(LOCAL_DIR)/ddr_dvfs_sharkl5pro.c
endif

ifneq (,$(filter $(PLATFORM), qogirl6))
MODULE_SRCS += \
	$(LOCAL_DIR)/ddr_dvfs_qogirl6.c
endif

ifneq (,$(filter $(PLATFORM), qogirn6pro))
MODULE_SRCS += \
	$(LOCAL_DIR)/ddr_dvfs_qogirn6pro.c
endif

ifneq (,$(filter $(PLATFORM), qogirn6l))
MODULE_SRCS += \
	$(LOCAL_DIR)/ddr_dvfs_qogirn6l.c
endif

ifneq (,$(filter $(PLATFORM), sharkle))
MODULE_SRCS += \
        $(LOCAL_DIR)/ddr_dvfs_sharkle.c
endif

ifneq (,$(filter $(PLATFORM), pike2))
MODULE_SRCS += \
        $(LOCAL_DIR)/ddr_dvfs_pike2.c
endif
