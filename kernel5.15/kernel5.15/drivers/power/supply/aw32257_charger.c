/*
 * charger-aw32257.c   aw32257 charge module
 *
 * Copyright (c) 2022 AWINIC Technology CO., LTD
 *
 *  Author: lujiazhuo <lujiazhuo@awinic.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/alarmtimer.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include <soc/sprd/board.h>
#define AW32257_DRIVER_VERSION "V1.1.0"

#define AW32257_BATTERY_NAME			"sc27xx-fgu"
#define BIT_DP_DM_BC_ENB			BIT(0)
#define AW32257_OTG_VALID_MS			500
#define AW32257_FEED_WATCHDOG_VALID_MS		50
#define AW32257_OTG_ALARM_TIMER_MS		15000

#define AW32257_CHARGE

#define AW32257_CON0			0x00
#define AW32257_CON1			0x01
#define AW32257_CON2			0x02
#define AW32257_CON3			0x03
#define AW32257_CON4			0x04
#define AW32257_CON5			0x05
#define AW32257_CON6			0x06
#define AW32257_CON7			0x07
#define AW32257_CON8			0x08
#define AW32257_CON9			0x09
#define AW32257_CONA			0x0A
#define AW32257_REG_NUM			11

#define CON0_OTG_MASK			GENMASK(7, 7)
#define CON0_OTG_SHIFT			7
#define CON0_EN_STAT_MASK		GENMASK(6, 6)
#define CON0_EN_STAT_SHIFT		6
#define CON0_STAT_MASK			GENMASK(5, 4)
#define CON0_STAT_SHIFT			4
#define CON0_BOOST_MASK			GENMASK(3, 3)
#define CON0_BOOST_SHIFT		3
#define CON0_CHG_FAULT_MASK		GENMASK(2, 0)
#define CON0_CHG_FAULT_SHIFT		0

#define CON1_CHAR_ENABLE_MASK		GENMASK(2, 0)
#define CON1_TE_MASK			GENMASK(3, 3)
#define CON1_TE_SHIFT			3
#define CON1_CEN_MASK			GENMASK(2, 2)
#define CON1_CEN_SHIFT			2
#define CON1_HZ_MODE_MASK		GENMASK(1, 1)
#define CON1_HZ_MODE_SHIFT		1
#define CON1_OPA_MODE_MASK		GENMASK(0, 0)
#define CON1_OPA_MODE_SHIFT		0

#define CON2_VOREG_MASK			GENMASK(7, 2)
#define CON2_VOREG_SHIFT		2
#define CON2_OTG_PL_MASK		GENMASK(1, 1)
#define CON2_OTG_PL_SHIFT		1
#define CON2_OTG_EN_MASK		GENMASK(0, 0)
#define CON2_OTG_EN_SHIFT		0

#define CON3_VENDER_MASK		GENMASK(7, 5)
#define CON3_VENDER_SHIFT		5
#define CON3_PN_MASK			GENMASK(4, 3)
#define CON3_PN_SHIFT			3
#define CON3_REVISION_MASK		GENMASK(2, 0)
#define CON3_REVISION_SHIFT		0

#define CON4_RESET_MASK			GENMASK(7, 7)
#define CON4_RESET_SHIFT		7
#define CON4_I_CHR_MASK			GENMASK(6, 3)
#define CON4_I_CHR_SHIFT		3
#define CON4_I_TERM_MASK		GENMASK(2, 0)
#define CON4_I_TERM_SHIFT		0

#define CON5_DPM_STATUS_MASK		GENMASK(4, 4)
#define CON5_DPM_STATUS_SHIFT		4
#define CON5_CD_STATUS_MASK		GENMASK(3, 3)
#define CON5_CD_STATUS_SHIFT		3
#define CON5_VSP_MASK			GENMASK(2, 0)
#define CON5_VSP_SHIFT			0

#define CON6_ISAFE_MASK			GENMASK(7, 4)
#define CON6_ISAFE_SHIFT		4
#define CON6_VSAFE_MASK			GENMASK(3, 0)
#define CON6_VSAFE_SHIFT		0

#define CON7_TE_P_MASK			GENMASK(7, 7)
#define CON7_TE_P_SHIFT			7
#define CON7_TE_NUM_MASK		GENMASK(6, 5)
#define CON7_TE_NUM_SHIFT		5
#define CON7_TE_DEG_TM_MASK		GENMASK(4, 3)
#define CON7_TE_DEG_TM_SHIFT		3
#define CON7_VRECHG_MASK		GENMASK(1, 0)
#define CON7_VRECHG_SHIFT		0

#define CON8_VENDOR_MASK		GENMASK(7, 0)
#define CON8_TE_P_SHIFT			0

#define CON9_BST_FAULT_MASK		GENMASK(2, 0)
#define CON9_BST_FAULT_SHIFT		0

#define CONA_PWM_FRQ_MASK		GENMASK(7, 7)
#define CONA_PWM_FRQ_SHIFT		7
#define CONA_SLOW_SW_MASK		GENMASK(6, 5)
#define CONA_SLOW_SW_SHIFT		5
#define CONA_FIX_DEADT_MASK		GENMASK(4, 4)
#define CONA_FIX_DEADT_SHIFT		4
#define CONA_FPWM_MASK			GENMASK(3, 3)
#define CONA_FPWM_SHIFT			3
#define CONA_BSTOUT_CFG_MASK		GENMASK(1, 0)
#define CONA_BSTOUT_CFG_SHIFT		0

#define AW32257_DISABLE_PIN_MASK_2730	BIT(0)
#define AW32257_DISABLE_PIN_MASK_2721	BIT(15)
#define AW32257_DISABLE_PIN_MASK_2720	BIT(0)

#define AW32257_VENDOR_ID			0x2

#define CON30_UNLOCK			0x30
#define CON30_UNLOCK_MASK		GENMASK(7, 0)	

#if defined(ZCFG_MK_CHAR_EN_ONLY_USE_GPIO)|| defined(ZCFG_MK_CHAR_EN_USE_GPIO_AND_PMIC)
	static uint32_t charge_en_gpio =  0;
	#define ENABLE_CHARGE 			  0
	#define DISENABLE_CHARGE 		  1
#endif



const unsigned int CSTH33[] = {
	496000, 620000, 868000, 992000,
	1116000, 1240000, 1364000, 1488000,
	1612000, 1736000, 1860000, 1984000,
	2108000, 2232000, 2356000, 2480000
};

struct aw32257_charge_current {
	int sdp_limit;
	int sdp_cur;
	int dcp_limit;
	int dcp_cur;
	int cdp_limit;
	int cdp_cur;
	int unknown_limit;
	int unknown_cur;
};

struct aw32257_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct power_supply *psy_usb;
	struct aw32257_charge_current cur;
	struct work_struct work;
	struct mutex lock;
	struct delayed_work otg_work;
	struct regmap *pmic;
	struct gpio_desc *gpiod;
	struct extcon_dev *edev;
	struct alarm otg_timer;

	u32 limit;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	u32 r_sns;

	bool charging;
	bool otg_enable;
};

unsigned int charging_value_to_parameter(const unsigned int *parameter,
			const unsigned int array_size, const unsigned int val)
{
	if (val < array_size)
		return parameter[val];
	pr_info("Can't find the parameter\n");
	return parameter[0];
}

unsigned int charging_parameter_to_value(const unsigned int *parameter,
			const unsigned int array_size, const unsigned int val)
{
	unsigned int i;

	pr_debug_ratelimited("array_size = %d\n", array_size);

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
	}

	pr_info("NO register value match\n");
	/* TODO: ASSERT(0);	// not find the value */
	return 0;
}

