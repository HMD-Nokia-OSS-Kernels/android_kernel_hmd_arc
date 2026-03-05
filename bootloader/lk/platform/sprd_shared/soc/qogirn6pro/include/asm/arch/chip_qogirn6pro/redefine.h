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

/* referenced by mmc_v40.c */
#define AON_CLK_FREQ_26M                        (0x1)
#define AON_CLK_FREQ_384M			0X02
#define AP_CLK_FREQ_384M			0X03
#define BIT_AON_APB_EFUSE_NORMAL_EB		BIT_AON_APB_EFUSE_EB
/* bit redefinition for compatible */

#define SPRD_GPIO_PHYS     SPRD_AON_GPIO_PHYS

/* referenced by adi.c */
#define BIT_ADI_EB                              BIT_AON_APB_AON_ADI_EB
#define REG_ADI_EB				REG_AON_APB_APB_EB2

#define SPRD_ADISLAVE_PHYS                0x64420000
#define SPRD_ADISLAVE_BASE   SPRD_ADISLAVE_PHYS

#define SPRD_MISC_PHYS                            SPRD_AON_ADI_PHYS
#define SPRD_APCPUTS0_BASE     SPRD_AON_APCPU_TS_PHYS
#define REG_AON_APB_APB_RTC_EB                 REG_AON_APB_APB_RTC_EB0

#define SPRD_EMMC_BASE   SPRD_AP_EMMC_PHYS
#define BIT_AP_APB_EMMC_EB  BIT_AP_AHB_EMMC_EB
#define BIT_AP_APB_EMMC_SOFT_RST   BIT_AP_AHB_EMMC_SOFT_RST
#define SPRD_SDIO0_BASE   SPRD_AP_SDIO0_PHYS
#define BIT_AP_APB_SDIO0_EB   BIT_AP_AHB_SDIO0_EB
#define BIT_AP_APB_SDIO0_SOFT_RST   BIT_AP_AHB_SDIO0_SOFT_RST

#define SPRD_AON_TIMER_PHYS   SPRD_AON_TMR_PHYS
#define SPRD_SYSCNT_PHYS                        SPRD_AON_AP_SYST_PHYS

#define SPRD_UIDEFUSE_PHYS  SPRD_AON_EFUSE_PHYS
#define BIT_EFUSE_EB   BIT_AON_APB_AON_EFUSE_EB

/* referenced by sprd_spi.c*/
#define SPRD_RFSPI_CMD0 (SPRD_AON_BB_SW_RFSPI_MST_PHYS + 0x30)
#define SPRD_RFSPI_CMD1 (SPRD_AON_BB_SW_RFSPI_MST_PHYS + 0x34)
#define SPRD_RFSPI_RDATA (SPRD_AON_BB_SW_RFSPI_MST_PHYS + 0x3C)

/* referenced by gpio.c */
#define REG_GPIO_EB                             REG_AON_APB_APB_EB1
#define BIT_GPIO_EB                             BIT_AON_APB_AON_GPIO_EB

/* referenced by eic.c */
#define REG_EIC_EB                              REG_AON_APB_APB_EB2
#define BIT_EIC_EB                              BIT_AON_APB_AON_EIC_EB
#define BIT_EIC_RTC_EB                          BIT_AON_APB_EIC_RTC_EB
#define BIT_EIC_RTCDV5_EB                       BIT_AON_APB_EIC_RTCDV5_EB
#define SPRD_EIC_PHYS                           SPRD_AON_EIC2_PHYS
#define SPRD_EIC1_PHYS                          SPRD_AON_EIC3_PHYS

/* referenced by sprd_bl.c */
#define CTL_BASE_PWM  SPRD_AON_PWM0_PHYS

#define SPRD_PIN_PHYS                            SPRD_AON_PIN_REG_PHYS

/* referenced by qogirn6pro_usb_phy.c */
#define REG_ANLG_PHY_G2_ANALOG_USB20_USB20_UTMI_CTL1 0x64300004
#define BIT_ANLG_PHY_G2_ANALOG_USB20_USB20_VBUSVLDEXT BIT_16
#define BIT_ANLG_PHY_G2_ANALOG_USB20_USB20_DATABUS16_8 BIT_28

/* referenced by serial_sprd.c */
#define BIT_UART0_EB                            BIT_AP_APB_UART0_EB
#define BIT_UART1_EB                            BIT_AP_APB_UART1_EB
#define SPRD_UART0_PHYS   SPRD_AP_UART0_PHYS
#define SPRD_UART1_PHYS   SPRD_AP_UART1_PHYS

/* referenced by sprd_musb2_driver.c */
#define BIT_USB_EB   BIT_AON_APB_AON_USB2_TOP_EB

