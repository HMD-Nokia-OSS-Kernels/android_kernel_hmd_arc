/*
 * Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
*/
//ZOVERLAY_TAG_HMD_ONEIMAGE
#include <boot_mode.h>
#include <asm/arch/sprd_reg.h>
#include <asm/arch/common.h>
#include <adi_hal_internal.h>
#include <asm/arch/check_reboot.h>
#include <asm/arch/sprd_debug.h>
#include <sprd_pmic_misc.h>
#include <sprd_common.h>
#include <eic.h>
#include <sprd_log.h>
#include <chipram_env.h>

unsigned reboot_reg = 0;
unsigned sysdump_flag = 0;
unsigned g_dmverity_flag = 0;
unsigned g_dmverity_corrupt_flag = 0;
extern int hw_watchdog_rst_pending(void);
extern void power_down_cpu(ulong ignored);
extern const char *bootcause_cmdline;

static void rtc_domain_reg_write(uint32_t val)
{
#if  !defined(CONFIG_ADIE_SC2713S)  &&  !defined(CONFIG_ADIE_SC2713)
	sci_adi_write(ANA_REG_GLB_RTC_RST1, (~val),(~0)); //clear status reg
	sci_adi_write(ANA_REG_GLB_RTC_RST0, (val),(~0));  //set status reg
#else
	val = val;
#endif
}

static uint32_t rtc_domain_reg_read(void)
{
#if  !defined(CONFIG_ADIE_SC2713S)  &&  !defined(CONFIG_ADIE_SC2713)
	return sci_adi_read(ANA_REG_GLB_RTC_RST2);  //read
#else
	return 0;
#endif
}

