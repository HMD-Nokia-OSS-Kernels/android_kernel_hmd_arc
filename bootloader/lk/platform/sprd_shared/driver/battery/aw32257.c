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
#include <sprd_bitops.h>
//#include <kernel.h>
#include <lk/board.h>


#define R_SNS 33

#define AW32257_CON0		0x00
#define AW32257_CON1		0x01
#define AW32257_CON2		0x02
#define AW32257_CON3		0x03
#define AW32257_CON4		0x04
#define AW32257_CON5		0x05
#define AW32257_CON6		0x06
#define AW32257_CON7		0x07
#define AW32257_CON8		0x08
#define AW32257_CON9		0x09
#define AW32257_CONA		0x0A
#define AW32257_REG_NUM		11
#define AW32257_CON38			0x38

#define CON0_OTG_MASK		GENMASK(7, 7)
#define CON0_OTG_SHIFT		7
#define CON0_EN_STAT_MASK	GENMASK(6, 6)
#define CON0_EN_STAT_SHIFT	6
#define CON0_STAT_MASK		GENMASK(5, 4)
#define CON0_STAT_SHIFT		4
#define CON0_BOOST_MASK		GENMASK(3, 3)
#define CON0_BOOST_SHIFT	3
#define CON0_CHG_FAULT_MASK	GENMASK(2, 0)
#define CON0_CHG_FAULT_SHIFT	0

#define CON1_CHAR_ENABLE_MASK	GENMASK(2, 0)
#define CON1_TE_MASK		GENMASK(3, 3)
#define CON1_TE_SHIFT		3
#define CON1_CEN_MASK		GENMASK(2, 2)
#define CON1_CEN_SHIFT		2
#define CON1_HZ_MODE_MASK	GENMASK(1, 1)
#define CON1_HZ_MODE_SHIFT	1
#define CON1_OPA_MODE_MASK	GENMASK(0, 0)
#define CON1_OPA_MODE_SHIFT	0

#define CON2_VOREG_MASK		GENMASK(7, 2)
#define CON2_VOREG_SHIFT	2
#define CON2_OTG_PL_MASK	GENMASK(1, 1)
#define CON2_OTG_PL_SHIFT	1
#define CON2_OTG_EN_MASK	GENMASK(0, 0)
#define CON2_OTG_EN_SHIFT	0

#define CON3_VENDER_MASK	GENMASK(7, 5)
#define CON3_VENDER_SHIFT	5
#define CON3_PN_MASK		GENMASK(4, 3)
#define CON3_PN_SHIFT		3
#define CON3_REVISION_MASK	GENMASK(2, 0)
#define CON3_REVISION_SHIFT	0

#define CON4_RESET_MASK		GENMASK(7, 7)
#define CON4_RESET_SHIFT	7
#define CON4_I_CHR_MASK		GENMASK(6, 3)
#define CON4_I_CHR_SHIFT	3
#define CON4_I_TERM_MASK	GENMASK(2, 0)
#define CON4_I_TERM_SHIFT	0

#define CON5_DPM_STATUS_MASK	GENMASK(4, 4)
#define CON5_DPM_STATUS_SHIFT	4
#define CON5_CD_STATUS_MASK	GENMASK(3, 3)
#define CON5_CD_STATUS_SHIFT	3
#define CON5_VSP_MASK		GENMASK(2, 0)
#define CON5_VSP_SHIFT		0

#define CON6_ISAFE_MASK		GENMASK(7, 4)
#define CON6_ISAFE_SHIFT	4
#define CON6_VSAFE_MASK		GENMASK(3, 0)
#define CON6_VSAFE_SHIFT	0

#define CON7_TE_P_MASK		GENMASK(7, 7)
#define CON7_TE_P_SHIFT		7
#define CON7_TE_NUM_MASK	GENMASK(6, 5)
#define CON7_TE_NUM_SHIFT	5
#define CON7_TE_DEG_TM_MASK	GENMASK(4, 3)
#define CON7_TE_DEG_TM_SHIFT	3
#define CON7_VRECHG_MASK	GENMASK(1, 0)
#define CON7_VRECHG_SHIFT	0

#define CON8_VENDOR_MASK	GENMASK(7, 0)
#define CON8_TE_P_SHIFT		0

#define CON9_BST_FAULT_MASK	GENMASK(2, 0)
#define CON9_BST_FAULT_SHIFT	0

#define CONA_PWM_FRQ_MASK	GENMASK(7, 7)
#define CONA_PWM_FRQ_SHIFT	7
#define CONA_SLOW_SW_MASK	GENMASK(6, 5)
#define CONA_SLOW_SW_SHIFT	5
#define CONA_FIX_DEADT_MASK	GENMASK(4, 4)
#define CONA_FIX_DEADT_SHIFT	4
#define CONA_FPWM_MASK		GENMASK(3, 3)
#define CONA_FPWM_SHIFT		3
#define CONA_BSTOUT_CFG_MASK	GENMASK(1, 0)
#define CONA_BSTOUT_CFG_SHIFT	0

#define I2C_SPEED		(100 * 1000)
#define SLAVE_ADDR		(0x6a)

