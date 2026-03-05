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

#ifndef __DEV_INTERRUPT_ARM_GICV3_H
#define __DEV_INTERRUPT_ARM_GICV3_H

#include <sys/types.h>

void arm_gic_init(void);

enum {
	/* Ignore cpu_mask and forward interrupt to all CPUs other than the current cpu */
	ARM_GIC_SGI_FLAG_TARGET_FILTER_NOT_SENDER = 0x1,
	/* Ignore cpu_mask and forward interrupt to current CPU only */
	ARM_GIC_SGI_FLAG_TARGET_FILTER_SENDER = 0x2,
	ARM_GIC_SGI_FLAG_TARGET_FILTER_MASK = 0x3,

	/* Only forward the interrupt to CPUs that has the interrupt configured as group 1 (non-secure) */
	ARM_GIC_SGI_FLAG_NS = 0x4,
};

status_t arm_gic_sgi(u_int irq, u_int flags, u_int cpu_mask);

#if WITH_LIB_SM
void arm_gic_terminal (void);
#endif

#endif

