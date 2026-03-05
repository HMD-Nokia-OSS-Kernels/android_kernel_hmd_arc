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

/*
 *  sprd_glb.c - Support register operation interface
 *
 *  History:
 *      2021/7/29 porter.xu@unisoc.com
 *      Add register operation interface
 */

#include <sprd_common.h>
#include <errno.h>
#include <asm/arch/common.h>
#include <asm/types.h>

u32 sci_glb_read(u32 reg, u32 msk)
{
	return readl(reg) & msk;
}

int sci_glb_write(u32 reg, u32 val, u32 msk)
{
	writel((readl(reg) & ~msk) | val, reg);
	return 0;
}

int sci_glb_set(u32 reg, u32 bit)
{
	writel(readl(reg) | bit, reg);
	return 0;
}

int sci_glb_clr(u32 reg, u32 bit)
{

	writel((readl(reg) & ~bit), reg);
	return 0;
}
