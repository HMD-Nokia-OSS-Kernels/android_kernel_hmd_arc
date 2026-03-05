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
//ZOVERLAY_TAG_HMD_ONEIMAGE
#ifndef __SPRD_BOARDID_H__
#define __SPRD_BOARDID_H__

int sprd_get_bandinfo(void);
int sprd_get_crystal(void);
int sprd_get_32k(void);
int sprd_get_boardid(void);
int sprd_get_wcn_crystal(void);
int sprd_get_wifi_mode(void);
int sprd_get_data_mode(void);
char sprd_get_sim(void);
int sprd_get_power_mode(void);
#ifdef CONFIG_MUTI_DCDC
int sprd_get_dcdc_muti(void);
#endif
int sprd_get_rf_band_type(void);
int sprd_get_uob_boardid(void);
#ifdef CONFIG_WCN_BOARD_ID
int sprd_get_wcn_boardid(void);
#endif
int sprd_get_pcb_version(void);

#endif
