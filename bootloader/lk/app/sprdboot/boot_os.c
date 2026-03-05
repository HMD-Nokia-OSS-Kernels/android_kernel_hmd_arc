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

#include <sprd_common.h>
#include <sprd_common_rw.h>
#include "android_bootimg.h"
#include <boot_mode.h>
#include <boot_parse.h>
#include <string.h>
#include <sprd_log.h>
#include <arch/sprd_cache.h>
#include <secureboot/sec_common.h>
#include <linux/usb/usb_uboot.h>
#include "fastboot.h"
#include <chipram_env.h>
#ifdef CONFIG_SPRD_HW_I2C
#include <sprd_i2c_hwchn.h>
#endif
#ifdef SPRD_TRACE
#include <sprd_trace.h>
#endif
#include <arch/arch_ops.h>

#ifdef CONFIG_VERIFY_GPT
#include <secureboot/sprd_verify.h>
#include <sprd_sizes.h>
#include <lk_sec_drv.h>
#endif

extern uint32_t lk_start_time;
extern void modem_entry(void);
extern char *bootcause_cmdline;
extern char *pwroffcause_cmdline;
extern phys_size_t real_ram_size;

uint32_t lk_end_time;
#ifdef CONFIG_ARM
#ifndef machine_arch_type
#define machine_arch_type	0xF
#endif
static void start_linux(uchar *dt_addr)
{
	void (*theKernel) (int zero, int arch, u32 params);
	theKernel = (void (*)(void *, int, int, int))KERNEL_ADR;

#if ARM_WITH_MMU
	arch_disable_mmu();
#endif

	/*start modem CP */
#ifndef CONFIG_ZEBU
	modem_entry();
#endif

#ifdef CONFIG_MINI_TRUSTZONE
	trustzone_entry(TRUSTZONE_ADR + 0x200);
#endif

#ifdef SPRD_SECBOOT
	secboot_unlock_display();
#endif

	dprintf(CRITICAL, "[%s] start linux at PC:0x%lx, dt_addr:%p\n", __func__, KERNEL_ADR, dt_addr);
	lk_end_time = SCI_GetTickCount();
	dprintf(CRITICAL, "lk consume time: %dms\n", lk_end_time - lk_start_time);

#ifdef CONFIG_TIME_STATISTIC
	/* append to bootargs */
	if (fdt_fixup_timeconsuming(dt_addr)) {
		errorf("failed to append timeconsuming to bootargs\n");
	}
#endif

	/* the last time to write log */
	write_log_before_entry_kernel();

	if (real_ram_size > SZ_2G)
		flush_dcache_range(CONFIG_SYS_SDRAM_BASE, CONFIG_SYS_SDRAM_BASE + ((u32)SZ_2G - (u32)SZ_4K));
	else
		flush_dcache_range(CONFIG_SYS_SDRAM_BASE, CONFIG_SYS_SDRAM_BASE + real_ram_size);

	/* disable cache before jump into kernel*/
	cleanup_cache_environment();
	/* jump to kernel with register set */
	theKernel(0, machine_arch_type, (u32)dt_addr);

	while (1) ;
}
#elif defined(CONFIG_ARM64)
static void start_linux(uchar *dt_addr)
{
	void (*theKernel) (void *dtb_addr, int zero, int arch, int reserved);
	theKernel = (void (*)(void *, int, int, int))KERNEL_ADR;

#ifndef CONFIG_ZEBU
	/*start modem CP */
	modem_entry();
#endif

#ifdef CONFIG_MINI_TRUSTZONE
	trustzone_entry(TRUSTZONE_ADR + 0x200);
#endif

#ifdef SPRD_SECBOOT
	secboot_unlock_display();
#endif

	/*kernel must run in el2, so here switch to el2 */
	//armv8_switch_to_el2();
	dprintf(CRITICAL, "[%s] start linux at PC:0x%lx, dt_addr:%p\n", __func__, KERNEL_ADR, dt_addr);
	lk_end_time = SCI_GetTickCount();
	dprintf(CRITICAL, "lk consume time: %dms\n", lk_end_time - lk_start_time);

#ifdef CONFIG_TIME_STATISTIC
	/* append to bootargs */
	if (fdt_fixup_timeconsuming(dt_addr)) {
		errorf("failed to append timeconsuming to bootargs\n");
	}
#endif

	/* the last time to write log */
	write_log_before_entry_kernel();
#ifdef SPRD_TRACE
	trace_save();
#endif

	/*before switch to el2,flush all cache */
	/*FIXME: cleanup_cache_environment() will cause panic here, we need to find the solution*/
	if (real_ram_size > SZ_2G)
		flush_dcache_range(CONFIG_SYS_SDRAM_BASE, CONFIG_SYS_SDRAM_BASE + SZ_2G);
	else
		flush_dcache_range(CONFIG_SYS_SDRAM_BASE, CONFIG_SYS_SDRAM_BASE + real_ram_size);
	FTL_Panic_Flag_Write(0, LOG_RESERVED_ADDR);
	/* disable cache before jump into kernel*/
	cleanup_cache_environment();

	theKernel(dt_addr, 0, 0, 0);

	/*never enter here*/
	while (1) ;
}
#endif

