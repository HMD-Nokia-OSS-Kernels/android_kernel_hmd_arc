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

#define SY6970_REG_00				0x00
#define SY6970_REG_01				0x01
#define SY6970_REG_02				0x02
#define SY6970_REG_03				0x03
#define SY6970_REG_04				0x04
#define SY6970_REG_05				0x05
#define SY6970_REG_06				0x06
#define SY6970_REG_07				0x07
#define SY6970_REG_08				0x08
#define SY6970_REG_09				0x09
#define SY6970_REG_0A				0x0a
#define SY6970_REG_0B				0x0b
#define SY6970_REG_0C				0x0c
#define SY6970_REG_0D				0x0d
#define SY6970_REG_0E				0x0e
#define SY6970_REG_0F				0x0f
#define SY6970_REG_10				0x10
#define SY6970_REG_11				0x11
#define SY6970_REG_12				0x12
#define SY6970_REG_13				0x13
#define SY6970_REG_14				0x14
#define SY6970_REG_NUM				21

/* Register 0x00 */
#define REG00_ENHIZ_MASK			0x80
#define REG00_ENHIZ_SHIFT			7
#define REG00_EN_ILIM_MASK			0x40
#define REG00_EN_ILIM_SHIFT			6
#define REG00_IINLIM_MASK			0x3f
#define REG00_IINLIM_SHIFT			0

/* Register 0x01*/
#define REG01_BHOT_MASK				0xc0
#define REG01_BHOT_SHIFT			6
#define REG01_BCOLD_MASK			0x20
#define REG01_BCOLD_SHIFT			5
#define REG01_VINDPM_OS_MASK			0x1f
#define REG01_VINDPM_OS_SHIFT			0

/* Register 0x02*/
#define REG02_CONV_START_MASK			0x80
#define REG02_CONV_START_SHIFT			7
#define REG02_CONV_RATE_MASK			0x40
#define REG02_CONV_RATE_SHIFT			6
#define REG02_BOOST_FREQ_MASK			0x20
#define REG02_BOOST_FREQ_SHIFT			5
#define REG02_ICO_EN_MASK			0x10
#define REG02_ICO_EN_SHIFT			4
#define REG02_HVDCP_EN_MASK			0x08
#define REG02_HVDCP_EN_SHIFT			3
#define REG02_MAXC_EN_MASK			0x04
#define REG02_MAXC_EN_SHIFT			2
#define REG02_FORCE_DPDM_MASK			0x02
#define REG02_FORCE_DPDM_SHIFT			1
#define REG02_AUTO_DPDM_EN_MASK			0x01
#define REG02_AUTO_DPDM_EN_SHIFT		0

/* Register 0x03 */
#define REG03_BAT_LOADEN_MASK			0x80
#define REG03_BAT_LOADEN_SHIFT			7
#define REG03_WDT_RESET_MASK			0x40
#define REG03_WDT_RESET_SHIFT			6
#define REG03_OTG_CONFIG_MASK			0x20
#define REG03_OTG_CONFIG_SHIFT			5
#define REG03_CHG_CONFIG_MASK			0x10
#define REG03_CHG_CONFIG_SHIFT			4
#define REG03_SYS_MINV_MASK			0x0e
#define REG03_SYS_MINV_SHIFT			1

/* Register 0x04*/
#define REG04_EN_PUMPX_MASK			0x80
#define REG04_EN_PUMPX_SHIFT			7
#define REG04_ICHG_MASK				0x7f
#define REG04_ICHG_SHIFT			0

/* Register 0x05*/
#define REG05_IPRECHG_MASK			0xf0
#define REG05_IPRECHG_SHIFT			4
#define REG05_ITERM_MASK			0x0f
#define REG05_ITERM_SHIFT			0

/* Register 0x06*/
#define REG06_VREG_MASK				0xfc
#define REG06_VREG_SHIFT			2
#define REG06_BATLOWV_MASK			0x02
#define REG06_BATLOWV_SHIFT			1
#define REG06_VRECHG_MASK			0x01
#define REG06_VRECHG_SHIFT			0

