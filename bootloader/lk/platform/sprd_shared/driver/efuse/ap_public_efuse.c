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
#include <chipram_env.h>
#include <kernel/spinlock.h>

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define DIV_ROUND(n,d)		(((n) + ((d)/2)) / (d))
#define IS_ERR_VALUE(x) unlikely((x) >= (unsigned long)-MAX_ERRNO)

#ifndef SCI_ADDR
#define SCI_ADDR(_b_, _o_)		((unsigned int)(_b_) + (_o_))
#endif

#ifndef bool
typedef int bool;
#endif
#ifndef false
#define false 0
#endif
#ifndef true
#define true 1
#endif

static volatile spin_lock_t efuse_rw_spin = SPIN_LOCK_INITIAL_VALUE;
#ifdef CONFIG_SPRD_GICV3
#define SPRD_EFUSE_LOCK(state) spin_lock_irqsave(&efuse_rw_spin, state)
#define SPRD_EFUSE_UNLOCK(state) spin_unlock_irqrestore(&efuse_rw_spin, state)
#else
#define SPRD_EFUSE_LOCK(state) sprd_spin_lock(&efuse_rw_spin)
#define SPRD_EFUSE_UNLOCK(state) sprd_spin_unlock(&efuse_rw_spin)
#endif

typedef unsigned int  u32;

#define EFUSE_ALL0_INDEX		SCI_ADDR(SPRD_UIDEFUSE_PHYS, 0x0008)
#define EFUSE_MODE_CTRL			SCI_ADDR(SPRD_UIDEFUSE_PHYS, 0x000c)
#define EFUSE_IP_VER			SCI_ADDR(SPRD_UIDEFUSE_PHYS, 0x0014)
#define EFUSE_CFG0			SCI_ADDR(SPRD_UIDEFUSE_PHYS, 0x0018)
#define EFUSE_NS_EN			SCI_ADDR(SPRD_UIDEFUSE_PHYS, 0x0020)
#define EFUSE_NS_ERR_FLAG		SCI_ADDR(SPRD_UIDEFUSE_PHYS, 0x0024)
#define EFUSE_NS_FLAG_CLR		SCI_ADDR(SPRD_UIDEFUSE_PHYS, 0x0028)
#define EFUSE_NS_MAGIC_NUM		SCI_ADDR(SPRD_UIDEFUSE_PHYS, 0x002c)
#define EFUSE_FW_CFG			SCI_ADDR(SPRD_UIDEFUSE_PHYS, 0x0050)
#define EFUSE_PW_SWT			SCI_ADDR(SPRD_UIDEFUSE_PHYS, 0x0054)
#define EFUSE_MEM(val)			SCI_ADDR(SPRD_UIDEFUSE_PHYS,(0x1000 + (val << 2)))

#define SEC_REG_OFFSET			0x20

/* bits definitions for register EFUSE_MODE_CTRL */
#define BIT_EFUSE_ALL0_CHECK_START       (BIT(0))

/* bits definitions for register EFUSE_NS_EN/EFUSE_SE_EN */
#define BIT_VDD_EN			(BIT(0))
#define BIT_AUTO_CHECK_ENABLE		(BIT(1))
#define BIT_DOUBLE_BIT_EN		(BIT(2))
#define BIT_MARGIN_RD_ENABLE		(BIT(3))
#define BIT_LOCK_BIT_WR_EN		(BIT(4))

/* bits definitions for register EFUSE_NS_ERR_FLAG/EFUSE_SE_ERR_FLAG */
#define BIT_WORD0_ERR_FLAG		(BIT(0))
#define BIT_WORD1_ERR_FLAG		(BIT(1))
#define BIT_WORD0_PROT_FLAG		(BIT(4))
#define BIT_WORD1_PROT_FLAG		(BIT(5))
#define BIT_PG_EN_WR_FLAG		(BIT(8))
#define BIT_VDD_ON_RD_FLAG		(BIT(9))
#define BIT_BLOCK0_RD_FLAG		(BIT(10))
#define BIT_MAGNUM_WR_FLAG		(BIT(11))
#define BIT_ENK_ERR_FLAG		(BIT(12))
#define BIT_ALL0_CHECK_FLAG		(BIT(13))

/* bits definitions for register EFUSE_NS_FLAG_CLR/EFUSE_SE_FLAG_CLR */
#define BIT_WORD0_ERR_CLR		(BIT(0))
#define BIT_WORD1_ERR_CLR		(BIT(1))
#define BIT_WORD0_PROT_CLR		(BIT(4))
#define BIT_WORD1_PROT_CLR		(BIT(5))
#define BIT_PG_EN_WR_CLR		(BIT(8))
#define BIT_VDD_ON_RD_CLR		(BIT(9))
#define BIT_BLOCK0_RD_CLR		(BIT(10))
#define BIT_MAGNUM_WR_CLR		(BIT(11))
#define BIT_ENK_ERR_CLR			(BIT(12))
#define BIT_ALL0_CHECK_CLR		(BIT(13))

