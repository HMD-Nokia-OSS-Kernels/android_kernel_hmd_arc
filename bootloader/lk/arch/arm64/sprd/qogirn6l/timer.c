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

#include <sprd_div64.h>
#include <linux/types.h>
#include "asm/arch/common.h"
#include <lk/debug.h>
#include <arch/arm64.h>
#include "asm/arch/sys_timer_reg_v0.h"
#include "asm/arch/timer_reg_r4p0.h"
#include <lk/reg.h>

#define TIMER_MAX_VALUE			0xFFFFFFFF
#define GENERAL_CNT_VAL			(26)
#define TMR0_CNT_VAL			(32)
#define TMR1_CNT_VAL			(32)
#define TMR2_CNT_VAL			(26)

static unsigned long long timestamp;
static ulong lastinc;

#if WITH_SMP
void secondary_timer_init(void)
{
	ARM64_WRITE_SYSREG(cntp_ctl_el0, 0);
	ARM64_WRITE_SYSREG(cntp_tval_el0, TIMER_MAX_VALUE);
	ARM64_WRITE_SYSREG(cntp_ctl_el0, 0x3);
}
#endif

/* nothing really to do with interrupts, just starts up a counter. */
/* The 32KHz 23-bit timer overruns in 512 seconds */
int sprd_timer_init(void)
{
	/* AP SYS TMR and AP TMR0/1/2 enable */
	writel(readl(REG_AON_APB_APB_EB1) | BIT_AON_APB_AP_SYST_EB,
				REG_AON_APB_APB_EB1);
	writel(readl(REG_AON_APB_APB_RTC_EB) | BIT_AON_APB_AP_SYST_RTC_EB,
				REG_AON_APB_APB_RTC_EB);

	writel(readl(REG_AON_APB_APB_EB2) | BIT_AON_APB_AP_TMR0_EB |
				BIT_AON_APB_AP_TMR1_EB | BIT_AON_APB_AP_TMR2_EB,
				REG_AON_APB_APB_EB2);
	writel(readl(REG_AON_APB_APB_RTC_EB) | BIT_AON_APB_AP_TMR0_RTC_EB |
				BIT_AON_APB_AP_TMR1_RTC_EB | BIT_AON_APB_AP_TMR2_RTC_EB,
				REG_AON_APB_APB_RTC_EB);

	/* AON SYS TMR and AON TMR enable */
	writel(readl(REG_AON_APB_APB_EB1) | BIT_AON_APB_AON_SYST_EB,
				REG_AON_APB_APB_EB1);
	writel(readl(REG_AON_APB_APB_RTC_EB) | BIT_AON_APB_AON_SYST_RTC_EB,
				REG_AON_APB_APB_RTC_EB);

	writel(readl(REG_AON_APB_APB_EB1) | BIT_AON_APB_AON_TMR_EB,
				REG_AON_APB_APB_EB1);
	writel(readl(REG_AON_APB_APB_RTC_EB) | BIT_AON_APB_AON_TMR_RTC_EB,
				REG_AON_APB_APB_RTC_EB);

	/* enable ts clk output to arch timer */
	writel(0x1, SPRD_APCPUTS0_BASE);
#if WITH_SMP
	secondary_timer_init();
#endif
	return 0;
}

/*
 * "time" is measured in 1 / CONFIG_SYS_HZ seconds,
 * "tick" is internal timer period
 */
static inline unsigned long long tick_to_time(unsigned long long tick)
{
	tick *= CONFIG_SYS_HZ;
	do_div(tick, CONFIG_SPRD_TIMER_CLK);
	return tick;
}

static inline unsigned long long time_to_tick(unsigned long long time)
{
	time *= CONFIG_SPRD_TIMER_CLK;
	do_div(time, CONFIG_SYS_HZ);
	return time;
}

static inline unsigned long long us_to_tick(unsigned long long us)
{
	us = us*CONFIG_SPRD_TIMER_CLK;
	do_div(us, CONFIG_SYS_HZ*1000);
	return us;
}

void reset_timer(void)
{
	//capture current incrementer value time
	lastinc = readl(SYS_VLAUE_SHDW);
	timestamp = 0;
}
void reset_timer_masked(void)
{
	reset_timer();
}
unsigned long long get_ticks(void)
{
	ulong now = readl(SYS_VLAUE_SHDW);

	if(now >= lastinc) {
	/* not roll
	 * move stamp forward with absolut diff ticks
	 * */
		timestamp += (now - lastinc);
	}else{
		//timer roll over
		timestamp += (TIMER_MAX_VALUE - lastinc) + now;
	}
	lastinc = now;
	return timestamp;
}

ulong get_timer_masked(void)
{
	return get_ticks();
}

ulong get_timer(ulong base)
{
	return get_timer_masked() - base;
}

void set_timer(ulong t)
{
	timestamp = time_to_tick(t);
}

#if WITH_SMP
void __udelay (unsigned long usec)
{
	unsigned long delta_usec = usec * GENERAL_CNT_VAL;

	ARM64_WRITE_SYSREG(cntp_tval_el0, delta_usec);
	while(ARM64_READ_SYSREG(cntp_cval_el0) > ARM64_READ_SYSREG(cntpct_el0));
}
#else
void __udelay (unsigned long usec)
{
	unsigned long delta_usec = usec * TMR2_CNT_VAL;

	writel(delta_usec, TM2_LOAD_LO);
	writel(readl(TM2_CTL) | BIT_TIMER_RUN,TM2_CTL);
	while(readl(TM2_SHDW_LO) > 0);
	writel(readl(TM2_CTL) & (~BIT_TIMER_RUN),TM2_CTL);
}
#endif

void tmr_udelay (unsigned long usec)
{
	unsigned long delta_usec = usec * TMR2_CNT_VAL;

	writel(delta_usec, TM2_LOAD_LO);
	writel(readl(TM2_CTL) | BIT_TIMER_RUN,TM2_CTL);
	while(readl(TM2_SHDW_LO) > 0);
	writel(readl(TM2_CTL) & (~BIT_TIMER_RUN),TM2_CTL);
}

void tmr_mdelay (unsigned long msec)
{
	unsigned long delta_usec = msec * TMR0_CNT_VAL;

	writel(delta_usec, TM0_LOAD_LO);
	writel(readl(TM0_CTL) | BIT_TIMER_RUN,TM0_CTL);
	while(readl(TM0_SHDW_LO) > 0);
	writel(readl(TM0_CTL) & (~BIT_TIMER_RUN),TM0_CTL);
}
u32 SCI_GetTickCount(void)
{
#ifndef CONFIG_ZEBU
	volatile u32 tmp_tick1;
	volatile u32 tmp_tick2;

	tmp_tick1 = SYSTEM_CURRENT_CLOCK;
	tmp_tick2 = SYSTEM_CURRENT_CLOCK;

	while (tmp_tick1 != tmp_tick2)
	{
		tmp_tick1 = tmp_tick2;
		tmp_tick2 = SYSTEM_CURRENT_CLOCK;
	}
	return tmp_tick1;
#else
	return 0;
#endif
}
#ifndef CONFIG_WD_PERIOD
#define CONFIG_WD_PERIOD	(10 * 1000 * 1000)	/* 10 seconds default */
#endif

void udelay(unsigned long usec)
{
	ulong kv;

	do {
		kv = usec > CONFIG_WD_PERIOD ? CONFIG_WD_PERIOD : usec;
		__udelay (kv);
		usec -= kv;
	} while(usec);
}

void mdelay(unsigned long msec)
{
	while (msec--)
		udelay(1000);
}

