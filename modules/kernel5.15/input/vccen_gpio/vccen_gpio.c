/*
 * Copyright (C) 2015-2016 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/errno.h>

#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioctl.h>

#include "soc/sprd/board.h"

#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#endif


//#define VCCEN_GPIOS  GPIO_VCC_EN+192
int gpio_num=0;
static int gpio_vcc_en_enable(void)
{
	printk("set gpio %d high\n",gpio_num);
	gpio_direction_output(gpio_num, 1);
	gpio_set_value(gpio_num, 1);

	return 0;
}

static int gpio_vcc_en_disable(void)
{
	printk("set gpio %d low\n",gpio_num);
	gpio_direction_output(gpio_num, 0);
	gpio_set_value(gpio_num, 0);
	return 0;
}

static ssize_t vcc_en_gpio_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n",
		gpio_get_value(gpio_num) ? "1" : "0");
}

static ssize_t vcc_en_gpio_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	printk("set gpio value %d\n",buf[0]);
	if (buf[0] == 49)
	{
		gpio_vcc_en_enable();
	}
	else if (buf[0] == 48)
	{
		gpio_vcc_en_disable();
	}

	return count;
}

static DEVICE_ATTR(vcc_en_gpio, S_IRUGO | S_IWUSR, vcc_en_gpio_show,
				vcc_en_gpio_store);


static struct attribute *vccen_dev_vcc_en_gpio_atts[] = {
		&dev_attr_vcc_en_gpio.attr,
		NULL
};

static const struct attribute_group vccen_dev_vcc_en_gpio_atts_group = {
    .attrs = vccen_dev_vcc_en_gpio_atts,
};

static const struct attribute_group *vccen_dev_attr_groups[] = {
    &vccen_dev_vcc_en_gpio_atts_group,
    NULL
};

int vccen_sysfs_add_device(struct device *dev) {
	int ret, i;
	for(i = 0; vccen_dev_attr_groups[i]; i++) {
		ret = sysfs_create_group(&dev->kobj, vccen_dev_attr_groups[i]);
        if (ret) {
            while (--i >= 0) {
                sysfs_remove_group(&dev->kobj, vccen_dev_attr_groups[i]);
            }
            break;
        }
	}
	if (ret) {
        return ret;
    }
	return 0;
}

int vccen_sysfs_remove_device(struct device *dev) {
	int i;
	sysfs_remove_link(NULL, "vccgpio");
    for (i = 0; vccen_dev_attr_groups[i]; i++) {
        sysfs_remove_group(&dev->kobj, vccen_dev_attr_groups[i]);
    }
	return 0;
}

static int sprd_vccen_gpio_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(pdev))
	{
		printk("IS_ERR_OR_NULL(pdev)\n");
		return -EINVAL;
	}
	printk("sprd_vccen_gpio_probe start\n");

	gpio_num = of_get_named_gpio(pdev->dev.of_node, "vcc_en-gpios", 0);
	if (!gpio_is_valid(gpio_num)) {
		printk("sprd_vccen_gpio_probe gpio = %d\n",gpio_num);
		return -1;
	}
	ret = devm_gpio_request(&pdev->dev, gpio_num, "vcc_en-gpios");
	if (ret)
		goto exit;
	ret = sysfs_create_link(NULL,&pdev->dev.kobj, "vccgpio");
	if (ret < 0) 
	{
		printk("Failed to create link!\n");
		goto sysfs_exit;
	}
	vccen_sysfs_add_device(&pdev->dev);
	printk("sprd_vccen_gpio_probe Success!\n");
	return 0;
sysfs_exit:
	gpio_free(gpio_num);
exit:
	return ret;
}

static int sprd_vccen_gpio_remove(struct platform_device *pdev)
{
	vccen_sysfs_remove_device(&pdev->dev);
	return 0;
}

static const struct of_device_id sprd_vccen_gpio_of_match[] = {
	{ .compatible = "sprd,vccen_gpio", },
	{},
};

static struct platform_driver sprd_vccen_gpio_driver = {
	.probe = sprd_vccen_gpio_probe,
	.remove = sprd_vccen_gpio_remove,
	.driver = {
		.name = "vccen-gpio",
		.of_match_table = of_match_ptr(sprd_vccen_gpio_of_match),
	},
};


static int sprd_vccen_gpio_register_driver(void)
{
	int ret = 0;

	ret =platform_driver_register(&sprd_vccen_gpio_driver);
	return ret;
}

static void sprd_vccen_gpio_unregister_driver(void)
{

	platform_driver_unregister(&sprd_vccen_gpio_driver);

}

static int __init  sprd_vccen_gpio_driver_init(void)
{
	int ret = 0;

	ret = sprd_vccen_gpio_register_driver();
	return ret;
}

static void __exit sprd_vccen_gpio_driver_deinit(void)
{
	sprd_vccen_gpio_unregister_driver();
}

module_init(sprd_vccen_gpio_driver_init);
module_exit(sprd_vccen_gpio_driver_deinit);
MODULE_DESCRIPTION("Sprd gpio Vcc en Driver");
MODULE_LICENSE("GPL");