static unsigned int bmt_find_closest_level(const unsigned int *pList,
			unsigned int number, unsigned int level)
{
	unsigned int i;
	unsigned int max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = 1;
	else
		max_value_in_last_element = 0;

	if (max_value_in_last_element == 1) {
		/* max value in the last element */
		for (i = (number - 1); i != 0; i--) {
			if (pList[i] <= level)
				return pList[i];
		}
		pr_info("Can't find closest level\n");
		return pList[0];
		/* return 000; */
	} else {
		/* max value in the first element */
		for (i = 0; i < number; i++) {
			if (pList[i] <= level)
				return pList[i];
		}
		pr_info("Can't find closest level\n");
		return pList[number - 1];
		/* return 000; */
	}

	return 0;
}

static int aw32257_read(struct aw32257_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int aw32257_write(struct aw32257_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int aw32257_update_bits(struct aw32257_charger_info *info, u8 reg,
		u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = aw32257_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return aw32257_write(info, reg, v);
}

static int aw32257_get_vendor_id(struct aw32257_charger_info *info, u8 *data)
{
	u8 reg_val = 0;
	int ret = 0;

	ret = aw32257_read(info, AW32257_CON3, &reg_val);
	if (ret < 0)
	   return ret;

	reg_val &= CON3_VENDER_MASK;
	reg_val = reg_val >> CON3_VENDER_SHIFT;
	*data = reg_val;

	return ret;
}

static int aw32257_get_vsafe_value(struct aw32257_charger_info *info, u32 *data)
{
	u8 reg_val = 0;
	int ret = 0;

	ret = aw32257_read(info, AW32257_CON6, &reg_val);
	//printk("aw32257_get_vsafe_value: reg_val=0x%x ret=%d\n", reg_val, ret);
	if (ret < 0)
	   return ret;

	reg_val &= CON6_VSAFE_MASK;
	reg_val = reg_val >> CON6_VSAFE_SHIFT;
	//printk("aw32257_get_vsafe_value: reg_val--2=0x%x\n", reg_val);
	if (reg_val == 0x1)
		*data = 4220;
	else if (reg_val == 0x2)
		*data = 4240;
	else if (reg_val == 0x3)
		*data = 4260;
	else if (reg_val == 0x4)
		*data = 4280;
	else if (reg_val == 0x5)
		*data = 4300;
	else if (reg_val == 0x6)
		*data = 4320;
	else if (reg_val == 0x7)
		*data = 4340;
	else if (reg_val == 0x8)
		*data = 4360;
	else if (reg_val == 0x9)
		*data = 4380;
	else if (reg_val == 0xa)
		*data = 4400;
	else if (reg_val == 0xb)
		*data = 4420;
	else if (reg_val == 0xc)
		*data = 4440;
	else if (reg_val == 0xd)
		*data = 4460;
	else if (reg_val == 0xe)
		*data = 4480;
	else if (reg_val == 0xf)
		*data = 4500;
	else
		*data = 4200;
	
	return ret;
}
#if 0
static bool aw32257_is_bat_present(struct aw32257_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(AW32257_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return present;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
					&val);
	if ((ret == 0) && val.intval)
		present = true;
	power_supply_put(psy);

	if (ret)
		dev_err(info->dev,
			"Failed to get property of present:%d\n", ret);

	return present;
}
#endif
static int aw32257_set_safety_vol(struct aw32257_charger_info *info,
					u32 vol)
{
	u8 reg_val;
#ifdef ZCFG_AW32257_SET_SAFETY_VOLTAGE
	int ret = 0;
#endif
	if (vol < 4200)
		vol = 4200;
	if (vol > 4500)
		vol = 4500;
	reg_val = (vol - 4200) / 20 + 1;

#ifdef ZCFG_AW32257_SET_SAFETY_VOLTAGE
	aw32257_update_bits(info,
				CON30_UNLOCK,
				CON30_UNLOCK_MASK,
				0xC7);

	ret = aw32257_update_bits(info,
				   AW32257_CON6,
				   CON6_VSAFE_MASK,
				   reg_val);

	aw32257_update_bits(info,
				CON30_UNLOCK,
				CON30_UNLOCK_MASK,
				0x0);
	return ret;
#else
	return aw32257_update_bits(info,
				   AW32257_CON6,
				   CON6_VSAFE_MASK,
				   reg_val);
#endif

	
}

static int aw32257_set_termina_vol(struct aw32257_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < 3500)
		reg_val = 0x0;
	else if (vol >= 4500)
		reg_val = 0x32;
	else
		reg_val = (vol - 3499) / 20;

	return aw32257_update_bits(info,
				   AW32257_CON2,
				   CON2_VOREG_MASK,
				   reg_val << CON2_VOREG_SHIFT);
}

static int aw32257_set_safety_cur(struct aw32257_charger_info *info, u32 cur)
{
	u8 reg_val = 0;
	int ret = 0;

	if (info->r_sns == 33) {
		if (cur < 620000)
			reg_val = 0x0;
		else if (cur >= 620000 && cur < 868000)
			reg_val = 0x1;
		else if (cur >= 868000 && cur < 992000)
			reg_val = 0x2;
		else if (cur >= 992000 && cur < 1116000)
			reg_val = 0x3;
		else if (cur >= 1116000 && cur < 1240000)
			reg_val = 0x4;
		else if (cur >= 1240000 && cur < 1364000)
			reg_val = 0x5;
		else if (cur >= 1364000 && cur < 1488000)
			reg_val = 0x6;
		else if (cur >= 1488000 && cur < 1612000)
			reg_val = 0x7;
		else if (cur >= 1612000 && cur < 1736000)
			reg_val = 0x8;
		else if (cur >= 1736000 && cur < 1860000)
			reg_val = 0x9;
		else if (cur >= 1860000 && cur < 1984000)
			reg_val = 0xa;
		else if (cur >= 1984000 && cur < 2108000)
			reg_val = 0xb;
		else if (cur >= 2108000 && cur < 2232000)
			reg_val = 0xc;
		else if (cur >= 2232000 && cur < 2356000)
			reg_val = 0xd;
		else if (cur >= 2356000 && cur < 2480000)
			reg_val = 0xe;
		else if (cur >= 2480000)
			reg_val = 0xf;
	}


	aw32257_update_bits(info,
				CON30_UNLOCK,
				CON30_UNLOCK_MASK,
				0xC7);

	ret = aw32257_update_bits(info,
				   AW32257_CON6,
				   CON6_ISAFE_MASK,
				   reg_val << CON6_ISAFE_SHIFT);

	aw32257_update_bits(info,
				CON30_UNLOCK,
				CON30_UNLOCK_MASK,
				0x0);

	return ret;
}

extern void zyt_info_s2(char* s1,char* s2);

static void charge_ic_zyt_info(struct aw32257_charger_info *info)
{
	u8 vendor_id = 0;
	int ret;

	ret = aw32257_get_vendor_id(info, &vendor_id);
	if (vendor_id == AW32257_VENDOR_ID)
	{
		zyt_info_s2("[CHARGE_IC]:","AW32257");
	}
}
extern int s_max_volt,s_reg06_vsafe_val;
static int aw32257_hw_init(struct aw32257_charger_info *info)
{
	struct sprd_battery_info bat_info = {};
	int voltage_max_microvolt, current_max_ua;
	int ret;
	u32 reg_val = 0;

	ret = sprd_battery_get_battery_info(info->psy_usb, &bat_info);
	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");

		/*
		 * If no battery information is supplied, we should set
		 * default charge termination current to 100 mA, and default
		 * charge termination voltage to 4.2V.
		 */
		info->cur.sdp_limit = 500000;
		info->cur.sdp_cur = 500000;
		info->cur.dcp_limit = 5000000;
		info->cur.dcp_cur = 500000;
		info->cur.cdp_limit = 5000000;
		info->cur.cdp_cur = 1500000;
		info->cur.unknown_limit = 1000000;
		info->cur.unknown_cur = 1000000;
	} else {
		info->cur.sdp_limit = bat_info.cur.sdp_limit;
		info->cur.sdp_cur = bat_info.cur.sdp_cur;
		info->cur.dcp_limit = bat_info.cur.dcp_limit;
		info->cur.dcp_cur = bat_info.cur.dcp_cur;
		info->cur.cdp_limit = bat_info.cur.cdp_limit;
		info->cur.cdp_cur = bat_info.cur.cdp_cur;
		info->cur.unknown_limit = bat_info.cur.unknown_limit;
		info->cur.unknown_cur = bat_info.cur.unknown_cur;

		voltage_max_microvolt =
			bat_info.constant_charge_voltage_max_uv / 1000;
		current_max_ua =
			bat_info.constant_charge_current_max_ua / 1000;

		/*****for battry info start*******/
		s_max_volt = voltage_max_microvolt;
		/*****for battry info end*******/	
		sprd_battery_put_battery_info(info->psy_usb, &bat_info);

		if (of_device_is_compatible(info->dev->of_node,
					"awinic,aw32257_chg")) {
			ret = aw32257_set_safety_vol(info,
						voltage_max_microvolt);
			if (ret) {
				dev_err(info->dev,
					"set aw32257 safety vol failed\n");
				return -EIO;
			}

			ret = aw32257_set_safety_cur(info,
						info->cur.dcp_cur);
			if (ret) {
				dev_err(info->dev,
					"set aw32257 safety cur failed\n");
				return -EIO;
			}
			/*****for battry info start*******/
			ret = aw32257_get_vsafe_value(info, &reg_val);
			//printk("aw32257_hw_init: get_vsafe_value = 0x%x\n", reg_val);
			s_reg06_vsafe_val = reg_val;
			/*****for battry info end*******/	
		}

		ret = aw32257_update_bits(info,
					  AW32257_CON4,
					  CON4_RESET_MASK,
					  (0x01 << CON4_RESET_SHIFT));
		if (ret) {
			dev_err(info->dev, "reset aw32257 failed\n");
			return -EIO;
		}

		ret = aw32257_update_bits(info,
					  AW32257_CON5,
					  CON5_VSP_MASK,
					  (0x00 << CON5_VSP_SHIFT));
		if (ret) {
			dev_err(info->dev, "set aw32257 vsp failed\n");
			return -EIO;
		}
#ifdef ZCFG_ENABLE_CHARGE_CURRENT_TERMINATION
		ret = aw32257_update_bits(info,
					  AW32257_CON1,
					  CON1_TE_MASK,
					  (0x01 << CON1_TE_SHIFT));
#else
		ret = aw32257_update_bits(info,
					  AW32257_CON1,
					  CON1_TE_MASK,
					  0);
#endif
		if (ret) {
			dev_err(info->dev, "set aw32257 terminal cur failed\n");
			return -EIO;
		}

		ret = aw32257_set_termina_vol(info, voltage_max_microvolt);
		if (ret) {
			dev_err(info->dev, "set aw32257 terminal vol failed\n");
			return -EIO;
		}
	}

	return ret;
}

