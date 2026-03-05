/*
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2016-2023 Unisoc (Shanghai) Technologies Co. Ltd

 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

#include <regs_adi.h>
#include "adi_hal_internal.h"
#include <asm/arch/sprd_reg.h>
#include <asm/arch/sprd_eic.h>
#include <sprd_adc.h>
#include <sprd_battery.h>
#include <asm/arch/check_reboot.h>
#include <chipram_env.h>
#include <sprd_chg_helper.h>
#include "sprd_common_rw.h"
#include "sprd_fdt_support.h"
#include <boot_parse.h>
#include <lk/debug.h>
#include <delay.h>
#include <lk/board.h>

#define BAT_VOL_3100         3100
#define IBUS_LOW_CUR_1000    1000
#define IBAT_LOW_CUR_360     360
#ifdef  ZCFG_UBOOT_IBAT_CUR
#define IBUS_CUR_1000        ZCFG_UBOOT_IBAT_CUR*2
#define IBAT_CUR_500         ZCFG_UBOOT_IBAT_CUR
#else
#define IBUS_CUR_1000        1000
#define IBAT_CUR_500         500
#endif
#define NTC_VOL_BUFF_CNT     10

#define POWER_PATH_ENABLE    1
#define POWER_PATH_DISABLE   0

#ifndef MISCDATA_ADDRESS_BASE
#define MISCDATA_ADDRESS_BASE 0
#endif
#ifndef MISCDATA_MAGIC_OFFSET
#define MISCDATA_MAGIC_OFFSET 0
#endif
#ifndef MISCDATA_MAGIC_SIZE
#define MISCDATA_MAGIC_SIZE 0
#endif
#ifndef MISCDATA_RTC_TIME_OFFSET
#define MISCDATA_RTC_TIME_OFFSET 0
#endif
#ifndef MISCDATA_RTC_TIME_SIZE
#define MISCDATA_RTC_TIME_SIZE 0
#endif
#ifndef MISCDATA_CHARGE_CYCLE_OFFSET
#define MISCDATA_CHARGE_CYCLE_OFFSET 0
#endif
#ifndef MISCDATA_CHARGE_CYCLE_SIZE
#define MISCDATA_CHARGE_CYCLE_SIZE 0
#endif
#ifndef MISCDATA_BASP_OFFSET
#define MISCDATA_BASP_OFFSET 0
#endif
#ifndef MISCDATA_BASP_SIZE
#define MISCDATA_BASP_SIZE 0
#endif
#ifdef  ZCFG_MK_BATTERY_COMPATIBLE
#define BAT_ID  1
#else
#ifndef BAT_ID
#define BAT_ID  0
#endif
#endif
#ifndef MISCDATA_CAPACITY_OFFSET
#define MISCDATA_CAPACITY_OFFSET	0
#endif
#ifndef MISCDATA_CAPACITY_SIZE
#define MISCDATA_CAPACITY_SIZE		0
#endif
#ifndef MISCDATA_CAPACITY_CHECK_OFFSET
#define MISCDATA_CAPACITY_CHECK_OFFSET	0
#endif
#ifndef MISCDATA_CAPACITY_CHECK_SIZE
#define MISCDATA_CAPACITY_CHECK_SIZE	0
#endif
#ifndef MISCDATA_SHUTDOWN_CHARGE_FLAG_OFFSET
#define MISCDATA_SHUTDOWN_CHARGE_FLAG_OFFSET 0
#endif
#ifndef MISCDATA_SHUTDOWN_CHARGE_FLAG_SIZE
#define MISCDATA_SHUTDOWN_CHARGE_FLAG_SIZE 0
#endif
#ifndef MISCDATA_AUTO_POWERON_FLAG_OFFSET
#define MISCDATA_AUTO_POWERON_FLAG_OFFSET 0
#endif
#ifndef MISCDATA_AUTO_POWERON_FLAG_SIZE
#define MISCDATA_AUTO_POWERON_FLAG_SIZE 0
#endif
#ifndef MISCDATA_SHUTDOWN_CHARGE_EIGHTY_PERCENTAGE_STOP_CHARGE_OFFSET
#define MISCDATA_SHUTDOWN_CHARGE_EIGHTY_PERCENTAGE_STOP_CHARGE_OFFSET 0
#endif
#ifndef MISCDATA_SHUTDOWN_CHARGE_EIGHTY_PERCENTAGE_STOP_CHARGE_SIZE
#define MISCDATA_SHUTDOWN_CHARGE_EIGHTY_PERCENTAGE_STOP_CHARGE_SIZE 0
#endif

#ifndef CONFIG_NTC_VOL
#define CONFIG_NTC_VOL 0
#define NTC_VOL_CH 0
#define NTC_VOL_SCALE 0
#define NTC_VOL_MUX 0
#define NTC_VOL_CAL_TYPE 0
#define VOL2TEMP_DISCHARGING_LOW 0
#define VOL2TEMP_DISCHARGING_HIGH 0
#define VOL2TEMP_DISBOOTING 0
#endif

#define CAPACITY_TRACK_CAP_KEY0	0x20160726
#define CAPACITY_TRACK_CAP_KEY1	0x15211517
/* 30min = 30*60*1000ms*/
#define DEAD_DETECT_MS	(30 * 60 * 1000)
#define DEAD_DETECT_CNT	((DEAD_DETECT_MS) / (SPRDBAT_CHG_POLLING_T))
#define DEAD_DETECT_VOL	2000	/* 2000mv */
#define ENABLE_CHARGE_DOWNLOAD_THRESHOLD      3700
#define SPRD_BATTERY_MAGIC_NUM    0x5a5aa5a5