unsigned check_reboot_mode(void)
{
	unsigned rst_mode= 0, reg_rst_mode = 0, hw_wdt_int_raw = 0;
	unsigned hw_rst_mode = ANA_REG_GET(ANA_REG_GLB_POR_SRC_FLAG);

#ifdef CONFIG_ADIE_UMP518
	chipram_env_t* cr_env = get_chipram_env();
	dprintf(INFO,"Get pwr_status from cr ocp:%x scp:%x!\n", cr_env->ocp_flag, cr_env->scp_flag);
#endif

	lr_cause = LR_NORMAL;
	debugf("hw_rst_mode==%x\n", hw_rst_mode);
	reg_rst_mode = hw_rst_mode;
	reg_rst_mode &= (0xffff & BIT_REG_RST_FLG);
	dprintf(INFO,"check_reboot_mode:get raw reg_rst_mode is %x\n", reg_rst_mode);

	sci_adi_set(ANA_REG_GLB_POR_SRC_FLAG, BIT_REG_SOFT_RST_FLG_CLR);
	sci_adi_set(ANA_REG_GLB_POR_7S_CTRL, BIT_PBINT_FLAG_CLR |
		    BIT_PBINT2_FLAG_CLR | BIT_CHGR_INT_FLAG_CLR |
		    BIT_EXT_RSTN_FLAG_CLR );
	udelay(10);
	sci_adi_clr(ANA_REG_GLB_POR_7S_CTRL, BIT_PBINT_FLAG_CLR|
		    BIT_PBINT2_FLAG_CLR | BIT_CHGR_INT_FLAG_CLR |
		    BIT_EXT_RSTN_FLAG_CLR );
	sci_adi_clr(ANA_REG_GLB_POR_SRC_FLAG, BIT_REG_SOFT_RST_FLG_CLR);

/*for download mode reboot system */
	if(HWRST_RTCSTATUS_DOWNLOAD_BOOT == rtc_domain_reg_read()) {
		debugf("rtc_domain_reg get reboot normal mode \n");
		rtc_domain_reg_write(HWRST_RTCSTATUS_DEFAULT);
		ANA_REG_SET(ANA_REG_GLB_POR_RST_MONITOR, 0); //clear flag
		bootcause_cmdline= "Reboot into normal due to download finish";
		return CMD_NORMAL_MODE;
	}
/*for download mode reboot system ---- end*/

	reboot_reg = rst_mode = ANA_REG_GET(ANA_REG_GLB_POR_RST_MONITOR);
	sysdump_flag = rst_mode & HWRST_STATUS_SYSDUMPEN;
	dprintf(INFO,"check_reboot_mode:get raw rst_mode is %x and sysdump_flag is %x\n",rst_mode,sysdump_flag);
	rst_mode &= 0xFF;
	ANA_REG_SET(ANA_REG_GLB_POR_RST_MONITOR, sysdump_flag | 0); //clear flag

	debugf("rst_mode==%x\n",rst_mode);
	hw_wdt_int_raw = hw_watchdog_rst_pending();
	if(hw_wdt_int_raw || reg_rst_mode){
		debugf("hw watchdog rst int pending\n");
		debugf("register reboot method reg_rst_mode is %x\n", reg_rst_mode);
		if(rst_mode == HWRST_STATUS_RECOVERY) {
			bootcause_cmdline="Reboot into reocovery";
			return CMD_RECOVERY_MODE;
		} else if(rst_mode == HWRST_STATUS_FASTBOOT){
			bootcause_cmdline="Reboot into fastboot";
			return CMD_FASTBOOT_MODE;
		}else if(rst_mode == HWRST_STATUS_NORMAL) {
			bootcause_cmdline="Reboot into normal";
			return CMD_NORMAL_MODE;
		} else if(rst_mode == HWRST_STATUS_NORMAL2) {
			bootcause_cmdline="Reboot into watchdog";
			return CMD_WATCHDOG_REBOOT;
		} else if(rst_mode == HWRST_STATUS_NORMAL3) {
			bootcause_cmdline="Reboot into ap watchdog";
			return CMD_AP_WATCHDOG_REBOOT;
		} else if(rst_mode == HWRST_STATUS_ALARM) {
			bootcause_cmdline="Reboot into alarm";
			return CMD_ALARM_MODE;
		} else if(rst_mode == HWRST_STATUS_SLEEP) {
			bootcause_cmdline="Reboot into sleep";
			return CMD_SLEEP_MODE;
		} else if(rst_mode == HWRST_STATUS_CALIBRATION) {
			bootcause_cmdline="Reboot into calibration";
			return CMD_CALIBRATION_MODE;
		} else if(rst_mode == HWRST_STATUS_PANIC) {
			lr_cause = LR_ABNORMAL;
			bootcause_cmdline="Reboot into panic";
			return CMD_PANIC_REBOOT;
		} else if(rst_mode == HWRST_STATUS_SPECIAL) {
			bootcause_cmdline="Reboot into special";
			return CMD_SPECIAL_MODE;
		} else if(rst_mode == HWRST_STATUS_AUTODLOADER) {
			bootcause_cmdline="Reboot into autodloader";
			return CMD_AUTODLOADER_REBOOT;
		} else if(rst_mode == HWRST_STATUS_IQMODE) {
			bootcause_cmdline="Reboot into iqmode";
			return CMD_IQ_REBOOT_MODE;
		} else if(rst_mode == HWRST_STATUS_SILENT) {
			bootcause_cmdline="Reboot into silent";
			return CMD_SILENT_MODE;
		} else if(rst_mode == HWRST_STATUS_SPRDISK) {
			bootcause_cmdline="Reboot into sprdisk";
			return CMD_SPRDISK_MODE;
		} else if(rst_mode == HWRST_STATUS_SECBOOT) {
			bootcause_cmdline="Reboot into normal";
			g_dmverity_flag = rst_mode;
			g_dmverity_corrupt_flag = 1;
			return CMD_NORMAL_MODE;
		} else if(rst_mode == HWRST_STATUS_BOOTLOADER_PANIC) {
			bootcause_cmdline="Reboot into bootloader panic";
			return CMD_BOOTLOADER_PANIC_MODE;
		} else if(rst_mode == HWRST_STATUS_SML_PANIC) {
			bootcause_cmdline="Reboot into sml panic";
			return CMD_SML_PANIC_MODE;
		} else if(rst_mode == HWRST_STATUS_SYSDUMP) {
			bootcause_cmdline="Reboot due to sysdump finish";
			return CMD_NORMAL_MODE;
		}
#if defined(CONFIG_X86)
		else if(rst_mode == HWRST_STATUS_MOBILEVISOR)
			return CMD_VMM_PANIC_MODE;
		else if(rst_mode == HWRST_STATUS_SECURITY)
			return CMD_TOS_PANIC_MODE;
#elif defined(HWRST_STATUS_SECURITY)
		else if(rst_mode == HWRST_STATUS_SECURITY) {
			lr_cause = LR_ABNORMAL;
			bootcause_cmdline="Reboot into tos_panic";
			return CMD_TOS_PANIC_MODE;
		}
#endif
		else{
			if (hw_wdt_int_raw) {
				lr_cause = LR_UNKNOWN;
				debugf(" Boot failure triggered by reboot\n");
				bootcause_cmdline="Reboot into abnormal";
				return CMD_ABNORMAL_REBOOT_MODE;
			} else
				return 0;
		}
	} else{
		dprintf(INFO,"is_7s_reset 0x%x, systemdump 0x%x\n", is_7s_reset(), is_7s_reset_for_systemdump());
		debugf("no hw watchdog and reg rst int pending\n");
		if(is_7s_reset_for_systemdump()) {
			ANA_REG_SET(ANA_REG_GLB_WDG_RST_MONITOR, SW_7SRST_STATUS);
			lr_cause = LR_ABNORMAL;
			bootcause_cmdline="7s reset for systemdump";
			return CMD_UNKNOW_REBOOT_MODE;
		} else if(hw_rst_mode & SW_EXT_RSTN_STATUS) {
			lr_cause = LR_LONG_PRESS;
			bootcause_cmdline="Software extern reset status";
			return CMD_EXT_RSTN_REBOOT_MODE;
		} else if(rst_mode == HWRST_STATUS_NORMAL2) {
			ANA_REG_SET(ANA_REG_GLB_WDG_RST_MONITOR, SW_7SRST_STATUS);
			lr_cause = LR_UNKNOWN;
			bootcause_cmdline="STATUS_NORMAL2 without watchdog pending";
			return CMD_UNKNOW_REBOOT_MODE;
		} else if(is_7s_reset()) {
			lr_cause = LR_ABNORMAL;
			bootcause_cmdline="7s reset";
			return CMD_NORMAL_MODE;
		}
		else
			return 0;
	}
	return 0;
}

