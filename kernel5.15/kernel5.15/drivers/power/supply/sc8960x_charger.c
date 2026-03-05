// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
*/

#include <linux/alarmtimer.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/sysfs.h>
#include <linux/pm_wakeup.h>
#include <soc/sprd/board.h>
#include <linux/power/charger-manager.h>

#define SC8960X_REG_0                   0x00
#define SC8960X_REG_1                   0x01
#define SC8960X_REG_2                   0x02
#define SC8960X_REG_3                   0x03
#define SC8960X_REG_4                   0x04
#define SC8960X_REG_5                   0x05
#define SC8960X_REG_6                   0x06
#define SC8960X_REG_7                   0x07
#define SC8960X_REG_8                   0x08
#define SC8960X_REG_9                   0x09
#define SC8960X_REG_A                   0x0a
#define SC8960X_REG_B                   0x0b
#define SC8960X_REG_C                   0x0c
#define SC8960X_REG_D                   0x0d
#define SC8960X_REG_E                   0x0e
#define SC8960X_REG_NUM                 15


#define SC8960X_BATTERY_NAME            "sc27xx-fgu"
#define BIT_DP_DM_BC_ENB                BIT(0)
#define SC8960X_OTG_ALARM_TIMER_S       15

#define SC8960X_REG_IINLIM_BASE         100

#define SC8960X_REG_ICHG_LSB            60
#define SC8960X_REG_ICHG_MASK           GENMASK(5, 0)

#define SC8960X_REG_CHG_MASK            BIT(4)
#define SC8960X_REG_CHG_SHIFT           4


#define SC8960X_REG_RESET_MASK          BIT(7)

#define SC8960X_REG_OTG_MASK            BIT(5)
#define SC8960X_REG_BOOST_FAULT_MASK    BIT(6)

#define SC8960X_REG_WATCHDOG_MASK       BIT(6)

#define SC8960X_REG_WATCHDOG_TIMER_MASK GENMASK(5, 4)
#define SC8960X_REG_WATCHDOG_TIMER_SHIFT    4

#define SC8960X_REG_TERMINAL_VOLTAGE_MASK   GENMASK(7, 3)
#define SC8960X_REG_TERMINAL_VOLTAGE_SHIFT  3

#define SC8960X_REG_TERMINAL_CUR_MASK   GENMASK(3, 0)

#define SC8960X_REG_VINDPM_VOLTAGE_MASK GENMASK(3, 0)
#define SC8960X_REG_OVP_MASK            GENMASK(7, 6)
#define SC8960X_REG_OVP_SHIFT           6

#define SC8960X_REG_EN_HIZ_MASK         BIT(7)
#define SC8960X_REG_EN_HIZ_SHIFT        7

#define SC8960X_DISABLE_BATFET_RST_MASK     BIT(2)
#define SC8960X_DISABLE_BATFET_RST_SHIFT    2

#define SC8960X_REG_LIMIT_CURRENT_MASK  GENMASK(4, 0)

#define SC8960X_DISABLE_PIN_MASK        BIT(0)
#define SC8960X_DISABLE_PIN_MASK_2721   BIT(15)

#define SC8960X_OTG_VALID_MS            500
#define SC8960X_FEED_WATCHDOG_VALID_MS  50
#define SC8960X_OTG_RETRY_TIMES         10
#define SC8960X_LIMIT_CURRENT_MAX       3200000
#define SC8960X_LIMIT_CURRENT_OFFSET    100000
#define SC8960X_REG_IINDPM_LSB          100

#define SC8960X_ROLE_MASTER_DEFAULT     1
#define SC8960X_ROLE_SLAVE              2

#define SC8960X_FCHG_OVP_6V             6000
#define SC8960X_FCHG_OVP_9V             9000
#define SC8960X_FCHG_OVP_14V            14000
#define SC8960X_FAST_CHARGER_VOLTAGE_MAX    10500000
#define SC8960X_NORMAL_CHARGER_VOLTAGE_MAX  6500000

#define SC8960X_WAKE_UP_MS              1000
#define SC8960X_CURRENT_WORK_MS         msecs_to_jiffies(100)

struct sc8960x_charger_sysfs {
    char *name;
    struct attribute_group attr_g;
    struct device_attribute attr_sc8960x_dump_reg;
    struct device_attribute attr_sc8960x_lookup_reg;
    struct device_attribute attr_sc8960x_sel_reg_id;
    struct device_attribute attr_sc8960x_reg_val;
    struct attribute *attrs[5];

    struct sc8960x_charger_info *info;
};

struct sc8960x_charge_current {
    int sdp_limit;
    int sdp_cur;
    int dcp_limit;
    int dcp_cur;
    int cdp_limit;
    int cdp_cur;
    int unknown_limit;
    int unknown_cur;
    int fchg_limit;
    int fchg_cur;
};

struct sc8960x_charger_info {
    struct i2c_client *client;
    struct device *dev;
    struct power_supply *psy_usb;
    struct sc8960x_charge_current cur;
    struct mutex lock;
    //struct mutex input_limit_cur_lock;
    struct delayed_work otg_work;
    struct delayed_work wdt_work;
    struct delayed_work cur_work;
    struct regmap *pmic;
    struct gpio_desc *gpiod;
    struct extcon_dev *typec_extcon;
    struct alarm otg_timer;
    struct sc8960x_charger_sysfs *sysfs;
    u32 charger_detect;
    u32 charger_pd;
    u32 charger_pd_mask;
    u32 new_charge_limit_cur;
    u32 current_charge_limit_cur;
    u32 new_input_limit_cur;
    u32 current_input_limit_cur;
    u32 last_limit_cur;
    u32 actual_limit_cur;
    u32 role;
    bool charging;
    bool need_disable_Q1;
    int termination_cur;
    bool otg_enable;
    unsigned int irq_gpio;
    bool is_wireless_charge;
    bool is_charger_online;
bool use_typec_extcon;
    int reg_id;
    bool disable_power_path;
};

struct sc8960x_charger_reg_tab {
    int id;
    u32 addr;
    char *name;
};

static struct sc8960x_charger_reg_tab reg_tab[SC8960X_REG_NUM + 1] = {
    {0, SC8960X_REG_0, "EN_HIZ/EN_ICHG_MON/IINDPM"},
    {1, SC8960X_REG_1, "PFM_DIS/WD_RST/OTG_CONFIG/CHG_CONFIG/SYS_Min/"
                "Min_VBAT_SEL"},
    {2, SC8960X_REG_2, "BOOST_LIM/ICHG"},
    {3, SC8960X_REG_3, "IPRECHG/ITERM"},
    {4, SC8960X_REG_4, "VREG/TOPOFF_TIMER/VRECHG"},
    {5, SC8960X_REG_5, "EN_TERM/WATCHDOG/EN_TIMER/CHG_TIMER/TREG/"
                "JEITA_ISET"},
    {6, SC8960X_REG_6, "OVP/BOOSTV1/VINDPM"},
    {7, SC8960X_REG_7, "IINDET_EN/TMR2X_EN/BATFET_DIS/JEITA_VSET/BATFET_DLY/"
                "BATFET_RST_EN/VDPM_BAT_TRACK"},
    {8, SC8960X_REG_8, "VBUS_STAT/CHRG_STAT/PG_STAT/THERM_STAT/VSYS_STAT"},
    {9, SC8960X_REG_9, "WATCHDOG_FAULT/BOOST_FAULT/CHRG_FAULT/BAT_FAULT/"
                "NTC_FAULT"},
    {10, SC8960X_REG_A, "VBUS_GD/VINDPM_STAT/IINDPM_STAT/CV_STAT/"
                "TOPOFF_ACTIVE/ACOV_STAT/VINDPM_INT_MASK/IINDPM_INT_MASK"},
    {11, SC8960X_REG_B, "REG_RST/PN/DEV_REV"},
    {12, SC8960X_REG_C, "JEITA_COOL_ISET2/JEITA_WARM_VSET2/JEITA_WARM_ISET/"
                "JEITA_COOL_TEMP/JEITA_WARM_TEMP"},
    {13, SC8960X_REG_D, "VBAT_REG_LSB/BOOST_NTC_HOT_TEMP/BOOST_NTC_COLD_TEMP/"
                "BOOSTV2/ISCP"},
    {14, SC8960X_REG_E, "VTC_VBATLOW/INPUT_DET_DONE/AUTO_DPDM_EN/BUCK_FRE/"
                "BOOST_FRE/VSYSOVP/NTC_DIS"},
    {15, 0, "null"},
};

