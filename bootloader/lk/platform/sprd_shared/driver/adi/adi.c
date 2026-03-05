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

#include <sys/types.h>
#include <stdio.h>
#include <rand.h>
#include <lk/err.h>
#include <stdlib.h>
#include <string.h>
#include <asm/arch/sprd_reg.h>
#include <asm/arch/common.h>
#include <sprd_common.h>
#include <kernel/spinlock.h>

volatile spin_lock_t sprd_adi_lock = SPIN_LOCK_INITIAL_VALUE;
#ifdef CONFIG_SPRD_GICV3
#define SPRD_ADI_LOCK(state) spin_lock_saved_state_t state; spin_lock_irqsave(&sprd_adi_lock, state)
#define SPRD_ADI_UNLOCK(state) spin_unlock_irqrestore(&sprd_adi_lock, state)
#else
#define SPRD_ADI_LOCK(state) sprd_spin_lock(&sprd_adi_lock)
#define SPRD_ADI_UNLOCK(state) sprd_spin_unlock(&sprd_adi_lock)
#endif


#ifdef SPRD_MISC_BASE
#undef SPRD_MISC_BASE
#endif
#define SPRD_MISC_BASE SPRD_MISC_PHYS

/* registers definitions for controller CTL_ADI */
#define REG_ADI_CTRL0					(SPRD_MISC_BASE + 0x04)

#define REG_ADI_CHNL_PRIL				(SPRD_MISC_PHYS + 0x08)
#define REG_ADI_CHNL_PRIH				(SPRD_MISC_PHYS + 0x0C)
#define REG_ADI_INT_RAW					(SPRD_MISC_PHYS + 0x14)
#define REG_ADI_RD_CMD					(SPRD_MISC_PHYS + 0x28)
#define REG_ADI_RD_DATA					(SPRD_MISC_PHYS + 0x2C)
#define REG_ADI_FIFO_STS				(SPRD_MISC_PHYS + 0x30)
#define REG_ADI_GSSI_CFG0                               (SPRD_MISC_PHYS + 0x20)
#define REG_ADI_GSSI_CFG1                               (SPRD_MISC_PHYS + 0x24)
#define REG_ADI_CHNL_EN0				(SPRD_MISC_PHYS + 0x40)
#define REG_ADI_CHNL_EN1				(SPRD_MISC_PHYS + 0x20c)
#define REG_ADI_CHNL_ADDR(id)				(SPRD_MISC_PHYS + 0x44 + (id - 2) * 4)
#define HW_CHN_CNT					50

/* bits definitions for register REG_ADI_CTRL0 */
#define BIT_ARM_SCLK_EN                 ( BIT_1 )
#define BITS_CMMB_WR_PRI			( (1) << 4 & (BIT_4|BIT_5) )

/* bits definitions for register REG_ADI_CHNL_PRI */
#define BITS_PD_WR_PRI             ( (1) << 14 & (BIT_14|BIT_15) )
#define BITS_RFT_WR_PRI       	   ( (1) << 12 & (BIT_12|BIT_13) )
#define BITS_DSP_RD_PRI            ( (2) << 10 & (BIT_10|BIT_11) )
#define BITS_DSP_WR_PRI            ( (2) << 8 & (BIT_8|BIT_9) )
#define BITS_ARM_RD_PRI            ( (3) << 6 & (BIT_6|BIT_7) )
#define BITS_ARM_WR_PRI            ( (3) << 4 & (BIT_4|BIT_5) )
#define BITS_STC_WR_PRI            ( (1) << 2 & (BIT_2|BIT_3) )
#define BITS_INT_STEAL_PRI         ( (3) << 0 & (BIT_0|BIT_1) )

/* bits definitions for register REG_ADI_RD_DATA */
#define BIT_RD_CMD_BUSY                 ( BIT_31 )
#define SHIFT_RD_ADDR                   ( 16 )

#define SHIFT_RD_VALU                   ( 0 )
#define MASK_RD_VALU                    ( 0xFFFF )

/* bits definitions for register REG_ADI_FIFO_STS */
#define BIT_ARM_WR_FREQ                 ( BIT_31 )
#define BIT_FIFO_FULL                   ( BIT_11 )
#define FIFO_IS_FULL()	(readl(REG_ADI_FIFO_STS) & BIT_FIFO_FULL)

