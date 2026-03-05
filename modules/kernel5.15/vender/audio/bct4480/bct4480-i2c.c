// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/usb/typec.h>
#include <linux/extcon.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/jx_msg_notifier.h>
#include <linux/delay.h>

#include <soc/sprd/board.h>

#include "bct4480-i2c.h"

#define BCT4480_I2C_NAME	"bct4480-driver"

#define BCT4480_SWITCH_SETTINGS 0x04
#define BCT4480_SWITCH_CONTROL  0x05
#define BCT4480_SWITCH_STATUS1  0x07
#define BCT4480_SLOW_L          0x08
#define BCT4480_SLOW_R          0x09
#define BCT4480_SLOW_MIC        0x0A
#define BCT4480_SLOW_SENSE      0x0B
#define BCT4480_SLOW_GND        0x0C
#define BCT4480_DELAY_L_R       0x0D
#define BCT4480_DELAY_L_MIC     0x0E
#define BCT4480_DELAY_L_SENSE   0x0F
#define BCT4480_DELAY_L_AGND    0x10
#define BCT4480_RESET           0x1E
#define BCT4480_CHIP_ID         0x00
#define BCT4480_VENDOR_NUM      0x09
#define HL1280_VENDOR_NUM      0x49

#define BCT4480_RES_DETECT_EN    	0x12
#define BCT4480_RES_DETECT_EN_MASK 0x1
#define BCT4480_RES_DETECT_EN_SHIFT 0x1
#define BCT4480_RES_DETECT_VALUE_MASK 0x1
#define BCT4480_RES_DETECT_VALUE_SHIFT 0x5


#define BCT4480_RES_PIN_SETTING	0x13
#define BCT4480_RES_VALUE		0x14
#define BCT4480_RES_THRESHOLD	0x15
#define BCT4480_RES_TIME	0x16
#define BCT4480_RES_INT_FLAG    0x18
#define BCT4480_RES_INT_MASK    0x19

#define AS6480_SWITCH_USB             0
#define AS6480_MODE_CTRL              1
#define AS6480_SWITCH_HEADSET         2
#define AS6480_SWITCH_GND_MIC_SWAP    3
#define AS6480_SWITCH_OFF             7




#if defined(ZCFG_WATERCHECK_INT)
int water_flag = 0;
extern int jx_msg_notifier_call_chain(unsigned long val, void *v);
#endif

static bool jack_plugin = false;
static bool usb_plugin = false;
static bool otg_plugin = false;

struct bct4480_info {
	struct i2c_client   *client;
	struct regmap *regmap;
	struct extcon_dev *edev;
	struct extcon_dev *vbus_edev;
	struct extcon_dev *otg_edev;
	struct device *dev;
	struct notifier_block nb;
	struct notifier_block vbus_nb;
	struct notifier_block otg_nb;
	bool usbc_mode;
	struct work_struct usbc_analog_work;
#if defined(ZCFG_WATERCHECK_INT)
	struct work_struct int_read_work;
#endif
	struct blocking_notifier_head bct4480_notifier;
	struct mutex notification_lock;
	int switch_control;
	int chip_id;

#if defined(ZCFG_WATERCHECK_INT)
int32_t irq_gpio;
int32_t to_irq;
#endif
};

struct bct4480_info *g_bct4480_data = NULL;

struct bct4480_reg_val {
	u16 reg;
	u8 val;
};

static const struct regmap_config bct4480_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = BCT4480_RESET,
};

static const struct bct4480_reg_val fsa_reg_i2c_defaults[] = {
	{BCT4480_RESET, 0x01},
	{BCT4480_SLOW_L, 0x00},
	{BCT4480_SLOW_R, 0x00},
	{BCT4480_SLOW_MIC, 0x00},
	{BCT4480_SLOW_SENSE, 0x00},
	{BCT4480_SLOW_GND, 0x00},
	{BCT4480_DELAY_L_R, 0x00},
	{BCT4480_DELAY_L_MIC, 0x00},
	{BCT4480_DELAY_L_SENSE, 0x00},
	{BCT4480_DELAY_L_AGND, 0x09},
	{BCT4480_SWITCH_SETTINGS, 0x98},
	{BCT4480_SWITCH_CONTROL, 0x18},
};

