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

#include "sprd_chg_helper.h"
#include <errno.h>
#include <i2c.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>
#include <lk/debug.h>

/* Register 00h */
#define BQ2560X_REG_00				0x00
#define REG00_ENHIZ_MASK			0x80
#define REG00_ENHIZ_SHIFT			7
#define	REG00_HIZ_ENABLE			1
#define	REG00_HIZ_DISABLE			0

#define	REG00_STAT_CTRL_MASK			0x60
#define REG00_STAT_CTRL_SHIFT			5
#define	REG00_STAT_CTRL_STAT			0
#define	REG00_STAT_CTRL_ICHG			1
#define	REG00_STAT_CTRL_IINDPM			2
#define	REG00_STAT_CTRL_DISABLE			3

#define REG00_IINLIM_MASK			0x1f
#define REG00_IINLIM_SHIFT			0
#define	REG00_IINLIM_LSB			100
#define	REG00_IINLIM_BASE			100
#define REG00_IINLIM_OFFSET			100
#define REG00_IINLIM_CURRENT_MAX		3200
#define REG00_IINLIM_CURRENT_OFFSET		100

/* Register 01h */
#define BQ2560X_REG_01				0x01
#define REG01_PFM_DIS_MASK			0x80
#define	REG01_PFM_DIS_SHIFT			7
#define	REG01_PFM_ENABLE			0
#define	REG01_PFM_DISABLE			1

#define REG01_WDT_RESET_MASK			0x40
#define REG01_WDT_RESET_SHIFT			6
#define REG01_WDT_RESET				1

#define	REG01_OTG_CONFIG_MASK			0x20
#define	REG01_OTG_CONFIG_SHIFT			5
#define	REG01_OTG_ENABLE			1
#define	REG01_OTG_DISABLE			0

#define REG01_CHG_CONFIG_MASK			0x10
#define REG01_CHG_CONFIG_SHIFT			4
#define REG01_CHG_DISABLE			0
#define REG01_CHG_ENABLE			1

#define REG01_SYS_MINV_MASK			0x0e
#define REG01_SYS_MINV_SHIFT			1

#define	REG01_MIN_VBAT_SEL_MASK			0x01
#define	REG01_MIN_VBAT_SEL_SHIFT		0
#define	REG01_MIN_VBAT_2P8V			0
#define	REG01_MIN_VBAT_2P5V			1

/* Register 0x02*/
#define BQ2560X_REG_02				0x02
#define	REG02_BOOST_LIM_MASK			0x80
#define	REG02_BOOST_LIM_SHIFT			7
#define	REG02_BOOST_LIM_0P5A			0
#define	REG02_BOOST_LIM_1P2A			1

#define	REG02_Q1_FULLON_MASK			0x40
#define	REG02_Q1_FULLON_SHIFT			6
#define	REG02_Q1_FULLON_ENABLE			1
#define	REG02_Q1_FULLON_DISABLE			0

#define REG02_ICHG_MASK				0x3f
#define REG02_ICHG_SHIFT			0
#define REG02_ICHG_BASE				0
#define REG02_ICHG_LSB				60
#define REG02_ICHG_MAX				3000

/* Register 0x03*/
#define BQ2560X_REG_03				0x03
#define REG03_IPRECHG_MASK			0xf0
#define REG03_IPRECHG_SHIFT			4
#define REG03_IPRECHG_BASE			60
#define REG03_IPRECHG_LSB			60
#define REG03_IPRECHG_CURRENT_MAX		780
#define REG03_IPRECHG_CURRENT_MAX_SGM41513		240
#define REG03_IPRECHG_CURRENT_OFFSET		60

#define REG03_ITERM_MASK			0x0f
#define REG03_ITERM_SHIFT			0
#define REG03_ITERM_BASE			60
#define REG03_ITERM_LSB				60

