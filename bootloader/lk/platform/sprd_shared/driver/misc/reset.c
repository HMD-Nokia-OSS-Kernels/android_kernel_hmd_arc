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

#include <asm/arch/sprd_reg.h>
#include <sprd_common.h>
#include "sprd_wdt.h"
#include <asm/arch/check_reboot.h>
#include <sprd_log.h>

/*
 * Reset the cpu by setting up the watchdog timer and let it time out
 */

int (*arch_poweroff)(void) = NULL;
extern void rtc_clean_all_int(void);

/**
 * notifier_reset_register - register notifiers about reset.
 *	@n:		notifier call back
 *	@type:		register poweroff or reboot
 */
void reset_cpu(void)
{
	dprintf(INFO,"[%s]: rebooting\n", __func__);
	write_log_last();
	FTL_Panic_Flag_Write(0, LOG_RESERVED_ADDR);
	start_watchdog(5);
	while(1);
}

void system_reboot(u32 cmd)
{
	u32 reboot_mode = 0;

	ANA_REG_AND(ANA_REG_GLB_POR_RST_MONITOR, ~0xFF);

	reboot_mode = cmd & 0xFF;

	ANA_REG_OR(ANA_REG_GLB_POR_RST_MONITOR, reboot_mode);

	ANA_REG_OR(ANA_REG_GLB_SWRST_CTRL0, BIT_REG_RST_EN);
	ANA_REG_OR(ANA_REG_GLB_SOFT_RST_HW, BIT_REG_SOFT_RST);
	while (1);
}

void power_down_cpu(ulong ignored)
{
	int v = 0;
	v = ANA_REG_GET(ANA_REG_GLB_POR_SRC_FLAG);
	dprintf(INFO,"power on src = 0x%.8x\n", v);
	write_log_last();

	if(arch_poweroff) {
		arch_poweroff();
	}

	v = 0x1000000;
	while (v--); // log delay

	rtc_clean_all_int();
#if defined(CONFIG_ADIE_SC2723)
	sci_adi_set(ANA_REG_GLB_DCDC_SLP_CTRL0,  BIT_PWR_OFF_SEQ_EN); //auto poweroff by chip
#else
	sci_adi_write(ANA_REG_GLB_PWR_WR_PROT_VALUE,BITS_PWR_WR_PROT_VALUE(0x6e7F),BITS_PWR_WR_PROT_VALUE(~0));

	while (!(sci_adi_read(ANA_REG_GLB_PWR_WR_PROT_VALUE)&BIT_PWR_WR_PROT))
		dprintf(INFO, "powerdown wait key\n");

	sci_adi_clr(ANA_REG_GLB_SLP_CTRL, BIT_SLP_LDO_PD_EN | BIT_LDO_XTL_EN);
	sci_adi_set(ANA_REG_GLB_POWER_PD_HW,  BIT_PWR_OFF_SEQ_EN); //auto poweroff by chip
#endif

	while(1);
}