#if defined(ZCFG_WATERCHECK_INT)
static const struct bct4480_reg_val bct4480_reg_check_reg[] = {
	{BCT4480_RES_PIN_SETTING, 0x01},  //reg 0x13
	// {BCT4480_RES_THRESHOLD, 0x16},	 //reg 0x15
	{BCT4480_RES_TIME, 0x03},	//reg 0x16
};


static int bct4481_read_reg(u8 addr, u8 *pdata)
{
	int ret = 0;
	u8 buf[2] = {addr, 0};

	struct i2c_msg msgs[] = {
		{
			.addr	= g_bct4480_data->client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= buf,
		},
		{
			.addr	=  g_bct4480_data->client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= buf+1,
		},
	};

	if (i2c_transfer( g_bct4480_data->client->adapter, msgs, 2) != 2) {
		ret = -EIO;
		pr_err("msg %s i2c read error: %d\n", __func__, ret);
	}

	*pdata = buf[1];
	return ret;
}



static int write_register(u8 reg, u8 value ,u8 mask, u8 shift)
{

    u8 data[2] = {0};
	u8 temp;
    int ret;

	data[0] = reg;

	bct4481_read_reg(reg,&temp);

	printk("bct4480 write_register 1 temp = 0x%x\n",temp);
	temp &= ~mask;

	temp |= (value << shift);

	data[1] = temp;

	printk("bct4480 write_register 2 temp = 0x%x\n",data[1]);

    ret = i2c_master_send( g_bct4480_data->client, data, sizeof(data));
    if (ret < 0)
        return ret;

    if (ret != sizeof(data))
        return -EIO;

    return 0;
}
#endif



static void bct4480_usbc_update_settings(struct bct4480_info *bct4480_data,
		u32 switch_control, u32 switch_enable)
{
	u32 prev_control, prev_enable;

	if (!bct4480_data->regmap) {
		dev_err(bct4480_data->dev, "%s: regmap invalid\n", __func__);
		return;
	}

	regmap_read(bct4480_data->regmap, BCT4480_SWITCH_CONTROL, &prev_control);
	regmap_read(bct4480_data->regmap, BCT4480_SWITCH_SETTINGS, &prev_enable);

	if (prev_control == switch_control && prev_enable == switch_enable) {
		dev_dbg(bct4480_data->dev, "%s: settings unchanged\n", __func__);
		return;
	}

	regmap_write(bct4480_data->regmap, BCT4480_SWITCH_SETTINGS, 0x80);
	regmap_write(bct4480_data->regmap, BCT4480_SWITCH_CONTROL, switch_control);
	/* BCT4480 chip hardware requirement */
	usleep_range(50, 55);
	regmap_write(bct4480_data->regmap, BCT4480_SWITCH_SETTINGS, switch_enable);
}

static int bct4480_usbc_event_changed_run(struct bct4480_info *bct4480_data,
				      unsigned long evt, void *ptr)
{
	struct device *dev;

	dev = bct4480_data->dev;
	dev_dbg(dev, "%s: USB change event received, usbc mode %d\n",
			__func__, bct4480_data->usbc_mode);

	bct4480_data->usbc_mode = !!evt;
	pm_stay_awake(bct4480_data->dev);
	queue_work(system_freezable_wq, &bct4480_data->usbc_analog_work);

	return 0;
}

