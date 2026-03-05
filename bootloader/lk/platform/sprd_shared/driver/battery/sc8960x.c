// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */
#include <sprd_chg_helper.h>
#include <errno.h>
#include <i2c.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>
#include <lk/debug.h>
#include <lk/board.h>
#include "sc8960x_reg.h"

#define SC8960X_LK_DRV_VERSION "1.0.0_UNISOC"
static int i2c_bus_num = SPRDCHG_I2C_BUS;
enum sc8960x_part_no {
	SC89601D = 0x03,
};

struct sc8960x_config {
	int chg_mv;
	int chg_ma;

	int ivl_mv;
	int icl_ma;
	
	int iterm_ma;
	
	bool enable_term;
};


struct sc8960x {
	struct sc8960x_config cfg;
    int id;
    int addr;
};

/* Info of primary charger */
static struct sc8960x g_sc8960x = {
	.cfg = {
		.chg_ma = 2000,
		.chg_mv = 4400,
		.ivl_mv = 4500,
		.icl_ma = 1000,
	},
    .id = 2,
    .addr = 0x6B,
};


static int sc8960x_read_byte(struct sc8960x *sc, u8 reg, u8 *data)
{
	int ret = 0;
	u8 ret_data[2] = {0};


    ret_data[0] = reg;

	ret = i2c_read_write(i2c_bus_num, sc->addr, ret_data, 1, data, 1);

	if (ret < 0)
		dprintf(ALWAYS, "%s: I2CR[0x%02X] failed, code = %d\n",
			__func__, reg, ret);

	return ret;
}


static int sc8960x_write_byte(struct sc8960x *sc, u8 reg, u8 data)
{
	int ret = 0;
	unsigned char write_buf[2] = {reg, data};
	

	ret = i2c_send(sc->id, sc->addr, write_buf, 2);

	if (ret < 0)
		dprintf(ALWAYS,
			"%s: I2CW[0x%02X] = 0x%02X failed, code = %d\n",
			__func__, reg, data, ret);

	return ret;
}


static int sc8960x_update_bits(struct sc8960x *sc, u8 reg,
					u8 mask, u8 data)
{
	int ret = 0;
	u8 reg_data = 0;

	ret = sc8960x_read_byte(sc, reg, &reg_data);
	if (ret < 0)
		return ret;

	reg_data &= ~mask;
	reg_data |= (data & mask);
	ret = sc8960x_write_byte(sc, reg, reg_data);
	
	return ret;
}

static bool sc8960x_is_hw_exist(struct sc8960x *sc)
{
	int ret;
	u8 data;
	u8 partno;

	ret = sc8960x_read_byte(sc, SC8960X_REG_0B, &data);
	if (ret < 0)
			return 0;
	partno = (data & SC8960X_PN_MASK) >> SC8960X_PN_SHIFT;
	if (partno != SC89601D) {
		dprintf(ALWAYS, "%s: incorrect part number, not sc8960x\n", __func__);
		return false;
	}
	dprintf(ALWAYS, "%s: chip PN:%d\n", __func__, partno);

	return true;	
}

static int sc8960x_enable_term(bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = SC8960X_EN_TERM_ENABLE;
	else
		val = SC8960X_EN_TERM_DISABLE;

	val <<= SC8960X_EN_TERM_SHIFT;

	ret = sc8960x_update_bits(&g_sc8960x, SC8960X_REG_05,
				SC8960X_EN_TERM_MASK, val);

	return ret;
}


static void sc8960x_enable_charger(int temp)
{
	(void)temp;
	u8 val = SC8960X_CHG_CFG_ENABLE << SC8960X_CHG_CFG_SHIFT;

	sc8960x_update_bits(&g_sc8960x, SC8960X_REG_01,
				SC8960X_CHG_CFG_MASK, val);
}

static void sc8960x_disable_charger(int temp)
{
	(void)temp;
	u8 val = SC8960X_CHG_CFG_ENABLE << SC8960X_CHG_CFG_SHIFT;

	sc8960x_update_bits(&g_sc8960x, SC8960X_REG_01,
				SC8960X_CHG_CFG_MASK, val);
}

/* =========================================================== */
/* The following is implementation for interface of sc8960x */
/* =========================================================== */

