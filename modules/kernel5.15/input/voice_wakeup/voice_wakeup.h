#ifndef __VOICE_WAKEUP__
#define __VOICE_WAKEUP__

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/input.h>

#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_address.h>
#endif

struct voice_data {
    int    irq_num;
    int    irq;
};

#endif//ndef __VOICE_WAKEUP__