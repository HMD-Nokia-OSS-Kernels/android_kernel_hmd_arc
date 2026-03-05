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

#ifndef __CAMERASYS_GLB_H____
#define __CAMERASYS_GLB_H____

/* Some defs, in case these are not defined elsewhere */
#ifndef SCI_IOMAP
#define SCI_IOMAP(_b_) ( (_b_) )
#endif

#ifndef SCI_ADDR
#define SCI_ADDR(_b_, _o_) ( (_b_) + (_o_) )
#endif

#ifndef CTL_CAMERASYS_GLB_BASE
#define CTL_CAMERASYS_GLB_BASE          SCI_IOMAP(0x30000000)
#endif

/* registers definitions for CTL_CAMERASYS_GLB, 0x30000000 */
#define REG_CAMERASYS_GLB_MM_SYS_EN                                             SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x0000)
#define REG_CAMERASYS_GLB_ISP_BLK_EN                                            SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x0004)
#define REG_CAMERASYS_GLB_DCAM_BLK_EN                                           SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x000C)
#define REG_CAMERASYS_GLB_MM_QOS                                                SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x0010)
#define REG_CAMERASYS_GLB_MM_LP_DISABLE                                         SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x0014)
#define REG_CAMERASYS_GLB_MM_0P5_APPEND                                         SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x001C)
#define REG_CAMERASYS_GLB_MM_IP_BUSY0                                           SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x0020)
#define REG_CAMERASYS_GLB_MM_IP_BUSY1                                           SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x0024)
#define REG_CAMERASYS_GLB_MM_AS_BDG_STATE                                       SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x0028)
#define REG_CAMERASYS_GLB_MM_SYS_SYS_M0_LPC_CTRL                                SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x003C)
#define REG_CAMERASYS_GLB_MM_SYS_SYS_M1_LPC_CTRL                                SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x0044)
#define REG_CAMERASYS_GLB_MM_SYS_SYS_M2_LPC_CTRL                                SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x0048)
#define REG_CAMERASYS_GLB_MM_SYS_SYS_S0_LPC_CTRL                                SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x004C)
#define REG_CAMERASYS_GLB_MM_SYS_SYS_S1_LPC_CTRL                                SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x0050)
#define REG_CAMERASYS_GLB_MM_SYS_ASB_HB_LPC_CTRL                                SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x0054)
#define REG_CAMERASYS_GLB_MM_SYS_ASB_LL_LPC_CTRL                                SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x0058)
#define REG_CAMERASYS_GLB_MM_ISP_BLK_M0_LPC_CTRL                                SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x005C)
#define REG_CAMERASYS_GLB_MM_ISP_BLK_M1_LPC_CTRL                                SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x0060)
#define REG_CAMERASYS_GLB_MM_ISP_BLK_M2_LPC_CTRL                                SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x0068)
#define REG_CAMERASYS_GLB_MM_ISP_BLK_M4_LPC_CTRL                                SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x006C)
#define REG_CAMERASYS_GLB_MM_ISP_BLK_S0_LPC_CTRL                                SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x0070)
#define REG_CAMERASYS_GLB_MM_DCAM_BLK_M0_LPC_CTRL                               SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x0074)
#define REG_CAMERASYS_GLB_MM_DCAM_BLK_M1_LPC_CTRL                               SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x0078)
#define REG_CAMERASYS_GLB_MM_DCAM_BLK_S0_LPC_CTRL                               SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x007C)
#define REG_CAMERASYS_GLB_MIPI_PHY_SEL                                          SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x00A8)
#define REG_CAMERASYS_GLB_USER_GATE_FORCE_OFF0                                  SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x00AC)
#define REG_CAMERASYS_GLB_USER_GATE_FORCE_OFF1                                  SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x00B0)
#define REG_CAMERASYS_GLB_USER_GATE_AUTO_GATE_EN0                               SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x00B4)
#define REG_CAMERASYS_GLB_USER_GATE_AUTO_GATE_EN1                               SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x00B8)
#define REG_CAMERASYS_GLB_DCAM_BLK_SOFT_RST                                     SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x00C8)
#define REG_CAMERASYS_GLB_ISP_BLK_SOFT_RST                                      SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x00CC)
#define REG_CAMERASYS_GLB_SYS_SOFT_RST                                          SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x00D0)
#define REG_CAMERASYS_GLB_SYS_LGT_STOP_MASK                                     SCI_ADDR(CTL_CAMERASYS_GLB_BASE, 0x00D8)

