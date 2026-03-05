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

#include "lk_sec_drv.h"
#include "tee_smc_call.h"
#include <string.h>
#include <kernel/spinlock.h>

spin_lock_t smc_call_lock = SPIN_LOCK_INITIAL_VALUE;
#ifdef CONFIG_SPRD_GICV3
#include <platform/timer.h>
#define SPRD_SMC_LOCK(state) spin_lock_saved_state_t state; spin_lock_irqsave(&smc_call_lock, state); platform_stop_timer()
#define SPRD_SMC_UNLOCK(state) spin_unlock_irqrestore(&smc_call_lock, state); platform_resume_timer()
#else
#define SPRD_SMC_LOCK(state) spin_lock(&smc_call_lock)
#define SPRD_SMC_UNLOCK(state) spin_unlock(&smc_call_lock)
#endif

static smc_param param;
void arch_smc_call(struct smc_param *param);

/**
start_addr must be 32 bits addr
there are two ways if you want to use 64 bits addr:
1. put 64 bits addr to your own structure, then pass structure's addr here
2. seperate 64 bits addr to two 4 bytes and then put them into param, then call arch_smc_call
*/
smc_param *tee_common_call(uint32_t funcid, uint32_t start_addr, uint32_t lenth)
{
	memset(&param, 0x00, sizeof(smc_param));
	param.a0 = TEESMC_SIPCALL_WITH_ARG;
	param.a1 = funcid;
	param.a2 = start_addr;
	param.a3 = lenth;
	SPRD_SMC_LOCK(state);
	arch_smc_call(&param);
	SPRD_SMC_UNLOCK(state);
	return &param;
}


int tee_smc_sip_call(uint32_t sip_call_id, uint32_t *a1, uint32_t *a2, uint32_t *a3)
{
	smc_param params;

	memset(&params, 0x00, sizeof(smc_param));
	params.a0 = TEESMC_SIP_CALL_ID(sip_call_id);
	params.a1 = *a1;
	params.a2 = *a2;
	params.a3 = *a3;
	SPRD_SMC_LOCK(state);
	arch_smc_call(&params);
	SPRD_SMC_UNLOCK(state);

	if(0 == params.a0) {
		*a1 = params.a1;
		*a2 = params.a2;
		*a3 = params.a3;
	} else {
		*a1 = 0;
		*a2 = 0;
		*a3 = 0;
	}

	return params.a0;
}

int tee_smc_fast_call(uint32_t fast_call_id, uint32_t paddr_l, uint32_t paddr_h, uint32_t size)
{
	smc_param params;

	memset(&param, 0x00, sizeof(smc_param));
	params.a0 = TEESMC_FAST_CALL_ID(fast_call_id);
	params.a1 = paddr_l;
	params.a2 = paddr_h;
	params.a3 = size;
	SPRD_SMC_LOCK(state);
	arch_smc_call(&params);
	SPRD_SMC_UNLOCK(state);

	return params.a0;
}
