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

#include <sprd_common.h>
#include <errno.h>
#include <sprd_glb.h>
#include <asm/arch/sprd_reg.h>
#include <asm/arch/common.h>

typedef unsigned long long  uint64;

/******************/
#define SEC_EFUSE_MAGIC					0x8810
#define SEC_ERR_FLAG_MASK				0xFFFF
#define SEC_BIT_MASK(bit)  				(~(1<<bit))

/*******EFUSE CONTROL REGISTER***************************/
#define REG_AON_EFUSE_BASE			SPRD_UIDEFUSE_PHYS
#define EFUSE_ALL0_INDEX			(REG_AON_EFUSE_BASE + 0x8)
#define EFUSE_MODE_CTRL				(REG_AON_EFUSE_BASE + 0xC)
#define	EFUSE_IP_VER				(REG_AON_EFUSE_BASE + 0x14)
#define	EFUSE_CFG0					(REG_AON_EFUSE_BASE + 0x18)
#define	EFUSE_SEC_EN				(REG_AON_EFUSE_BASE + 0x40)
#define	EFUSE_SEC_ERR_FLAG			(REG_AON_EFUSE_BASE + 0x44)
#define	EFUSE_SEC_FLAG_CLR			(REG_AON_EFUSE_BASE + 0x48)
#define	EFUSE_SEC_MAGIC_NUM			(REG_AON_EFUSE_BASE + 0x4C)
#define	EFUSE_FW_CFG				(REG_AON_EFUSE_BASE + 0x50)
#define	EFUSE_PW_SWT				(REG_AON_EFUSE_BASE + 0x54)
#define BLOCK_MAP					(REG_AON_EFUSE_BASE + 0x1000)

static inline void set_reg_bit(uint64 addr, u32 bit)
{
	(*(volatile u32 *)(addr)) |= 1<<bit;
}

static inline void clr_reg_bit(uint64 addr, u32 bit)
{
	(*(volatile u32 *)(addr)) &= SEC_BIT_MASK(bit);
}


static void efuse_lock(void)
{

}

static void efuse_unlock(void)
{

}

int sprd_secure_efuse_read(u32 start_id, u32 end_id, u32 *pReadData,u32 Isdouble)
{
	u32  i, j;
	int ret = 0;
	efuse_lock();

	write32(SEC_EFUSE_MAGIC, EFUSE_SEC_MAGIC_NUM);		// SET magic number
	write32(SEC_ERR_FLAG_MASK, EFUSE_SEC_FLAG_CLR);		// CLR err flag
	set_reg_bit(EFUSE_SEC_EN, 0);					// SET vdd on
	if(Isdouble)
		set_reg_bit(EFUSE_SEC_EN, 2);				// SET doublt bit en
	else
		clr_reg_bit(EFUSE_SEC_EN, 2);				// CLR doublt bit en

	for (i = start_id, j = 0; i <= end_id; i++, j++)
	{
		pReadData[j] = read32(BLOCK_MAP + (i << 2));
	}

	if (read32(EFUSE_SEC_ERR_FLAG) != 0)			// Read err register
	{
		ret = 1;
		debugf("secure efuse err = %x start_id = %d end_id = %d\n", read32(EFUSE_SEC_ERR_FLAG), start_id, end_id);
	}

	clr_reg_bit(EFUSE_SEC_EN, 0);					// CLR vdd on
	write32(0X0, EFUSE_SEC_MAGIC_NUM);				// CLR magic number
	efuse_unlock();

	return ret;
}
