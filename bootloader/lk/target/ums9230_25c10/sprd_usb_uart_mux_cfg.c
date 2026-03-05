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

#include <asm/arch/sprd_reg.h>
#include <asm/arch/pinmap.h>
#include <sprd_common.h>
#include <sprd_keys.h>
#include <boot_mode.h>
#include <sprd_common_rw.h>
#include <miscdata_def.h>

/* set uart inf9 as ap_uart1 and inf1 as auddsp_uart0  */
#define PIN_UART_MATRIX_MTX_CFG_VALUE	(0x5BA90C84)
#define PIN_UART_MATRIX_MTX_CFG1_VALUE	(0x000002E2)
#define PIN_UART_SEL	BIT(2)
#define PIN_JTAG_SEL	(BIT(3) | BIT(2) | BIT(1))
#define PIN_JTAG_APWDG_SEL	BIT(0)

static void sprd_usb_power_init(void)
{
	CHIP_REG_AND(REG_ANLG_PHY_G2_ANALOG_USB20_USB20_BATTER_PLL,
			~(BIT_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_L | BIT_ANLG_PHY_G2_ANALOG_USB20_USB20_PS_PD_S));
	CHIP_REG_AND(REG_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW, ~BIT_ANLG_PHY_G2_ANALOG_USB20_USB20_ISO_SW_EN);
}

static void sprd_uart_inf_sel(void)
{
	CHIP_REG_SET(CTL_PIN_BASE + REG_PIN_UART_MATRIX_MTX_CFG, PIN_UART_MATRIX_MTX_CFG_VALUE);
	CHIP_REG_SET(CTL_PIN_BASE + REG_PIN_UART_MATRIX_MTX_CFG1, PIN_UART_MATRIX_MTX_CFG1_VALUE);
}

static int check_keys_mode(void)
{
	uint32_t key_mode = 0;
	uint32_t key_code = 0;
	volatile int i;

	if (boot_pwr_check() >= PWR_KEY_DETECT_CNT) {
		mdelay(50);
		for (i = 0; i < 10; i++) {
			key_code = board_key_scan();
			if(key_code != KEY_RESERVED)
			  break;
		}
		key_mode = check_key_boot(key_code);
		if(key_mode == CMD_USB_MUX_MODE) {
			dprintf(INFO, "Enter USB_MUX_MODE!\n");
			return 1;
		}
	}

	return 0;
}

int read_mux_cfg_flag(void)
{
	if(CHIP_REG_GET(REG_AON_APB_USB_UART_JTAG_MUX))
		return 1;
	else
		return 0;
}

/* if usb switch to uart in chipram, it is called by target_init() in LK */
void usb_uart_inf_config(void)
{
	if (CHIP_REG_GET(REG_AON_APB_USB_UART_JTAG_MUX) == PIN_UART_SEL)
		sprd_uart_inf_sel();
}
/* usb switch to uart/jtag/jtag_apwdg by keys detection  */
void usb_uart_key_config(void)
{
	if (check_keys_mode() == 1) {
		CHIP_REG_SET(REG_AON_APB_USB_UART_JTAG_MUX, PIN_UART_SEL);
		sprd_usb_power_init();
		sprd_uart_inf_sel();
		dprintf(INFO, "keys_detection: usb switch to uart!\n");
	}
	else {
		dprintf(INFO, "keys_detection: keep default mode!\n");
	}
}

void usb_jtag_key_config(void)
{
	if (check_keys_mode() == 1) {
		dprintf(INFO, "keys_detection: usb switch to jtag!\n");
		CHIP_REG_SET(REG_AON_APB_USB_UART_JTAG_MUX, PIN_JTAG_SEL);
		sprd_usb_power_init();
	}
	else {
		dprintf(INFO, "keys_detection: keep default mode!\n");
	}
}

/* usb switch to uart/jtag/jtag_apwdg by engineer mode  */
void usb_mux_uart_config(void)
{
	debugf("usb switch to uart!\n");
	CHIP_REG_SET(REG_AON_APB_USB_UART_JTAG_MUX, PIN_UART_SEL);
	sprd_usb_power_init();
	sprd_uart_inf_sel();
}

void usb_mux_jtag_config(void)
{
	CHIP_REG_SET(REG_AON_APB_USB_UART_JTAG_MUX, PIN_JTAG_SEL);
	sprd_usb_power_init();
	debugf("usb switch to jtag!\n");
}

void usb_mux_jtag_apwdg_config(void)
{
	CHIP_REG_SET(REG_AON_APB_USB_UART_JTAG_MUX, PIN_JTAG_APWDG_SEL);
	sprd_usb_power_init();
	debugf("usb switch to jtag_apwdg!\n");
}

/* write default_val to miscdata for usbpinmux in engineer mode */
void usbmux_miscdata_first_write(void)
{
	char buf[USBMUX_DATA_LEN] = {0};

	strcpy(buf, "off");
	if (0 != common_raw_write("miscdata", USBMUX_DATA_LEN, 0, USBMUX_DATA_OFFSET, buf)) {
		errorf("write default USBMUX config flag fail\n");
	} else {
		debugf("write default USBMUX config flag success\n");
	}
}

