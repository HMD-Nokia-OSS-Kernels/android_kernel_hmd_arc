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

#include <secureboot/sec_efuse_qogirn6l_drv.h>

static inline void set_reg_bit(uint64 addr, uint32 bit)
{
	(*(volatile uint32 *)(addr)) |= 1<<bit;
}

static inline void clr_reg_bit(uint64 addr, uint32 bit)
{
	(*(volatile uint32 *)(addr)) &= BIT_MASK(bit);
}

static void msleep(uint32 ms)
{
	mdelay(ms);
}

static void efuse_lock(void)
{

}

static void efuse_unlock(void)
{

}

Efuse_Result_Ret efuse_write_drv(uint32 start_id, uint32 end_id, uint32 *pReadData,uint32 Isdouble)
{
	uint32  i, j;
	Efuse_Result_Ret ret = EFUSE_RESULT_SUCCESS;
	efuse_lock();
	efuse_enable();
	write32(EFUSE_MAGIC, EFUSE_SEC_MAGIC_NUM);			// SET magic number
	write32(ERR_FLAG_MASK, EFUSE_SEC_FLAG_CLR);			// CLR err flag
	set_reg_bit(EFUSE_SEC_EN, 4);					// SET lock bit
	set_reg_bit(EFUSE_PW_SWT, 2);					// SET PG_EN
	clr_reg_bit(EFUSE_PW_SWT, 1);					// CLR enk2
	msleep(1);							// SLP
	set_reg_bit(EFUSE_PW_SWT, 0);					// SET enk1
	msleep(1);							// SLP
	set_reg_bit(EFUSE_SEC_EN, 1);					// SET auto check
	if(Isdouble)
		set_reg_bit(EFUSE_SEC_EN, 2);				// SET doublt bit en
	else
		clr_reg_bit(EFUSE_SEC_EN, 2);				// CLR doublt bit en

	for (i = start_id, j = 0; i <= end_id; i++, j++)
	{
		write32(pReadData[j], BLOCK_MAP + (i << 2));
	}

	if (read32(EFUSE_SEC_ERR_FLAG) != 0)				// Read err register
	{
		ret = EFUSE_WR_ERROR;
	}

	clr_reg_bit(EFUSE_PW_SWT, 0);					// CLR enk1
	msleep(1);							// SLP
	set_reg_bit(EFUSE_PW_SWT, 1);					// SET enk2
	msleep(1);							// SLP
	clr_reg_bit(EFUSE_PW_SWT, 2);					// CLR PG_EN
	clr_reg_bit(EFUSE_SEC_EN, 4);					// CLR lock bit
	write32(0x0, EFUSE_SEC_MAGIC_NUM);				// CLR magic number
	efuse_disable();
	efuse_unlock();
	return ret;
}

Efuse_Result_Ret efuse_read_drv(uint32 start_id, uint32 end_id, uint32 *pReadData,uint32 Isdouble)
{
	uint32  i, j;
	Efuse_Result_Ret ret = EFUSE_RESULT_SUCCESS;
	efuse_lock();
	efuse_enable();
	write32(EFUSE_MAGIC, EFUSE_SEC_MAGIC_NUM);			// SET magic number
	write32(ERR_FLAG_MASK, EFUSE_SEC_FLAG_CLR);			// CLR err flag
	set_reg_bit(EFUSE_SEC_EN, 0);					// SET vdd on
	if (Isdouble)
		set_reg_bit(EFUSE_SEC_EN, 2);				// SET doublt bit en
	else
		clr_reg_bit(EFUSE_SEC_EN, 2);				// CLR doublt bit en

	for (i = start_id, j = 0; i <= end_id; i++, j++)
	{
		pReadData[j] = read32(BLOCK_MAP + (i << 2));
	}

	if (read32(EFUSE_SEC_ERR_FLAG) != 0)				// Read err register
	{
		ret = EFUSE_RD_ERROR;
	}

	clr_reg_bit(EFUSE_SEC_EN, 0);					// CLR vdd on
	write32(0x0, EFUSE_SEC_MAGIC_NUM);				// CLR magic number
  	efuse_disable();
	efuse_unlock();
	return ret;
}