static int aw32257_start_charge(struct aw32257_charger_info *info)
{
	int ret;
	dev_info(info->dev, "aw32257 start charge\n");
#if defined(ZCFG_MK_CHAR_EN_USE_GPIO_AND_PMIC)
		printk("start charge_kernel for GPIO and PMIC control\n");
		if (charge_en_gpio)
			gpio_set_value(charge_en_gpio, ENABLE_CHARGE);
		
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask, 0);
		if (ret)
		dev_err(info->dev, "enable aw32257 charge failed\n");
#else
		#if defined(ZCFG_MK_CHAR_EN_ONLY_USE_GPIO)
		printk("start charge_kernel for GPIO control\n");
		if (charge_en_gpio)
			gpio_set_value(charge_en_gpio, ENABLE_CHARGE);
		#else
		printk("start charge_kernel for PMIC control\n");
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask, 0);
		if (ret)
		dev_err(info->dev, "enable aw32257 charge failed\n");
		#endif
#endif

	ret = aw32257_update_bits(info,
				  AW32257_CON1,
				  CON1_CHAR_ENABLE_MASK,
				  0);
	if (ret) {
		dev_err(info->dev, "aw32257 start charge failed\n");
		return -EIO;
	}

	return ret;
}

static int aw32257_stop_charge(struct aw32257_charger_info *info)
{
	int ret = 0;
	dev_info(info->dev, "aw32257_stop_charge\n");

#if defined(ZCFG_MK_CHAR_EN_USE_GPIO_AND_PMIC)
	printk("stop charge_kernel for GPIO and PMIC control\n");
	if (charge_en_gpio)
		gpio_set_value(charge_en_gpio, DISENABLE_CHARGE);
	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask,
				 info->charger_pd_mask);
	if (ret)
		dev_err(info->dev, "disable aw32257 charge failed\n");
