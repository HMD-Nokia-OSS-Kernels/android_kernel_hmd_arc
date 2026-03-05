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

#include <errno.h>
#include <i2c.h>
#include <lk/debug.h>
#include <sprd_chg_helper.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#include <sgm41516.h>


#define I2C_SPEED			100000

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define SPRD_CHG_TAG			"sprd_chg"

/* SGM41516 Register 0x00 IINDPM[4:0] */
static const u32 sgm41516_iindpm[] = {
	100, 200, 300, 400, 500, 600, 700, 800,
	900, 1000, 1100, 1200, 1300, 1400, 1500, 1600,
	1700, 1800, 1900, 2000, 2100, 2200, 2300, 2400,
	2500, 2600, 2700, 2800, 2900, 3000, 3100, 3800
};

static int i2c_bus_num = SPRDCHG_I2C_BUS;

static int sgm41516_write_reg(u8 reg, u8 val)
{
	int ret = 0;
	uint8_t buf[2] = {0};
	buf[0] = reg;
	buf[1] = val;

	ret = i2c_send(i2c_bus_num, SLAVE_ADDR, buf, 2);
	if (ret < 0) {
		dprintf(ALWAYS,"%s: %s, failed to write reg[0x%x], ret = %d\n",
			SPRD_CHG_TAG, __func__, reg, ret);
		return ret;
	}

	return 0;
}

static int sgm41516_read_reg(u8 reg, u8 *value)
{
	int ret;
	uint8_t reg_addr[2] = {0};

	reg_addr[0] = reg;
	ret = i2c_read_write(i2c_bus_num, SLAVE_ADDR, reg_addr, 1, value, 1);
	if (ret < 0) {
		dprintf(ALWAYS,"%s: %s, failed to read reg[0x%x], ret = %d\n",
			SPRD_CHG_TAG, __func__, reg, ret);
		return ret;
	}

	dprintf(INFO,"%s: %s, reg[0x%x] = %d/%x\n", SPRD_CHG_TAG, __func__, reg, *value, *value);

	return 0;
}

