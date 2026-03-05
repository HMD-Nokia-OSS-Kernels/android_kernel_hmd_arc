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

#include <lk/debug.h>
#include <asm/arch/common.h>
#include <asm/arch/sprd_reg.h>
#include <i2c.h>
#include <adi_hal_internal.h>
#include <sprd_regulator.h>
#include <sprd_boardid.h>
#include <sprd_common.h>

#define ETA355D_SLAVE_ADDR	0x60
#define ETA_REG0		0x00
#define ETA_INIT_VAL		0x9e

static int eta_i2c_bus_num = SPRDETA_I2C_BUS;

static int power_on_voltage_init(void)
{
	regulator_set_voltage("vddrf1v25",1250);

	/*check dcdc_cpu1 spuuly mode */
	if(sprd_get_power_mode() != 1) { /*cpu1 power supply by pmic */
		regulator_set_voltage("vddgpu",1000);
	}

	return 0;
}

static int eta355d_write_reg(int reg, u8 val)
{
	int ret;
	uint8_t buf[2] = {0};
	buf[0] = reg;
	buf[1] = val;

	ret = i2c_send(eta_i2c_bus_num, ETA355D_SLAVE_ADDR, buf, 2);
	if (ret < 0)
		errorf("eta355d i2c write failed!");

	return ret;
}

static int eta355d_read_reg(u8 reg, u8 *value)
{
	int ret;
	uint8_t reg_addr[2] = {0};
	reg_addr[0] = reg;
	ret = i2c_read_write(eta_i2c_bus_num, ETA355D_SLAVE_ADDR, reg_addr, 1, value, 1);

	if (ret < 0) {
		errorf("reg(0x%x), ret(%d)\n", reg, ret);
		return ret;
	}

	dprintf(INFO,"eta_read_reg reg = 0x%x, value = %d / %x\n", reg, *value, *value);

	return 0;
}

int regulator_init(void)
{
	u8 reg_value = 0;

	power_on_voltage_init();

	eta355d_write_reg(ETA_REG0, ETA_INIT_VAL);
	eta355d_read_reg(ETA_REG0, &reg_value);

	return 0;
}
