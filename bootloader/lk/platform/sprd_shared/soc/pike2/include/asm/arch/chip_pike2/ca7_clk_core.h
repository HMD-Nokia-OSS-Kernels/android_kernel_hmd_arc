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

#ifndef __CA7_CLK_CORE_H____
#define __CA7_CLK_CORE_H____

/* Some defs, in case these are not defined elsewhere */
#ifndef SCI_IOMAP
#define SCI_IOMAP(_b_) ( (_b_) )
#endif

#ifndef SCI_ADDR
#define SCI_ADDR(_b_, _o_) ( (_b_) + (_o_) )
#endif

#ifndef CTL_CA7_CLK_CORE_BASE
#define CTL_CA7_CLK_CORE_BASE           SCI_IOMAP(0x20E00000)
#endif

/* registers definitions for CTL_CA7_CLK_CORE, 0x20E00000 */
#define REG_CA7_CLK_CORE_CGM_CA7_MCU_CFG                    SCI_ADDR(CTL_CA7_CLK_CORE_BASE, 0x0020)
#define REG_CA7_CLK_CORE_CGM_CA7_CORE_CFG                   SCI_ADDR(CTL_CA7_CLK_CORE_BASE, 0x0024)
#define REG_CA7_CLK_CORE_CGM_CA7_AXI_CFG                    SCI_ADDR(CTL_CA7_CLK_CORE_BASE, 0x0028)
#define REG_CA7_CLK_CORE_CGM_CA7_DBG_CFG                    SCI_ADDR(CTL_CA7_CLK_CORE_BASE, 0x002C)
#define REG_CA7_CLK_CORE_CGM_AXI_EMC_CFG                    SCI_ADDR(CTL_CA7_CLK_CORE_BASE, 0x0030)

/* bits definitions for REG_CA7_CLK_CORE_CGM_CA7_MCU_CFG, [0x20E00020] */
#define BIT_CA7_CLK_CORE_CGM_CA7_MCU_CFG_CGM_CA7_MCU_SEL(x)                 ( (x) << 0  & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for REG_CA7_CLK_CORE_CGM_CA7_CORE_CFG, [0x20E00024] */
#define BIT_CA7_CLK_CORE_CGM_CA7_CORE_CFG_CGM_CA7_CORE_SEL                    ( BIT(0) )

/* bits definitions for REG_CA7_CLK_CORE_CGM_CA7_AXI_CFG, [0x20E00028] */
#define BIT_CA7_CLK_CORE_CGM_CA7_AXI_CFG_CGM_CA7_AXI_SEL                      ( BIT(0) )

/* bits definitions for REG_CA7_CLK_CORE_CGM_CA7_DBG_CFG, [0x20E0002C] */
#define BIT_CA7_CLK_CORE_CGM_CA7_DBG_CFG_CGM_CA7_DBG_SEL                      ( BIT(0) )

/* bits definitions for REG_CA7_CLK_CORE_CGM_AXI_EMC_CFG, [0x20E00030] */

/* vars definitions for controller CTL_CA7_CLK_CORE */


#endif /* __CA7_CLK_CORE_H____ */