/* bits definitions for REG_CAMERASYS_GLB_MM_SYS_EN, [0x30000000] */
#define BIT_CAMERASYS_GLB_MM_MTX_DATA_EN                         BIT(8)
#define BIT_CAMERASYS_GLB_SYS_TCK_EN                             BIT(7)
#define BIT_CAMERASYS_GLB_SYS_MST_BUSMON_EN                      BIT(6)
#define BIT_CAMERASYS_GLB_SYS_CFG_MTX_BUSMON_EN                  BIT(5)
#define BIT_CAMERASYS_GLB_SYS_MTX_CFG_EN                         BIT(4)
#define BIT_CAMERASYS_GLB_DVFS_EN                                BIT(3)
#define BIT_CAMERASYS_GLB_CKG_EN                                 BIT(1)
#define BIT_CAMERASYS_GLB_JPG_EN                                 BIT(0)

/* bits definitions for REG_CAMERASYS_GLB_ISP_BLK_EN, [0x30000004] */
#define BIT_CAMERASYS_GLB_ISP_TCK_EN                             BIT(10)
#define BIT_CAMERASYS_GLB_ISP_BLK_MST_BUSMON_EN                  BIT(9)
#define BIT_CAMERASYS_GLB_ISP_BLK_CFG_EN                         BIT(6)
#define BIT_CAMERASYS_GLB_ISP_MTX_EN                             BIT(5)
#define BIT_CAMERASYS_GLB_CPP_EN                                 BIT(1)
#define BIT_CAMERASYS_GLB_ISP_EN                                 BIT(0)

/* bits definitions for REG_CAMERASYS_GLB_DCAM_BLK_EN, [0x3000000C] */
#define BIT_CAMERASYS_GLB_IPA_EN                                 BIT(16)
#define BIT_CAMERASYS_GLB_CSI3_EN                                BIT(15)
#define BIT_CAMERASYS_GLB_CSI2_EN                                BIT(14)
#define BIT_CAMERASYS_GLB_CSI0_EN                                BIT(12)
#define BIT_CAMERASYS_GLB_DCAM_TCK_EN                            BIT(11)
#define BIT_CAMERASYS_GLB_SENSOR3_EN                             BIT(9)
#define BIT_CAMERASYS_GLB_SENSOR2_EN                             BIT(8)
#define BIT_CAMERASYS_GLB_SENSOR0_EN                             BIT(6)
#define BIT_CAMERASYS_GLB_DCAM_BLK_CFG_EN                        BIT(5)
#define BIT_CAMERASYS_GLB_DCAM_LITE_MTX_EN                       BIT(4)
#define BIT_CAMERASYS_GLB_DCAM_MTX_EN                            BIT(3)
#define BIT_CAMERASYS_GLB_PHY_CFG_EN                             BIT(2)
#define BIT_CAMERASYS_GLB_DCAM_IF_LITE_EN                        BIT(1)
#define BIT_CAMERASYS_GLB_DCAM_IF_EN                             BIT(0)

/* bits definitions for REG_CAMERASYS_GLB_MM_QOS, [0x30000010] */
#define BIT_CAMERASYS_GLB_AR_QOS_THRESHOLD_MM(x)                 ((x) << 4  & (BIT(4) | BIT(5) | BIT(6) | BIT(7)))
#define BIT_CAMERASYS_GLB_AW_QOS_THRESHOLD_MM(x)                 ((x) << 0  & (BIT(0) | BIT(1) | BIT(2) | BIT(3)))

/* bits definitions for REG_CAMERASYS_GLB_MM_LP_DISABLE, [0x30000014] */
#define BIT_CAMERASYS_GLB_CGM_ISP_AUTO_GATE_SEL                  BIT(3)
#define BIT_CAMERASYS_GLB_CGM_DCAM_AXI_AUTO_GATE_SEL             BIT(2)
#define BIT_CAMERASYS_GLB_CGM_MM_MTX_S0_AUTO_GATE_EN             BIT(1)
#define BIT_CAMERASYS_GLB_MM_LPC_DISABLE                         BIT(0)

