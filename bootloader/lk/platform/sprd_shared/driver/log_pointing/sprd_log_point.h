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

#ifndef SPRD_LOG_POINT_H_
#define SPRD_LOG_POINT_H_

#include <asm/arch/sprd_reg.h>

#ifdef CONFIG_ENABLE_LOGPOINT

#define SPRD_AON_SYSFRT_BASE		    (AON_SYS_FRT_BASE)
#define LOG_POINT_SAVE_BASE             CONFIG_LOG_POINT_BASE


/* STAG ADDRESS DEFINATION */
#define FIRST_TS_BASE                   (LOG_POINT_SAVE_BASE)   /* 16 BYTE WRITE*/
#define FIRST_STEP_BASE                 (LOG_POINT_SAVE_BASE + 0x4)

#define LAST_TS_BASE                    (LOG_POINT_SAVE_BASE + 0x10)
#define LAST_STEPCORE0_BASE             (LOG_POINT_SAVE_BASE + 0x14)
#define LAST_TSCORE1_BASE               (LOG_POINT_SAVE_BASE + 0x18)
#define LAST_STEPCORE1_BASE             (LOG_POINT_SAVE_BASE + 0x1C)

#define SPT_PUBLIC_TS_BASE              (LOG_POINT_SAVE_BASE + 0x20)
#define SPT_TS_DELTA_BASE               (LOG_POINT_SAVE_BASE + 0x24)
#define SPT_PUBLIC_FLAG_BASE            (LOG_POINT_SAVE_BASE + 0x28)

#define SPT_CHAR_DESCRI_BASE            (LOG_POINT_SAVE_BASE + 0x30)
#define LOG_PONIT_LK_END                (LOG_POINT_SAVE_BASE + 0x40)
#define LK_BAKEUP_BASE					(LOG_POINT_SAVE_BASE + 0x40)

/* Stag num defination */
#define NUM_FIRST_STEP                  (0x1)

#ifdef CONFIG_SOC_ETB_PROJECT

/* SOC ETF register map */
#define SOC_ETF_BASE                    (SOC_CORESIGHT_ETF_BASE)
#define CS_LOCK_KEY_VALUE				(0xC5ACCE55)
#define ETB_LOCK_OFFSET_KEY             (SOC_ETF_BASE + 0xFB0)
#define ETB_ENABLE                      (SOC_ETF_BASE + 0x20)
#define ETB_RWP                         (SOC_ETF_BASE + 0x18)
#define ETB_RWD                         (SOC_ETF_BASE + 0x24)
#define ETB_RRP                         (SOC_ETF_BASE + 0x14)
#define ETB_RRD                         (SOC_ETF_BASE + 0x10)
#ifdef CONFIG_UMS9230_ETB
#define CHIPRAM_BAKEUP_BASE             (0x80)
#else
#define CHIPRAM_BAKEUP_BASE             (0x7C80)
#endif

#endif  /* CONFIG_SOC_ETB_PROJECT */

#endif  /* CONFIG_ENABLE_LOGPOINT */

typedef enum {
	PHASE_LK_ENTER=0,
	PHASE_ARCH_EARLY_INIT,
	PHASE_PLATFORM_EARLY_INIT,
	PHASE_HEAP_INIT,
	PHASE_KERNEL_INIT,
	PHASE_BOOTSTRAP2_ENTER,
	PHASE_TARGET_INIT,
	PHASE_POWER_DONE,
	PHASE_CLK_DONE,
	PHASE_BLOCK_DONE,
	PHASE_BATTERY_DONE,
	PHASE_LOG_PARTITION_DONE,
	PHASE_SPRDBOOT_INIT,
	PHASE_NORMAL_MODE=13,
	PHASE_FASTBOOT_MODE,
	PHASE_RECOVERY_MODE,
	PHASE_DOWNLOAD_MODE,
	PHASE_CHARGE_MODE,
	PHASE_CALIBRATION_MODE,
	PHASE_FULLDUMP_MODE,
	PHASE_MINIDUMP_MODE,
#if WITH_SMP
	PHASE_THRD0_PRELOAD=23,
	PHASE_THRD1_LCD,
	PHASE_THRD2_SECBOOT,
#else
	PHASE_LCD_INIT=24,
	PHASE_SECURE_INIT,
#endif
	PHASE_SECURE_AVB,
#if WITH_SMP
	PHASE_THRD3_KERNEL,
#else
	PHASE_KERNEL_LOAD,
#endif
	PHASE_KERNEL_DTBO,
	PHASE_KERNEL_RAMDISK,
	PHASE_PRE_KERNEL=30,
	PHASE_ENTER_KERNEL=31,
}point_phase;

/* function statement */
u32 FTL_Reboot_count(void);
void FTL_Savepoint_Private(point_phase laststep);
void FTL_Savepoint_Public(u32 public_num, char *str);
void FTL_Panic_Flag_Write(u32 flag, u32 addr);
extern volatile uint32_t g_last_step;
#endif //LOG_POINT_H_
