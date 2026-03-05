#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

struct jx_gpio_data {
    int gpio_count;
    int *gpio_numbers;
    const char **gpio_names;
    struct device_attribute *attrs;
    struct device *dev;
};

struct jx_gpio_data *g_data_t;

/*提供给其他内核模块调用的接口，用于读取该驱动注册了的GPIO。
param attr_name : dts的gpio标签
*/
int jx_gpio_read(const char *attr_name)
{
    int i, value;
    printk("jx_gpio_read %s\n",attr_name);
    // 查找对应的 GPIO
    for (i = 0; i < g_data_t->gpio_count; i++) {
        if (strcmp(attr_name, g_data_t->gpio_names[i]) == 0) {
            value = gpio_get_value(g_data_t->gpio_numbers[i]);
            return value;
        }
    }

    return -EINVAL;
}
EXPORT_SYMBOL(jx_gpio_read);


/*提供给其他内核模块调用的接口，用于设置该驱动注册了的GPIO电平。
param attr_name : dts的gpio标签
*/
int jx_gpio_set(char *attr_name, int value)
{
    int i;
    
    // 解析输入值不符合规范就不成功
    if ((value != 0) && (value != 1))
        return -EINVAL;
    // 查找对应的 GPIO
    printk("%s gpio count = %d\n",__func__,g_data_t->gpio_count);
    for (i = 0; i < g_data_t->gpio_count; i++) {
        if (strcmp(attr_name, g_data_t->gpio_names[i]) == 0) {
            gpio_set_value(g_data_t->gpio_numbers[i], value);
            printk("%s %d = %d\n",__func__,g_data_t->gpio_numbers[i],value);
            return 0;
        }
    }

    return -EINVAL;
}
EXPORT_SYMBOL(jx_gpio_set);

// show 所有通过该驱动注册的gpio
static ssize_t gpio_list_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct jx_gpio_data *data = dev_get_drvdata(dev);
    int i, value;
    ssize_t len = 0;

    // 遍历所有 GPIO，读取状态并格式化输出
    for (i = 0; i < data->gpio_count; i++) {
        value = gpio_get_value(data->gpio_numbers[i]);
        len += sprintf(buf + len, "(%s->%d:%d)\n", data->gpio_names[i],data->gpio_numbers[i], value);
        printk("gpio_value_show (%s:%d)\n",data->gpio_names[i],value);
    }

    return len;

}
static DEVICE_ATTR(gpio_list, 0644, gpio_list_show, NULL);
// Sysfs show 回调函数
static ssize_t gpio_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct jx_gpio_data *data = dev_get_drvdata(dev);
    const char *attr_name = attr->attr.name;
    int i, value;

    // 查找对应的 GPIO
    for (i = 0; i < data->gpio_count; i++) {
        if (strcmp(attr_name, data->gpio_names[i]) == 0) {
            value = gpio_get_value(data->gpio_numbers[i]);
            return sprintf(buf, "%d\n", value);
        }
    }

    return -EINVAL;
}

// Sysfs store 回调函数
static ssize_t gpio_value_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct jx_gpio_data *data = dev_get_drvdata(dev);
    const char *attr_name = attr->attr.name;
    int i, value;

    // 解析输入值
    if (kstrtoint(buf, 10, &value) < 0)
        return -EINVAL;

    // 查找对应的 GPIO
    for (i = 0; i < data->gpio_count; i++) {
        if (strcmp(attr_name, data->gpio_names[i]) == 0) {
            gpio_set_value(data->gpio_numbers[i], value);
            return count;
        }
    }

    return -EINVAL;
}

