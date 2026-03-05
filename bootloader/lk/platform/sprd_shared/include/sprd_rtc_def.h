/*
 * <sprd_rtc_def.h> - <struct time>
 *
 * Copyright (C) 2021 Unisoc Communications Inc.
 * History:
 * 	<2021> <sprd_rtc_def.h>
 *	Pass data from the general interface to the underlying code related
 *	to the hardware, and vice versa, but please note that there is still
 *	a difference from the common "struct time".
 *	struct time:        		struct rtc_time:
 *	tm_mon   0 ... 11        	1 ... 12
 *	tm_year  years since 1900   years since 0

 */
#ifndef _SPRD_RTC_DEF_H
#define _SPRD_RTC_DEF_H

struct rtc_time {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

#endif /*_SPRD_RTC_DEF_H*/