/* bits definitions for REG_CAMERASYS_GLB_MM_0P5_APPEND, [0x3000001C] */
#define BIT_CAMERASYS_GLB_SYS_SRST_BUSY_DCAM2_3                  BIT(9)
#define BIT_CAMERASYS_GLB_SYS_SRST_BUSY_DCAM0_1                  BIT(8)
#define BIT_CAMERASYS_GLB_SYS_SRST_BUSY_CPP                      BIT(2)
#define BIT_CAMERASYS_GLB_SYS_SRST_BUSY_ISP                      BIT(1)
#define BIT_CAMERASYS_GLB_SYS_SRST_BUSY_JPG                      BIT(0)

/* bits definitions for REG_CAMERASYS_GLB_MM_IP_BUSY0, [0x30000020] */
#define BIT_CAMERASYS_GLB_ISP_BLK_M0_CGM_BUSY_LPC                BIT(31)
#define BIT_CAMERASYS_GLB_ISP_BLK_M1_CGM_BUSY_LPC                BIT(30)
#define BIT_CAMERASYS_GLB_ISP_BLK_S0_CGM_BUSY_LPC                BIT(26)
#define BIT_CAMERASYS_GLB_SYS_M0_CGM_BUSY_LPC                    BIT(25)
#define BIT_CAMERASYS_GLB_SYS_M1_CGM_BUSY_LPC                    BIT(24)
#define BIT_CAMERASYS_GLB_SYS_M2_CGM_BUSY_LPC                    BIT(23)
#define BIT_CAMERASYS_GLB_SYS_S0_CGM_BUSY_LPC                    BIT(21)
#define BIT_CAMERASYS_GLB_SYS_S1_CGM_BUSY_LPC                    BIT(20)
#define BIT_CAMERASYS_GLB_DCAM_IF_LITE_BUSY                      BIT(4)
#define BIT_CAMERASYS_GLB_DCAM_IF_BUSY                           BIT(3)
#define BIT_CAMERASYS_GLB_ISP_BUSY                               BIT(2)
#define BIT_CAMERASYS_GLB_JPG_BUSY                               BIT(1)
#define BIT_CAMERASYS_GLB_CPP_BUSY                               BIT(0)

/* bits definitions for REG_CAMERASYS_GLB_MM_IP_BUSY1, [0x30000024] */
#define BIT_CAMERASYS_GLB_ASB_HB_CGM_BUSY_LPC                    BIT(9)
#define BIT_CAMERASYS_GLB_ASB_LL_CGM_BUSY_LPC                    BIT(8)
#define BIT_CAMERASYS_GLB_DCAM_BLK_M0_CGM_BUSY_LPC               BIT(3)
#define BIT_CAMERASYS_GLB_DCAM_BLK_M1_CGM_BUSY_LPC               BIT(2)
#define BIT_CAMERASYS_GLB_DCAM_BLK_S0_CGM_BUSY_LPC               BIT(1)

/* bits definitions for REG_CAMERASYS_GLB_MM_AS_BDG_STATE, [0x30000028] */
#define BIT_CAMERASYS_GLB_AXI_DETECTOR_OVERFLOW_LL               BIT(3)
#define BIT_CAMERASYS_GLB_AXI_DETECTOR_OVERFLOW_HB               BIT(2)
#define BIT_CAMERASYS_GLB_BRIDGE_TRANS_IDLE_LL                   BIT(1)
#define BIT_CAMERASYS_GLB_BRIDGE_TRANS_IDLE_HB                   BIT(0)

/* bits definitions for REG_CAMERASYS_GLB_MM_SYS_SYS_M0_LPC_CTRL, [0x3000003C] */
#define BIT_CAMERASYS_GLB_SYS_M0_PU_NUM(x)                       ((x) << 24 & (BIT(24) | BIT(25) | BIT(26) | BIT(27) | BIT(28) | BIT(29) | BIT(30) | BIT(31)))
#define BIT_CAMERASYS_GLB_SYS_M0_LP_EB                           BIT(16)
#define BIT_CAMERASYS_GLB_SYS_M0_LP_NUM(x)                       ((x) << 0  & (0x0000FFFF))

