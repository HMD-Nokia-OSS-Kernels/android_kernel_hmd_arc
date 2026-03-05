#include "sprd_chg_helper.h"
//#include <dm.h>
#include <errno.h>
//#include <common.h>
//#include <fdtdec.h>
//#include <asm/io.h>
#include <i2c.h>
//#include <clk.h>
#include <chipram_env.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>
#include <lk/debug.h>


#define UPM6920_REG_00				0x00
#define UPM6920_REG_01				0x01
#define UPM6920_REG_02				0x02
#define UPM6920_REG_03				0x03
#define UPM6920_REG_04				0x04
#define UPM6920_REG_05				0x05
#define UPM6920_REG_06				0x06
#define UPM6920_REG_07				0x07
#define UPM6920_REG_08				0x08
#define UPM6920_REG_09				0x09
#define UPM6920_REG_0A				0x0a
#define UPM6920_REG_0B				0x0b
#define UPM6920_REG_0C				0x0c
#define UPM6920_REG_0D				0x0d
#define UPM6920_REG_0E				0x0e
#define UPM6920_REG_0F				0x0f
#define UPM6920_REG_10				0x10
#define UPM6920_REG_11				0x11
#define UPM6920_REG_12				0x12
#define UPM6920_REG_13				0x13
#define UPM6920_REG_14				0x14
#define UPM6920_REG_NUM				21

/* Register 0x00 */
#define REG00_ENHIZ_MASK			0x80
#define REG00_ENHIZ_SHIFT			7
#define REG00_EN_ILIM_MASK			0x40
#define REG00_EN_ILIM_SHIFT			6
#define REG00_IINLIM_MASK			0x3f
#define REG00_IINLIM_SHIFT			0

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
#define REG03_WDT_RESET_MASK			0x40
#define REG03_WDT_RESET_SHIFT			6
#define REG03_OTG_CONFIG_MASK			0x20
#define REG03_OTG_CONFIG_SHIFT			5
#define REG03_CHG_CONFIG_MASK			0x10
#define REG03_CHG_CONFIG_SHIFT			4

/* Register 0x04*/
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

#define REG14_PN				3

#define UPM6920_REG_A2              0xA2
#define REGA2_VBT_3V6_MASK        	0x10
#define REGA2_VBT_3V6_SHIFT       	4
#define REGA2_VBT_3V6_VALUE       	1

#define UPM6920_REG_A3              0xA3
#define REGA3_VDPM_IBUS_MASK        0x40
#define REGA3_VDPM_IBUS_SHIFT       6
#define REGA3_VDPM_IBUS_VALUE       0

#define UPM6920_REG_A4              0xA4
#define REGA4_VDPM_IBUS_MASK        0x10
#define REGA4_VDPM_IBUS_SHIFT       4
#define REGA4_VDPM_IBUS_VALUE       0

#define UPM6920_REG_A5 				0xA5
#define REGA5_CBP_MASK 				0x0C
#define REGA5_CBP_SHIFT  			2
#define REGA5_CBP_VALUE  			3

#define UPM6920_REG_A9              0xA9
#define REGA9_MODE_ENTER            0x6E
#define REGA9_MODE_EXIT             0x00

#define UPM6920_REG_C1 				0xC1
#define REGC1_TBT_MASK 				0x01
#define REGC1_TBT_SHIFT				0
#define REGC1_TBT_VALUE 			1

#define UPM6920_REG_C7              0xC7
#define REGC7_TBT_MASK              0x08
#define REGC7_TBT_SHIFT             3
#define REGC7_TBT_VALUE             1

#define REGC7_CBP_MASK              0x08
#define REGC7_CBP_SHIFT             3
#define REGC7_CBP_VALUE             1

#define UPM6920_REG_C9              0xC9
#define REGC9_BT_MASK               0x40
#define REGC9_BT_SHIFT              6
#define REGC9_BT_VALUE              1

#define UPM6920_REG_D3              0xD3
#define REGD3_OC_MASK               0xE0
#define REGD3_OC_SHIFT              5
#define REGD3_OC_VALUE              0x04

#define UPM6920_REG_D5              0xD5
#define REGD5_CBP_MASK              0x01
#define REGD5_CBP_SHIFT             0
#define REGD5_CBP_VALUE             0x01

#define I2C_SPEED			100000
#define SLAVE_ADDR			0x6A

#define POWER_PATH_ENABLE               0
#define POWER_PATH_DISABLE              1

//extern chipram_env_t* get_chipram_env(void);

static int i2c_bus_num = SPRDCHG_I2C_BUS;

