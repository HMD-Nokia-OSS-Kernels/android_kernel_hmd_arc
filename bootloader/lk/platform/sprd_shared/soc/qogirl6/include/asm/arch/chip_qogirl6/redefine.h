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
#define GIC_INT(n)                                       (n+32)

/* referenced by timer.c */
#define BIT_AON_TMR_26M_EB                               BIT_AON_APB_CGM_TMR_EN

/* referenced by mmc_v40.c */
#define AON_SDIO0_CLK_2X_CFG                      0x80
#define AON_EMMC_CLK_2X_CFG                       0x90
#define AON_CLK_FREQ_26M                          0x1
#define BIT_AON_APB_EFUSE_NORMAL_EB                      BIT_AON_APB_EFUSE_EB

/* referenced by timer.c */
#define REG_AON_TMR_CLK_EN                               REG_AON_APB_APB_EB1
#define REG_AON_TMR_CLK_RTC_EN                           REG_AON_APB_APB_RTC_EB0
#define REG_AON_TMR_26M_EN                               REG_AON_APB_CGM_REG1

#define SPRD_DPU_PHYS                                   ( 0x31000000 )
#define SPRD_DSI_PHYS                                   ( 0x31100000 )

#define SPRD_PIN_PHYS                                    SPRD_IO_CENTRAL_PIN_REG_BASE

/* referenced by aon_apb.h */
#define REG_AON_APB_APB_RTC_EB                           REG_AON_APB_APB_RTC_EB0

/* referenced by sprd_musb2_driver.c */
#define BIT_USB_EB                                BIT_AON_APB_OTG_UTMI_EB
#define SPRD_USB_BASE                             SPRD_AON_USB_BASE

 /* referenced by serial_sprd.c */
#define BIT_UART0_EB                              BIT_AP_APB_UART0_EB
#define BIT_UART1_EB                              BIT_AP_APB_UART1_EB
#define SPRD_UART0_PHYS                           SPRD_AP_UART0_BASE
#define SPRD_UART1_PHYS                           SPRD_AP_UART1_BASE

/* referenced by gpio.c */
#define REG_GPIO_EB                             REG_AON_APB_APB_EB1
#define BIT_GPIO_EB                             BIT_AON_APB_GPIO_EB
#define TB_GPIO_INT                             GIC_INT(61)
/* referenced by adi.c */
#define BIT_ADI_EB                              BIT_AON_APB_ADI_EB
#define REG_ADI_EB								REG_AON_APB_APB_EB2

/*referenced by gpio_reg_v0.h*/
#define SPRD_GPIO_PHYS                            SPRD_AON_GPIO_BASE

/* referenced by time.c */
#define SPRD_APCPUTS0_BASE                        SPRD_AON_TS_PRE_BASE

/* referenced by sprd_timer */
#define SPRD_AON_TIMER_PHYS                       SPRD_AON_TMRFULL_BASE
#define SPRD_SYSCNT_PHYS                          SPRD_AON_SYST_AP_BASE

/*referenced by sprd_ce.c*/
#define REG_AP_CLK_CORE_CGM_CE_CFG                REG_AP_CLK_CGM_CE_CFG

/* referenced by eic.c */
#define REG_EIC_EB                                REG_AON_APB_APB_EB2
#define BIT_EIC_EB                                BIT_AON_APB_EIC_EB
#define BIT_EIC_RTC_EB                            BIT_AON_APB_EIC_RTC_EB
#define BIT_EIC_RTCDV5_EB                         BIT_AON_APB_EIC_RTCDV5_EB
#define SPRD_EIC_PHYS                             SPRD_AON_EIC_U8_EXT2_BASE
#define SPRD_EIC1_PHYS                            SPRD_AON_EIC_U8_EXT3_BASE
#define TB_EICA_INT                               GIC_INT(173)

/* referenced by efuse.c */
#define BIT_EFUSE_SOFT_RST                        BIT_AON_APB_EFUSE_SOFT_RST
#define BIT_EFUSE_EB                              BIT_AON_APB_EFUSE_NORMAL_EB
#define SPRD_UIDEFUSE_PHYS                        SPRD_AON_RF_EFUSE_APB_BASE

