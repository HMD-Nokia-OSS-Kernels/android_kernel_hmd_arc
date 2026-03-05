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

#include "boot_mode.h"
#include <lk/debug.h>
#include <sprd_common.h>
#include <dl_common.h>
#include <sprd_wdt.h>
#include <malloc.h>
#include <android_bootimg.h>
#include <sprd_common_rw.h>
#include <boot_parse.h>
#include <logo_bin.h>
#include <secureboot/sec_common.h>
#include <asm/arch/check_reboot.h>

#ifdef ZCFG_HMD_ENTERPRISE_API
//[HMDEnterpriseService] begin 2024-09-04
#include <sprd_common_rw.h>
#include <enterprise_api_info.h>
//[HMDEnterpriseService] end 2024-09-04
#endif

extern int do_fastboot(void);
extern void lcd_printf(const char *fmt, ...);
extern void lcd_printf_xy(struct sprd_font_info *font_info, const char *fmt, ...);
void lcd_enable(void);
extern void fastboot_lcd_printf(void);
#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
extern int fb_check_reboot_edl(void *ptr);
#endif

const char* g_env_bootmode = "normal";
unsigned int g_charger_mode = 0;
uint64_t backstage_query_size = 0;
extern char backtrace[512];
extern volatile int dump_backtrace_once;

void __attribute__((weak)) fastboot_lcd_printf(void)
{
	lcd_printf("   fastboot mode\n");
#ifdef CONFIG_ENABLE_LOGPOINT
	if(dump_backtrace_once && strlen(backtrace))
		lcd_printf("lk_panic:\n%s\n", backtrace);
#endif
	return;
}

#ifdef ZCFG_HMD_ENTERPRISE_API
//[HMDEnterpriseService] add block factory reset api begin 2024-09-04 
int is_block_factory_reset(void)
{
    char block_factory_reset[1];
    char isBlocked[1] = "1";
    if (0 != common_raw_read(ENTERPRISEAPIINFO_PARTITION_NAME, (uint64_t)1, BLOCK_FACTORY_RESET_OFFSET, block_factory_reset)) {
        errorf("read error\n");
        return 0;
    } else{
        debugf("block_factory_reset=%s", block_factory_reset);
        if (!memcmp(isBlocked, block_factory_reset, sizeof(block_factory_reset))) {
            errorf("factory reset is blocked!");
            return 1;
        }
    }
    errorf("factory reset is not blocked!");
    return 0;
}
//[HMDEnterpriseService] add block factory reset api end 2024-09-04 
//[HMDEnterpriseService] add block download mode api begin 2024-09-04 
int is_block_download_mode(void)
{
    char block_download_mode[1];
    char isBlocked[1] = "1";
    if (0 != common_raw_read(ENTERPRISEAPIINFO_PARTITION_NAME, (uint64_t)1, BLOCK_DOWNLOAD_MODE_OFFSET, block_download_mode)) {
        errorf("read error\n");
        return 0;
    } else{
        debugf("block_download_mode=%s", block_download_mode);
        if (!memcmp(isBlocked, block_download_mode, sizeof(block_download_mode))) {
            errorf("download mode is blocked!");
            return 1;
        }
    }
    errorf("download mode is not blocked!");
    return 0;
}
//[HMDEnterpriseService] add block download mode api end 2024-09-04 
//[HMDEnterpriseService] for error code showing begin 2025-03-04 
#define ERROR_CODE_NUM 7

int is_hmd_error_code_set(char* ptrErr)
{
    char error_code[ERROR_CODE_LEN];
    char *errorcode[ERROR_CODE_NUM] = {
        "SSE-000",
        "SSE-001",
        "SSE-002",
        "SSE-003",
        "SSE-004",
        "SSE-005",
        "SSE-006",
    };
    if (0 != common_raw_read(ENTERPRISEAPIINFO_PARTITION_NAME, (uint64_t)ERROR_CODE_LEN, ENTERPRISE_ERROR_CODE_OFFSET, error_code)) {
        errorf("read error\n");
        return 0;
    } else{
        debugf("error_code=%s", error_code);
        for(int i=0;i<ERROR_CODE_NUM;i++){
            if (!memcmp(error_code, errorcode[i], ERROR_CODE_LEN)) {
                errorf("error code is set!");
                if(ptrErr != NULL)
                    memcpy(ptrErr,error_code,ERROR_CODE_LEN);
                return 1;
            }
        }

    }
    errorf("error code not set!");
    return 0;
}
//[HMDEnterpriseService] for error code showing end 2025-03-04 
#endif