/* Register 0x07*/
#define REG07_EN_TERM_MASK			0x80
#define REG07_EN_TERM_SHIFT			7
#define REG07_STAT_DIS_MASK			0x40
#define REG07_STAT_DIS_SHIFT			6
#define REG07_WDT_MASK				0x30
#define REG07_WDT_SHIFT				4
#define REG07_EN_TIMER_MASK			0x08
#define REG07_EN_TIMER_SHIFT			3
#define REG07_CHG_TIMER_MASK			0x06
#define REG07_CHG_TIMER_SHIFT			1
#define REG07_JEITA_ISET_MASK			0x01
#define REG07_JEITA_ISET_SHIFT			0

/* Register 0x08*/
#define REG08_IR_COMP_MASK			0xe0
#define REG08_IR_COMP_SHIFT			5
#define REG08_VCLAMP_MASK			0x1c
#define REG08_VCLAMP_SHIFT			2
#define REG08_TREG_MASK				0x03
#define REG08_TREG_SHIFT			2

/* Register 0x09*/
#define REG09_FORCE_ICO_MASK			0x80
#define REG09_FORCE_ICO_SHIFT			7
#define REG09_TMR2X_EN_MASK			0x40
#define REG09_TMR2X_EN_SHIFT			6
#define REG09_BATFET_DIS_MASK			0x20
#define REG09_BATFET_DIS_SHIFT			5
#define REG09_JEITA_VSET_MASK			0x10
#define REG09_JEITA_VSET_SHIFT			4
#define REG09_BATFET_DLY_MASK			0x08
#define REG09_BATFET_DLY_SHIFT			3
#define REG09_BATFET_RST_EN_MASK		0x04
#define REG09_BATFET_RST_EN_SHIFT		2
#define REG09_PUMPX_UP_MASK			0x02
#define REG09_PUMPX_UP_SHIFT			1
#define REG09_PUMPX_DN_MASK			0x01
#define REG09_PUMPX_DN_SHIFT			0

/* Register 0x0A*/
#define REG0A_BOOSTV_MASK			0xf0
#define REG0A_BOOSTV_SHIFT			4
#define REG0A_BOOSTV_LIM_MASK			0x07
#define REG0A_BOOSTV_LIM_SHIFT			0

/* Register 0x0B*/
#define REG0B_VBUS_STAT_MASK			0xe0
#define REG0B_VBUS_STAT_SHIFT			5
#define REG0B_CHRG_STAT_MASK			0x18
#define REG0B_CHRG_STAT_SHIFT			3
#define REG0B_PG_STAT_MASK			0x04
#define REG0B_PG_STAT_SHIFT			2
#define REG0B_VSYS_STAT_MASK			0x01
#define REG0B_VSYS_STAT_SHIFT			0

/* Register 0x0C*/
#define REG0C_FAULT_WDT_MASK			0x80
#define REG0C_FAULT_WDT_SHIFT			7
#define REG0C_FAULT_BOOST_MASK			0x40
#define REG0C_FAULT_BOOST_SHIFT			6
#define REG0C_FAULT_CHRG_MASK			0x30
#define REG0C_FAULT_CHRG_SHIFT			4
#define REG0C_FAULT_BAT_MASK			0x08
#define REG0C_FAULT_BAT_SHIFT			3
#define REG0C_FAULT_NTC_MASK			0x07
#define REG0C_FAULT_NTC_SHIFT			0

/* Register 0x0D*/
#define REG0D_FORCE_VINDPM_MASK			0x80
#define REG0D_FORCE_VINDPM_SHIFT		7
#define REG0D_VINDPM_MASK			0x7f
#define REG0D_VINDPM_SHIFT			0

/* Register 0x0E*/
#define REG0E_THERM_STAT_MASK			0x80
#define REG0E_THERM_STAT_SHIFT			7
#define REG0E_BATV_MASK				0x7f
#define REG0E_BATV_SHIFT			0

/* Register 0x0F*/
#define REG0F_SYSV_MASK				0x7f
#define REG0F_SYSV_SHIFT			0

/* Register 0x10*/
#define REG10_TSPCT_MASK			0x7f
#define REG10_TSPCT_SHIFT			0

/* Register 0x11*/
#define REG11_VBUS_GD_MASK			0x80
#define REG11_VBUS_GD_SHIFT			7
#define REG11_VBUSV_MASK			0x7f
#define REG11_VBUSV_SHIFT			0