static int bct4480_usbc_event_changed(struct notifier_block *nb_ptr,
				      unsigned long evt, void *ptr)
{
	struct bct4480_info *bct4480_data =
			container_of(nb_ptr, struct bct4480_info, nb);
	struct device *dev;
	pr_info("%s: evt = %d\n", __func__,evt);
#if defined(ZCFG_WATERCHECK_INT)
	if(evt){
		jack_plugin = true;
		write_register( BCT4480_RES_DETECT_EN, 0x0 ,BCT4480_RES_DETECT_EN_MASK << BCT4480_RES_DETECT_EN_SHIFT,BCT4480_RES_DETECT_EN_SHIFT);
		jx_msg_notifier_call_chain(MSG_WATER_CHECK_HIGH_RES,NULL);
	}else{
		jack_plugin = false;
		write_register( BCT4480_RES_DETECT_EN, 0x1 ,BCT4480_RES_DETECT_EN_MASK << BCT4480_RES_DETECT_EN_SHIFT,BCT4480_RES_DETECT_EN_SHIFT);
	}
#endif
	

	if (!bct4480_data)
		return -EINVAL;
	dev = bct4480_data->dev;
	if (!dev)
		return -EINVAL;

    pr_info("%s: get into the function\n", __func__);
	return bct4480_usbc_event_changed_run(bct4480_data, evt, ptr);
}

static int bct4480_vbus_event_changed(struct notifier_block *nb_ptr,
				      unsigned long evt, void *ptr)
{

pr_info("%s: evt = %d\n", __func__,evt);

#if defined(ZCFG_WATERCHECK_INT)
	if(evt){
		usb_plugin = true;
		write_register( BCT4480_RES_DETECT_EN, 0x0 ,BCT4480_RES_DETECT_EN_MASK << BCT4480_RES_DETECT_EN_SHIFT,BCT4480_RES_DETECT_EN_SHIFT);
		jx_msg_notifier_call_chain(MSG_WATER_CHECK_HIGH_RES,NULL);
	}else{
		usb_plugin = false;
		write_register( BCT4480_RES_DETECT_EN, 0x1 ,BCT4480_RES_DETECT_EN_MASK << BCT4480_RES_DETECT_EN_SHIFT,BCT4480_RES_DETECT_EN_SHIFT);
	}
#endif

	return 0;
}



static int bct4480_otg_event_changed(struct notifier_block *nb_ptr,
				      unsigned long evt, void *ptr)
{

pr_info("%s: evt = %d\n", __func__,evt);

#if defined(ZCFG_WATERCHECK_INT)
	if(evt){
		otg_plugin = true;
		write_register( BCT4480_RES_DETECT_EN, 0x0 ,BCT4480_RES_DETECT_EN_MASK << BCT4480_RES_DETECT_EN_SHIFT,BCT4480_RES_DETECT_EN_SHIFT);
		jx_msg_notifier_call_chain(MSG_WATER_CHECK_HIGH_RES,NULL);
	}else{
		otg_plugin = false;
		write_register( BCT4480_RES_DETECT_EN, 0x1 ,BCT4480_RES_DETECT_EN_MASK << BCT4480_RES_DETECT_EN_SHIFT,BCT4480_RES_DETECT_EN_SHIFT);
	}
#endif

	return 0;
}

static int bct4480_usbc_analog_setup_switches_ucsi(
						struct bct4480_info *bct4480_data)
{
	int rc = 0;
	struct device *dev;

	if (!bct4480_data)
		return -EINVAL;
	dev = bct4480_data->dev;
	if (!dev)
		return -EINVAL;

	mutex_lock(&bct4480_data->notification_lock);

	if (bct4480_data->usbc_mode) {
		pr_info("%s: set the audio attach\n", __func__);
		/* activate switches */
		if((bct4480_data->chip_id == BCT4480_VENDOR_NUM) || (bct4480_data->chip_id == HL1280_VENDOR_NUM))
		{
			bct4480_usbc_update_settings(bct4480_data, 0x00, 0x9F);
		}

	} else {
		pr_info("%s: set the audio dettach\n", __func__);


		/* deactivate switches */
		if((bct4480_data->chip_id == BCT4480_VENDOR_NUM) || (bct4480_data->chip_id == HL1280_VENDOR_NUM))
		{
			bct4480_usbc_update_settings(bct4480_data, 0x18, 0x98);
		}

	}
	mutex_unlock(&bct4480_data->notification_lock);