#define VENDOR_AW32257		(0x2)
/*clk.h中有定义但引用不到，因此自行定义*/
#define GENMASK(h, l) \
	(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
 
#define ARRAY_SIZE(x)		(sizeof(x) / sizeof((x)[0]))
static int i2c_bus_num = SPRDCHG_I2C_BUS;
extern int sprd_charge_pd_control(bool enable);

const unsigned int CSTH33[] = {
	496, 620, 868, 992,
	1116, 1240, 1364, 1488,
	1612, 1736, 1860, 1984,
	2108, 2232, 2356, 2480
};

#if defined(CONFIG_DM_SPRD_I2C)
struct udevice *charger;
struct sprd_aw32257_dm_data {
	int i2c_num;
};

static struct sprd_aw32257_dm_data dm_data = {
	.i2c_num = -1,
};

static int sprd_dm_aw32257_i2c_init(void)
{
	int ret;
	printf("lhx sprd_dm_aw32257_i2c_init: dm_data.i2c_num = %d\n", dm_data.i2c_num);

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
		pr_err("lhx %s:failed to set i2c bus num\n", __func__);
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

static int aw32257_write_reg(int reg, u8 val)
{
	dm_i2c_reg_write(charger, reg, val);
	return 0;
}

static int aw32257_read_reg(int reg, u8 *value)
{
	int ret;

	ret = dm_i2c_reg_read(charger, reg);
	if (ret < 0) {
		pr_err("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

	*value = (u8)ret;

	return 0;
}
#else
static int sprd_aw32257_i2c_init(void)
{
	
	//i2c_set_bus_num(CONFIG_SPRDCHG_I2C_BUS);
	//i2c_init(I2C_SPEED, SLAVE_ADDR);
	return 0;
}

static int aw32257_write_reg(int reg, u8 val)
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

//	i2c_reg_write(SLAVE_ADDR,reg,val);
   	return 0;
}

static int aw32257_read_reg(int reg, u8 *value)
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
	dprintf(ALWAYS,"######aw32257readreg reg  = %d value =%d/%x\n",reg, *value, *value);
	return 0;
}


#endif
unsigned int charging_parameter_to_value(const unsigned int *parameter,
			const unsigned int array_size, const unsigned int val)
{
	unsigned int i;

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
	}

	dprintf(ALWAYS,"NO register value match\n");
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
		dprintf(ALWAYS,"Can't find closest level\n");
		return pList[0];
		/* return 000; */
	} else {
		/* max value in the first element */
		for (i = 0; i < number; i++) {
			if (pList[i] <= level)
				return pList[i];
		}
		dprintf(ALWAYS,"Can't find closest level\n");
		return pList[number - 1];
		/* return 000; */
	}

	return 0;
}

static void aw32257_set_value(BYTE reg, BYTE reg_bit,
		BYTE reg_shift, BYTE val)
{
	BYTE tmp;
	int ret;

	ret = aw32257_read_reg(reg, &tmp);
	if (ret < 0)
		return;

	tmp = (tmp & (~reg_bit)) | (val << reg_shift);
	if ((0x04 == reg) && ((CON4_RESET_MASK) != reg_bit))
		tmp &= 0x7f;

	aw32257_write_reg(reg, tmp);
}

static BYTE aw32257_get_value(BYTE reg, BYTE reg_bit, BYTE reg_shift)
{
	BYTE data = 0;
	BYTE ret = 0 ;
	ret = aw32257_read_reg(reg, &data);

	ret = (data & reg_bit) >> reg_shift;

	return ret;
}

void sprdchg_aw32257_start_chg(int type)
{
	sprd_charge_pd_control(true);
}

void sprdchg_aw32257_stop_charging(int value)
{
	dprintf(ALWAYS,"aw32257 stop charge\n");
	sprd_charge_pd_control(false);
}

void  aw32257_sw_reset(void)
{
	dprintf(ALWAYS,"aw32257_sw_reset\n");
	aw32257_set_value(AW32257_CON4, CON4_RESET_MASK, CON4_RESET_SHIFT, 1);
}

void sprdchg_aw32257_ic_init(void)
{
	unsigned char reg_data;

#if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)
	aw32257_write_reg(0x06, 0xfb); // set ISAFE and HW CV point (4.42)
	aw32257_write_reg(0x02, 0xba); // 4.42
#else
	aw32257_write_reg(0x06, 0x40); // set ISAFE	and HW CV point (4.20)
	aw32257_write_reg(0x02, 0x8e); // 4.2
#endif
	aw32257_write_reg(0x01, 0x08);
	aw32257_write_reg(0x04, 0x01);
	
	aw32257_write_reg(0x30, 0xC7);

#if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)
	aw32257_write_reg(0x06, 0xfb); // set ISAFE and HW CV point (4.42)
#else
	aw32257_write_reg(0x06, 0x40); // set ISAFE	and HW CV point (4.20)
#endif	
	aw32257_read_reg(AW32257_CON38,&reg_data);
	pr_info("aw32257 read reg0x38 = 0x%x\n", reg_data);	
	reg_data=reg_data | 0x0C;
	aw32257_write_reg(AW32257_CON38, reg_data);
	pr_info("aw32257 write reg0x38 = 0x%x\n", reg_data);
	aw32257_write_reg(0x30, 0x00);
}

