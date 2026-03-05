/*
 * <sprd_stddef.h> - <Definition types and offset of the function of this file>
 *
 * Copyright (C) 2021 Unisoc Communications Inc.
 * History:
 * 	<2021> <sprd_stddef.h>
 */

#ifndef _SPRD_STDDEF_H
#define _SPRD_STDDEF_H

#undef NULL
#if defined(__cplusplus)
#define NULL 0
#else
#define NULL ((void *)0)
#endif

#ifndef __CHECKER__
#undef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#ifndef _SIZE_T
#include <linux/types.h>
#endif

#endif
