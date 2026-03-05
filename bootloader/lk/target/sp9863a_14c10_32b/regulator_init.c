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
#include <adi_hal_internal.h>
#include <sprd_regulator.h>
#include <i2c.h>

#define ETA355D_SLAVE_ADDR     0x60
#define ETA_REG0               0x00
#define ETA_REG3               0x03
#define ETA_INIT_VAL           0x9e


static int eta355d_read_reg(u8 reg, u8 *value)
{
       int ret;
       uint8_t reg_addr[2] = {0};
       reg_addr[0] = reg;
       ret = i2c_read_write(7, ETA355D_SLAVE_ADDR, reg_addr, 1, value, 1);

       if (ret < 0) {
               errorf("reg(0x%x), ret(%d)\n", reg, ret);
               return ret;
       }

       dprintf(INFO,"eta_read_reg reg = 0x%x, value = %d / %x\n", reg, *value, *value);

       return 0;
}

int sprd_get_dcdc_muti(void)
{
       u8 reg_value = 0;

       eta355d_read_reg(ETA_REG3, &reg_value);
	   dprintf(INFO,"ETA_REG3 reg = 0x%x, value = %x\n", ETA_REG3,  reg_value);

       return reg_value;
}

static int power_on_voltage_init(void)
{
	regulator_set_voltage("vddsim2",2800);
	return 0;
}

int regulator_init(void)
{
	power_on_voltage_init();
	return 0;
}