#else
	#if defined(ZCFG_MK_CHAR_EN_ONLY_USE_GPIO)
	printk("stop charge_kernel for GPIO control\n");
	if (charge_en_gpio)
		gpio_set_value(charge_en_gpio, DISENABLE_CHARGE);
	#else
	printk("stop charge_kernel for PMIC control\n");
	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask,
				 info->charger_pd_mask);
	if (ret)
		dev_err(info->dev, "disable aw32257 charge failed\n");
	#endif
#endif	
	/*
	ret = aw32257_update_bits(info,
	 			  AW32257_CON1,
	 			  CON1_CEN_MASK,
	 			  (0x01 << CON1_CEN_SHIFT));
	 if (ret)
	 	dev_err(info->dev, "aw32257 stop charge failed\n");
	 */
	 
	return ret;
	
}

u32 old_cur = 2;

static int aw32257_set_current(struct aw32257_charger_info *info,
					u32 current_value)
{
	unsigned int status = 0;
	unsigned int set_chr_current = 0;
	unsigned int array_size;
	unsigned int register_value = 0;
	int ret;

 	//dev_info(info->dev, "eta6937_charger_set_current cur=%d, old_cur=%d\n",current_value,old_cur);
 	if(current_value == 0) {
 			aw32257_stop_charge(info);
 			old_cur = 0;
 			return 0;
 	}else {
 			if(old_cur == 0)
 			aw32257_start_charge(info);
 			old_cur = current_value;
 	}

	if (info->r_sns == 33) {
		array_size = ARRAY_SIZE(CSTH33);
		set_chr_current = bmt_find_closest_level(CSTH33,
				array_size, current_value);
		register_value = charging_parameter_to_value(CSTH33,
				array_size, set_chr_current);
		/*register_value = 0x0a;*/ /*0x0=533mA------(0x0A-0XF)=2000mA;*/
	}

	dev_info(info->dev, "aw32257 set_cru_lev = 0x%x\n", register_value);
	ret = aw32257_update_bits(info,
				  AW32257_CON4,
				  CON4_I_CHR_MASK,
				  (register_value << CON4_I_CHR_SHIFT));
	if (ret) {
		dev_err(info->dev, "aw32257 set current failed\n");
		return -EIO;
	}

	return status;
}