/* Register 0x12*/
#define REG12_ICHGR_MASK			0x7f
#define REG12_ICHGR_SHIFT			0

/* Register 0x13*/
#define REG13_VDPM_STAT_MASK			0x80
#define REG13_VDPM_STAT_SHIFT			7
#define REG13_IDPM_STAT_MASK			0x40
#define REG13_IDPM_STAT_SHIFT			6
#define REG13_IDPM_LIM_MASK			0x3f
#define REG13_IDPM_LIM_SHIFT			0

/* Register 0x14 */
#define REG14_REG_RESET_MASK			0x80
#define REG14_REG_RESET_SHIFT			7
#define REG14_REG_ICO_OP_MASK			0x40
#define REG14_REG_ICO_OP_SHIFT			6
#define REG14_PN_MASK				0x38
#define REG14_PN_SHIFT				3
#define REG14_TS_PROFILE_MASK			0x04
#define REG14_TS_PROFILE_SHIFT			2
#define REG14_DEV_REV_MASK			0x03
#define REG14_DEV_REV_SHIFT			0

#define REG00_HIZ_DISABLE			0
#define REG00_HIZ_ENABLE			1
#define REG00_EN_ILIM_DISABLE			0
#define REG00_EN_ILIM_ENABLE			1
#define REG00_IINLIM_OFFSET			100
#define REG00_IINLIM_STEP			50
#define REG00_IINLIM_MIN			100
#define REG00_IINLIM_MAX			3250

#define REG01_BHOT_VBHOT1			0
#define REG01_BHOT_VBHOT0			1
#define REG01_BHOT_VBHOT2			2
#define REG01_BHOT_DISABLE			3
#define REG01_BHOT_VBCOLD0			0
#define REG01_BHOT_VBCOLD1			1
#define REG01_VINDPM_OS_OFFSET			0
#define REG01_VINDPM_OS_STEP			100
#define REG01_VINDPM_OS_MIN			0
#define REG01_VINDPM_OS_MAX			3100

#define REG02_CONV_START_DISABLE		0
#define REG02_CONV_START_ENABLE			1
#define REG02_CONV_START_DISABLE		0
#define REG02_CONV_START_ENABLE			1
#define REG02_BOOST_FREQ_1p5M			0
#define REG02_BOOST_FREQ_500K			1
#define REG02_ICO_EN_DISABLE			0
#define REG02_ICO_EN_DENABLE			1
#define REG02_HVDCP_EN_DISABLE			0
#define REG02_HVDCP_EN_DENABLE			1
#define REG02_MAXC_EN_DISABLE			0
#define REG02_MAXC_EN_DENABLE			1
#define REG02_FORCE_DPDM_DISABLE		0
#define REG02_FORCE_DPDM_DENABLE		1
#define REG02_AUTO_DPDM_EN_DISABLE		0
#define REG02_AUTO_DPDM_EN_DENABLE		1

#define REG03_BAT_ENABLE			0
#define REG03_BAT_DISABLE			1
#define REG03_WDT_RESET				1
#define REG03_OTG_DISABLE			0
#define REG03_OTG_ENABLE			1
#define REG03_CHG_DISABLE			0
#define REG03_CHG_ENABLE			1
#define REG03_SYS_MINV_OFFSET			3000
#define REG03_SYS_MINV_STEP			100
#define REG03_SYS_MINV_MIN			3000
#define REG03_SYS_MINV_MAX			3700

#define REG04_EN_PUMPX_DISABLE			0
#define REG04_EN_PUMPX_ENABLE			1
#define REG04_ICHG_OFFSET			0
#define REG04_ICHG_STEP				64
#define REG04_ICHG_MIN				0
#define REG04_ICHG_MAX				5056

#define REG05_IPRECHG_OFFSET			64
#define REG05_IPRECHG_STEP			64
#define REG05_IPRECHG_MIN			64
#define REG05_IPRECHG_MAX			1024
#define REG05_ITERM_OFFSET			64
#define REG05_ITERM_STEP			64
#define REG05_ITERM_MIN				64
#define REG05_ITERM_MAX				1024

