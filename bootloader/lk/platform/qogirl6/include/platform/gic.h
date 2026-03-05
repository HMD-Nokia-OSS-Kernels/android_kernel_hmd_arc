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

#ifndef __PLATFORM_GIC_H
#define __PLATFORM_GIC_H

#define MAX_INT 256
#define GIC_BASE_VIRT 0x10000000


#ifdef CONFIG_SPRD_GICV3
#define GICD_OFFSET	(0x00000)
#define GICD_SIZE	(0x10000)
#define GICD_BASE_VIRT	(GIC_BASE_VIRT + GICD_OFFSET)

#define GICA_OFFSET	(0x10000)
#define GICA_SIZE	(0x10000)
#define GICA_BASE_VIRT	(GIC_BASE_VIRT + GICA_OFFSET)

#define GICT_OFFSET	(0x20000)
#define GICT_SIZE	(0x10000)
#define GICT_BASE_VIRT	(GIC_BASE_VIRT + GICT_OFFSET)

#define GICP_OFFSET	(0x30000)
#define GICP_SIZE	(0x10000)
#define GICP_BASE_VIRT	(GIC_BASE_VIRT + GICP_OFFSET)

#define GICR_OFFSET	(0x40000 + 0x10000 * (CONFIG_SPRD_CICV3_ITSNUM) * 2)
#define GICR_SIZE	(0x10000 * (CONFIG_SPRD_GICV3_RDNUM) * 2)
#define GICR_BASE_VIRT	(GIC_BASE_VIRT + GICR_OFFSET)

#define GICDA_OFFSET	(GICR_OFFSET + GICR_SIZE)
#define GICDA_SIZE	(0x10000)
#define GICDA_BASE_VIRT	(GICDA_BASE_VIRT + GICDA_OFFSET)

#define GIC_SIZE	(GICD_SIZE + GICA_SIZE + GICT_SIZE + GICP_SIZE + GICR_SIZE + GICDA_SIZE)

#else
#define GICBASE(b) (GIC_BASE_VIRT)

#define GICC_SIZE (0x1000)
#define GICD_SIZE (0x1000)

#define GICC_OFFSET (0x0000)
#define GICD_OFFSET (GICC_SIZE)

#define GICC_BASE_VIRT (GIC_BASE_VIRT + GICC_OFFSET)
#define GICD_BASE_VIRT (GIC_BASE_VIRT + GICD_OFFSET)
#endif

#endif