/* Register 0x04*/
#define BQ2560X_REG_04				0x04
#define REG04_VREG_MASK				0xf8
#define REG04_VREG_SHIFT			3
#define REG04_VREG_BASE				3856
#define REG04_VREG_LSB				32

#define	REG04_TOPOFF_TIMER_MASK			0x06
#define	REG04_TOPOFF_TIMER_SHIFT		1
#define	REG04_TOPOFF_TIMER_DISABLE		0
#define	REG04_TOPOFF_TIMER_15M			1
#define	REG04_TOPOFF_TIMER_30M			2
#define	REG04_TOPOFF_TIMER_45M			3

#define REG04_VRECHG_MASK			0x01
#define REG04_VRECHG_SHIFT			0
#define REG04_VRECHG_100MV			0
#define REG04_VRECHG_200MV			1

/* Register 0x05*/
#define BQ2560X_REG_05				0x05
#define REG05_EN_TERM_MASK			0x80
#define REG05_EN_TERM_SHIFT			7
#define REG05_TERM_ENABLE			1
#define REG05_TERM_DISABLE			0

#define REG05_WDT_MASK				0x30
#define REG05_WDT_SHIFT				4
#define REG05_WDT_DISABLE			0
#define REG05_WDT_40S				1
#define REG05_WDT_80S				2
#define REG05_WDT_160S				3
#define REG05_WDT_BASE				0
#define REG05_WDT_LSB				40

#define REG05_EN_TIMER_MASK			0x08
#define REG05_EN_TIMER_SHIFT			3
#define REG05_CHG_TIMER_ENABLE			1
#define REG05_CHG_TIMER_DISABLE			0

#define REG05_CHG_TIMER_MASK			0x04
#define REG05_CHG_TIMER_SHIFT			2
#define REG05_CHG_TIMER_5HOURS			0
#define REG05_CHG_TIMER_10HOURS			1

#define	REG05_TREG_MASK				0x02
#define	REG05_TREG_SHIFT			1
#define	REG05_TREG_90C				0
#define	REG05_TREG_110C				1

#define REG05_JEITA_ISET_MASK			0x01
#define REG05_JEITA_ISET_SHIFT			0
#define REG05_JEITA_ISET_50PCT			0
#define REG05_JEITA_ISET_20PCT			1

/* Register 0x06*/
#define BQ2560X_REG_06				0x06
#define	REG06_OVP_MASK				0xc0
#define	REG06_OVP_SHIFT				0x6
#define	REG06_OVP_5P5V				0
#define	REG06_OVP_6P2V				1
#define	REG06_OVP_10P5V				2
#define	REG06_OVP_14P3V				3

#define	REG06_BOOSTV_MASK			0x30
#define	REG06_BOOSTV_SHIFT			4
#define	REG06_BOOSTV_4P85V			0
#define	REG06_BOOSTV_5V				1
#define	REG06_BOOSTV_5P15V			2
#define	REG06_BOOSTV_5P3V			3

#define	REG06_VINDPM_MASK			0x0f
#define	REG06_VINDPM_SHIFT			0
#define	REG06_VINDPM_BASE			3900
#define	REG06_VINDPM_LSB			100

/* Register 0x07*/
#define BQ2560X_REG_07				0x07
#define REG07_FORCE_DPDM_MASK			0x80
#define REG07_FORCE_DPDM_SHIFT			7
#define REG07_FORCE_DPDM			1

#define REG07_TMR2X_EN_MASK			0x40
#define REG07_TMR2X_EN_SHIFT			6
#define REG07_TMR2X_ENABLE			1
#define REG07_TMR2X_DISABLE			0

#define REG07_BATFET_DIS_MASK			0x20
#define REG07_BATFET_DIS_SHIFT			5
#define REG07_BATFET_OFF			1
#define REG07_BATFET_ON				0

#define REG07_JEITA_VSET_MASK			0x10
#define REG07_JEITA_VSET_SHIFT			4
#define REG07_JEITA_VSET_4100			0
#define REG07_JEITA_VSET_VREG			1

