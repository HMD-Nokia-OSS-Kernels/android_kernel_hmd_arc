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
#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>
#include <lk/debug.h>

/******************************************************************************
* Register addresses
******************************************************************************/
#define FAN5405_REG_CONTROL0	              0
#define FAN5405_REG_CONTROL1	              1
#define FAN5405_REG_OREG	              2
#define FAN5405_REG_IC_INFO                   3
#define FAN5405_REG_IBAT	              4
#define FAN5405_REG_SP_CHARGER                5
#define FAN5405_REG_SAFETY                    6
#define FAN5405_REG_MONITOR                  16

/******************************************************************************
* Register bits
******************************************************************************/
/* FAN5405_REG_CONTROL0 (0x00) */
#define FAN5405_FAULT                   (0x07)
#define FAN5405_FAULT_SHIFT                  0
#define FAN5405_BOOST              (0x01 << 3)
#define FAN5405_BOOST_SHIFT                  3
#define FAN5405_STAT               (0x3 <<  4)
#define FAN5405_STAT_SHIFT                   4
#define FAN5405_EN_STAT            (0x01 << 6)
#define FAN5405_EN_STAT_SHIFT                6
#define FAN5405_TMR_RST_OTG        (0x01 << 7)  // writing a 1 resets the t32s timer, writing a 0 has no effect
#define FAN5405_TMR_RST_OTG_SHIFT            7

/* FAN5405_REG_CONTROL1 (0x01) */
#define FAN5405_OPA_MODE                (0x01)
#define FAN5405_OPA_MODE_SHIFT               0
#define FAN5405_HZ_MODE            (0x01 << 1)
#define FAN5405_HZ_MODE_SHIFT                1
#define FAN5405_CE_N               (0x01 << 2)
#define FAN5405_CE_N_SHIFT                   2
#define FAN5405_TE                 (0x01 << 3)
#define FAN5405_TE_SHIFT                     3
#define FAN5405_VLOWV              (0x03 << 4)
#define FAN5405_VLOWV_SHIFT                  4
#define FAN5405_IINLIM             (0x03 << 6)
#define FAN5405_IINLIM_SHIFT                 6

/* FAN5405_REG_OREG (0x02) */
#define FAN5405_OTG_EN                  (0x01)
#define FAN5405_OTG_EN_SHIFT                 0
#define FAN5405_OTG_PL             (0x01 << 1)
#define FAN5405_OTG_PL_SHIFT                 1
#define FAN5405_OREG               (0x3f << 2)
#define FAN5405_OREG_SHIFT                   2

/* FAN5405_REG_IC_INFO (0x03) */
#define FAN5405_REV                     (0x03)
#define FAN5405_REV_SHIFT                    0
#define FAN5405_PN                 (0x07 << 2)
#define FAN5405_PN_SHIFT                     2
#define FAN5405_VENDOR_CODE        (0x07 << 5)
#define FAN5405_VENDOR_CODE_SHIFT            5

/* FAN5405_REG_IBAT (0x04) */
#define FAN5405_ITERM                   (0x07)
#define FAN5405_ITERM_SHIFT                  0
#define FAN5405_IOCHARGE           (0x07 << 4)
#define FAN5405_IOCHARGE_SHIFT               4
#define FAN5405_RESET              (0x01 << 7)
#define FAN5405_RESET_SHIFT                  7

/* FAN5405_REG_SP_CHARGER (0x05) */
#define FAN5405_VSP                     (0x07)
#define FAN5405_VSP_SHIFT                    0
#define FAN5405_EN_LEVEL           (0x01 << 3)
#define FAN5405_EN_LEVEL_SHIFT               3
#define FAN5405_SP                 (0x01 << 4)
#define FAN5405_SP_SHIFT                     4
#define FAN5405_IO_LEVEL           (0x01 << 5)
#define FAN5405_IO_LEVEL_SHIFT               5
#define FAN5405_DIS_VREG           (0x01 << 6)
#define FAN5405_DIS_VREG_SHIFT               6

/* FAN5405_REG_SAFETY (0x06) */
#define FAN5405_VSAFE                   (0x0f)
#define FAN5405_VSAFE_SHIFT                  0
#define FAN5405_ISAFE              (0x07 << 4)
#define FAN5405_ISAFE_SHIFT                  4

