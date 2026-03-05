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
#include <app.h>
#include <asm/arch/check_reboot.h>
#include <boot_mode.h>
#include <lk/debug.h>
#include <stdlib.h>
#include "boot_mode.h"
#include "boot_parse.h"
#include <logo_bin.h>
#include <lib/console.h>
#include <string.h>
#include "fastboot.h"
#include <sprd_common.h>
#include <sprd_sysdump.h>
#include <dl_common.h>
#include <sprd_wdt.h>
#include <asm/types.h>
#include <sprd_regulator.h>
#include "calibration_detect.h"
#include <sprd_battery.h>
#include <sprd_keys.h>
#include <sprd_common_rw.h>
#include <miscdata_def.h>
#include <sprd_ddr_debug.h>
#ifdef CONFIG_ANDROID_AB
#include <android_ab.h>
#include <chipram_env.h>
#include <sprd_pmic_misc.h>
#include <lk/board.h>

char g_env_slot[3] = {"_a"};
#endif
#ifdef CONFIG_UDC
#include <udc.h>
#include <chipram_env.h>
#endif
#ifdef CONFIG_DOWNLOAD_FOR_CUSTOMER
extern int pctool_connected(void);
#endif
#ifdef CONFIG_TEECFG
#include <teecfg_parse.h>
#endif
#ifdef SPRD_MINI_SYSDUMP
extern void handle_minidump_before_boot(int rst_mode);
#endif
#ifdef CONFIG_USB_SPRD_DWC31_PHY
extern void usb_phy_enable(u32 is_on);
extern int usb_phy_enabled(void);
#endif

extern int read_boot_flag(void);
extern CBOOT_FUNC s_boot_func_array[CHECK_BOOTMODE_FUN_NUM];
extern void board_boot_mode_regist(CBOOT_MODE_ENTRY *array);
extern const char *bootcause_cmdline;
extern int sprdbat_get_shutdown_charge_flag(void);
extern int sprdbat_get_auto_poweron_flag(void);
extern int is_hw_smpl_enable(void);

#define USBPHY_VDDUSB33_NORMAL		3300
#define USBPHY_VDDUSB33_FULLSPEED_TUNE	2700
#define LOWPOWER_LOGO_VOL	        3000
#define LOWPOWER_LOGO_ENABLE_TIME	15
#define LOWPOWER_LOGO_DISABLE_TIME	0

static const char *g_mode_str[CMD_MAX_MODE+1] = {
	"UNDEFINED_MODE",
	"POWER_DOWN_DEVICE",
	"NORMAL_MODE",
	"DOWNLOAD_MODE",
	"RECOVERY_MODE",
	"FASTBOOT_MODE",
	"ALARM_MODE",
	"CHARGE_MODE",
	"ENGTEST_MODE",
	"WATCHDOG_REBOOT",
	"AP_WATCHDOG_REBOOT",
	"SPECIAL_MODE",
	"UNKNOW_REBOOT_MODE",
	"PANIC_REBOOT",
	"VMM_PANIC_MODE",
	"TOS_PANIC_MODE",
	"EXT_RSTN_REBOOT_MODE",
	"CALIBRATION_MODE",
	"USB_MUX_MODE",
	"AUTODLOADER_REBOOT",
	"AUTOTEST_MODE",
	"IQ_REBOOT_MODE",
	"SLEEP_MODE",
	"SPRDISK_MODE",
	"APKMMI_MODE",
	"UPT_MODE",
	"APKMMI_AUTO_MODE",
	"ABNORMAL_REBOOT_MODE",
	"SILENT_MODE",
	"BOOTLOADER_PANIC_MODE",
	"SML_PANIC_MODE",
	"MAX_MODE",
};

