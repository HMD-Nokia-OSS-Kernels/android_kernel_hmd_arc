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

#ifndef _REDEFINE_H
#define _REDEFINE_H

/* macro definition for compatible */

/* bit redefinition for compatible */
#define BIT_EFUSE_EB                            BIT_AON_APB_EFUSE_EB
#define BIT_AON_APB_EFUSE_NORMAL_EB             BIT_AON_APB_EFUSE_EB
#define BIT_EFUSE_SOFT_RST                      BIT_AON_APB_EFUSE_SOFT_RST
#define BIT_GPIO_EB                             BIT_AON_APB_GPIO_EB
#define BIT_EIC_EB                              BIT_AON_APB_EIC_EB
#define BIT_EIC_RTC_EB                          BIT_AON_APB_EIC_RTC_EB
#define BIT_EIC_RTCDV5_EB                       BIT_AON_APB_EIC_RTCDV5_EB

/* there is no uart0 on sharkle, just tricky */
#define BIT_UART0_EB                            BIT_AP_APB_UART1_EB
#define BIT_UART1_EB                            BIT_AP_APB_UART1_EB
#define BIT_USB_EB                              BIT_AP_AHB_OTG_EB
#define BIT_OTG_SOFT_RST                        BIT_AP_AHB_OTG_SOFT_RST
#define BIT_OTG_UTMI_SOFT_RST                   BIT_AP_AHB_OTG_UTMI_SOFT_RST
#define BIT_OTG_PHY_SOFT_RST                    BIT(6)
#define BIT_I2C_EB                              BIT_AP_APB_I2C0_EB
#define BIT_ADI_EB                              BIT_AON_APB_ADI_EB
#define REG_ADI_EB				REG_AON_APB_APB_EB0

/* for CE */
#define REG_AP_AHB_CE_SEC_EB                                           REG_AP_AHB_AHB_EB
#define BIT_AP_AHB_CE_SEC_EB                                           BIT_AP_AHB_CE_EB
#define REG_AP_AHB_CE_SEC_SOFT_RST                                     REG_AP_AHB_AHB_RST

#endif