/* bits definitions for register EFUSE_PW_SWT */

#define BIT_EFS_ENK1_ON      		(BIT(0))
#define BIT_EFS_ENK2_ON      		(BIT(1))
#define BIT_NS_S_PG_EN      		(BIT(2))

/* Magic number, only when this field is 0x8810, the efuse programming
 * command can be handle. So, if SW want to program efuse memory,
 * except open clocks and power, the follow conditions must be met:
 * 1. PGM_EN = 1;
 * 2. EFUSE_MAGIC_NUMBER = 0x8810
 */
#define BITS_EFUSE_MAGIC_NUMBER(_x_)          ((_x_) << 0 & (BIT(0)|BIT(1)\
	|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)\
	|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)))

#define EFUSE_MAGIC_NUMBER		( 0x8810 )
#define ERR_CLR_MASK			0x3fff

#if defined(PLATFORM_SHARKL3)
	#define EFUSE_BLK    43
	#define EFUSE_PROG_MASK(x)    ((x) & GENMASK(7, 0))
	#define EFUSE_PROG(x)    ((~ 0xFFFFFFFF) | (EFUSE_PROG_MASK(x) << 24))
	#define EFUSE_READ_MASK(x)    ((x) & GENMASK(31, 24))
	#define EFUSE_READ(x)    ((~ 0xFFFFFFFF) | (EFUSE_READ_MASK(x) >> 24))
#elif (defined(PLATFORM_SHARKL5PRO) || defined(PLATFORM_QOGIRL6))
	#define EFUSE_BLK_0    84
	#define EFUSE_BLK_1    85
	#define EFUSE_PROG_MASK_1(x)    ((x) & GENMASK(6, 4))
	#define EFUSE_PROG_MASK_2(x)    ((x) & GENMASK(3, 0))
	#define EFUSE_PROG_0(x)    (((x) << 24) & (~ 0x7FFFFFFF))
	#define EFUSE_PROG_1(x)    ((~ 0xFFFFFFFF) | (EFUSE_PROG_MASK_1(x) << 25) | (EFUSE_PROG_MASK_2(x) << 12))
	#define EFUSE_READ_0(x)     ((~ 0xFFFFFFFF) | (((x) & BIT(31)) >> 24))
	#define EFUSE_READ_MASK_1(x)    ((x) & GENMASK(31, 29))
	#define EFUSE_READ_MASK_2(x)    ((x) & GENMASK(15, 12))
	#define EFUSE_READ_1(x)    ((~ 0xFFFFFFFF) | (EFUSE_READ_MASK_1(x) >> 25))
	#define EFUSE_READ_2(x)    ((~ 0xFFFFFFFF) | (EFUSE_READ_MASK_2(x) >> 12))
#endif