	return rc;
}

static int bct4480_usbc_analog_setup_switches(struct bct4480_info *bct4480_data)
{
	return bct4480_usbc_analog_setup_switches_ucsi(bct4480_data);
}

static int bct4480_validate_display_port_settings(struct bct4480_info *bct4480_data)
{
	u32 switch_status = 0;

	regmap_read(bct4480_data->regmap, BCT4480_SWITCH_STATUS1, &switch_status);

	if ((switch_status != 0x23) && (switch_status != 0x1C)) {
		pr_err("AUX SBU1/2 switch status is invalid = %u\n",
				switch_status);
		return -EIO;
	}

	return 0;
}

int bct4480_switch_event_enable(enum fsa_function event)
{
	// int old_mode, new_mode;

	if(g_bct4480_data == NULL) {
		pr_info("%s: g_bct4480_data is null\n", __func__);
		return -EINVAL;
	}

	switch (event) {
	case FSA_MIC_GND_SWAP:
		// if(g_bct4480_data->chip_id == BCT4480_VENDOR_NUM) {
		if((g_bct4480_data->chip_id == BCT4480_VENDOR_NUM) || (g_bct4480_data->chip_id == HL1280_VENDOR_NUM)){
			regmap_read(g_bct4480_data->regmap,
                                    BCT4480_SWITCH_CONTROL, &g_bct4480_data->switch_control);
			if ((g_bct4480_data->switch_control & 0x07) == 0x07)
				g_bct4480_data->switch_control = 0x0;
			else
				g_bct4480_data->switch_control = 0x7;
			bct4480_usbc_update_settings(g_bct4480_data, g_bct4480_data->switch_control, 0x9F);
		} 
		break;
	case FSA_USBC_ORIENTATION_CC1:
		bct4480_usbc_update_settings(g_bct4480_data, 0x18, 0xF8);
		return bct4480_validate_display_port_settings(g_bct4480_data);
	case FSA_USBC_ORIENTATION_CC2:
		bct4480_usbc_update_settings(g_bct4480_data, 0x78, 0xF8);
		return bct4480_validate_display_port_settings(g_bct4480_data);
	case FSA_USBC_DISPLAYPORT_DISCONNECTED:
		bct4480_usbc_update_settings(g_bct4480_data, 0x18, 0x98);
		break;
	default:
		break;
	}

	return 0;
}

int typec_i2c_switch_event_enable(enum fsa_function event, bool need_delay) {
	if (!need_delay) {
		bct4480_switch_event_enable(event);
	} else {
		/* AS3UA6485CR chip headmicbisa power on has 130ms delay, hw why? */
		if (g_bct4480_data->chip_id == BCT4480_CHIP_ID)
			msleep(130);
	}
	return 0;
}
EXPORT_SYMBOL(typec_i2c_switch_event_enable);

void typec_i2c_switch_depop(void) {
	// int old_mode, new_mode;
	pr_info("%s: get into the function\n", __func__);
	// regmap_read(g_bct4480_data->regmap,AS6480_MODE_CTRL,&old_mode);
	// new_mode = AS6480_SWITCH_HEADSET;
	// regmap_write(g_bct4480_data->regmap,AS6480_MODE_CTRL, AS6480_SWITCH_OFF);
	// regmap_write(g_bct4480_data->regmap,AS6480_MODE_CTRL, new_mode);
}
EXPORT_SYMBOL(typec_i2c_switch_depop);

static void bct4480_usbc_analog_work_fn(struct work_struct *work)
{
	struct bct4480_info *bct4480_data =
		container_of(work, struct bct4480_info, usbc_analog_work);

	if (!bct4480_data) {
		pr_err("%s: fsa container invalid\n", __func__);
		return;
	}
	pr_info("%s: get into the function\n", __func__);
	bct4480_usbc_analog_setup_switches(bct4480_data);
	pm_relax(bct4480_data->dev);
}