/* bits definitions for REG_CAMERASYS_GLB_MM_SYS_SYS_M1_LPC_CTRL, [0x30000044] */
#define BIT_CAMERASYS_GLB_SYS_M1_PU_NUM(x)                       ((x) << 24 & (BIT(24) | BIT(25) | BIT(26) | BIT(27) | BIT(28) | BIT(29) | BIT(30) | BIT(31)))
#define BIT_CAMERASYS_GLB_SYS_M1_LP_EB                           BIT(16)
#define BIT_CAMERASYS_GLB_SYS_M1_LP_NUM(x)                       ((x) << 0  & (0x0000FFFF))

/* bits definitions for REG_CAMERASYS_GLB_MM_SYS_SYS_M2_LPC_CTRL, [0x30000048] */
#define BIT_CAMERASYS_GLB_SYS_M2_PU_NUM(x)                       ((x) << 24 & (BIT(24) | BIT(25) | BIT(26) | BIT(27) | BIT(28) | BIT(29) | BIT(30) | BIT(31)))
#define BIT_CAMERASYS_GLB_SYS_M2_LP_EB                           BIT(16)
#define BIT_CAMERASYS_GLB_SYS_M2_LP_NUM(x)                       ((x) << 0  & (0x0000FFFF))

/* bits definitions for REG_CAMERASYS_GLB_MM_SYS_SYS_S0_LPC_CTRL, [0x3000004C] */
#define BIT_CAMERASYS_GLB_SYS_S0_PU_NUM(x)                       ((x) << 24 & (BIT(24) | BIT(25) | BIT(26) | BIT(27) | BIT(28) | BIT(29) | BIT(30) | BIT(31)))
#define BIT_CAMERASYS_GLB_SYS_S0_LP_EB                           BIT(16)
#define BIT_CAMERASYS_GLB_SYS_S0_LP_NUM(x)                       ((x) << 0  & (0x0000FFFF))

/* bits definitions for REG_CAMERASYS_GLB_MM_SYS_SYS_S1_LPC_CTRL, [0x30000050] */
#define BIT_CAMERASYS_GLB_SYS_S1_PU_NUM(x)                       ((x) << 24 & (BIT(24) | BIT(25) | BIT(26) | BIT(27) | BIT(28) | BIT(29) | BIT(30) | BIT(31)))
#define BIT_CAMERASYS_GLB_SYS_S1_LP_EB                           BIT(16)
#define BIT_CAMERASYS_GLB_SYS_S1_LP_NUM(x)                       ((x) << 0  & (0x0000FFFF))

/* bits definitions for REG_CAMERASYS_GLB_MM_SYS_ASB_HB_LPC_CTRL, [0x30000054] */
#define BIT_CAMERASYS_GLB_ASB_HB_PU_NUM(x)                       ((x) << 24 & (BIT(24) | BIT(25) | BIT(26) | BIT(27) | BIT(28) | BIT(29) | BIT(30) | BIT(31)))
#define BIT_CAMERASYS_GLB_ASB_HB_LP_EB                           BIT(16)
#define BIT_CAMERASYS_GLB_ASB_HB_LP_NUM(x)                       ((x) << 0  & (0x0000FFFF))

/* bits definitions for REG_CAMERASYS_GLB_MM_SYS_ASB_LL_LPC_CTRL, [0x30000058] */
#define BIT_CAMERASYS_GLB_ASB_LL_PU_NUM(x)                       ((x) << 24 & (BIT(24) | BIT(25) | BIT(26) | BIT(27) | BIT(28) | BIT(29) | BIT(30) | BIT(31)))
#define BIT_CAMERASYS_GLB_ASB_LL_LP_EB                           BIT(16)
#define BIT_CAMERASYS_GLB_ASB_LL_LP_NUM(x)                       ((x) << 0  & (0x0000FFFF))

/* bits definitions for REG_CAMERASYS_GLB_MM_ISP_BLK_M0_LPC_CTRL, [0x3000005C] */
#define BIT_CAMERASYS_GLB_ISP_BLK_M0_PU_NUM(x)                   ((x) << 24 & (BIT(24) | BIT(25) | BIT(26) | BIT(27) | BIT(28) | BIT(29) | BIT(30) | BIT(31)))
#define BIT_CAMERASYS_GLB_ISP_BLK_M0_LP_EB                       BIT(16)
#define BIT_CAMERASYS_GLB_ISP_BLK_M0_LP_NUM(x)                   ((x) << 0  & (0x0000FFFF))