#if (defined(PLATFORM_SHARKL3) || defined(PLATFORM_SHARKL5PRO) || defined(PLATFORM_QOGIRL6))
	#define GENMASK(h, l) \
			(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#endif

u8 cust_efuse_glob_val = 0;

extern void tmr_udelay (unsigned long usec);

#define udelay(x)	\
	do { \
		volatile int i; \
		int cnt = 1000 * x; \
		for (i=0; i<cnt; i++);\
	} while(0);

static inline u32 ap_efuse_reg_read(unsigned long reg)
{
	return readl(reg);
}

static inline void ap_efuse_reg_write(u32 val, unsigned long reg)
{
	writel(val, reg);
}

static void ap_efuse_prog_power_on(void)
{
	u32 cfg0;

	cfg0 = ap_efuse_reg_read(EFUSE_PW_SWT);
	cfg0 &= ~BIT_EFS_ENK2_ON;
	ap_efuse_reg_write(cfg0, EFUSE_PW_SWT);
	tmr_udelay(1000);
	cfg0 |= BIT_EFS_ENK1_ON;
	ap_efuse_reg_write(cfg0, EFUSE_PW_SWT);
	tmr_udelay(1000);
}

static void ap_efuse_prog_power_off(void)
{
	u32 cfg0;

	cfg0 = ap_efuse_reg_read(EFUSE_PW_SWT);
	cfg0 &= ~BIT_EFS_ENK1_ON;
	ap_efuse_reg_write(cfg0, EFUSE_PW_SWT);
	tmr_udelay(1000);
	cfg0 |= BIT_EFS_ENK2_ON;
	ap_efuse_reg_write(cfg0, EFUSE_PW_SWT);
	tmr_udelay(1000);
}

static void ap_efuse_read_power_on(unsigned long sec_offset)
{
	u32 cfg0;

	cfg0 = ap_efuse_reg_read(EFUSE_NS_EN + sec_offset);
	cfg0 |= BIT_VDD_EN;
	ap_efuse_reg_write(cfg0, EFUSE_NS_EN + sec_offset);
	tmr_udelay(1000);
}

static void ap_efuse_read_power_off(unsigned long sec_offset)
{
	u32 cfg0;

	cfg0 = ap_efuse_reg_read(EFUSE_NS_EN + sec_offset);
	cfg0 &= ~BIT_VDD_EN;
	ap_efuse_reg_write(cfg0, EFUSE_NS_EN + sec_offset);
	tmr_udelay(1000);
}

static void efuse_clk_enable(void)
{
#if defined(PLATFORM_SHARKL5) || defined(PLATFORM_SHARKL5PRO) || \
	defined(PLATFORM_ROC1) || defined(PLATFORM_ORCA) || \
	defined(PLATFORM_QOGIRL6) || defined(PLATFORM_QOGIRN6PRO) || defined(PLATFORM_QOGIRN6L)
	sci_glb_set(REG_AON_APB_APB_EB1, BIT_EFUSE_EB);
#else
	sci_glb_set(REG_AON_APB_APB_EB0, BIT_EFUSE_EB);
#endif
}

static void ap_efuse_prog_lock(bool en_lock, unsigned long sec_offset)
{
	u32 cfg0;

	cfg0 = ap_efuse_reg_read(EFUSE_NS_EN + sec_offset);
	if(en_lock)
		cfg0 |= BIT_LOCK_BIT_WR_EN;
	else
		cfg0 &= ~BIT_LOCK_BIT_WR_EN;

	ap_efuse_reg_write(cfg0, EFUSE_NS_EN + sec_offset);
}

static void ap_efuse_double(bool backup, unsigned long sec_offset)
{
	u32 cfg0;

	cfg0 = ap_efuse_reg_read(EFUSE_NS_EN + sec_offset);
	if(backup)
		cfg0 |= BIT_DOUBLE_BIT_EN;
	else
		cfg0 &= ~BIT_DOUBLE_BIT_EN;

	ap_efuse_reg_write(cfg0, EFUSE_NS_EN + sec_offset);
}

static void ap_efuse_auto_check(bool en_lock, unsigned long sec_offset)
{
	u32 cfg0;

	cfg0 = ap_efuse_reg_read(EFUSE_NS_EN + sec_offset);
	if (en_lock)
		cfg0 |= BIT_AUTO_CHECK_ENABLE;
	else
		cfg0 &= ~BIT_AUTO_CHECK_ENABLE;

	ap_efuse_reg_write(cfg0, EFUSE_NS_EN + sec_offset);
}

static u32 ap_efuse_read(int blk)
{
	u32 val = 0;

	val = ap_efuse_reg_read(EFUSE_MEM(blk));
	return val;
}

static int ap_efuse_prog(u32 blk, bool backup, bool lock, u32 val)
{
	u32 cfg0;
	int ret;
	unsigned long sec_offset = 0;

	if (get_boot_role() == BOOTLOADER_MODE_LOAD) {
		sec_offset = 0;
	} else if (get_boot_role() == BOOTLOADER_MODE_DOWNLOAD) {
		sec_offset = SEC_REG_OFFSET;
	} else {
		errorf("boot role is unknown\n");
		return -EINVAL;
	}

	ap_efuse_reg_write(BITS_EFUSE_MAGIC_NUMBER(EFUSE_MAGIC_NUMBER),
                           EFUSE_NS_MAGIC_NUM + sec_offset);
	ap_efuse_prog_power_on();
	ap_efuse_reg_write(ERR_CLR_MASK, EFUSE_NS_FLAG_CLR + sec_offset);
	cfg0 = ap_efuse_reg_read(EFUSE_PW_SWT);
	cfg0 |= BIT_NS_S_PG_EN;
	ap_efuse_reg_write(cfg0, EFUSE_PW_SWT);

	ap_efuse_double(backup, sec_offset);
	if (lock)
		ap_efuse_auto_check(true, sec_offset);
	ap_efuse_reg_write(val, EFUSE_MEM(blk));
	if (lock)
		ap_efuse_auto_check(false, sec_offset);
	ap_efuse_double(false, sec_offset);
	ret = ap_efuse_reg_read(EFUSE_NS_ERR_FLAG + sec_offset);
	if(ret) {
		errorf("write efuse error status ret 0x%x\n", ret);
	} else if (lock) {
		ap_efuse_prog_lock(lock, sec_offset);
		ap_efuse_reg_write(0x0, EFUSE_MEM(blk));
		ap_efuse_prog_lock(false, sec_offset);
	}
	ap_efuse_reg_write(ERR_CLR_MASK, EFUSE_NS_FLAG_CLR + sec_offset);

	cfg0 = ap_efuse_reg_read(EFUSE_PW_SWT);
	cfg0 &= ~BIT_NS_S_PG_EN;
	ap_efuse_reg_write(cfg0, EFUSE_PW_SWT);
	ap_efuse_prog_power_off();
	ap_efuse_reg_write(0, EFUSE_NS_MAGIC_NUM + sec_offset);

	return ret;
}

u32 sprd_efuse_double_read(int blk, bool backup)
{
	u32 val = 0;
	int ret;
	unsigned long sec_offset = 0;
	spin_lock_saved_state_t state;

	if (get_boot_role() == BOOTLOADER_MODE_LOAD) {
		sec_offset = 0;
	} else if (get_boot_role() == BOOTLOADER_MODE_DOWNLOAD) {
		sec_offset = SEC_REG_OFFSET;
	} else {
		errorf("boot role is unknown\n");
		return 0;
	}

	SPRD_EFUSE_LOCK(state);
	efuse_clk_enable();

	ap_efuse_reg_write(ERR_CLR_MASK, EFUSE_NS_FLAG_CLR + sec_offset);
	ap_efuse_read_power_on(sec_offset);
	ap_efuse_double(backup, sec_offset);
	val = ap_efuse_read(blk);
	ap_efuse_double(false, sec_offset);
	ap_efuse_read_power_off(sec_offset);

	ret = ap_efuse_reg_read(EFUSE_NS_ERR_FLAG + sec_offset);
	if (ret) {
		ap_efuse_reg_write(ERR_CLR_MASK, EFUSE_NS_FLAG_CLR + sec_offset);
		val = 0;
	}

	SPRD_EFUSE_UNLOCK(state);
	debugf("efuse read blk %d, ret 0x%x, val 0x%08x\n", blk, ret, val);

	return val;
}

u32 sprd_ap_efuse_read(int blk)
{
	return sprd_efuse_double_read(blk, 1);
}

int sprd_efuse_double_prog(u32 blk, bool backup, bool lock, u32 val)
{
	int ret;
	spin_lock_saved_state_t state;

	SPRD_EFUSE_LOCK(state);
	efuse_clk_enable();
	ret = ap_efuse_prog(blk, backup, lock, val);
	if (ret) {
		ret = -EIO;
		dprintf(ALWAYS, "efuse prog blk:%d, ret:0x%x, val:0x%08x\n", blk, ret, val);
	}
	SPRD_EFUSE_UNLOCK(state);

	return ret;
}

void custom_efuse_prog(u8 val)
{
	u32 val_prog = 0;
#if defined(PLATFORM_SHARKL3)
	val_prog = val;
	sprd_efuse_double_prog(EFUSE_BLK, 1, 0, EFUSE_PROG(val_prog));
#elif (defined(PLATFORM_SHARKL5PRO) || defined(PLATFORM_QOGIRL6))
	val_prog = val;
	sprd_efuse_double_prog(EFUSE_BLK_0, 0, 0, EFUSE_PROG_0(val_prog));
	mdelay(100);
	sprd_efuse_double_prog(EFUSE_BLK_1, 0, 0, EFUSE_PROG_1(val_prog));
#endif
}

void custom_efuse_read(void)
{
	u32 val_read = 0;
	u8 custom_efuse_val = 0;
#if defined(PLATFORM_SHARKL3)
	val_read = sprd_efuse_double_read(EFUSE_BLK, 1);
	custom_efuse_val = (u8)EFUSE_READ(val_read);
#elif (defined(PLATFORM_SHARKL5PRO) || defined(PLATFORM_QOGIRL6))
	u32 val_read0 = 0;
	u32 val_read1 = 0;
	val_read0 = sprd_efuse_double_read(EFUSE_BLK_0, 0);
	val_read |= EFUSE_READ_0(val_read0);
	val_read1 = sprd_efuse_double_read(EFUSE_BLK_1, 0);
	val_read |= EFUSE_READ_1(val_read1);
	val_read |= EFUSE_READ_2(val_read1);
	custom_efuse_val = (u8)val_read;
#endif
	cust_efuse_glob_val = custom_efuse_val;
}

u8 custom_efuse_read_val(void)
{
	u8 cust_efuse_val = 0;
	cust_efuse_val = cust_efuse_glob_val;
	return cust_efuse_val;
}