static void power_path_control(struct sc8960x_charger_info *info)
{
    struct device_node *cmdline_node;
    const char *cmd_line;
    int ret;
    char *match;
    char result[5];

    cmdline_node = of_find_node_by_path("/chosen");
    ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
    if (ret) {
        info->disable_power_path = false;
        return;
    }

    if (strncmp(cmd_line, "charger", strlen("charger")) == 0)
        info->disable_power_path = true;

    match = strstr(cmd_line, "androidboot.mode=");
    if (match) {
        memcpy(result, (match + strlen("androidboot.mode=")),
            sizeof(result) - 1);
        if ((!strcmp(result, "cali")) || (!strcmp(result, "auto")))
            info->disable_power_path = true;
    }
}

static int sc8960x_charger_set_limit_current(struct sc8960x_charger_info *info,
                        u32 limit_cur, bool enable);
static u32 sc8960x_charger_get_limit_current(struct sc8960x_charger_info *info,
                        u32 *limit_cur);

static bool sc8960x_charger_is_bat_present(struct sc8960x_charger_info *info)
{
    struct power_supply *psy;
    union power_supply_propval val;
    bool present = false;
    int ret;

    psy = power_supply_get_by_name(SC8960X_BATTERY_NAME);
    if (!psy) {
        dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
        return present;
    }

    val.intval = 0;
    ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
                    &val);
    if (ret == 0 && val.intval)
        present = true;
    power_supply_put(psy);

    if (ret)
        dev_err(info->dev,
            "Failed to get property of present:%d\n", ret);

    return present;
}

static int sc8960x_charger_is_fgu_present(struct sc8960x_charger_info *info)
{
    struct power_supply *psy;

    psy = power_supply_get_by_name(SC8960X_BATTERY_NAME);
    if (!psy) {
        dev_err(info->dev, "Failed to find psy of sc27xx_fgu\n");
        return -ENODEV;
    }
    power_supply_put(psy);

    return 0;
}

static int sc8960x_read(struct sc8960x_charger_info *info, u8 reg, u8 *data)
{
    int ret;

    ret = i2c_smbus_read_byte_data(info->client, reg);
    if (ret < 0)
        return ret;

    *data = ret;
    return 0;
}

