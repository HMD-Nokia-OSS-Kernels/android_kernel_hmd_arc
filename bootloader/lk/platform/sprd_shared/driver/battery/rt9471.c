/*
* SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
*
* Copyright 2016-2023 Unisoc (Shanghai) Technologies Co. Ltd

* Licensed under the Unisoc General Software License, version 1.0 (the License);
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
* Software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* See the Unisoc General Software License, version 1.0 for more details.
*/

#include <sprd_chg_helper.h>
#include <errno.h>
#include <i2c.h>
#include <chipram_env.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>
#include <lk/debug.h>

/* Register 00h */
#define RT9471_REG_00				0x00

/* Register 01h */
#define RT9471_REG_01				0x01
#define REG01_WDT_RESET_MASK			0x04
#define REG01_WDT_RESET_SHIFT			2
#define REG01_WDT_RESET				1

/* Register 0x02*/
#define RT9471_REG_02				0x02
#define REG02_CHG_CONFIG_MASK			0x01
#define REG02_CHG_CONFIG_SHIFT			0
#define REG02_CHG_DISABLE			0
#define REG02_CHG_ENABLE			1

/* Register 0x03*/
#define RT9471_REG_03				0x03

#define REG03_IINLIM_MASK 			0x3f
#define REG03_IINLIM_SHIFT 			0
#define REG03_IINLIM_LSB 			50
#define REG03_IINLIM_BASE 			50
#define REG03_IINLIM_OFFSET 		50
#define REG03_IINLIM_CURRENT_MAX 	3200
#define REG03_IINLIM_CURRENT_OFFSET 50

/* Register 0x04*/
#define RT9471_REG_04				0x04

/* Register 0x05*/
#define RT9471_REG_05				0x03
#define REG05_IPRECHG_MASK			0x0f
#define REG05_IPRECHG_SHIFT			0
#define REG05_IPRECHG_LSB			50
#define REG05_IPRECHG_CURRENT_MAX		800

/* Register 0x06*/
#define RT9471_REG_06				0x06

/* Register 0x07*/
#define RT9471_REG_07				0x07

/* Register 0x08*/
#define RT9471_REG_08					0x08
#define REG08_ICHG_MASK				0x3f
#define REG08_ICHG_SHIFT			0
#define REG08_ICHG_BASE				0
#define REG08_ICHG_LSB				50
#define REG08_ICHG_MAX				3150

/* Register 0x09*/
#define RT9471_REG_09				0x09

/* Register 0x0A */
#define RT9471_REG_0A				0x0a

/* Register 0x0B */
#define	RT9471_REG_0B				0x0b
#define	REG0B_REG_RESET_MASK			0x80
#define	REG0B_REG_RESET_SHIFT			7
#define	REG0B_REG_RESET				1

#define RT9471_REG_PART_MASK		0x08
#define RT9471_REG_PART_SHIFT		3
#define RT9471_PART_VALUE			1

#define I2C_SPEED			100000
#define SLAVE_ADDR			0x53

#define POWER_PATH_ENABLE               0
#define POWER_PATH_DISABLE              1

static int i2c_bus_num = SPRDCHG_I2C_BUS;

