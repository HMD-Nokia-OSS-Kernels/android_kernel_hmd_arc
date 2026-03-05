/*通常会用到头文件*/
#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/spinlock.h>
#include <soc/sprd/board.h>

/*中断函数头文件*/
#include <linux/interrupt.h>
#include <linux/irq.h>

#define INPUT_DEV_NAME "hall_event"

static int pick_up_hall;
static int put_down_hall;
static struct input_dev *input_dev = NULL;
static int IRQ_HALL;
static struct work_struct work;
static struct workqueue_struct *wq = NULL;

extern bool globle_bl;
static int hall_gpio;
#ifdef ZCFG_TCL_KEYBOARD_VCC_CONTROL
extern int ldo_enable(int enable);
#endif



static int key_state = 0;

static ssize_t hall_status_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return sprintf(buf, "%d\n", key_state);
}

static ssize_t hall_status_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

static DEVICE_ATTR(hall_status, 0644, hall_status_show, hall_status_store);

static struct attribute *hall_attributes[] = {
&dev_attr_hall_status.attr,
NULL
};

static struct attribute_group hall_attribute_group = {
.attrs = hall_attributes
};

static void hall_key_work_func(struct work_struct *work)
{
	unsigned int hall_trigger = irqd_get_trigger_type(irq_get_irq_data(IRQ_HALL));
#ifdef ZCFG_HALL_KEY_TCL
	printk("%s  hall_trigger:%d \n",__func__,(hall_trigger == IRQF_TRIGGER_HIGH)?1:0);
	if(hall_trigger == IRQF_TRIGGER_LOW)
	{
#ifdef ZCFG_TCL_KEYBOARD_VCC_CONTROL
        ldo_enable(0);
#endif
		input_report_key(input_dev, put_down_hall, 1);
		msleep(10); 
		input_report_key(input_dev, put_down_hall, 0);
		input_sync(input_dev);
		irq_set_irq_type(IRQ_HALL,IRQF_TRIGGER_HIGH);
		key_state = 0;
	}
	else
	{
#ifdef ZCFG_TCL_KEYBOARD_VCC_CONTROL
		ldo_enable(1);
#endif
		input_report_key(input_dev, pick_up_hall, 1);
		msleep(10); 
		input_report_key(input_dev, pick_up_hall, 0);
		input_sync(input_dev);
		irq_set_irq_type(IRQ_HALL,IRQF_TRIGGER_LOW);
		key_state = 1;
	}
#else
	printk("%s  hall_trigger:%d hall_gpio:%d globle_bl:%d\n",__func__,(hall_trigger == IRQF_TRIGGER_HIGH)?1:0,gpio_get_value(hall_gpio),globle_bl);
	if(gpio_get_value(hall_gpio)&&(globle_bl==0))
	{
		input_event(input_dev, EV_KEY, pick_up_hall, 1);		
		input_sync(input_dev);
		input_event(input_dev, EV_KEY, pick_up_hall, 0);		
		input_sync(input_dev);
		irq_set_irq_type(IRQ_HALL,IRQF_TRIGGER_LOW);		
		key_state = 1;
	}
	else if(!gpio_get_value(hall_gpio)&&(globle_bl==1))
	{
		input_sync(input_dev);
		input_report_key(input_dev, put_down_hall, 1);
		input_report_key(input_dev, put_down_hall, 0);
		input_sync(input_dev);
		irq_set_irq_type(IRQ_HALL,IRQF_TRIGGER_HIGH);
		key_state = 0;
	}
	else if(!gpio_get_value(hall_gpio)&&(globle_bl==0))irq_set_irq_type(IRQ_HALL,IRQF_TRIGGER_HIGH);
	else if(gpio_get_value(hall_gpio)&&(globle_bl==1))irq_set_irq_type(IRQ_HALL,IRQF_TRIGGER_LOW);	
	hall_trigger = irqd_get_trigger_type(irq_get_irq_data(IRQ_HALL));
	printk("%s hall_trigger:%d \n",__func__,(hall_trigger == IRQF_TRIGGER_HIGH)?1:0);
#endif		
 	enable_irq(IRQ_HALL);
}

static irqreturn_t hall_trigger_irq_handler(int irq, void *dev_id)
{
	disable_irq_nosync(IRQ_HALL);
  	queue_work(wq, &work);
  	return IRQ_HANDLED;
}

static const struct of_device_id hall_of_match[] = {
	{ .compatible = "hall_report", },
	{ },
};

