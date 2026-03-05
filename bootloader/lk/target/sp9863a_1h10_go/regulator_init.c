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
//#include <asm/io.h>
#include <secureboot/sec_efuse_sharkl3.h>
#include <i2c.h>

#define DCDC_CORE_SS_R	950		//SS bin vol set for L3R
#define DCDC_CORE_TT_E	950             //TT bin vol set for L3E

#define EFUSE_BLOCK_BIN	38		//efuse block
#define EFUSE_BLOCK_FRQ	46		//efuse block
#define BIT_GPU_BIN	0x0f000000	//bin bit
#define BIT_SS_BLK_R	0x2		//ss bin bit for L3R
#define BIT_TT_BLK_E	0x1		//tt bin bit for L3E

#define L3R_VERSION	0x00000a00	//L3R version
#define CHIP_MASK	0xf0000		//L3E version

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
	u32 MFT_ID_READ = 0, CHIP_FREQ_MASK = 0;
	u32 MASK_17 = ((1 << 16) | (1 << 17 ));
	u32 MASK_18 = ((1 << 18));

	MFT_ID_READ = __raw_readl(REG_AON_APB_AON_MFT_ID);
	CHIP_FREQ_MASK = (sprd_ap_efuse_read(EFUSE_BLOCK_FRQ)) & CHIP_MASK;
	dprintf(INFO,"version read val is :%d\n",MFT_ID_READ);

	regulator_set_voltage("vddsim2",2800);

	if (MFT_ID_READ == L3R_VERSION) {
		u32 block = 0, val = 0;
		if ((CHIP_FREQ_MASK == MASK_17) || (CHIP_FREQ_MASK == MASK_18)) {
			block = sprd_ap_efuse_read(EFUSE_BLOCK_BIN);
			val = ((block & BIT_GPU_BIN) >> 24);
			printf("block val l3e=%d\n", val);
			if (val == BIT_TT_BLK_E)
				regulator_set_voltage("vddcore", DCDC_CORE_TT_E);//GPU TT BIN L3E
			return 0;
                }

		block = sprd_ap_efuse_read(EFUSE_BLOCK_BIN);
		val = ((block & BIT_GPU_BIN) >> 24);
		printf("block val l3r=%d\n", val);
		if (val == BIT_SS_BLK_R)
			regulator_set_voltage("vddcore", DCDC_CORE_SS_R);//GPU SS BIN L3R
	}
	return 0;
}

int regulator_init(void)
{
	power_on_voltage_init();
	return 0;
}
