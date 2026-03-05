LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_log.c \
	$(LOCAL_DIR)/chipram_env.c \
	$(LOCAL_DIR)/sprd_hwfeature.c \
	$(LOCAL_DIR)/android_ab.c \
	$(LOCAL_DIR)/fdtdec.c \
	$(LOCAL_DIR)/sprd_fdt_support.c \
	$(LOCAL_DIR)/sprd_fdt_memory.c \
	$(LOCAL_DIR)/sprd_fdt_boardid.c

ifneq ($(DEBUG), 0)
MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_shell_cmd.c

GLOBAL_DEFINES += \
	CONFIG_LK_SHELL_DELAY=0

endif

ifeq ($(strip $(BSP_LK_FLASH_NV)),true)

GLOBAL_DEFINES += \
	CONFIG_FASTBOOT_FLASH

MODULE_SRCS += \
	$(LOCAL_DIR)/nvmerge.c

NV_CFG_TO_H_INFO = $(shell $(BSP_ROOT_DIR)/bootloader/lk/tools/nvmerge/cfg_to_h.sh $(BSP_ROOT_DIR)/../vendor/sprd/common_configs/nvmerge/$(BSP_SYSTEM_VERSION)/nvmerge.cfg $(BSP_LK_OUT)/build-$(BSP_LK_DEFCONFIG)/nvmerge.h)
$(info $(strip $(findstring error:,$(NV_CFG_TO_H_INFO))))
ifneq ($(strip $(findstring error:,$(NV_CFG_TO_H_INFO))),error:)
$(info $(NV_CFG_TO_H_INFO))
else
$(error $(NV_CFG_TO_H_INFO))
endif

endif

ifeq ($(ENABLE_DDR_BOOT), 1)
MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_ddr_rw.c

GLOBAL_DEFINES += \
	CONFIG_DDR_BOOT \
	CONFIG_DDR_BOOT_BOOTIMAGE_ADR=0xa0000000 \
	CONFIG_DDR_BOOT_DTBOIMAGE_ADR=0x84000000 \
	CONFIG_DDR_BOOT_VENDOR_BOOTIMAGE_ADR=0xc3200000 \
	CONFIG_DDR_BOOT_INIT_BOOTIMAGE_ADR=0xca000000
else
MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_common_rw.c
endif

# common
LINKER_SCRIPT += \
	$(BUILDDIR)/system-onesegment.ld

MODULE_DEPS += \
	lib/fdt \
	lib/cksum \
	lib/console \
	lib/version \
	lib/mincrypt \
	dev/timer/arm_generic \
	platform/common/partition \
	external/lib/miniz \
	external/lib/zlib \
	external/lib/gzip

MODULE_DEPS += \
	platform/common/ddr_debug \
	platform/common/ddr_memtest

ifeq ($(SPRD_TRACE_SUPPORT), 1)
MODULE_DEPS += lib/trace
endif

ifeq ($(ENABLE_SPRD_GICV3), true)

MODULE_DEPS += \
	dev/interrupt/arm_gic_v3

GLOBAL_DEFINES += \
	CONFIG_SPRD_GICV3 \
	TIMER_ARM_GENERIC_SELECTED=CNTHP \
	CONFIG_SPRD_CICV3_ITSNUM=0 \
	CONFIG_SPRd_GICV3_RDNUM=8

else
MODULE_DEPS += \
	dev/interrupt/arm_gic

GLOBAL_DEFINES += \
	TIMER_ARM_GENERIC_SELECTED=CNTP

endif

# secure
ifneq ($(SPRD_ZEBU_SUPPORT),1)
MODULE_DEPS += \
	platform/sprd_shared/trustzone \
	platform/sprd_shared/lib/crypto \
	platform/sprd_shared/driver/crypto
endif

ifeq ($(ENABLE_SECBOOT_OPENSSL), 1)
GLOBAL_DEFINES += \
	CONFIG_SECBOOT_OPENSSL

MODULE_DEPS += \
	external/lib/openssl
endif