/* bits definitions for REG_CAMERASYS_GLB_MM_ISP_BLK_M1_LPC_CTRL, [0x30000060] */
#define BIT_CAMERASYS_GLB_ISP_BLK_M1_PU_NUM(x)                   ((x) << 24 & (BIT(24) | BIT(25) | BIT(26) | BIT(27) | BIT(28) | BIT(29) | BIT(30) | BIT(31)))
#define BIT_CAMERASYS_GLB_ISP_BLK_M1_LP_EB                       BIT(16)
#define BIT_CAMERASYS_GLB_ISP_BLK_M1_LP_NUM(x)                   ((x) << 0  & (0x0000FFFF))

/* bits definitions for REG_CAMERASYS_GLB_MM_ISP_BLK_M2_LPC_CTRL, [0x30000068] */

/* bits definitions for REG_CAMERASYS_GLB_MM_ISP_BLK_M4_LPC_CTRL, [0x3000006C] */

/* bits definitions for REG_CAMERASYS_GLB_MM_ISP_BLK_S0_LPC_CTRL, [0x30000070] */
#define BIT_CAMERASYS_GLB_ISP_BLK_S0_PU_NUM(x)                   ((x) << 24 & (BIT(24) | BIT(25) | BIT(26) | BIT(27) | BIT(28) | BIT(29) | BIT(30) | BIT(31)))
#define BIT_CAMERASYS_GLB_ISP_BLK_S0_LP_EB                       BIT(16)
#define BIT_CAMERASYS_GLB_ISP_BLK_S0_LP_NUM(x)                   ((x) << 0  & (0x0000FFFF))

/* bits definitions for REG_CAMERASYS_GLB_MM_DCAM_BLK_M0_LPC_CTRL, [0x30000074] */
#define BIT_CAMERASYS_GLB_DCAM_BLK_M0_PU_NUM(x)                  ((x) << 24 & (BIT(24) | BIT(25) | BIT(26) | BIT(27) | BIT(28) | BIT(29) | BIT(30) | BIT(31)))
#define BIT_CAMERASYS_GLB_DCAM_BLK_M0_LP_EB                      BIT(16)
#define BIT_CAMERASYS_GLB_DCAM_BLK_M0_LP_NUM(x)                  ((x) << 0  & (0x0000FFFF))

/* bits definitions for REG_CAMERASYS_GLB_MM_DCAM_BLK_M1_LPC_CTRL, [0x30000078] */
#define BIT_CAMERASYS_GLB_DCAM_BLK_M1_PU_NUM(x)                  ((x) << 24 & (BIT(24) | BIT(25) | BIT(26) | BIT(27) | BIT(28) | BIT(29) | BIT(30) | BIT(31)))
#define BIT_CAMERASYS_GLB_DCAM_BLK_M1_LP_EB                      BIT(16)
#define BIT_CAMERASYS_GLB_DCAM_BLK_M1_LP_NUM(x)                  ((x) << 0  & (0x0000FFFF))

/* bits definitions for REG_CAMERASYS_GLB_MM_DCAM_BLK_S0_LPC_CTRL, [0x3000007C] */
#define BIT_CAMERASYS_GLB_DCAM_BLK_S0_PU_NUM(x)                  ((x) << 24 & (BIT(24) | BIT(25) | BIT(26) | BIT(27) | BIT(28) | BIT(29) | BIT(30) | BIT(31)))
#define BIT_CAMERASYS_GLB_DCAM_BLK_S0_LP_EB                      BIT(16)
#define BIT_CAMERASYS_GLB_DCAM_BLK_S0_LP_NUM(x)                  ((x) << 0  & (0x0000FFFF))