#ifdef CONFIG_DM_UPM6920
struct sprd_UPM6920_dm_data {
	int i2c_num;
};

static struct sprd_UPM6920_dm_data dm_data = {
	.i2c_num = -1,
};

static struct udevice *charger;

static int sprd_dm_UPM6920_i2c_init(void)
{
	int ret;
	u8 value;

	if (dm_data.i2c_num != -1) {
		ret = i2c_get_chip_for_busnum(dm_data.i2c_num,
					      SLAVE_ADDR, 1,
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

	if (!charger) {
		pr_err("NULL pointer in %s:line=%d\n", __func__, __LINE__);
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

static int UPM6920_write_reg(int reg, u8 val)
{
	if (!charger) {
		pr_err("NULL pointer in %s:line%d\n", __func__, __LINE__);
		return -EINVAL;
	}

	dm_i2c_reg_write(charger, reg, val);
	return 0;
}

static int UPM6920_read_reg(int reg, u8 *value)
{
	u8 val;
	int ret;

	if (!charger) {
		pr_err("NULL pointer in %s:line%d\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = dm_i2c_reg_read(charger, reg);
	if (ret < 0) {
		pr_err("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

	*value = (u8)ret;
	debugf("######UPM6920 readreg reg = %d value = %d/%x\n", reg, ret, ret);
	return 0;
}
#else
static int UPM6920_i2c_init(void)
{
//	i2c_set_bus_num(CONFIG_SPRDCHG_I2C_BUS);
//	i2c_init(I2C_SPEED, SLAVE_ADDR);

	return 0;
}

static int UPM6920_write_reg(u8 reg, u8 val)
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

//	i2c_reg_write(SLAVE_ADDR, reg, val);

	return 0;
}

static int UPM6920_read_reg(u8 reg, u8 *value)
{
	int ret;
	uint8_t reg_addr[2] = {0};
	reg_addr[0] = reg;
//	ret = i2c_reg_read(SLAVE_ADDR , reg);
	ret = i2c_read_write(i2c_bus_num, SLAVE_ADDR, reg_addr, 1, value, 1);

	if (ret < 0) {
		dprintf(ALWAYS,"%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

//	ret &= 0xff;
//	*value = ret;
//	printf("UPM6920_read_reg reg=0x%x, value=0x%x\n",reg, ret);
	dprintf(ALWAYS,"UPM6920_read_reg reg = 0x%x, value = %d/0x%x\n", reg, *value, *value);

	return 0;
}
#endif

static void UPM6920_set_value(u8 reg, u8 reg_bit, u8 reg_shift, u8 val)
{
	u8 tmp = 0;

	UPM6920_read_reg(reg, &tmp);
	tmp = (tmp & (~reg_bit)) | (val << reg_shift);
	UPM6920_write_reg(reg, tmp);
}

static u8 UPM6920_get_value(u8 reg, u8 reg_bit, u8 reg_shift)
{
	u8 reg_value = 0;

	UPM6920_read_reg(reg, &reg_value);
	reg_value = (reg_value & reg_bit) >> reg_shift;

	return reg_value;
}

static void chg_UPM6920_set_chg_cur(u32 cur)
{
	u8 reg_value;

	if (cur <= REG04_ICHG_MIN)
		cur = REG04_ICHG_MIN;
	else if (cur >= REG04_ICHG_MAX)
		cur = REG04_ICHG_MAX;
	reg_value = cur / REG04_ICHG_STEP;

	UPM6920_set_value(UPM6920_REG_04, REG04_ICHG_MASK,
			  REG04_ICHG_SHIFT, reg_value);

}

static void chg_UPM6920_set_limit_cur(u32 limit)
{
	u8 reg_value;

	UPM6920_set_value(UPM6920_REG_00, REG00_EN_ILIM_MASK,
			  REG00_EN_ILIM_SHIFT, REG00_EN_ILIM_DISABLE);

	if (limit >= REG00_IINLIM_MAX)
		limit = REG00_IINLIM_MAX;
	if (limit <= REG00_IINLIM_MIN)
		limit = REG00_IINLIM_MIN;

	reg_value = (limit - REG00_IINLIM_OFFSET) / REG00_IINLIM_STEP;
	UPM6920_set_value(UPM6920_REG_00, REG00_IINLIM_MASK,
			  REG00_IINLIM_SHIFT, reg_value);

}

static int UPM6920_set_prechg(u32 ichg)
{
	u8 reg_value;

	if (ichg <= REG05_IPRECHG_MIN)
		ichg = REG05_ITERM_MIN;
	else if (ichg >= REG05_IPRECHG_MAX)
		ichg = REG05_IPRECHG_MAX;

	ichg = ichg - REG05_IPRECHG_OFFSET;
	reg_value = ichg / REG05_IPRECHG_STEP;
	UPM6920_set_value(UPM6920_REG_05, REG05_IPRECHG_MASK,
			  REG05_IPRECHG_SHIFT, reg_value);

	return 0;
}

static int chg_UPM6920_cmd(enum sprdchg_cmd cmd, int value)
{
	switch (cmd) {
	case CHG_SET_CURRENT:
		chg_UPM6920_set_chg_cur(value);
		break;
	case CHG_SET_LIMIT_CURRENT:
		chg_UPM6920_set_limit_cur(value);
		break;
	case CHG_SET_PRE_CURRENT:
		UPM6920_set_prechg(value);
	default:
		break;
	}

	return 0;
}

static void chg_UPM6920_reset(void)
{
	UPM6920_set_value(UPM6920_REG_14, REG14_REG_RESET_MASK,
			  REG14_REG_RESET_SHIFT, REG14_REG_RESET);
}

static void chg_UPM6920_init(void)
{
	int ret;

#ifdef CONFIG_DM_UPM6920
	ret = sprd_dm_UPM6920_i2c_init();
#else
	ret = UPM6920_i2c_init();
#endif
	if (ret) {
		dprintf(ALWAYS,"%s:UPM6920 i2c init fail\n", __func__);
		return;
	}
}

static void chg_UPM6920_enable_chg(int val)
{
	UPM6920_set_value(UPM6920_REG_03, REG03_CHG_CONFIG_MASK,
			  REG03_CHG_CONFIG_SHIFT, REG03_CHG_ENABLE);
}

static void chg_UPM6920_disable_chg(int val)
{
//	chipram_env_t* cr_env = get_chipram_env();

	UPM6920_set_value(UPM6920_REG_03, REG03_CHG_CONFIG_MASK,
			  REG03_CHG_CONFIG_SHIFT, REG03_CHG_DISABLE);
#if 0
	if (cr_env->mode == BOOTLOADER_MODE_DOWNLOAD) {
		UPM6920_set_value(UPM6920_REG_00, REG00_ENHIZ_MASK,
				  REG00_ENHIZ_SHIFT, POWER_PATH_DISABLE);
		UPM6920_set_value(UPM6920_REG_05, REG05_WDT_MASK,
				  REG05_WDT_SHIFT, REG05_WDT_DISABLE);
	}
#endif
}

static void upm6920_additional_setting(void)
{
    u8 val = 0;
    u8 tmp = 0;
    u8 val_00 = 0;

    UPM6920_write_reg(UPM6920_REG_A9, REGA9_MODE_ENTER);

	UPM6920_read_reg(UPM6920_REG_D5, &val);
        printf("regd5:0x%x\n", val);
    tmp = (val & (~REGD5_CBP_MASK)) | (REGD5_CBP_VALUE << REGD5_CBP_SHIFT);
    UPM6920_write_reg(UPM6920_REG_D5, tmp);

	UPM6920_read_reg(UPM6920_REG_A5, &val);
    printf("regA5:0x%x\n", val);
    tmp = (val & (~REGA5_CBP_MASK)) | (REGA5_CBP_VALUE << REGA5_CBP_SHIFT);
    UPM6920_write_reg(UPM6920_REG_A5, tmp);

	UPM6920_read_reg(UPM6920_REG_C7, &val);
    printf("regc7:0x%x\n", val);
    tmp = (val & (~REGC7_CBP_MASK)) | (REGC7_CBP_VALUE << REGC7_CBP_SHIFT);
    UPM6920_write_reg(UPM6920_REG_C7, tmp);

    UPM6920_read_reg(UPM6920_REG_C1, &val);
    printf("regc1:0x%x\n", val);
    tmp = (val & (~REGC1_TBT_MASK)) | (REGC1_TBT_VALUE << REGC1_TBT_SHIFT);
    UPM6920_write_reg(UPM6920_REG_C1, tmp);

    UPM6920_read_reg(UPM6920_REG_A2, &val);
	printf("rega2:0x%x\n", val);
	if (val & REGA2_VBT_3V6_MASK) {
		UPM6920_read_reg(UPM6920_REG_00, &val_00);
		printf("force hiz 100ms, reg00:0x%x\n", val_00);
		tmp = (val_00 & (~REG00_ENHIZ_MASK)) | (1 << REG00_ENHIZ_SHIFT);
		UPM6920_write_reg(UPM6920_REG_00, tmp);
		UPM6920_write_reg(UPM6920_REG_00, tmp);
		mdelay(100);
		tmp = (val_00 & (~REG00_ENHIZ_MASK)) | (0 << REG00_ENHIZ_SHIFT);
		UPM6920_write_reg(UPM6920_REG_00, tmp);
	}

	UPM6920_write_reg(UPM6920_REG_A9, REGA9_MODE_EXIT);
}

static void chg_UPM6920_reset_timer(void)
{
	UPM6920_set_value(UPM6920_REG_03, REG03_WDT_RESET_MASK,
			  REG03_WDT_RESET_SHIFT, REG03_WDT_RESET);
}

static struct sprdchg_ic_operations UPM6920_op ={
	//.ic_init = chg_UPM6920_init,
	.chg_start = chg_UPM6920_enable_chg,
	.chg_stop = chg_UPM6920_disable_chg,
	.timer_callback = chg_UPM6920_reset_timer,
	.chg_cmd = chg_UPM6920_cmd,
};

#ifdef CONFIG_DM_UPM6920
static int sprd_UPM6920_probe(struct udevice *dev)
{
	u32 i2c_id;
	int ret;

	ret = dev_read_u32(dev, "sprd,UPM6920-i2c-bus", &i2c_id);
	if (ret) {
		pr_err("%s:failed to get i2c-bus ret = %d!\n", __func__, ret);
		return ret;
	}
	debugf("i2c_id = %d\n", i2c_id);
	dm_data.i2c_num = i2c_id;
	return 0;
}

static int sprd_dm_UPM6920_init(void)
{
	struct udevice *devp;
	int ret;

	ret = uclass_get_device(UCLASS_CHARGER, 0, &devp);
	if (ret) {
		pr_err("%s:failed to get device ret = %d", __func__, ret);
		return ret;
	}
	ret = sprd_dm_UPM6920_i2c_init();
  	if (ret) {
          	pr_err("%s:failed to init dm i2c ret = %d", __func__, ret);
		return ret;
        }
	return 0;
}
#endif

static int UPM6920_charger_get_vendor_id_part_value(void)
{
	int ret = 0;
	u8 reg_part_val = 0;

	ret = UPM6920_read_reg(UPM6920_REG_14, &reg_part_val);
	if (ret < 0) {
		printf("[%s]l=%d: Failed to get vendor id, ret=%d\n",
			__FUNCTION__, __LINE__, ret);
		return ret;
	}

	reg_part_val = (reg_part_val & REG14_PN_MASK) >> REG14_PN_SHIFT;
	if (reg_part_val != REG14_PN) {
		printf("[%s]l=%d: The part value is 0x%x\n",
			__FUNCTION__, __LINE__, reg_part_val);
		return -EINVAL;
	}

	return ret;
}

static int upm6920_dump_register(void)
{
	int ret;
	u8 addr;
	u8 val;

	for (addr = 0x00; addr <= 0x14; addr++) {
		mdelay(2);
		ret = UPM6920_read_reg(addr, &val);
		if (!ret)
			printf("%s:Reg[%02X] = 0x%02X\n", __func__, addr, val);
	}

	return ret;
}

void sprdchg_upm6920_init(void)
{
	int ret;

	printf("UPM6920 init\n");
#ifdef CONFIG_DM_UPM6920
	ret = sprd_dm_UPM6920_init();
#else
	ret = UPM6920_i2c_init();
#endif
	if (ret) {
		dprintf(ALWAYS,"UPM6920 i2c init failed\n");
		return;
	}

	ret = UPM6920_charger_get_vendor_id_part_value();
	if (ret) {
		printf("[%s]l=%d: upm6920 is not found, not register ops\n",
			__FUNCTION__, __LINE__);
		return ;
	}
	//chg_UPM6920_reset;
	upm6920_additional_setting();
	upm6920_dump_register();
	printf("UPM6920 register charge ops!\n");
	sprdchg_register_ops(&UPM6920_op);
}

#ifdef CONFIG_DM_UPM6920
static const struct udevice_id sprd_UPM6920_ids[] = {
	{.compatible = "sprd,UPM6920-charger"},
	{ }
};

U_BOOT_DRIVER(UPM6920) = {
	.name = "sprd-UPM6920",
	.id = UCLASS_CHARGER,
	.of_match = sprd_UPM6920_ids,
	.probe = sprd_UPM6920_probe,
};

/*
UCLASS_DRIVER(charger)= {
	.name = "charger",
	.id = UCLASS_CHARGER,
};
*/

#endif