#define	REG07_BATFET_DLY_MASK			0x08
#define	REG07_BATFET_DLY_SHIFT			3
#define	REG07_BATFET_DLY_0S			0
#define	REG07_BATFET_DLY_10S			1

#define	REG07_BATFET_RST_EN_MASK		0x04
#define	REG07_BATFET_RST_EN_SHIFT		2
#define	REG07_BATFET_RST_DISABLE		0
#define	REG07_BATFET_RST_ENABLE			1

#define	REG07_VDPM_BAT_TRACK_MASK		0x03
#define	REG07_VDPM_BAT_TRACK_SHIFT		0
#define	REG07_VDPM_BAT_TRACK_DISABLE		0
#define	REG07_VDPM_BAT_TRACK_200MV		1
#define	REG07_VDPM_BAT_TRACK_250MV		2
#define	REG07_VDPM_BAT_TRACK_300MV		3

/* Register 0x08*/
#define BQ2560X_REG_08				0x08
#define REG08_VBUS_STAT_MASK			0xe0
#define REG08_VBUS_STAT_SHIFT			5
#define REG08_VBUS_TYPE_NONE			0
#define REG08_VBUS_TYPE_USB			1
#define REG08_VBUS_TYPE_ADAPTER			3
#define REG08_VBUS_TYPE_OTG			7

#define REG08_CHRG_STAT_MASK			0x18
#define REG08_CHRG_STAT_SHIFT			3
#define REG08_CHRG_STAT_IDLE			0
#define REG08_CHRG_STAT_PRECHG			1
#define REG08_CHRG_STAT_FASTCHG			2
#define REG08_CHRG_STAT_CHGDONE			3

#define REG08_PG_STAT_MASK			0x04
#define REG08_PG_STAT_SHIFT			2
#define REG08_POWER_GOOD			1

#define REG08_THERM_STAT_MASK			0x02
#define REG08_THERM_STAT_SHIFT			1

#define REG08_VSYS_STAT_MASK			0x01
#define REG08_VSYS_STAT_SHIFT			0
#define REG08_IN_VSYS_STAT			1

/* Register 0x09*/
#define BQ2560X_REG_09				0x09
#define REG09_FAULT_WDT_MASK			0x80
#define REG09_FAULT_WDT_SHIFT			7
#define REG09_FAULT_WDT				1

#define REG09_FAULT_BOOST_MASK			0x40
#define REG09_FAULT_BOOST_SHIFT			6

#define REG09_FAULT_CHRG_MASK			0x30
#define REG09_FAULT_CHRG_SHIFT			4
#define REG09_FAULT_CHRG_NORMAL			0
#define REG09_FAULT_CHRG_INPUT			1
#define REG09_FAULT_CHRG_THERMAL		2
#define REG09_FAULT_CHRG_TIMER			3

#define REG09_FAULT_BAT_MASK			0x08
#define REG09_FAULT_BAT_SHIFT			3
#define	REG09_FAULT_BAT_OVP			1

#define REG09_FAULT_NTC_MASK			0x07
#define REG09_FAULT_NTC_SHIFT			0
#define	REG09_FAULT_NTC_NORMAL			0
#define REG09_FAULT_NTC_WARM			2
#define REG09_FAULT_NTC_COOL			3
#define REG09_FAULT_NTC_COLD			5
#define REG09_FAULT_NTC_HOT			6

/* Register 0x0A */
#define BQ2560X_REG_0A				0x0a
#define	REG0A_VBUS_GD_MASK			0x80
#define	REG0A_VBUS_GD_SHIFT			7
#define	REG0A_VBUS_GD				1

#define	REG0A_VINDPM_STAT_MASK			0x40
#define	REG0A_VINDPM_STAT_SHIFT			6
#define	REG0A_VINDPM_ACTIVE			1

#define	REG0A_IINDPM_STAT_MASK			0x20
#define	REG0A_IINDPM_STAT_SHIFT			5
#define	REG0A_IINDPM_ACTIVE			1