#define SPRD_I2C_PHYS   0
#define BIT_AON_APB_I2C_EB   0//BIT_AON_APB_I2C0_EB
#define BIT_AP_APB_I2C9_EB  BIT_AP_AHB_I2C9_EB
#define SPRD_I2C9_PHYS   SPRD_AP_I2C9_PHYS
#define BIT_AP_APB_I2C8_EB  BIT_AP_AHB_I2C8_EB
#define SPRD_I2C8_PHYS   SPRD_AP_I2C8_PHYS
#define BIT_AP_APB_I2C7_EB  BIT_AP_AHB_I2C7_EB
#define SPRD_I2C7_PHYS   SPRD_AP_I2C7_PHYS
#define BIT_AP_APB_I2C6_EB  BIT_AP_AHB_I2C6_EB
#define SPRD_I2C6_PHYS   SPRD_AP_I2C6_PHYS
#define BIT_AP_APB_I2C5_EB  BIT_AP_AHB_I2C5_EB
#define SPRD_I2C5_PHYS   SPRD_AP_I2C5_PHYS
#define BIT_AP_APB_I2C4_EB  BIT_AP_AHB_I2C4_EB
#define SPRD_I2C4_PHYS   SPRD_AP_I2C4_PHYS
#define BIT_AP_APB_I2C4_EB  BIT_AP_AHB_I2C4_EB
#define SPRD_I2C3_PHYS   SPRD_AP_I2C3_PHYS
#define BIT_AP_APB_I2C3_EB  BIT_AP_AHB_I2C3_EB
#define SPRD_I2C2_PHYS   SPRD_AP_I2C2_PHYS
#define BIT_AP_APB_I2C2_EB   BIT_AP_AHB_I2C2_EB
#define SPRD_I2C1_PHYS   SPRD_AP_I2C1_PHYS
#define BIT_AP_APB_I2C1_EB   BIT_AP_AHB_I2C1_EB
#define SPRD_I2C0_PHYS   SPRD_AP_I2C0_PHYS
#define BIT_AP_APB_I2C0_EB   BIT_AP_AHB_I2C0_EB

/*referenced by misc.c*/
#define REG_AON_APB_AON_CHIP_ID0   REG_AON_APB_PROJECT_ID_L
#define REG_AON_APB_AON_CHIP_ID1   REG_AON_APB_PROJECT_ID_H
#define REG_AON_APB_AON_VER_ID  REG_AON_APB_DERIVED_ID

/*referenced by ce*/
#define SPRD_SECURE_CE_PHYS             SPRD_AP_CE_SEC_PHYS
#define SPRD_PUBLIC_CE_PHYS             SPRD_AP_CE_PUB_PHYS
#define REG_AP_CLK_CORE_CGM_CE_CFG      REG_AP_CLK_CGM_CE_SEL_CFG
#define CTL_BASE_AP_APB                 CTL_AP_APB_BASE
#define REG_AP_AHB_CE_SEC_EB            REG_AP_APB_APB_EB
#define BIT_AP_AHB_CE_SEC_EB            BIT_AP_APB_CE_SEC_EB
#define REG_AP_AHB_CE_SEC_SOFT_RST      REG_AP_APB_APB_RST
#define BIT_AP_AHB_CE_SEC_SOFT_RST      BIT_AP_APB_CE_SEC_SOFT_RST
#define BIT_AP_AHB_CE_PUB_SOFT_RST      BIT_AP_APB_CE_PUB_SOFT_RST

#define SPRD_CORESIGHT_ETB              SPRD_DBG_CXTMC_ETF_32K_64B_SPRD_0_PHYS
#define REG_PMU_APB_SOFT_RST_SEL     REG_PMU_APB_SOFT_RST_SEL_0

#define BIT_MM_AHB_CKG_EB   BIT_CAMERASYS_GLB_CKG_EN
#define REG_MM_AHB_AHB_EB   REG_CAMERASYS_GLB_MM_SYS_EN

/*referenced by cp_boot.h*/
#define REG_AON_APB_PCP_SOFT_RST REG_AON_APB_PSCP_SOFT_RST

/* referenced by dwc3 */
#define EVT_BUF_ADDR                            0x25400000

/* referenced by ufs_global.c */
#define SPRD_UFS_HCI_BASE	SPRD_AP_UFS_HCI_PHYS

/* referenced by sprd_spi.c */
#define BIT_AON_APB_AP_HS_SPI_EB		BIT_AP_APB_SPI1_EB
#define REG_AON_APB_CLK_EB0			REG_AP_APB_APB_EB
#define SPRD_SPI0_PHYS				SPRD_AP_SPI0_PHYS
#define SPRD_SPI1_PHYS				SPRD_AP_SPI1_PHYS
#define SPRD_SPI2_PHYS				SPRD_AP_SPI2_PHYS
#define REG_AP_CLK_CORE_CGM_SPI0_CFG		REG_AON_CLK_CGM_SPI0_SEL_CFG
#define REG_AP_CLK_CORE_CGM_SPI1_CFG		REG_AON_CLK_CGM_SPI1_SEL_CFG
#define REG_AP_CLK_CORE_CGM_SPI2_CFG		REG_AON_CLK_CGM_SPI2_SEL_CFG

