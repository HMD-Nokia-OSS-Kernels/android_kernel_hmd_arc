#
# The MIT License (MIT)
# Copyright (c) 2008-2015 Travis Geiselbrecht
# Copyright (c) 2021, Spreadtrum Communications.
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files
# (the "Software"), to deal in the Software without restriction,
# including without limitation the rights to use, copy, modify, merge,
# publish, distribute, sublicense, and/or sell copies of the Software,
# and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

LOCAL_DIR := $(GET_LOCAL_DIR)
MODULE := $(LOCAL_DIR)

MEMBASE := 0x9f000000
MEMSIZE := 0x00F00000   # 15 MB (heap size = 15 MB -_end)
CUSTOM_DEFAULT_STACK_SIZE:=0x2000 #thread stack size 8k+0x100(padding)
#KERNEL_LOAD_OFFSET := 0x100000 # 1MB
KERNEL_LOAD_OFFSET := 0 # 0MB

ifeq ($(ARCH), arm64)
ARM_CPU := cortex-a55
WITH_SMP ?= 0
WITH_KERNEL_VM := 0
GLOBAL_DEFINES += \
    CONFIG_ARM64 \
    CONFIG_PHYS_64BIT

else ifeq ($(ARCH), arm)
ARCH := arm
ARM_CPU := cortex-a7
WITH_SMP ?= 0
WITH_KERNEL_VM := 0
KERNEL_BASE := $(MEMBASE)
GLOBAL_DEFINES += \
    CONFIG_ARM \
    CONFIG_SYS_64BIT_LBA

else
$(error ARCH = $(BSP_BOARD_ARCH) is not null)

endif

ifneq ($(WITH_SMP), 0)
MODULE_SRCS += \
    $(LOCAL_DIR)/secondary_boot.S
endif

MODULE_SRCS += \
    $(LOCAL_DIR)/sprd_bl.c

ifeq ($(SPRD_SENSOR_HUB_LK),1)
GLOBAL_DEFINES += \
	CONFIG_SENSOR_I2C_MATRIX_BASE=0x402A0018 \
	CONFIG_SENSOR_I2C_MATRIX_VALUE=0x3000 \
	CONFIG_SENSOR_HUB_LK
endif

GLOBAL_DEFINES += \
    DT_PLATFORM_ID=9832 \
	CONFIG_SYS_PROMPT="sharkle> "

GLOBAL_DEFINES += MMU_WITH_TRAMPOLINE=1 \

GLOBAL_DEFINES += \
    ADI_R3P0_VER \
    CONFIG_ADIE_SC2721 \
    CONFIG_SYS_HZ=1000 \
    CONFIG_SPRD_TIMER_CLK=1000 \
    CONFIG_PARTITIONS \
    CONFIG_EFI_PARTITION \
    CONFIG_DOS_PARTITION \
    HAVE_BLOCK_DEVICE \
    ARCH_DMA_MINALIGN=64 \
    CONFIG_MMC \
    CONFIG_MMC_HS200_SUPPORT \
    CONFIG_MMC_UHS_SUPPORT \
    CONFIG_MMC_SUPPORTS_TUNING \
    CONFIG_MMC_RST_N_FUNCTION \
    SPRD_SPARSE_SUPER_SPEEDUP \
    DFS_ADDR=0x50800000 \
    DFS_SIZE=0x10400 \
    SP_IRAM_ADDR=0x50800000 \
    SP_IRAM_SIZE=0x10400

# charger
GLOBAL_DEFINES += \
         CALIB_RESISTANCE_MICRO_OHMS=20000

# fastboot speedup
GLOBAL_DEFINES += \
    CONFIG_WR_SPARSE \
    CONFIG_WRBG_SPARSE

# mem layout:fastboot
GLOBAL_DEFINES += \
    FB_BUF_ADDR=0x84000000 \
    FB_BUF_SIZE=0x10000000
# memory layout:download
GLOBAL_DEFINES += \
    DL_EMMC_BUF_ADDR=0x84000000 \
    DL_EMMC_BUF_SIZE=0x10000000 \
    DL_ALT1_BUF_ADDR=0x94000000 \
    DL_ALT1_BUF_SIZE=0x00200000 \
    DL_ALT2_BUF_ADDR=0x94200000 \
    DL_ALT2_BUF_SIZE=0x00200000 \
    DL_SPARSE_TEMP_BUF_ADDR=0x95000000 \
    DL_SPARSE_TEMP_BUF_SIZE=0x00200000
# memory layout:log
GLOBAL_DEFINES += \
    LOG_RESERVED_ADDR=0x9de80000 \
    LOG_RESERVED_SIZE=0x00040000
# memory layout:secboot
GLOBAL_DEFINES += \
    ARG_START_BASE=0x98100000 \
    VBMETA_IMG_BASE=0x99000000 \
    VERIFY_BASE=0x99800000
# memory layout: lcd
GLOBAL_DEFINES += \
    BMP_RESERVED_ADDR=0x9d000000 \
    LOGO_RESERVED_ADDR=0x9e000000 \
    LOGO_BUFFER_SIZE=0x7e9000
# memory layout: sml
GLOBAL_DEFINES += \
    SML_RESERVED_ADDR=0xb0000000 \
    SML_RESERVED_SIZE=0x00020000
# memory layout: tos
GLOBAL_DEFINES += \
    TOS_RESERVED_ADDR=0xb0020000 \
    TOS_RESERVED_SIZE=0x013c00000

# uid
GLOBAL_DEFINES += \
    UID_START=95 \
    UID_END=94 \
    UID_DOUBLE=0

# Enable it for debug build
ifneq ($(DEBUG), 0)
WITH_FUNCTION_SYMBOLS ?= 1
endif

include make/module.mk
