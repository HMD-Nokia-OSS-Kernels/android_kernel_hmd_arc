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

#define BIT(nr)					(1UL << (nr))

#define SGM4154X_REG_00				0x00
#define SGM4154X_REG_01				0x01
#define SGM4154X_REG_02				0x02
#define SGM4154X_REG_03				0x03
#define SGM4154X_REG_04				0x04
#define SGM4154X_REG_05				0x05
#define SGM4154X_REG_06				0x06
#define SGM4154X_REG_07				0x07
#define SGM4154X_REG_08				0x08
#define SGM4154X_REG_09				0x09
#define SGM4154X_REG_0A				0x0a
#define SGM4154X_REG_0B				0x0b
#define SGM4154X_REG_0C				0x0c
#define SGM4154X_REG_0D				0x0d
#define SGM4154X_REG_0E				0x0e
#define SGM4154X_REG_0F				0x0f
#define SGM4154X_REG_NUM			16

/* Register 0x00 */
#define REG00_ENHIZ_MASK			BIT(7)
#define REG00_ENHIZ_SHIFT			7
#define REG00_EN_ICHG_MASK			0x60
#define REG00_EN_ILIM_SHIFT			5
#define REG00_IINLIM_MASK			0x1f
#define REG00_IINLIM_SHIFT			0
#define	REG00_IINLIM_BASE			100
#define REG00_IINLIM_OFFSET			100
#define REG00_IINLIM_CURRENT_MAX		3200
#define REG00_IINLIM_CURRENT_OFFSET		100

/* Register 0x01*/
#define REG01_ENPFM_MASK			BIT(7)
#define REG01_ENPFM_SHIFT			7
#define REG01_WDT_MASK				BIT(6)
#define REG01_WDT_SHIFT				6
#define REG01_OTG_CONFIG_MASK			BIT(5) //enable otg
#define REG01_OTG_CONFIG_SHIFT			5
#define REG01_ENCHG_MASK			BIT(4) //enable battery charging
#define REG01_ENCHG_SHIFT			4
#define REG01_SYS_MIN_MASK			0x0e
#define REG01_SYS_MIN_SHIFT			1
#define REG01_MIN_BAT_SEL_MASK			BIT(0)
#define REG01_MIN_BAT_SEL_SHIFT			0
#define REG01_CHG_DISABLE			0
#define REG01_CHG_ENABLE			1

/* Register 0x02*/
#define REG02_BOOST_LIM_MASK			BIT(7)
#define REG02_BOOST_LIM_SHIFT			7
#define REG02_Q1_FULLON_MASK			BIT(6)
#define REG02_Q1_FULLON_SHIFT			6
#define REG02_ICHG_MASK				0x3f
#define REG02_ICHG_SHIFT			0

/* Register 0x03 */
#define REG03_IPRECHG_MASK			0xf0
#define REG03_IPRECHG_SHIFT			4
#define REG03_ITERM_MASK			0x0f
#define REG03_ITERM_SHIFT			0
#define REG03_IPRECHG_BASE			60
#define REG03_IPRECHG_LSB			60
#define REG03_IPRECHG_CURRENT_MAX		780
#define REG03_IPRECHG_CURRENT_OFFSET		60

/* Register 0x04*/
#define REG04_VREG_MASK				0xf8
#define REG04_VREG_SHIFT			3
#define REG04_TOPOFF_TIMER_MASK			0x06
#define REG04_TOPOFF_TIMER_SHIFT		1
#define REG04_VRECHG_MASK			BIT(0)
#define REG04_VRECHG_SHIFT			0

/* Register 0x05*/
#define REG05_EN_TERM_MASK			BIT(7)
#define REG05_EN_TERM_SHIFT			7
#define REG05_ARDCEN_ITS_MASK			BIT(6)
#define REG05_ARDCEN_ITS_SHIFT			6
#define REG05_WDT_TIMER_MASK			0x30
#define REG05_WDT_TIMER_SHIFT			4
#define REG05_EN_TIMER_MASK			BIT(3)
#define REG05_EN_TIMER_SHIFT			3
#define REG05_CHG_TIMER_MASK			BIT(2)
#define REG05_CHG_TIMER_SHIFT			2
#define REG05_TREG_MASK				BIT(1)
#define REG05_TREG_SHIFT			1
#define REG05_JEITA_ISET_L_MASK			BIT(0)
#define REG05_JEITA_ISET_L_SHIFT		0

