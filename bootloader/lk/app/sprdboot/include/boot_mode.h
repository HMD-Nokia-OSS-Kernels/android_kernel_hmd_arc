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

#ifndef _BOOT_MODE_H_
#define _BOOT_MODE_H_

#include <asm/types.h>
#include <lk/board.h>
#ifdef ZCFG_UBOOT_BACKLIGHT_ON_LEVEL  
#define BACKLIGHT_ON ZCFG_UBOOT_BACKLIGHT_ON_LEVEL
#else
#define BACKLIGHT_ON 25
#endif
#define BACKLIGHT_OFF 0
#define LCD_ON 1
#define LCD_OFF 0

#define SPL_PART "spl"
#define LOGO_PART "logo"
#define CHARGER_LOGO_PART "chargelogo"
#define BOOT_PART "boot"
#define RECOVERY_PART "recovery"
#define FACTORY_PART "prodnv"
#define PRODUCTINFO_FILE_PATITION  "miscdata"
#define DT_PART "dt"

extern const char* g_env_bootmode;
extern char g_env_slot[3];
extern unsigned int g_charger_mode;

typedef enum {
	CMD_UNDEFINED_MODE=0,
	CMD_POWER_DOWN_DEVICE,
	CMD_NORMAL_MODE,
	CMD_DOWNLOAD_MODE,
	CMD_RECOVERY_MODE,
	CMD_FASTBOOT_MODE,
	CMD_ALARM_MODE,
	CMD_CHARGE_MODE,
	CMD_ENGTEST_MODE,
	CMD_WATCHDOG_REBOOT,
	CMD_AP_WATCHDOG_REBOOT,
	CMD_SPECIAL_MODE,
	CMD_UNKNOW_REBOOT_MODE,
	CMD_PANIC_REBOOT,
	CMD_VMM_PANIC_MODE,   //0xd
	CMD_TOS_PANIC_MODE,
	CMD_EXT_RSTN_REBOOT_MODE,
	CMD_CALIBRATION_MODE,
	CMD_USB_MUX_MODE,
	CMD_AUTODLOADER_REBOOT,
	CMD_AUTOTEST_MODE,
	CMD_IQ_REBOOT_MODE,
	CMD_SLEEP_MODE,
	CMD_SPRDISK_MODE,
	CMD_APKMMI_MODE,
	CMD_UPT_MODE,
	CMD_APKMMI_AUTO_MODE,
	CMD_ABNORMAL_REBOOT_MODE,
	CMD_SILENT_MODE,
	CMD_BOOTLOADER_PANIC_MODE,
	CMD_SML_PANIC_MODE,

	/*this is not a mode name ,beyond CMD_MAX_MODE means overflow*/
	CMD_MAX_MODE
}boot_mode_enum_type;

typedef enum {
	CMD_SET_FIRST_NORMAL_BOOT_MODE = 0,

	CMD_SET_FIRST_GSM_CLA_MODE,
	CMD_SET_FIRST_GSM_FINAL_TEST_MODE,

	CMD_SET_FIRST_WCDMA_CLA_MODE,
	CMD_SET_FIRST_WCDMA_FINAL_TEST_MODE,

	CMD_SET_FIRST_TDSCDMA_CLA_MODE,
	CMD_SET_FIRST_TDSCDMA_FINAL_TEST_MODE,

	CMD_SET_FIRST_LTETDD_CLA_MODE,
	CMD_SET_FIRST_LTETDD_FINAL_TEST_MODE,

	CMD_SET_FIRST_LTEFDD_CLA_MODE,
	CMD_SET_FIRST_LTEFDD_FINAL_TEST_MODE,

	CMD_SET_FIRST_NR5GSUB6G_CLA_MODE,
	CMD_SET_FIRST_NR5GSUB6G_FINAL_TEST_MODE,

	CMD_SET_FIRST_NRMMW_CLA_MODE,
	CMD_SET_FIRST_NRMMW_FINAL_TEST_MODE,

	CMD_SET_FIRST_CDMA2K_CLA_MODE,
	CMD_SET_FIRST_CDMA2K_FINAL_TEST_MODE,

	CMD_SET_FIRST_BBAT_MODE,

	CMD_SET_FIRST_NATIVE_MMI_MODE, /* MMI for feature phone */

	CMD_SET_FIRST_APK_MMI_MODE,/* apply for smartphone */

	CMD_SET_FIRST_NBIOT_CAL_MODE,
	CMD_SET_FIRST_NBIOT_FINAL_TEST_MODE,

	CMD_SET_FIRST_UPT_MODE,

	CMD_SET_FIRST_AUTOPON_MODE,/* auto power on */
	CMD_SET_FIRST_FASTBOOT_MODE,/* fastboot */

	CMD_SET_FIRST_APK_MMI_AUTO_MODE,
	CMD_SET_FIRST_AUTODLOADER_REBOOT_MODE,/* 0x1A */

	CMD_SET_FIRST_MAX_MODE
}set_first_mode_enum_type;