CBOOT_FUNC s_boot_func_array[CHECK_BOOTMODE_FUN_NUM] = {
	/* 0 get mode from chipram_env*/
	get_mode_from_chipram_env,
	/* 1 get mode from bat low*/
	get_mode_from_bat_low,
	/* 2 get mode from sysdump*/
	write_sysdump_before_boot_extend,
	/* 3 get mode from miscdata flag*/
	get_mode_from_miscdata_boot_flag,
	/* 4 get mode from file*/
	get_mode_from_file_extend,
	/* 5 get mode from watch dog*/
	get_mode_from_watchdog,
	/* 6 get mode from alarm register*/
	get_mode_from_alarm_register,
	/* 7 get mode from calibration detect*/
	get_mode_from_pctool,
	/* 8 get mode from smpl*/
	get_mode_from_smpl,
	/* 9 get mode from charger*/
	get_mode_from_charger,
	/* 10 get mode from keypad*/
	get_mode_from_keypad,
	/* 11 get mode from gpio*/
	get_mode_from_gpio_extend,

	0
};

void board_boot_mode_regist(CBOOT_MODE_ENTRY *array)
{
	MODE_REGIST(CMD_NORMAL_MODE, normal_mode);
	MODE_REGIST(CMD_DOWNLOAD_MODE, download_mode);
	MODE_REGIST(CMD_RECOVERY_MODE, recovery_mode);
	MODE_REGIST(CMD_FASTBOOT_MODE, fastboot_mode);
	MODE_REGIST(CMD_WATCHDOG_REBOOT, watchdog_mode);
	MODE_REGIST(CMD_AP_WATCHDOG_REBOOT, ap_watchdog_mode);
	MODE_REGIST(CMD_UNKNOW_REBOOT_MODE, unknow_reboot_mode);
	MODE_REGIST(CMD_PANIC_REBOOT, panic_reboot_mode);
	MODE_REGIST(CMD_AUTODLOADER_REBOOT, autodloader_mode);
	MODE_REGIST(CMD_SPECIAL_MODE, special_mode);
	MODE_REGIST(CMD_CHARGE_MODE, charge_mode);
	MODE_REGIST(CMD_ENGTEST_MODE,engtest_mode);
	MODE_REGIST(CMD_CALIBRATION_MODE, calibration_mode);
	MODE_REGIST(CMD_EXT_RSTN_REBOOT_MODE, normal_mode);
	MODE_REGIST(CMD_IQ_REBOOT_MODE, iq_mode);
	MODE_REGIST(CMD_ALARM_MODE, alarm_mode);
	MODE_REGIST(CMD_SPRDISK_MODE, sprdisk_mode);
	MODE_REGIST(CMD_AUTOTEST_MODE, autotest_mode);
	MODE_REGIST(CMD_APKMMI_MODE, apkmmi_mode);
	MODE_REGIST(CMD_UPT_MODE, upt_mode);
	MODE_REGIST(CMD_APKMMI_AUTO_MODE, apkmmi_auto_mode);
	MODE_REGIST(CMD_ABNORMAL_REBOOT_MODE, abnormal_reboot_mode);
	MODE_REGIST(CMD_SILENT_MODE, silent_mode);
	MODE_REGIST(CMD_BOOTLOADER_PANIC_MODE, bootloader_panic_reboot_mode);
	MODE_REGIST(CMD_SML_PANIC_MODE, sml_panic_reboot_mode);
	return ;
}

int chrtodec(char chr)
{
	int value=0;
	if((chr>='a')&&(chr<='z'))
	{
		chr=chr-32;
	}
	if((chr>='0')&&(chr<='9'))
	{
		value=chr-48;
	}else if((chr>='A')&&(chr<='Z')){
		value=chr-65+10;
	}
	return value;
}

#ifdef CONFIG_HMD_FASTBOOT
int get_off_mode_charge(void)
{
	unsigned short flag,value;
	
	if (common_raw_read("miscdata",MISCDATA_OFF_MODE_CHARGE_FLAG_DATA_LEN, MISCDATA_OFF_MODE_CHARGE_FLAG_BASE, &flag)) {
		errorf("read off-mode-charge flag error!\n");
	}
	value=chrtodec(flag);
	debugf("get_off_mode_charge: MISCDATA_OFF_MODE_CHARGE_FLAG_BASE=%d  data=%d flag=%d\n",MISCDATA_OFF_MODE_CHARGE_FLAG_BASE,value,flag);		
	return value;
}
#endif