/* Register 0x06*/
#define REG06_OVP_MASK				0xc0
#define REG06_OVP_SHIFT				6
#define REG06_BOOSTV_MASK			0x30
#define REG06_BOOSTV_SHIFT			4
#define REG06_VINDPM_MASK			0x0f
#define REG06_VINDPM_SHIFT			0

/* Register 0x07*/
#define REG07_IINDET_EN_MASK			BIT(7)
#define REG07_IINDET_EN_SHIFT			7
#define REG07_TMR2X_EN_MASK			BIT(6)
#define REG07_TMR2X_EN_SHIFT			6
#define REG07_BATFET_DIS_MASK			BIT(5)
#define REG07_BATFET_DIS_SHIFT			5
#define REG07_JEITA_VSET_H_MASK			BIT(4)
#define REG07_JEITA_VSET_H_SHIFT		4
#define REG07_BATFET_DLY_MASK			BIT(3)
#define REG07_BATFET_DLY_SHIFT			3
#define REG07_BATFET_RST_EN_MASK		BIT(2)
#define REG07_BATFET_RST_EN_SHIFT		2
#define REG07_VDPM_BAT_TRACK_MASK		0x03
#define REG07_VDPM_BAT_TRACK_SHIFT		0

/* Register 0x08*/
#define REG08_VBUS_STAT_MASK			0xe0
#define REG08_VBUS_STAT_SHIFT			5
#define REG08_CHRG_STAT_MASK			0x18
#define REG08_CHRG_STAT_SHIFT			3
#define REG08_PG_STAT_MASK			BIT(2)
#define REG08_PG_STAT_SHIFT			2
#define REG08_THERM_STAT_MASK			BIT(1)
#define REG08_THERM_STAT_SHIFT			1
#define REG08_VSYS_STAT_MASK			BIT(0)
#define REG08_VSYS_STAT_SHIFT			0

/* Register 0x09*/
#define REG09_WDT_FAULT_MASK			BIT(7)
#define REG09_WDT_FAULT_SHIFT			7
#define REG09_BOOST_FAULT_MASK			BIT(6)
#define REG09_BOOST_FAULT_SHIFT			6
#define REG09_CHRG_FAULT_MASK			0x30
#define REG09_CHRG_FAULT_SHIFT			4
#define REG09_BAT_FAULT_MASK			BIT(3)
#define REG09_BAT_FAULT_SHIFT			3
#define REG09_NTC_FAULT_MASK			0x07
#define REG09_NTC_FAULT_SHIFT			0

/* Register 0x0A*/
#define REG0A_VBUS_GD_MASK			BIT(7)
#define REG0A_VBUS_GD_SHIFT			7
#define REG0A_VINDPM_SATA_MASK			BIT(6)
#define REG0A_VINDPM_SATA_SHIFT			6
#define REG0A_IINDPM_SATA_MASK			BIT(5)
#define REG0A_IINDPM_SATA_SHIFT			5
#define REG0A_CV_SATA_MASK			BIT(4)
#define REG0A_CV_SATA_SHIFT			4
#define REG0A_TOPOFF_ACTIVE_MASK		BIT(3)
#define REG0A_TOPOFF_ACTIVE_SHIFT		3
#define REG0A_ACOV_SATA_MASK			BIT(2)
#define REG0A_ACOV_SATA_SHIFT			2
#define REG0A_VINDPM_INT_MASK			BIT(1)
#define REG0A_VINDPM_INT_SHIFT			1
#define REG0A_IINDPM_INT_MASK			BIT(0)
#define REG0A_IINDPM_INT_SHIFT			0

/* Register 0x0B*/
#define REG0B_REG_RST_MASK			BIT(7)
#define REG0B_REG_RST_SHIFT			7
#define REG0B_PN_ID_MASK			0x78
#define REG0B_PN_ID_SHIFT			3
#define REG0B_SGMPART_MASK			BIT(2)
#define REG0B_SGMPART_SHIFT			2
#define REG0B_DEV_REV_MASK			0x03
#define REG0B_DEV_REV_SHIFT			0

