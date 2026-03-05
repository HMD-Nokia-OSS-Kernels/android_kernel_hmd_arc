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


int gpio_num=0;
static int hac_enable(void)
{
	printk("set gpio %d high\n",gpio_num);
	gpio_direction_output(gpio_num, 1);
	gpio_set_value(gpio_num, 1);

	return 0;
}

static int hac_disable(void)
{
	printk("set gpio %d low\n",gpio_num);
	gpio_direction_output(gpio_num, 0);
	gpio_set_value(gpio_num, 0);
	return 0;
}

static ssize_t hac_en_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n",
		gpio_get_value(gpio_num) ? "1" : "0");
}

static ssize_t hac_en_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	printk("set gpio value %d\n",buf[0]);
	if (buf[0] == 49)
	{
		hac_enable();
	}
	else if (buf[0] == 48)
	{
		hac_disable();
	}

	return count;
}

static DEVICE_ATTR(hac_en, S_IRUGO | S_IWUSR, hac_en_show,
				hac_en_store);


static struct attribute *dev_hac_en_atts[] = {
		&dev_attr_hac_en.attr,
		NULL
};

static const struct attribute_group dev_hac_en_atts_group = {
    .attrs = dev_hac_en_atts,
};

static const struct attribute_group *dev_attr_groups[] = {
    &dev_hac_en_atts_group,
    NULL
};

int hac_sysfs_add_device(struct device *dev) {
	int ret, i;
	for(i = 0; dev_attr_groups[i]; i++) {
		ret = sysfs_create_group(&dev->kobj, dev_attr_groups[i]);
        if (ret) {
            while (--i >= 0) {
                sysfs_remove_group(&dev->kobj, dev_attr_groups[i]);
            }
            break;
        }
	}
	if (ret) {
        return ret;
    }
	return 0;
}

int hac_sysfs_remove_device(struct device *dev) {
	int i;
	sysfs_remove_link(NULL, "hac");
    for (i = 0; dev_attr_groups[i]; i++) {
        sysfs_remove_group(&dev->kobj, dev_attr_groups[i]);
    }
	return 0;
}

static int sprd_hac_gpio_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(pdev))
	{
		printk("IS_ERR_OR_NULL(pdev)\n");
		return -EINVAL;
	}
	printk("sprd_hac_gpio_probe start\n");

	gpio_num = of_get_named_gpio(pdev->dev.of_node, "hacen-gpios", 0);
	if (!gpio_is_valid(gpio_num)) {
		printk("sprd_hac_gpio_probe gpio = %d\n",gpio_num);
		return -1;
	}
	ret = devm_gpio_request(&pdev->dev, gpio_num, "hacen_gpio");
	if (ret)
		goto exit;
	ret = sysfs_create_link(NULL,&pdev->dev.kobj, "hac");
	if (ret < 0) 
	{
		printk("Failed to create link!\n");
		goto sysfs_exit;
	}
	hac_sysfs_add_device(&pdev->dev);
	printk("sprd_hac_gpio_probe Success!\n");
	return 0;
sysfs_exit:
	gpio_free(gpio_num);
exit:
	return ret;
}

static int sprd_hac_gpio_remove(struct platform_device *pdev)
{
	hac_sysfs_remove_device(&pdev->dev);
	return 0;
}

static const struct of_device_id sprd_hac_gpio_of_match[] = {
	{ .compatible = "sprd,hac_gpio", },
	{},
};

static struct platform_driver sprd_hac_gpio_driver = {
	.probe = sprd_hac_gpio_probe,
	.remove = sprd_hac_gpio_remove,
	.driver = {
		.name = "hac-gpio",
		.of_match_table = of_match_ptr(sprd_hac_gpio_of_match),
	},
};


static int sprd_hac_gpio_register_driver(void)
{
	int ret = 0;

	ret =platform_driver_register(&sprd_hac_gpio_driver);
	return ret;
}

static void sprd_hac_gpio_unregister_driver(void)
{

	platform_driver_unregister(&sprd_hac_gpio_driver);

}

static int __init  sprd_hac_gpio_driver_init(void)
{
	int ret = 0;

	ret = sprd_hac_gpio_register_driver();
	return ret;
}

static void __exit sprd_hac_gpio_driver_deinit(void)
{
	sprd_hac_gpio_unregister_driver();
}

module_init(sprd_hac_gpio_driver_init);
module_exit(sprd_hac_gpio_driver_deinit);
MODULE_DESCRIPTION("Sprd gpio hac Driver");
MODULE_LICENSE("GPL");