static int hall_report_probe(struct platform_device *pdev)
{
 
	int ret;
	int irq;  
	int state;  
	struct device_node *gpio_node = pdev->dev.of_node; 
	hall_gpio = of_get_named_gpio(gpio_node, "hall-gpio", 0); 
	of_property_read_u32(gpio_node, "pick_up",&pick_up_hall);
	of_property_read_u32(gpio_node, "put_down",&put_down_hall);
	printk("pick_up_hall %d,put_down_hall %d\n", pick_up_hall,put_down_hall);
	ret = gpio_request(hall_gpio, "HALL");
	if (ret < 0) 
	{
   		printk("failed to request hall GPIO_%d,error %d\n", hall_gpio,ret);
   		goto failed_to_request_gpio;            
	}
 
   wq= create_singlethread_workqueue("hall_wq");

	 if (!wq) {
		 printk("Creat hall_wq workqueue failed.");
		 ret = -ENOMEM;
		 goto failed_to_create_singlethread_workqueue;    
	 }
   state = gpio_get_value(hall_gpio);
	 irq = gpio_to_irq(hall_gpio);
 	 IRQ_HALL = irq;
   input_dev = input_allocate_device();
   if (!input_dev) 
   {
   	printk("input_allocate_device for input_dev failed!\n");
   	ret = -ENOMEM;
   	goto failed_to_allocate_input_device;
   }

	input_dev->name = INPUT_DEV_NAME;
    ret = input_register_device(input_dev);
    if (ret)
    {
    	printk("input_register_device for input_dev failed!\n");
   	  goto failed_to_register_input_device;
    }
    
   INIT_WORK(&work, hall_key_work_func);



   if(state)
   {
 		ret = request_irq(IRQ_HALL,hall_trigger_irq_handler,
		IRQF_TRIGGER_LOW | IRQF_NO_SUSPEND,
		"hall_key_irq",
		pdev);

		key_state = 1;

   }
   else
   {
   	ret = request_irq(IRQ_HALL,hall_trigger_irq_handler,
		IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND,
		"hall_key_irq",
		pdev);

		key_state = 0;

   }
	if(ret)
	{
			printk("Request IRQ failed!ERRNO:%d\n", ret);
			goto exit;       
	}
	
	__set_bit(pick_up_hall, input_dev->keybit);
  __set_bit(put_down_hall,input_dev->keybit);

	__set_bit(EV_KEY,input_dev->evbit);	

	ret = sysfs_create_group(&pdev->dev.kobj, &hall_attribute_group);
	if (ret < 0) {
		printk("%s error creating sysfs attr files\n",__func__);
	}

	ret = sysfs_create_link(NULL, &pdev->dev.kobj, "hall");
	if (ret < 0) {
		printk("%s Failed to create link!\n",__func__);
		sysfs_remove_link(NULL, "hall");
	}

	return 0;
		
exit:    
	
failed_to_register_input_device:
				input_free_device(input_dev);
failed_to_allocate_input_device:
			  kfree(input_dev);

failed_to_create_singlethread_workqueue:
				destroy_workqueue(wq);		
failed_to_request_gpio:
				gpio_free(hall_gpio);	
				return -1;

}


static int __maybe_unused hall_driver_suspend(struct device *dev)
{	
	printk("%s --%d-- \n",__func__,__LINE__);
	return 0;
}
static int __maybe_unused hall_driver_resume(struct device *dev)
{
	printk("%s --%d-- \n",__func__,__LINE__);
	return 0;
}
static SIMPLE_DEV_PM_OPS(hall_driver_pm_ops, hall_driver_suspend, hall_driver_resume);

static struct platform_driver hall_report_driver = {
        .driver = {
			.name = "hall_report",
			.owner = THIS_MODULE,
			.pm	= &hall_driver_pm_ops,           
			.of_match_table = hall_of_match,
        },
        .probe = hall_report_probe,
};



static int __init hall_driver_init(void)
{
	int ret;
	printk("hall_driver_init\n");
	ret = platform_driver_register(&hall_report_driver);

	return ret;
}

static void __exit hall_driver_exit(void)
{
	remove_proc_entry("leather_cover_status", NULL);
    platform_driver_unregister(&hall_report_driver);
}

module_init(hall_driver_init);
module_exit(hall_driver_exit);

MODULE_DESCRIPTION("hall_key_driver");
MODULE_AUTHOR("Boxian Xu ");
MODULE_LICENSE("GPL");