#ifdef CONFIG_USBPINMUX
extern void usb_jtag_key_config(void);
extern int read_mux_cfg_flag(void);
#endif

static void vlx_entry(uchar *dt_addr)
{
#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
	/* clear reboot-edl flag if necessary */
	if (!fb_check_reboot_edl(NULL)) {
		(void)fb_require_reboot_edl(0);
	}
#endif

	const char *bootmode = g_env_bootmode;
	dprintf(CRITICAL,"enter mode %s, boot_reason: %s, pwroff_reason: %s\n",
		!bootmode ? "normal" : bootmode,
		!bootcause_cmdline ? "Bootcause hasn't been set yet" : bootcause_cmdline,
		!pwroffcause_cmdline ? "pwroffcause hasn't been set yet" : pwroffcause_cmdline);

#ifdef DFS_ON_ARM7
	cp_dfs_param_for_arm7();
#endif

	/*shutdown usb ldo, can't shutdown it in the ldo_sleep.c because download mode must use usb */
	/*if USB opens Pin mux config in chipram, can't shutdown usb ldo */
#ifdef CONFIG_USBPINMUX
	/* only for SS customer */
	//usb_jtag_key_config();

	if (!read_mux_cfg_flag()) {
		usb_driver_exit();
		dprintf(INFO,"usb_pin_mux config is closed!\n");
	}
#else
	usb_driver_exit();
#endif

	// smp kick all cpus if needed
	start_linux(dt_addr);
}