#define BIT_FIFO_EMPTY                  ( BIT_10 )

/* special V1 (sc8830 soc) defined */
/* bits definitions for register REG_ADI_CTRL0 */
#define BIT_ADI_WR(_X_)                 ( (_X_) << 2 )
#define BITS_ADDR_BYTE_SEL(_X_)			( (_X_) << 0 & (BIT_0|BIT_1) )

/* bits definitions for register REG_ADI_CHNL_PRI */
#define VALUE_CH_PRI	(0x0)
#define ADI_WRITE_TIMEOUT_MS		(2000)
/* soc defined end*/

static u32 readback_addr_mak = 0;
static u32 readback_offset = 0;
static u8 is_ana_init = 0;

#if defined(CONFIG_NAND_SPL) || defined(CONFIG_FDL1)
#define panic(x...) do{}while(0)
#define printf(x...) do{}while(0)
#define udelay(x)	\
	do { \
		volatile int i; \
		int cnt = 1000 * x; \
		for (i=0; i<cnt; i++);\
	} while(0);
#endif

/*FIXME: Now define adi IP version, sc8825 is zero, sc8830 is one,
* Adi need init early that than read soc id, now using this ARCH dependency.
*/
static inline int __adi_ver(void)
{
	return 1;
}

#define	TO_ADDR(_x_)		( ((_x_) >> SHIFT_RD_ADDR) & readback_addr_mak )

static inline int __adi_fifo_drain(void)
{
#if defined(CONFIG_FPGA)
	return 0;
#else
	int cnt = 1000;
	while (!(readl(REG_ADI_FIFO_STS) & BIT_FIFO_EMPTY) && cnt--) {
		udelay(1);
	}
	if(cnt < 0) {
		printf("[%s]: wait ADI fifo empty timeout!!! \n", __func__);
		return -1;
	}
	return 0;
#endif  /* CONFIG_FPGA */
}

#define ANA_VIRT_BASE			( SPRD_MISC_BASE )
#define ANA_PHYS_BASE			( SPRD_MISC_PHYS )

#ifdef ADI_R5P1_VER
#define ANA_ADDR_SIZE			(SZ_128K)
#else
#define ANA_ADDR_SIZE			( SZ_32K + SZ_4K )
#endif

#define ADDR_VERIFY(_X_)	do { \
	BUG_ON((_X_) < SPRD_ADISLAVE_PHYS || (_X_) > (SPRD_ADISLAVE_PHYS + ANA_ADDR_SIZE));} while (0)

static inline u32 __adi_translate_addr(u32 regvddr)
{
	regvddr = regvddr - ANA_VIRT_BASE + ANA_PHYS_BASE;
	return regvddr;
}

static inline int __adi_read(u32 regPddr)
{
	u32 val;
	int cnt = 2000;
#if defined(CONFIG_FPGA)
	return 0;
#else
	/*
	 * We don't wait write fifo empty in here,
	 * Because if normal write is SYNC, that will
	 * wait fifo empty at the end of the write.
	 */
	writel(regPddr, REG_ADI_RD_CMD);

	/*
	 * wait read operation complete, RD_data[31]
	 * is set simulaneously when writing read command.
	 * RD_data[31] will be cleared after the read operation complete,
	 */
	do {
		val = readl(REG_ADI_RD_DATA);
		udelay(1);
	} while ((val & BIT_RD_CMD_BUSY) && cnt--);

	if (cnt < 0){
		printf("[%s]: ADI READ timeout!!! reg = 0x%x, value = 0x%x\n", __func__, regPddr, val);
	}
#ifndef ADI_R5P1_VER
	/* val high part should be the address of the last read operation */
	BUG_ON(TO_ADDR(val) != ((regPddr & readback_addr_mak) >> readback_offset));
#endif

	return (val & MASK_RD_VALU);
#endif  /* CONFIG_FPGA */
}