#define REG06_VREG_OFFSET			3840
#define REG06_VREG_STEP				16
#define REG06_VREG_MIN				3840
#define REG06_VREG_MAX				4608
#define REG06_BATLOWV_2p8v			0
#define REG06_BATLOWV_3v			1
#define REG06_VRECHG_100MV			0
#define REG06_VRECHG_200MV			1

#define REG07_TERM_DISABLE			0
#define REG07_TERM_ENABLE			1
#define REG07_STAT_DIS_DISABLE			1
#define REG07_STAT_DIS_ENABLE			0
#define REG07_WDT_DISABLE			0
#define REG07_WDT_40S				1
#define REG07_WDT_80S				2
#define REG07_WDT_160S				3
#define REG07_CHG_TIMER_DISABLE			0
#define REG07_CHG_TIMER_ENABLE			1
#define REG07_CHG_TIMER_5HOURS			0
#define REG07_CHG_TIMER_8HOURS			1
#define REG07_CHG_TIMER_12HOURS			2
#define REG07_CHG_TIMER_20HOURS			3
#define REG07_JEITA_ISET_50PCT			0
#define REG07_JEITA_ISET_20PCT			1

#define REG08_COMP_R_OFFSET			0
#define REG08_COMP_R_STEP			20
#define REG08_COMP_R_MIN			0
#define REG08_COMP_R_MAX			140
#define REG08_VCLAMP_OFFSET			0
#define REG08_VCLAMP_STEP			32
#define REG08_VCLAMP_MIN			0
#define REG08_VCLAMP_MAX			224
#define REG08_TREG_60				0
#define REG08_TREG_80				1
#define REG08_TREG_100				2
#define REG08_TREG_120				3

#define REG09_FORCE_ICO_DISABLE			0
#define REG09_FORCE_ICO_ENABLE			1
#define REG09_TMR2X_EN_DISABLE			0
#define REG09_TMR2X_EN_ENABLE			1
#define REG09_BATFET_DIS_DISABLE		0
#define REG09_BATFET_DIS_ENABLE			1
#define REG09_JEITA_VSET_DISABLE		0
#define REG09_JEITA_VSET_ENABLE			1
#define REG09_BATFET_DLY_DISABLE		0
#define REG09_BATFET_DLY_ENABLE			1
#define REG09_BATFET_RST_EN_DISABLE		0
#define REG09_BATFET_RST_EN_ENABLE		1
#define REG09_PUMPX_UP_DISABLE			0
#define REG09_PUMPX_UP_ENABLE			1
#define REG09_PUMPX_DN_DISABLE			0
#define REG09_PUMPX_DN_ENABLE			1

#define REG0A_BOOSTV_OFFSET			4550
#define REG0A_BOOSTV_STEP			64
#define REG0A_BOOSTV_MIN			4550
#define REG0A_BOOSTV_MAX			5510
#define REG0A_BOOSTV_LIM_500MA			0
#define REG0A_BOOSTV_LIM_750MA			1
#define REG0A_BOOSTV_LIM_1200MA			2
#define REG0A_BOOSTV_LIM_1400MA			3
#define REG0A_BOOSTV_LIM_1650MA			4
#define REG0A_BOOSTV_LIM_1875MA			5
#define REG0A_BOOSTV_LIM_2150MA			6
#define REG0A_BOOSTV_LIM_2450MA			7

#define REG0B_VBUS_TYPE_NONE			0
#define REG0B_VBUS_TYPE_USB_SDP			1
#define REG0B_VBUS_TYPE_USB_CDP			2
#define REG0B_VBUS_TYPE_USB_DCP			3
#define REG0B_VBUS_TYPE_DCP			4
#define REG0B_VBUS_TYPE_UNKNOWN			5
#define REG0B_VBUS_TYPE_ADAPTER			6
#define REG0B_VBUS_TYPE_OTG			7

#define REG0B_CHRG_STAT_IDLE			0
#define REG0B_CHRG_STAT_PRECHG			1
#define REG0B_CHRG_STAT_FASTCHG			2
#define REG0B_CHRG_STAT_CHGDONE			3
#define REG0B_POWER_NOT_GOOD			0
#define REG0B_POWER_GOOD			1
#define REG0B_NOT_IN_VSYS_STAT			0
#define REG0B_IN_VSYS_STAT			1

