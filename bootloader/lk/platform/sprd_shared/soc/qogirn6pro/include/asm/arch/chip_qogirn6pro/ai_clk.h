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

#ifndef __AI_CLK_H____
#define __AI_CLK_H____

/* Some defs, in case these are not defined elsewhere */
#ifndef SCI_IOMAP
#define SCI_IOMAP(_b_) ( (_b_) )
#endif

#ifndef SCI_ADDR
#define SCI_ADDR(_b_, _o_) ( (_b_) + (_o_) )
#endif

#ifndef CTL_AI_CLK_BASE
#define CTL_AI_CLK_BASE                 SCI_IOMAP(0x27004000)
#endif

/* registers definitions for CTL_AI_CLK, 0x27004000 */
#define REG_AI_CLK_CGM_POWERVR_DIV_CFG                      SCI_ADDR(CTL_AI_CLK_BASE, 0x0024)
#define REG_AI_CLK_CGM_POWERVR_SEL_CFG                      SCI_ADDR(CTL_AI_CLK_BASE, 0x0028)
#define REG_AI_CLK_CGM_MAIN_MTX_DIV_CFG                     SCI_ADDR(CTL_AI_CLK_BASE, 0x0030)
#define REG_AI_CLK_CGM_MAIN_MTX_SEL_CFG                     SCI_ADDR(CTL_AI_CLK_BASE, 0x0034)
#define REG_AI_CLK_CGM_CFG_MTX_DIV_CFG                      SCI_ADDR(CTL_AI_CLK_BASE, 0x003C)
#define REG_AI_CLK_CGM_CFG_MTX_SEL_CFG                      SCI_ADDR(CTL_AI_CLK_BASE, 0x0040)
#define REG_AI_CLK_CGM_OCM_DIV_CFG                          SCI_ADDR(CTL_AI_CLK_BASE, 0x0048)
#define REG_AI_CLK_CGM_OCM_SEL_CFG                          SCI_ADDR(CTL_AI_CLK_BASE, 0x004C)
#define REG_AI_CLK_CGM_DVFS_SEL_CFG                         SCI_ADDR(CTL_AI_CLK_BASE, 0x0058)

/* bits definitions for REG_AI_CLK_CGM_POWERVR_DIV_CFG, [0x27004024] */

/* bits definitions for REG_AI_CLK_CGM_POWERVR_SEL_CFG, [0x27004028] */

/* bits definitions for REG_AI_CLK_CGM_MAIN_MTX_DIV_CFG, [0x27004030] */

/* bits definitions for REG_AI_CLK_CGM_MAIN_MTX_SEL_CFG, [0x27004034] */

/* bits definitions for REG_AI_CLK_CGM_CFG_MTX_DIV_CFG, [0x2700403C] */
#define BIT_AI_CLK_CGM_CFG_MTX_DIV(x)                       ( (x) << 0  & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for REG_AI_CLK_CGM_CFG_MTX_SEL_CFG, [0x27004040] */
#define BIT_AI_CLK_CGM_CFG_MTX_SEL(x)                       ( (x) << 0  & (BIT(0)|BIT(1)) )

/* bits definitions for REG_AI_CLK_CGM_OCM_DIV_CFG, [0x27004048] */

/* bits definitions for REG_AI_CLK_CGM_OCM_SEL_CFG, [0x2700404C] */

/* bits definitions for REG_AI_CLK_CGM_DVFS_SEL_CFG, [0x27004058] */
#define BIT_AI_CLK_CGM_DVFS_SEL(x)                          ( (x) << 0  & (BIT(0)|BIT(1)) )

/* vars definitions for controller CTL_AI_CLK */


#endif /* __AI_CLK_H____ */
