/*
 * Copyright (c) 2013, Google, Inc. All rights reserved
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#ifndef __TRACE_H
#define __TRACE_H

#include <sprd_types.h>

/* Flags for ftrace_record */
enum ftrace_flags {
	FUNCF_EXIT		= 0UL << 30,
	FUNCF_ENTRY		= 1UL << 30,
	FUNCF_TEXTBASE		= 2UL << 30,

	FUNCF_TIMESTAMP_MASK	= 0x3fffffff,
};

/* Information about a single function entry/exit */
struct trace_call {
	uint32_t func;		/* Function offset */
	uint32_t caller;	/* Caller function offset */
	uint32_t flags;		/* Flags and timestamp */
};

/* The header at the start of the trace memory area */
struct trace_hdr {
	u64 magic;	/*magic number*/
	u64 func_count;		/* Total number of function call sites */
	u64 call_count;		/* Total number of tracked function calls */
	u64 untracked_count;	/* Total number of untracked function calls */
	u64 ftrace_offset;	/*The offset of function call records*/
	u64 ftrace_num;	/* The number for trace_call struct we have space for */
	u64 ftrace_count;	/* The number of ftrace records written */
	u64 trace_size;
	u32 depth;
	u32 depth_limit;
};
/**
 * Init the trace system
 */
int trace_init(void);
void trace_save(void);

#endif