/* bits definitions for REG_CAMERASYS_GLB_MIPI_PHY_SEL, [0x300000A8] */
#define BIT_CAMERASYS_GLB_MIPI_CSI_CDPHY_C3_SEL(x)               ((x) << 15 & (BIT(15) | BIT(16) | BIT(17)))
#define BIT_CAMERASYS_GLB_MIPI_CSI_CDPHY_C2_SEL(x)               ((x) << 12 & (BIT(12) | BIT(13) | BIT(14)))
#define BIT_CAMERASYS_GLB_MIPI_CSI_DPHY_C1_SEL1(x)               ((x) << 9  & (BIT(9) | BIT(10) | BIT(11)))
#define BIT_CAMERASYS_GLB_MIPI_CSI_DPHY_C1_SEL0(x)               ((x) << 6  & (BIT(6) | BIT(7) | BIT(8)))
#define BIT_CAMERASYS_GLB_MIPI_CSI_DPHY_C0_SEL1(x)               ((x) << 3  & (BIT(3) | BIT(4) | BIT(5)))
#define BIT_CAMERASYS_GLB_MIPI_CSI_DPHY_C0_SEL0(x)               ((x) << 0  & (BIT(0) | BIT(1) | BIT(2)))

/* bits definitions for REG_CAMERASYS_GLB_USER_GATE_FORCE_OFF0, [0x300000AC] */
#define BIT_CAMERASYS_GLB_MM_SYS_CFG_DVFS_FORCE_OFF              BIT(30)
#define BIT_CAMERASYS_GLB_MM_SYS_CFG_CKG_FORCE_OFF               BIT(29)
#define BIT_CAMERASYS_GLB_MM_SYS_CFG_DCAM_BLK_FORCE_OFF          BIT(28)
#define BIT_CAMERASYS_GLB_MM_SYS_CFG_ISP_BLK_FORCE_OFF           BIT(27)
#define BIT_CAMERASYS_GLB_MM_SYS_CFG_JPG_FORCE_OFF               BIT(26)
#define BIT_CAMERASYS_GLB_MM_SYS_CFG_SYS_MTX_FORCE_OFF           BIT(25)
#define BIT_CAMERASYS_GLB_JPG_FR_FORCE_OFF                       BIT(24)
#define BIT_CAMERASYS_GLB_JPG_CFG_FORCE_OFF                      BIT(23)
#define BIT_CAMERASYS_GLB_JPG_JPG_FORCE_OFF                      BIT(22)
#define BIT_CAMERASYS_GLB_MM_MTX_DATA_FR_FORCE_OFF               BIT(21)
#define BIT_CAMERASYS_GLB_DCAM_BLK_CFG_DCAM_BLK_FORCE_OFF        BIT(20)
#define BIT_CAMERASYS_GLB_DCAM_MTX_FR_FORCE_OFF                  BIT(19)
#define BIT_CAMERASYS_GLB_CPHY_CFG_FR_FORCE_OFF                  BIT(18)
#define BIT_CAMERASYS_GLB_DCAM2_3_AXI_FR_FORCE_OFF               BIT(17)
#define BIT_CAMERASYS_GLB_DCAM2_3_AXI_DCAM2_3_FORCE_OFF          BIT(16)
#define BIT_CAMERASYS_GLB_DCAM2_3_FR_FORCE_OFF                   BIT(15)
#define BIT_CAMERASYS_GLB_DCAM0_1_AXI_FR_FORCE_OFF               BIT(14)
#define BIT_CAMERASYS_GLB_DCAM0_1_AXI_DCAM0_1_FORCE_OFF          BIT(13)
#define BIT_CAMERASYS_GLB_DCAM0_1_FR_FORCE_OFF                   BIT(12)
#define BIT_CAMERASYS_GLB_CPP_FR_FORCE_OFF                       BIT(9)
#define BIT_CAMERASYS_GLB_ISP_FR_FORCE_OFF                       BIT(8)

/* bits definitions for REG_CAMERASYS_GLB_USER_GATE_FORCE_OFF1, [0x300000B0] */
#define BIT_CAMERASYS_GLB_TZPC_FR_FORCE_OFF                      BIT(11)
#define BIT_CAMERASYS_GLB_DJTAG_TCK_SYS_FORCE_OFF                BIT(10)
#define BIT_CAMERASYS_GLB_DJTAG_TCK_DCAM_BLK_FORCE_OFF           BIT(8)
#define BIT_CAMERASYS_GLB_DJTAG_TCK_ISP_BLK_FORCE_OFF            BIT(7)
#define BIT_CAMERASYS_GLB_SENSOR3_FR_FORCE_OFF                   BIT(6)
#define BIT_CAMERASYS_GLB_SENSOR2_FR_FORCE_OFF                   BIT(5)
#define BIT_CAMERASYS_GLB_SENSOR0_FR_FORCE_OFF                   BIT(3)
#define BIT_CAMERASYS_GLB_MM_SYS_CFG_FR_FORCE_OFF                BIT(2)
#define BIT_CAMERASYS_GLB_MM_SYS_CFG_ASBR_FORCE_OFF              BIT(1)

