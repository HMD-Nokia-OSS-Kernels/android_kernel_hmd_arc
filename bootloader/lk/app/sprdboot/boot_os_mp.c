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
#include <sprd_cache.h>
#include <secureboot/sec_common.h>
#include <linux/usb/usb_uboot.h>
#include <kernel/thread.h>
#include <uboot_avb_ops.h>
#include <linux/kernel.h>
#include <android_ab.h>
#include "fastboot.h"
#include <chipram_env.h>
#ifdef CONFIG_BOOTCONFIG
#include <sprd_bootconfig_support.h>
#endif
#ifdef CONFIG_SPRD_HW_I2C
#include <sprd_i2c_hwchn.h>
#endif
#ifdef SPRD_TRACE
#include <sprd_trace.h>
#endif

#ifdef CONFIG_VERIFY_GPT
#include <secureboot/sprd_verify.h>
#include <sprd_sizes.h>
#include <lk_sec_drv.h>
#endif

extern uint32_t lk_start_time;
extern void modem_entry(void);
extern char *bootcause_cmdline;
extern char *pwroffcause_cmdline;
extern char *g_ramdisk_addr;
extern boot_img_info_t s_bootimg_info;
extern phys_size_t real_ram_size;
extern uchar *add_bootconfig_trailer(uchar *ramdisk_addr);
#ifdef CONFIG_UFS
extern boot_device_t get_bootdevice(void);
extern long ufs_ssu(void);
#endif
uint32_t lk_end_time;

#ifdef CONFIG_SP_DDR_BOOT
extern int load_sp_boot_code(void);
#endif

thread_t *sprd_threads[5];
extern volatile u32 cpu_num;
spin_lock_t thread_created_lock = 1;
volatile u32 preload_done_flag = 0;
volatile u32 lcd_done_flag = 0;
static int secure_vbmeta_verify(void *arg);
static int secure_sign_verify(void *arg);
static int sprd_lcd(void *arg);
static int kernel_dtb_parse(void *arg);
static int sprd_preload(void *arg);
static int sprd_self_monitor(void *arg);
static int pre_kernel_flag;

static struct lcd_status {
	int blacklight;
	int status;
} lcd;

extern void smp_boot(void);
extern int psci_call(ulong arg0, ulong arg1, ulong arg2, ulong arg3);
void secondary_cpu_poweron(void)
{
	for (int cpuid = 1; cpuid < SMP_MAX_CPUS; cpuid++) {
		dprintf(ALWAYS, "call smc to power on core%d\n", cpuid);
		psci_call(PSCI_CPU_ON_AARCH64, cpuid << 8, (unsigned long)smp_boot, 0);
	}
}

static void charge_mode_bootsp(void)
{
#ifdef PROJECT_SEC_CM4
	if(g_charger_mode) {
		const boot_image_required_t pm_image = {
			"pm_sys", "", (uint64_t)DFS_SIZE, (char *)DFS_ADDR};
		/* load sp ddr boot */
#ifdef CONFIG_SP_DDR_BOOT
		load_sp_boot_code();
#endif
		/***secboot 2nd step***/
		vboot_secure_process_flow_cm4((char *)pm_image.partition);
	}
#endif
}