/* referenced by sprd_log_pointing.c */
#define AON_SYS_FRT_BASE            SPRD_AON_FRT_PHYS
#define SOC_CORESIGHT_ETF_BASE      SPRD_DBG_CXTMC_ETF_32K_64B_SPRD_0_PHYS

/* referenced by autosave_hal.c/autosave_phy.c */
#define SOC_CORESIGHT_BASE					(0x78000000)
#define CORINTH_CORESIGHT_BASE				(SOC_CORESIGHT_BASE+0x2000000)
#define APCPU_CORESIGHT_BASE				(SOC_CORESIGHT_BASE+0x3000000)
#define CORINTH_BIG_TMC_BASE				(CORINTH_CORESIGHT_BASE+0x3000)
#define ARM_DEBUG							(APCPU_CORESIGHT_BASE+0x10000)
#define ARM_OS_LOCK							(ARM_DEBUG+0x300)
#define ARM_EDPRSR							(ARM_DEBUG+0x314)
//EDPCSR External Debug Program Counter Sample Register
#define ARM_PMU_BASE						(APCPU_CORESIGHT_BASE+0x30000)
#define ARM_EDPCSR_BASE						(ARM_PMU_BASE+0x200)
#define ARM_EDPCSR_OFFSET					(0x100000) //per cpu offset
#define ARM_EDPCSR_LOHI_OFFSET				(0x4)   //for 64 bit
#define ARM_EDPCSR_CORE_NUM					(0x8)   //cpu num
#define ARM_EDPCSR_SAVE_NUM					(8)  //we want save counter
#define APCU_PWR_STATE0						( REG_PMU_APB_PWR_STATUS_DBG_0 )	//core0_State~core2_state
#define APCU_PWR_STATE1						( REG_PMU_APB_PWR_STATUS_DBG_1 )	//core7_State
#define APCU_PWR_STATE2						( REG_PMU_APB_PWR_STATUS_DBG_2 )	//core3_State~core6_state
//soc etb
#define SOC_ETB_BASE						(SOC_CORESIGHT_BASE + 0x3000)
#define ETB_ENABLE							(0x20)
#define ETB_RWP								(0x18)
#define ETB_RWD								(0x24)
#define CS_LOCK_OFFSET						(0xfb0)
#define CS_LOCK_KEY							(0xc5acce55)
//soc dump region
#define ETB_CLEAR_START						(0x0)
#define ETB_CLEAR_SIZE						(0x7C20)
#define ETB_SOC_DUMP_HEADER_START			(0x0)
#define ETB_AUTOSAVE_SIZE					(0x2000)  //8k
#define ETB_AONAPB_SIZE						(0x1000)  //4k
#define ETB_AONPMU_SIZE						(0x1000)  //4k
#define ETB_ANALOG_G8_SIZE					(0x50)
#define ETB_ANALOG_G9_SIZE					(0x50)
#define ETB_ANALOG_G10_SIZE					(0x90)
#define ETB_TOP_DVFS_SIZE					(0x1000)
#define ETB_APCPU_DVFS_SIZE					(0x430)
#define ETB_DDR_BASE_SIZE					(0x600)   //2k
#define ETB_DDR_PHY0_SIZE					(0x430)   //2k
#define ETB_DDR_PHY1_SIZE					(0x430)   //2k
#define ETB_AONCLK_SIZE						(0x200)
#define ETB_IOPIN_SIZE						(0x200)  //1K
#define ETB_ADI_BASE_SIZE					(0x430)  //2k
#define ETB_SOC_DUMP_END_START				(0x7C00)
#define ETB_SOC_DUMP_END_SIZE				(0x10)    //16 byte, display one row in etb.bin

#define REG_DBG_APB							(SOC_CORESIGHT_BASE + 0xa000)
#define REG_DBG_APB_CTRL					(REG_DBG_APB + 0xc)
#define ARM_AUTOSAVE_ENABLE					(CORINTH_BIG_TMC_BASE + 0x20)
#define ARM_AUTOSAVE_LOCK					(CORINTH_BIG_TMC_BASE + 0xfb0)
#define ARM_AUTOSAVE_SET_READ				(CORINTH_BIG_TMC_BASE + 0x14)
#define ARM_AUTOSAVE_READ_DATA				(CORINTH_BIG_TMC_BASE + 0x10)
#define ARM_AUTOSAVE_LOCK_KEY				(0xc5acce55)
//Enable Automatic Register Saving
#define AUTOSAVE_ENABLE_BIT						((0x1<<7) | (0x1<<9))
//soft trigger
#define AUTOSAVE_SOFT_TRIG_BIT					((0x1<<7) | (0x1<<8) | (0x1<<9))
//disable Automatic Registor saving
#define AUTOSAVE_DISABLE_AUTOSAVE_BIT			(~(0x1<<7))
#define CTL_BASE_AON_ADI					(0x64400000)

#endif