#define	REG0A_TOPOFF_ACTIVE_MASK		0x08
#define	REG0A_TOPOFF_ACTIVE_SHIFT		3
#define	REG0A_TOPOFF_ACTIVE			1

#define	REG0A_ACOV_STAT_MASK			0x04
#define	REG0A_ACOV_STAT_SHIFT			2
#define	REG0A_ACOV_ACTIVE			1

#define	REG0A_VINDPM_INT_MASK			0x02
#define	REG0A_VINDPM_INT_SHIFT			1
#define	REG0A_VINDPM_INT_ENABLE			0
#define	REG0A_VINDPM_INT_DISABLE		1

#define	REG0A_IINDPM_INT_MASK			0x01
#define	REG0A_IINDPM_INT_SHIFT			0
#define	REG0A_IINDPM_INT_ENABLE			0
#define	REG0A_IINDPM_INT_DISABLE		1

#define	REG0A_INT_MASK_MASK			0x03
#define	REG0A_INT_MASK_SHIFT			0

#define	BQ2560X_REG_0B				0x0b
#define	REG0B_REG_RESET_MASK			0x80
#define	REG0B_REG_RESET_SHIFT			7
#define	REG0B_REG_RESET				1

#define REG0B_PART_ID_MASK			0x78
#define REG0B_PART_ID_SHIFT			3
#define SGM41511_PART_ID_VALUE				2
#define SGM41513_PART_ID_VALUE				0
#define REG0B_PN_MASK				0x78
#define REG0B_PN_SHIFT				3

#define REG0B_DEV_REV_MASK			0x03
#define REG0B_DEV_REV_SHIFT			0

#define I2C_SPEED				100000
#define SLAVE_ADDR				0x6b

static int i2c_bus_num = SPRDCHG_I2C_BUS;
static uint32_t slave_addr = SLAVE_ADDR;