static int aw32257_get_current(struct aw32257_charger_info *info, u32 *ichg)
{
	int status = 0;
	unsigned int array_size;
	unsigned char reg_value;
    extern char s_reg_value[16];
	array_size = ARRAY_SIZE(CSTH33);

	aw32257_read(info, AW32257_CON4, &reg_value);
    s_reg_value[4] = reg_value;
	reg_value &= CON4_I_CHR_MASK;
	reg_value = reg_value >> CON4_I_CHR_SHIFT;
	if (info->r_sns == 33)
		*ichg = charging_value_to_parameter(CSTH33, array_size, reg_value);
   
	dev_info(info->dev, "aw32257 get_current,0x%x", reg_value);
    
	return status;
}

static int aw32257_get_health(struct aw32257_charger_info *info,
					u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}
#if 0
static int aw32257_get_online(struct aw32257_charger_info *info,
					u32 *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}
#endif
static int aw32257_get_status(struct aw32257_charger_info *info)
{
	if (info->charging == true)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int aw32257_set_status(struct aw32257_charger_info *info,
		int val)
{
	int ret = 0;

	if (!val /*&& info->charging*/) {			//gwh Battery high and low temperature judgment, charging IC turned off charging.
		aw32257_stop_charge(info);
		info->charging = false;
	} else if (val /*&& !info->charging*/) {    //gwh Battery high and low temperature judgment, charging IC turned on charging
		ret = aw32257_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}
#if 0
static void aw32257_work(struct work_struct *data)
{
	struct aw32257_charger_info *info =
		container_of(data, struct aw32257_charger_info, work);
	int ret;
	u32 limit_cur, cur;
	bool present = aw32257_is_bat_present(info);

	dev_info(info->dev, "aw32257_work\n");

	mutex_lock(&info->lock);

	if (info->limit > 0 && !info->charging && present) {
		/* set current limitation and start to charge */
		switch (info->usb_phy->chg_type) {
		case SDP_TYPE:
			limit_cur = info->cur.sdp_limit;
			cur = info->cur.sdp_cur;
			break;
		case DCP_TYPE:
			limit_cur = info->cur.dcp_limit;
			cur = info->cur.dcp_cur;
			break;
		case CDP_TYPE:
			limit_cur = info->cur.cdp_limit;
			cur = info->cur.cdp_cur;
			break;
		default:
			limit_cur = info->cur.unknown_limit;
			cur = info->cur.unknown_cur;
		}

		dev_info(info->dev, "info_cur_val = %u\n", cur);

		ret = aw32257_set_current(info, cur);
		if (ret)
			goto out;
		ret = aw32257_start_charge(info);
		if (ret)
			goto out;

		info->charging = true;
	} else if ((!info->limit && info->charging) || !present) {
		/* Stop charging */
		info->charging = false;
		aw32257_stop_charge(info);
	}

out:
	mutex_unlock(&info->lock);
	dev_info(info->dev, "battery present = %d, charger type = %d\n",
		 present, info->usb_phy->chg_type);
	cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);
}

static int aw32257_usb_change(struct notifier_block *nb,
					unsigned long limit, void *data)
{
	struct aw32257_charger_info *info =
		container_of(nb, struct aw32257_charger_info, usb_notify);

	dev_info(info->dev, "aw32257_usb_change\n");

	info->limit = limit;

	schedule_work(&info->work);
	return NOTIFY_OK;
}
#endif

static int aw32257_usb_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct aw32257_charger_info *info = power_supply_get_drvdata(psy);
	#if defined(ZCFG_MK_CHAR_EN_ONLY_USE_GPIO) || defined(ZCFG_MK_CHAR_EN_USE_GPIO_AND_PMIC)
	u32 cur, health;
	#else
	u32 cur, health, enabled = 0;
	#endif

//	u32 online = 0;
//	enum usb_charger_type type;
	int ret = 0;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	//	if (info->limit)
			val->intval = aw32257_get_status(info);
	//	else
	//		val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = aw32257_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = aw32257_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;
#if 0
	case POWER_SUPPLY_PROP_ONLINE:
		ret = aw32257_get_online(info, &online);
		if (ret)
			goto out;

		val->intval = online;

		break;
#endif
	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = aw32257_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;
#if 0
	case POWER_SUPPLY_PROP_USB_TYPE:
		type = info->usb_phy->chg_type;

		switch (type) {
		case SDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_SDP;
			break;

		case DCP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
			break;

		case CDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_CDP;
			break;

		default:
			val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		}

		break;
#endif
	case POWER_SUPPLY_PROP_CALIBRATE:
		#if defined(ZCFG_MK_CHAR_EN_ONLY_USE_GPIO) || defined(ZCFG_MK_CHAR_EN_USE_GPIO_AND_PMIC)
			if (charge_en_gpio)
			val->intval = !gpio_get_value(charge_en_gpio);
		#else
			ret = regmap_read(info->pmic, info->charger_pd, &enabled);
			if (ret) {
				dev_err(info->dev, "get aw32257 charge status failed\n");
				goto out;
			}
			val->intval = !(enabled & info->charger_pd_mask);
		#endif
		break;

	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int aw32257_usb_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct aw32257_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;
	u32 reg_val = 0;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = aw32257_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
			
			ret = aw32257_set_safety_cur(info,val->intval);
			if (ret) {
				dev_err(info->dev,"set aw32257 safety cur failed\n");
			}
			/*****for battry info start*******/
			ret = aw32257_get_vsafe_value(info, &reg_val);
			//printk("aw32257_hw_init: get_vsafe_value = 0x%x\n", reg_val);
			s_reg06_vsafe_val = reg_val;
			/*****for battry info end*******/	

		// ret = aw32257_set_current(info, val->intval);
		// if (ret < 0)
		// 	dev_err(info->dev, "set charge current failed\n");

		break;

	case POWER_SUPPLY_PROP_STATUS:
		ret = aw32257_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;

	/*case POWER_SUPPLY_PROP_FEED_WATCHDOG:
		break;*/

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = aw32257_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;

	case POWER_SUPPLY_PROP_CALIBRATE:
		if (val->intval == true) {
			ret = aw32257_start_charge(info);
			if (ret)
				dev_err(info->dev, "aw32257 start charge failed\n");
		} else if (val->intval == false) {
			aw32257_stop_charge(info);
		}
		break;
	#if defined(ZCFG_TIMER_DUMP_CHARGE_REGISTER)
	case POWER_SUPPLY_PROP_DUMP_REGISTER:
		aw32257_dump_register(info);
		break;
	#endif
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int aw32257_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_STATUS:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}