static int sc8960x_write(struct sc8960x_charger_info *info, u8 reg, u8 data)
{
    return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int sc8960x_update_bits(struct sc8960x_charger_info *info, u8 reg,
                u8 mask, u8 data)
{
    u8 v;
    int ret;

    ret = sc8960x_read(info, reg, &v);
    if (ret < 0)
        return ret;

    v &= ~mask;
    v |= (data & mask);

    return sc8960x_write(info, reg, v);
}

static int sc8960x_get_vendor_id(struct sc8960x_charger_info *info, u8 *data)
{
   u8 reg_val = 0;
   int ret = 0;
   ret = sc8960x_read(info, SC8960X_REG_B, &reg_val);
   if (ret < 0)
	   return ret;
  
   *data = reg_val;
   return ret;
}
static int sc8960x_charger_set_vindpm(struct sc8960x_charger_info *info, u32 vol)
{
    u8 reg_val;

    if (vol < 3900)
        reg_val = 0x0;
    else if (vol <= 5100)
        reg_val = (vol - 3900) / 100;
    else if (vol <= 8000)
        reg_val = 0x0D;
    else if (vol <= 8200)
        reg_val = 0x0E;
    else
        reg_val = 0x0F;

    return sc8960x_update_bits(info, SC8960X_REG_6,
                SC8960X_REG_VINDPM_VOLTAGE_MASK, reg_val);
}

static int sc8960x_charger_set_ovp(struct sc8960x_charger_info *info, u32 vol)
{
    u8 reg_val;

    if (vol <= 5800)
        reg_val = 0x0;
    else if(vol<= 6400)
        reg_val = 0x01;
    else if (vol <= 11000)
        reg_val = 0x02;
    else
        reg_val = 0x03;

    return sc8960x_update_bits(info, SC8960X_REG_6,
                SC8960X_REG_OVP_MASK,
                reg_val << SC8960X_REG_OVP_SHIFT);
}

static int sc8960x_charger_set_termina_vol(struct sc8960x_charger_info *info, u32 vol)
{
    u8 reg_val;

    if (vol < 3848)
        reg_val = 0x0;
    else
        reg_val = (vol - 3848) / 32;

    return sc8960x_update_bits(info, SC8960X_REG_4,
                SC8960X_REG_TERMINAL_VOLTAGE_MASK,
                reg_val << SC8960X_REG_TERMINAL_VOLTAGE_SHIFT);
}

static int sc8960x_charger_set_termina_cur(struct sc8960x_charger_info *info, u32 cur)
{
    u8 reg_val;

    if (cur <= 60)
        reg_val = 0x0;
    else if (cur >= 960)
        reg_val = 0x0F;
    else
        reg_val = (cur - 60) / 60;

    return sc8960x_update_bits(info, SC8960X_REG_3,
                SC8960X_REG_TERMINAL_CUR_MASK,
                reg_val);
}

extern void zyt_info_s2(char* s1,char* s2);

static void charge_ic_zyt_info(struct sc8960x_charger_info *info)
{
	u8 vendor_id = 0;
	int ret;

	ret = sc8960x_get_vendor_id(info, &vendor_id);
	if (info->role == SC8960X_ROLE_SLAVE) {
		zyt_info_s2("[CHARGE_IC]:","sc8960x");
	}else{
	
			zyt_info_s2("[CHARGE_IC]:","sc8960x_charger");
	
	}
}
static int sc8960x_charger_hw_init(struct sc8960x_charger_info *info)
{
    struct sprd_battery_info bat_info = {};
    int voltage_max_microvolt, termination_cur;
    int ret;

    ret = sprd_battery_get_battery_info(info->psy_usb, &bat_info);
    if (ret) {
        dev_warn(info->dev, "no battery information is supplied\n");

        info->cur.sdp_limit = 500000;
        info->cur.sdp_cur = 500000;
        info->cur.dcp_limit = 1500000;
        info->cur.dcp_cur = 1500000;
        info->cur.cdp_limit = 1000000;
        info->cur.cdp_cur = 1000000;
        info->cur.unknown_limit = 1000000;
        info->cur.unknown_cur = 1000000;

        /*
        * If no battery information is supplied, we should set
        * default charge termination current to 120 mA, and default
        * charge termination voltage to 4.44V.
        */
        voltage_max_microvolt = 4440;
        termination_cur = 120;
        info->termination_cur = termination_cur;
    } else {
        info->cur.sdp_limit = bat_info.cur.sdp_limit;
        info->cur.sdp_cur = bat_info.cur.sdp_cur;
        info->cur.dcp_limit = bat_info.cur.dcp_limit;
        info->cur.dcp_cur = bat_info.cur.dcp_cur;
        info->cur.cdp_limit = bat_info.cur.cdp_limit;
        info->cur.cdp_cur = bat_info.cur.cdp_cur;
        info->cur.unknown_limit = bat_info.cur.unknown_limit;
        info->cur.unknown_cur = bat_info.cur.unknown_cur;
        info->cur.fchg_limit = bat_info.cur.fchg_limit;
        info->cur.fchg_cur = bat_info.cur.fchg_cur;

        voltage_max_microvolt = bat_info.constant_charge_voltage_max_uv / 1000;
        termination_cur = bat_info.charge_term_current_ua / 1000;
        info->termination_cur = termination_cur;
        sprd_battery_put_battery_info(info->psy_usb, &bat_info);
    }

    ret = sc8960x_update_bits(info, SC8960X_REG_B,
                SC8960X_REG_RESET_MASK,
                SC8960X_REG_RESET_MASK);
    if (ret) {
        dev_err(info->dev, "reset sc8960x failed\n");
        return ret;
    }

    if (info->role == SC8960X_ROLE_MASTER_DEFAULT) {
        ret = sc8960x_charger_set_ovp(info, SC8960X_FCHG_OVP_6V);
        if (ret) {
            dev_err(info->dev, "set sc8960x ovp failed\n");
            return ret;
        }
    } else if (info->role == SC8960X_ROLE_SLAVE) {
        ret = sc8960x_charger_set_ovp(info, SC8960X_FCHG_OVP_9V);
        if (ret) {
            dev_err(info->dev, "set sc8960x slave ovp failed\n");
            return ret;
        }
    }

    ret = sc8960x_charger_set_vindpm(info, voltage_max_microvolt);
    if (ret) {
        dev_err(info->dev, "set sc8960x vindpm vol failed\n");
        return ret;
    }

    ret = sc8960x_charger_set_termina_vol(info, voltage_max_microvolt);
    if (ret) {
        dev_err(info->dev, "set sc8960x terminal vol failed\n");
        return ret;
    }

    ret = sc8960x_charger_set_termina_cur(info, termination_cur);
    if (ret) {
        dev_err(info->dev, "set sc8960x terminal cur failed\n");
        return ret;
    }

    ret = sc8960x_charger_set_limit_current(info, info->cur.unknown_cur, false);
    if (ret)
        dev_err(info->dev, "set sc8960x limit current failed\n");

    ret = sc8960x_update_bits(info, SC8960X_REG_7,
                SC8960X_DISABLE_BATFET_RST_MASK,
                0x0 << SC8960X_DISABLE_BATFET_RST_SHIFT);
    if (ret)
        dev_err(info->dev, "disable batfet_rst_en failed\n");

    info->current_charge_limit_cur = SC8960X_REG_ICHG_LSB * 1000;
    info->current_input_limit_cur = SC8960X_REG_IINDPM_LSB * 1000;

    return ret;
}

static int sc8960x_charger_get_charge_voltage(struct sc8960x_charger_info *info,
                u32 *charge_vol)
{
    struct power_supply *psy;
    union power_supply_propval val;
    int ret;

    psy = power_supply_get_by_name(SC8960X_BATTERY_NAME);
    if (!psy) {
        dev_err(info->dev, "failed to get SC8960X_BATTERY_NAME\n");
        return -ENODEV;
    }

    val.intval = 0;
    ret = power_supply_get_property(psy,
                    POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
                    &val);
    power_supply_put(psy);
    if (ret) {
        dev_err(info->dev, "failed to get CONSTANT_CHARGE_VOLTAGE\n");
        return ret;
    }

    *charge_vol = val.intval;

    return 0;
}

static int sc8960x_charger_start_charge(struct sc8960x_charger_info *info)
{
    int ret = 0;

    ret = sc8960x_update_bits(info, SC8960X_REG_0,
                SC8960X_REG_EN_HIZ_MASK, 0);
    if (ret)
        dev_err(info->dev, "disable HIZ mode failed\n");

    ret = sc8960x_update_bits(info, SC8960X_REG_5,
                SC8960X_REG_WATCHDOG_TIMER_MASK,
                0x01 << SC8960X_REG_WATCHDOG_TIMER_SHIFT);
    if (ret) {
        dev_err(info->dev, "Failed to enable sc8960x watchdog\n");
        return ret;
    }

    if (info->role == SC8960X_ROLE_MASTER_DEFAULT) {
        ret = regmap_update_bits(info->pmic, info->charger_pd,
                    info->charger_pd_mask, 0);
        if (ret) {
            dev_err(info->dev, "enable sc8960x charge failed\n");
            return ret;
        }

        ret = sc8960x_update_bits(info, SC8960X_REG_1,
                    SC8960X_REG_CHG_MASK,
                    0x1 << SC8960X_REG_CHG_SHIFT);
        if (ret) {
            dev_err(info->dev, "enable sc8960x charge en failed\n");
            return ret;
        }
    } else if (info->role == SC8960X_ROLE_SLAVE) {
        gpiod_set_value_cansleep(info->gpiod, 0);
    }

    ret = sc8960x_charger_set_limit_current(info, info->last_limit_cur, false);
    if (ret) {
        dev_err(info->dev, "failed to set limit current\n");
        return ret;
    }

    ret = sc8960x_charger_set_termina_cur(info, info->termination_cur);
    if (ret)
        dev_err(info->dev, "set sc8960x terminal cur failed\n");

    return ret;
}

static void sc8960x_charger_stop_charge(struct sc8960x_charger_info *info, bool present)
{
    int ret;

    if (info->role == SC8960X_ROLE_MASTER_DEFAULT) {
        if (!present || info->need_disable_Q1) {
            ret = sc8960x_update_bits(info, SC8960X_REG_0,
                        SC8960X_REG_EN_HIZ_MASK,
                        0x01 << SC8960X_REG_EN_HIZ_SHIFT);
            if (ret)
                dev_err(info->dev, "enable HIZ mode failed\n");

            info->need_disable_Q1 = false;
        }

        ret = regmap_update_bits(info->pmic, info->charger_pd,
                    info->charger_pd_mask,
                    info->charger_pd_mask);
        if (ret)
            dev_err(info->dev, "disable sc8960x charge failed\n");

        if (info->is_wireless_charge) {
            ret = sc8960x_update_bits(info, SC8960X_REG_1,
                        SC8960X_REG_CHG_MASK,
                        0x0);
            if (ret)
                dev_err(info->dev, "disable sc8960x charge en failed\n");
        }
    } else if (info->role == SC8960X_ROLE_SLAVE) {
        ret = sc8960x_update_bits(info, SC8960X_REG_0,
                    SC8960X_REG_EN_HIZ_MASK,
                    0x01 << SC8960X_REG_EN_HIZ_SHIFT);
        if (ret)
            dev_err(info->dev, "enable HIZ mode failed\n");

        gpiod_set_value_cansleep(info->gpiod, 1);
    }

    if (info->disable_power_path) {
        ret = sc8960x_update_bits(info, SC8960X_REG_0,
                    SC8960X_REG_EN_HIZ_MASK,
                    0x01 << SC8960X_REG_EN_HIZ_SHIFT);
        if (ret)
            dev_err(info->dev, "Failed to disable power path\n");
    }

    ret = sc8960x_update_bits(info, SC8960X_REG_5,
                SC8960X_REG_WATCHDOG_TIMER_MASK, 0);
    if (ret)
        dev_err(info->dev, "Failed to disable sc8960x watchdog\n");
}

static int sc8960x_charger_set_current(struct sc8960x_charger_info *info, u32 cur)
{
    u8 reg_val;

    cur = cur / 1000;
    if (cur > 3000) {
        reg_val = 0x32;
    } else {
        reg_val = cur / SC8960X_REG_ICHG_LSB;
        reg_val &= SC8960X_REG_ICHG_MASK;
    }

    return sc8960x_update_bits(info, SC8960X_REG_2,
                SC8960X_REG_ICHG_MASK,
                reg_val);
}

static int sc8960x_charger_get_current(struct sc8960x_charger_info *info, u32 *cur)
{
    u8 reg_val;
    int ret;

    ret = sc8960x_read(info, SC8960X_REG_2, &reg_val);
    if (ret < 0)
        return ret;

    reg_val &= SC8960X_REG_ICHG_MASK;
    *cur = reg_val * SC8960X_REG_ICHG_LSB * 1000;

    return 0;
}

static int sc8960x_charger_set_limit_current(struct sc8960x_charger_info *info,
                        u32 limit_cur, bool enable)
{
    u8 reg_val;
    int ret = 0;

   // mutex_lock(&info->input_limit_cur_lock);
    if (enable) {
        ret = sc8960x_charger_get_limit_current(info, &limit_cur);
        if (ret) {
            dev_err(info->dev, "get limit cur failed\n");
            goto out;
        }

        if (limit_cur == info->actual_limit_cur)
            goto out;
    }

    if (limit_cur >= SC8960X_LIMIT_CURRENT_MAX)
        limit_cur = SC8960X_LIMIT_CURRENT_MAX;

    info->last_limit_cur = limit_cur;
    limit_cur -= SC8960X_LIMIT_CURRENT_OFFSET;
    limit_cur = limit_cur / 1000;
    reg_val = limit_cur / SC8960X_REG_IINLIM_BASE;
    info->actual_limit_cur = reg_val * SC8960X_REG_IINLIM_BASE * 1000;
    info->actual_limit_cur += SC8960X_LIMIT_CURRENT_OFFSET;
    ret = sc8960x_update_bits(info, SC8960X_REG_0,
                SC8960X_REG_LIMIT_CURRENT_MASK,
                reg_val);
    if (ret)
        dev_err(info->dev, "set sc8960x limit cur failed\n");

    dev_info(info->dev, "set limit current reg_val = %#x, actual_limit_cur = %d\n",
        reg_val, info->actual_limit_cur);

out:
   // mutex_unlock(&info->input_limit_cur_lock);

    return ret;
}

static u32 sc8960x_charger_get_limit_current(struct sc8960x_charger_info *info, u32 *limit_cur)
{
    u8 reg_val;
    int ret;

    ret = sc8960x_read(info, SC8960X_REG_0, &reg_val);
    if (ret < 0)
        return ret;

    reg_val &= SC8960X_REG_LIMIT_CURRENT_MASK;
    *limit_cur = reg_val * SC8960X_REG_IINLIM_BASE * 1000;
    *limit_cur += SC8960X_LIMIT_CURRENT_OFFSET;
    if (*limit_cur >= SC8960X_LIMIT_CURRENT_MAX)
        *limit_cur = SC8960X_LIMIT_CURRENT_MAX;

    return 0;
}

static int sc8960x_charger_get_health(struct sc8960x_charger_info *info, u32 *health)
{
    *health = POWER_SUPPLY_HEALTH_GOOD;

    return 0;
}

static void sc8960x_dump_register(struct sc8960x_charger_info *info)
{
    int i, ret, len, idx = 0;
    u8 reg_val;
    char buf[256];

    memset(buf, '\0', sizeof(buf));
    for (i = 0; i < SC8960X_REG_NUM; i++) {
        ret = sc8960x_read(info,  reg_tab[i].addr, &reg_val);
        if (ret == 0) {
            len = snprintf(buf + idx, sizeof(buf) - idx,
                    "[REG_0x%.2x]=0x%.2x  ",
                    reg_tab[i].addr, reg_val);
            idx += len;
        }
    }

    dev_info(info->dev, "%s: %s", __func__, buf);
}

static int sc8960x_charger_feed_watchdog(struct sc8960x_charger_info *info)
{
    int ret = 0;

    ret = sc8960x_update_bits(info, SC8960X_REG_1,
                SC8960X_REG_WATCHDOG_MASK,
                SC8960X_REG_WATCHDOG_MASK);
    if (ret) {
        dev_err(info->dev, "reset sc8960x failed\n");
        return ret;
    }

    if (info->otg_enable)
        return ret;

    ret = sc8960x_charger_set_limit_current(info, 0, true);
    if (ret)
        dev_err(info->dev, "set limit cur failed\n");

    return ret;
}

static irqreturn_t sc8960x_int_handler(int irq, void *dev_id)
{
    struct sc8960x_charger_info *info = dev_id;

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return IRQ_HANDLED;
    }

    dev_info(info->dev, "interrupt occurs\n");
    sc8960x_dump_register(info);

    return IRQ_HANDLED;
}

