/*
# Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
# Licensed under the Unisoc General Software License, version 1.0 (the License);
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
# Software distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OF ANY KIND, either express or implied.
# See the Unisoc General Software License, version 1.0 for more details.
*/
//ZOVERLAY_TAG_HMD_ONEIMAGE

struct bootconfig_cmd_data;
void set_bootconfig_len(uint64_t size);
uint64_t get_bootconfig_len(void);
void bootconfig_fixup_vendor_init_flag(uint8_t *ramdisk_addr);
struct bootconfig_cmd_data find_in_bootconfig(uint8_t *ramdisk_addr, char *buf);
int bootconfig_replace(uint8_t *ramdisk_addr, const char *buf, char *change);
int delete_bootconfig(uint8_t *ramdisk_addr, const char *buf);
void bootconfig_chosen_bootargs_append(uint8_t *ramdisk_addr, const char* buf);

#if DEBUG
void bootconfig_fixup_selinux_switch(uint8_t *ramdisk_addr);
#endif

void bootconfig_fixup_ro_boot_ramsize(uint8_t *ramdisk_addr);
void bootconfig_fixup_serialno(uint8_t *ramdisk_addr);
void bootconfig_fixup_dtbo_index(uint8_t *ramdisk_addr);
void bootconfig_fixup_ddrsize_range(uint8_t *ramdisk_addr);
void bootconfig_fixup_wdten(uint8_t *ramdisk_addr);
void bootconfig_fixup_dswdten(uint8_t *ramdisk_addr);
void bootconfig_fixup_verified_boot(uint8_t *ramdisk_addr);
void bootconfig_fixup_flash_lock_state(uint8_t *ramdisk_addr);
void bootconfig_fixup_dvfs_set(uint8_t *ramdisk_addr);

#ifdef CONFIG_ANDROID_AB
void bootconfig_fixup_add_slot_suffix(uint8_t *ramdisk_addr);
#ifdef CONFIG_SPL_DOUBLE_SLOT
void bootconfig_fixup_dual_slot_flag(uint8_t *ramdisk_addr);
#endif
void bootconfig_fixup_add_force_mode(uint8_t *ramdisk_addr);
#endif

#if defined(SPRD_UFS_BOOTDEVICE) || defined(SPRD_EMMC_BOOTDEVICE)
void bootconfig_fixup_boot_device(uint8_t *ramdisk_addr);
#endif

#ifdef SPRD_VBOOT_V2
void bootconfig_fixup_vboot(uint8_t *ramdisk_addr);

#ifdef CONFIG_VBOOT_SYSTEMASROOT
void bootconfig_fixup_vboot_system(uint8_t *ramdisk_addr);
#else
void bootconfig_fixup_ando_vboot(uint8_t *ramdisk_addr);
#endif
#endif

void bootconfig_fixup_hwfeature(uint8_t *ramdisk_addr);
void bootconfig_fixup_high_refresh(uint8_t *ramdisk_addr);
void cp_bootconfig_fixup(uint8_t *ramdisk_addr);

void print_bootconfig_param(uint8_t *ramdisk_addr);
void bootconfig_fixup_hmd_cmdline(uint8_t *ramdisk_addr);

// Add by changmei.chen for hw-anti-rollback version 20241211 begin
void bootconfig_fixup_eps_state(uint8_t *ramdisk_addr);
// Add by changmei.chen for hw-anti-rollback version 20241211 end

