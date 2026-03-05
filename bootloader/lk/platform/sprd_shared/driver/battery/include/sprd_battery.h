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

#ifndef _SPRD_BATTERY_H_
#define _SPRD_BATTERY_H_
#include <stdint.h>

#define SPRDBAT_CHG_POLLING_T	200
#define SPRD_CHGDET_DELAY_US	20000
#define SPRD_CHGDET_CNT		100

enum sprd_adapter_type {
	ADP_TYPE_UNKNOW = 0,	//unknow adapter type
	ADP_TYPE_CDP = 1,	//Charging Downstream Port,USB&standard charger
	ADP_TYPE_DCP = 2,	//Dedicated Charging Port, standard charger
	ADP_TYPE_SDP = 4,	//Standard Downstream Port,USB and nonstandard charge
};

enum temp_status {
	NORMAL_TEMP = 0,
	DISCHARGING_TEMP = 1,
	DISBOOTING_TEMP = 2,
};

int sprdchg_charger_is_adapter(void);
uint16_t sprdbat_auxadc2vbatvol(uint16_t adcvalue);
uint32_t sprdbat_get_vbatauxadc_caltype(void);
void sprdbat_get_vbatauxadc_caldata(void);
int charger_connected(void);
int get_mode_from_vchg(void);
int get_mode_from_gpio(void);
void sprdbat_init(void);
int sprdbat_get_battery_temp_status(void);
void sprdbat_lowbat_chg(int abnormal_temp_flag);
int sprdbat_is_battery_connected(int log_flag);
void sprdbat_show_chglogo(void);
void sprdbat_show_lowpower_charge_logo(int chg_logo_time);
void sprdbat_chg_led(int on);
uint32_t sprdbat_get_aux_vbatvol(void);
void sprdchg_common_cfg(void);
uint32_t sprdfgu_read_vbat_vol(void);
int sprdfgu_read_ibat_cur(void);

/* ext chg ic init function*/
void sprdchg_fan54015_init(void);
void sprdchg_aw32257_init(void);
void sprdchg_eta6937_init(void);
void sprdchg_bq25896_init(void);
void sprdchg_bq2560x_init(void);
void sprdchg_sc8960x_init(void);
void sprdchg_sgm41511_init(void);
void sprdchg_sgm41516_init(void);
void sprdchg_rt9471_init(void);
void sprdchg_bq25890_init(void);
void sprdchg_sgm4154x_init(void);
void sprdchg_sy6970_init(void);
void sprdchg_2700_init(void);
void sprdchg_2701_init(void);
void sprdchg_2705_init(void);
void sprdchg_2703_init(void);
void sprdchg_2731_init(void);
void sprdchg_2721_init(void);
void sprdchg_2723_init(void);
void sprdchg_2720_init(void);
void sprdchg_upm6920_init(void);

#endif

