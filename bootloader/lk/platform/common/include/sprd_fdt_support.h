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

#ifndef __SPRD_FDT_SUPPORT__
#define __SPRD_FDT_SUPPORT__
//ZOVERLAY_TAG_HMD_ONEIMAGE
#include <libfdt_env.h>
#include <asm/types.h>
#include <fdtdec.h>
#include <lk/board.h>
#define SPRD_DT_SUCCESS        0
#define SPRD_DT_VERSION        1
#define SPRD_DT_HEADER_SIZE    12
#define SPRD_DT_MAGIC          0x44525053 /* "SPRD" */
#define SPRD_DT_MAGIC_LEN      4
#define NRIQ_MODE   "nr_iq"

struct dt_table_t
{
	uint32_t magic;
	uint32_t version;
	uint32_t num_of_entries;
};

struct dt_entry_t
{
	uint32_t dt_platform_id;
	uint32_t dt_hardware_id;
	uint32_t dt_soc_rev;
	uint32_t dt_offset;
	uint32_t dt_size;
};

int fdt_print_bootargs(void *fdt);
int fdt_fixup_verified_boot(void *fdt);
int fdt_fixup_flash_lock_state(void *fdt);

#ifdef SPRD_VBOOT_V2
int fdt_fixup_vboot(void *fdt);
#ifdef CONFIG_VBOOT_SYSTEMASROOT
int fdt_fixup_vboot_system(void *fdt);
#else
int fdt_fixup_ando_vboot(void *fdt);
#endif
#endif

int fdt_initrd_norsvmem(void *fdt, ulong initrd_start, ulong initrd_end, int force);
int fdt_chosen_bootargs_append(void *fdt, const char *append_args, int force);
#ifdef CONFIG_TEE_FIREWALL
int fdt_reserved_mem_multimedia_parse(void*fdt);
#endif
int fdt_chosen_bootargs_replace(void *fdt, const char *old_args, const char *new_args);
void fdt_fixup_pmic_wa(void *fdt);
int fdt_fixup_parse_miscdata_cmd(char *cmd, char *value);
int fdt_fixup_nrphy_iqmem(void *blob, const char *node_name);
int fdt_fixup_iq_reserved_mem(void *fdt);
int fdt_fixup_cp_reserved_mem(void *fdt);
int fdt_fixup_cp_boot(void *fdt);
#ifndef CONFIG_ZEBU
int fdt_fixup_tee_reserved_mem(void *fdt);
#endif

#ifdef CONFIG_NAND_BOOT
int fdt_fixup_mtd(void *fdt);
#ifdef CONFIG_UBI_ATTACH_MTD
int fdt_fixup_ubi_ai(void *fdt);
#endif
#endif

int fdt_fixup_first_mode(void *fdt);
int fdt_fixup_emmc_swcq(void *fdt);
#ifdef DISABLE_UART
int fdt_close_console(void *fdt);
#endif
#ifdef CONFIG_EMMC_WP
int fdt_fixup_protect_part(void *fdt);
#endif
#ifdef CONFIG_SPLASH_SCREEN
int fdt_fixup_lcdid(void *fdt);
int fdt_fixup_lcdbase(void *fdt);
int fdt_fixup_lcdsize(void *fdt);
int fdt_fixup_lcdname(void *fdt);
int fdt_fixup_lcdbpix(void *fdt);
#endif
int fdt_fixup_pixelclock(void *fdt);
#ifdef CONFIG_SPI_SLAVER_PANEL
int fdt_fixup_spi_panel_name(void *fdt);
#endif

#ifdef CONFIG_SENSOR_HUB_LK
int fdt_fixup_sensor_name(void *fdt);
#endif

int fdt_fixup_serialno(void *fdt);
int fdt_fixup_wdten(void *fdt);
int fdt_fixup_dswdten(void *fdt);
int fdt_fixup_dvfs_set(void *fdt);

void boot_adjust_wdt_flag(void);
#ifdef CONFIG_USBPINMUX
int fdt_fixup_usbmux(void *fdt);
#endif

int fdt_fixup_baudrate(void *fdt);

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
int fdt_fixup_oem_repair(void *fdt);
#endif

int fdt_fixup_memleakon(void *fdt);

#ifdef SPRD_SYSDUMP
int fdt_fixup_sysdump_magic(void *fdt);
int fdt_fixup_sysdump_bootloader(void *fdt);
#endif

//int fdt_fixup_dram_training(void *fdt); //remove for this function due to No API Call
void fdt_fixup_chosen_bootargs_board(char *buf, int calibration_mode);
int fdt_fixup_chosen_bootargs_board_private(void *fdt);
void fdt_fixup_secureboot_param(void *fdt_blob);
int fdt_fixup_ddr_size(void *fdt);
int fdt_fixup_ro_boot_ramsize(void *fdt);
int fdt_fixup_ddrsize_range(void *fdt);

#ifdef CONFIG_SANSA_SECBOOT
int fdt_fixup_socid(void *fdt);
#endif

#ifdef CONFIG_BOARD_KERNEL_CMDLINE
int fdt_fixup_board_kernel_cmdline(void *fdt);
int fdt_fixup_board_kernel_cmdline_from_vendorboot(void *fdt);
#endif

int fdt_fixup_dtbo_index(void *fdt);
#if ((defined CONFIG_FOR_LOGLEVEL) || (DEBUG))
int fdt_fixup_loglevel(void *fdt);
#endif
#if DEBUG
int fdt_fixup_selinux_switch(void *fdt);
#endif
int fdt_fixup_cpu_serial_number(void *fdt);
#ifdef CONFIG_ANDROID_AB
int fdt_fixup_add_slot_suffix(void *fdt);
int fdt_fixup_add_force_mode(void *fdt);
#endif
#ifdef CONFIG_TIME_STATISTIC
int fdt_fixup_timeconsuming(u8 *fdt);
#endif
int fdt_fixup_bootcause(void *fdt);
int fdt_fixup_pwroffcause(void *fdt);
int fdt_fixup_charger_parameters(u8 *fdt);
void fdt_fixup_boardid_hwlevel(void *fdt);
int fdt_fixup_bootloader_log_reserved(void *fdt);
int fdt_fixup_startup_core(void *fdt);
int fdt_fixup_vendor_init_flag(void *fdt);
struct dt_entry_t * fdt_get_entry_ptr_by_table(struct dt_table_t *table);

extern phys_size_t get_real_ram_size(void);
extern const char *load_sensor_to_kernel(void);

int fdt_fixup_switch_storage_probe(void *fdt);

#if defined(CONFIG_MMC) && defined(SPRD_EMMC_BOOTDEVICE)
int fdt_fixup_boot_device(void *fdt);
#endif

#if 0
void fdt_fixup_pinctrl_l3(void *fdt);
#endif

int fdt_fixup_sprdboot_device(void *fdt);
#if DEBUG
int fdt_fixup_ptm_reserved(void *fdt);
#else
inline int fdt_fixup_ptm_reserved(void *fdt) {return 0;};
#endif

#endif