typedef struct first_boot_mode {
	u32 set_mode;
	u32 boot_mode;
	u32 cail_parameter;
} first_boot_mode_t;

/*==fixme secboot not ready start==*/
#define MAX_HASH_BITS_LEN (256)
#define MAX_HASH_BYTES_LEN (MAX_HASH_BITS_LEN>>3)

/*
typedef struct {
    u32  mMagicNum;        // "BTHD"=="0x42544844"=="boothead"
    u32  mVersion;         // 1
    u8   mPayloadHash[MAX_HASH_BYTES_LEN]; // sha256 hash value
    u64  mImgAddr;         // image loaded address
    u32  mImgSize;         // image size
    u32  is_packed;        // packed image flag 0:false 1:true
    u32  mFirmwareSize;    // runtime firmware size
    u32  ImgRealSize;      //image real size befor sign
    u32  SizeAfterSign;      //image all size after sign
    u8   reserved[444];    // 444 + 17*4 = 512
} sys_img_header;
*/
/*==fixme secboot not ready end==*/

#define PAC_VERSION_SIZE 256
#define PAC_VERSION_OFFSET (9 * 1024 + 512)

#define CHECK_BOOTMODE_FUN_NUM 15
typedef boot_mode_enum_type (*CBOOT_FUNC) (void);
typedef void (*CBOOT_MODE_ENTRY) (void);

#define MODE_REGIST( index, fun) \
    do{\
            array[index]  = fun;\
        }while(0)


//fixme-driver/rtc-not-ready
int sprd_is_poweroff_alarm(void);

int get_mode_from_file(void);
int set_recovery_run_fastbootd(void);
int clear_recovery_not_run_fastbootd(void);
int pctool_mode_detect(void);
int is_bat_low(void);
int alarm_flag_check(void);
int cali_file_check(void);
int read_adc_calibration_data(char *buffer,int size);
int pctool_mode_detect_uart(void);
int sprdisk_mode_detect(void);
int autodloader_mainhandler(void);
#ifdef CONFIG_AUTOLOAD_MODE
void autoload_mode(void);
#endif

void normal_mode(void);
void download_mode(void);
void recovery_mode(void);
void charge_mode(void);
void fastboot_mode(void);
void alarm_mode(void);
void engtest_mode(void);
void calibration_mode(void);
void watchdog_mode(void);
void ap_watchdog_mode(void);
void unknow_reboot_mode(void);
void special_mode(void);
void panic_reboot_mode(void);
void autodloader_mode(void);
void iq_mode(void);
void autotest_mode(void);
void sprdisk_mode(void);
void apkmmi_mode(void);
void upt_mode(void);
void apkmmi_auto_mode(void);
void abnormal_reboot_mode(void);
void silent_mode(void);
void bootloader_panic_reboot_mode(void);
void sml_panic_reboot_mode(void);

boot_mode_enum_type get_mode_from_chipram_env(void);
boot_mode_enum_type get_mode_from_arg(char* mode_name);
boot_mode_enum_type get_mode_from_pctool(void);
boot_mode_enum_type get_mode_from_smpl(void);
boot_mode_enum_type get_mode_from_bat_low(void);
boot_mode_enum_type write_sysdump_before_boot_extend(void);
boot_mode_enum_type get_mode_from_file_extend(void);
boot_mode_enum_type get_mode_from_watchdog(void);
boot_mode_enum_type get_mode_from_alarm_register(void);
boot_mode_enum_type get_mode_from_charger(void);
boot_mode_enum_type get_mode_from_keypad(void);
boot_mode_enum_type get_mode_from_gpio_extend(void);
boot_mode_enum_type get_mode_from_miscdata_boot_flag(void);
unsigned reboot_mode_check(void);
int sprd_boot(void);
void vlx_boot(const char * kernel_pname, int backlight_set, int lcd_enable);
#endif