#define VCHG_DETECT_US		20000
#define VCHG_DETECT_CNT		150

extern chipram_env_t* get_chipram_env(void);

extern int sprd_eic_request(unsigned offset);
extern int sprd_eic_get(unsigned offset);

extern bool charge_first_power_on;
static bool reset_miscdata_flag;
extern unsigned long sprd_rtc_get_sec(void);
static int pre_cur,chg_cur,limit_cur;
static const struct sprdchg_ic_operations *sprd_chg_ic_op;
static int count,index;
static int ntc_vol_buff[NTC_VOL_BUFF_CNT];

extern int sprd_get_batid(void);

static void sprdchg_timer_callback(void)
{
	if (!sprd_chg_ic_op)
		return;

	count ++;
	if(count == 50){
		count = 0;
		sprd_chg_ic_op->timer_callback();
	}
}

static int get_average_ntc_vol(int ntc_vol)
{
	int i, min, max;
	int sum = 0;

	if (ntc_vol_buff[0] == -500) {
		for (i = 0; i < NTC_VOL_BUFF_CNT; i++)
			ntc_vol_buff[i] = ntc_vol;
	}
	if (index >= NTC_VOL_BUFF_CNT)
		index = 0;
	ntc_vol_buff[index++] = ntc_vol;
	min = max = ntc_vol_buff[0];

	for (i = 0; i < NTC_VOL_BUFF_CNT; i++) {
		if (ntc_vol_buff[i] > max)
			max = ntc_vol_buff[i];

		if (ntc_vol_buff[i] < min)
			min = ntc_vol_buff[i];

		sum += ntc_vol_buff[i];
	}

	sum = sum - max -min;

	return sum / (NTC_VOL_BUFF_CNT - 2);
}