/* bits definitions for REG_CAMERASYS_GLB_USER_GATE_AUTO_GATE_EN0, [0x300000B4] */
#define BIT_CAMERASYS_GLB_MM_SYS_CFG_DVFS_AUTO_GATE_EN           BIT(30)
#define BIT_CAMERASYS_GLB_MM_SYS_CFG_CKG_AUTO_GATE_EN            BIT(29)
#define BIT_CAMERASYS_GLB_MM_SYS_CFG_DCAM_BLK_AUTO_GATE_EN       BIT(28)
#define BIT_CAMERASYS_GLB_MM_SYS_CFG_ISP_BLK_AUTO_GATE_EN        BIT(27)
#define BIT_CAMERASYS_GLB_MM_SYS_CFG_JPG_AUTO_GATE_EN            BIT(26)
#define BIT_CAMERASYS_GLB_MM_SYS_CFG_SYS_MTX_AUTO_GATE_EN        BIT(25)
#define BIT_CAMERASYS_GLB_JPG_FR_AUTO_GATE_EN                    BIT(24)
#define BIT_CAMERASYS_GLB_JPG_CFG_AUTO_GATE_EN                   BIT(23)
#define BIT_CAMERASYS_GLB_JPG_JPG_AUTO_GATE_EN                   BIT(22)
#define BIT_CAMERASYS_GLB_MM_MTX_DATA_FR_AUTO_GATE_EN            BIT(21)
#define BIT_CAMERASYS_GLB_DCAM_BLK_CFG_DCAM_BLK_AUTO_GATE_EN     BIT(20)
#define BIT_CAMERASYS_GLB_DCAM_MTX_FR_AUTO_GATE_EN               BIT(19)
#define BIT_CAMERASYS_GLB_CPHY_CFG_FR_AUTO_GATE_EN               BIT(18)
#define BIT_CAMERASYS_GLB_DCAM2_3_AXI_FR_AUTO_GATE_EN            BIT(17)
#define BIT_CAMERASYS_GLB_DCAM2_3_AXI_DCAM2_3_AUTO_GATE_EN       BIT(16)
#define BIT_CAMERASYS_GLB_DCAM2_3_FR_AUTO_GATE_EN                BIT(15)
#define BIT_CAMERASYS_GLB_DCAM0_1_AXI_FR_AUTO_GATE_EN            BIT(14)
#define BIT_CAMERASYS_GLB_DCAM0_1_AXI_DCAM0_1_AUTO_GATE_EN       BIT(13)
#define BIT_CAMERASYS_GLB_DCAM0_1_FR_AUTO_GATE_EN                BIT(12)
#define BIT_CAMERASYS_GLB_CPP_FR_AUTO_GATE_EN                    BIT(9)
#define BIT_CAMERASYS_GLB_ISP_FR_AUTO_GATE_EN                    BIT(8)

/* bits definitions for REG_CAMERASYS_GLB_USER_GATE_AUTO_GATE_EN1, [0x300000B8] */
#define BIT_CAMERASYS_GLB_TZPC_FR_AUTO_GATE_EN                   BIT(11)
#define BIT_CAMERASYS_GLB_DJTAG_TCK_SYS_AUTO_GATE_EN             BIT(10)
#define BIT_CAMERASYS_GLB_DJTAG_TCK_DCAM_BLK_AUTO_GATE_EN        BIT(8)
#define BIT_CAMERASYS_GLB_DJTAG_TCK_ISP_BLK_AUTO_GATE_EN         BIT(7)
#define BIT_CAMERASYS_GLB_SENSOR3_FR_AUTO_GATE_EN                BIT(6)
#define BIT_CAMERASYS_GLB_SENSOR2_FR_AUTO_GATE_EN                BIT(5)
#define BIT_CAMERASYS_GLB_SENSOR0_FR_AUTO_GATE_EN                BIT(3)
#define BIT_CAMERASYS_GLB_MM_SYS_CFG_FR_AUTO_GATE_EN             BIT(2)
#define BIT_CAMERASYS_GLB_MM_SYS_CFG_ASBR_AUTO_GATE_EN           BIT(1)