static int jx_gpio_probe(struct platform_device *pdev)
{
    struct device_node *np = pdev->dev.of_node;
    struct jx_gpio_data *data;
    struct property *prop;
    const char *propname;
    int i = 0, gpio_count = 0 ,ret = 0;

    // 分配内存存储 GPIO 数据
    data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    g_data_t = data;

    data->dev = &pdev->dev;

    // 第一次遍历：计算 GPIO 数量
    for_each_property_of_node(np, prop) {
        propname = prop->name;
        if (strstr(propname, "-gpio")) {
            gpio_count++;
        }
    }

    // 分配内存存储 GPIO 编号和名称
    data->gpio_count = gpio_count;
    data->gpio_numbers = devm_kcalloc(&pdev->dev, gpio_count, sizeof(int), GFP_KERNEL);
    data->gpio_names = devm_kcalloc(&pdev->dev, gpio_count, sizeof(const char *), GFP_KERNEL);
    data->attrs = devm_kcalloc(&pdev->dev, gpio_count, sizeof(struct device_attribute), GFP_KERNEL);
    if (!data->gpio_numbers || !data->gpio_names || !data->attrs)
        return -ENOMEM;

    
    // 第二次遍历：获取 GPIO 编号和名称
    for_each_property_of_node(np, prop) {
        propname = prop->name;
        if (strstr(propname, "-gpio")) {
            data->gpio_numbers[i] = of_get_named_gpio(np, propname, 0);
            if (data->gpio_numbers[i] < 0) {
                dev_err(&pdev->dev, "Failed to get GPIO %s: %d\n", propname, data->gpio_numbers[i]);
                goto free_gpios;
            }

            data->gpio_names[i] = devm_kstrdup(&pdev->dev, propname, GFP_KERNEL);
            if (!data->gpio_names[i])
                goto free_gpios;

            // 申请和配置 GPIO
            if (gpio_request(data->gpio_numbers[i], data->gpio_names[i])) {
                dev_err(&pdev->dev, "Failed to request GPIO %s\n", data->gpio_names[i]);
                goto free_gpios;
            }
            gpio_direction_output(data->gpio_numbers[i], 1);  // 设置 GPIO 为高电平
            dev_info(&pdev->dev, "GPIO %s set to high\n", data->gpio_names[i]);

            // 动态创建 sysfs 节点
            sysfs_attr_init(&data->attrs[i].attr);
            data->attrs[i].attr.name = data->gpio_names[i];
            data->attrs[i].attr.mode = 0644;
            data->attrs[i].show = gpio_value_show;
            data->attrs[i].store = gpio_value_store;

            if (sysfs_create_file(&pdev->dev.kobj, &data->attrs[i].attr)) {
                dev_err(&pdev->dev, "Failed to create sysfs file for GPIO %s\n", data->gpio_names[i]);
                goto free_gpios;
            }

            i++;
        }
    }

    if (sysfs_create_file(&pdev->dev.kobj, &dev_attr_gpio_list.attr)) {
        dev_err(&pdev->dev, "Failed to create gpio_list sysfs file for GPIO \n");
    }

    ret = sysfs_create_link(NULL, &pdev->dev.kobj, "gpios");
	if (ret < 0) {
		dev_info(&pdev->dev, "%s Failed to create link!\n",
			 __func__);
		sysfs_remove_link(NULL, "gpios");
	}

    platform_set_drvdata(pdev, data);

    return 0;

free_gpios:
    // 释放已申请的 GPIO 和 sysfs 节点
    while (i--) {
        gpio_free(data->gpio_numbers[i]);
        sysfs_remove_file(&pdev->dev.kobj, &data->attrs[i].attr);
    }
    kobject_put(&pdev->dev.kobj);
    return -EBUSY;
}

static int jx_gpio_remove(struct platform_device *pdev)
{
    struct jx_gpio_data *data = platform_get_drvdata(pdev);
    int i;

    // 释放所有 GPIO 和 sysfs 节点
    for (i = 0; i < data->gpio_count; i++) {
        gpio_set_value(data->gpio_numbers[i], 0);  // 设置 GPIO 为低电平
        gpio_free(data->gpio_numbers[i]);          // 释放 GPIO
        sysfs_remove_file(&pdev->dev.kobj, &data->attrs[i].attr);  // 删除 sysfs 节点
        dev_info(&pdev->dev, "GPIO %s freed\n", data->gpio_names[i]);
    }

    // 删除 /sys/gpios 目录
    kobject_put(&pdev->dev.kobj);

    return 0;
}

static const struct of_device_id jx_gpio_of_match[] = {
    { .compatible = "jx,jx-gpio" },
    {},
};
MODULE_DEVICE_TABLE(of, jx_gpio_of_match);

static struct platform_driver jx_gpio_driver = {
    .probe = jx_gpio_probe,
    .remove = jx_gpio_remove,
    .driver = {
        .name = "jx-gpio",
        .of_match_table = jx_gpio_of_match,
    },
};

module_platform_driver(jx_gpio_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chen xiaopeng");
MODULE_DESCRIPTION("Driver for dynamically handling GPIOs with sysfs support");