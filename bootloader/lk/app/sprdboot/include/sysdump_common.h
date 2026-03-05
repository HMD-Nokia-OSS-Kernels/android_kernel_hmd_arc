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

#include <boot_mode.h>
#include <sprd_log.h>

#define SYSDUMPDB_LOG_TAG "sysdumpdb"

/* NOTE: the array need be updated on the basis of 'boot_mode_enum_type' */
static const char *rstmode[CMD_MAX_MODE] = {
	"undefind mode",                        //CMD_UNDEFINED_MODE=0,
	"power down",                           //CMD_POWER_DOWN_DEVICE,
	"normal",                               //CMD_NORMAL_MODE,
	"download",                             //CMD_DOWNLOAD_MODE,
	"recovery",                             //CMD_RECOVERY_MODE,
	"fastboot",                             //CMD_FASTBOOT_MODE,
	"alarm",                                //CMD_ALARM_MODE,
	"charge",                               //CMD_CHARGE_MODE,
	"engtest",                              //CMD_ENGTEST_MODE,
	"cm4_watchdog_timeout",                 //CMD_WATCHDOG_REBOOT,
	"ap_watchdog_timeout",                  //CMD_AP_WATCHDOG_REBOOT,
	"framework crash",                      //CMD_SPECIAL_MODE,
	"manual_dump",                          //CMD_UNKNOW_REBOOT_MODE,
	"kernel_crash",                         //CMD_PANIC_REBOOT,
	"vmm_panic",                            //CMD_VMM_PANIC_MODE,
	"tos_panic",                            //CMD_TOS_PANIC_MODE,
	"ext rstn reboot",                      //CMD_EXT_RSTN_REBOOT_MODE,
	"calibration",                          //CMD_CALIBRATION_MODE,
	"usb mux",                              //CMD_USB_MUX_MODE,
	"autodloader",                          //CMD_AUTODLOADER_REBOOT,
	"autotest",                             //CMD_AUTOTEST_MODE,
	"iq reboot",                            //CMD_IQ_REBOOT_MODE,
	"sleep",                                //CMD_SLEEP_MODE,
	"sprd disk",                            //CMD_SPRDISK_MODE,
	"apk mmi",                              //CMD_APKMMI_MODE,
	"upt",                                  //CMD_UPT_MODE,
	"apkmmi auto",                          //CMD_APKMMI_AUTO_MODE,
	"abnormal mode",                        //CMD_ABNORMAL_REBOOT_MODE,
	"silent",                               //CMD_SILENT_MODE,
	"bootloader panic",                      //CMD_BOOTLOADER_PANIC_MODE
	"sml panic",				//CMD_SML_PANIC_MODE,
};

extern int is_sysdump_boot_mode(int rst_mode);
#define GET_RST_MODE(x) rstmode[(x) < CMD_MAX_MODE ? (x) : CMD_UNDEFINED_MODE]

/* dump_flag bits introduce */
#define BIT(x)                          (1 << x)
#define ORIG_STATUS                     BIT(0)
#define AP_FULL_DUMP_ENABLE             BIT(1)
#define AP_MINI_DUMP_ENABLE             BIT(2)
#define AP_FULLDUMP_INTERNAL            BIT(4)
#define BOOT_FROM_DUMP_STATUS           BIT(9)
#define DUMP_FINISH_AUTO_REBOOT         BIT(15)
#define SIZEOF_UNSIGNED_LONG            BIT(16)
#define IS_STRUCT_PACKET                BIT(17)
#define SYSDUMP_STATUS_MASK             0xffffff00      /* init sysdump status in low bits every time but record high bits value */
#define IS_ORIG_STATUS(x)               (x & BIT(0) ? 0: 1)   /* x & BIT(0) = 1  means not orig status . or the value must be 0 .*/
#define IS_BOOT_FROM_DUMP(x)            (x & BIT(9) ? 1: 0)   /* x & BIT(3) = 1  means boot from dump. Not boot from the value must be 0 .*/
#define IS_FULLDUMP_INTERNAL(x)         (x & BIT(4) ? 1: 0)   /* x & BIT(4) = 1  means fulldump internal supoort */
#define IS_DUMPFINISH_AUTOREBOOT(x)     (x & BIT(15) ? 1: 0)   /* x & BIT(15) = 1  means need auto reboot when dump finish. */
#define SYSDUMPDB_PARTITION_DUMP_FLAG_OFFSET    (16) /* bit 16 ~23  'Y' */

/* the struct to save minidump info description */
struct info_desc{
	unsigned long long paddr;
	int size;
};

/* the struct to save dump header info */
struct dumpdb_header{
	char uboot_magic[4];            /* for uboot lable,type: "U2.0" ,means uboot saved minidump data*/
	char app_magic[4];              /* for app lable,type:"A2.0" ,means app read minidump description ok */
	int  dump_flag;
	int  reset_mode;     /*record which reset mode  enter sysdump*/
	struct info_desc minidump_info_desc;
};

extern void record_dumpdb_header_status(struct dumpdb_header *header_g);

#define DUMPINFO_FILE_SIZE (4 * 1024)

#define DUMP_LOG_TAG    "sprd_minidump"

#define dump_logd(fmt, args...)  do { dprintf(INFO, "(%s): ", DUMP_LOG_TAG); dprintf(INFO, fmt, ##args); } while (0)
#define dump_loge(fmt, args...)  do { dprintf(CRITICAL, "(%s): ", DUMP_LOG_TAG); dprintf(CRITICAL, fmt, ##args); } while (0)
#define dump_loga(fmt, args...)  do { dprintf(ALWAYS, "(%s): ", DUMP_LOG_TAG); dprintf(ALWAYS,fmt, ##args);} while (0)