#if defined(ZCFG_WATERCHECK_INT)
static void int_read_work_fun(struct work_struct *work)
{
	u8 reg18,reg14,reg15;
	pr_err("%s enter\n", __func__);

	if(jack_plugin || usb_plugin || otg_plugin){
		pr_err("%s usb or headset plug in\n", __func__);
		bct4481_read_reg(BCT4480_RES_INT_FLAG,&reg18);
	}else{

		bct4481_read_reg(BCT4480_RES_INT_FLAG,&reg18);
		if(reg18 & 0x1){
			pr_err("%s enter 1\n", __func__);
			if(reg18 & 0x2){
				jx_msg_notifier_call_chain(MSG_WATER_CHECK_LOW_RES,NULL);
				bct4481_read_reg(BCT4480_RES_VALUE,&reg14);
				bct4481_read_reg(BCT4480_RES_THRESHOLD,&reg15);
				if(reg14 != 0){
					water_flag = 1;
				}
				pr_err("%s enter 2 reg14 = 0x%x , reg15 = 0x%x\n", __func__,reg14,reg15);
			}
		}else{
			pr_err("%s enter 3\n", __func__);
		} 
	}
}
#endif

static void bct4480_update_reg_defaults(struct regmap *regmap)
{
	u8 i;

	for (i = 0; i < ARRAY_SIZE(fsa_reg_i2c_defaults); i++)
		regmap_write(regmap, fsa_reg_i2c_defaults[i].reg,
				   fsa_reg_i2c_defaults[i].val);
}

#if defined(ZCFG_WATERCHECK_INT)
static void bct4480_cfg_reg_check(struct regmap *regmap)
{
	u8 i;
	// u8 reg = 0,buf = 0;
	for (i = 0; i < ARRAY_SIZE(bct4480_reg_check_reg); i++)
		regmap_write(regmap, bct4480_reg_check_reg[i].reg,
				   bct4480_reg_check_reg[i].val);

	// write_register( BCT4480_RES_DETECT_EN, 0x1 ,BCT4480_RES_DETECT_EN_MASK << BCT4480_RES_DETECT_EN_SHIFT,BCT4480_RES_DETECT_EN_SHIFT);
	// reg = 0x12;
	// bct4481_read_reg(reg,&buf);
	// printk("bct4480_cfg_reg_check 1 reg12 = 0x%x\n",buf);

	write_register( BCT4480_RES_DETECT_EN, 0x1 ,BCT4480_RES_DETECT_VALUE_MASK << BCT4480_RES_DETECT_VALUE_SHIFT,BCT4480_RES_DETECT_VALUE_SHIFT);
	// bct4481_read_reg(reg,&buf);
	// printk("bct4480_cfg_reg_check 2 reg12 = 0x%x\n",buf);
}


static ssize_t bct4880_water_flag_get(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	return sprintf(buf, "%d\n", water_flag);
}

static ssize_t bct4880_water_flag_set(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	int ret;
	printk("bct4880_water_flag_set\n");
	ret = kstrtoint(buf, 0, &water_flag);
	if (ret == 0) {
        pr_info("Stored value: %d\n", water_flag);
		if(water_flag == 0){
			jx_msg_notifier_call_chain(MSG_WATER_CHECK_HIGH_RES,NULL);
			if(jack_plugin || usb_plugin || otg_plugin)
				write_register( BCT4480_RES_DETECT_EN, 0x0 ,BCT4480_RES_DETECT_EN_MASK << BCT4480_RES_DETECT_EN_SHIFT,BCT4480_RES_DETECT_EN_SHIFT);
			else
				write_register( BCT4480_RES_DETECT_EN, 0x1 ,BCT4480_RES_DETECT_EN_MASK << BCT4480_RES_DETECT_EN_SHIFT,BCT4480_RES_DETECT_EN_SHIFT);
		}
    } else {
        pr_err("Failed to store value\n");
    }
	return count;
}