Efuse_Result_Ret efuse_rewrite_double_bit(uint32 block_id, uint32 writeData)
{
	Efuse_Result_Ret ret = EFUSE_RESULT_SUCCESS;
	efuse_lock();
	efuse_enable();
	write32(EFUSE_MAGIC, EFUSE_SEC_MAGIC_NUM);			// SET magic number
	write32(ERR_FLAG_MASK, EFUSE_SEC_FLAG_CLR);			// CLR err flag
	set_reg_bit(EFUSE_PW_SWT, 2);					// SET PG_EN
	clr_reg_bit(EFUSE_PW_SWT, 1);					// CLR enk2
	msleep(1);							// SLP
	set_reg_bit(EFUSE_PW_SWT, 0);					// SET enk1
	msleep(1);							// SLP
	clr_reg_bit(EFUSE_SEC_EN, 1);					// clear auto check
	set_reg_bit(EFUSE_SEC_EN, 2);					// SET doublt bit en

	write32(writeData, BLOCK_MAP + (block_id << 2));

	if (read32(EFUSE_SEC_ERR_FLAG) != 0)				// Read err register
	{
		ret = EFUSE_WR_ERROR;
	}

	clr_reg_bit(EFUSE_PW_SWT, 0);					// CLR enk1
	msleep(1);							// SLP
	set_reg_bit(EFUSE_PW_SWT, 1);					// SET enk2
	msleep(1);							// SLP
	clr_reg_bit(EFUSE_PW_SWT, 2);					// CLR PG_EN
	write32(0x0, EFUSE_SEC_MAGIC_NUM);				// CLR magic number
	efuse_disable();
	efuse_unlock();
	return ret;
}

Efuse_Result_Ret efuse_huk_program(void)
{
	Efuse_Result_Ret ret = EFUSE_RESULT_SUCCESS;
	uint32_t val = 0;

	efuse_lock();
	efuse_enable();
	write32(EFUSE_MAGIC, EFUSE_SEC_MAGIC_NUM);			// SET magic number
	write32(ERR_FLAG_MASK, EFUSE_SEC_FLAG_CLR);			// CLR err flag
	set_reg_bit(EFUSE_SEC_EN, 4);					// SET lock bit
	set_reg_bit(EFUSE_PW_SWT, 2);					// SET PG_EN
	clr_reg_bit(EFUSE_PW_SWT, 1);					// CLR enk2
	msleep(1);							// SLP
	set_reg_bit(EFUSE_PW_SWT, 0);					// SET enk1
	msleep(1);							// SLP
	set_reg_bit(EFUSE_SEC_EN, 1);					// SET auto check
	set_reg_bit(EFUSE_SEC_EN, 2);					// SET doublt bit en

	set_reg_bit(CE_SEC_EB,30);					// enable ce module

	write32(0x20, CE_CLK_EB_REG);					// enable trng clk
	write32(0x02, CE_RNG_EB_REG);					// enable trng ring
	write32(0x03, CE_RNG_EB_REG);					// enable trng module
	val = read32(CE_HUK_KEY_CONFIG);
	val |= 0x640000;
	write32(val, CE_HUK_KEY_CONFIG);

	//enable ce secure key trng write
	set_reg_bit(CE_SEC_KEY_USE_WAY_REG, 31);

	while (((*(volatile uint32*)(CE_SEC_KEY_USE_WAY_REG)) & (0x1 << 31)) != 0x0);
									// Poll for write huk finish
	write32(0x0, CE_RNG_EB_REG);					// disable trng ring
	write32(0x0, CE_CLK_EB_REG);					// disable trng clk
	clr_reg_bit(CE_SEC_EB,30); 					// disable ce module

	if (read32(EFUSE_SEC_ERR_FLAG) != 0)				// Read err register
	{
		ret = EFUSE_WR_ERROR;
	}

	clr_reg_bit(EFUSE_PW_SWT, 0);					// CLR enk1
	msleep(1);							// SLP
	set_reg_bit(EFUSE_PW_SWT, 1);					// SET enk2
	msleep(1);							// SLP
	clr_reg_bit(EFUSE_PW_SWT, 2);					// CLR PG_EN
	clr_reg_bit(EFUSE_SEC_EN, 4);					// CLR lock bit
	write32(0x0, EFUSE_SEC_MAGIC_NUM);				// CLR magic number
	efuse_disable();
	efuse_unlock();
	return ret;
}

void efuse_enable(void)
{
	set_reg_bit(REG_AON_SEC_APB_EFUSE_SEC_ENABLE, 0);		//enable sec efuse
	write32(ERR_FLAG_MASK, EFUSE_SEC_FLAG_CLR);			// CLR err flag
	set_reg_bit(EFUSE_SEC_EN, 0);					// SET vdd on
	set_reg_bit(EFUSE_SEC_EN, 2);					// SET doublt bit en
}

void efuse_disable(void)
{
	clr_reg_bit(EFUSE_SEC_EN, 0);					// CLR vdd on
	clr_reg_bit(REG_AON_SEC_APB_EFUSE_SEC_ENABLE, 0);		//disable sec efuse
}


