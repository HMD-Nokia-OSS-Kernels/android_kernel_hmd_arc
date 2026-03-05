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

/* bit redefinition for compatible */
#define BIT_EFUSE_EB                            BIT_AON_APB_REE_EFUSE_EB
#define BIT_EFUSE_SOFT_RST                      BIT_AON_APB_EFUSE_SOFT_RST
#define BIT_GPIO_EB                             BIT_AON_APB_GPIO_EB
#define BIT_EIC_EB                              BIT_AON_APB_EIC_EB
#define BIT_EIC_RTC_EB                          BIT_AON_APB_EIC_RTC_EB
#define BIT_EIC_RTCDV5_EB                       BIT_AON_APB_EIC_RTCDV5_EB
#define BIT_UART0_EB                            BIT_AP_APB_UART0_EB
#define BIT_UART1_EB                            BIT_AP_APB_UART1_EB
#define BIT_USB_EB                              BIT_AP_AHB_OTG_EB
#define BIT_OTG_SOFT_RST                        BIT_AP_AHB_OTG_SOFT_RST
#define BIT_OTG_UTMI_SOFT_RST                   BIT_AP_AHB_OTG_UTMI_SOFT_RST
#define BIT_OTG_PHY_SOFT_RST                    BIT_AP_AHB_OTG_PHY_SOFT_RST
#define BIT_PD_MM_TOP_FORCE_SHUTDOWN            BIT_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN
#define BIT_PD_GPU_TOP_FORCE_SHUTDOWN           BIT_PMU_APB_PD_GPU_TOP_FORCE_SHUTDOWN
#define BIT_MM_EB                               BIT_AON_APB_MM_EB
#define BIT_CKG_EB                              BIT_MM_AHB_RF_CKG_EB
#define BIT_GPU_EB                              BIT_AON_APB_AP_TMR0_EB
#define BIT_ADI_EB                              BIT_AON_APB_ADI_EB

#define BIT_INTC_EB                             BIT_AON_APB_INTC_EB
#define BIT_INTC0_EB                            BIT_AP_APB_INTC0_EB
#define BIT_INTC1_EB                            BIT_AP_APB_INTC1_EB
#define BIT_INTC2_EB                            BIT_AP_APB_INTC2_EB
#define BIT_INTC3_EB                            BIT_AP_APB_INTC3_EB

/* register redefiniton for compatible */
#define REG_PMU_APB_CP_SOFT_RST                 REG_PMU_APB_SOFT_RST
#define REG_ADI_EB				REG_AON_APB_APB_EB0

#define REG_AP_CLK_CORE_CGM_CE_CFG 		REG_AP_CLK_CE_CFG
#endif