static int sc8960x_charger_get_status(struct sc8960x_charger_info *info)
{
    if (info->charging)
        return POWER_SUPPLY_STATUS_CHARGING;
    else
        return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static bool sc8960x_charger_get_power_path_status(struct sc8960x_charger_info *info)
{
    u8 value;
    int ret;
    bool power_path_enabled = true;

    ret = sc8960x_read(info, SC8960X_REG_0, &value);
    if (ret < 0) {
        dev_err(info->dev, "Fail to get power path status, ret = %d\n", ret);
        return power_path_enabled;
    }

    if (value & SC8960X_REG_EN_HIZ_MASK)
        power_path_enabled = false;

    return power_path_enabled;
}

static int sc8960x_charger_set_power_path_status(struct sc8960x_charger_info *info, bool enable)
{
    int ret = 0;
    u8 value = 0x1;

    if (enable)
        value = 0;

    ret = sc8960x_update_bits(info, SC8960X_REG_0,
                SC8960X_REG_EN_HIZ_MASK,
                value << SC8960X_REG_EN_HIZ_SHIFT);
    if (ret)
        dev_err(info->dev, "%s HIZ mode failed, ret = %d\n",
            enable ? "Enable" : "Disable", ret);

    return ret;
}

static int sc8960x_charger_check_power_path_status(struct sc8960x_charger_info *info)
{
    int ret = 0;

    if (info->disable_power_path)
        return 0;

    if (sc8960x_charger_get_power_path_status(info))
        return 0;

    dev_info(info->dev, "%s:line%d, disable HIZ\n", __func__, __LINE__);

    ret = sc8960x_update_bits(info, SC8960X_REG_0,
                SC8960X_REG_EN_HIZ_MASK, 0);
    if (ret)
        dev_err(info->dev, "disable HIZ mode failed, ret = %d\n", ret);

    return ret;
}

static void sc8960x_check_wireless_charge(struct sc8960x_charger_info *info, bool enable)
{
    int ret;

    if (!enable)
        cancel_delayed_work_sync(&info->cur_work);

    if (info->is_wireless_charge && enable) {
        cancel_delayed_work_sync(&info->cur_work);
        ret = sc8960x_charger_set_current(info, info->current_charge_limit_cur);
        if (ret < 0)
            dev_err(info->dev, "%s:set charge current failed\n", __func__);

        ret = sc8960x_charger_set_current(info, info->current_input_limit_cur);
        if (ret < 0)
            dev_err(info->dev, "%s:set charge current failed\n", __func__);

        pm_wakeup_event(info->dev, SC8960X_WAKE_UP_MS);
        schedule_delayed_work(&info->cur_work, SC8960X_CURRENT_WORK_MS);
    } else if (info->is_wireless_charge && !enable) {
        info->new_charge_limit_cur = info->current_charge_limit_cur;
        info->current_charge_limit_cur = SC8960X_REG_ICHG_LSB * 1000;
        info->new_input_limit_cur = info->current_input_limit_cur;
        info->current_input_limit_cur = SC8960X_REG_IINDPM_LSB * 1000;
    } else if (!info->is_wireless_charge && !enable) {
        info->new_charge_limit_cur = SC8960X_REG_ICHG_LSB * 1000;
        info->current_charge_limit_cur = SC8960X_REG_ICHG_LSB * 1000;
        info->new_input_limit_cur = SC8960X_REG_IINDPM_LSB * 1000;
        info->current_input_limit_cur = SC8960X_REG_IINDPM_LSB * 1000;
    }
}

static int sc8960x_charger_set_status(struct sc8960x_charger_info *info,
                    int val, u32 input_vol, bool bat_present)
{
    int ret = 0;

    if (val == CM_FAST_CHARGE_OVP_ENABLE_CMD) {
        ret = sc8960x_charger_set_ovp(info, SC8960X_FCHG_OVP_9V);
        if (ret) {
            dev_err(info->dev, "failed to set fast charge 9V ovp\n");
            return ret;
        }
    } else if (val == CM_FAST_CHARGE_OVP_DISABLE_CMD) {
        ret = sc8960x_charger_set_ovp(info, SC8960X_FCHG_OVP_6V);
        if (ret) {
            dev_err(info->dev, "failed to set fast charge 5V ovp\n");
            return ret;
        }
        if (info->role == SC8960X_ROLE_MASTER_DEFAULT) {
            if (input_vol > SC8960X_FAST_CHARGER_VOLTAGE_MAX)
                info->need_disable_Q1 = true;
        }
    } else if ((val == false) &&
        (info->role == SC8960X_ROLE_MASTER_DEFAULT)) {
        if (input_vol > SC8960X_NORMAL_CHARGER_VOLTAGE_MAX)
            info->need_disable_Q1 = true;
    }

    if (val > CM_FAST_CHARGE_NORMAL_CMD)
        return 0;

    if (!val && info->charging) {
        sc8960x_check_wireless_charge(info, false);
        sc8960x_charger_stop_charge(info, bat_present);
        info->charging = false;
    } else if (val && !info->charging) {
        sc8960x_check_wireless_charge(info, true);
        ret = sc8960x_charger_start_charge(info);
        if (ret)
            dev_err(info->dev, "start charge failed\n");
        else
            info->charging = true;
    }

    return ret;
}

static void sc8960x_current_work(struct work_struct *data)
{
    struct delayed_work *dwork = to_delayed_work(data);
    struct sc8960x_charger_info *info =
        container_of(dwork, struct sc8960x_charger_info, cur_work);
    int ret = 0;
    bool need_return = false;

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return;
    }

    if (info->current_charge_limit_cur > info->new_charge_limit_cur) {
        ret = sc8960x_charger_set_current(info, info->new_charge_limit_cur);
        if (ret < 0)
            dev_err(info->dev, "%s: set charge limit cur failed\n", __func__);
        return;
    }

    if (info->current_input_limit_cur > info->new_input_limit_cur) {
        ret = sc8960x_charger_set_limit_current(info, info->new_input_limit_cur, false);
        if (ret < 0)
            dev_err(info->dev, "%s: set input limit cur failed\n", __func__);
        return;
    }

    if (info->current_charge_limit_cur + SC8960X_REG_ICHG_LSB * 1000 <=
        info->new_charge_limit_cur)
        info->current_charge_limit_cur += SC8960X_REG_ICHG_LSB * 1000;
    else
        need_return = true;

    if (info->current_input_limit_cur + SC8960X_REG_IINDPM_LSB * 1000 <=
        info->new_input_limit_cur)
        info->current_input_limit_cur += SC8960X_REG_IINDPM_LSB * 1000;
    else if (need_return)
        return;

    ret = sc8960x_charger_set_current(info, info->current_charge_limit_cur);
    if (ret < 0) {
        dev_err(info->dev, "set charge limit current failed\n");
        return;
    }

    ret = sc8960x_charger_set_limit_current(info, info->current_input_limit_cur, false);
    if (ret < 0) {
        dev_err(info->dev, "set input limit current failed\n");
        return;
    }

    dev_info(info->dev, "set charge_limit_cur %duA, input_limit_curr %duA\n",
        info->current_charge_limit_cur, info->current_input_limit_cur);

    schedule_delayed_work(&info->cur_work, SC8960X_CURRENT_WORK_MS);
}