static int rt9471_write_reg(u8 reg, u8 val)
{
	int ret;
	uint8_t buf[2] = {0};
	buf[0] = reg;
	buf[1] = val;
	ret = i2c_send(i2c_bus_num, SLAVE_ADDR, buf, 2);
	if (ret < 0) {
		dprintf(ALWAYS,"%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

	return 0;
}

static int rt9471_read_reg(u8 reg, u8 *value)
{
	int ret;
	uint8_t reg_addr[2] = {0};
	reg_addr[0] = reg;
	ret = i2c_read_write(i2c_bus_num, SLAVE_ADDR, reg_addr, 1, value, 1);

	if (ret < 0) {
		dprintf(ALWAYS,"%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

	dprintf(ALWAYS,"rt9471_read_reg reg = 0x%x, value = %d/0x%x\n", reg, *value, *value);

	return 0;
}

static void rt9471_set_value(u8 reg, u8 reg_bit, u8 reg_shift, u8 val)
{
	u8 tmp = 0;

	rt9471_read_reg(reg, &tmp);
	tmp = tmp & (~reg_bit) | (val << reg_shift);
	rt9471_write_reg(reg, tmp);
}

static u8 rt9471_get_value(u8 reg, u8 reg_bit, u8 reg_shift)
{
	u8 reg_value = 0;

	rt9471_read_reg(reg, &reg_value);
	reg_value = (reg_value & reg_bit) >> reg_shift;

	return reg_value;
}

static void chg_rt9471_set_chg_cur(u32 cur)
{
	u8 reg_value;

	dprintf(ALWAYS,"chg_rt9471_set_chg_cur : %d \n", cur);

	if (cur > REG08_ICHG_MAX)
		cur = REG08_ICHG_MAX;

	reg_value = cur / REG08_ICHG_LSB;

	rt9471_set_value(RT9471_REG_08, REG08_ICHG_MASK,
			  REG08_ICHG_SHIFT, reg_value);

}

static void chg_rt9471_set_limit_cur(u32 limit)
{
	u8 reg_value;

	dprintf(ALWAYS,"chg_rt9471_set_limit_cur : %d \n", limit);

	if (limit > REG03_IINLIM_CURRENT_MAX)
		limit = REG03_IINLIM_CURRENT_MAX;

	reg_value = limit / REG03_IINLIM_BASE;
	rt9471_set_value(RT9471_REG_03, REG03_IINLIM_MASK,
			  REG03_IINLIM_SHIFT, reg_value);

}

static int chg_rt9471_set_prechg(u32 ichg)
{
	u8 reg_value;

	if (ichg > REG05_IPRECHG_CURRENT_MAX)
		ichg = REG05_IPRECHG_CURRENT_MAX;

	reg_value = ichg / REG05_IPRECHG_LSB;
	rt9471_set_value(RT9471_REG_05, REG05_IPRECHG_MASK,
			  REG05_IPRECHG_SHIFT, reg_value);

	return 0;
}

static void chg_rt9471_cmd(enum sprdchg_cmd cmd, int value)
{
	switch (cmd) {
	case CHG_SET_CURRENT:
		chg_rt9471_set_chg_cur(value);
		break;
	case CHG_SET_LIMIT_CURRENT:
		chg_rt9471_set_limit_cur(value);
		break;
	case CHG_SET_PRE_CURRENT:
		chg_rt9471_set_prechg(value);
		break;
	default:
		break;
	}
}

static void chg_rt9471_reset(void)
{
	rt9471_set_value(RT9471_REG_0B, REG0B_REG_RESET_MASK,
			  REG0B_REG_RESET_SHIFT, REG0B_REG_RESET);
}

static void chg_rt9471_enable_chg(void)
{
	rt9471_set_value(RT9471_REG_02, REG02_CHG_CONFIG_MASK,
			  REG02_CHG_CONFIG_SHIFT, REG02_CHG_ENABLE);
}

static void chg_rt9471_disable_chg(void)
{

	rt9471_set_value(RT9471_REG_02, REG02_CHG_CONFIG_MASK,
			  REG02_CHG_CONFIG_SHIFT, REG02_CHG_DISABLE);
}

static void chg_rt9471_reset_timer(void)
{
	rt9471_set_value(RT9471_REG_01, REG01_WDT_RESET_MASK,
			  REG01_WDT_RESET_SHIFT, REG01_WDT_RESET);
}

static struct sprdchg_ic_operations rt9471_op = {
	.chg_start = chg_rt9471_enable_chg,
	.chg_stop = chg_rt9471_disable_chg,
	.timer_callback = chg_rt9471_reset_timer,
	.chg_cmd = chg_rt9471_cmd,
};

static int rt9471_charger_get_vendor_id_part_value(void)
{
	int ret = 0;
	u8 reg_part_val = 0;

	ret = rt9471_read_reg(RT9471_REG_0B, &reg_part_val);
	if (ret < 0) {
		dprintf(INFO,"[%s]l=%d: Failed to get vendor id, ret=%d\n",
			__FUNCTION__, __LINE__, ret);
		return ret;
	}

	reg_part_val = (reg_part_val & RT9471_REG_PART_MASK) >> RT9471_REG_PART_SHIFT;
	if (reg_part_val != RT9471_PART_VALUE) {
		errorf("[%s]l=%d: The part value is 0x%x\n",
			__FUNCTION__, __LINE__, reg_part_val);
		return -EINVAL;
	}

	return ret;
}

void sprdchg_rt9471_init(void)
{
	int ret;

	dprintf(INFO,"rt9471 init\n");

	ret = rt9471_charger_get_vendor_id_part_value();
	if (ret) {
		dprintf(INFO,"[%s]l=%d: rt9471 is not found, not register ops\n",
			__FUNCTION__, __LINE__);
		return ret;
	}
	dprintf(INFO,"rt9471 register charge ops!\n");
	sprdchg_register_ops(&rt9471_op);
}