static enum power_supply_property aw32257_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,

	POWER_SUPPLY_PROP_HEALTH,

	POWER_SUPPLY_PROP_CALIBRATE,
};

static const struct power_supply_desc aw32257_charger_desc = {
	.name			= "aw32257_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= aw32257_usb_props,
	.num_properties		= ARRAY_SIZE(aw32257_usb_props),
	.get_property		= aw32257_usb_get_property,
	.set_property		= aw32257_usb_set_property,
	.property_is_writeable	= aw32257_property_is_writeable,

};
#if 0
static void aw32257_detect_status(struct aw32257_charger_info *info)
{
	int min, max;

	/*
	 * If the USB charger status has been USB_CHARGER_PRESENT before
	 * registering the notifier, we should start to charge with getting
	 * the charge current.
	 */
	if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
		return;

	usb_phy_get_charger_current(info->usb_phy, &min, &max);
	info->limit = min;
	schedule_work(&info->work);
}
#endif 
#ifdef CONFIG_REGULATOR
static void aw32257_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct aw32257_charger_info *info = container_of(dwork,
			struct aw32257_charger_info, otg_work);
	int ret;

	if (!extcon_get_state(info->edev, EXTCON_USB)) {
	dev_dbg(info->dev, "%s:line%d:restart charger otg\n", __func__, __LINE__);
		ret = aw32257_update_bits(info,
					  AW32257_CON1,
					  CON1_HZ_MODE_MASK | CON1_OPA_MODE_MASK,
					  CON1_OPA_MODE_MASK);
		if (ret)
			dev_err(info->dev, "restart aw32257 charger otg failed\n");
	}
	dev_dbg(info->dev, "%s:line%d:schedule_work\n", __func__, __LINE__);
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(500));
}

static int aw32257_enable_otg(struct regulator_dev *dev)
{
	struct aw32257_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				 BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
	if (ret) {
		dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
		return -EIO;
	}

	ret = aw32257_update_bits(info,
				  AW32257_CON1,
				  CON1_HZ_MODE_MASK | CON1_OPA_MODE_MASK,
				  CON1_OPA_MODE_MASK);
	if (ret) {
		dev_err(info->dev, "enable aw32257 otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect,
				BIT_DP_DM_BC_ENB, 0);
		return -EIO;
	}
	dev_dbg(info->dev, "%s:line%d:enable_otg\n", __func__, __LINE__);
	info->otg_enable = true;
	schedule_delayed_work(&info->otg_work,
			msecs_to_jiffies(AW32257_OTG_VALID_MS));

	return 0;
}