#ifdef CONFIG_AUTOLOAD_MODE
void autoload_mode(void)
{
	struct boot_img_hdr *boot_hdr;
	ulong kernel_size = 0;
	uchar * ramdisk_addr = 0;
	ulong ramdisk_size = 0;
	uchar * dt_addr = 0;
	ulong dt_size = 0;
	struct dt_table_t *table = NULL;
	uint64_t size = 0;
	struct dt_entry_t *dt_entry_ptr = NULL;
	uint64_t offset = 0;
	debugf("autoload_mode\n");

#if !DEBUG
	stop_watchdog();
#endif

	boot_hdr = malloc(512*4);
	if (boot_hdr == NULL) {
		errorf("autoload_mode malloc failed!\n");
		return;
	}

	memcpy((void *)boot_hdr,(const void *)BOOTIMG_ADR,512*4);
	if (!memcmp(boot_hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
		kernel_size = (boot_hdr->kernel_size + (KERNL_PAGE_SIZE-1)) & (~(KERNL_PAGE_SIZE -1));
		ramdisk_addr =  (void *)BOOTIMG_ADR+512*4+kernel_size;
		ramdisk_size = (boot_hdr->ramdisk_size + (KERNL_PAGE_SIZE-1)) & (~(KERNL_PAGE_SIZE -1));
		dt_addr = (void *)BOOTIMG_ADR+512*4+kernel_size+ramdisk_size;
		dt_size = (boot_hdr->dt_size + (KERNL_PAGE_SIZE-1)) & (~(KERNL_PAGE_SIZE -1));
		memcpy((void *)KERNEL_ADR, (const void *)BOOTIMG_ADR+512*4, kernel_size);
		memcpy((void *)RAMDISK_ADR, ramdisk_addr, ramdisk_size);

		table = (struct dt_table_t*)dt_addr;
		/* Validate the device tree table header */
		if((table->magic != SPRD_DT_MAGIC) || (table->version != SPRD_DT_VERSION)) {
			errorf("Cannot validate Device Tree Table \n");
		}
		/* Calculate the offset of device tree within device tree table */
		dt_entry_ptr = fdt_get_entry_ptr_by_table(table);
		if(NULL == dt_entry_ptr) {
			errorf("Getting device tree address failed\n");
		}

		memcpy((void *)DT_ADR, dt_addr+dt_entry_ptr->dt_offset, dt_entry_ptr->dt_size);

		debugf("\nthe kernel is loading in the address:0x%x,size is 0x%lx\n", KERNEL_ADR, kernel_size);
		debugf("\nthe ramdisk is loading in the address:0x%x,size is 0x%lx\n", RAMDISK_ADR, ramdisk_size);
		debugf("\nthe dtb is loading in the address:0x%x,size is 0x%lx\n", DT_ADR, dt_entry_ptr->dt_size);
	}
	fdt_initrd_norsvmem(DT_ADR, RAMDISK_ADR, RAMDISK_ADR + boot_hdr->ramdisk_size, 1);
	free(boot_hdr);

	vlx_entry(DT_ADR);

}

#endif

void normal_mode(void)
{
//[HMDEnterpriseService] for error code showing begin 2025-03-04 
#ifdef ZCFG_HMD_ENTERPRISE_API
    if(is_hmd_error_code_set(NULL)){
        fastboot_mode();
	    dprintf(ALWAYS, "FASTBOOT_mode!\n");
        return;
    }
#endif
//[HMDEnterpriseService] for error code showing end 2025-03-04 
        
	FTL_Savepoint_Private(PHASE_NORMAL_MODE);
#ifndef CONFIG_ZEBU
	vibrator_hw_init();
	set_vibrator(1);
#endif
	char flag[1] = {0};
	if (common_raw_write("miscdata", (uint64_t)MISCDATA_HMD_FASTBOOT_SALE_DOWNLOAD_MODE_DATA_LEN, (uint64_t)0, MISCDATA_HMD_FASTBOOT_SALE_DOWNLOAD_MODE,flag)) {
			errorf("write miscdata data %d fail on offset %lld.\n", MISCDATA_HMD_FASTBOOT_SALE_DOWNLOAD_MODE_DATA_LEN, MISCDATA_HMD_FASTBOOT_SALE_DOWNLOAD_MODE);
	}
	vlx_boot(BOOT_PART, BACKLIGHT_ON, LCD_ON);
	return;
}

void download_mode(void)
{
	FTL_Savepoint_Private(PHASE_DOWNLOAD_MODE);
	dprintf(ALWAYS, "download mode!\n");
	stop_watchdog();
	g_env_bootmode = "download";
	write_log();
	do_download();
}

void calibration_mode(void)
{
	FTL_Savepoint_Private(PHASE_CALIBRATION_MODE);
	dprintf(ALWAYS, "calibration_mode\n");
	g_env_bootmode = "cali";
	stop_watchdog();
#ifdef SPRD_CALIMODE_USE_BOOTIMG
	vlx_boot(BOOT_PART, BACKLIGHT_OFF, LCD_OFF);
#else
	vlx_boot(RECOVERY_PART, BACKLIGHT_OFF, LCD_OFF);
#endif
	return;
}

void autotest_mode(void)
{
	dprintf(ALWAYS, "autotest_mode\n");
	stop_watchdog();
	g_env_bootmode = "autotest";
	vlx_boot(BOOT_PART, BACKLIGHT_OFF, LCD_ON);
	return;
}

void recovery_mode(void)
{
	FTL_Savepoint_Private(PHASE_RECOVERY_MODE);
	dprintf(ALWAYS, "recovery_mode\n");

#ifdef ZCFG_HMD_ENTERPRISE_API
	//[HMDEnterpriseService] add block factory reset api begin 2024-09-04 
	if (is_block_factory_reset()) {
	    printf("factory reset was block, enter normal\n");
	    normal_mode();
	    return;
	}
	//[HMDEnterpriseService] add block factory reset api end 2024-09-04 
	//[HMDEnterpriseService] add block download mode api begin 2024-09-04 
	if (is_block_download_mode()) {
	    printf("download mode was block, enter normal\n");
	    normal_mode();
	    return;
	}
	//[HMDEnterpriseService] add block download mode api end 2024-09-04 
#endif

	stop_watchdog();
	g_env_bootmode = "recovery";
#ifdef CONFIG_SC27XX_VDDVIB_VSD2
	vibrator_hw_init();
	set_vibrator(1);
#endif
	vlx_boot(RECOVERY_PART, BACKLIGHT_ON, LCD_ON);
	return;
}


void special_mode(void)
{
	dprintf(ALWAYS, "special_mode\n");
	stop_watchdog();
	g_env_bootmode = "special";
	vlx_boot(BOOT_PART, BACKLIGHT_OFF, LCD_ON);
	return;
}

void iq_mode(void)
{
	dprintf(ALWAYS, "iq_mode\n");
	stop_watchdog();
	g_env_bootmode = "iq";
	vlx_boot(BOOT_PART, BACKLIGHT_OFF, LCD_ON);
	return;
}

void watchdog_mode(void)
{
	dprintf(ALWAYS, "watchdog_mode\n");
	g_env_bootmode = "wdgreboot";
	vlx_boot(BOOT_PART, BACKLIGHT_OFF, LCD_ON);
	return;
}

void ap_watchdog_mode(void)
{
	dprintf(ALWAYS, "ap_watchdog_mode\n");
	g_env_bootmode = "apwdgreboot";
	vlx_boot(BOOT_PART, BACKLIGHT_OFF, LCD_ON);
	return;
}

void unknow_reboot_mode(void)
{
	dprintf(ALWAYS, "unknow_reboot_mode\n");
	stop_watchdog();
	g_env_bootmode = "unknowreboot";
	vlx_boot(BOOT_PART, BACKLIGHT_OFF, LCD_ON);
	return;
}

void abnormal_reboot_mode(void)
{
	dprintf(ALWAYS, "undefine_reboot_mode\n");
	g_env_bootmode = "abnormalreboot";
	vlx_boot(BOOT_PART, BACKLIGHT_OFF, LCD_ON);
	return;
}

void panic_reboot_mode(void)
{
	dprintf(ALWAYS, "enter [%s]\n", __func__);
	g_env_bootmode = "panic";
	vlx_boot(BOOT_PART, BACKLIGHT_OFF, LCD_ON);
	return;
}

void bootloader_panic_reboot_mode(void)
{
	dprintf(ALWAYS, "enter [%s]\n", __func__);
	g_env_bootmode = "bootloader_panic";
	vlx_boot(BOOT_PART, BACKLIGHT_OFF, LCD_ON);
	return;
}

void sml_panic_reboot_mode(void)
{
	dprintf(ALWAYS, "enter [%s]\n", __func__);
	g_env_bootmode = "sml_panic";
	vlx_boot(BOOT_PART, BACKLIGHT_OFF, LCD_ON);
	return;
}

/** modify by yxiaobin CMT-224 NYX-223 Start of PayJoy changes. */

extern uint8_t check_payjoy_flag(void) ;

void fastboot_mode(void)
{
	FTL_Savepoint_Private(PHASE_FASTBOOT_MODE);
	dprintf(ALWAYS, "enter [%s]\n",__func__);

#ifdef ZCFG_HMD_ENTERPRISE_API
	//[HMDEnterpriseService] add block download mode api begin 2024-09-04 
	if (!is_hmd_error_code_set(NULL) && is_block_download_mode()) { //[HMDEnterpriseService] for error code showing 2025-03-04
	    printf("download mode was block, enter normal\n");
	    normal_mode();
	    return;
	}
	//[HMDEnterpriseService] add block download mode api end 2024-09-04 
#endif

	stop_watchdog();
	g_env_bootmode = "fastboot";
	write_log();
	 /** modify by yxiaobin CMT-224 NYX-223 Start of PayJoy changes. */
    // Don't let phone get into fastboot mode by command "adb reboot bootloader" or other ways
    // if the payjoy flag has been set to 1.

    if (1 == check_payjoy_flag()) {
        normal_mode();
		return;
    }

    /** modify by yxiaobin CMT-224 NYX-223 End of PayJoy changes. */
#ifdef CONFIG_SPLASH_SCREEN
	logo_display(6, BACKLIGHT_ON, LCD_ON); // ning.wei@hmd++ for display first fastboot ui

	vibrator_hw_init();
	set_vibrator(1);

	fastboot_lcd_printf();
	mdelay(400);
	set_vibrator(0);
#endif
	//MMU_DisableIDCM();
#if (defined CONFIG_X86) && (defined CONFIG_MOBILEVISOR) && (defined CONFIG_SPRD_SOC_SP9853I)
	tos_start_notify();
#endif
#ifdef SPRD_SECBOOT
	if(!dump_backtrace_once){ //if dump enter fastboot, cannot get lockstatus
		if (get_lock_status() == VBOOT_STATUS_UNLOCK){
			debugf("INFO: LOCK FLAG IS : UNLOCK!!!\n");
			lcd_printf("\n   INFO: LOCK FLAG IS : UNLOCK!!!\n");
		}
		get_secboot_base_from_dt();
	}
#endif
	do_fastboot();
}

#ifdef CONFIG_ERASE_SPL_AUTO_DOWNLOAD
/**
 * If the SPL is erased, the romcode will enter the download process.
 * Notice: SPL must download.
 */
int erase_spl_enter_download_mode(void)
{
	stop_watchdog();
	if (0 != common_raw_erase("splloader", (uint64_t)0, (uint64_t)0)) {
		debugf("erase partition splloader fail!\n");
		return -1;
	} else {
		if (0 != common_raw_erase("splloader_bak", (uint64_t)0, (uint64_t)0)) {
			debugf("erase partition splloader_bak fail!\n");
			return -2;
		} else {
			debugf("erase partition splloader and splloader_bak ok\n");
			reboot_devices(CMD_NORMAL_MODE);
			while(1);
		}
	}

	return 0;
}
#endif

void autodloader_mode(void)
{

#ifdef CONFIG_FASTBOOT_SECURITY_DOWNLOAD
	/* disable to use 'adb reboot autodloader' */
	if (fb_check_reboot_edl(NULL)) {
		dprintf(INFO,"adb reboot autodloader was disabled\n");
		reboot_devices(CMD_NORMAL_MODE);
		while(1);
		return;
	}
#endif

	debugf("Enter autodloader mode\n");

#if !DEBUG
	stop_watchdog();
#endif

#ifdef CONFIG_ERASE_SPL_AUTO_DOWNLOAD
	if (0 != erase_spl_enter_download_mode()) {
		debugf("erase partition splloader and splloader_bak fail!\n");
		debugf("enter old autodloader_mode!\n");
	}
#endif

#if (defined CONFIG_X86) && (defined CONFIG_MOBILEVISOR) && (defined CONFIG_SPRD_SOC_SP9853I)
	tos_start_notify();
#endif

#ifdef SPRD_SECBOOT
	get_secboot_base_from_dt();
#endif
	/* remap iram */
	//autodlader_remap();
	/* main handler receive and jump */
	autodloader_mainhandler();

	/*reach here means error happened*/
	return;
}

void charge_mode(void)
{
	FTL_Savepoint_Private(PHASE_CHARGE_MODE);
	debugf("enter\n");
	stop_watchdog();
	g_charger_mode = 1;
	g_env_bootmode = "charger";
#ifdef CONFIG_SC27XX_VDDVIB_VSD2
	vibrator_hw_init();
	set_vibrator(1);
#endif
	vlx_boot(BOOT_PART, BACKLIGHT_ON, LCD_ON);
}

void engtest_mode(void)
{
	debugf("enter\n");
	stop_watchdog();
	g_env_bootmode = "engtest";
	vlx_boot(BOOT_PART, BACKLIGHT_OFF, LCD_ON);
}


void alarm_mode(void)
{
	debugf("enter\n");
	g_env_bootmode = "alarm";
	vlx_boot(BOOT_PART, BACKLIGHT_OFF, LCD_ON);
}

void sprdisk_mode(void)
{
	debugf("enter\n");
	stop_watchdog();
	g_env_bootmode = "sprdisk";
	vlx_boot(BOOT_PART, BACKLIGHT_OFF, LCD_ON);
}

void apkmmi_mode(void)
{
	debugf("apkmmi enter\n");
	stop_watchdog();
	g_env_bootmode = "apkmmi_mode";
#ifdef CONFIG_SC27XX_VDDVIB_VSD2
	vibrator_hw_init();
	set_vibrator(1);
#endif
	vlx_boot(BOOT_PART, BACKLIGHT_ON, LCD_ON);
}

void upt_mode(void)
{
	debugf("upt enter\n");
	stop_watchdog();
	g_env_bootmode = "upt_mode";
	vibrator_hw_init();
	set_vibrator(1);
	vlx_boot(BOOT_PART, BACKLIGHT_ON, LCD_ON);
}

void apkmmi_auto_mode(void)
{
	debugf("enter\n");
	stop_watchdog();
	g_env_bootmode = "apkmmi_auto_mode";
	vlx_boot(BOOT_PART, BACKLIGHT_ON, LCD_ON);
}

void silent_mode(void)
{
	dprintf(INFO,"silent_mode enter\n");
	stop_watchdog();
	g_env_bootmode = "silent";
	vibrator_hw_init();
	set_vibrator(0);
	vlx_boot(BOOT_PART, BACKLIGHT_OFF, LCD_ON);
}

/* ZOVERLAY_TAG_HMD_COMMON */