/*referenced by ce*/
#define SPRD_SECURE_CE_PHYS                       SPRD_AP_CE_SEC_BASE
#define SPRD_PUBLIC_CE_PHYS                       SPRD_AP_CE_PUB_BASE
#define REG_AP_AHB_CE_SEC_EB                      REG_AP_APB_APB_EB
#define BIT_AP_AHB_CE_SEC_EB                      BIT_AP_APB_CE_SEC_EB
#define REG_AP_AHB_CE_SEC_SOFT_RST                REG_AP_APB_APB_RST
#define BIT_AP_AHB_CE_SEC_SOFT_RST                BIT_AP_APB_CE_SEC_SOFT_RST
#define BIT_AP_AHB_CE_PUB_SOFT_RST                BIT_AP_APB_CE_PUB_SOFT_RST

#define CTL_BASE_AP_APB                           CTL_AP_APB_BASE

/* referenced by adi */
#define SPRD_MISC_PHYS                            SPRD_AON_ADI_MST_BASE

/* referenced by sc2730 */
#define SPRD_ADISLAVE_PHYS                        (SPRD_MISC_PHYS + 0x20000)
#define SPRD_ADISLAVE_BASE                        SPRD_ADISLAVE_PHYS

/* referenced by sprd_bl.c */
#define CTL_BASE_PWM                              SPRD_AON_PWM_BASE

/* referenced by mmc */
#define EMMC_BASE_ADDR                            SPRD_AP_EMMC_BASE
#define SDIO0_BASE_ADDR                           SPRD_AP_SDIO0_BASE
#define SPRD_AONCKG_BASE                          SPRD_AONCKG_PHYS

/* referenced by sdio_cfg.c */
#define SPRD_EMMC_BASE                            SPRD_AP_EMMC_BASE
#define SPRD_SDIO0_BASE                           SPRD_AP_SDIO0_BASE

/*referenced by sprd_i2c_v2.c*/
#define SPRD_I2C0_PHYS                             SPRD_AP_I2C0_BASE
#define SPRD_I2C1_PHYS                             SPRD_AP_I2C1_BASE
#define SPRD_I2C2_PHYS                             SPRD_AP_I2C2_BASE
#define SPRD_I2C3_PHYS                             SPRD_AP_I2C3_BASE
#define SPRD_I2C4_PHYS                             SPRD_AP_I2C4_BASE
#define SPRD_I2C5_PHYS                             SPRD_AP_I2C5_BASE
#define SPRD_I2C6_PHYS                             SPRD_AP_I2C6_BASE
#define SPRD_AON_I2C_PHYS			SPRD_AON_I2C_HW_BASE
/*referenced by global_dispc.c*/
#define REG_AP_CLK_CORE_CGM_DISPC0_CFG             REG_AP_CLK_CGM_DISPC0_CFG
#define REG_AP_CLK_CORE_CGM_DISPC0_DPI_CFG         REG_AP_CLK_CGM_DISPC0_DPI_CFG

#define BIT_PMU_APB_PD_AUDCP_SYS_FORCE_SHUTDOWN                   ( BIT(25) )

#define SPRD_CORESIGHT_ETB                        SPRD_DBG_SOC_CORESIGHT_ETF_BASE

/* referenced by ufs_global.c */
#define SPRD_UFS_HCI_BASE			SPRD_AP_UFS_AES_BASE

/* referenced by sprd_spi.c */
#define BIT_AON_APB_AP_HS_SPI_EB		BIT_AP_APB_SPI1_EB
#define REG_AON_APB_CLK_EB0			REG_AP_APB_APB_EB
#define SPRD_SPI0_PHYS				SPRD_AP_SPI0_HS_BASE
#define SPRD_SPI1_PHYS				SPRD_AP_SPI1_HS_BASE
#define SPRD_SPI2_PHYS				SPRD_AP_SPI2_BASE
#define REG_AP_CLK_CORE_CGM_SPI0_CFG            REG_AP_CLK_CGM_SPI0_CFG
#define REG_AP_CLK_CORE_CGM_SPI1_CFG            REG_AP_CLK_CGM_SPI1_CFG
#define REG_AP_CLK_CORE_CGM_SPI2_CFG            REG_AP_CLK_CGM_SPI2_CFG

