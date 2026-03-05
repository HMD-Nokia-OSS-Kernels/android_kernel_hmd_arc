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

#ifndef MM_CLK_CORE_H
#define MM_CLK_CORE_H

#define CTL_BASE_MM_CLK_CORE 0x60900000


#define REG_MM_CLK_CORE_CGM_MIPI_CSI_CFG    ( CTL_BASE_MM_CLK_CORE + 0x0020 )
#define REG_MM_CLK_CORE_CGM_MIPI_CSI_S_CFG  ( CTL_BASE_MM_CLK_CORE + 0x0024 )
#define REG_MM_CLK_CORE_CGM_MIPI_CSI_T_CFG  ( CTL_BASE_MM_CLK_CORE + 0x0028 )

/* REG_MM_CLK_CORE_CGM_MIPI_CSI_CFG */

#define BIT_MM_CLK_CORE_CGM_MIPI_CSI_CFG_CGM_MIPI_CSI_PAD_SEL      BIT(16)
#define BIT_MM_CLK_CORE_CGM_MIPI_CSI_CFG_CGM_MIPI_CSI_SEL          BIT(0)

/* REG_MM_CLK_CORE_CGM_MIPI_CSI_S_CFG */

#define BIT_MM_CLK_CORE_CGM_MIPI_CSI_S_CFG_CGM_MIPI_CSI_S_PAD_SEL  BIT(16)
#define BIT_MM_CLK_CORE_CGM_MIPI_CSI_S_CFG_CGM_MIPI_CSI_S_SEL      BIT(0)

/* REG_MM_CLK_CORE_CGM_MIPI_CSI_T_CFG */

#define BIT_MM_CLK_CORE_CGM_MIPI_CSI_T_CFG_CGM_MIPI_CSI_T_PAD_SEL  BIT(16)
#define BIT_MM_CLK_CORE_CGM_MIPI_CSI_T_CFG_CGM_MIPI_CSI_T_SEL      BIT(0)


#endif /* MM_CLK_CORE_H */

