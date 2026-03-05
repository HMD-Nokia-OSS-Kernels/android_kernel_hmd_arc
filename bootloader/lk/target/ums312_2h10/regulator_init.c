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
#include <uid_helper.h>

#define WCN_26M_EN 0x3

static int power_on_voltage_init(void)
{
	u32 efuse_val, efuse_bits_val, efuse_bits_mask = 0xc000;
	int block_id = 80;

	regulator_set_voltage("vddsim2",2800);
/*resolve asic workaround issue that the power supply fluctuate serious.*/
	regulator_set_voltage("vddrf1v25",1245);

	efuse_val = sprd_efuse_double_read(block_id, 0);
	dprintf(INFO,"the efuse val is 0x%x\n", efuse_val);
	efuse_bits_val = efuse_val & efuse_bits_mask;
	dprintf(INFO,"the efuse bits val is 0x%x\n", efuse_bits_val);

	if (efuse_bits_val)
		regulator_set_voltage("vddsram", 850);

	return 0;
}

int regulator_init(void)
{
	/*
	 */
	ANA_REG_OR(ANA_REG_GLB_TSX_CTRL0, BITS_DCXO_26M_REF_OUT_EN(WCN_26M_EN));
	power_on_voltage_init();
	return 0;
}
