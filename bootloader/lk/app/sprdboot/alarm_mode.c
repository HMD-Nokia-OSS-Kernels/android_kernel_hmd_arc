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

#include <boot_mode.h>
#include <sprd_common_rw.h>

#define msleep(a) udelay(a * 1000)
#define PROD_PART "prodnv"

extern void sprd_rtc_init(void);
extern void sprd_rtc_set_alarm_sec(unsigned long secs);
extern unsigned long sprd_rtc_get_alarm_sec(void);
extern unsigned long sprd_rtc_get_sec(void);
extern int sprd_is_poweroff_alarm(void);

int alarm_file_check(char *time_buf)
{
	if (!do_fs_file_read(PROD_PART, "/alarm_flag", time_buf, 200))
		return 1;
	else
		return -1;

}

int poweron_file_check(char *time_buf)
{
	if (!do_fs_file_read(PROD_PART, "/poweron_timeinmillis", time_buf, 200))
		return 1;
	else
		return -1;

}

static void uboot_rtc_set_alarm_sec(unsigned long secs)
{
	int i = 0;
	unsigned long read_secs;
	sprd_rtc_set_alarm_sec(secs);
	msleep(150);
	do {
		if (i != 0) {
			sprd_rtc_set_alarm_sec(secs);
			msleep(150);
		}
		read_secs = sprd_rtc_get_alarm_sec();
		msleep(1);
		i++;
	} while (read_secs != secs && i < 100);
}

__weak int sprd_is_poweroff_alarm(void)
{
	debugf("in temp function: [%s]...\n",__func__);
	return 0;
}

int alarm_flag_check(void)
{
	if (sprd_is_poweroff_alarm())
		return 1;
	else
		return 2;
}

#if 0
int alarm_flag_check(void)
{
	int ret = -1, ret1 = -1;
	long time = 0;
	long time1 = 0;
	unsigned long time_rtc = 0;
	unsigned long time_rtc1 = 0;
	unsigned long now_rtc = 0;
	int time_lead = 0;
	char time_buf[200] = { 0 };
	char time_buf1[200] = { 0 };

	memset(time_buf, 0x0, 200);
	memset(time_buf1, 0x0, 200);

	ret = alarm_file_check(time_buf);
	if (ret > 0) {
		dprintf(INFO,"file: alarm_flag exist\n");
		dprintf(INFO,"time get %s", time_buf);
		time = simple_strtol(time_buf, NULL, 10);
		time_rtc = time;
	}
	ret1 = poweron_file_check(time_buf1);
	if (ret1 > 0) {
		dprintf(INFO,"file: poweron_timeinmillis exist\n");
		dprintf(INFO,"time get %s", time_buf1);
		time1 = simple_strtol(time_buf1, NULL, 10);
		time_rtc1 = time1;
	}
	sprd_rtc_init();
	now_rtc = sprd_rtc_get_sec();
	dprintf(INFO,"now rtc %lu\n", now_rtc);
	time_lead = ALARM_LEAD_SET_MS;
	time = time - now_rtc;
	time1 = time1 - now_rtc;
	if ((time < time_lead + 180) && (time > time_lead - 10)) {
		if ((time1 < time_lead + 180) && (time1 > time_lead - 10) && (time - time1 > 50))
			ret = 2;
		else
			ret = 1;
	} else if ((time1 < time_lead + 180) && (time1 > time_lead - 10))
		ret = 2;
	else {
		if (ret == -1 && ret1 == -1) {
			dprintf(INFO,"file: all not found\n");
		} else if (ret1 == -1) {
			uboot_rtc_set_alarm_sec(time_rtc);
		} else if (ret == -1) {
			uboot_rtc_set_alarm_sec(time_rtc1);
		} else {
			if (time_rtc1 > time_rtc) {
				uboot_rtc_set_alarm_sec(time_rtc);
			} else {
				uboot_rtc_set_alarm_sec(time_rtc1);
			}
		}
		ret = 0;
	}

	return ret;
}
#endif