boot_mode_enum_type get_mode_from_arg(char* mode_name)
{

	dprintf(INFO,"cboot:get mode from argument:%s\n",mode_name);
	if(!strcmp(mode_name,"normal"))
		return CMD_NORMAL_MODE;

	if(!strcmp(mode_name,"recovery"))
		return CMD_RECOVERY_MODE;

	if(!strcmp(mode_name,"fastboot"))
		return CMD_FASTBOOT_MODE;

	if(!strcmp(mode_name,"charge"))
		return CMD_CHARGE_MODE;

	if(!strcmp(mode_name,"sprdisk"))
		return CMD_SPRDISK_MODE;
	/*just for debug*/
#ifdef SPRD_SYSDUMP
	if(!strcmp(mode_name,"sysdump"))
		write_sysdump_before_boot(CMD_UNKNOW_REBOOT_MODE);
#endif
	return CMD_UNDEFINED_MODE;
}

unsigned reboot_mode_check(void)
{
	static unsigned rst_mode = 0;
	static unsigned check_times = 0;

	if(!check_times) {
		rst_mode = check_reboot_mode();
		check_times++;
	}
	dprintf(INFO,"reboot_mode_check rst_mode=0x%x\n",rst_mode);

	return rst_mode;
}

void low_power_charge(int temp_status)
{
	int32_t vbat_vol;
	enum sprd_adapter_type charger_type;

	charger_type = sprdchg_charger_is_adapter();
	vbat_vol = sprdfgu_read_vbat_vol();
	if((charger_type != ADP_TYPE_SDP && charger_type != ADP_TYPE_UNKNOW) || vbat_vol > LOWPOWER_LOGO_VOL ) {
		sprdbat_show_lowpower_charge_logo(LOWPOWER_LOGO_ENABLE_TIME);
		debugf("cboot:vbat greater than 3V or other charger type\n");
	} else {
		sprdbat_show_lowpower_charge_logo(LOWPOWER_LOGO_DISABLE_TIME);
		debugf("cboot:vbat less than 3V and charge type is SDP or UNKNOW\n");
	}
	sprdbat_lowbat_chg(temp_status);
	mdelay(SPRDBAT_CHG_POLLING_T);
}

boot_mode_enum_type  check_mode_from_keypad(void)
{
	uint32_t key_mode = 0;
	int key_code = 0;
	volatile int i;

	if (boot_pwr_check() >= PWR_KEY_DETECT_CNT) {
		mdelay(50);
		for (i = 0; i < 10; i++) {
			key_code = board_key_scan();
			if(key_code != KEY_RESERVED)
			  break;
		}
		key_mode = check_key_boot(key_code);
		if (!key_mode)
			key_mode = CMD_NORMAL_MODE;
	}
	return key_mode;
}

// get mode from chipram_env
boot_mode_enum_type  get_mode_from_chipram_env(void)
{
	boot_mode_t boot_role;
	char buf[DL_PROCESS_LEN] = {0};
	dl_get_downloadflag(buf);
	dprintf(INFO,"==== in  [%s] \n", __func__);
	boot_role = get_boot_role();
	if (boot_role == BOOTLOADER_MODE_DOWNLOAD) {
		return CMD_DOWNLOAD_MODE;
	} else if (!strncmp("enable", buf, strlen("enable"))) {
		dl_clear_downloadflag();
		return CMD_AUTODLOADER_REBOOT;
	}
	else if (check_mode_from_keypad() == CMD_FASTBOOT_MODE) {
		return CMD_FASTBOOT_MODE;
	}
	else if (boot_role == BOOTLOADER_MODE_UNKNOW) {
		errorf("unkown uboot role!!!!!!!!!!!!!!!!!\n");
	}
	return CMD_UNDEFINED_MODE;
}

  // 0 get mode from pc tool
