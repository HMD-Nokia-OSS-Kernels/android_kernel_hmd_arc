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

#ifndef _CHECK_REBOOT_H_
#define _CHECK_REBOOT_H_

#include <sprd_battery.h>

#define   HWRST_STATUS_POWERON_MASK 		(0xf0)

/*
Definition of ANA_RST_STATUS(ANA_REG_GLB_POR_RST_MONITOR) register
bit[0..7]: Reboot mode
bit[9]: SYSDUMP enable/disable flag
*/

#define   HWRST_STATUS_BOOTLOADER_PANIC         (0x0010)
#define   HWRST_STATUS_RECOVERY 		(0x0020)
#define   HWRST_STATUS_FASTBOOT 		(0x0030)
#define   HWRST_STATUS_NORMAL 			(0x0040)
#define   HWRST_STATUS_ALARM 			(0x0050)
#define   HWRST_STATUS_SLEEP 			(0x0060)
#define   HWRST_STATUS_SPECIAL			(0x0070)
#define   HWRST_STATUS_CALIBRATION		(0x0090)
#define   HWRST_STATUS_PANIC			(0x0080)
#define   HWRST_STATUS_AUTODLOADER 		(0x00a0)
#define   HWRST_STATUS_IQMODE           	(0x00b0)
#define   HWRST_STATUS_SPRDISK			(0x00c0)
#define   HWRST_STATUS_SILENT			(0x00d0)
#define   HWRST_STATUS_FACTORYTEST		(0x00e0)
#define   HWRST_STATUS_NORMAL2			(0x00f0)
#define   HWRST_STATUS_NORMAL3			(0x00f1)
#define   HWRST_STATUS_SYSDUMP                  (0x0005)
#define   HWRST_STATUS_SYSDUMPEN		(0x200)

#define   HWRST_STATUS_SECURITY			(0x0002)
#define   HWRST_STATUS_SECBOOT			(0x0003)
#define   HWRST_STATUS_SML_PANIC                (0x0004)

#define   HW_PBINT2_STATUS				(0x8)
#define   HW_VCHG_STATUS				(0x20)
#define   HW_7SRST_STATUS				(0x80)
#define   SW_EXT_RSTN_STATUS			(0x800)
#define   SW_7SRST_STATUS				(0x1000)

#define   HWRST_RTCSTATUS_DOWNLOAD_BOOT	(0x55)
#define   HWRST_RTCSTATUS_DEFAULT		(0xA596)
unsigned check_reboot_mode(void);
void reset_to_normal(unsigned reboot_mode);
void reboot_devices(unsigned reboot_mode);
void system_reboot(unsigned cmd);
void power_down_devices(unsigned pd_cmd);
int power_button_pressed(void);
int alarm_triggered(void);
int fastboot_charger_connected(void);
extern void reset_cpu(void);

#endif

