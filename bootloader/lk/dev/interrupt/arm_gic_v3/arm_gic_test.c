/*
# Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
# Licensed under the Unisoc General Software License, version 1.0 (the License);
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
# Software distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OF ANY KIND, either express or implied.
# See the Unisoc General Software License, version 1.0 for more details.
*/

/*just test gic timer feature.*/
#include <dev/interrupt/arm_gic.h>
#include <platform/timer.h>
#include <kernel/timer.h>
#include <kernel/thread.h>
#include <platform/interrupts.h>

static enum handler_return oneshot_timer_test(timer_t *timer, lk_time_t now, void *arg)
{
	printf("oneshot_timer_test:current system clock now:%d\n",now);
	return INT_NO_RESCHEDULE;
}

timer_t timer_test;
void gic_oneshot_timer_test(void)
{
	thread_t *gict = get_current_thread();
	thread_set_real_time(gict);
	dprintf(INFO,"====== gic test in function: %s, in line: %d\n",__FUNCTION__, __LINE__);

	__asm__ volatile("b .");

	timer_initialize(&timer_test);
	timer_set_oneshot(&timer_test, 10, oneshot_timer_test, NULL);
}