int sprdbat_get_battery_temp_status(void)
{
	int32_t ntc_vol;
	static int discharging_cnt, disbooting_cnt;

	if (!CONFIG_NTC_VOL || !sprdbat_is_battery_connected(0))
		return NORMAL_TEMP;

	ntc_vol = sprd_chan_adc_to_vol(NTC_VOL_CH, NTC_VOL_SCALE, NTC_VOL_MUX, NTC_VOL_CAL_TYPE);
	ntc_vol = get_average_ntc_vol(ntc_vol);

	if (ntc_vol >= 0 && ntc_vol < VOL2TEMP_DISBOOTING) {
		discharging_cnt = 0;
		if (disbooting_cnt++ < 5)
			dprintf(INFO,"sprd_chg: %s : ntc_vol = %d\n", __func__, ntc_vol);
		return DISBOOTING_TEMP;
	} else if ((ntc_vol >= VOL2TEMP_DISBOOTING && ntc_vol < VOL2TEMP_DISCHARGING_HIGH) ||
		 ntc_vol > VOL2TEMP_DISCHARGING_LOW) {
		disbooting_cnt = 0;
		if (discharging_cnt++ < 5)
			dprintf(INFO,"sprd_chg: %s : ntc_vol = %d\n", __func__, ntc_vol);
		return DISCHARGING_TEMP;
	} else {
		disbooting_cnt = 0;
		discharging_cnt = 0;
		return NORMAL_TEMP;
	}
}

void sprdbat_lowbat_chg(int abnormal_temp_flag)
{
	unsigned int vbat_vol = sprdfgu_read_vbat_vol();
	int ibat_cur = sprdfgu_read_ibat_cur();
	static unsigned int polling_cnt = 0;
	static int dead_bat_flag = 0;

	if (!sprd_chg_ic_op)
		return;

	if (abnormal_temp_flag) {
		dprintf(INFO,"sprd_chg: Battery temperature is too high or low!!!stop charge, abnormal_temp_flag:%d\n",
                        abnormal_temp_flag);
		sprd_chg_ic_op->chg_stop();
		return;
	}

	if (vbat_vol < BAT_VOL_3100) {
		limit_cur = IBUS_LOW_CUR_1000;
		chg_cur = IBAT_LOW_CUR_360;
	} else {
		enum sprd_adapter_type charger;
		limit_cur = IBUS_CUR_1000;
		chg_cur = IBAT_CUR_500;

		charger = sprdchg_charger_is_adapter();
		if (charger == ADP_TYPE_DCP) {
			#ifdef DCP_LIMIT_CUR
				limit_cur = DCP_LIMIT_CUR;
			#endif

			#ifdef DCP_CHG_CUR
				chg_cur = DCP_CHG_CUR;
			#endif
		}
	}

	#ifdef PRE_CHG_CUR
		pre_cur = PRE_CHG_CUR;
		sprd_chg_ic_op->chg_cmd(CHG_SET_PRE_CURRENT, pre_cur);
		dprintf(INFO,"sprd_chg: %s pre_cur:%d\n", __func__, pre_cur);
	#endif

	sprd_chg_ic_op->chg_cmd(CHG_SET_LIMIT_CURRENT, limit_cur);
	sprd_chg_ic_op->chg_cmd(CHG_SET_CURRENT, chg_cur);
	sprd_chg_ic_op->chg_start();

	dprintf(INFO,"sprd_chg: vbat_vol:%d, chg_cur:%d, limit_cur:%d, ibat_cur:%d\n",
		vbat_vol, chg_cur, limit_cur, ibat_cur);

	sprdchg_timer_callback();

	dprintf(INFO,"sprd_chg: vbat_vol:%d\n", vbat_vol);

	/* dead battery detect */
	if (dead_bat_flag) {
		dprintf(ALWAYS,"sprd_chg: ERR!!!:Dead battery,disable charge!!!\n");
	} else {
		polling_cnt ++;

		if (vbat_vol > DEAD_DETECT_VOL) {
			dprintf(INFO,"sprd_chg: polling_cnt:%d clear,DEAD_DETECT_VOL:%d\n",
			    polling_cnt,DEAD_DETECT_VOL);
			polling_cnt = 0;
		}

		if(polling_cnt > DEAD_DETECT_CNT) {
			dprintf(ALWAYS,"sprd_chg: ERR!!!:Dead battery shutdown charge now!!!\n");
			dead_bat_flag = 1;
			sprd_chg_ic_op->chg_stop();
		}
	}
}