static int sc8960x_charger_usb_get_property(struct power_supply *psy,
                        enum power_supply_property psp,
                        union power_supply_propval *val)
{
    struct sc8960x_charger_info *info = power_supply_get_drvdata(psy);
    u32 cur = 0, health, enabled = 0;
    int ret = 0;

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

   // mutex_lock(&info->lock);

    switch (psp) {
    case POWER_SUPPLY_PROP_STATUS:
        if (val->intval == CM_POWER_PATH_ENABLE_CMD ||
            val->intval == CM_POWER_PATH_DISABLE_CMD) {
            val->intval = sc8960x_charger_get_power_path_status(info);
            break;
        }

        val->intval = sc8960x_charger_get_status(info);
        break;

    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        if (!info->charging) {
            val->intval = 0;
        } else {
            ret = sc8960x_charger_get_current(info, &cur);
            if (ret)
                goto out;

            val->intval = cur;
        }
        break;

    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        if (!info->charging) {
            val->intval = 0;
        } else {
            ret = sc8960x_charger_get_limit_current(info, &cur);
            if (ret)
                goto out;

            val->intval = cur;
        }
        break;

    case POWER_SUPPLY_PROP_HEALTH:
        if (info->charging) {
            val->intval = 0;
        } else {
            ret = sc8960x_charger_get_health(info, &health);
            if (ret)
                goto out;

            val->intval = health;
        }
        break;

    case POWER_SUPPLY_PROP_CALIBRATE:
        if (info->role == SC8960X_ROLE_MASTER_DEFAULT) {
            ret = regmap_read(info->pmic, info->charger_pd, &enabled);
            if (ret) {
                dev_err(info->dev, "get sc8960x charge status failed\n");
                goto out;
            }
            val->intval = !(enabled & info->charger_pd_mask);
        } else if (info->role == SC8960X_ROLE_SLAVE) {
            enabled = gpiod_get_value_cansleep(info->gpiod);
            val->intval = !enabled;
        }

        break;
    default:
        ret = -EINVAL;
    }

out:
    //mutex_unlock(&info->lock);
    return ret;
}

static int sc8960x_charger_usb_set_property(struct power_supply *psy,
                        enum power_supply_property psp,
                        const union power_supply_propval *val)
{
    struct sc8960x_charger_info *info = power_supply_get_drvdata(psy);
    int ret = 0;
    u32 input_vol;
    bool bat_present;

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    /*
    * input_vol and bat_present should be assigned a value, only if psp is
    * POWER_SUPPLY_PROP_STATUS and POWER_SUPPLY_PROP_CALIBRATE.
    */
    if (psp == POWER_SUPPLY_PROP_STATUS || psp == POWER_SUPPLY_PROP_CALIBRATE) {
        bat_present = sc8960x_charger_is_bat_present(info);
        ret = sc8960x_charger_get_charge_voltage(info, &input_vol);
        if (ret) {
            input_vol = 0;
            dev_err(info->dev, "failed to get charge voltage! ret = %d\n", ret);
        }
    }

    //mutex_lock(&info->lock);

    switch (psp) {
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        if (info->is_wireless_charge) {
            cancel_delayed_work_sync(&info->cur_work);
            info->new_charge_limit_cur = val->intval;
            pm_wakeup_event(info->dev, SC8960X_WAKE_UP_MS);
            schedule_delayed_work(&info->cur_work, SC8960X_CURRENT_WORK_MS * 2);
            break;
        }

        ret = sc8960x_charger_set_current(info, val->intval);
        if (ret < 0)
            dev_err(info->dev, "set charge current failed\n");
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        if (info->is_wireless_charge) {
            cancel_delayed_work_sync(&info->cur_work);
            info->new_input_limit_cur = val->intval;
            pm_wakeup_event(info->dev, SC8960X_WAKE_UP_MS);
            schedule_delayed_work(&info->cur_work, SC8960X_CURRENT_WORK_MS * 2);
            break;
        }

        ret = sc8960x_charger_set_limit_current(info, val->intval, false);
        if (ret < 0)
            dev_err(info->dev, "set input current limit failed\n");
        break;

    case POWER_SUPPLY_PROP_STATUS:
        if (val->intval == CM_POWER_PATH_ENABLE_CMD) {
            ret = sc8960x_charger_set_power_path_status(info, true);
            break;
        } else if (val->intval == CM_POWER_PATH_DISABLE_CMD) {
            ret = sc8960x_charger_set_power_path_status(info, false);
            break;
        }

        ret = sc8960x_charger_set_status(info, val->intval, input_vol, bat_present);
        if (ret < 0)
            dev_err(info->dev, "set charge status failed\n");
        break;

    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
        ret = sc8960x_charger_set_termina_vol(info, val->intval / 1000);
        if (ret < 0)
            dev_err(info->dev, "failed to set terminate voltage\n");
        break;

    case POWER_SUPPLY_PROP_CALIBRATE:
        if (val->intval == true) {
            sc8960x_check_wireless_charge(info, true);
            ret = sc8960x_charger_start_charge(info);
            if (ret)
                dev_err(info->dev, "start charge failed\n");
        } else if (val->intval == false) {
            sc8960x_check_wireless_charge(info, false);
            sc8960x_charger_stop_charge(info, bat_present);
        }
        break;
    case POWER_SUPPLY_PROP_TYPE:
        if (val->intval == POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP) {
            info->is_wireless_charge = true;
            ret = sc8960x_charger_set_ovp(info, SC8960X_FCHG_OVP_6V);
        } else if (val->intval == POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP) {
            info->is_wireless_charge = true;
            ret = sc8960x_charger_set_ovp(info, SC8960X_FCHG_OVP_14V);
        } else {
            info->is_wireless_charge = false;
            ret = sc8960x_charger_set_ovp(info, SC8960X_FCHG_OVP_6V);
        }
        if (ret)
            dev_err(info->dev, "failed to set fast charge ovp\n");

        break;
    case POWER_SUPPLY_PROP_PRESENT:
        info->is_charger_online = val->intval;
        if (val->intval == true) {
            schedule_delayed_work(&info->wdt_work, 0);
        } else {
            info->actual_limit_cur = 0;
            cancel_delayed_work_sync(&info->wdt_work);
        }
        break;
    default:
        ret = -EINVAL;
    }

    //mutex_unlock(&info->lock);
    return ret;
}

static int sc8960x_charger_property_is_writeable(struct power_supply *psy,
                        enum power_supply_property psp)
{
    int ret;

    switch (psp) {
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
    case POWER_SUPPLY_PROP_CALIBRATE:
    case POWER_SUPPLY_PROP_TYPE:
    case POWER_SUPPLY_PROP_STATUS:
    case POWER_SUPPLY_PROP_PRESENT:
        ret = 1;
        break;

    default:
        ret = 0;
    }

    return ret;
}

static enum power_supply_property sc8960x_usb_props[] = {
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
    POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
    POWER_SUPPLY_PROP_HEALTH,
    POWER_SUPPLY_PROP_CALIBRATE,
    POWER_SUPPLY_PROP_TYPE,
};

static const struct power_supply_desc sc8960x_charger_desc = {
    .name            = "sc8960x_charger",
    .type            = POWER_SUPPLY_TYPE_UNKNOWN,
    .properties        = sc8960x_usb_props,
    .num_properties        = ARRAY_SIZE(sc8960x_usb_props),
    .get_property        = sc8960x_charger_usb_get_property,
    .set_property        = sc8960x_charger_usb_set_property,
    .property_is_writeable    = sc8960x_charger_property_is_writeable,
};

static const struct power_supply_desc sc8960x_slave_charger_desc = {
    .name            = "sc8960x_slave_charger",
    .type            = POWER_SUPPLY_TYPE_UNKNOWN,
    .properties        = sc8960x_usb_props,
    .num_properties        = ARRAY_SIZE(sc8960x_usb_props),
    .get_property        = sc8960x_charger_usb_get_property,
    .set_property        = sc8960x_charger_usb_set_property,
    .property_is_writeable    = sc8960x_charger_property_is_writeable,
};