boot_mode_enum_type get_mode_from_pctool(void)
{
	int ret, to_vol;
	dprintf(INFO,"==== in  [%s] \n", __func__);

#if defined(PLATFORM_SHARKLE) || defined(PLATFORM_SHARKL3) \
	|| defined(PLATFORM_QOGIRL6) || defined(PLATFORM_QOGIRN6PRO) || defined(PLATFORM_QOGIRN6L)
	to_vol = USBPHY_VDDUSB33_NORMAL;
#else
	to_vol = USBPHY_VDDUSB33_FULLSPEED_TUNE;
#endif

	regulator_set_voltage("vddusb33", to_vol);
	ret = pctool_mode_detect();
	regulator_set_voltage("vddusb33", USBPHY_VDDUSB33_NORMAL);
	if (ret < 0)
		return CMD_UNDEFINED_MODE;
	else {
		bootcause_cmdline="The pctools sends control commands";
		return ret;
	}
}

#ifndef CONFIG_ZEBU
boot_mode_enum_type get_mode_from_smpl(void)
{
	dprintf(INFO,"==== in  [%s] \n", __func__);

	if(is_smpl_bootup()) {
		debugf("is_hw_smpl_enable %d\n", is_hw_smpl_enable());
		lr_cause = LR_ABNORMAL;
		bootcause_cmdline="Sudden momentary power loss";
		debugf("SMPL bootup!\n");
		return CMD_NORMAL_MODE;
	}
	return CMD_UNDEFINED_MODE;
}
#else
boot_mode_enum_type get_mode_from_smpl(void)
{
	return CMD_UNDEFINED_MODE;
}
#endif

boot_mode_enum_type get_mode_from_bat_low(void)
{
	dprintf(INFO,"==== in  [%s] \n", __func__);
#ifndef CONFIG_FPGA       //jump loop
	int is_bat_low_flag, temp_status;

	while(1) {
		is_bat_low_flag = is_bat_low();
		temp_status = sprdbat_get_battery_temp_status();
		if (!is_bat_low_flag && !temp_status)
			break;
		if (temp_status && is_bat_low_flag) {
			if(charger_connected() || get_mode_from_vchg()) {
				dprintf(INFO,"cboot:low battery and abnormal temerature,stop charging...\n");
				low_power_charge(temp_status);
			} else {
				dprintf(INFO, "cboot:low battery and abnormal temerature,shutdown\n");
				logo_display(LOGO_LOW_VOL, BACKLIGHT_ON, LCD_DISPLAY_ENABLE);
				mdelay(LOW_VOL_DISPLAY_DELAY_TIME);
				return CMD_POWER_DOWN_DEVICE;
			}
		} else if (temp_status && !is_bat_low_flag) {
			dprintf(INFO, "cboot:abnormal temerature,stop charging...\n");
			sprdbat_lowbat_chg(temp_status);
			if (temp_status == DISCHARGING_TEMP)
				return CMD_UNDEFINED_MODE;
			dprintf(INFO, "cboot:abnormal temerature,disbooting\n");
			mdelay(SPRDBAT_CHG_POLLING_T);
		} else if (!temp_status && is_bat_low_flag) {
			if(charger_connected() || get_mode_from_vchg()) {
				dprintf(INFO, "cboot:low battery,charging...\n");
				low_power_charge(temp_status);
			} else {
				dprintf(INFO, "cboot:low battery and shutdown\n");
				logo_display(LOGO_LOW_VOL, BACKLIGHT_ON, LCD_DISPLAY_ENABLE);
				mdelay(LOW_VOL_DISPLAY_DELAY_TIME);
				return CMD_POWER_DOWN_DEVICE;
			}
		}
#if !DEBUG
		stop_watchdog();
#endif /* DEBUG */
	}
#endif /* CONFIG_FPGA */
	sprdbat_chg_led(0);
	return CMD_UNDEFINED_MODE;
}