void sprdchg_register_ops(const struct sprdchg_ic_operations *ops)
{
	sprd_chg_ic_op = ops;
}

const struct sprdchg_ic_operations *sprdchg_ic_ops_get(void)
{
	return sprd_chg_ic_op;
}

int charger_connected(void)
{
	static int i = 0;

	sprd_eic_request(EIC_CHG_INT);
	udelay(3000);

	if (!(i++ % 100))
		dprintf(INFO,"sprd_chg: eica status %x\n", sprd_eic_get(EIC_CHG_INT));

	return !!sprd_eic_get(EIC_CHG_INT);
}

int get_mode_from_vchg(void)
{
	int ret = 0, i = 0;
	unsigned hw_rst_mode;

	hw_rst_mode = ANA_REG_GET(ANA_REG_GLB_POR_SRC_FLAG);
	ret = hw_rst_mode & HW_VCHG_STATUS;
	dprintf(INFO,"sprd_chg: %s hw_rst_mode= %x ret= %x\n", __func__, hw_rst_mode, ret);
	while (ret && !charger_connected()) {
		if  (!(i % 10))
			dprintf(INFO," %s Vchg is not present, debounce times = %d\n", __func__, i);
		if (i++ > VCHG_DETECT_CNT) {
			ret = 0;
			break;
		};
		udelay(VCHG_DETECT_US);
	};

	return ret;
}

int get_mode_from_gpio(void)
{
	int ret = 0;
	unsigned hw_rst_mode;

	hw_rst_mode = ANA_REG_GET(ANA_REG_GLB_POR_SRC_FLAG);
	ret = (hw_rst_mode & HW_PBINT2_STATUS) && !charger_connected();
	dprintf(INFO,"sprd_chg: %s hw_rst_mode= %x ret= %x\n", __func__, hw_rst_mode, ret);

	return ret;
}

static int sprdbat_read_miscdata_data(char *out, unsigned int address, unsigned int len)
{
	if (0 != common_raw_read("miscdata", len,
				 (uint64_t)(MISCDATA_ADDRESS_BASE + address), out)) {
		errorf("sprd_chg: sprdbat Fail to read address[%d]\n", address);
		return -1;
	}

	return 0;
}

static int sprdbat_write_miscdata_data(char *in, unsigned int address, unsigned int len)
{
	if (0 != common_raw_write("miscdata", len, (uint64_t)0,
				  (uint64_t)(MISCDATA_ADDRESS_BASE + address), in)) {
		errorf("sprd_chg: Fail to set address[%d] to %s\n", address, in);
		return -1;
	}

	return 0;
}

static void sprdbat_reset_miscdata_parameters(char *out, unsigned int address, unsigned int len)
{
	int ret;

	if (address != 0 && len != 0) {
		ret = sprdbat_write_miscdata_data(out, address, len);
		if (ret)
			dprintf(INFO,"sprd_chg: sprdbat fail to reset address = %d, len = %d\n",
				address, len);
	}
}

static void sprdbat_reset_auto_poweron_flag(void)
{
	char autopoweron_flag = '1';

	/* reset auto poweron flag to zero */
	sprdbat_reset_miscdata_parameters((char *)(&autopoweron_flag),
					  MISCDATA_AUTO_POWERON_FLAG_OFFSET,
					  MISCDATA_AUTO_POWERON_FLAG_SIZE);
}

static void sprdbat_reset_shutdown_charge_flag(void)
{
	u32 charging_flag = 0;

	/* reset charging flag to zero */
	sprdbat_reset_miscdata_parameters((char *)(&charging_flag),
					  MISCDATA_SHUTDOWN_CHARGE_FLAG_OFFSET,
					  MISCDATA_SHUTDOWN_CHARGE_FLAG_SIZE);
}

