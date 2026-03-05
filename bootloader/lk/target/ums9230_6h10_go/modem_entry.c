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
#include <asm/arch/common.h>
#include <asm/arch/sprd_reg.h>

#include "cp_boot.h"

/*add macro for memset agdsp/tddsp/ldsp share memory*/
#define DSP_BOOT_IRAM_START   0x65004400//2k
#define LDSP_SHARE_MEM_START  0x65004c00//10k
#define TGDSP_SHARE_MEM_START 0x65007400//4k
#define MULTI_MODE_MEM_START  0x65008400//4k

/* not sure tyc */
/* audio cp aon iram, and ddr memory for communication */
#define AUDCP_AON_IRAM_START 0x65009400
/* size 4k */
#define AUDCP_AON_IRAM_SIZE 0x1000

#define AUDCP_DDR_COMMU_START 0x94380000
#define AUDCP_DDR_COMMU_SIZE 0xe10


/*****************************************************************************/
//  Description:    Gets the current reset mode.
//  Author:         Andrew.Yang
//  Note:
/*****************************************************************************/
#ifdef CONFIG_ARM7_RAM_ACTIVE
void pmic_arm7_RAM_active(void)
{
	*((volatile u32*)REG_AON_APB_CM4_SYS_SOFT_RST) |= BIT_AON_APB_CM4_CORE_SOFT_RST;
	msleep(50);
	*((volatile u32*)REG_PMU_APB_SYS_SOFT_RST_1) &= ~BIT_PMU_APB_SP_SOFT_RST;
	*((volatile u32*)REG_PMU_APB_FORCE_DEEP_SLEEP_CFG_0) &= ~BIT_PMU_APB_SP_FORCE_DEEP_SLEEP_REG;   /*clear sp force sleep */
	*((volatile u32*)REG_AON_APB_CM4_SYS_SOFT_RST) &= ~BIT_AON_APB_CM4_SYS_SOFT_RST;
	msleep(50);
}
#endif

/*bug:533275  memset tg/l/agdsp share memory
root cause: In agdsp share IRAM memory ,this area exits command for agdsp,when interunpt happens
from ap or cp,the agdsp will use this area. While the area exit the dirty binary data ,ap or cp
do not clean up this area when interrupt happens at the right time,so error command will cause
unexcepted problems.*/

void memset_dsp_share_memory(void)
{
        memset((void *)DSP_BOOT_IRAM_START,0x0,0x800);//2k
        memset((void *)LDSP_SHARE_MEM_START,0x0,0x2800);//10k
        memset((void *)TGDSP_SHARE_MEM_START,0x0,0x1000);//4k
        memset((void *)MULTI_MODE_MEM_START,0x0,0x1000);//4k
}

void memset_audcp_share_memory(void)
{
	/* audio cp aon iram 4k */
	memset((void *)AUDCP_AON_IRAM_START,0x0,AUDCP_AON_IRAM_SIZE);
	/* audio cp ddr 3.5k+16 bytes */
	memset((void *)AUDCP_DDR_COMMU_START,0x0,AUDCP_DDR_COMMU_SIZE);
}

void audcp_boot_header(u32 boot_vector)
{
	if (!memcmp(boot_vector, AUDCP_HEADER_STR, strlen(AUDCP_HEADER_STR)))
	{
		audcp_boot(((boot_vector+0x80)/2));
		debugf("audio cp boot ok!\n");
	}
	else
	{
		debugf("audio cp boot fail, can't find AUDCP section header!\n");

	}



}
void modem_entry(void)
{

	memset_dsp_share_memory();
	memset_audcp_share_memory();
	audcp_boot_header(LTE_AGDSP_ADDR);

#ifndef CONFIG_KERNEL_BOOT_CP
        sp_boot();
        pubcp_boot();
        debugf("boot CP1 OK\n");
#else
        extern unsigned int g_charger_mode;
        if(g_charger_mode) {
#ifndef PROJECT_SEC_CM4
                sp_boot();
#endif
                debugf("boot sp OK\n");
        }
#endif
}