static const unsigned int IPRECHG_CURRENT_STABLE[] = {
	5, 10, 15, 20, 30, 40, 50, 60,
	80, 100, 120, 140, 160, 180, 200, 240
};
#ifdef CONFIG_DM_BQ2560X
struct sprd_bq2560x_dm_data {
	int i2c_num;
};
static struct sprd_bq2560x_dm_data dm_data = {
	.i2c_num = -1,
};
struct udevice *charger;
static int sprd_dm_bq2560x_i2c_init(void)
{
	int ret;
	u8 value;
	if (dm_data.i2c_num != -1) {
		ret = i2c_get_chip_for_busnum(dm_data.i2c_num,
					      slave_addr, 1,
					      &charger);
		if (ret) {
			pr_err("%s: i2c%d failed to get\n",
			       __func__, dm_data.i2c_num);
			return ret;
		}
	} else {
		pr_err("%s:failed to set i2c bus num\n", __func__);
		return -EINVAL;
	}
	ret = dm_i2c_set_bus_speed(dev_get_parent(charger), I2C_SPEED);
	if (ret) {
		pr_err("%s: failed to set i2c%d speed\n",
		       __func__, dm_data.i2c_num);
		return ret;
	}
	return 0;
}
static int bq2560x_write_reg(int reg, u8 val)
{
	dm_i2c_reg_write(charger, reg, val);
	return 0;
}
static int bq2560x_read_reg(int reg, u8 *value)
{
	u8 val;
	int ret;
	ret = dm_i2c_reg_read(charger, reg);
	if (ret < 0) {
		pr_err("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}
	*value = (u8)ret;
	debugf("sprd_chg: %s reg = %d value = %d/%x\n", __func__, reg, ret, ret);
	return 0;
}
#else
static int bq2560x_i2c_init(void)
{
	return 0;
}
static int bq2560x_write_reg(u8 reg, u8 val)
{
	int ret;
	uint8_t buf[2] = {0};

	buf[0] = reg;
	buf[1] = val;
	ret = i2c_send(i2c_bus_num, slave_addr, buf, 2);
	if (ret < 0) {
		dprintf(ALWAYS,"sprd_chg: %s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

	return 0;
}

static int bq2560x_read_reg(u8 reg, u8 *value)
{
	int ret;
	uint8_t reg_addr[2] = {0};

	reg_addr[0] = reg;
	ret = i2c_read_write(i2c_bus_num, slave_addr, reg_addr, 1, value, 1);
	if (ret < 0) {
		dprintf(ALWAYS,"sprd_chg: %s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

	dprintf(INFO,"sprd_chg: %s reg = 0x%x, value = %d/0x%x\n", __func__, reg, *value, *value);

	return 0;
}
#endif

static int bq2560x_get_vendor_id_value(u8 *data)
{
	int ret = 0;
	u8 reg_part_val = 0;
	ret = bq2560x_read_reg(BQ2560X_REG_0B, &reg_part_val);
	if (ret < 0) {
		printf("[%s]line=%d: Failed to get vendor id, ret=%d\n",
			__FUNCTION__, __LINE__, ret);
		return ret;
	}
	reg_part_val = (reg_part_val & REG0B_PART_ID_MASK) >> REG0B_PART_ID_SHIFT;
	*data = reg_part_val;
	return ret;
}
static void bq2560x_set_value(u8 reg, u8 reg_bit, u8 reg_shift, u8 val)
{
	u8 tmp = 0;

	bq2560x_read_reg(reg, &tmp);
	tmp = (tmp & (~reg_bit)) | (val << reg_shift);
	bq2560x_write_reg(reg, tmp);
}

static u8 bq2560x_get_value(u8 reg, u8 reg_bit, u8 reg_shift)
{
	u8 reg_value = 0;

	bq2560x_read_reg(reg, &reg_value);
	reg_value = (reg_value & reg_bit) >> reg_shift;

	return reg_value;
}

static void chg_bq2560x_set_chg_cur(u32 cur)
{
	u8 reg_value, vendor_id = 0;
	int ret;

	if (cur > REG02_ICHG_MAX)
		cur = REG02_ICHG_MAX;

	ret = bq2560x_get_vendor_id_value(&vendor_id);
	if (vendor_id == SGM41513_PART_ID_VALUE)
	{
		if (cur <= 40)
			reg_value = cur / 5;
		else if (cur < 50)
			reg_value = 0x08;
		else if (cur <= 110)
			reg_value = 0x08 + (cur -40) / 10;
		else if (cur < 130)
			reg_value = 0x0F;
		else if (cur <= 270)
			reg_value = 0x0F + (cur -110) / 20;
		else if (cur < 300)
			reg_value = 0x17;
		else if (cur <= 540)
			reg_value = 0x17 + (cur -270) / 30;
		else if (cur < 600)
			reg_value = 0x20;
		else if (cur <= 1500)
			reg_value = 0x20 + (cur -540) / 60;
		else if (cur < 1620)
			reg_value = 0x30;
		else if (cur <= 2940)
			reg_value = 0x30 + (cur -1500) / 120;
		else 
			reg_value = 0x3d;
	}
	else
	{
	reg_value = cur / REG02_ICHG_LSB;
	}

	bq2560x_set_value(BQ2560X_REG_02, REG02_ICHG_MASK,
			  REG02_ICHG_SHIFT, reg_value);
}

static void chg_bq2560x_set_limit_cur(u32 limit)
{
	u8 reg_value;

	if (limit > REG00_IINLIM_CURRENT_MAX)
		limit = REG00_IINLIM_CURRENT_MAX;

	limit = limit - REG00_IINLIM_CURRENT_OFFSET;
	reg_value = limit / REG00_IINLIM_BASE;
	bq2560x_set_value(BQ2560X_REG_00, REG00_IINLIM_MASK,
			  REG00_IINLIM_SHIFT, reg_value);
}

static int bq2560x_set_prechg(u32 ichg)
{
	u8 reg_value, vendor_id = 0;
	int ret;
	ret = bq2560x_get_vendor_id_value(&vendor_id);
	if (vendor_id == SGM41513_PART_ID_VALUE)
	{
		if (ichg > REG03_IPRECHG_CURRENT_MAX_SGM41513)
			ichg = REG03_IPRECHG_CURRENT_MAX_SGM41513;
		for(reg_value = 1; reg_value < 16 && ichg >= IPRECHG_CURRENT_STABLE[reg_value]; reg_value++)
			;
		reg_value--;
	}
	else
	{
	if (ichg > REG03_IPRECHG_CURRENT_MAX)
		ichg = REG03_IPRECHG_CURRENT_MAX;

	ichg = ichg - REG03_IPRECHG_CURRENT_OFFSET;
	reg_value = ichg / REG03_IPRECHG_LSB;
	}
	bq2560x_set_value(BQ2560X_REG_03, REG03_IPRECHG_MASK,
			  REG03_IPRECHG_SHIFT, reg_value);

	return 0;
}

static void bq2560x_set_power_path(int val)
{
	dprintf(INFO,"sprd_chg: %s val = %d\n", __func__, val);

	if (val)
		bq2560x_set_value(BQ2560X_REG_00, REG00_ENHIZ_MASK, REG00_ENHIZ_SHIFT,
				  REG00_HIZ_DISABLE);
	else
		bq2560x_set_value(BQ2560X_REG_00, REG00_ENHIZ_MASK, REG00_ENHIZ_SHIFT,
				  REG00_HIZ_ENABLE);

	bq2560x_set_value(BQ2560X_REG_05, REG05_WDT_MASK,
			  REG05_WDT_SHIFT, REG05_WDT_DISABLE);
}

static int bq2560x_is_support_power_path(void)
{
	return true;
}

static int chg_bq2560x_cmd(enum sprdchg_cmd cmd, int value)
{
	switch (cmd) {
	case CHG_SET_CURRENT:
		chg_bq2560x_set_chg_cur(value);
		break;
	case CHG_SET_LIMIT_CURRENT:
		chg_bq2560x_set_limit_cur(value);
		break;
	case CHG_SET_PRE_CURRENT:
		bq2560x_set_prechg(value);
		break;
	case CHG_SET_POWER_PATH:
		bq2560x_set_power_path(value);
		break;
	default:
		break;
	}
	return 0;
}

static void chg_bq2560x_init(void)
{
	int ret;
	u8 vendor_id = 0;
#ifdef CONFIG_DM_BQ2560X
	ret = sprd_dm_bq2560x_i2c_init();
#else
	ret = bq2560x_i2c_init();
#endif
	if (ret) {
		dprintf(ALWAYS,"sprd_chg: %s fail\n", __func__);
		return;
	}
	dprintf(INFO,"sprd_chg: %s\n", __func__);
	ret = bq2560x_get_vendor_id_value(&vendor_id);
	if (vendor_id == SGM41513_PART_ID_VALUE)
	{
		slave_addr = 0x1A;
	}
	else
	{
		slave_addr = SLAVE_ADDR;
	}
}
static void chg_bq2560x_enable_chg(int val)
{
	dprintf(INFO,"sprd_chg: %s\n", __func__);

	bq2560x_set_value(BQ2560X_REG_00, REG00_ENHIZ_MASK,
			  REG00_ENHIZ_SHIFT, REG00_HIZ_DISABLE);

	bq2560x_set_value(BQ2560X_REG_01, REG01_CHG_CONFIG_MASK,
			  REG01_CHG_CONFIG_SHIFT, REG01_CHG_ENABLE);
}

static void chg_bq2560x_disable_chg(int val)
{
	dprintf(INFO,"sprd_chg: %s\n", __func__);
	bq2560x_set_value(BQ2560X_REG_01, REG01_CHG_CONFIG_MASK,
			  REG01_CHG_CONFIG_SHIFT, REG01_CHG_DISABLE);
}

static void chg_bq2560x_reset_timer(void)
{
	dprintf(INFO,"sprd_chg: %s\n", __func__);
	bq2560x_set_value(BQ2560X_REG_01, REG01_WDT_RESET_MASK,
			  REG01_WDT_RESET_SHIFT, REG01_WDT_RESET);
}

static void chg_bq2560x_disable_batfet_rst_en(void)
{
	bq2560x_set_value(BQ2560X_REG_07, REG07_BATFET_RST_EN_MASK,
			  REG07_BATFET_RST_EN_SHIFT, REG07_BATFET_RST_DISABLE);
}

static void chg_bq2560x_disable_watchdog(void)
{
	bq2560x_set_value(BQ2560X_REG_05, REG05_WDT_MASK,
			  REG05_WDT_SHIFT, REG05_WDT_DISABLE);
}

static struct sprdchg_ic_operations bq2560x_op = {
	//.ic_init = chg_bq2560x_init,
	.chg_start = chg_bq2560x_enable_chg,
	.chg_stop = chg_bq2560x_disable_chg,
	.timer_callback = chg_bq2560x_reset_timer,
	.chg_cmd = chg_bq2560x_cmd,
	.is_support_power_path = bq2560x_is_support_power_path,
};

#ifdef CONFIG_DM_BQ2560X
static int sprd_bq2560x_probe(struct udevice *dev)
{
	u32 i2c_id;
	int ret;
	ret = dev_read_u32(dev, "sprd,bq2560x-i2c-bus", &i2c_id);
	if (ret) {
		pr_err("sprd_chg: %s:failed to get i2c-bus ret = %d!\n", __func__, ret);
		return ret;
	}
	debugf("sprd_chg: i2c_id = %d\n", i2c_id);
	dm_data.i2c_num = i2c_id;
	return 0;
}
static int sprd_dm_bq2560x_init(void)
{
	struct udevice *devp;
	int ret;
	ret = uclass_get_device(UCLASS_CHARGER, 0, &devp);
	if (ret) {
		pr_err("sprd_chg: %s:failed to get device ret = %d", __func__, ret);
		return ret;
	}
	ret = sprd_dm_bq2560x_i2c_init();
  	if (ret) {
		pr_err("sprd_chg: %s:failed to init dm i2c ret = %d", __func__, ret);
		return ret;
        }
	return 0;
}
#endif
void sprdchg_bq2560x_init(void)
{
	int ret;
	u8 vendor_id = 0;
#ifdef CONFIG_DM_BQ2560X
	ret = sprd_dm_bq2560x_init();
#else
	ret = bq2560x_i2c_init();
#endif
	if (ret) {
		dprintf(ALWAYS, "sprd_chg: %s failed\n", __func__);
		return;
	}
	ret = bq2560x_get_vendor_id_value(&vendor_id);
	
	ret = bq2560x_get_vendor_id_value(&vendor_id);

	if (vendor_id == SGM41513_PART_ID_VALUE)
	{
		slave_addr = 0x1A;
	}
	else
	{
		slave_addr = SLAVE_ADDR;
	}
	chg_bq2560x_disable_batfet_rst_en();
	chg_bq2560x_disable_watchdog();
	sprdchg_register_ops(&bq2560x_op);
	dprintf(INFO,"sprd_chg: %s\n", __func__);
}
#ifdef CONFIG_DM_BQ2560X
static const struct udevice_id sprd_bq2560x_ids[] = {
	{.compatible = "sprd,bq2560x-charger"},
	{ }
};
U_BOOT_DRIVER(bq2560x) = {
	.name = "sprd-bq2560x",
	.id = UCLASS_CHARGER,
	.of_match = sprd_bq2560x_ids,
	.probe = sprd_bq2560x_probe,
};
UCLASS_DRIVER(charger)= {
	.name = "charger",
	.id = UCLASS_CHARGER,
};
#endif