static DEVICE_ATTR(water_flag, 0644, bct4880_water_flag_get, bct4880_water_flag_set);

static struct attribute *bct4880_attributes[] = {
	&dev_attr_water_flag.attr,
	NULL
};

static struct attribute_group bct4880_attribute_group = {
	.attrs = bct4880_attributes
};
#endif


#if defined(ZCFG_WATERCHECK_INT)
static irqreturn_t bct4480_irq_handler(int32_t irq, void *data)
{	
	if((!usb_plugin) || (!jack_plugin) || (!otg_plugin))
		queue_work(system_freezable_wq, &g_bct4480_data->int_read_work);

	return IRQ_HANDLED;
}
#endif
static int bct4480_probe(struct i2c_client *i2c,
			 const struct i2c_device_id *id)
{
	struct bct4480_info *bct4480_data;
	int rc = 0;
	int chip_id=0;
#if defined(ZCFG_WATERCHECK_INT)	
	struct device_node *np = i2c->dev.of_node;
	int32_t irq_flags = 0;
#endif
	pr_info("%s: get the function\n", __func__);
	bct4480_data = devm_kzalloc(&i2c->dev, sizeof(*bct4480_data),
				GFP_KERNEL);
	if (!bct4480_data)
		return -ENOMEM;

	memset(bct4480_data, 0, sizeof(struct bct4480_info));
	bct4480_data->dev = &i2c->dev;

	bct4480_data->regmap = devm_regmap_init_i2c(i2c, &bct4480_regmap_config);
	if (IS_ERR_OR_NULL(bct4480_data->regmap)) {
		dev_err(bct4480_data->dev, "%s: Failed to initialize regmap: %d\n",
			__func__, rc);
		if (!bct4480_data->regmap) {
			rc = -EINVAL;
			goto err_data;
		}
		rc = PTR_ERR(bct4480_data->regmap);
		goto err_data;
	}
	regmap_read(bct4480_data->regmap, BCT4480_CHIP_ID ,&chip_id);
	pr_info("%s: chip_id is %d\n",__func__, chip_id);
	bct4480_data->chip_id = chip_id;
	// if(chip_id == BCT4480_VENDOR_NUM) {
	if((chip_id == BCT4480_VENDOR_NUM) || (chip_id == HL1280_VENDOR_NUM)) {
		pr_info("%s: chip is bct4480\n",__func__);
	    bct4480_update_reg_defaults(bct4480_data->regmap);
		bct4480_usbc_update_settings(bct4480_data, 0x18, 0x98);
	}
	bct4480_data->edev = extcon_get_edev_by_phandle(bct4480_data->dev, 0);
	if (IS_ERR(bct4480_data->edev)) {
		dev_err(bct4480_data->dev,
			"typec headset failed to find gpio extcon device, ret %d\n", PTR_ERR(bct4480_data->edev));
		return PTR_ERR(bct4480_data->edev);
	}
	pr_info("%s: get the extcon handle\n", __func__);
	bct4480_data->nb.notifier_call = bct4480_usbc_event_changed;
	rc = extcon_register_notifier(bct4480_data->edev,
						EXTCON_JACK_HEADPHONE, &bct4480_data->nb);
	if (rc) {
		dev_err(bct4480_data->dev,
		  "%s: ucsi glink notifier registration failed: %d\n",
		  __func__, rc);
		goto err_data;
	}

	bct4480_data->vbus_edev = extcon_get_edev_by_phandle(bct4480_data->dev, 1);
	if (IS_ERR(bct4480_data->vbus_edev)) {
		dev_err(bct4480_data->dev,
			"typec headset failed to find gpio extcon device, ret %d\n", PTR_ERR(bct4480_data->vbus_edev));
		return PTR_ERR(bct4480_data->vbus_edev);
	}
	pr_info("%s: get the extcon handle\n", __func__);
	bct4480_data->vbus_nb.notifier_call = bct4480_vbus_event_changed;
	rc = extcon_register_notifier(bct4480_data->vbus_edev,
						EXTCON_USB, &bct4480_data->vbus_nb);
	if (rc) {
		dev_err(bct4480_data->dev,
		  "%s: ucsi glink notifier registration failed: %d\n",
		  __func__, rc);
		goto err_data;
	}

	bct4480_data->otg_edev = extcon_get_edev_by_phandle(bct4480_data->dev, 1);
	if (IS_ERR(bct4480_data->otg_edev)) {
		dev_err(bct4480_data->dev,
			"typec headset failed to find gpio extcon device, ret %d\n", PTR_ERR(bct4480_data->otg_edev));
		return PTR_ERR(bct4480_data->otg_edev);
	}
	pr_info("%s: get the extcon handle\n", __func__);
	bct4480_data->otg_nb.notifier_call = bct4480_otg_event_changed;
	rc = extcon_register_notifier(bct4480_data->otg_edev,
						EXTCON_USB_HOST, &bct4480_data->otg_nb);
	if (rc) {
		dev_err(bct4480_data->dev,
		  "%s: ucsi glink notifier registration failed: %d\n",
		  __func__, rc);
		goto err_data;
	}

	mutex_init(&bct4480_data->notification_lock);
	i2c_set_clientdata(i2c, bct4480_data);
	g_bct4480_data = bct4480_data;
	
	bct4480_data->client = i2c;
#if defined(ZCFG_WATERCHECK_INT)
	/* attribute */
	rc = sysfs_create_group(&i2c->dev.kobj, &bct4880_attribute_group);
	if (rc < 0) {
		dev_info(&i2c->dev, "%s error creating sysfs attr files\n",
			 __func__);
	}

	rc = sysfs_create_link(NULL, &i2c->dev.kobj, "watercheck");
	if (rc < 0) {
		dev_info(&i2c->dev, "%s Failed to create link!\n",
			 __func__);
		sysfs_remove_link(NULL, "radar");
	}
#endif




	INIT_WORK(&bct4480_data->usbc_analog_work,
					bct4480_usbc_analog_work_fn);

	if (extcon_get_state(bct4480_data->vbus_edev, EXTCON_USB)) {
		usb_plugin = true;
	}else{
		usb_plugin = false;
	}

	if (extcon_get_state(bct4480_data->otg_edev, EXTCON_USB_HOST)) {
		otg_plugin = true;
	}else{
		otg_plugin = false;
	}

	if (extcon_get_state(bct4480_data->edev, EXTCON_JACK_HEADPHONE)) {
		bct4480_data->usbc_mode = true;
		queue_work(system_freezable_wq, &bct4480_data->usbc_analog_work);
		jack_plugin = true;
	}else{
		jack_plugin = false;
	}


#if defined(ZCFG_WATERCHECK_INT)
	INIT_WORK(&bct4480_data->int_read_work,int_read_work_fun);

	bct4480_data->irq_gpio = of_get_named_gpio(np, "int-gpio", 0);
	if (gpio_is_valid(bct4480_data->irq_gpio)) {
		rc = devm_gpio_request_one(&i2c->dev,
							bct4480_data->irq_gpio,
							GPIOF_DIR_IN,
							"bct4480_irq_gpio");
		if (rc) {
			pr_err("%s: request irq gpio failed, rc = %d\n",
							__func__, rc);
			rc = -1;
		} else {
			irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
			bct4480_data->to_irq = gpio_to_irq(bct4480_data->irq_gpio);
			rc = devm_request_threaded_irq(&i2c->dev,
						bct4480_data->to_irq,
						NULL, bct4480_irq_handler, irq_flags,
						"bct4480_irq", bct4480_data);

			if (rc != 0) {
				pr_err("%s: failed to request IRQ %d: %d\n",__func__,bct4480_data->to_irq,rc);
				rc = -1;
			} else {
				pr_info("%s: IRQ request successfully!\n",__func__);
				rc = 0;
			}
		}
	} else {
		pr_err("%s: irq gpio invalid!\n", __func__);
		return -1;
	}

	rc = gpio_get_value(bct4480_data->irq_gpio);
	pr_err("%s: irq gpio = %d\n", __func__,rc);


	bct4480_cfg_reg_check(bct4480_data->regmap);
	msleep(80);
	if(jack_plugin || usb_plugin || otg_plugin)
		write_register( BCT4480_RES_DETECT_EN, 0x0 ,BCT4480_RES_DETECT_EN_MASK << BCT4480_RES_DETECT_EN_SHIFT,BCT4480_RES_DETECT_EN_SHIFT);
	else
		write_register( BCT4480_RES_DETECT_EN, 0x1 ,BCT4480_RES_DETECT_EN_MASK << BCT4480_RES_DETECT_EN_SHIFT,BCT4480_RES_DETECT_EN_SHIFT);
#endif
	bct4480_data->bct4480_notifier.rwsem =
		(struct rw_semaphore)__RWSEM_INITIALIZER
		((bct4480_data->bct4480_notifier).rwsem);
	bct4480_data->bct4480_notifier.head = NULL;

	return 0;

err_data:
	devm_kfree(&i2c->dev, bct4480_data);
	return rc;
}