static void sprdbat_reset_shutdown_rtc_time(void)
{
	s64 rtc_time = -1;

	rtc_time = sprd_rtc_get_sec();
	dprintf(INFO,"sprd_chg: sprdbat reset rtc time = %lld\n", rtc_time);
	sprdbat_reset_miscdata_parameters((char *)(&rtc_time),
					  MISCDATA_RTC_TIME_OFFSET,
					  MISCDATA_RTC_TIME_SIZE);
}

static void sprdbat_reset_miscdata(void)
{
	int magic_num = SPRD_BATTERY_MAGIC_NUM;
	int charge_cycle = -1;
	int basp = -1;
	int total_mah = -1;
	int check_mah = -1;
	int ret;

	/* reset magic number to 0x5a5aa5a5 */
	if (MISCDATA_MAGIC_OFFSET == 0 && MISCDATA_MAGIC_SIZE != 0) {
		ret = sprdbat_write_miscdata_data((char *)(&magic_num),
						  MISCDATA_MAGIC_OFFSET,
						  MISCDATA_MAGIC_SIZE);
		if (ret != 0)
			errorf("sprd_chg: sprdbat Fail to reset charge magic num\n");
	}

	/* reset rtc to current time */
	sprdbat_reset_shutdown_rtc_time();

	/* reset charge cycle to zero */
	sprdbat_reset_miscdata_parameters((char *)(&charge_cycle),
					  MISCDATA_CHARGE_CYCLE_OFFSET,
					  MISCDATA_CHARGE_CYCLE_SIZE);

	/* reset basp to zero */
	sprdbat_reset_miscdata_parameters((char *)(&basp),
					  MISCDATA_BASP_OFFSET,
					  MISCDATA_BASP_SIZE);

	/* reset total_mah to zero */
	sprdbat_reset_miscdata_parameters((char *)(&total_mah),
					  MISCDATA_CAPACITY_OFFSET,
					  MISCDATA_CAPACITY_SIZE);

	/* reset check_mah to zero */
	sprdbat_reset_miscdata_parameters((char *)(&check_mah),
					  MISCDATA_CAPACITY_CHECK_OFFSET,
					  MISCDATA_CAPACITY_CHECK_SIZE);

	/* reset charging flag to zero */
	sprdbat_reset_shutdown_charge_flag();
}

static void sprdbat_miscdata_init(void)
{
	int magic_num = -1, ret;

	reset_miscdata_flag = false;

	if (MISCDATA_ADDRESS_BASE == 0)
		return;

	if (MISCDATA_MAGIC_OFFSET == 0 && MISCDATA_MAGIC_SIZE != 0) {
		ret = sprdbat_read_miscdata_data((char *)(&magic_num),
						 MISCDATA_MAGIC_OFFSET,
						 MISCDATA_MAGIC_SIZE);
		if (ret != 0 || magic_num != SPRD_BATTERY_MAGIC_NUM) {
			errorf("sprd_chg: sprdbat fail to read charge magic num = %d\n",
			       magic_num);
			sprdbat_reset_auto_poweron_flag();
			goto out;
		}
	}

	if (charge_first_power_on) {
		dprintf(INFO,"sprd_chg: sprdbat first power on, reset miscdata\n");
		goto out;
	}

	return;

out:
	sprdbat_reset_miscdata();
	reset_miscdata_flag = true;
	return;
}

