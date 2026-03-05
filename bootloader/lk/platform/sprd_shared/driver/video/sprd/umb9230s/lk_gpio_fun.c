/*
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2016-2023 Unisoc (Shanghai) Technologies Co., Ltd
 *
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License. You may obtain a copy of the License at
 *
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 *
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

#include <gpio.h>
#include <sprd_common.h>
#include "lk_gpio_fun.h"

#define EXT_RST_B_AP    171 //pin U2TXD for umb9230 reset
#define XBUF_PD         172 //pin SPI2_CS1N suppy power for umb9230
#define INTR            170 //pin U2RXD int signal of umb9230s

/* umb9230s init */
/* return 0 --success Non-0 --fail */
int umb9230s_gpio_init(void)
{
	int ret;

	ret = sprd_gpio_request(XBUF_PD);
	if (ret) {
		errorf("Failed to request gpio172.\n");
	}
	ret = sprd_gpio_request(EXT_RST_B_AP);
	if (ret) {
		errorf("Failed to request gpio171.\n");
	}
	ret = sprd_gpio_request(INTR);
	if (ret) {
		errorf("Failed to request gpio170.\n");
	}

	ret = sprd_gpio_direction_input(INTR);
	if (ret) {
		errorf("Failed to set gpio170 direction.\n");
	}
	ret = sprd_gpio_direction_output(XBUF_PD, 1);
	if (ret) {
		errorf("Failed to set gpio172 to high.\n");
	}
	ret = sprd_gpio_direction_output(EXT_RST_B_AP, 0);
	if (ret) {
		errorf("Failed to set gpio171 to low.\n");
	}
	ret = sprd_gpio_direction_output(XBUF_PD, 0);
	if (ret) {
		errorf("Failed to set gpio172 to low\n");
	}
	udelay(100);

	ret = sprd_gpio_direction_output(EXT_RST_B_AP, 1);
	if (ret) {
		errorf("Failed to set gpio171  to high\n");
	}

	return ret;
}

/* via gpio to reset umb9230s */
/* return 0 --success Non-0 --fail */
/* flag=0 --low  flag=1 -- high */
int umb9230s_gpio_reset(int flag)
{
	int ret;

	ret = sprd_gpio_direction_output(EXT_RST_B_AP, flag);
	if (ret) {
		errorf("Failed to set reset.\n");
	}
	return ret;
}

/* via gpio to control power of umb9230s */
/* return 0 --success Non-0 --fail */
/* flag=0 --low  flag=1 -- high */
int umb9230s_set_gpio_power(int flag)
{
	int ret;

	ret = sprd_gpio_direction_output(XBUF_PD, flag);
	if (ret) {
		errorf("Failed to set power.\n");
	}
	return ret;
}

/* return umb9230s state of intr */
/* 0 --low power  > 0 -- high power  -1 --fail */
int umb9230s_get_gpio_intr(void)
{
	int ret;
	ret = sprd_gpio_get(INTR);
	if (ret < 0) {
		errorf("Failed to get intr.\n");
		goto err_gpio;
	}
	return ret;
err_gpio:
	return -1;
}