static ssize_t sc8960x_register_value_show(struct device *dev,
                    struct device_attribute *attr,
                    char *buf)
{
    struct sc8960x_charger_sysfs *sc8960x_sysfs =
        container_of(attr, struct sc8960x_charger_sysfs,
                attr_sc8960x_reg_val);
    struct  sc8960x_charger_info *info =  sc8960x_sysfs->info;
    u8 val;
    int ret;

    if (!info)
        return snprintf(buf, PAGE_SIZE, "%s  sc8960x_sysfs->info is null\n", __func__);

    ret = sc8960x_read(info, reg_tab[info->reg_id].addr, &val);
    if (ret) {
        dev_err(info->dev, "fail to get  SC8960X_REG_0x%.2x value, ret = %d\n",
            reg_tab[info->reg_id].addr, ret);
        return snprintf(buf, PAGE_SIZE, "fail to get  SC8960X_REG_0x%.2x value\n",
                reg_tab[info->reg_id].addr);
    }

    return snprintf(buf, PAGE_SIZE, "SC8960X_REG_0x%.2x = 0x%.2x\n",
            reg_tab[info->reg_id].addr, val);
}

static ssize_t sc8960x_register_value_store(struct device *dev,
                        struct device_attribute *attr,
                        const char *buf, size_t count)
{
    struct sc8960x_charger_sysfs *sc8960x_sysfs =
        container_of(attr, struct sc8960x_charger_sysfs,
                attr_sc8960x_reg_val);
    struct sc8960x_charger_info *info = sc8960x_sysfs->info;
    u8 val;
    int ret;

    if (!info) {
        dev_err(dev, "%s sc8960x_sysfs->info is null\n", __func__);
        return count;
    }

    ret =  kstrtou8(buf, 16, &val);
    if (ret) {
        dev_err(info->dev, "fail to get addr, ret = %d\n", ret);
        return count;
    }

    ret = sc8960x_write(info, reg_tab[info->reg_id].addr, val);
    if (ret) {
        dev_err(info->dev, "fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
                val, reg_tab[info->reg_id].addr, ret);
        return count;
    }

    dev_info(info->dev, "wite 0x%.2x to REG_0x%.2x success\n", val, reg_tab[info->reg_id].addr);
    return count;
}

static ssize_t sc8960x_register_id_store(struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    struct sc8960x_charger_sysfs *sc8960x_sysfs =
        container_of(attr, struct sc8960x_charger_sysfs,
                attr_sc8960x_sel_reg_id);
    struct sc8960x_charger_info *info = sc8960x_sysfs->info;
    int ret, id;

    if (!info) {
        dev_err(dev, "%s sc8960x_sysfs->info is null\n", __func__);
        return count;
    }

    ret =  kstrtoint(buf, 10, &id);
    if (ret) {
        dev_err(info->dev, "%s store register id fail\n", sc8960x_sysfs->name);
        return count;
    }

    if (id < 0 || id >= SC8960X_REG_NUM) {
        dev_err(info->dev, "%s store register id fail, id = %d is out of range\n",
            sc8960x_sysfs->name, id);
        return count;
    }

    info->reg_id = id;

    dev_info(info->dev, "%s store register id = %d success\n", sc8960x_sysfs->name, id);
    return count;
}

static ssize_t sc8960x_register_id_show(struct device *dev,
                    struct device_attribute *attr,
                    char *buf)
{
    struct sc8960x_charger_sysfs *sc8960x_sysfs =
        container_of(attr, struct sc8960x_charger_sysfs,
                attr_sc8960x_sel_reg_id);
    struct sc8960x_charger_info *info = sc8960x_sysfs->info;

    if (!info)
        return snprintf(buf, PAGE_SIZE, "%s sc8960x_sysfs->info is null\n", __func__);

    return snprintf(buf, PAGE_SIZE, "Curent register id = %d\n", info->reg_id);
}

static ssize_t sc8960x_register_table_show(struct device *dev,
                    struct device_attribute *attr,
                    char *buf)
{
    struct sc8960x_charger_sysfs *sc8960x_sysfs =
        container_of(attr, struct sc8960x_charger_sysfs,
                attr_sc8960x_lookup_reg);
    struct sc8960x_charger_info *info = sc8960x_sysfs->info;
    int i, len, idx = 0;
    char reg_tab_buf[1024];

    if (!info)
        return snprintf(buf, PAGE_SIZE, "%s sc8960x_sysfs->info is null\n", __func__);

    memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
    len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
            "Format: [id] [addr] [desc]\n");
    idx += len;

    for (i = 0; i < SC8960X_REG_NUM; i++) {
        len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
                "[%d] [REG_0x%.2x] [%s];\n",
                reg_tab[i].id, reg_tab[i].addr, reg_tab[i].name);
        idx += len;
    }

    return snprintf(buf, PAGE_SIZE, "%s\n", reg_tab_buf);
}

static ssize_t sc8960x_dump_register_show(struct device *dev,
                    struct device_attribute *attr,
                    char *buf)
{
    struct sc8960x_charger_sysfs *sc8960x_sysfs =
        container_of(attr, struct sc8960x_charger_sysfs,
                attr_sc8960x_dump_reg);
    struct sc8960x_charger_info *info = sc8960x_sysfs->info;

    if (!info)
        return snprintf(buf, PAGE_SIZE, "%s sc8960x_sysfs->info is null\n", __func__);

    sc8960x_dump_register(info);

    return snprintf(buf, PAGE_SIZE, "%s\n", sc8960x_sysfs->name);
}

static int sc8960x_register_sysfs(struct sc8960x_charger_info *info)
{
    struct sc8960x_charger_sysfs *sc8960x_sysfs;
    int ret;

    sc8960x_sysfs = devm_kzalloc(info->dev, sizeof(*sc8960x_sysfs), GFP_KERNEL);
    if (!sc8960x_sysfs)
        return -ENOMEM;

    info->sysfs = sc8960x_sysfs;
    sc8960x_sysfs->name = "sc8960x_sysfs";
    sc8960x_sysfs->info = info;
    sc8960x_sysfs->attrs[0] = &sc8960x_sysfs->attr_sc8960x_dump_reg.attr;
    sc8960x_sysfs->attrs[1] = &sc8960x_sysfs->attr_sc8960x_lookup_reg.attr;
    sc8960x_sysfs->attrs[2] = &sc8960x_sysfs->attr_sc8960x_sel_reg_id.attr;
    sc8960x_sysfs->attrs[3] = &sc8960x_sysfs->attr_sc8960x_reg_val.attr;
    sc8960x_sysfs->attrs[4] = NULL;
    sc8960x_sysfs->attr_g.name = "debug";
    sc8960x_sysfs->attr_g.attrs = sc8960x_sysfs->attrs;

    sysfs_attr_init(&sc8960x_sysfs->attr_sc8960x_dump_reg.attr);
    sc8960x_sysfs->attr_sc8960x_dump_reg.attr.name = "sc8960x_dump_reg";
    sc8960x_sysfs->attr_sc8960x_dump_reg.attr.mode = 0444;
    sc8960x_sysfs->attr_sc8960x_dump_reg.show = sc8960x_dump_register_show;

    sysfs_attr_init(&sc8960x_sysfs->attr_sc8960x_lookup_reg.attr);
    sc8960x_sysfs->attr_sc8960x_lookup_reg.attr.name = "sc8960x_lookup_reg";
    sc8960x_sysfs->attr_sc8960x_lookup_reg.attr.mode = 0444;
    sc8960x_sysfs->attr_sc8960x_lookup_reg.show = sc8960x_register_table_show;

    sysfs_attr_init(&sc8960x_sysfs->attr_sc8960x_sel_reg_id.attr);
    sc8960x_sysfs->attr_sc8960x_sel_reg_id.attr.name = "sc8960x_sel_reg_id";
    sc8960x_sysfs->attr_sc8960x_sel_reg_id.attr.mode = 0644;
    sc8960x_sysfs->attr_sc8960x_sel_reg_id.show = sc8960x_register_id_show;
    sc8960x_sysfs->attr_sc8960x_sel_reg_id.store = sc8960x_register_id_store;

    sysfs_attr_init(&sc8960x_sysfs->attr_sc8960x_reg_val.attr);
    sc8960x_sysfs->attr_sc8960x_reg_val.attr.name = "sc8960x_reg_val";
    sc8960x_sysfs->attr_sc8960x_reg_val.attr.mode = 0644;
    sc8960x_sysfs->attr_sc8960x_reg_val.show = sc8960x_register_value_show;
    sc8960x_sysfs->attr_sc8960x_reg_val.store = sc8960x_register_value_store;

    ret = sysfs_create_group(&info->psy_usb->dev.kobj, &sc8960x_sysfs->attr_g);
    if (ret < 0)
        dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

    return ret;
}