static int sc8960x_dump_register(void)
{
	int ret;
	u8 addr;
	u8 val;

	for (addr = 0x00; addr <= 0x0E; addr++) {
		ret = sc8960x_read_byte(&g_sc8960x, addr, &val);
		if (!ret)
			dprintf(ALWAYS,"%s:Reg[%02X] = 0x%02X\n", __func__, addr, val);
	}
	
	return ret;
}

static int sc8960x_enable_charging(bool enable)
{
	dprintf(ALWAYS, "%s: enable = %d\n", __func__, enable);	

	if (enable)
		sc8960x_enable_charger(0);
	else
		sc8960x_disable_charger(0);
	
	return 0;
}

static int sc8960x_set_vchg(u32 vchg)
{
	u8 reg_vchg;

	vchg /= 1000; /* to mV */

	if (vchg < SC8960X_VBAT_REG_BASE)
		vchg = SC8960X_VBAT_REG_BASE;
	
	reg_vchg = (vchg - SC8960X_VBAT_REG_BASE) / SC8960X_VBAT_REG_LSB;
	reg_vchg <<= SC8960X_VBAT_REG_SHIFT;

	return sc8960x_update_bits(&g_sc8960x, SC8960X_REG_04,
				SC8960X_VBAT_REG_MASK, reg_vchg);
}


static int sc8960x_set_ichg(u32 ichg)
{
	u8 reg_ichg;
	
	ichg /= 1000; /*to mA */
	if (ichg < 500)
		ichg = 500;
	reg_ichg = (ichg - SC8960X_ICC_BASE) / SC8960X_ICC_LSB;

	reg_ichg <<= SC8960X_ICC_SHIFT;
	
	return sc8960x_update_bits(&g_sc8960x, SC8960X_REG_02,
				SC8960X_ICC_MASK, reg_ichg);
	
}

static int sc8960x_get_ichg(u32 *ichg)
{
	int ret = 0;
	u8 val = 0;
	int curr;

		
	ret = sc8960x_read_byte(&g_sc8960x, SC8960X_REG_02, &val);
	if (!ret) {
		curr = ((u32)(val & SC8960X_ICC_MASK ) >> SC8960X_ICC_SHIFT) * SC8960X_ICC_LSB;
		curr +=  SC8960X_ICC_BASE;
		
		*ichg = curr * 1000; /*to uA*/
	}
	
	return ret;
}

static int sc8960x_set_aicr(u32 curr)
{
	u8 val;
	
	curr /= 1000;/*to mA*/
	
	if (curr < 1000)
		curr = 1000;

	val = (curr - SC8960X_IINLIM_BASE) /  SC8960X_IINLIM_LSB;
	val <<= SC8960X_IINLIM_SHIFT;
		
	return sc8960x_update_bits(&g_sc8960x, SC8960X_REG_00,
				SC8960X_IINLIM_MASK, val);	
}


static int sc8960x_get_aicr(u32 *curr)
{
	int ret = 0;
	u8 val;
	int ilim;
	
	ret = sc8960x_read_byte(&g_sc8960x, SC8960X_REG_00, &val);
	if (!ret) {
		val = val & SC8960X_IINLIM_MASK;
		val = val >> SC8960X_IINLIM_SHIFT;
		ilim = val * SC8960X_IINLIM_LSB + SC8960X_IINLIM_BASE;	
		*curr = ilim * 1000; /*to uA*/
	}
	
	return ret;
}

static int sc8960x_set_itc(u32 curr)
{
	u8 val;
	
	curr /= 1000;/*to mA*/
	
	if (curr < SC8960X_IPRECHG_BASE)
		curr = SC8960X_IPRECHG_BASE;

	val = (curr - SC8960X_IPRECHG_BASE) /  SC8960X_IPRECHG_LSB;
	val <<= SC8960X_IPRECHG_SHIFT;
		
	return sc8960x_update_bits(&g_sc8960x, SC8960X_REG_03,
				SC8960X_IPRECHG_MASK, val);	
}


static int sc8960x_get_itc(u32 *curr)
{
	int ret = 0;
	u8 val;
	int itc;
	
	ret = sc8960x_read_byte(&g_sc8960x, SC8960X_REG_03, &val);
	if (!ret) {
		val = val & SC8960X_IPRECHG_MASK;
		val = val >> SC8960X_IPRECHG_SHIFT;
		itc = val * SC8960X_IPRECHG_LSB + SC8960X_IPRECHG_BASE;	
		*curr = itc * 1000; /*to uA*/
	}
	
	return ret;
}


