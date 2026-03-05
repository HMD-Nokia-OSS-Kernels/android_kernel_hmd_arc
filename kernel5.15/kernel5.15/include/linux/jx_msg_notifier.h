#ifndef __JX_MSG_NOTIFIER_H__
#define __JX_MSG_NOTIFIER_H__

#include <linux/notifier.h>

enum {
	MSG_WATER_CHECK_LOW_RES = 0, //Circuit short circuit
	MSG_WATER_CHECK_HIGH_RES, //Circuit is normal
};

int jx_msg_notifier_register(struct notifier_block *nb);
int jx_msg_notifier_unregister(struct notifier_block *nb);
int jx_msg_notifier_call_chain(unsigned long val, void *v);

#endif