#ifndef CONFIG_ZEBU
boot_mode_enum_type write_sysdump_before_boot_extend(void)
{
	dprintf(INFO,"==== in  [%s] \n", __func__);
	unsigned rst_mode = reboot_mode_check();
#ifdef SPRD_MINI_SYSDUMP
	handle_minidump_before_boot(rst_mode);
#endif
#ifdef SPRD_SYSDUMP
	write_sysdump_before_boot(rst_mode);
#endif
	return CMD_UNDEFINED_MODE;
}
#else
boot_mode_enum_type write_sysdump_before_boot_extend(void)
{
	return CMD_UNDEFINED_MODE;
}
#endif

/* get mode from miscdata */
boot_mode_enum_type get_mode_from_miscdata_boot_flag(void)
{
	dprintf(INFO,"==== in  [%s] \n", __func__);
#if defined(CONFIG_CUSTOMER_PHONE) && defined(CONFIG_AUTOBOOT)
	extern unsigned reboot_reg;
	if ((reboot_reg & 0xFF) == HWRST_STATUS_AUTODLOADER) {
		dprintf(INFO,"%s:rst_mode is autodloader\n", __func__);
		return CMD_AUTODLOADER_REBOOT;
	}
#elif defined CONFIG_AUTOBOOT
	dprintf(INFO," AUTOBOOT ENABLE! \n");
	return CMD_NORMAL_MODE;
#endif

	boot_mode_enum_type first_mode = read_boot_flag();
	if (first_mode != CMD_UNDEFINED_MODE) {
		bootcause_cmdline="Detect the firsrt_mode flag in the miscdata partition";
		dprintf(INFO,"get mode from firstmode field: %s\n", g_mode_str[first_mode]);
		return first_mode;
	}

	return CMD_UNDEFINED_MODE;
}

/*1 get mode from file, just for recovery mode now*/
boot_mode_enum_type get_mode_from_file_extend(void)
{
	dprintf(INFO,"==== in  [%s] \n", __func__);
	switch (get_mode_from_file()) {
		case CMD_RECOVERY_MODE:
			dprintf(INFO,"cboot:get mode from file:recovery\n");
			bootcause_cmdline="Detect the recovery message in the misc partition";
			return CMD_RECOVERY_MODE;
		default:
			return CMD_UNDEFINED_MODE;
	}
	return CMD_UNDEFINED_MODE;
}

// 2 get mode from watch dog
boot_mode_enum_type get_mode_from_watchdog(void)
{
	dprintf(INFO,"==== in  [%s] \n", __func__);
	unsigned rst_mode = reboot_mode_check();
	int flag;
	switch (rst_mode) {
		case CMD_RECOVERY_MODE:
		case CMD_FASTBOOT_MODE:
		case CMD_NORMAL_MODE:
		case CMD_WATCHDOG_REBOOT:
		case CMD_AP_WATCHDOG_REBOOT:
		case CMD_ABNORMAL_REBOOT_MODE:
		case CMD_UNKNOW_REBOOT_MODE:
		case CMD_PANIC_REBOOT:
		case CMD_AUTODLOADER_REBOOT:
		case CMD_SPECIAL_MODE:
		case CMD_EXT_RSTN_REBOOT_MODE:
		case CMD_IQ_REBOOT_MODE:
		case CMD_SPRDISK_MODE:
		case CMD_SILENT_MODE:
		case CMD_BOOTLOADER_PANIC_MODE:
		case CMD_SML_PANIC_MODE:
			return rst_mode;
		case CMD_ALARM_MODE:
			if ((flag = alarm_flag_check())) {
				dprintf(INFO,"get_mode_from_watchdog flag=%d\n", flag);
				if (flag == 1) {
				  bootcause_cmdline="PMIC reg reset and poweroff alarm";
				  return CMD_ALARM_MODE;
				}
				else if (flag == 2) {
				  bootcause_cmdline="PMIC reg reset but Shutdown alarm is abnormal";
				  return CMD_NORMAL_MODE;
				}
			}
		default:
			return CMD_UNDEFINED_MODE;
	}
}

