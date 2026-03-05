/*
* Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
* Licensed under the Unisoc General Software License, version 1.0 (the License);
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
* Software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* See the Unisoc General Software License, version 1.0 for more details.
*/

#include <asm/arch/hw_i2c/sprd_hw_i2c.h>
#include <stdio.h>
#include <lk/debug.h>

#define I2C_RST				0x2c
#define I2C_CTL				0x00
#define HW_CHNL_PRIL			0x34
#define CHNL_EN0			0x60
#define CHNL_EN1			0x64
#define CHNL2_ADDR			0X68
#define CHNLX_ADDR(x)			(CHNL2_ADDR + (((x) - 2) << 2))
#define CHNL_GROUP0_SIZE		32
#define CHN_MAX_CNT			50
#define CHN_START			2

/*I2C_CTL*/
#define I2C_EN				BIT(2)

/* HW_CHNL_PRIL  */
#define CHNL_PRIL_DCDC_EN		BIT(6)

#ifdef CINFIG_HW_I2C_FOR_DCDC_EN
void hw_i2c_chnl_pril_set(int n)
{
        u32 val;

	val = readl(sprd_hw_i2c[n].base + HW_CHNL_PRIL);
	val |= CHNL_PRIL_DCDC_EN;
	writel(val, sprd_hw_i2c[n].base + HW_CHNL_PRIL);
}
#endif

void hw_i2c_clk_enable(int n)
{
        u32 val = readl(sprd_hw_i2c[n].apb_base);

        val |= sprd_hw_i2c[n].apb_eb;
        writel(val, sprd_hw_i2c[n].apb_base);

        val = readl(sprd_hw_i2c[n].base);
        val |= I2C_EN;
        writel(val, sprd_hw_i2c[n].base);
}

int i2c_hwchannel_set(unsigned int chn, unsigned int config, int n)
{
        u32 val;

        if (chn < CHN_START || chn >= CHN_MAX_CNT)
                return -1;

        writel(config, sprd_hw_i2c[n].base + CHNLX_ADDR(chn));
        if (chn < CHNL_GROUP0_SIZE){
                val = readl(sprd_hw_i2c[n].base + CHNL_EN0);
                val |= BIT(chn);
                writel(val, sprd_hw_i2c[n].base + CHNL_EN0);
        } else {
                val = readl(sprd_hw_i2c[n].base + CHNL_EN1);
                val |= BIT(chn - CHNL_GROUP0_SIZE);
                writel(val, sprd_hw_i2c[n].base + CHNL_EN1);
        }

        return 0;
}

void i2c_dvfs_hwchn_init(void)
{
	int i, j;

	for(i = 0; i < SPRD_I2C_NUM; i++){
		hw_i2c_clk_enable(i);
		__raw_writel(0x1, sprd_hw_i2c[i].base + I2C_RST);
#ifdef CINFIG_HW_I2C_FOR_DCDC_EN
		hw_i2c_chnl_pril_set(i);
#endif
		for(j = 0; j < sprd_hw_i2c[i].num; j++){
			i2c_hwchannel_set(sprd_hw_i2c[i].channel[j].channel_num,
				sprd_hw_i2c[i].channel[j].addr, i);
		}
	}

}