static int sc8960x_set_mivr(u32 mivr)
{
	u8 reg_mivr = 0;


	mivr /= 1000; /*to mV*/

	if (mivr < SC8960X_VINDPM_BASE)
		mivr = SC8960X_VINDPM_BASE;
		
	reg_mivr = (mivr - SC8960X_VINDPM_BASE) / SC8960X_VINDPM_LSB;
	reg_mivr <<= SC8960X_VINDPM_SHIFT;

	dprintf(ALWAYS, "%s: mivr = %d(0x%02X)\n", __func__, mivr, reg_mivr);
	
	return sc8960x_update_bits(&g_sc8960x, SC8960X_REG_06,
				SC8960X_VINDPM_MASK, reg_mivr);

}

static void sc8960x_set_power_path(int val)
{
	dprintf(INFO,"sprd_chg: %s val = %d\n", __func__, val);

	if (val)
		sc8960x_update_bits(&g_sc8960x, SC8960X_REG_00,
				  SC8960X_ENHIZ_MASK, SC8960X_HIZ_DISABLE);
	else
		sc8960x_update_bits(&g_sc8960x, SC8960X_REG_00,
				  SC8960X_ENHIZ_MASK, SC8960X_HIZ_ENABLE);

	sc8960x_update_bits(&g_sc8960x, SC8960X_REG_05,
				  SC8960X_WD_TIMER_MASK, SC8960X_WD_TIMER_DIS);
}

static int sc8960x_is_support_power_path(void)
{
	return true;
}

static int sc8960x_cmd(enum sprdchg_cmd cmd, int value)
{
	switch (cmd) {
	case CHG_SET_CURRENT:
		sc8960x_set_ichg(value);
		break;
	case CHG_SET_LIMIT_CURRENT:
		sc8960x_set_aicr(value);
		break;
	case CHG_SET_PRE_CURRENT:
		sc8960x_set_itc(value);
		break;
	case CHG_SET_POWER_PATH:
		sc8960x_set_power_path(value);
		break;
	default:
		break;
	}
	return 0;
}

static int sc8960x_init_setting(struct sc8960x *sc)
{
	int ret = 0;

	dprintf(ALWAYS, "%s\n", __func__);
	
	ret = sc8960x_set_vchg(sc->cfg.chg_mv * 1000);
	if (ret < 0)
		dprintf(ALWAYS, "%s: set chargevolt failed\n", __func__);
	
	ret = sc8960x_set_ichg(sc->cfg.chg_ma * 1000);
	if (ret < 0)
		dprintf(ALWAYS, "%s: set ichg failed\n", __func__);

	ret = sc8960x_set_aicr(sc->cfg.icl_ma * 1000);
	if (ret < 0)
		dprintf(ALWAYS, "%s: set aicr failed\n", __func__);

	ret = sc8960x_set_mivr(sc->cfg.ivl_mv * 1000);
	if (ret < 0)
		dprintf(ALWAYS, "%s: set mivr failed\n", __func__);
	
	ret = sc8960x_enable_term(true);
	if (ret < 0)
		dprintf(ALWAYS, "%s: failed to enable termination\n", __func__);

	return ret;
}

static void sc8960x_reset_timer(void)
{
	dprintf(ALWAYS,"sprd_chg: %s\n", __func__);
	sc8960x_update_bits(&g_sc8960x, SC8960X_REG_01,
				  SC8960X_WD_RST_MASK, SC8960X_WD_RESET);
}

static void sc8960x_init(void)
{
	dprintf(ALWAYS,"sprd_chg: %s\n", __func__);
}


const struct sprdchg_ic_operations sc8960x_ops = {
	//.ic_init = sc8960x_init,
	.chg_start = sc8960x_enable_charger,
	.chg_stop = sc8960x_disable_charger,
	.timer_callback = sc8960x_reset_timer,
	.chg_cmd = sc8960x_cmd,
	.is_support_power_path = sc8960x_is_support_power_path,
};

void sprdchg_sc8960x_init(void)
{
	/* Check primary charger */
	if (sc8960x_is_hw_exist(&g_sc8960x)) {
        sprdchg_register_ops(&sc8960x_ops);
		sc8960x_init_setting(&g_sc8960x);
		dprintf(ALWAYS, "%s: %s\n", __func__, SC8960X_LK_DRV_VERSION);
	}
}