// 3 get mode from alarm register
boot_mode_enum_type  get_mode_from_alarm_register(void)
{
	int flag;

	dprintf(INFO,"==== in  [%s] \n", __func__);
	if (alarm_triggered() && (flag = alarm_flag_check())) {
		dprintf(INFO,"get_mode_from_alarm_register flag=%d\n", flag);
		if (flag == 1) {
			bootcause_cmdline="RTC poweroff alarm expiry";
			return CMD_ALARM_MODE;
		}
		else if (flag == 2) {
			bootcause_cmdline="Shutdown alarm is abnormal";
			return CMD_NORMAL_MODE;
		}
	}

	return CMD_UNDEFINED_MODE;
}

/* 4 get mode from charger*/
boot_mode_enum_type  get_mode_from_charger(void)
{
	int shutdown_charge_flag = 0, auto_poweron_flag = 0;
	boot_mode_enum_type bootmode = CMD_UNDEFINED_MODE;

	dprintf(INFO,"==== in  [%s] \n", __func__);
	auto_poweron_flag = sprdbat_get_auto_poweron_flag();
	shutdown_charge_flag = sprdbat_get_shutdown_charge_flag();
	dprintf(INFO,"get_shutdown_charge_flag flag=%d\n", shutdown_charge_flag);
	if ((!charger_connected()) && (!get_mode_from_vchg()))
		return bootmode;

	if (!auto_poweron_flag) {
		dprintf(INFO,"auto_poweron_flag get mode from charger\n");
		bootcause_cmdline="in charging during shutdown the devices";
		bootmode = CMD_CHARGE_MODE;
	} else {
		dprintf(INFO,"get_auto_poweron_flag auto_poweron_flag=%d\n", auto_poweron_flag);
		if (shutdown_charge_flag) {
			dprintf(INFO,"get mode from charger\n");
			bootcause_cmdline="in charging during shutdown the devices";
			bootmode = CMD_CHARGE_MODE;
		} else {
			bootcause_cmdline="Charger connected";
			#ifdef ZCFG_CMD_CHARGE_MODE   
			bootmode = CMD_CHARGE_MODE;
			#else
	        bootmode = CMD_NORMAL_MODE;
            #endif
		}
	}

#ifdef CONFIG_DOWNLOAD_FOR_CUSTOMER
	if (bootmode == CMD_CHARGE_MODE && (board_key_scan() == (KEY_VOLUMEUP + KEY_VOLUMEDOWN)) &&
	   (pctool_connected())) {
		dprintf(ALWAYS, "KEY_VOLUMEUP + KEY_VOLUMEDOWN trigger to download\n");
		bootmode = CMD_AUTODLOADER_REBOOT;
	}
#endif

#ifdef CONFIG_HMD_FASTBOOT
	if (get_off_mode_charge() == 1) {
		return CMD_NORMAL_MODE;
	} else {
		return CMD_CHARGE_MODE;
	}
#endif

	return bootmode;
}

/*5 get mode from keypad*/
boot_mode_enum_type  get_mode_from_keypad(void)
{
	uint32_t key_mode = 0;

	dprintf(INFO,"==== in  [%s] \n", __func__);
	key_mode = check_mode_from_keypad();
	dprintf(INFO,"cboot:get mode from keypad:%d, %s\n",key_mode, g_mode_str[key_mode]);
	if (!key_mode)
		return CMD_UNDEFINED_MODE;

	switch(key_mode) {
		case CMD_RECOVERY_MODE:
			bootcause_cmdline="Keypad trigger to recovery";
			return CMD_RECOVERY_MODE;
		default:
			bootcause_cmdline="Pbint triggered";
			return CMD_NORMAL_MODE;
	}
}