#define REG0C_FAULT_WDT				1
#define REG0C_FAULT_BOOST			1
#define REG0C_FAULT_CHRG_NORMAL			0
#define REG0C_FAULT_CHRG_INPUT			1
#define REG0C_FAULT_CHRG_THERMAL		2
#define REG0C_FAULT_CHRG_TIMER			3
#define REG0C_FAULT_BAT_OVP			1
#define REG0C_FAULT_NTC_NORMAL			0
#define REG0C_FAULT_NTC_WARM			2
#define REG0C_FAULT_NTC_COOL			3
#define REG0C_FAULT_NTC_COLD			5
#define REG0C_FAULT_NTC_HOT			6

#define REG0D_FORCE_VINDPM_DISABLE		0
#define REG0D_FORCE_VINDPM_ENABLE		1
#define REG0D_VINDPM_OFFSET			2600
#define REG0D_VINDPM_STEP			100
#define REG0D_VINDPM_MIN			3900
#define REG0D_VINDPM_MAX			15300

#define REG0E_THERM_STAT			1
#define REG0E_BATV_OFFSET			2304
#define REG0E_BATV_STEP				20
#define REG0E_BATV_MIN				2304
#define REG0E_BATV_MAX				4848

#define REG0F_SYSV_OFFSET			2304
#define REG0F_SYSV_STEP				20
#define REG0F_SYSV_MIN				2304
#define REG0F_SYSV_MAX				4848

#define REG10_TSPCT_OFFSET			21
#define REG10_TSPCT_STEP			0.465
#define REG10_TSPCT_MIN				21
#define REG10_TSPCT_MAX				80

#define REG11_VBUS_GD				1
#define REG11_VBUSV_OFFSET			2600
#define REG11_VBUSV_STEP			100
#define REG11_VBUSV_MIN				2600
#define REG11_VBUSV_MAX				15300

#define REG12_ICHGR_OFFSET			0
#define REG12_ICHGR_MIN				0
#define REG12_ICHGR_MAX				6350

#define REG13_VDPM_STAT				1
#define REG13_IDPM_STAT				1
#define REG13_IDPM_LIM_OFFSET			100
#define REG13_IDPM_LIM_STEP			50
#define REG13_IDPM_LIM_MIN			100
#define REG13_IDPM_LIM_MAX			3250

#define REG14_REG_RESET				1
#define REG14_REG_ICO_OP			1
#define REG14_PN				1

#define I2C_SPEED				100000
#define SLAVE_ADDR				0x6A

#define POWER_PATH_ENABLE			0
#define POWER_PATH_DISABLE			1

static int i2c_bus_num = SPRDCHG_I2C_BUS;

static int sy6970_write_reg(u8 reg, u8 val)
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

