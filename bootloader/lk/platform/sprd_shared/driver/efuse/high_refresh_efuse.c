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
#include <asm/arch/common.h>
#include <asm/arch/sprd_reg.h>
#include <errno.h>
#include <i2c.h>
#include <sprd_common.h>
#include <sprd_glb.h>

#define UMB9230S_EFUSE_BASE		0x2000
#define UMB9230S_I2C_BUS_NUM		3
#define UMB9230S_SLAVE_ADDR		0x12

#define EFUSE_ALL0_INDEX		SCI_ADDR(UMB9230S_EFUSE_BASE, 0x0008)
#define EFUSE_MODE_CTRL			SCI_ADDR(UMB9230S_EFUSE_BASE, 0x000c)
#define EFUSE_SEC_EN			SCI_ADDR(UMB9230S_EFUSE_BASE, 0x0040)
#define EFUSE_SEC_ERR_FLAG		SCI_ADDR(UMB9230S_EFUSE_BASE, 0x0044)
#define EFUSE_SEC_FLAG_CLR		SCI_ADDR(UMB9230S_EFUSE_BASE, 0x0048)
#define EFUSE_SEC_MAGIC_NUM		SCI_ADDR(UMB9230S_EFUSE_BASE, 0x004c)
#define EFUSE_PW_SWT			SCI_ADDR(UMB9230S_EFUSE_BASE, 0x0054)
#define EFUSE_PW_ON_RD_END_FLAG		SCI_ADDR(UMB9230S_EFUSE_BASE, 0x0068)
#define EFUSE_POR_READ_DATA(n)		SCI_ADDR(UMB9230S_EFUSE_BASE, n)

#define EFUSE_MEM(index)		SCI_ADDR(UMB9230S_EFUSE_BASE,(0x1000 + (index << 2)))

/* bits definitions for register EFUSE_SEC_EN */
#define BIT_VDD_EN			(BIT(0))
#define BIT_AUTO_CHECK_ENABLE		(BIT(1))
#define BIT_DOUBLE_BIT_EN		(BIT(2))
#define BIT_MARGIN_RD_ENABLE		(BIT(3))
#define BIT_LOCK_BIT_WR_EN		(BIT(4))

/* bits definitions for register EFUSE_SEC_ERR_FLAG */
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
#define ERR_FLAG_MASK			0x3fff

/* bits definitions for register EFUSE_PW_SWT */
#define BIT_EFS_ENK1_ON			(BIT(0))
#define BIT_EFS_ENK2_ON			(BIT(1))
#define BIT_NS_S_PG_EN			(BIT(2))

#define EFUSE_MAGIC_NUMBER		0x8810
#define ERR_CLR_MASK			0x3fff

extern void tmr_udelay(unsigned long usec);

static int efuse_i2c_read(uint16_t reg, uint32_t *data)
{
	int ret = 0;

	ret = iic2cmd_read(UMB9230S_I2C_BUS_NUM, UMB9230S_SLAVE_ADDR, reg, data, 1);
	if (ret < 0)
		dprintf(SPEW, "%s: i2c read failed\n", __func__);

	return ret;
}

static int efuse_i2c_write(uint16_t reg, uint32_t val)
{
	int ret = 0;
	uint32_t buf[2] = {0};

	buf[0] = reg;
	buf[1] = val;

	ret = iic2cmd_write(UMB9230S_I2C_BUS_NUM, UMB9230S_SLAVE_ADDR, buf, 2);
	if (ret < 0)
		dprintf(SPEW, "%s: efuse i2c write failed\n", __func__);

	return ret;
}

static void efuse_reg_conf(uint16_t reg, uint32_t bit, bool enable)
{
	uint32_t cfg0;
	int ret = 0;

	ret = efuse_i2c_read(reg, &cfg0);
	if (ret < 0) {
		dprintf(SPEW, "%s: efuse read reg:0x%x failed\n", __func__, reg);
	} else {
		if (enable)
			cfg0 |= bit;
		else
			cfg0 &= ~bit;

		efuse_i2c_write(reg, cfg0);
	}
}

static u32 efuse_check_status(void)
{
	int ret = 0;
	uint32_t val = ERR_FLAG_MASK;

	ret = efuse_i2c_read(EFUSE_SEC_ERR_FLAG, &val);
	if (ret < 0)
		dprintf(SPEW, "%s: efuse read status failed\n", __func__);

	efuse_i2c_write(EFUSE_SEC_FLAG_CLR, ERR_CLR_MASK);

	return val;
}