void secondary_task_create(const char* kernel_pname)
{
	dprintf(ALWAYS, "creating secondary task\n");

	sprd_threads[0] = thread_create("sprd_preload", &sprd_preload, NULL, HIGH_PRIORITY, DEFAULT_STACK_SIZE);
	sprd_threads[0]->pinned_cpu = 0;

	sprd_threads[1] = thread_create("sprd_lcd", &sprd_lcd, NULL, HIGH_PRIORITY, DEFAULT_STACK_SIZE);
	sprd_threads[1]->pinned_cpu = 1;

	sprd_threads[2] = thread_create("self_monitor", &sprd_self_monitor, NULL, HIGH_PRIORITY, DEFAULT_STACK_SIZE);
	sprd_threads[2]->pinned_cpu = 2;

	sprd_threads[3] = thread_create("secure_sign_verify", &secure_sign_verify, (void*)kernel_pname, HIGH_PRIORITY, DEFAULT_STACK_SIZE);
	sprd_threads[3]->pinned_cpu = 0;

	sprd_threads[4] = thread_create("kernel_dtb_parse", &kernel_dtb_parse, (void*)kernel_pname, HIGH_PRIORITY, DEFAULT_STACK_SIZE);
	sprd_threads[4]->pinned_cpu = 1;

	for(int i = 0; i < ARRAY_SIZE(sprd_threads); i++)
		thread_detach(sprd_threads[i]);

	spin_unlock(&thread_created_lock);
	thread_resume(sprd_threads[0]); //yield core0 to process "sprd_preload" thread
}

u64 bootimg_load_flag = 0;
u64 vendorbootimg_load_flag = 0;
static int sprd_preload(void *arg)
{
	int func_start = SCI_GetTickCount();
	int cpu = arch_curr_cpu_num();

	uint64_t partition_size=0;
	int ret;
	char partition_name[20];

	FTL_Savepoint_Private(PHASE_THRD0_PRELOAD);
	dprintf(INFO, "[lk core%d] enter %s\n", cpu, __func__);
	get_slot_ab(partition_name, "boot");
	ret = get_img_partition_size(partition_name, &partition_size);
	if (ret < 0) {
		debugf("secure_get_partition_size load boot error!\n");
		goto error;
	}
	if (0 != common_raw_read(partition_name, partition_size, (uint64_t)0, (char *)VBOOT_VERIFY_BUF_ADDR)) {
		errorf("load boot.img fail\n");
		goto error;
	}
	dprintf(ALWAYS, "load boot.img succeed!!\n");
	bootimg_load_flag = 1;

	get_slot_ab(partition_name, "vendor_boot");
	ret = get_img_partition_size(partition_name, &partition_size);
	if (ret < 0) {
		debugf("secure_get_partition_size load boot error!\n");
		goto error;
	}
	if (0 != common_raw_read(partition_name, partition_size, (uint64_t)0, (char *)(VBOOT_VERIFY_BUF_ADDR+100*1024*1024))) {
		errorf("load vendor_boot.img fail\n");
		goto error;
	}
	dprintf(ALWAYS, "load vendor_boot.img succeed!!\n");
	vendorbootimg_load_flag = 1;
	dprintf(INFO, "%s consume %dms\n", __func__, SCI_GetTickCount() - func_start);

	preload_done_flag = 1; //let core1 process "kernel_dtb_parse" thread
	thread_resume(sprd_threads[3]); //yield core0 to process "secure_sign_verify" thread
	return 0;

error:
	panic("sprd_preload thread is error, panic...\n");
	return -1;
}

static int sprd_lcd(void *arg)
{
	int func_start = SCI_GetTickCount();
	int cpu = arch_curr_cpu_num();

	FTL_Savepoint_Private(PHASE_THRD1_LCD);
	dprintf(INFO, "[lk core%d] enter %s\n", cpu, __func__);
#ifndef CONFIG_ZEBU
	sprd_set_preload(lcd.status, lcd.blacklight);
#endif
	dprintf(INFO, "%s consume %dms\n", __func__, SCI_GetTickCount() - func_start);
	lcd_done_flag = 1; // can let core0 use lcd_printf when unlock

	while(!preload_done_flag); // wait for preload ready
	thread_resume(sprd_threads[4]); //yield core1 to process "kernel_dtb_parse" thread
	return 0;
}

