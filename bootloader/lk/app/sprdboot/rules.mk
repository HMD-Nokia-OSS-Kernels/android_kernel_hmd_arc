#ZOVERLAY_TAG_HMD_ONEIMAGE
LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

#add for NYX-253/NYX-208 HMD fastboot oem get_devinfo
${shell echo YPLATFORM_SECURITY_PATCH=`grep 'PLATFORM_SECURITY_PATCH :=' ../../../../sys/build/make/core/version_defaults.mk |awk -F= {'print $2'}`  > HmdVersion.mk}
${shell sed -i "s/PLATFORM_SECURITY_PATCH := //g" HmdVersion.mk}
${shell grep HMD_PRI_AND_SEC= ../../../device/sprd/generic_mpool/module/generic/app/main.mk >> HmdVersion.mk}
${shell grep HMD_MP_VALUE= ../../../device/sprd/generic_mpool/module/generic/app/main.mk >> HmdVersion.mk}

include $(shell pwd)/HmdVersion.mk
include $(shell pwd)/../../../bsp/tools/secureboot_key/config/version.cfg

#add for check that avb_version_vbmeta is the same as PLATFORM_SECURITY_PATCH
$(warning  ### echo PLATFORM_SECURITY_PATCH: "$(YPLATFORM_SECURITY_PATCH)")
ifneq ($(strip $(avb_version_vbmeta)),$(shell date -d 'TZ="GMT" ${YPLATFORM_SECURITY_PATCH}' +%s))
  $(error  ### echo avb_version_vbmeta doesn't match PLATFORM_SECURITY_PATCH: "$(avb_version_vbmeta)" "$(shell date -d 'TZ="GMT" $(YPLATFORM_SECURITY_PATCH)' +%s)")
endif

GLOBAL_DEFINES += \
	HMD_PRODUCTION_VERSION=\"00WW_${HMD_MP_VALUE}_${HMD_PRI_AND_SEC}\" \
	ANTIROLLBACK_HW=\"$(trusted_version)\" \
	ANTIROLLBACK_VBMETA=\"$(avb_version_vbmeta)\" \
	ANTIROLLBACK_VBMETA_SYSTEM=\"$(avb_version_system)\"

$(warning  ################## echo log: "00WW_${HMD_MP_VALUE}_${HMD_PRI_AND_SEC}")
$(warning  ################## echo trusted_version: "$(trusted_version)")
$(warning  ################## echo avb_version_vbmeta: "$(avb_version_vbmeta)")
$(warning  ################## echo avb_version_system: "$(avb_version_system)")

MODULE_DEPS += \
	platform/common \
	lib/fs \
	$(LOCAL_DIR)/download

MODULE_SRCS += \
	$(LOCAL_DIR)/sprdboot.c \
	$(LOCAL_DIR)/sprd_cpcmdline.c \
	$(LOCAL_DIR)/process_cp_coupling_info.c \
	$(LOCAL_DIR)/boot_parse.c \
	$(LOCAL_DIR)/sprdisk.c \
	$(LOCAL_DIR)/calibration_detect.c \
	$(LOCAL_DIR)/boot_mode.c \
	$(LOCAL_DIR)/fastboot.c \
	$(LOCAL_DIR)/recv_mode.c \
	$(LOCAL_DIR)/download.c \
	$(LOCAL_DIR)/sysdump.c \
	$(LOCAL_DIR)/minidump.c \
	$(LOCAL_DIR)/autodloader_mode.c \
	$(LOCAL_DIR)/alarm_mode.c \
	$(LOCAL_DIR)/oem_fastboot_cmd.c \
	$(LOCAL_DIR)/read_miscdata_info.c \
	$(LOCAL_DIR)/read_miscdata_boot_flag.c

ifeq ($(WITH_SMP), 1)
MODULE_SRCS += \
	$(LOCAL_DIR)/boot_os_mp.c
else
MODULE_SRCS += \
	$(LOCAL_DIR)/boot_os.c
endif

# MODULE_CFLAGS += -finstrument-functions

include make/module.mk