/* Register 0x0C*/
#define REG0C_JEITA_VSET_L_MASK			BIT(7)
#define REG0C_JEITA_VSET_L_SHIFT		7
#define REG0C_JEITA_ISET_L_EN_MASK		BIT(6)
#define REG0C_JEITA_ISET_L_EN_SHIFT		6
#define REG0C_JEITA_ISET_H_MASK			0x30
#define REG0C_JEITA_ISET_H_SHIFT		4
#define REG0C_JEITA_VT2_MASK			0x0c
#define REG0C_JEITA_VT2_SHIFT			2
#define REG0C_JEITA_VT3_MASK			0x03
#define REG0C_JEITA_VT3_SHIFT			0

/* Register 0x0D*/
#define REG0D_EN_PUMPX_MASK			BIT(7)
#define REG0D_EN_PUMPX_SHIFT			7
#define REG0D_PUMPX_UP_MASK			BIT(6)
#define REG0D_PUMPX_UP_SHIFT			6
#define REG0D_PUMPX_DN_MASK			BIT(5)
#define REG0D_PUMPX_DN_SHIFT			5
#define REG0D_DP_VSET_SHIFT			3
#define REG0D_DM_VSET_SHIFT			1
#define REG0D_JEITA_EN_MASK			BIT(0)
#define REG0D_JEITA_EN_SHIFT			0

/* Register 0x0E*/
#define REG0E_INPUT_DET_DONE_MASK		BIT(7)
#define REG0E_INPUT_DET_DONE_SHIFT		7
#define REG0E_RESERVED_SHIFT			0

/* Register 0x0F*/
#define REG0F_VREG_FT_SHIFT			6
#define REG0F_RESERVED_MASK			BIT(5)
#define REG0F_RESERVED_SHIFT			5
#define REG0F_DCEN_MASK				BIT(4)
#define REG0F_DCEN_SHIFT			4
#define REG0F_STAT_SET_SHIFT			2
#define REG0F_VINDPM_OS_SHIFT			0

#define WDT_RESET				1
#define REG_RESET				1
#define OTG_DISABLE				0
#define OTG_ENABLE				1
/* charge status flags  */
#define HIZ_DISABLE				0
#define HIZ_ENABLE				1

#define SGM4154X_OTG_EN				BIT(5)

/* Part ID  */
#define SGM4154X_PN_41541_ID			(BIT(6)| BIT(5))
#define SGM4154X_PN_41542_ID			(BIT(6)| BIT(5)| BIT(3))

/* WDT TIMER SET  */
#define SGM4154X_WDT_TIMER_DISABLE		0
#define SGM4154X_WDT_TIMER_40S			1
#define SGM4154X_WDT_TIMER_80S			2
#define SGM4154X_WDT_TIMER_160S			3

#define SGM4154X_WDT_RST_MASK			BIT(6)

/* SAFETY TIMER SET  */
#define SGM4154X_SAFETY_TIMER_DISABLE		0
#define SGM4154X_SAFETY_TIMER_EN		BIT(3)
#define SGM4154X_SAFETY_TIMER_5H		0
#define SGM4154X_SAFETY_TIMER_10H		BIT(2)

/* recharge voltage  */
#define SGM4154X_VRECHARGE			BIT(0)
#define SGM4154X_VRECHRG_STEP_mV		100
#define SGM4154X_VRECHRG_OFFSET_mV		100

/* charge status  */
#define SGM4154X_VSYS_STAT			BIT(0)
#define SGM4154X_THERM_STAT			BIT(1)
#define SGM4154X_PG_STAT			BIT(2)
#define SGM4154X_PRECHRG			BIT(3)
#define SGM4154X_FAST_CHRG			BIT(4)
#define SGM4154X_TERM_CHRG			(BIT(3)| BIT(4))

/* charge type  */
#define SGM4154X_NOT_CHRGING			0
#define SGM4154X_USB_SDP			BIT(5)
#define SGM4154X_USB_CDP			BIT(6)
#define SGM4154X_USB_DCP			(BIT(5) | BIT(6))
#define SGM4154X_UNKNOWN			(BIT(7) | BIT(5))
#define SGM4154X_NON_STANDARD			(BIT(7) | BIT(6))
#define SGM4154X_OTG_MODE			(BIT(7) | BIT(6) | BIT(5))