static int aw32257_disable_otg(struct regulator_dev *dev)
{
	struct aw32257_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	info->otg_enable = false;
	cancel_delayed_work_sync(&info->otg_work);
	ret = aw32257_update_bits(info,
				  AW32257_CON1,
				  CON1_HZ_MODE_MASK | CON1_OPA_MODE_MASK,
				  0);
	if (ret) {
		dev_err(info->dev, "disable aw32257 otg failed\n");
		return -EIO;
	}

	/* Enable charger detection function to identify the charger type */
	return regmap_update_bits(info->pmic, info->charger_detect,
					BIT_DP_DM_BC_ENB, 0);
	dev_dbg(info->dev, "%s:line%d:disable_otg\n", __func__, __LINE__);
}

static int aw32257_vbus_is_enabled(struct regulator_dev *dev)
{
	struct aw32257_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	ret = aw32257_read(info, AW32257_CON1, &val);
	if (ret) {
		dev_err(info->dev, "failed to get aw32257 otg status\n");
		return -EIO;
	}

	val &= CON1_OPA_MODE_MASK;
	dev_dbg(info->dev, "%s:line%d:vbus_is_enabled\n", __func__, __LINE__);
	return val;
}

static const struct regulator_ops aw32257_vbus_ops = {
	.enable = aw32257_enable_otg,
	.disable = aw32257_disable_otg,
	.is_enabled = aw32257_vbus_is_enabled,
};

static const struct regulator_desc aw32257_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &aw32257_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int aw32257_register_vbus_regulator(struct aw32257_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &aw32257_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

#else
static int aw32257_register_vbus_regulator(struct aw32257_charger_info *info)
{
	return 0;
}
#endif

static int aw32257_dump_register(struct aw32257_charger_info *info)
{
	int i;
	unsigned char reg_data;
	extern char s_reg_value[16];
	for (i = 0; i < 0x0b; i++) {
		aw32257_read(info, i, &reg_data);
		pr_info("aw32257 read in kernel reg0x%x = 0x%x\n", i, reg_data);
		/*****for battry info start*******/
		s_reg_value[i] = reg_data;
		/*****for battry info end*******/
	}

	return 0;
}

#ifdef AW32257_CHARGE
static ssize_t aw32257_set_reg(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int databuf[2];
	struct aw32257_charger_info *info = dev_get_drvdata(dev);

	sscanf(buf, "%x %x", &databuf[0], &databuf[1]);
	aw32257_write(info, databuf[0], databuf[1]);
	return count;
}

static ssize_t aw32257_get_reg(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int idx;
	unsigned char reg_data;

	struct aw32257_charger_info *info = dev_get_drvdata(dev);

	for (idx = 0; idx < 0x0b; idx++) {
		aw32257_read(info, idx, &reg_data);
		pr_info("aw32257 read in kernel reg0x%x = 0x%x)",
				idx, reg_data);
		len += snprintf(buf+len, PAGE_SIZE-len, "reg[0x%x] = 0x%x\n",
				idx, reg_data);
	}
	return len;
}

static ssize_t aw32257_set_sw_rst(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int databuf[1];
	struct aw32257_charger_info *info = dev_get_drvdata(dev);

	sscanf(buf, "%d", &databuf[0]);
	if (databuf[0] == 1)
		aw32257_update_bits(info,
				    AW32257_CON4,
				    CON4_RESET_MASK,
				    (0x01 << CON4_RESET_SHIFT));
	return count;
}

static DEVICE_ATTR(reg, 0660, aw32257_get_reg, aw32257_set_reg);
static DEVICE_ATTR(sw_rst, 0660, NULL,  aw32257_set_sw_rst);

static int aw32257_create_attr(struct i2c_client *client)
{
	int err;
	struct device *dev = &(client->dev);

	err = device_create_file(dev, &dev_attr_reg);
	err = device_create_file(dev, &dev_attr_sw_rst);
	return err;
}
#endif

static int aw32257_parse_dt(struct aw32257_charger_info *info,
					struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret;

	pr_info("%s\n", __func__);

	if (!np) {
		pr_err("%s: no of node\n", __func__);
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "aw32257,r_sns",
					&info->r_sns);
	if (ret) {
		dev_err(info->dev, "read aw32257,r_sns failed");
		return -EINVAL;
	} else
		pr_info("aw32257,r_sns = %d\n", info->r_sns);

	return 0;
}

static int aw32257_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct aw32257_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	int ret;
	
	#if defined(ZCFG_MK_CHAR_EN_ONLY_USE_GPIO) || defined(ZCFG_MK_CHAR_EN_USE_GPIO_AND_PMIC)
	struct device_node *np = client->dev.of_node;
	#endif
	pr_info("aw32257 driver version %s\n", AW32257_DRIVER_VERSION);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->client = client;
	info->dev = dev;
	ret = aw32257_parse_dt(info, &client->dev);
	if (ret < 0)
		goto exit_parse_dts_failed;

	i2c_set_clientdata(client, info);
#if 0
	info->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		dev_err(dev, "failed to find USB phy\n");
		return PTR_ERR(info->usb_phy);
	} else
		dev_info(dev, "successed to find USB phy\n");
#endif
	info->edev = extcon_get_edev_by_phandle(info->dev, 0);
	if (IS_ERR(info->edev)) {
		dev_err(dev, "failed to find vbus extcon device.\n");
		return PTR_ERR(info->edev);
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 1,
					 &info->charger_detect);
	if (ret) {
		dev_err(dev, "failed to get charger_detect\n");
		return -EINVAL;
	}

