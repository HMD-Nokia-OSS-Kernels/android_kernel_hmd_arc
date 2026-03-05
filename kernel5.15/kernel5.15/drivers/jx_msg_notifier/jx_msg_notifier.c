#include <linux/jx_msg_notifier.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/device.h>
#include <asm/uaccess.h>

#include <linux/stat.h>
#include <linux/of.h>


static BLOCKING_NOTIFIER_HEAD(jx_msg_notifier_list);

int jx_msg_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&jx_msg_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(jx_msg_notifier_register);

int jx_msg_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&jx_msg_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(jx_msg_notifier_unregister);

int jx_msg_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&jx_msg_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(jx_msg_notifier_call_chain);

static int __init jx_msg_notifier_init(void) {
 printk("jx_msg_notifier_init");
 return 0;
}

static void __exit jx_msg_notifier_exit(void) {
	printk("jx_msg_notifier_exit");
}

MODULE_LICENSE("GPL");
module_init(jx_msg_notifier_init);
module_exit(jx_msg_notifier_exit);