void sprdbat_init(void)
{
	boot_mode_t boot_role;
	unsigned int keep_charge, battery_connected;
	chipram_env_t* cr_env = get_chipram_env();

	boot_role = cr_env->mode;
	keep_charge = cr_env->keep_charge;

	sprdbat_help_init();
	sprdfgu_init();
	if (!sprd_chg_ic_op) {
		dprintf(ALWAYS,"sprd_chg: ERR!!!:chg op is NULL....\n");
		dprintf(ALWAYS,"sprd_chg: ERR!!!:chg op is NULL....\n");
		dprintf(ALWAYS,"sprd_chg: ERR!!!:chg op is NULL....\n");
		dprintf(ALWAYS,"sprd_chg: ERR!!!:chg op is NULL....\n");
		dprintf(ALWAYS,"sprd_chg: ERR!!!:chg op is NULL....\n");
		return;
	}

	ntc_vol_buff[0] = -500;

	battery_connected = sprdbat_is_battery_connected(1);
	if(boot_role == BOOTLOADER_MODE_DOWNLOAD) {
		dprintf(ALWAYS,"sprd_chg: %s keep_charge = %d\n", __func__, keep_charge);
		limit_cur = IBUS_CUR_1000;
		chg_cur = IBAT_CUR_500;
		sprd_chg_ic_op->chg_cmd(CHG_SET_LIMIT_CURRENT, limit_cur);
		sprd_chg_ic_op->chg_cmd(CHG_SET_CURRENT, chg_cur);
		if(!keep_charge) {
			sprd_chg_ic_op->chg_stop();
			sprd_chg_ic_op->chg_cmd(CHG_SET_POWER_PATH, POWER_PATH_DISABLE);
			return;
		}

		if (battery_connected) {
			sprd_chg_ic_op->chg_start();
			return;
		}

		//if battery do not connect and support power_path, stop charge//
		if (sprd_chg_ic_op->is_support_power_path && sprd_chg_ic_op->is_support_power_path()) {
			sprd_chg_ic_op->chg_stop();
			sprd_chg_ic_op->chg_cmd(CHG_SET_POWER_PATH, POWER_PATH_ENABLE);
		}
		return;
	}

	if (charger_connected()) {
		int temp_status = sprdbat_get_battery_temp_status();
		enum sprd_adapter_type charger;

		limit_cur = IBUS_CUR_1000;
		chg_cur = IBAT_CUR_500;

		charger = sprdchg_charger_is_adapter();
		if (charger == ADP_TYPE_DCP) {
			#ifdef DCP_LIMIT_CUR
				limit_cur = DCP_LIMIT_CUR;
			#endif

			#ifdef DCP_CHG_CUR
				chg_cur = DCP_CHG_CUR;
			#endif
		}

		dprintf(INFO,"sprd_chg: chg_cur:%d,limit_cur:%d\n",chg_cur,limit_cur);

		sprd_chg_ic_op->chg_cmd(CHG_SET_LIMIT_CURRENT, limit_cur);
		sprd_chg_ic_op->chg_cmd(CHG_SET_CURRENT, chg_cur);
		if (temp_status != NORMAL_TEMP) {
			dprintf(INFO,"sprd_chg: sprdbat_init abnormal temp and stop charging,temp_status:%d\n",
                                temp_status);
			sprd_chg_ic_op->chg_stop();
		} else {
			sprd_chg_ic_op->chg_start();
		}
	}

	//if battery do NOT connect, shutdown charge,maybe system poweroff
	if (!battery_connected) {
		dprintf(ALWAYS,"sprd_chg: battery unconnected shutdown charge!!!!!\n");
		sprd_chg_ic_op->chg_stop();
	}

	sprdfgu_late_init();
	sprdbat_miscdata_init();
}

int sprdbat_get_auto_poweron_flag(void)
{
	int auto_poweron_flag = 1;
	char flag;

	if ((!MISCDATA_ADDRESS_BASE) || (!MISCDATA_AUTO_POWERON_FLAG_OFFSET) ||
	    (!MISCDATA_AUTO_POWERON_FLAG_SIZE)) {
		dprintf(INFO,"sprd_chg: the function is not defined and enters the auto poweron!\n");
		return auto_poweron_flag;
	}

	sprdbat_read_miscdata_data((char *)(&flag),
				   MISCDATA_AUTO_POWERON_FLAG_OFFSET,
				   MISCDATA_AUTO_POWERON_FLAG_SIZE);
	if (flag == '0')
		auto_poweron_flag = 0;
	else if (flag == '1')
		auto_poweron_flag = 1;

	dprintf(INFO,"sprd_chg: %s miscdata flag=%c, auto_poweron_flag=%d\n",
		__func__, flag, auto_poweron_flag);

	return auto_poweron_flag;
}