/* referenced by log_point.c */
#define AON_SYS_FRT_BASE            SPRD_AON_FRT_BASE
#define SOC_CORESIGHT_ETF_BASE      SPRD_DBG_SOC_CORESIGHT_ETF_BASE 
#endif

/* referenced by autosave_hal.c/autosave_phy.c */
#define SOC_CORESIGHT_BASE					(0x7C000000)
#define CORINTH_CORESIGHT_BASE				(SOC_CORESIGHT_BASE+0x2000000)
#define APCPU_CORESIGHT_BASE				(SOC_CORESIGHT_BASE+0x3000000)
#define CORINTH_BIG_TMC_BASE				(CORINTH_CORESIGHT_BASE+0x3000)
#define ARM_DEBUG							(APCPU_CORESIGHT_BASE+0x10000)
//EDPCSR External Debug Program Counter Sample Register
#define ARM_OS_LOCK							(ARM_DEBUG+0x300)
#define ARM_EDPRSR							(ARM_DEBUG+0x314)
#define ARM_PMU_BASE						(APCPU_CORESIGHT_BASE+0x30000)
#define ARM_EDPCSR_BASE						(ARM_PMU_BASE+0x200)
#define ARM_EDPCSR_OFFSET					(0x100000) //per cpu offset
#define ARM_EDPCSR_LOHI_OFFSET				(0x4)   //for 64 bit
#define ARM_EDPCSR_CORE_NUM					(0x8)   //cpu num
#define ARM_EDPCSR_SAVE_NUM					(8)  //we want save counter

/* pmu register */
#define APCU_PWR_STATE0						( CTL_PMU_APB_BASE + 0x04f0 )	//core0_State~core1_state
#define APCU_PWR_STATE1						( CTL_PMU_APB_BASE + 0x04f4 )	//core2_State~core5_state
#define APCU_PWR_STATE2						( CTL_PMU_APB_BASE + 0x04f8 )	//core6_State~core7_state

//soc etb
#define SOC_ETB_BASE						(SOC_CORESIGHT_BASE + 0x3000)
#define ETB_ENABLE							(0x20)
#define ETB_RWP								(0x18)
#define ETB_RWD								(0x24)
#define CS_LOCK_OFFSET						(0xfb0)
#define CS_LOCK_KEY							(0xc5acce55)

#define ARM_AUTOSAVE_ENABLE					(CORINTH_BIG_TMC_BASE + 0x20)
#define ARM_AUTOSAVE_LOCK					(CORINTH_BIG_TMC_BASE + 0xfb0)
#define ARM_AUTOSAVE_SET_READ				(CORINTH_BIG_TMC_BASE + 0x14)
#define ARM_AUTOSAVE_READ_DATA				(CORINTH_BIG_TMC_BASE + 0x10)
#define ARM_AUTOSAVE_LOCK_KEY				(0xc5acce55)

//soc dump region
#define ETB_CLEAR_START						(0x150)
#define ETB_CLEAR_SIZE						(0x3EC0)
#define ETB_SOC_DUMP_HEADER_START			(0x180)
#define ETB_AUTOSAVE_HEADER_START			(0x200)
#define ETB_AUTOSAVE_START					(0x210)
#define ETB_AUTOSAVE_SIZE					(0x2000)  //8k
#define ETB_PC_SAMPLE_START					(0x2220)
#define ETB_PC_SAMPLE_SIZE					(0x400)  //1k
#define ETB_CORE_STATUS_START				(0x2630)
#define ETB_CORE_STATUS_SIZE				(0x100)  //1k
#define ETB_DB_START						(0x2740)
#define ETB_DB_SIZE							(0x1800) //6k
#define ETB_SOC_DUMP_END_START				(0x3F50)
#define ETB_SOC_DUMP_END_SIZE				(0x10)

#define REG_DBG_APB							(SOC_CORESIGHT_BASE + 0xa000)
#define REG_DBG_APB_CTRL					(REG_DBG_APB + 0xc)

//Enable Automatic Register Saving
#define AUTOSAVE_ENABLE_BIT						(BIT_7|BIT_9)
//soft trigger
#define AUTOSAVE_SOFT_TRIG_BIT					(BIT_7|BIT_9|BIT_8)
//disable Automatic Registor saving
#define AUTOSAVE_DISABLE_AUTOSAVE_BIT			(~(BIT_7))
