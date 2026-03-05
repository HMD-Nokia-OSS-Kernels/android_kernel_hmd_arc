/*
 * <sprd_compat.h>
 * Copyright (C) 2021 Unisoc Communications Inc.
 * History:
 * 	<2021> <sprd_compat.h>
 *
 */

#ifndef _SPRD_COMPAT_H_
#define _SPRD_COMPAT_H_
#include <malloc.h>
#include <linux/types.h>
#include <string.h>
#include <sprd_common.h>
#include <linux/compiler.h>

#define __init
#define __exit
#define __devinit
#define __devinitdata

#define KERN_ERR
#define KERN_NOTICE

#define GFP_ATOMIC ((gfp_t) 0)
#define GFP_KERNEL ((gfp_t) 0)
#define GFP_ZERO	((__force gfp_t)0x8000u)	/* Return zeroed page on success */

struct work_struct {};
struct timer_list {};

/*notifer struct*/
struct notifier_block;
typedef	int (*notifier_fn_t)(struct notifier_block *nb,
			unsigned long action, void *data);
struct notifier_block {
	char *name;
	notifier_fn_t notifier_call;
	struct notifier_block *next;
	int priority; /*Notified at last if priority set 0 */
};

struct device {
	struct device	*parent;
	struct class	*class;
	dev_t		devt;	 /* dev_t, creates the sysfs "dev" */
	void	(*release)(struct device *dev); /* This is used from drivers/usb
								   /musb-new subsystem only */
	void	*driver_data; /* data private to the driver */
	void	*device_data; /* data private to the device */
};

/**
  * kmalloc - allocate memory from heap
  * @size: how many bytes of memory are required.
  * @flags: the type of memory to allocate.
*/
static inline void *kmalloc(size_t size, int flags)
{
	void *p;

	p = memalign(ARCH_DMA_MINALIGN, size);
	if (flags & GFP_ZERO)
		memset(p, 0, size);

	return p;
}
/**
  * kzalloc - allocate memory. The memory is set to zero.
**/
static inline void *kzalloc(size_t size, gfp_t flags)
{
	return kmalloc(size, flags | GFP_ZERO);
}

static inline void kfree(const void *block)
{
	free((void *)block);
}

#define __GFP_ZERO	((__force gfp_t)0x8000u)
static inline void *kmalloc_array(size_t n, size_t size, gfp_t flags)
{
	if (size != 0 && n > SIZE_MAX / size)
		return NULL;
	return kmalloc(n * size, flags | __GFP_ZERO);
}

#define vmalloc(size)	kmalloc(size, 0)
#define __vmalloc(size, flags, pgsz)	kmalloc(size, flags)

#define IRQ_NONE 0
#define IRQ_HANDLED 1
#define IRQ_WAKE_THREAD 2

typedef int irqreturn_t;
typedef int	wait_queue_head_t;

struct unused {};
typedef struct unused unused_t;
typedef unused_t spinlock_t;

#define spin_lock_init(lock) do {} while (0)
#define spin_lock(lock) do {} while (0)
#define spin_unlock(lock) do {} while (0)
#define spin_lock_irqsave(lock, flags) do { /* debug("%lu\n", flags); */ } while (0)
#define spin_unlock_irqrestore(lock, flags) do { flags = 0; } while (0)

struct mutex { int i; };
#define DEFINE_MUTEX(...)
#define mutex_init(...)
#define mutex_lock(...)
#define mutex_unlock(...)

#define cpu_relax() do {} while (0)

#define setup_timer(timer, func, data) do {} while (0)
#define del_timer_sync(timer) do {} while (0)
#define schedule_work(work) do {} while (0)
#define INIT_WORK(work, fun) do {} while (0)

#define pm_runtime_get_sync(dev) do {} while (0)
#define pm_runtime_put(dev) do {} while (0)

#define dev_set_drvdata(dev, data) do {} while (0)
#define free_irq(irq, data) do {} while (0)
#define disable_irq_wake(irq) do {} while (0)

#define module_put(...)		do { } while (0)
#define module_init(...)
#define module_exit(...)
#define MODULE_DESCRIPTION(...)
#define MODULE_AUTHOR(...)
#define MODULE_LICENSE(...)
#define MODULE_ALIAS(...)
#define module_param(...)
#define MODULE_PARM_DESC(...)

#define wake_up_interruptible(...)	do { } while (0)
#define init_waitqueue_head(...)	do { } while (0)

#endif