/* TEMP Status  */
#define SGM4154X_TEMP_NORMAL			BIT(0)
#define SGM4154X_TEMP_WARM			BIT(1)
#define SGM4154X_TEMP_COOL			(BIT(0) | BIT(1))
#define SGM4154X_TEMP_COLD			(BIT(0) | BIT(3))
#define SGM4154X_TEMP_HOT			(BIT(2) | BIT(3))

/* precharge current  */
#define SGM4154X_PRECHRG_CURRENT_STEP_uA	60000
#define SGM4154X_PRECHRG_I_MIN_uA		60000
#define SGM4154X_PRECHRG_I_MAX_uA		780000
#define SGM4154X_PRECHRG_I_DEF_uA		180000

/* termination current  */
#define SGM4154X_TERMCHRG_CURRENT_STEP_uA	60
#define SGM4154X_TERMCHRG_I_MIN_uA		60
#define SGM4154X_TERMCHRG_I_MAX_uA		960
#define SGM4154X_TERMCHRG_I_DEF_uA		180

/* charge current  */
#define SGM4154X_ICHRG_CURRENT_STEP		60
#define SGM4154X_ICHRG_I_MIN			60
#define SGM4154X_ICHRG_I_MAX			3780
#define SGM4154X_ICHRG_I_DEF			2040

/* charge voltage  */
#define SGM4154X_VREG_V_MAX_uV			4624
#define SGM4154X_VREG_V_MIN_uV			3856
#define SGM4154X_VREG_V_DEF_uV			4208
#define SGM4154X_VREG_V_STEP_uV			32

/* VREG Fine Tuning  */
#define SGM4154X_VREG_FT_UP_8mV			BIT(6)
#define SGM4154X_VREG_FT_DN_8mV			BIT(7)
#define SGM4154X_VREG_FT_DN_16mV		(BIT(7) | BIT(6))

/* iindpm current  */
#define SGM4154X_IINDPM_I_MIN			100
#define SGM4154X_IINDPM_I_MAX			3800
#define SGM4154X_IINDPM_STEP			100
#define SGM4154X_IINDPM_DEF			2400

/* vindpm voltage  */
#define SGM4154X_VINDPM_V_MIN_uV		3900000
#define SGM4154X_VINDPM_V_MAX_uV		12000000
#define SGM4154X_VINDPM_STEP_uV			100000
#define SGM4154X_VINDPM_DEF_uV			3600000

/* PUMPX SET  */
#define SGM4154X_EN_PUMPX			BIT(7)
#define SGM4154X_PUMPX_UP			BIT(6)
#define SGM4154X_PUMPX_DN			BIT(5)

/* PN_ID 0x1101-4353 for sgm4154x */
#define REG0B_PN_ID				13
#define I2C_SPEED				100000
#define SLAVE_ADDR				0x3B

#define POWER_PATH_ENABLE			0
#define POWER_PATH_DISABLE			1

static int i2c_bus_num = SPRDCHG_I2C_BUS;

static int sgm4154x_write_reg(u8 reg, u8 val)
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