int sprdbat_get_shutdown_charge_flag(void)
{
	int flag = 1;

	if ((!MISCDATA_ADDRESS_BASE) || (!MISCDATA_SHUTDOWN_CHARGE_FLAG_OFFSET) ||
	    (!MISCDATA_SHUTDOWN_CHARGE_FLAG_SIZE)) {
		dprintf(INFO,"sprd_chg: the function is not defined and enters the shutdown charging mode!\n");
		return flag;
	}
	sprdbat_read_miscdata_data((char *)(&flag),
				   MISCDATA_SHUTDOWN_CHARGE_FLAG_OFFSET,
				   MISCDATA_SHUTDOWN_CHARGE_FLAG_SIZE);
	sprdbat_reset_shutdown_charge_flag();

	return flag;
}

static void sprdbat_format_cmdline(u8 *fdt, char *buf)
{
	int ret;

	ret = fdt_chosen_bootargs_append(fdt, buf, 1);
	if (ret < 0)
		dprintf(INFO,"sprd_chg: sprdbat Fail to append shutdown_rtc_time to bootargs\n");
}

int fdt_fixup_charger_parameters(u8 *fdt)
{
	char buf[256] = {0};
	int charge_cycle = -1;
	s64 rtc_time = -1;
	int magic_num = -1;
	int basp = -1;
	int total_mah = -1;
	int check_mah = -1;
	int mah = -1;
	int charge_stop_flag = 48;
	int batid;
	int ret;

	if (MISCDATA_ADDRESS_BASE == 0)
		goto format_cmdline_batid;

	memset(buf, '\0', sizeof(buf));

	if (reset_miscdata_flag) {
		rtc_time = -1;
		charge_cycle = -1;
		basp = -1;
		total_mah = -1;
		charge_stop_flag = 48;
		goto format_cmdline;
	}

	/* Read shutdown rtc time */
	if (MISCDATA_RTC_TIME_OFFSET != 0 && MISCDATA_RTC_TIME_SIZE != 0) {
		ret = sprdbat_read_miscdata_data((char *)(&rtc_time),
						 MISCDATA_RTC_TIME_OFFSET,
						 MISCDATA_RTC_TIME_SIZE);
		if (ret) {
			errorf("sprd_chg: sprdbat Fail to read rtc time\n");
			rtc_time = -1;
		}
	}

	/* Read charge cycle */
	if (MISCDATA_CHARGE_CYCLE_OFFSET != 0 && MISCDATA_CHARGE_CYCLE_SIZE != 0) {
		ret = sprdbat_read_miscdata_data((char *)(&charge_cycle),
						 MISCDATA_CHARGE_CYCLE_OFFSET,
						 MISCDATA_CHARGE_CYCLE_SIZE);
		if (ret) {
			errorf("sprd_chg: sprdbat Fail to read charge_cycle\n");
			charge_cycle = -1;
		}
	}

	/* Read BASP */
	if (MISCDATA_BASP_OFFSET != 0 && MISCDATA_BASP_SIZE != 0) {
		ret = sprdbat_read_miscdata_data((char *)(&basp),
						 MISCDATA_BASP_OFFSET,
						 MISCDATA_BASP_SIZE);
		if (ret) {
			errorf("sprd_chg: sprdbat Fail to read basp\n");
			basp = -1;
		}
	}

	/* Read Total mah */
	if (MISCDATA_CAPACITY_OFFSET != 0 && MISCDATA_CAPACITY_SIZE != 0) {
		ret = sprdbat_read_miscdata_data((char *)(&total_mah),
						 MISCDATA_CAPACITY_OFFSET,
						 MISCDATA_CAPACITY_SIZE);
		if (ret) {
			errorf("sprd_chg: sprdbat Fail to read total_mah\n");
			total_mah = -1;
		}
	}

	/* Read Check mah */
	if (MISCDATA_CAPACITY_CHECK_OFFSET != 0 && MISCDATA_CAPACITY_CHECK_SIZE != 0) {
		ret = sprdbat_read_miscdata_data((char *)(&check_mah),
						 MISCDATA_CAPACITY_CHECK_OFFSET,
						 MISCDATA_CAPACITY_CHECK_SIZE);
		if (ret) {
			errorf("sprd_chg: sprdbat Fail to read check_mah\n");
			check_mah = -1;
		}
	}

	/* Read Shutdown charge 80% capacity flag*/
	if (MISCDATA_SHUTDOWN_CHARGE_EIGHTY_PERCENTAGE_STOP_CHARGE_OFFSET != 0 && MISCDATA_SHUTDOWN_CHARGE_EIGHTY_PERCENTAGE_STOP_CHARGE_SIZE != 0) {
		ret = sprdbat_read_miscdata_data((char *)(&charge_stop_flag),
						 MISCDATA_SHUTDOWN_CHARGE_EIGHTY_PERCENTAGE_STOP_CHARGE_OFFSET,
						 MISCDATA_SHUTDOWN_CHARGE_EIGHTY_PERCENTAGE_STOP_CHARGE_SIZE);
		if (ret) {
			errorf("sprd_chg: sprdbat Fail to read charge_stop_flag\n");
			charge_stop_flag = -1;
		}
	}

format_cmdline:
	if (total_mah != -1 && check_mah != -1) {
		total_mah = total_mah ^ CAPACITY_TRACK_CAP_KEY0;
		check_mah = check_mah ^ CAPACITY_TRACK_CAP_KEY1;

		if (total_mah == check_mah)
			mah = total_mah;
		else
			dprintf(INFO,"sprd_chg: sprdbat total_mah != check_mah, total_mah = %d, check_mah = %d\n",
			       total_mah, check_mah);
	}

	dprintf(INFO,"sprd_chg: sprdbat rtc_time = %lld, charge_cycle = %d, charge magic num = 0x%x, "
	       "basp = %d, total_mah = %d charge_stop_flag = %d\n",
	       rtc_time, charge_cycle, magic_num, basp, mah, charge_stop_flag);

	/* Parse shutdown rtc time to kernel */
	snprintf(buf, sizeof(buf), "charge.shutdown_rtc_time=%lld ", rtc_time);
	sprdbat_format_cmdline(fdt, buf);

	/* Parse shutdown charge cycle to kernel */
	memset(buf, '\0', sizeof(buf));
	snprintf(buf, sizeof(buf), "charge.charge_cycle=%d ", charge_cycle);
	sprdbat_format_cmdline(fdt, buf);

	/* Parse basp to kernel */
	memset(buf, '\0', sizeof(buf));
	snprintf(buf, sizeof(buf), "charge.basp=%d ", basp);
	sprdbat_format_cmdline(fdt, buf);

	/* Parse capacity to kernel */
	memset(buf, '\0', sizeof(buf));
	snprintf(buf, sizeof(buf), "charge.total_mah=%d ", mah);
	sprdbat_format_cmdline(fdt, buf);

	/* Parse capacity to kernel */
	memset(buf, '\0', sizeof(buf));
	snprintf(buf, sizeof(buf), "charge.shutdown_cap_limit=%d ", charge_stop_flag);
	sprdbat_format_cmdline(fdt, buf);

	/* reset rtc to current time */
	sprdbat_reset_shutdown_rtc_time();

format_cmdline_batid:
	if (BAT_ID) {
		/* Parse bat id to kernel */
		memset(buf, '\0', sizeof(buf));
		batid = sprd_get_batid();
		snprintf(buf, sizeof(buf), "bat.id=%d", batid);
		sprdbat_format_cmdline(fdt, buf);
	}

	return 0;
}
//ZOVERLAY_TAG_HMD_ONEIMAGE