#if defined(ZCFG_MK_CHAR_EN_USE_GPIO_AND_PMIC)
	ret = of_get_named_gpio(np, "chg-enable-gpios", 0);
	if (gpio_is_valid(ret)) {
		charge_en_gpio= ret;
		ret = gpio_request(charge_en_gpio, "charge enable");
		if (ret) {
			//printk("already request chg-enable-gpios:%d\n",charge_en_gpio);
		}
		gpio_direction_output(charge_en_gpio,0);
	} else {
		charge_en_gpio = 0;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 2,
					 &info->charger_pd);
	if (ret) {
		dev_err(dev, "failed to get charger_pd reg\n");
		return ret;
	}
#else

#if defined(ZCFG_MK_CHAR_EN_ONLY_USE_GPIO)
	ret = of_get_named_gpio(np, "chg-enable-gpios", 0);
	if (gpio_is_valid(ret)) {
		charge_en_gpio = ret;
		ret = gpio_request(charge_en_gpio, "charge enable");
		if (ret) {
			//printk("already request chg-enable-gpios:%d\n",charge_en_gpio);
		}
		gpio_direction_output(charge_en_gpio,0);
	} else {
		charge_en_gpio = 0;
	}
#else
	ret = of_property_read_u32_index(regmap_np, "reg", 2,
					 &info->charger_pd);
	if (ret) {
		dev_err(dev, "failed to get charger_pd reg\n");
		return -EINVAL;
	}
#endif
#endif

	if (of_device_is_compatible(regmap_np->parent, "sprd,sc2730"))
		info->charger_pd_mask = AW32257_DISABLE_PIN_MASK_2730;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
		info->charger_pd_mask = AW32257_DISABLE_PIN_MASK_2721;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2720"))
		info->charger_pd_mask = AW32257_DISABLE_PIN_MASK_2720;
	else {
		dev_err(dev, "failed to get charger_pd mask\n");
		return -EINVAL;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon device\n");
		return -ENODEV;
	}

	of_node_put(regmap_np);
	info->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!info->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		return -ENODEV;
	}

	mutex_init(&info->lock);

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	info->psy_usb = devm_power_supply_register(dev,
						   &aw32257_charger_desc,
						   &charger_cfg);
	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		return PTR_ERR(info->psy_usb);
	}

	ret = aw32257_hw_init(info);
	if (ret)
		return ret;
	aw32257_dump_register(info);

	alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);
	INIT_DELAYED_WORK(&info->otg_work, aw32257_otg_work);

	ret = aw32257_register_vbus_regulator(info);
	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
		goto exit_register_notifier_failed;
	}
#if 0
	INIT_WORK(&info->work, aw32257_work);

	info->usb_notify.notifier_call = aw32257_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(dev, "failed to register notifier:%d\n", ret);
		goto exit_register_notifier_failed;
	}

	aw32257_detect_status(info);
#endif
	ret = aw32257_create_attr(client);
	if (ret) {
		pr_err("create attribute err = %d\n", ret);
		goto exit_create_attr_failed;
	}
    
	charge_ic_zyt_info(info);
	dev_info(dev, "aw32257_probe successfully\n");

	return 0;

exit_create_attr_failed:
	cancel_delayed_work(&info->otg_work);
//	usb_unregister_notifier(info->usb_phy, &info->usb_notify);
exit_register_notifier_failed:
	ret = PTR_ERR(info->psy_usb);
	ret = PTR_ERR(info->edev);
  //ret = PTR_ERR(info->usb_phy);
	cancel_work_sync(&info->work);
exit_parse_dts_failed:
	return ret;
}

static int aw32257_remove(struct i2c_client *client)
{
  //struct aw32257_charger_info *info = i2c_get_clientdata(client);

  //usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}

static int aw32257_suspend(struct device *dev)
{
	struct aw32257_charger_info *info = dev_get_drvdata(dev);
	ktime_t now, add;
	unsigned int wakeup_ms = AW32257_OTG_ALARM_TIMER_MS;

	if (!info->otg_enable)
		return 0;

	now = ktime_get_boottime();
	add = ktime_set(wakeup_ms / MSEC_PER_SEC,
			(wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
	alarm_start(&info->otg_timer, ktime_add(now, add));

	return 0;
}

static int aw32257_resume(struct device *dev)
{
	struct aw32257_charger_info *info = dev_get_drvdata(dev);

	if (!info->otg_enable)
		return 0;

	alarm_cancel(&info->otg_timer);

	return 0;
}

static const struct dev_pm_ops aw32257_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(aw32257_suspend,
				aw32257_resume)
};

static const struct i2c_device_id aw32257_i2c_id[] = {
	{"aw32257_chg", 0},
	{}
};

static const struct of_device_id aw32257_of_match[] = {
	{ .compatible = "awinic,aw32257_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, aw32257_of_match);

static struct i2c_driver aw32257_driver = {
	.driver = {
		.name = "aw32257_chg",
		.of_match_table = aw32257_of_match,
		.pm = &aw32257_pm_ops,
	},
	.probe = aw32257_probe,
	.remove = aw32257_remove,
	.id_table = aw32257_i2c_id,
};

module_i2c_driver(aw32257_driver);
MODULE_DESCRIPTION("AW32257 Charger Driver");
MODULE_LICENSE("GPL v2");