static void efuse_prog_power_on(void)
{
	efuse_reg_conf(EFUSE_PW_SWT, BIT_NS_S_PG_EN, TRUE);

	efuse_reg_conf(EFUSE_PW_SWT, BIT_EFS_ENK2_ON, FALSE);
	tmr_udelay(1000);

	efuse_reg_conf(EFUSE_PW_SWT, BIT_EFS_ENK1_ON, TRUE);
	tmr_udelay(1000);
}

static void efuse_prog_power_off(void)
{
	efuse_reg_conf(EFUSE_PW_SWT, BIT_EFS_ENK1_ON, FALSE);
	tmr_udelay(1000);

	efuse_reg_conf(EFUSE_PW_SWT, BIT_EFS_ENK2_ON, TRUE);
	tmr_udelay(1000);

	efuse_reg_conf(EFUSE_PW_SWT, BIT_NS_S_PG_EN, FALSE);
}

static void efuse_double(bool enable)
{
	efuse_reg_conf(EFUSE_SEC_EN, BIT_DOUBLE_BIT_EN, enable);
}

static void efuse_read_power_on(void)
{
	efuse_reg_conf(EFUSE_SEC_EN, BIT_VDD_EN, TRUE);
}

static void efuse_read_power_off(void)
{
	efuse_reg_conf(EFUSE_SEC_EN, BIT_VDD_EN, FALSE);
}

uint32_t high_refresh_efuse_prog(int start_index, int end_index, bool isdouble, uint32_t *val)
{
	int blk_index, i;
	uint32_t err_flag = ERR_FLAG_MASK;

	efuse_i2c_write(EFUSE_SEC_FLAG_CLR, ERR_CLR_MASK);
	efuse_i2c_write(EFUSE_SEC_MAGIC_NUM, EFUSE_MAGIC_NUMBER);
	efuse_prog_power_on();
	efuse_double(isdouble);

	for (blk_index = start_index; blk_index <= end_index; blk_index++) {
		i = blk_index - start_index;
		efuse_i2c_write(EFUSE_MEM(blk_index), val[i]);
		err_flag = efuse_check_status();
		if (err_flag != 0)
			dprintf(SPEW, "%s: efuse write failed, status:0x%08x\n", __func__,
				err_flag);
		dprintf(SPEW, "%s: efuse write blk%d, val:0x%08x\n", __func__, blk_index, val[i]);
	}

	efuse_double(FALSE);
	efuse_prog_power_off();
	efuse_i2c_write(EFUSE_SEC_MAGIC_NUM, 0);

	return err_flag;
}

uint32_t high_refresh_efuse_read(int start_index, int end_index, bool isdouble, uint32_t *val)
{
	int blk_index, i, ret = 0;
	uint32_t err_flag = ERR_FLAG_MASK;

	efuse_i2c_write(EFUSE_SEC_FLAG_CLR, ERR_CLR_MASK);
	efuse_read_power_on();
	efuse_double(isdouble);

	for (blk_index = start_index; blk_index <= end_index; blk_index++) {
		i = blk_index - start_index;
		ret = efuse_i2c_read(EFUSE_MEM(blk_index), &val[i]);
		if (ret < 0) {
			dprintf(SPEW, "%s: efuse read block error\n", __func__);
		} else {
			err_flag = efuse_check_status();
			if (err_flag != 0)
				dprintf(SPEW, "%s: efuse read failed, status:0x%08x\n", __func__,
					err_flag);
			dprintf(SPEW, "%s: efuse read blk%d, val:0x%08x\n", __func__, blk_index,
				val[i]);
		}
	}

	efuse_double(FALSE);
	efuse_read_power_off();

	return err_flag;
}

int high_refresh_efuse_power_on_read(uint32_t reg_offset, uint32_t *val)
{
	int ret = 0;
	uint32_t flag = 0;

	ret = efuse_i2c_read(EFUSE_PW_ON_RD_END_FLAG, &flag);
	if (ret >= 0 && flag == 0x1) {
		ret = efuse_i2c_read(EFUSE_POR_READ_DATA(reg_offset), val);
		if (ret >= 0)
			dprintf(SPEW, "%s: reg%x, val:0x%08x\n", __func__, reg_offset, *val);
	}
	return ret;
}
