LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

GLOBAL_INCLUDES += \
	$(LOCAL_DIR)/include

# shared platform code
MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_battery.c \
	$(LOCAL_DIR)/sprd_chg_helper.c \
	$(LOCAL_DIR)/sprd_chg_logo.c \
	$(LOCAL_DIR)/sprd_fgu.c \
	$(LOCAL_DIR)/vbat_check.c \


ifeq ($(SPRD_CHARGER_FAN54015_SUPPORT),1)
MODULE_SRCS += \
	$(LOCAL_DIR)/fan54015.c
endif

ifeq ($(SPRD_CHARGER_AW32257_SUPPORT),1)
MODULE_SRCS += \
	$(LOCAL_DIR)/aw32257.c
endif

ifeq ($(SPRD_CHARGER_BQ2560X_SUPPORT),1)
MODULE_SRCS += \
	$(LOCAL_DIR)/bq2560x.c
endif

ifeq ($(SPRD_CHARGER_UPM6920_SUPPORT),1)
MODULE_SRCS += \
	$(LOCAL_DIR)/upm6920.c
endif

ifeq ($(SPRD_CHARGER_SGM41511_SUPPORT),1)
MODULE_SRCS += \
	$(LOCAL_DIR)/sgm41511.c
endif
ifeq ($(SPRD_CHARGER_SGM41516_SUPPORT),1)
MODULE_SRCS += \
	$(LOCAL_DIR)/sgm41516.c
endif

ifeq ($(SPRD_CHARGER_RT9471_SUPPORT),1)
MODULE_SRCS += \
	$(LOCAL_DIR)/rt9471.c
endif

ifeq ($(SPRD_CHARGER_BQ25890_SUPPORT),1)
MODULE_SRCS += \
	$(LOCAL_DIR)/bq25890.c
endif

ifeq ($(SPRD_CHARGER_SGM4154X_SUPPORT),1)
MODULE_SRCS += \
	$(LOCAL_DIR)/sgm4154x.c
endif

ifeq ($(SPRD_CHARGER_SY6970_SUPPORT),1)
MODULE_SRCS += \
	$(LOCAL_DIR)/sy6970.c
endif

ifeq ($(SPRD_CHARGER_SC8960X_SUPPORT),1)
MODULE_SRCS += \
	$(LOCAL_DIR)/sc8960x.c
endif

ifeq (CONFIG_ADIE_SC2720, $(findstring CONFIG_ADIE_SC2720, $(GLOBAL_DEFINES)))
MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_chg_2720.c
else ifeq (CONFIG_ADIE_SC2721, $(findstring CONFIG_ADIE_SC2721, $(GLOBAL_DEFINES)))
MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_chg_2721.c
else ifeq (CONFIG_ADIE_SC2730, $(findstring CONFIG_ADIE_SC2730, $(GLOBAL_DEFINES)))
MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_chg_2730.c
else ifeq (CONFIG_ADIE_9620, $(findstring CONFIG_ADIE_9620, $(GLOBAL_DEFINES)))
MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_chg_9620.c
else ifeq (CONFIG_ADIE_UMP518, $(findstring CONFIG_ADIE_UMP518, $(GLOBAL_DEFINES)))
MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_chg_9620.c
endif

include make/module.mk