static int sgm4154x_read_reg(u8 reg, u8 *value)
{
	int ret;
	uint8_t reg_addr[2] = {0};

	reg_addr[0] = reg;
	ret = i2c_read_write(i2c_bus_num, SLAVE_ADDR, reg_addr, 1, value, 1);
	if (ret < 0) {
		dprintf(ALWAYS,"%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

	dprintf(ALWAYS,"sgm4154x_read_reg reg = 0x%x, value = %d/0x%x\n", reg, *value, *value);

	return 0;
}

static void sgm4154x_set_value(u8 reg, u8 reg_bit, u8 reg_shift, u8 val)
{
	u8 tmp = 0;

	sgm4154x_read_reg(reg, &tmp);
	tmp = tmp & (~reg_bit) | (val << reg_shift);
	sgm4154x_write_reg(reg, tmp);
}

static u8 sgm4154x_get_value(u8 reg, u8 reg_bit, u8 reg_shift)
{
	u8 reg_value = 0;

	sgm4154x_read_reg(reg, &reg_value);
	reg_value = (reg_value & reg_bit) >> reg_shift;

	return reg_value;
}

static void chg_sgm4154x_set_chg_cur(u32 cur)
{
	u8 reg_value;

	if (cur <= SGM4154X_ICHRG_I_MIN)
		cur = SGM4154X_ICHRG_I_MIN;
	else if (cur >= SGM4154X_ICHRG_I_MAX)
		cur = SGM4154X_ICHRG_I_MAX;
	reg_value = cur / SGM4154X_ICHRG_CURRENT_STEP;

	sgm4154x_set_value(SGM4154X_REG_02, REG02_ICHG_MASK,
			   REG02_ICHG_SHIFT, reg_value);

}

static void chg_sgm4154x_set_limit_cur(u32 limit)
{
	u8 reg_value;

	if (limit > REG00_IINLIM_CURRENT_MAX)
		limit = REG00_IINLIM_CURRENT_MAX;

	limit = limit - REG00_IINLIM_CURRENT_OFFSET;
	reg_value = limit / REG00_IINLIM_BASE;
	sgm4154x_set_value(SGM4154X_REG_00, REG00_IINLIM_MASK,
			   REG00_IINLIM_SHIFT, reg_value);

}

static int chg_sgm4154x_set_prechg(u32 ichg)
{
	u8 reg_value;

	if (ichg > REG03_IPRECHG_CURRENT_MAX)
		ichg = REG03_IPRECHG_CURRENT_MAX;

	ichg = ichg - REG03_IPRECHG_CURRENT_OFFSET;
	reg_value = ichg / REG03_IPRECHG_LSB;
	sgm4154x_set_value(SGM4154X_REG_03, REG03_IPRECHG_MASK,
			   REG03_IPRECHG_SHIFT, reg_value);

	return 0;
}

static void chg_sgm4154x_cmd(enum sprdchg_cmd cmd, int value)
{
	switch (cmd) {
	case CHG_SET_CURRENT:
		chg_sgm4154x_set_chg_cur(value);
		break;
	case CHG_SET_LIMIT_CURRENT:
		chg_sgm4154x_set_limit_cur(value);
		break;
	case CHG_SET_PRE_CURRENT:
		chg_sgm4154x_set_prechg(value);
		break;
	default:
		break;
	}
}

static void chg_sgm4154x_enable_chg(void)
{
	sgm4154x_set_value(SGM4154X_REG_01, REG01_ENCHG_MASK,
			   REG01_ENCHG_SHIFT, REG01_CHG_ENABLE);
}

static void chg_sgm4154x_disable_chg(void)
{
	sgm4154x_set_value(SGM4154X_REG_01, REG01_ENCHG_MASK,
			   REG01_ENCHG_SHIFT, REG01_CHG_DISABLE);
}

static void chg_sgm4154x_reset_timer(void)
{
	sgm4154x_set_value(SGM4154X_REG_01, REG01_WDT_MASK,
			   REG01_WDT_SHIFT, WDT_RESET);
}

static struct sprdchg_ic_operations sgm4154x_op ={
	.chg_start = chg_sgm4154x_enable_chg,
	.chg_stop = chg_sgm4154x_disable_chg,
	.timer_callback = chg_sgm4154x_reset_timer,
	.chg_cmd = chg_sgm4154x_cmd,
};

static int sgm4154x_charger_get_vendor_id_part_value(void)
{
	int ret = 0;
	u8 reg_part_val = 0;

	ret = sgm4154x_read_reg(SGM4154X_REG_0B, &reg_part_val);
	if (ret < 0) {
		dprintf(INFO,"[%s]l=%d: Failed to get vendor id, ret=%d\n",
			__FUNCTION__, __LINE__, ret);
		return ret;
	}

	reg_part_val = (reg_part_val & REG0B_PN_ID_MASK) >> REG0B_PN_ID_SHIFT;
	if (reg_part_val != REG0B_PN_ID) {
		dprintf(INFO,"[%s]l=%d: The part value is 0x%x\n",
			__FUNCTION__, __LINE__, reg_part_val);
		return -EINVAL;
	}

	return ret;
}

void sprdchg_sgm4154x_init(void)
{
	int ret;

	dprintf(INFO,"sgm4154x init\n");

	ret = sgm4154x_charger_get_vendor_id_part_value();
	if (ret) {
		dprintf(INFO,"[%s]l=%d: sgm4154x is not found, not register ops\n",
			__FUNCTION__, __LINE__);
		return ret;
	}

	dprintf(INFO,"sgm4154x register charge ops!\n");
	sprdchg_register_ops(&sgm4154x_op);
}
