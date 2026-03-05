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
#include "voice_wakeup.h"

struct voice_data g_voice;

static irqreturn_t voice_irq_handler(int irq, void *dev_id)
{
    	struct input_dev *input = dev_id;
		
	pm_wakeup_event(input->dev.parent, 0);
	input_report_key(input, KEY_U, 1);
	input_report_key(input, KEY_U, 0);
	input_sync(input);

    return IRQ_HANDLED;
}

static int sprd_voice_wakeup_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct input_dev *input;
	int ret = 0;

	printk("sprd_voice_wakeup_probe start\n");

	input = devm_input_allocate_device(dev);
	if (!input) {
		dev_err(dev, "failed to allocate input device\n");
		return -ENOMEM;
	}

	input->name = "voice_wakeup";

	input_set_capability(input, EV_KEY, KEY_U);

	g_voice.irq_num = of_get_named_gpio(pdev->dev.of_node, "irq-gpios", 0);
	if (!gpio_is_valid(g_voice.irq_num)) {
		printk("sprd_voice_wakeup_probe gpio = %d\n",g_voice.irq_num);
	}
	ret = gpio_request(g_voice.irq_num, "voice_wakeup-gpios");
	if (ret < 0){
		printk("sprd_voice_wakeup_probe  gpio_request(%d) = %d\n",g_voice.irq_num,ret);
		goto sysfs_exit;
	}else{
		ret = gpio_direction_input(g_voice.irq_num);
		if (ret) {
		        printk("sprd_voice_wakeup_probe gpio_direction_input(%d) = %d\n", g_voice.irq_num, ret);
		}
		g_voice.irq = gpio_to_irq(g_voice.irq_num);
		if(g_voice.irq < 0 ){
			printk("sprd_voice_wakeup_probe gpio_to_irq(%d) failed\n", g_voice.irq_num);
		}else{
			printk("sprd_voice_wakeup_probe gpio_to_irq(%d) = %d\n", g_voice.irq_num,g_voice.irq);
		}
		ret  = request_irq(g_voice.irq,voice_irq_handler,IRQF_TRIGGER_RISING, "voice_wakeup",input);
	        if ( ret < 0 ) {
	            printk(" sprd_voice_wakeup_probe Filed to request_irq (%d), ert=%d\n",g_voice.irq, ret);
	            goto sysfs_exit;
	        } else {
		     printk(" sprd_voice_wakeup_probe Enable_irq_wake \n");
			/* Wake up the system while receiving the interrupt.*/
	            enable_irq_wake(g_voice.irq); 	
	        }
	}

	ret = input_register_device(input);
	if (ret) {
		dev_err(dev, "failed to register input device: %d\n", ret);
		return ret;
	}
	
	device_init_wakeup(dev, 1);
		
	printk("sprd_voice_wakeup_probe Success!\n");
	return 0;
sysfs_exit:
	gpio_free(g_voice.irq_num);

	return ret;
}

static int sprd_voice_wakeup_remove(struct platform_device *pdev)
{
	gpio_free(g_voice.irq_num);
	return 0;
}

static const struct of_device_id sprd_voice_wakeup_of_match[] = {
	{ .compatible = "sprd,voice_wakeup", },
	{},
};

static struct platform_driver sprd_voice_wakeup_driver = {
	.probe = sprd_voice_wakeup_probe,
	.remove = sprd_voice_wakeup_remove,
	.driver = {
		.name = "voice-wakeup",
		.of_match_table = sprd_voice_wakeup_of_match,
	},
};

static int __init  sprd_voice_wakeup_driver_init(void)
{

	return platform_driver_register(&sprd_voice_wakeup_driver);
}

static void __exit sprd_voice_wakeup_driver_deinit(void)
{
	platform_driver_unregister(&sprd_voice_wakeup_driver);
}

module_init(sprd_voice_wakeup_driver_init);
module_exit(sprd_voice_wakeup_driver_deinit);
MODULE_DESCRIPTION("Sprd voice_wakeup Driver");
MODULE_LICENSE("GPL");

