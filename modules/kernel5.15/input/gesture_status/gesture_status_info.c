/*-----------------------------------------------------------------------------*/
// File Name : gesture_status.c
// for kernel debug info
// Author : zengjianxiang
// Date : 2023-9-26
/*-----------------------------------------------------------------------------*/
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
u8 gesture_value=0;
u8 key_value=0;
u8 get_gesture_status_info(void)
{
	
	return gesture_value;
}

u8 set_gesture_status_info(u8 value)
{
	gesture_value=value;
	return 1;
}

EXPORT_SYMBOL(get_gesture_status_info);
EXPORT_SYMBOL(set_gesture_status_info);

static ssize_t gesture_status_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n",gesture_value ? "true" : "false");
}

static ssize_t gesture_status_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	printk("set gpio value %d\n",buf[0]);
	if (buf[0] == 49)
	{
		gesture_value=1;
	}
	else if (buf[0] == 48)
	{
		gesture_value=0;
	}
	return count;
}


static ssize_t key_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
		return sprintf(buf, "%s\n",key_value ? "true" : "false");
}

static ssize_t key_value_store(struct device *dev, struct device_attribute *attr, const char *buf,size_t count)
{
	return count;
}
static DEVICE_ATTR_RW(key_value);


static struct attribute *key_value_atts[] = {
		&dev_attr_key_value.attr,
		NULL
	};

static const struct attribute_group key_value_atts_group = {
    .attrs = key_value_atts,
};




static DEVICE_ATTR(gesture_status, S_IRUGO | S_IWUSR, gesture_status_show,
				gesture_status_store);


static struct attribute *gesture_status_atts[] = {
		&dev_attr_gesture_status.attr,
		NULL
};

static const struct attribute_group gesture_status_atts_group = {
    .attrs = gesture_status_atts,
};

static const struct attribute_group *gesture_status_attr_groups[] = {
    &gesture_status_atts_group,
	&key_value_atts_group,
    NULL
};
int gesture_status_sysfs_add_device(struct device *dev) {
	int ret, i;
	for(i = 0; gesture_status_attr_groups[i]; i++) {
		ret = sysfs_create_group(&dev->kobj, gesture_status_attr_groups[i]);
        if (ret) {
            while (--i >= 0) {
                sysfs_remove_group(&dev->kobj, gesture_status_attr_groups[i]);
            }
            break;
        }
	}
	if (ret) {
        return ret;
    }
	return 0;
}
int gesture_status_remove_device(struct device *dev) {
	int i;
	sysfs_remove_link(NULL, "touch");
    for (i = 0; gesture_status_attr_groups[i]; i++) {
        sysfs_remove_group(&dev->kobj, gesture_status_attr_groups[i]);
    }
	return 0;
}


static int sprd_gesture_status_probe(struct platform_device *pdev)
{
	int ret = 0;
     printk("sprd_gesture_status_probe start\n");
	if (IS_ERR_OR_NULL(pdev))
	{
		printk("IS_ERR_OR_NULL(pdev)\n");
		return -EINVAL;
	}
	

	ret = sysfs_create_link(NULL,&pdev->dev.kobj, "touch");
	if (ret < 0) 
	{
		printk("Failed to create link!\n");
		goto exit;
	}
	gesture_status_sysfs_add_device(&pdev->dev);
	printk("sprd_gesture_status_probe Success!\n");
	return 0;
exit:
	return ret;
}

static int sprd_gesture_status_remove(struct platform_device *pdev)
{
	gesture_status_remove_device(&pdev->dev);
	return 0;
}

static const struct of_device_id sprd_gesture_status_of_match[] = {
	{ .compatible = "sprd,gesture_status", },
	{},
};

static struct platform_driver sprd_gesture_status_driver = {
	.probe = sprd_gesture_status_probe,
	.remove = sprd_gesture_status_remove,
	.driver = {
		.name = "gesture-status",
		.of_match_table = of_match_ptr(sprd_gesture_status_of_match),
	},
};

static int sprd_gesture_status_register_driver(void)
{
	int ret = 0;
     printk("sprd_gesture_status_register_driver!\n");
	ret =platform_driver_register(&sprd_gesture_status_driver);
	return ret;
}
static void sprd_gesture_status_unregister_driver(void)
{
    printk("sprd_gesture_status_unregister_driver!\n");
	platform_driver_unregister(&sprd_gesture_status_driver);

}
static int __init  sprd_gesture_status_driver_init(void)
{
	int ret = 0;
    printk("sprd_gesture_status_driver_init!\n");
	ret = sprd_gesture_status_register_driver();
	return ret;
}
static void __exit sprd_gesture_status_driver_deinit(void)
{
	sprd_gesture_status_unregister_driver();
}

MODULE_LICENSE("GPL");
module_init(sprd_gesture_status_driver_init);
module_exit(sprd_gesture_status_driver_deinit);