ifeq ($(ENABLE_UFS_RPMB_WRITE_PROTECT), 1)
GLOBAL_DEFINES += \
	CONFIG_UFS_RPMB_WRITE_PROTECT
endif

MODULE_DEPS += \
	platform/common/efuse
ifeq ($(strip $(BSP_PRODUCT_SECURE_BOOT)),SPRD)
MODULE_DEPS += platform/common/secureboot
endif

# all unisoc project need to use
GLOBAL_DEFINES += \
	MEMBASE=$(MEMBASE) \
	MEMSIZE=$(MEMSIZE) \
	CONFIG_SYS_TEXT_BASE=$(MEMBASE) \
	CUSTOM_DEFAULT_STACK_SIZE=$(CUSTOM_DEFAULT_STACK_SIZE) \
	CONFIG_OF_LIBFDT_OVERLAY \
	DT_HARDWARE_ID=1 \
	DT_SOC_VER=0x20000 \
	CONFIG_RTC_START_YEAR=1970 \
	PLATFORM_SUPPORTS_PANIC_SHELL=1 \
	CONSOLE_HAS_INPUT_BUFFER=0 \
	KEEP_EL_MODE=0 \
	CHIPRAM_ENV_LOCATION=0x82000000 \
	__KERNEL__ \
	CONFIG_BOARD_KERNEL_CMDLINE \
	PRINT_BUFFER_SIZE=3072

# EFUSE
GLOBAL_DEFINES += \
	CONFIG_SPRD_UID \
	CONFIG_SPRD_AP_NORMAL_EFUSE
# Calibration
GLOBAL_DEFINES += \
	CALIBRATE_ENUM_MS=15000 \
	CALIBRATE_IO_MS=2000

# SprdLog
ifeq ($(SPRD_LOG_SUPPORT), 1)
GLOBAL_DEFINES += \
	CONFIG_SPRD_LOG \
	CONFIG_LOG_2_STORAGE \
	DISABLE_DEBUG_OUTPUT=0
endif

# I2C
WITH_SPRD_HW_I2C ?= 1
ifeq ($(WITH_SPRD_HW_I2C),1)
GLOBAL_DEFINES += CONFIG_SPRD_HW_I2C
endif

# DDR
ifneq ($(SPRD_ZEBU_SUPPORT)$(SPRD_HAPS_SUPPORT), 1)
GLOBAL_DEFINES += \
	SPRD_DDR_AUTO_DETECT
endif

#memory DDR address config.
PHYS_SDRAM_1 := 0x80000000ULL
REAL_SDRAM_SIZE := 0x40000000ULL

GLOBAL_DEFINES += \
	PHYS_SDRAM_1=$(PHYS_SDRAM_1) \
	REAL_SDRAM_SIZE=$(REAL_SDRAM_SIZE) \

GLOBAL_DEFINES += \
	CONFIG_SYS_SDRAM_BASE=PHYS_SDRAM_1

# For Download USB connect check
ifneq ($(SPRD_HAPS_SUPPORT), 1)
GLOBAL_DEFINES += \
	CONFIG_DOWNLOAD_USB_CHECK
endif

# BAT
ifeq ($(ZCFG_LOW_BAT_VOL_CHG_3V), 1)
GLOBAL_DEFINES += \
	BAT_LOW_MODE_SUPPORT \
	LOW_BAT_VOL=3400\
	LOW_BAT_VOL_CHG=3000
else
GLOBAL_DEFINES += \
	BAT_LOW_MODE_SUPPORT \
	LOW_BAT_VOL=3400
endif

# SMPL config  one step is 0.25S
WITH_SMPL ?= 1
ifeq ($(WITH_SMPL),1)
GLOBAL_DEFINES += \
	CONFIG_SMPL_EN \
	CONFIG_SMPL_THRESHOLD=0
endif

#seiral-number
SPRD_UID_FORMAT_16 ?= false
ifeq ($(SPRD_UID_FORMAT_16),true)
GLOBAL_DEFINES += \
	CONFIG_GET_CPU_SERIAL_NUMBER_NO_WD