void sprdchg_aw32257_reset_timer(void)
{
	dprintf(ALWAYS,"aw32257 reset rimer\n");
}

unsigned char aw32257_get_vendor_id(void)
{
	return aw32257_get_value(AW32257_CON3,
				 CON3_VENDER_MASK,
				 CON3_VENDER_SHIFT);
}

void aw32257_set_chg_current(unsigned char reg_val)
{
	aw32257_set_value(AW32257_CON4,
			  CON4_I_CHR_MASK,
			  CON4_I_CHR_SHIFT,
			  reg_val);
}

unsigned char sprdchg_aw32257_cur2reg(uint32_t cur)
{
	unsigned char reg_val;
	unsigned int set_chr_current;
	unsigned int array_size;

	if (R_SNS == 33) {
		array_size = ARRAY_SIZE(CSTH33);
		set_chr_current = bmt_find_closest_level(CSTH33,
				array_size, cur);
		reg_val = charging_parameter_to_value(CSTH33,
				array_size, set_chr_current);
		/*register_value = 0x0a;*/ /*0x0=533mA------(0x0A-0XF)=2000mA;*/
	}

	return reg_val;
}

void sprdchg_aw32257_set_cur(uint32_t cur)
{
	unsigned char reg_val = 0;

	reg_val = sprdchg_aw32257_cur2reg(cur);
	aw32257_set_chg_current(reg_val);
}

static int sprdchg_aw32257_cmd(enum sprdchg_cmd cmd, int value)
{
	switch (cmd) {
	case CHG_SET_CURRENT:
		sprdchg_aw32257_set_cur(value);
		break;
	default:
		break;
	}

	return 0;
}

static struct sprdchg_ic_operations sprd_extic_op = {
	//.ic_init = sprdchg_aw32257_ic_init,
	.chg_start = sprdchg_aw32257_start_chg,
	.chg_stop = sprdchg_aw32257_stop_charging,
	.timer_callback = sprdchg_aw32257_reset_timer,
	.chg_cmd = sprdchg_aw32257_cmd,
};
#if defined(CONFIG_DM_SPRD_I2C)
static int sprd_dm_aw32257_init(void)
{
	struct udevice *devp;
	int ret;

	ret = uclass_get_device(UCLASS_CHARGER, 0, &devp);
	if (ret) {
		pr_err("lhx %s:failed to get device ret = %d", __func__, ret);
		return ret;
	}
	sprd_dm_aw32257_i2c_init();
	return 0;
}
#endif
void sprdchg_aw32257_init(void)
{
	BYTE data = 0;
	unsigned char vendor_id = 0;
	int ret;
	int i;
	dprintf(ALWAYS,"lhx aw32257 init\n");
#if defined(CONFIG_DM_SPRD_I2C)
	ret = sprd_dm_aw32257_init();
#else
	ret = sprd_aw32257_i2c_init();
#endif
	if (ret) {
		pr_err("lhx aw32257 i2c init failed\n");
		return;
	}

	vendor_id = aw32257_get_vendor_id();
	dprintf(ALWAYS,"lhx aw32257 init vendor_id = 0x%x\n", vendor_id);
	if (vendor_id != VENDOR_AW32257) {
		dprintf(ALWAYS,"lhx aw32257 is not found,not register ops!!!\n");
		return;
	}

	dprintf(ALWAYS,"lhx aw32257 AW32257_REG_NUM = %d\n", AW32257_REG_NUM);

	sprdchg_aw32257_ic_init();

	for (i = 0; i < AW32257_REG_NUM; i++) {
		aw32257_read_reg(i, &data);
		dprintf(ALWAYS,"lhx aw32257 read reg = %d value = 0x%x\n", i, data);
	}

	dprintf(ALWAYS,"lhx aw32257 register charge ops!\n");

	sprdchg_register_ops(&sprd_extic_op);
}
#ifdef CONFIG_DM_SPRD_I2C
static int sprd_aw32257_probe(struct udevice *dev)
{
	u32 i2c_id;
	int ret;

	ret = dev_read_u32(dev, "sprd,aw32257-i2c-bus", &i2c_id);
	if (ret) {
		pr_err("lhx %s:failed to get i2c-bus ret = %d!\n", __func__, ret);
		return -EIO;
	}
	dprintf(ALWAYS,"lhx aw32257 i2c_id = %d\n", i2c_id);
	dm_data.i2c_num = i2c_id;
	return 0;
}

static const struct udevice_id sprd_aw32257_ids[] = {
	{.compatible = "sprd,aw32257-charger"},
	{ }
};

U_BOOT_DRIVER(aw32257) = {
	.name = "sprd-aw32257",
	.id = UCLASS_CHARGER,
	.of_match = sprd_aw32257_ids,
	.probe = sprd_aw32257_probe,
};

UCLASS_DRIVER(charger) = {
	.name = "charger",
	.id = UCLASS_CHARGER,
};
#endif