/* bits definitions for REG_CAMERASYS_GLB_DCAM_BLK_SOFT_RST, [0x300000C8] */
#define BIT_CAMERASYS_GLB_CSI_SWITCH_SOFT_RST                    BIT(16)
#define BIT_CAMERASYS_GLB_DCAM0_1_FMCU_SOFT_RST                  BIT(15)
#define BIT_CAMERASYS_GLB_DCAM0_1_VAU_SOFT_RST                   BIT(14)
#define BIT_CAMERASYS_GLB_DCAM2_3_VAU_SOFT_RST                   BIT(13)
#define BIT_CAMERASYS_GLB_DCAM2_3_AXI_SOFT_RST                   BIT(12)
#define BIT_CAMERASYS_GLB_MIPI_CSI0_SOFT_RST                     BIT(11)
#define BIT_CAMERASYS_GLB_MIPI_CSI1_SOFT_RST                     BIT(10)
#define BIT_CAMERASYS_GLB_MIPI_CSI2_SOFT_RST                     BIT(9)
#define BIT_CAMERASYS_GLB_MIPI_CSI3_SOFT_RST                     BIT(8)
#define BIT_CAMERASYS_GLB_DCAM0_SOFT_RST                         BIT(7)
#define BIT_CAMERASYS_GLB_DCAM1_SOFT_RST                         BIT(6)
#define BIT_CAMERASYS_GLB_DCAM2_SOFT_RST                         BIT(5)
#define BIT_CAMERASYS_GLB_DCAM3_SOFT_RST                         BIT(4)
#define BIT_CAMERASYS_GLB_DCAM0_1_AXI_SOFT_RST                   BIT(3)
#define BIT_CAMERASYS_GLB_DCAM2_3_ALL_SOFT_RST                   BIT(2)
#define BIT_CAMERASYS_GLB_DCAM0_1_ALL_SOFT_RST                   BIT(1)
#define BIT_CAMERASYS_GLB_REGU_SOFT_RST                          BIT(0)

/* bits definitions for REG_CAMERASYS_GLB_ISP_BLK_SOFT_RST, [0x300000CC] */
#define BIT_CAMERASYS_GLB_ISP_AHB_SOFT_RST                       BIT(15)
#define BIT_CAMERASYS_GLB_ISP_SOFT_RST                           BIT(14)
#define BIT_CAMERASYS_GLB_ISP_ALL_SOFT                           BIT(13)
#define BIT_CAMERASYS_GLB_ISP_VAU_SOFT_RST                       BIT(12)
#define BIT_CAMERASYS_GLB_CPP_ALL_SOFT_RST                       BIT(11)
#define BIT_CAMERASYS_GLB_CPP_SOFT_RST                           BIT(10)
#define BIT_CAMERASYS_GLB_CPP_VAU_SOFT_RST                       BIT(9)
#define BIT_CAMERASYS_GLB_CPP_PATH0_SOFT_RST                     BIT(8)
#define BIT_CAMERASYS_GLB_CPP_PATH1_SOFT_RST                     BIT(7)
#define BIT_CAMERASYS_GLB_CPP_DMA_SOFT_RST                       BIT(6)

/* bits definitions for REG_CAMERASYS_GLB_SYS_SOFT_RST, [0x300000D0] */
#define BIT_CAMERASYS_GLB_JPG_VAU_SOFT_RST                       BIT(4)
#define BIT_CAMERASYS_GLB_JPG_SOFT_RST                           BIT(3)
#define BIT_CAMERASYS_GLB_DVFS_SOFT_RST                          BIT(1)
#define BIT_CAMERASYS_GLB_CKG_SOFT_RST                           BIT(0)

/* bits definitions for REG_CAMERASYS_GLB_SYS_LGT_STOP_MASK, [0x300000D8] */
#define BIT_CAMERASYS_GLB_LGT_STP_BUSY_MASK                      BIT(0)

/* vars definitions for controller CTL_CAMERASYS_GLB */


#endif /* __CAMERASYS_GLB_H____ */