static int kernel_dtb_parse(void *arg)
{
	int ret = 0;
	int func_start = SCI_GetTickCount();
	int cpu = arch_curr_cpu_num();

	FTL_Savepoint_Private(PHASE_THRD3_KERNEL);
	dprintf(INFO, "[lk core%d] enter %s\n", cpu, __func__);
	boot_img_hdr *hdr = (void *)raw_header;

	ret = load_kernel_ramdisk((const char *)arg, hdr, (uchar *)DT_ADR, NULL);
	if(ret) {
		panic("kernel_dtb_parse thread is error, panic...\n");
		return ret;
	}
	dprintf(INFO, "%s consume %dms\n", __func__, SCI_GetTickCount() - func_start);

	return 0;
}

static int secure_sign_verify(void *arg)
{
	int func_start = SCI_GetTickCount();
	int cpu = arch_curr_cpu_num();

	FTL_Savepoint_Private(PHASE_THRD2_SECBOOT);

#ifdef CONFIG_VERIFY_GPT
	uint8_t *gpt_verify_data;
	uint8_t *gpt_entry_data;
	int ret = 0;
	u32 gpt_enable = 0;

	gpt_enable = !check_gpt_efuse();
	if (gpt_enable) {
		gpt_verify_data = memalign(SZ_4K, GPT_DATA_SIZE);
		if(gpt_verify_data == NULL) {
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

	dprintf(INFO, "[lk core%d] enter %s\n", cpu, __func__);

#ifdef SPRD_SECBOOT
	char boot_partition[16] = {0};
	debugf("SECUREBOOT_ENABLE\n");

	if (0 == memcmp((const char *)arg, RECOVERY_PART, strlen(RECOVERY_PART))) {
#ifdef CONFIG_ANDROID_AB
		strcpy(boot_partition, "boot");
#else
		strcpy(boot_partition, "recovery");
#endif
	} else {
		strcpy(boot_partition, "boot");
	}

	/***secboot only 3 steps***/
	/***secboot 1st step***/
	secboot_init(boot_partition);

#ifdef CONFIG_VERIFY_GPT
	if (gpt_enable) {
		ret = get_gpt_data(gpt_verify_data, 0);
		if (ret != 0) {
			errorf("get gpt data error! give up boot!\n");
			return -1;
		}
		ret = sprd_secure_process_flow("splloader", gpt_verify_data, NULL);
		if (ret == 0) {
			dprintf(INFO,"verify gpt entry success!\n");
			free(gpt_verify_data);
			free(gpt_entry_data);
		} else {
			dprintf(INFO,"verify gpt entry fail, try to verify backup entry!\n");
			get_gpt_data(gpt_verify_data, 1);
			ret = sprd_secure_process_flow("splloader", gpt_verify_data, NULL);
			if (ret == 0) {
				dprintf(INFO,"verify gpt entry success!\n");
				free(gpt_verify_data);
				free(gpt_entry_data);
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
					dprintf(INFO,"verify error! giveup boot!\n");
					free(gpt_entry_data);
					free(gpt_verify_data);
					fail_and_enter_fastboot_mode();
				}
			}
		}
	} else {
		dprintf(INFO, "GPT efuse bit not enable, skip verify gpt!\n");
	}
#endif

	/***secboot 2nd step***/
	/*set v-boot binding data eg:lock status*/
	loader_binding_data_set();

	vboot_secure_process_flow(boot_partition);
#endif

	load_require_image();

#ifdef SPRD_SECBOOT
	secboot_secure_process_flow(boot_partition,0,0,(char *)VERIFY_BASE);
	//things to do refer to verify ret
	take_action_with_vbootret();
	take_action_with_dmverity_ret();

	if(set_root_of_trust(root_of_trust_str, ROOT_OF_TRUST_MAXSIZE)) {
		errorf("set_root_of_trust failed.\n");
	} else {
		dprintf(INFO,"set_root_of_trust succeeded.\n");
	}
#endif

#ifndef CONFIG_ZEBU
	//sprd postload doing...
	sprd_set_postload();
#endif

#ifdef SPRD_SECBOOT
	/***secboot 3rd step***/
	secboot_terminal();
#endif

	dprintf(INFO, "%s consume %dms\n", __func__, SCI_GetTickCount() - func_start);

	return 0;
}

static int lk_mp_final_fixup(void *fdt, void *ramdisk)
{
#ifndef CONFIG_BOOTCONFIG
	/*for verified boot*/
	fdt_fixup_verified_boot(fdt);
	fdt_fixup_flash_lock_state(fdt);
#ifdef SPRD_VBOOT_V2
	fdt_fixup_vboot(fdt);
#endif
	fdt_print_bootargs(fdt);
#else //CONFIG_BOOTCONFIG
	/*for verified boot*/
	bootconfig_fixup_verified_boot(ramdisk);
	bootconfig_fixup_flash_lock_state(ramdisk);
#ifdef SPRD_VBOOT_V2
	bootconfig_fixup_vboot(ramdisk);
#endif
	s_bootimg_info.vendor_bootconfig_size = get_bootconfig_len();
	print_bootconfig_param(ramdisk);
	fdt_print_bootargs(fdt);
#endif
	ramdisk = add_bootconfig_trailer(ramdisk);
	fdt_initrd_norsvmem(fdt, (ulong)RAMDISK_ADR, (ulong)ramdisk, 1);
#ifdef CONFIG_SHOW_DEBUG_CRC
	show_crc_key_string((const void*)RAMDISK_ADR, ramdisk - RAMDISK_ADR);
#endif

	return 0;
}

static int sprd_self_monitor(void *arg)
{
	int cpu = arch_curr_cpu_num();
	int i = 0;

	dprintf(INFO, "[lk core%d] enter %s\n", cpu, __func__);

	while(1) {
#ifdef CONFIG_ENABLE_LOGPOINT
		if (g_last_step & (1 << PHASE_PRE_KERNEL))
#else
		if (pre_kernel_flag)
#endif
			return 0;

		if(i > 18000)
			panic("monitor the lk error\n");

		mdelay(10);
		i++;
	}
}

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

	dprintf(ALWAYS, "waif for core1 process kernel done and powerdown\n");
	while(cpu_num != 2);

	FTL_Savepoint_Private(PHASE_PRE_KERNEL);
	pre_kernel_flag = 1;
	lk_mp_final_fixup(dt_addr, g_ramdisk_addr);
	charge_mode_bootsp();

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

#ifdef CONFIG_UFS
	if (get_bootdevice() == BOOT_DEVICE_UFS)
		ufs_ssu();
#endif

	dprintf(ALWAYS, "waif for all mp powerdown\n");

	while(cpu_num != 1);
	dprintf(ALWAYS, "jump to kernel\n");
	/*before switch to el2,flush all cache */
	/*FIXME: cleanup_cache_environment() will cause panic here, we need to find the solution*/
	if (real_ram_size > SZ_2G)
		flush_dcache_range(CONFIG_SYS_SDRAM_BASE, CONFIG_SYS_SDRAM_BASE + SZ_2G);
	else
		flush_dcache_range(CONFIG_SYS_SDRAM_BASE, CONFIG_SYS_SDRAM_BASE + real_ram_size);
	FTL_Panic_Flag_Write(0, LOG_RESERVED_ADDR);
	FTL_Savepoint_Private(PHASE_ENTER_KERNEL);
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
void vlx_boot(const char *kernel_pname, int backlight_set, int lcd_on)
{
	lcd.blacklight = backlight_set;
	lcd.status = lcd_on;

	write_log();
	secondary_cpu_poweron();
	dprintf(ALWAYS, "secondary cpu poweron!\n");
	secondary_task_create(kernel_pname);

#ifdef  CONFIG_SPRD_HW_I2C
	i2c_dvfs_hwchn_init();
#endif
	if (dram_error_addr != BIST_PASS_KEY) {
		lcd_printf("ddr bist fail\n");
		lcd_printf("fail addr: 0x%llx", dram_error_addr);
		while(1);
	}
	vlx_entry((uchar *)DT_ADR);
}