static int bct4480_remove(struct i2c_client *i2c)
{
	struct bct4480_info *bct4480_data =
			(struct bct4480_info *)i2c_get_clientdata(i2c);
	pr_info("%s enter\n",__func__);
	if (!bct4480_data)
		return -EINVAL;
#if defined(ZCFG_WATERCHECK_INT)
	write_register( BCT4480_RES_DETECT_EN, 0x0 ,BCT4480_RES_DETECT_EN_MASK << BCT4480_RES_DETECT_EN_SHIFT,BCT4480_RES_DETECT_EN_SHIFT);
#endif
	extcon_unregister_notifier(bct4480_data->edev,
			EXTCON_JACK_HEADPHONE, &bct4480_data->nb);
	bct4480_usbc_update_settings(bct4480_data, 0x18, 0x98);
	cancel_work_sync(&bct4480_data->usbc_analog_work);
	pm_relax(bct4480_data->dev);
	mutex_destroy(&bct4480_data->notification_lock);
	dev_set_drvdata(&i2c->dev, NULL);

	return 0;
}

static void bct4480_shutdown(struct i2c_client *i2c)
{
	struct bct4480_info *bct4480_data = (struct bct4480_info *)i2c_get_clientdata(i2c);
	pr_info("%s enter 1\n",__func__);
	if(bct4480_data == NULL) 
		return ;

	regmap_write(bct4480_data->regmap, 0x1E,
				   0X01);
	pr_info("%s enter 2\n",__func__);
}

static const struct of_device_id bct4480_i2c_dt_match[] = {
	{
		.compatible = "jx,bct4480-i2c",
	},
	{}
};

static struct i2c_driver bct4480_i2c_driver = {
	.driver = {
		.name = BCT4480_I2C_NAME,
		.of_match_table = bct4480_i2c_dt_match,
	},
	.probe = bct4480_probe,
	.remove = bct4480_remove,
	.shutdown = bct4480_shutdown,
};

static int __init bct4480_init(void)
{
	int rc;
    pr_info("%s: get the function\n", __func__);
	rc = i2c_add_driver(&bct4480_i2c_driver);
	if (rc)
		pr_err("bct4480: Failed to register I2C driver: %d\n", rc);

	return rc;
}
module_init(bct4480_init);

static void __exit bct4480_exit(void)
{
	i2c_del_driver(&bct4480_i2c_driver);
}
module_exit(bct4480_exit);

MODULE_DESCRIPTION("BCT4480 I2C driver");
MODULE_LICENSE("GPL v2");