static int sy6970_read_reg(u8 reg, u8 *value)
{
	int ret;
	uint8_t reg_addr[2] = {0};

	reg_addr[0] = reg;
	ret = i2c_read_write(i2c_bus_num, SLAVE_ADDR, reg_addr, 1, value, 1);
	if (ret < 0) {
		dprintf(ALWAYS,"%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

	dprintf(ALWAYS,"sy6970_read_reg reg = 0x%x, value = %d/0x%x\n", reg, *value, *value);

	return 0;
}

static void sy6970_set_value(u8 reg, u8 reg_bit, u8 reg_shift, u8 val)
{
	u8 tmp = 0;

	sy6970_read_reg(reg, &tmp);
	tmp = tmp & (~reg_bit) | (val << reg_shift);
	sy6970_write_reg(reg, tmp);
}

static void chg_sy6970_set_chg_cur(u32 cur)
{
	u8 reg_value;

	if (cur <= REG04_ICHG_MIN)
		cur = REG04_ICHG_MIN;
	else if (cur >= REG04_ICHG_MAX)
		cur = REG04_ICHG_MAX;
	reg_value = cur / REG04_ICHG_STEP;

	sy6970_set_value(SY6970_REG_04, REG04_ICHG_MASK,
			 REG04_ICHG_SHIFT, reg_value);

}

static void chg_sy6970_set_limit_cur(u32 limit)
{
	u8 reg_value;

	sy6970_set_value(SY6970_REG_00, REG00_EN_ILIM_MASK,
			 REG00_EN_ILIM_SHIFT, REG00_EN_ILIM_DISABLE);

	if (limit >= REG00_IINLIM_MAX)
		limit = REG00_IINLIM_MAX;
	if (limit <= REG00_IINLIM_MIN)
		limit = REG00_IINLIM_MIN;

	reg_value = (limit - REG00_IINLIM_OFFSET) / REG00_IINLIM_STEP;
	sy6970_set_value(SY6970_REG_00, REG00_IINLIM_MASK,
			 REG00_IINLIM_SHIFT, reg_value);
}

static int chg_sy6970_set_prechg(u32 ichg)
{
	u8 reg_value;

	if (ichg <= REG05_IPRECHG_MIN)
		ichg = REG05_ITERM_MIN;
	else if (ichg >= REG05_IPRECHG_MAX)
		ichg = REG05_IPRECHG_MAX;

	ichg = ichg - REG05_IPRECHG_OFFSET;
	reg_value = ichg / REG05_IPRECHG_STEP;
	sy6970_set_value(SY6970_REG_05, REG05_IPRECHG_MASK,
			 REG05_IPRECHG_SHIFT, reg_value);

	return 0;
}

static void chg_sy6970_cmd(enum sprdchg_cmd cmd, int value)
{
	switch (cmd) {
	case CHG_SET_CURRENT:
		chg_sy6970_set_chg_cur(value);
		break;
	case CHG_SET_LIMIT_CURRENT:
		chg_sy6970_set_limit_cur(value);
		break;
	case CHG_SET_PRE_CURRENT:
		chg_sy6970_set_prechg(value);
		break;
	default:
		break;
	}
}

static void chg_sy6970_reset(void)
{
	sy6970_set_value(SY6970_REG_14, REG14_REG_RESET_MASK,
			 REG14_REG_RESET_SHIFT, REG14_REG_RESET);
}

static void chg_sy6970_enable_chg(void)
{
	sy6970_set_value(SY6970_REG_03, REG03_CHG_CONFIG_MASK,
			 REG03_CHG_CONFIG_SHIFT, REG03_CHG_ENABLE);
}

static void chg_sy6970_disable_chg(void)
{
	sy6970_set_value(SY6970_REG_03, REG03_CHG_CONFIG_MASK,
			 REG03_CHG_CONFIG_SHIFT, REG03_CHG_DISABLE);
}

static void chg_sy6970_reset_timer(void)
{
	sy6970_set_value(SY6970_REG_03, REG03_WDT_RESET_MASK,
			 REG03_WDT_RESET_SHIFT, REG03_WDT_RESET);
}

static struct sprdchg_ic_operations sy6970_op = {
	.chg_start = chg_sy6970_enable_chg,
	.chg_stop = chg_sy6970_disable_chg,
	.timer_callback = chg_sy6970_reset_timer,
	.chg_cmd = chg_sy6970_cmd,
};

static int sy6970_charger_get_vendor_id_part_value(void)
{
	int ret = 0;
	u8 reg_part_val = 0;

	ret = sy6970_read_reg(SY6970_REG_14, &reg_part_val);
	if (ret < 0) {
		dprintf(INFO,"[%s]l=%d: Failed to get vendor id, ret=%d\n",
			__FUNCTION__, __LINE__, ret);
		return ret;
	}

	reg_part_val = (reg_part_val & REG14_PN_MASK) >> REG14_PN_SHIFT;
	if (reg_part_val != REG14_PN) {
		errorf("[%s]l=%d: The part value is 0x%x\n",
			__FUNCTION__, __LINE__, reg_part_val);
		return -EINVAL;
	}

	return ret;
}

void sprdchg_sy6970_init(void)
{
	int ret;

	dprintf(INFO,"sy6970 init\n");

	ret = sy6970_charger_get_vendor_id_part_value();
	if (ret) {
		dprintf(INFO,"[%s]l=%d: sy6970 is not found, not register ops\n",
			__FUNCTION__, __LINE__);
		return ret;
	}

	dprintf(INFO,"sy6970 register charge ops!\n");
	sprdchg_register_ops(&sy6970_op);
}