// 6 get mode from gpio
boot_mode_enum_type  get_mode_from_gpio_extend(void)
{
	dprintf(INFO,"==== in  [%s] \n", __func__);
	if (get_mode_from_gpio()) {
		bootcause_cmdline="pbint2 triggered";
		dprintf(INFO,"Pbint2 triggered, do normal mode\n");
		return CMD_NORMAL_MODE;
	} else {
		return CMD_UNDEFINED_MODE;
	}
}

/** modify by yxiaobin CMT-224 NYX-223 Start of PayJoy changes. */

uint8_t check_payjoy_flag(void) {
    uint64_t offset;
    uint8_t val;

    offset = 2048*1024 - 1 - 1000 - 1024 - 10000;
    // read payjoy flag from persistent partition.
    // you may change the actual number of offset according to the size of FRP or persistent partition
    common_raw_read("persist", 1, offset,&val); // call mmc_read
    return val;
}
/** modify by yxiaobin CMT-224 NYX-223 End of PayJoy changes. */


#if defined CONFIG_ANDROID_AB
void do_select_ab(void)
{
	int ret;
	char slot[3];

#if defined (CONFIG_FPGA) && defined (CONFIG_DDR_BOOT)
	ret = 0;
#else
	ret = ab_select_slot();
#endif
	if (ret < 0) {
		errorf("Android boot failed, error %d.\n", ret);
		return;
	}

	/* Android standard slot names are 'a', 'b', ... */
	slot[0] = '_';
	slot[1] = BOOT_SLOT_NAME(ret);
	slot[2] = '\0';

	memcpy(g_env_slot, slot, sizeof(slot));

	dprintf(CRITICAL,"ANDROID: Booting slot%s\n", slot);
}
#endif

#ifdef CONFIG_MMC_POWP_SUPPORT
extern int g_part_protected;
extern char *g_part_name;