extern u64 dram_error_addr;
extern void lcd_printf(const char *fmt, ...);
extern int load_sp_boot_code(void);
void vlx_boot(const char *kernel_pname, int backlight_set, int lcd_on)
{
	boot_img_hdr *hdr = (void *)raw_header;
	int ret = 0;
	uchar *dt_addr = (uchar *)DT_ADR;

#ifdef CONFIG_VERIFY_GPT
	uint8_t *gpt_verify_data;
	uint8_t *gpt_entry_data;
	int ret = 0;
	u32 gpt_enable = 0;

	gpt_enable = !check_gpt_efuse();
	if (gpt_enable) {
		gpt_verify_data = memalign(SZ_4K, GPT_DATA_SIZE);
		if (gpt_verify_data == NULL) {
			errorf("malloc gpt_verify_data error!\n");
			return -1;
		}
		memset(gpt_verify_data, 0, GPT_DATA_SIZE);

		gpt_entry_data = memalign(SZ_4K, SZ_E);
		if (gpt_entry_data == NULL) {
			errorf("malloc gpt_entry_data error!\n");
			return -1;
		}
		memset(gpt_entry_data, 0, SZ_E);
	}
#endif

	write_log();
#ifndef CONFIG_FPGA
	FTL_Savepoint_Private(PHASE_LCD_INIT);
	//1. sprd preload doing...
	sprd_set_preload(lcd_on, backlight_set);
#endif

#if defined(SPRD_SECBOOT)
	char partition[16] = {0};
	debugf("SECUREBOOT_ENABLE\n");
	if (0 == memcmp(kernel_pname, RECOVERY_PART, strlen(RECOVERY_PART))) {
#ifdef CONFIG_ANDROID_AB
		strcpy(partition, "boot");
#else
		strcpy(partition, "recovery");
#endif
	} else {
		strcpy(partition, "boot");
	}

#if defined (SPRD_SECBOOT)
	FTL_Savepoint_Private(PHASE_SECURE_INIT);
	/***secboot only 3 steps***/
	/***secboot 1st step***/
	secboot_init(partition);

#ifdef CONFIG_VERIFY_GPT
	if (gpt_enable) {
		ret = get_gpt_data(gpt_verify_data, 0);
		if (ret != 0) {
			errorf("get gpt data error! giveup boot!\n");
			return;
		}

		ret = sprd_secure_process_flow("splloader", gpt_verify_data, NULL);
		if (ret == 0) {
			dprintf(INFO,"verify gpt entry success!\n");
			free(gpt_entry_data);
			free(gpt_verify_data);
		} else {
			dprintf(INFO,"verify gpt entry fail, try to verify backup entry!\n");
			get_gpt_data(gpt_verify_data, 1);
			ret = sprd_secure_process_flow("splloader", gpt_verify_data, NULL);
			if (ret == 0) {
				dprintf(INFO,"verify backup gpt entry success, continue!\n");
				ret = process_gpt_entry(gpt_entry_data, SZ_E, 0, 1);
				ret = process_gpt_entry(gpt_entry_data, SZ_E, 1, 0);
				if (ret != 0) {
					errorf("recovery primary gpt entry error!\n");
					free(gpt_entry_data);
					free(gpt_verify_data);
					fail_and_enter_fastboot_mode();
				}
				free(gpt_entry_data);
				free(gpt_verify_data);
			} else {
				errorf("verify error! givveup boot!\n");
				free(gpt_entry_data);
				free(gpt_verify_data);
				fail_and_enter_fastboot_mode();
			}
		}
	} else {
		dprintf(INFO, "GPT efuse bit not enable, skip verify gpt!\n");
	}
#endif

	/***secboot 2nd step***/
	/*set v-boot binding data eg:lock status*/
	loader_binding_data_set();

	vboot_secure_process_flow(partition);
#endif
#endif

#ifndef CONFIG_ZEBU
	//2. gsm/wcdma/td/lte/5g image loading....
	load_require_image();
#endif

#if defined (SPRD_SECBOOT)
	secboot_secure_process_flow(partition,0,0,(char *)VERIFY_BASE);
	//things to do refer to verify ret
	take_action_with_vbootret();
	take_action_with_dmverity_ret();

	if(set_root_of_trust(root_of_trust_str, ROOT_OF_TRUST_MAXSIZE)) {
		errorf("set_root_of_trust failed.\n");
	} else {
		dprintf(INFO,"set_root_of_trust succeeded.\n");
	}
#endif

	//4. kernel & ramdisk & dtb loading...
	FTL_Savepoint_Private(PHASE_KERNEL_LOAD);
	ret = load_kernel_ramdisk(kernel_pname, hdr, dt_addr, NULL);
	if (-1 == ret) {
		panic("the load_kernel_ramdisk ret is error!!!!\n");
		return;
	}

#ifdef PROJECT_SEC_CM4
	if(g_charger_mode) {
		const boot_image_required_t pm_image = {
			"pm_sys", "", (uint64_t)DFS_SIZE, (char *)DFS_ADDR};
		/* load sp ddr boot */
#ifdef CONFIG_SP_DDR_BOOT
		load_sp_boot_code();
#endif
		vboot_secure_process_flow_cm4((char *)pm_image.partition);
	}
#endif

#ifndef CONFIG_ZEBU
	//sprd postload doing...
	sprd_set_postload();
#endif

#if defined (SPRD_SECBOOT)
	/***secboot 3rd step***/
	secboot_terminal();
#endif

#ifdef  CONFIG_SPRD_HW_I2C
	i2c_dvfs_hwchn_init();
#endif
	if (dram_error_addr != BIST_PASS_KEY) {
		lcd_printf("ddr bist fail\n");
		lcd_printf("fail addr: 0x%llx", dram_error_addr);
		while(1);
	}
	vlx_entry(dt_addr);
}