int sci_adi_read(u32 reg)
{
	unsigned long val;
#if defined(CONFIG_FPGA)
	return 0;
#else

	SPRD_ADI_LOCK(state);
	ADDR_VERIFY(reg);
	reg = __adi_translate_addr(reg);
	val = __adi_read(reg);
	SPRD_ADI_UNLOCK(state);
	return val;
#endif  /* CONFIG_FPGA */
}

static inline int adi_write_wait(u32 val, u32 reg)
{
	writel(val, (unsigned long)reg);

#ifdef ADI_R5P1_VER

	u32 write_timeout = ADI_WRITE_TIMEOUT_MS;

	do {
		if (!(readl(REG_ADI_FIFO_STS) & BIT_ARM_WR_FREQ))
			break;

		udelay(1);
	} while (--write_timeout);

	if (write_timeout == 0)
		return -1;

#endif

	return 0;
}

static inline int __adi_write(u32 reg, u16 val, u32 sync)
{
	u32 cnt =  200000;
	int ret;

#if defined(CONFIG_FPGA)
	return 0;
#else
	ret = __adi_fifo_drain();
	if (ret < 0)
		return -1;

	do {
		if (!FIFO_IS_FULL()) {
			ret = adi_write_wait(val, reg);
			if (ret < 0)
				return -1;
			break;
		}

	} while (--cnt);

	if (cnt == 0) {
		return -1;
	}

	return 0;
#endif  /* CONFIG_FPGA */
}

int sci_adi_write_fast(u32 reg, u16 val, u32 sync)
{
#if defined(CONFIG_FPGA)
	return 0;
#else
	SPRD_ADI_LOCK(state);
	ADDR_VERIFY(reg);
	__adi_write(reg, val, sync);
	SPRD_ADI_UNLOCK(state);

	return 0;
#endif  /* CONFIG_FPGA */
}

int sci_adi_write(u32 reg, u16 or_val, u16 clear_msk)
{
#if defined(CONFIG_FPGA)
	return 0;
#else
	SPRD_ADI_LOCK(state);

	ADDR_VERIFY(reg);
	__adi_write(reg,
		    (__adi_read(__adi_translate_addr(reg)) &
		     ~clear_msk) | or_val, 1);

	SPRD_ADI_UNLOCK(state);
	return 0;
#endif  /* CONFIG_FPGA */
}

static void __adi_init(void)
{
#if defined(CONFIG_FPGA)
	return;
#else
	u32 value;

	value = VALUE_CH_PRI;
	writel(value, REG_ADI_CHNL_PRIL);
	writel(value, REG_ADI_CHNL_PRIH);

#endif  /* CONFIG_FPGA */
}

void sci_adi_init(void)
{
#if defined(CONFIG_FPGA)
	return;
#else
	if(!is_ana_init) {
		/* enable adi in global regs */
		CHIP_REG_OR(REG_ADI_EB, BIT_ADI_EB);
		__adi_init();

		is_ana_init = 1;
	}
#endif  /* CONFIG_FPGA */
}

int adi_hwchannel_set(unsigned int chn, unsigned int config)
{
	u32 val;

	if (chn < 2)
		return -1;

	writel(config, REG_ADI_CHNL_ADDR(chn));

	if (chn < 32) {
		val = readl(REG_ADI_CHNL_EN0);
		val |= BIT(chn);
		writel(val, REG_ADI_CHNL_EN0);
	} else if (chn < HW_CHN_CNT) {
		val = readl(REG_ADI_CHNL_EN1);
		val |= BIT(chn - 32);
		writel(val, REG_ADI_CHNL_EN1);
	} else {
		return -1;
	}

	return 0;
}

/*
 * sci_get_adie_chip_id - read a-die chip id
 *
 * return:
 * the a-die chip id, example: 0x2711A000
 */
u32 sci_get_adie_chip_id(void)
{
#if defined(CONFIG_FPGA)
	return 0;
#else
	u32 chip_id;

	if(!is_ana_init) {
		sci_adi_init();
	}

	chip_id = (sci_adi_read(ANA_REG_GLB_CHIP_ID_HIGH) & 0xffff) << 16;
	chip_id |= sci_adi_read(ANA_REG_GLB_CHIP_ID_LOW) & 0xffff;

	return chip_id;
#endif  /* CONFIG_FPGA */
}