/* FAN5405_REG_MONITOR (0x10) */
#define FAN5405_CV                      (0x01)
#define FAN5405_CV_SHIFT                     0
#define FAN5405_VBUS_VALID         (0x01 << 1)
#define FAN5405_VBUS_VALID_SHIFT             1
#define FAN5405_IBUS               (0x01 << 2)
#define FAN5405_IBUS_SHIFT                   2
#define FAN5405_ICHG               (0x01 << 3)
#define FAN5405_ICHG_SHIFT                   3
#define FAN5405_T_120              (0x01 << 4)
#define FAN5405_T_120_SHIFT                  4
#define FAN5405_LINCHG             (0x01 << 5)
#define FAN5405_LINCHG_SHIFT                 5
#define FAN5405_VBAT_CMP           (0x01 << 6)
#define FAN5405_VBAT_CMP_SHIFT               6
#define FAN5405_ITERM_CMP          (0x01 << 7)
#define FAN5405_ITERM_CMP_SHIFT              7

/******************************************************************************
* bit definitions
******************************************************************************/
/********** FAN5405_REG_CONTROL0 (0x00) **********/
// EN_STAT [6]
#define ENSTAT 1
#define DISSTAT 0
// TMR_RST [7]
#define RESET32S 1

/********** FAN5405_REG_CONTROL1 (0x01) **********/
// OPA_MODE [0]
#define CHARGEMODE 0
#define BOOSTMODE 1
//HZ_MODE [1]
#define NOTHIGHIMP 0
#define HIGHIMP 1
// CE/ [2]
#define ENCHARGER 0
#define DISCHARGER 1
// TE [3]
#define DISTE 0
#define ENTE 1
// VLOWV [5:4]
#define VLOWV3P4 0
#define VLOWV3P5 1
#define VLOWV3P6 2
#define VLOWV3P7 3
// IINLIM [7:6]
#define IINLIM100 0
#define IINLIM500 1
#define IINLIM800 2
#define NOLIMIT 3

/********** FAN5405_REG_OREG (0x02) **********/
// OTG_EN [0]
#define DISOTG 0
#define ENOTG 1
// OTG_PL [1]
#define OTGACTIVELOW 0
#define OTGACTIVEHIGH 1
// OREG [7:2]
#define VOREG4P2 35  // refer to table 3
/********** FAN5405_REG_IC_INFO (0x03) **********/

/********** FAN5405_REG_IBAT (0x04) **********/
// ITERM [2:0] - 68mOhm
#define ITERM49 0
#define ITERM97 1
#define ITERM146 2
#define ITERM194 3
#define ITERM243 4
#define ITERM291 5
#define ITERM340 6
#define ITERM388 7
// IOCHARGE [6:4] - 68mOhm
#define IOCHARGE550 0
#define IOCHARGE650 1
#define IOCHARGE750 2
#define IOCHARGE850 3
#define IOCHARGE1050 4
#define IOCHARGE1150 5
#define IOCHARGE1350 6
#define IOCHARGE1450 7

/********** FAN5405_REG_SP_CHARGER (0x05) **********/
// VSP [2:0]
#define VSP4P213 0
#define VSP4P293 1
#define VSP4P373 2
#define VSP4P453 3
#define VSP4P533 4
#define VSP4P613 5
#define VSP4P693 6
#define VSP4P773 7
// IO_LEVEL [5]
#define ENIOLEVEL 0
#define DISIOLEVEL 1
// DIS_VREG [6]
#define VREGON 0
#define VREGOFF 1

/********** FAN5405_REG_SAFETY (0x06) **********/
// VSAFE [3:0]
#define VSAFE4P20 0
#define VSAFE4P22 1
#define VSAFE4P24 2
#define VSAFE4P26 3
#define VSAFE4P28 4
#define VSAFE4P30 5
#define VSAFE4P32 6
#define VSAFE4P34 7
#define VSAFE4P36 8
#define VSAFE4P38 9
#define VSAFE4P40 10
#define VSAFE4P42 11
#define VSAFE4P44 12
// ISAFE [6:4] - 68mOhm
#define ISAFE550 0
#define ISAFE650 1
#define ISAFE750 2
#define ISAFE850 3
#define ISAFE1050 4
#define ISAFE1150 5
#define ISAFE1350 6
#define ISAFE1450 7

#define I2C_SPEED					(100000)
#define SLAVE_ADDR					(0x6a)

#define VENDOR_FAN54015					(0x4)
#define VENDOR_TQ24157					(0x2)
#define VENDOR_PSC5415Z					(0x7)

static unsigned char fan54015_get_vendor_id(void);
extern int sprd_charge_pd_control(bool enable);