void reset_to_normal(unsigned reboot_mode)
{
	unsigned rst_mode = 0;

#if  !defined(CONFIG_ADIE_SC2713S)  &&  !defined(CONFIG_ADIE_SC2713)
	if (CMD_NORMAL_MODE == reboot_mode) {
		rtc_domain_reg_write(HWRST_RTCSTATUS_DOWNLOAD_BOOT);
		udelay(300);
	}
#endif
	if (reboot_mode ==  CMD_NORMAL_MODE) {
		rst_mode = HWRST_STATUS_NORMAL;
	}

	ANA_REG_SET(ANA_REG_GLB_POR_RST_MONITOR, rst_mode);

	reset_cpu();
}
void reboot_devices(unsigned reboot_mode)
{
	unsigned rst_mode = 0;

	if(reboot_mode == CMD_RECOVERY_MODE){
		rst_mode = HWRST_STATUS_RECOVERY;
	}else if(reboot_mode == CMD_FASTBOOT_MODE){
		rst_mode = HWRST_STATUS_FASTBOOT;
	}else if(reboot_mode == CMD_NORMAL_MODE){
		rst_mode = HWRST_STATUS_NORMAL;
#if defined(CONFIG_FASTBOOT_SECURITY_DOWNLOAD) || defined(CONFIG_HMD_FASTBOOT) //modify by hyinfeng for hmd reboot to edl
	}else if(reboot_mode == CMD_AUTODLOADER_REBOOT){
		rst_mode = HWRST_STATUS_AUTODLOADER;
#endif
	}else{
		rst_mode = 0;
	}

	ANA_REG_SET(ANA_REG_GLB_POR_RST_MONITOR, rst_mode);

	reset_cpu();
}
void power_down_devices(unsigned pd_cmd)
{
	power_down_cpu(0);
}

int fastboot_charger_connected(void)
{
	sprd_eic_request(EIC_CHG_INT);

	return !!sprd_eic_get(EIC_CHG_INT);
}