static void sc8960x_charger_feed_watchdog_work(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct sc8960x_charger_info *info = container_of(dwork,
                            struct sc8960x_charger_info,
                            wdt_work);
    int ret;

    ret = sc8960x_charger_feed_watchdog(info);
    if (ret)
        schedule_delayed_work(&info->wdt_work, HZ * 5);
    else
        schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#if IS_ENABLED(CONFIG_REGULATOR)
static bool sc8960x_charger_check_otg_valid(struct sc8960x_charger_info *info)
{
    int ret;
    u8 value = 0;
    bool status = false;

    ret = sc8960x_read(info, SC8960X_REG_1, &value);
    if (ret) {
        dev_err(info->dev, "get sc8960x charger otg valid status failed\n");
        return status;
    }

    if (value & SC8960X_REG_OTG_MASK)
        status = true;
    else
        dev_err(info->dev, "otg is not valid, REG_1 = 0x%x\n", value);

    return status;
}

static bool sc8960x_charger_check_otg_fault(struct sc8960x_charger_info *info)
{
    int ret;
    u8 value = 0;
    bool status = true;

    ret = sc8960x_read(info, SC8960X_REG_9, &value);
    if (ret) {
        dev_err(info->dev, "get sc8960x charger otg fault status failed\n");
        return status;
    }

    if (!(value & SC8960X_REG_BOOST_FAULT_MASK))
        status = false;
    else
        dev_err(info->dev, "boost fault occurs, REG_9 = 0x%x\n", value);

    return status;
}

static void sc8960x_charger_otg_work(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct sc8960x_charger_info *info = container_of(dwork,
            struct sc8960x_charger_info, otg_work);
    bool otg_valid = sc8960x_charger_check_otg_valid(info);
    bool otg_fault;
    int ret, retry = 0;

    if (otg_valid)
        goto out;

    do {
        otg_fault = sc8960x_charger_check_otg_fault(info);
        if (!otg_fault) {
            dev_dbg(info->dev, "%s:line%d:restart charger otg\n", __func__, __LINE__);
            ret = sc8960x_update_bits(info, SC8960X_REG_1,
                        SC8960X_REG_OTG_MASK,
                        SC8960X_REG_OTG_MASK);
            if (ret)
                dev_err(info->dev, "restart sc8960x charger otg failed\n");
        }

        otg_valid = sc8960x_charger_check_otg_valid(info);
    } while (!otg_valid && retry++ < SC8960X_OTG_RETRY_TIMES);

    if (retry >= SC8960X_OTG_RETRY_TIMES) {
        dev_err(info->dev, "Restart OTG failed\n");
        return;
    }

out:
    dev_dbg(info->dev, "%s:line%d:schedule_work\n", __func__, __LINE__);
    schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int sc8960x_charger_enable_otg(struct regulator_dev *dev)
{
    struct sc8960x_charger_info *info = rdev_get_drvdata(dev);
    int ret = 0;

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    //mutex_lock(&info->lock);

    /*
    * Disable charger detection function in case
    * affecting the OTG timing sequence.
    */
   if (!info->use_typec_extcon) {
    ret = regmap_update_bits(info->pmic, info->charger_detect,
                BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
    if (ret) {
        dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
        goto out;
    }
}
    ret = sc8960x_update_bits(info, SC8960X_REG_1,
                  SC8960X_REG_CHG_MASK,
                  0);
    ret = sc8960x_update_bits(info, SC8960X_REG_1,
                SC8960X_REG_OTG_MASK,
                SC8960X_REG_OTG_MASK);
    if (ret) {
        dev_err(info->dev, "enable sc8960x otg failed\n");
        regmap_update_bits(info->pmic, info->charger_detect,
                BIT_DP_DM_BC_ENB, 0);
        goto out;
    }

    info->otg_enable = true;
    schedule_delayed_work(&info->wdt_work,
                msecs_to_jiffies(SC8960X_FEED_WATCHDOG_VALID_MS));
    schedule_delayed_work(&info->otg_work,
                msecs_to_jiffies(SC8960X_OTG_VALID_MS));
out:
   // mutex_unlock(&info->lock);
    dev_info(info->dev, "%s:line%d:enable_otg\n", __func__, __LINE__);

    return ret;
}

static int sc8960x_charger_disable_otg(struct regulator_dev *dev)
{
    struct sc8960x_charger_info *info = rdev_get_drvdata(dev);
    int ret = 0;

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    //mutex_lock(&info->lock);

    info->otg_enable = false;
    cancel_delayed_work_sync(&info->wdt_work);
    cancel_delayed_work_sync(&info->otg_work);
    ret = sc8960x_update_bits(info, SC8960X_REG_1,
                  SC8960X_REG_CHG_MASK,
                  1 << SC8960X_REG_CHG_SHIFT);
    ret |= sc8960x_update_bits(info, SC8960X_REG_1,
                SC8960X_REG_OTG_MASK,
                0);
    if (ret) {
        dev_err(info->dev, "disable sc8960x otg failed\n");
        goto out;
    }

    /* Enable charger detection function to identify the charger type */
    if (!info->use_typec_extcon) {
    ret = regmap_update_bits(info->pmic, info->charger_detect, BIT_DP_DM_BC_ENB, 0);
    if (ret)
        dev_err(info->dev, "enable BC1.2 failed\n");
}
out:
    //mutex_unlock(&info->lock);
    dev_info(info->dev, "%s:line%d:disable_otg\n", __func__, __LINE__);

    return ret;
}

static int sc8960x_charger_vbus_is_enabled(struct regulator_dev *dev)
{
    struct sc8960x_charger_info *info = rdev_get_drvdata(dev);
    int ret;
    u8 val;

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    //mutex_lock(&info->lock);

    ret = sc8960x_read(info, SC8960X_REG_1, &val);
    if (ret) {
        dev_err(info->dev, "failed to get sc8960x otg status\n");
        //mutex_unlock(&info->lock);
        return ret;
    }

    val &= SC8960X_REG_OTG_MASK;

    //mutex_unlock(&info->lock);
    dev_dbg(info->dev, "%s:line%d:vbus_is_enabled\n", __func__, __LINE__);

    return val;
}

static const struct regulator_ops sc8960x_charger_vbus_ops = {
    .enable = sc8960x_charger_enable_otg,
    .disable = sc8960x_charger_disable_otg,
    .is_enabled = sc8960x_charger_vbus_is_enabled,
};

static const struct regulator_desc sc8960x_charger_vbus_desc = {
    .name = "otg-vbus",
    .of_match = "otg-vbus",
    .type = REGULATOR_VOLTAGE,
    .owner = THIS_MODULE,
    .ops = &sc8960x_charger_vbus_ops,
    .fixed_uV = 5000000,
    .n_voltages = 1,
};

static int sc8960x_charger_register_vbus_regulator(struct sc8960x_charger_info *info)
{
    struct regulator_config cfg = { };
    struct regulator_dev *reg;
    int ret = 0;

    cfg.dev = info->dev;
    cfg.driver_data = info;
    reg = devm_regulator_register(info->dev,
                    &sc8960x_charger_vbus_desc, &cfg);
    if (IS_ERR(reg)) {
        ret = PTR_ERR(reg);
        dev_err(info->dev, "Can't register regulator:%d\n", ret);
    }

    return ret;
}

#else
static int sc8960x_charger_register_vbus_regulator(struct sc8960x_charger_info *info)
{
    return 0;
}
#endif

static int sc8960x_charger_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{
    struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
    struct device *dev = &client->dev;
    struct power_supply_config charger_cfg = { };
    struct sc8960x_charger_info *info;
    struct device_node *regmap_np;
    struct platform_device *regmap_pdev;
    int ret;
    bool bat_present;

    if (!adapter) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
        dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
        return -ENODEV;
    }

    info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
    if (!info)
        return -ENOMEM;

    info->client = client;
    info->dev = dev;

    i2c_set_clientdata(client, info);
    power_path_control(info);
info->use_typec_extcon = device_property_read_bool(dev, "use-typec-extcon");
    ret = sc8960x_charger_is_fgu_present(info);
    if (ret) {
        dev_err(dev, "sc27xx_fgu not ready.\n");
        return -EPROBE_DEFER;
    }
    ret = device_property_read_bool(dev, "role-slave");
    if (ret)
        info->role = SC8960X_ROLE_SLAVE;
    else
        info->role = SC8960X_ROLE_MASTER_DEFAULT;

    if (info->role == SC8960X_ROLE_SLAVE) {
        info->gpiod = devm_gpiod_get(dev, "enable", GPIOD_OUT_HIGH);
        if (IS_ERR(info->gpiod)) {
            dev_err(dev, "failed to get enable gpio\n");
            return PTR_ERR(info->gpiod);
        }
    }

    regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
    if (!regmap_np)
        regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");

    if (regmap_np) {
        if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
            info->charger_pd_mask = SC8960X_DISABLE_PIN_MASK_2721;
        else
            info->charger_pd_mask = SC8960X_DISABLE_PIN_MASK;
    } else {
        dev_err(dev, "unable to get syscon node\n");
        return -ENODEV;
    }

    ret = of_property_read_u32_index(regmap_np, "reg", 1,
                    &info->charger_detect);
    if (ret) {
        dev_err(dev, "failed to get charger_detect\n");
        return -EINVAL;
    }

    ret = of_property_read_u32_index(regmap_np, "reg", 2,
                    &info->charger_pd);
    if (ret) {
        dev_err(dev, "failed to get charger_pd reg\n");
        return ret;
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

    bat_present = sc8960x_charger_is_bat_present(info);

   //mutex_init(&info->lock);
    //mutex_init(&info->input_limit_cur_lock);
    //mutex_lock(&info->lock);

    charger_cfg.drv_data = info;
    charger_cfg.of_node = dev->of_node;
    if (info->role == SC8960X_ROLE_MASTER_DEFAULT) {
        info->psy_usb = devm_power_supply_register(dev,
                            &sc8960x_charger_desc,
                            &charger_cfg);
    } else if (info->role == SC8960X_ROLE_SLAVE) {
        info->psy_usb = devm_power_supply_register(dev,
                            &sc8960x_slave_charger_desc,
                            &charger_cfg);
    }

    if (IS_ERR(info->psy_usb)) {
        dev_err(dev, "failed to register power supply\n");
        ret = PTR_ERR(info->psy_usb);
        goto err_regmap_exit;
    }

    ret = sc8960x_charger_hw_init(info);
    if (ret) {
        dev_err(dev, "failed to sc8960x_charger_hw_init\n");
        goto err_psy_usb;
    }

    sc8960x_charger_stop_charge(info, bat_present);
    sc8960x_charger_check_power_path_status(info);

    device_init_wakeup(info->dev, true);

    alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);
    INIT_DELAYED_WORK(&info->otg_work, sc8960x_charger_otg_work);
    INIT_DELAYED_WORK(&info->wdt_work, sc8960x_charger_feed_watchdog_work);

    /*
    * only master to support otg
    */
    if (info->role == SC8960X_ROLE_MASTER_DEFAULT) {
        ret = sc8960x_charger_register_vbus_regulator(info);
        if (ret) {
            dev_err(dev, "failed to register vbus regulator.\n");
            goto err_psy_usb;
        }
    }

    INIT_DELAYED_WORK(&info->cur_work, sc8960x_current_work);

    ret = sc8960x_register_sysfs(info);
    if (ret) {
        dev_err(info->dev, "register sysfs fail, ret = %d\n", ret);
        goto error_sysfs;
    }

    info->irq_gpio = of_get_named_gpio(info->dev->of_node, "irq-gpio", 0);
    if (gpio_is_valid(info->irq_gpio)) {
        ret = devm_gpio_request_one(info->dev, info->irq_gpio,
                        GPIOF_DIR_IN, "sc8960x_int");
        if (!ret)
            info->client->irq = gpio_to_irq(info->irq_gpio);
        else
            dev_err(dev, "int request failed, ret = %d\n", ret);

        if (info->client->irq < 0) {
            dev_err(dev, "failed to get irq no\n");
            gpio_free(info->irq_gpio);
        } else {
            ret = devm_request_threaded_irq(&info->client->dev, info->client->irq,
                            NULL, sc8960x_int_handler,
                            IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                            "sc8960x interrupt", info);
            if (ret)
                dev_err(info->dev, "Failed irq = %d ret = %d\n",
                    info->client->irq, ret);
            else
                enable_irq_wake(client->irq);
        }
    } else {
        dev_err(dev, "failed to get irq gpio\n");
    }

   // mutex_unlock(&info->lock);

    sc8960x_dump_register(info);
dev_info(dev, "use_typec_extcon = %d\n", info->use_typec_extcon);

	charge_ic_zyt_info(info);
    return 0;

error_sysfs:
    sysfs_remove_group(&info->psy_usb->dev.kobj, &info->sysfs->attr_g);
err_psy_usb:
    if (info->irq_gpio)
        gpio_free(info->irq_gpio);
err_regmap_exit:
   // mutex_unlock(&info->lock);
   // mutex_destroy(&info->lock);
    return ret;
}

static void sc8960x_charger_shutdown(struct i2c_client *client)
{
    struct sc8960x_charger_info *info = i2c_get_clientdata(client);
    int ret = 0;

    cancel_delayed_work_sync(&info->wdt_work);
    if (info->otg_enable) {
        info->otg_enable = false;
        cancel_delayed_work_sync(&info->otg_work);
        ret = sc8960x_update_bits(info, SC8960X_REG_1,
                    SC8960X_REG_OTG_MASK,
                    0);
        if (ret)
            dev_err(info->dev, "disable sc8960x otg failed ret = %d\n", ret);

        /* Enable charger detection function to identify the charger type */
        ret = regmap_update_bits(info->pmic, info->charger_detect,
                    BIT_DP_DM_BC_ENB, 0);
        if (ret)
            dev_err(info->dev,
                "enable charger detection function failed ret = %d\n", ret);
    }
}

static int sc8960x_charger_remove(struct i2c_client *client)
{
    struct sc8960x_charger_info *info = i2c_get_clientdata(client);

    cancel_delayed_work_sync(&info->wdt_work);
    cancel_delayed_work_sync(&info->otg_work);

    return 0;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int sc8960x_charger_alarm_prepare(struct device *dev)
{
    struct sc8960x_charger_info *info = dev_get_drvdata(dev);
    ktime_t now, add;

    if (!info) {
        pr_err("%s: info is null!\n", __func__);
        return 0;
    }

    if (!info->otg_enable)
        return 0;

    now = ktime_get_boottime();
    add = ktime_set(SC8960X_OTG_ALARM_TIMER_S, 0);
    alarm_start(&info->otg_timer, ktime_add(now, add));
    return 0;
}

static void sc8960x_charger_alarm_complete(struct device *dev)
{
    struct sc8960x_charger_info *info = dev_get_drvdata(dev);

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return;
    }

    if (!info->otg_enable)
        return;

    alarm_cancel(&info->otg_timer);
}

static int sc8960x_charger_suspend(struct device *dev)
{
    struct sc8960x_charger_info *info = dev_get_drvdata(dev);

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }
    if (info->otg_enable || info->is_charger_online)
        sc8960x_charger_feed_watchdog(info);

    if (!info->otg_enable)
        return 0;

    cancel_delayed_work_sync(&info->wdt_work);
    cancel_delayed_work_sync(&info->cur_work);

    return 0;
}

static int sc8960x_charger_resume(struct device *dev)
{
    struct sc8960x_charger_info *info = dev_get_drvdata(dev);

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    if (info->otg_enable || info->is_charger_online)
        sc8960x_charger_feed_watchdog(info);

    if (!info->otg_enable)
        return 0;

    schedule_delayed_work(&info->wdt_work, HZ * 15);
    schedule_delayed_work(&info->cur_work, 0);

    return 0;
}
#endif

static const struct dev_pm_ops sc8960x_charger_pm_ops = {
    .prepare = sc8960x_charger_alarm_prepare,
    .complete = sc8960x_charger_alarm_complete,
    SET_SYSTEM_SLEEP_PM_OPS(sc8960x_charger_suspend,
                sc8960x_charger_resume)
};

static const struct i2c_device_id sc8960x_i2c_id[] = {
    {"sc8960x_chg", 0},
    {"sc8960x_slave_chg", 0},
    {}
};

static const struct of_device_id sc8960x_charger_of_match[] = {
    { .compatible = "sc,sc8960x_chg", },
    { .compatible = "sc,sc8960x_slave_chg", },
    { }
};

MODULE_DEVICE_TABLE(of, sc8960x_charger_of_match);

static struct i2c_driver sc8960x_charger_driver = {
    .driver = {
        .name = "sc8960x_chg",
        .of_match_table = sc8960x_charger_of_match,
        .pm = &sc8960x_charger_pm_ops,
    },
    .probe = sc8960x_charger_probe,
    .shutdown = sc8960x_charger_shutdown,
    .remove = sc8960x_charger_remove,
    .id_table = sc8960x_i2c_id,
};

module_i2c_driver(sc8960x_charger_driver);

MODULE_AUTHOR("SouthChip<Aiden-yu@southchip.com>");
MODULE_DESCRIPTION("SC8960X Charger Driver");
MODULE_LICENSE("GPL v2");