static int sgm41516_update_reg_bits(u8 reg, u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = sgm41516_read_reg(reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return sgm41516_write_reg(reg, v);
}

static int sgm41516_set_current(u32 cur)
{
	u8 reg_val;

	dprintf(INFO, "%s: %s, set Ibat %d mA\n", SPRD_CHG_TAG, __func__, cur);
	if (cur < SGM41516_ICHG_BASE)
		cur = SGM41516_ICHG_BASE;
	else if (cur > SGM41516_ICHG_MAX)
		cur = SGM41516_ICHG_MAX;

	reg_val = (cur - SGM41516_ICHG_BASE) / SGM41516_ICHG_LSB;
	reg_val <<= SGM41516_ICHG_SHIFT;

	return sgm41516_update_reg_bits(SGM41516_REG_02, SGM41516_ICHG_MASK, reg_val);
}

static int sgm41516_set_limit_current(u32 cur)
{
	u8 reg_val;
	int i;

	dprintf(INFO, "%s: %s, set Ibus %d mA\n", SPRD_CHG_TAG, __func__, cur);
	for (i = 0; i < ARRAY_SIZE(sgm41516_iindpm); i++) {
		if (cur < sgm41516_iindpm[i])
			break;
	}

	if (i == 0)
		reg_val = 0;
	else
		reg_val = i - 1;

	return sgm41516_update_reg_bits(SGM41516_REG_00,
					SGM41516_IINLIM_MASK,
					reg_val << SGM41516_IINLIM_SHIFT);
}

static int sgm41516_set_pre_charge_current(u32 cur)
{
	u8 reg_val;

	dprintf(INFO,"%s: %s, set pre-charge current %ld mA\n", SPRD_CHG_TAG, __func__, cur);
	if (cur > SGM41516_IPRECHG_MAX)
		cur = SGM41516_IPRECHG_MAX;
	else if (cur < SGM41516_IPRECHG_BASE)
		cur = SGM41516_IPRECHG_MAX;

	reg_val = (cur - SGM41516_IPRECHG_BASE) / SGM41516_IPRECHG_LSB;

	return sgm41516_update_reg_bits(SGM41516_REG_03,
					SGM41516_IPRECHG_MASK,
					reg_val << SGM41516_IPRECHG_SHIFT);
}

static int sgm41516_enable_hiz_mode(bool enable)
{
	u8 reg_val = SGM41516_HIZ_DISABLE;

	dprintf(INFO,"%s: %s, enable = %d\n", SPRD_CHG_TAG, __func__, enable);
	if (enable)
		reg_val = SGM41516_HIZ_ENABLE;

	return sgm41516_update_reg_bits(SGM41516_REG_00,
					SGM41516_ENHIZ_MASK,
					reg_val << SGM41516_ENHIZ_SHIFT);
}

static int sgm41516_enable_wdg(bool enable)
{
	u8 reg_val = SGM41516_WDT_DISABLE;

	dprintf(INFO, "%s: %s, enable = %d\n", SPRD_CHG_TAG, __func__, enable);
	if (enable)
		reg_val = SGM41516_WDT_40S;

	return sgm41516_update_reg_bits(SGM41516_REG_05,
					SGM41516_WDT_MASK,
					reg_val << SGM41516_WDT_SHIFT);
}

static int sgm41516_enalbe_power_path(bool enable)
{
	int ret = 0;

	dprintf(INFO,"%s: %s, enable = %d\n", SPRD_CHG_TAG, __func__, enable);
	ret = sgm41516_enable_hiz_mode(!enable);
	if (ret) {
		dprintf(ALWAYS,"%s: %s, faile to set %s hiz mode, ret = %d\n",
			SPRD_CHG_TAG, __func__, !enable ? "enable" : "disable");
		return ret;
	}

	return sgm41516_enable_wdg(false);
}

static int sgm41516_enable_charger(bool enable)
{
	u8 reg_val = SGM41516_CHG_DISABLE;

	dprintf(INFO,"%s: %s, enable = %d\n", SPRD_CHG_TAG, __func__, enable);
	if (enable)
		reg_val = SGM41516_CHG_ENABLE;

	return sgm41516_update_reg_bits(SGM41516_REG_01,
					SGM41516_CHG_CONFIG_MASK,
					reg_val << SGM41516_CHG_CONFIG_SHIFT);
}

static void sgm41516_start_charge(void)
{
	int ret = 0;

	dprintf(INFO,"%s: %s\n", SPRD_CHG_TAG, __func__);
	ret = sgm41516_enable_hiz_mode(false);
	if (ret)
		dprintf(ALWAYS,"%s: %s, failed to disable HIZ mode\n", SPRD_CHG_TAG, __func__);

	ret = sgm41516_enable_charger(true);
	if (ret)
		dprintf(ALWAYS,"%s: %s, failed to enable charge\n", SPRD_CHG_TAG, __func__);
}

static void sgm41516_stop_charge(void)
{
	int ret = 0;

	dprintf(INFO,"%s: %s\n", SPRD_CHG_TAG, __func__);
	ret = sgm41516_enable_charger(false);
	if (ret)
		dprintf(ALWAYS,"%s: %s, failed to disable charge\n", SPRD_CHG_TAG, __func__);
}

static void sgm41516_reset_timer(void)
{
	int ret = 0;

	dprintf(INFO,"%s: %s\n", SPRD_CHG_TAG, __func__);
	ret = sgm41516_update_reg_bits(SGM41516_REG_01,
				       SGM41516_WDT_RESET_MASK,
				       SGM41516_WDT_RESET << SGM41516_WDT_RESET_SHIFT);
	if (ret)
		dprintf(ALWAYS,"%s: %s, failed to reset watchdog timer, ret = %d\n",
			SPRD_CHG_TAG, __func__, ret);
}

static int sgm41516_enable_batfet_reset(bool enable)
{
	u8 reg_val = SGM41516_BATFET_RST_DISABLE;

	dprintf(INFO,"%s: %s, enable = %d\n", SPRD_CHG_TAG, __func__, enable);
	if (enable)
		reg_val = SGM41516_BATFET_RST_ENABLE;

	return sgm41516_update_reg_bits(SGM41516_REG_07,
					SGM41516_BATFET_RST_EN_MASK,
					reg_val << SGM41516_BATFET_RST_EN_SHIFT);
}

static int sgm41516_is_support_power_path(void)
{
	return true;
}

static void sgm41516_chg_cmd(enum sprdchg_cmd cmd, int value)
{
	int ret = 0;

	switch (cmd) {
	case CHG_SET_CURRENT:
		ret = sgm41516_set_current(value);
		if (ret)
			dprintf(ALWAYS,"%s: %s, failed to set Ibat %d mA, ret = %d\n",
				SPRD_CHG_TAG, __func__, value, ret);

		break;
	case CHG_SET_LIMIT_CURRENT:
		ret = sgm41516_set_limit_current(value);
		if (ret)
			dprintf(ALWAYS,"%s: %s, failed to set Ibus %d mA, ret = %d\n",
				SPRD_CHG_TAG, __func__, value, ret);

		break;
	case CHG_SET_PRE_CURRENT:
		ret = sgm41516_set_pre_charge_current(value);
		if (ret)
			dprintf(ALWAYS,"%s: %s, failed to set pre-charge Ibat %d mA, ret = %d\n",
				SPRD_CHG_TAG, __func__, value, ret);

		break;
	case CHG_SET_POWER_PATH:
		ret = sgm41516_enalbe_power_path(!!value);
		if (ret)
			dprintf(ALWAYS,"%s: %s, failed to %s power path, ret = %d\n",
				SPRD_CHG_TAG, __func__, !!value ? "enable" : "disable", ret);

		break;
	default:
		break;
	}
}

static struct sprdchg_ic_operations sgm41516_op ={
	.chg_start = sgm41516_start_charge,
	.chg_stop = sgm41516_stop_charge,
	.timer_callback = sgm41516_reset_timer,
	.chg_cmd = sgm41516_chg_cmd,
	.is_support_power_path = sgm41516_is_support_power_path,
};

void sprdchg_sgm41516_init(void)
{
	int ret;

	ret = sgm41516_enable_batfet_reset(false);
	if (ret)
		dprintf(ALWAYS,"%s: %s, failed to disable BATFET reset, ret = %d\n",
			SPRD_CHG_TAG, __func__, ret);

	ret = sgm41516_enable_wdg(false);
	if (ret)
		dprintf(ALWAYS,"%s: %s, failed to disable watchdog, ret = %d\n",
			SPRD_CHG_TAG, __func__, ret);

	sprdchg_register_ops(&sgm41516_op);
	dprintf(INFO,"%s: %s\n", SPRD_CHG_TAG, __func__);
}
