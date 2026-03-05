/*
 * Copyright (c) 2013, Google, Inc. All rights reserved
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <sprd_common.h>
#include <linux/kernel.h>
#include <sprd_trace.h>
#include <lk/debug.h>
#include <sprd_log.h>
#include <sprd_common_rw.h>
#include "asm/arch/sys_timer_reg_v0.h"

#define FUNC_SIZE 4
#define PTR_TO_NUM(x)	(((uintptr_t)x - (uintptr_t)CONFIG_SYS_TEXT_BASE) / FUNC_SIZE)

static struct trace_hdr *hdr;	/* Pointer to start of trace buffer */
static struct trace_call *ftrace;
static char trace_enabled __attribute__((section(".data")));
static unsigned long long timestampftrace;
static ulong lastincftrace;
#define TIMER_MAX_VALUE			0xFFFFFFFF
#define SPRD_TRACE_MAGIC	0x44525053 /* "SPRD" */

unsigned long long __attribute__((no_instrument_function)) get_ticks_ftrace(void)
{
	ulong now = readl(SYS_VLAUE_SHDW);

	if(now >= lastincftrace) {
	/* not roll
	 * move stamp forward with absolut diff ticks
	 * */
		timestampftrace += (now - lastincftrace);
	}else{
		//timer roll over
		timestampftrace += (TIMER_MAX_VALUE - lastincftrace) + now;
	}
	lastincftrace = now;
	return timestampftrace;
}

static void __attribute__((no_instrument_function)) add_trace_call(void *func_ptr, void *caller, ulong flags)
{
	if (hdr->depth > hdr->depth_limit) {
		errorf("the trace is too deep, so can't add trace call\n");
		return;
	}
	if (hdr->ftrace_count < hdr->ftrace_num) {
		struct trace_call *rec = &ftrace[hdr->ftrace_count];

		rec->func = PTR_TO_NUM(func_ptr);
		rec->caller = PTR_TO_NUM(caller);
		rec->flags = flags | (get_ticks_ftrace() & FUNCF_TIMESTAMP_MASK);
	}
	hdr->ftrace_count++;
}

void __attribute__((no_instrument_function)) __cyg_profile_func_enter(void *func_ptr, void *caller)
{
	if (trace_enabled) {
		int func;

		add_trace_call(func_ptr, caller, FUNCF_ENTRY);
		func = PTR_TO_NUM(func_ptr);
		if (func < hdr->func_count) {
			hdr->call_count++;
		} else {
			hdr->untracked_count++;
		}
		hdr->depth++;
	}
}

void __attribute__((no_instrument_function)) __cyg_profile_func_exit(void *func_ptr, void *caller)
{
	if (trace_enabled) {
		add_trace_call(func_ptr, caller, FUNCF_EXIT);
		hdr->depth--;
	}
}

/**
 * Init the trace for lk
 */
extern int _start, __bss_end;
int  __attribute__((no_instrument_function)) trace_init(void)
{
	uint64_t bin_len = (uint64_t)&__bss_end - (uint64_t)&_start;
	uint64_t func_count = bin_len / FUNC_SIZE;
	uint64_t needed;
	char *buff = (char *)TRACE_BUF_ADDR;
	uint64_t buf_size = (uint64_t)TRACE_BUF_SIZE;

	hdr = (struct trace_hdr *)buff;
	needed = sizeof(struct trace_hdr);
	if (needed > buf_size) {
		errorf("trace: buffer size %zd bytes: at least %zd needed\n", buf_size, needed);
		return -1;
	}

	memset(hdr, '\0', needed);
	hdr->ftrace_offset = needed;
	hdr->func_count = func_count;

	/* Use any remaining space for the timed function trace */
	hdr->ftrace_num = (buf_size - needed) / sizeof(struct trace_call);
	ftrace = (struct trace_call*)((u64)hdr + hdr->ftrace_offset);

	if (hdr->ftrace_count < hdr->ftrace_num) {
		struct trace_call *start = &ftrace[hdr->ftrace_count];
		start->func = CONFIG_SYS_TEXT_BASE;
		start->caller = 0;
		start->flags = FUNCF_TEXTBASE;
	}
	hdr->ftrace_count++;

	dprintf(INFO, "trace: enabled\n");
	hdr->depth_limit = 15;
	hdr->magic = SPRD_TRACE_MAGIC;
	trace_enabled = 1;

	return 0;
}

/**
 * Init the trace for lk
 */
void trace_save(void)
{
	hdr->trace_size = hdr->ftrace_offset + hdr->ftrace_count * sizeof(struct trace_call);

	dprintf(INFO, "the trace data size is 0x%x\n", hdr->trace_size);
	if (0 != common_raw_write(UBOOT_LOG_PARTITION, (uint64_t)hdr->trace_size, (uint64_t)0,
								(uint64_t)0, (char*)TRACE_BUF_ADDR)) {
		errorf("write trace 2 storage failed\n");
	}
}