else
GLOBAL_DEFINES += \
	CONFIG_GET_CPU_SERIAL_NUMBER
endif

# CP
FIXNV_SIZE ?= 0x100000
GLOBAL_DEFINES += \
	CONFIG_ARM7_RAM_ACTIVE \
	FIXNV_SIZE=$(FIXNV_SIZE) \
	CONFIG_DFS_ENABLE

ifneq ($(SPRD_ZEBU_SUPPORT), 1)
GLOBAL_DEFINES += \
	CONFIG_KERNEL_BOOT_CP \
	CONFIG_MEM_LAYOUT_DECOUPLING
endif

ifeq ($(SPRD_LTE_SUPPORT), 1)
GLOBAL_DEFINES += \
	CONFIG_SUPPORT_LTE \
	CONFIG_ADVANCED_LTE
endif
ifeq ($(SPRD_AGDSP_SUPPORT), 1)
LTE_AGDSP_ADDR ?= 0x89000000
LTE_AGDSP_SIZE ?= 0x00600000
GLOBAL_DEFINES += \
	CONFIG_SUPPORT_AGDSP \
	CONFIG_AUDCP_BOOT_VECTOR=0x44800040 \
	AUDCP_HEADER_STR='"SharkL5_AUDCP"' \
	LTE_AGDSP_SIZE=$(LTE_AGDSP_SIZE) \
	LTE_AGDSP_ADDR=$(LTE_AGDSP_ADDR)
endif

# Console
# CBSIZE: Console I/O Buffer Size
GLOBAL_DEFINES += \
	CONFIG_SYS_CBSIZE=4096


#bootargs cmdline buffer size
ifeq ($(BSP_KERNEL_VERSION), kernel5.15)
ifeq ($(ARCH),arm64)
GLOBAL_DEFINES += \
	CONFIG_BOOTAGRS_SIZE=2048
else
GLOBAL_DEFINES += \
	CONFIG_BOOTAGRS_SIZE=1024
endif
else
GLOBAL_DEFINES += \
	CONFIG_BOOTAGRS_SIZE=2048
endif

# SYSDUMP
GLOBAL_DEFINES += \
	SPRD_SYSDUMP \
	SPRD_SYSDUMP_MAGIC=0x80001000 \
	SPRD_DUMP_COMPRESS \
	CONFIG_SYSDUMP_LED \
	CONFIG_RAMDUMP_NO_SPLIT \
	SPRD_MINI_SYSDUMP


# first mode feature, fix bug 1776169
# CONFIG_CUSTOMER_PHONE:
#      used to judge whether it is our platform machine
#      if define CONFIG_CUSTOMER_PHONE, always erase first_mode
ifeq ($(CUSTOMER_PHONE), 1)

GLOBAL_DEFINES += \
	CONFIG_CUSTOMER_PHONE \
	CONFIG_DOWNLOAD_FOR_CUSTOMER \
	CONFIG_NO_GPT_REWRITE_BACKUP
endif

# vboot verify buffer, reuse the fastboot buffer addr
GLOBAL_DEFINES += \
	VBOOT_VERIFY_BUF_ADDR=FB_BUF_ADDR \
	VBOOT_VERIFY_BUF_SIZE=FB_BUF_SIZE

GLOBAL_DEFINES += \
        CONFIG_DOWNLOAD_RECORD

ifeq ($(ENABLE_BOOTCONFIG), true)
MODULE_SRCS += \
	$(LOCAL_DIR)/sprd_bootconfig_support.c

GLOBAL_DEFINES += \
	CONFIG_BOOTCONFIG

endif

ifeq ($(ENABLE_ASYNC_SUPPORT), true)
GLOBAL_DEFINES += \
	SPRD_ASYNC_SUPPORT

endif

include platform/sprd_shared/driver/rules.mk
include make/module.mk

GLOBAL_DEFINES += CONFIG_CHARGER_AW32257_SUPPORT