static int i2c_bus_num = SPRDCHG_I2C_BUS;

static int fan54015_write_reg(BYTE reg, u8 val)
{
	int ret;
	uint8_t buf[2] = {0};

	buf[0] = reg;
	buf[1] = val;
	ret = i2c_send(i2c_bus_num, SLAVE_ADDR, buf, 2);
	if (ret < 0) {
		dprintf(ALWAYS,"sprd_chg: %s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

	return 0;
}

static int fan54015_read_reg(BYTE reg, u8 *value)
{
	int ret;
	uint8_t reg_addr[2] = {0};

	reg_addr[0] = reg;
	ret = i2c_read_write(i2c_bus_num, SLAVE_ADDR, reg_addr, 1, value, 1);

	if (ret < 0) {
		dprintf(ALWAYS,"sprd_chg: %s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

	dprintf(INFO, "sprd_chg: %s reg  = %d value =%d/%x\n", __func__, reg, *value, *value);

	return 0;
}

static void fan54015_set_value(BYTE reg, BYTE reg_bit,BYTE reg_shift, BYTE val)
{
	BYTE tmp;
	int ret;

	ret = fan54015_read_reg(reg, &tmp);
	if (ret < 0)
		return;

	tmp = (tmp & (~reg_bit)) |(val << reg_shift);
	if((0x04 == reg)&&(FAN5405_RESET != reg_bit)){
		tmp &= 0x7f;
	}
	fan54015_write_reg(reg,tmp);
}

static BYTE fan54015_get_value(BYTE reg, BYTE reg_bit, BYTE reg_shift)
{
	BYTE data = 0;
	BYTE ret = 0 ;
	ret = fan54015_read_reg(reg, &data);
	ret = (data & reg_bit) >> reg_shift;

	return ret;
}

static void sprdchg_fan54015_start_chg(void)
{
	dprintf(INFO,"sprd_chg: %s\n", __func__);
	sprd_charge_pd_control(true);
}

static void sprdchg_fan54015_stop_charging(void)
{
	dprintf(INFO,"sprd_chg: %s\n", __func__);
	sprd_charge_pd_control(false);
}

static void fan54015_sw_reset(void)
{
	dprintf(INFO,"sprd_chg: %s\n", __func__);
	fan54015_set_value(FAN5405_REG_IBAT, FAN5405_RESET, FAN5405_RESET_SHIFT, 1);
}

static void sprdchg_fan54015_reset_timer(void)
{
	dprintf(INFO,"sprd_chg: %s\n", __func__);
	fan54015_set_value(FAN5405_REG_CONTROL0, FAN5405_TMR_RST_OTG,
			   FAN5405_TMR_RST_OTG_SHIFT, RESET32S);
}

static unsigned char fan54015_get_vendor_id(void)
{
	return fan54015_get_value(FAN5405_REG_IC_INFO,
				  FAN5405_VENDOR_CODE,
				  FAN5405_VENDOR_CODE_SHIFT);
}

static void fan54015_set_chg_current(unsigned char reg_val)
{
	if (reg_val == 0)
		fan54015_set_value(FAN5405_REG_CONTROL1,
				   FAN5405_IINLIM, FAN5405_IINLIM_SHIFT,
				   IINLIM500);
	else
		fan54015_set_value(FAN5405_REG_CONTROL1,
				   FAN5405_IINLIM, FAN5405_IINLIM_SHIFT,
				   NOLIMIT);
	fan54015_set_value(FAN5405_REG_IBAT, FAN5405_IOCHARGE,
			   FAN5405_IOCHARGE_SHIFT, reg_val);
}

static unsigned char sprdchg_fan54015_cur2reg(uint32_t cur)
{
	unsigned char reg_val;

	if (cur < 650)
		reg_val = 0;
	else if (cur < 750)
		reg_val = 1;
	else if (cur < 850)
		reg_val = 2;
	else if (cur < 1050)
		reg_val = 3;
	else if (cur < 1150)
		reg_val = 4;
	else if (cur < 1350)
		reg_val = 5;
	else if (cur < 1450)
		reg_val = 6;
	else
		reg_val = 7;

	return reg_val;
}

static unsigned char sprdchg_tq24157_cur2reg(uint32_t cur)
{
	unsigned char reg_val;

	if (cur < 650)
		reg_val = 0;
	else if (cur >= 1250)
		reg_val = 7;
	else
		reg_val = (cur - 550) / 100;

	return reg_val;
}

static unsigned char sprdchg_psc5415z_cur2reg(uint32_t cur)
{
	unsigned char reg_val;

	if (cur < 821)
		reg_val = 0;
	else if (cur < 1027)
		reg_val = 1;
	else if (cur < 1230)
		reg_val = 2;
	else if (cur < 1436)
		reg_val = 3;
	else if (cur < 1642)
		reg_val = 4;
	else if (cur < 1848)
		reg_val = 5;
	else if (cur < 2052)
		reg_val = 6;
	else
		reg_val = 7;

	return reg_val;
}

static void sprdchg_fan54015_set_cur(uint32_t cur)
{
	unsigned char reg_val = 0, vendor_id = VENDOR_FAN54015;

	vendor_id = fan54015_get_vendor_id();
	if (vendor_id == VENDOR_FAN54015)
		reg_val = sprdchg_fan54015_cur2reg(cur);

	if (vendor_id == VENDOR_TQ24157)
		reg_val = sprdchg_tq24157_cur2reg(cur);

	if (vendor_id == VENDOR_PSC5415Z)
		reg_val = sprdchg_psc5415z_cur2reg(cur);

	fan54015_set_chg_current(reg_val);
}


static void sprdchg_fan54015_cmd(enum sprdchg_cmd cmd,int value)
{
	switch (cmd) {
	case CHG_SET_CURRENT:
		sprdchg_fan54015_set_cur(value);
		break;
	default:
		break;
	}
}

static struct sprdchg_ic_operations fan54015_op ={
	.chg_start = sprdchg_fan54015_start_chg,
	.chg_stop = sprdchg_fan54015_stop_charging,
	.timer_callback = sprdchg_fan54015_reset_timer,
	.chg_cmd = sprdchg_fan54015_cmd,
};

void sprdchg_fan54015_init(void)
{
	BYTE data = 0;
	unsigned char vendor_id = 0;

	vendor_id = fan54015_get_vendor_id();
	dprintf(INFO,"sprd_chg: fan54015_get_vendor_id = 0x%x\n", vendor_id);
	if (vendor_id == 0x0)
		vendor_id = fan54015_get_vendor_id();

	if ((vendor_id != VENDOR_FAN54015) && (vendor_id != VENDOR_TQ24157)
	    && (vendor_id != VENDOR_PSC5415Z)) {
		dprintf(ALWAYS,"sprd_chg: fan54015 is not found,not register ops!!!\n");
		return;
	}

	//reg 6
	fan54015_set_value(FAN5405_REG_SAFETY, FAN5405_VSAFE,
			   FAN5405_VSAFE_SHIFT, VSAFE4P38);// VSAFE = 4.38V
	fan54015_set_value(FAN5405_REG_SAFETY, FAN5405_ISAFE,
			   FAN5405_ISAFE_SHIFT, ISAFE1450);// ISAFE = 1450mA (68mOhm)
	fan54015_sw_reset();

	if(vendor_id == VENDOR_PSC5415Z) {
		fan54015_set_value(FAN5405_REG_SAFETY, FAN5405_VSAFE,
				   FAN5405_VSAFE_SHIFT, VSAFE4P38);// VSAFE = 4.38V
		fan54015_set_value(FAN5405_REG_SAFETY, FAN5405_ISAFE,
				   FAN5405_ISAFE_SHIFT, ISAFE1450);// ISAFE = 1450mA (68mOhm)
	}
	//reg 1
	fan54015_set_value(FAN5405_REG_CONTROL1, FAN5405_VLOWV,
			   FAN5405_VLOWV_SHIFT, VLOWV3P4);// VLOWV = 3.4V
	fan54015_set_value(FAN5405_REG_CONTROL1, FAN5405_IINLIM,
			   FAN5405_IINLIM_SHIFT, NOLIMIT);// INLIM = 500mA
	//reg 2
	fan54015_set_value(FAN5405_REG_OREG, FAN5405_OREG,
			   FAN5405_OREG_SHIFT, VOREG4P2);//OREG = 4.35V
	//reg 5
	fan54015_set_value(FAN5405_REG_SP_CHARGER, FAN5405_IO_LEVEL,
			   FAN5405_IO_LEVEL_SHIFT, ENIOLEVEL);//IO_LEVEL is 0. Output current is controlled by IOCHARGE bits.
	dprintf(INFO,"sprd_chg: %s\n", __func__);

	sprdchg_register_ops(&fan54015_op);
	dprintf(INFO,"sprd_chg: %s, i2c_bus_num = %d\n", __func__, i2c_bus_num);
}