static void do_emmc_powp(void)
{
	if (g_part_protected) {
		if (common_set_powp(g_part_name)) {
			errorf("partition %s can't be protected, pls check it\n", g_part_name);
			return -1;
		}
		debugf("do emmc powp success\n");
	}
}
#endif
int is_sprd_enter_cali = 0;
int sprd_boot(void)
{
	volatile int i;
	boot_mode_enum_type bootmode = CMD_UNDEFINED_MODE;
	CBOOT_MODE_ENTRY boot_mode_array[CMD_MAX_MODE] ={0};

	FTL_Savepoint_Private(PHASE_SPRDBOOT_INIT);
#ifdef CONFIG_AUTOLOAD_MODE
	autoload_mode();
#endif

#ifdef CONFIG_ANDROID_AB
	if(adjust_spl_slot())
		return -1;
	do_select_ab();
#endif
	//add by jinqiang for udc
#ifdef CONFIG_UDC
	  boot_mode_t boot_role;
	  chipram_env_t* cr_env = get_chipram_env();
	  boot_role = cr_env->mode;
	  if(boot_role == BOOTLOADER_MODE_DOWNLOAD)
	  {  
		 printf("sprd_boot: download mode\n");
	  }
	  else
		{
		 printf("udc_create: start...\n");
		 udc_create( (char*)UDC_MEM_ADDR );
		}
#endif

	if (get_boot_role() == BOOTLOADER_MODE_LOAD){
#ifndef CONFIG_ZEBU
		reconfig_baudrate(); /* switch baudrate by following kernel menu */
#endif
		ddr_eng_mode_debug(); // config ddr paras from eng mode
#ifdef CONFIG_USB_SPRD_DWC31_PHY
		if (usb_phy_enabled())
			usb_phy_enable(0);
#endif
	}

#if defined CONFIG_ZEBU
	normal_mode();
	errorf("normal mode exit with exception!!!\n");
	while(1);
#endif

#ifndef CONFIG_FPGA
	boot_pwr_check();
#endif

	for (i = 0;  i < CHECK_BOOTMODE_FUN_NUM; i++) {
		if (0 == s_boot_func_array[i]) {
			bootmode = CMD_POWER_DOWN_DEVICE;
			dprintf(INFO,"==== get boot mode in boot func array[%d]\n",i);
			break;
		}
		bootmode = s_boot_func_array[i]();
		if (CMD_UNDEFINED_MODE == bootmode) {
			continue;
		} else {
			dprintf(INFO,"get boot mode in boot func array[%d]\n",i);
			break;
		}
	}

	board_boot_mode_regist(boot_mode_array);
	dprintf(ALWAYS,"enter boot mode g_mode_str[%d]:%s\n", bootmode, g_mode_str[bootmode]);
	if((bootmode != CMD_FASTBOOT_MODE)&&(bootmode!=CMD_CALIBRATION_MODE)&&(bootmode!=CMD_AUTOTEST_MODE)){
		if(check_mode_from_keypad()==CMD_FASTBOOT_MODE){
			bootmode=CMD_FASTBOOT_MODE;
		}
	}
	dprintf(ALWAYS,"jinq lk enter boot mode g_mode_str[%d]:%s\n", bootmode, g_mode_str[bootmode]);
	#if defined(CONFIG_EMMC_WP)
	if((bootmode==CMD_CALIBRATION_MODE)||(bootmode==CMD_AUTOTEST_MODE))
	{	
		is_sprd_enter_cali = 1;
		#if defined(CONFIG_CALI_MODE)
		//modify by ysong for cali mode begin
		int is_enter_cali = 0;
		extern int cali_verify_and_set_flag(void);
		is_enter_cali = cali_verify_and_set_flag();
		if(is_enter_cali){
			debugf("enter cali mode\n");
		}else{
			debugf("enter cali fail, enable shutdown cali port, power down device!!!\n");
			bootmode=CMD_POWER_DOWN_DEVICE;
		}
		//modify by ysong for cali mode end
		#endif
	}else{
		if(bootmode != CMD_DOWNLOAD_MODE)
		{
			extern int powp_verify_and_set_flag(void);
			powp_verify_and_set_flag();
			//#define EMMC  0
			//extern ulong mmc_set_pwr_wp(int dev_num, lbaint_t start, int grp_cnt);
			//mmc_set_pwr_wp(EMMC,0x00024000,1);  //72M~80M POWP
		}
	}
	#endif
#ifdef CONFIG_MMC_POWP_SUPPORT
	if (get_boot_role() == BOOTLOADER_MODE_LOAD)
		do_emmc_powp();
#endif

	// Start of PayJoy changes.
	if (bootmode == CMD_FASTBOOT_MODE) {
        if (1 == check_payjoy_flag()) {
            bootmode = CMD_NORMAL_MODE;
        }
    }
	// end of PayJoy changes.
	
	if ((bootmode > CMD_POWER_DOWN_DEVICE) &&(bootmode < CMD_MAX_MODE)
		&& (0 != boot_mode_array[bootmode])) {
		boot_mode_array[bootmode]();
	} else {
#ifdef CONFIG_FPGA
		/*FPGA has no power button ,if hasn't detect any mode ,use normal mode*/
		dprintf(INFO, "FPGA use normal mode instead of power down.\n");
		normal_mode();
#else
		dprintf(ALWAYS, "power down device\n");
		write_log();
		power_down_devices(0);
		dprintf(ALWAYS, "===== after regist, loop forever!!! \n");
		while(1);
#endif
	}

	dprintf(ALWAYS, "==== in  [%s] \n", __func__);
	return 0;
}

static void boot_init(const struct app_descriptor *app) {
    console_init();
#if DEBUG
    manual_cmd_shell();
#endif
    sprd_boot();
}

APP_START(sprdboot)
.init = boot_init,
APP_END


/* ZOVERLAY_TAG_HMD_COMMON */
