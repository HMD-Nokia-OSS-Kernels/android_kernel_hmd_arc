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
#include <asm/arch/common.h>

#define I2C_RST				0x2c
#define I2C_CTL 			0x00
#define CHNL_TX_BASEADDR		0x4C
#define CHNL_TX_ADDR(x)			(CHNL_TX_BASEADDR + (((x) - 5) << 1))
#define CHNL_RX_BASEADDR		0x7C
#define CHNL_RX_ADDR(x)			(CHNL_RX_BASEADDR + (((x) - 6) << 1))
#define CHN_MAX_CNT			50
#define CHN_START			2

#define CHN_TX_START		5
#define CHN_TX_MAX_CNT		27

#define CHN_RX_START		6
#define CHN_RX_MAX_CNT		28

/*I2C_CTL*/
#define I2C_EN				BIT(2)
#define I2C_HW_EN			BIT(0)

/* HW_CHNL_PRIL  */

void hw_i2c_chnl_pril_set(unsigned int chn, unsigned int config, int n)
{
	u32 val;

	if (chn < CHN_START || chn >= CHN_MAX_CNT)
		return;

	if ((chn <= CHN_TX_MAX_CNT || chn >= CHN_TX_START) & (chn % 2)) {
		val = readl(sprd_hw_i2c[n].base + CHNL_TX_ADDR(chn));
		val |= config;
		writel(val, sprd_hw_i2c[n].base + CHNL_TX_ADDR(chn));
	}

	if ((chn <= CHN_RX_MAX_CNT || chn >= CHN_RX_START) & !(chn % 2)) {
		val = readl(sprd_hw_i2c[n].base + CHNL_TX_ADDR(chn));
		val |= config;
		writel(val, sprd_hw_i2c[n].base + CHNL_TX_ADDR(chn));
	}
}

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

	if ((chn <= CHN_TX_MAX_CNT || chn >= CHN_TX_START) & (chn % 2)) {
		config |= I2C_HW_EN;
		writel(config, sprd_hw_i2c[n].base + CHNL_TX_ADDR(chn));
	}

	if ((chn <= CHN_RX_MAX_CNT || chn >= CHN_RX_START) & !(chn % 2)) {
		config |= I2C_HW_EN;
		writel(config, sprd_hw_i2c[n].base + CHNL_RX_ADDR(chn));
	}
	val = readl(sprd_hw_i2c[n].base + CHNL_TX_ADDR(chn));
        return 0;
}

void i2c_dvfs_hwchn_init(void)
{
	int i, j;
	for(i = 0; i < SPRD_I2C_NUM; i++){
		hw_i2c_clk_enable(i);
		__raw_writel(0x1, sprd_hw_i2c[i].base + I2C_RST);
		for(j = 0; j < sprd_hw_i2c[i].num; j++){
			i2c_hwchannel_set(sprd_hw_i2c[i].channel[j].channel_num,
				sprd_hw_i2c[i].channel[j].addr, i);

			hw_i2c_chnl_pril_set(sprd_hw_i2c[i].channel[j].channel_num,
				sprd_hw_i2c[i].channel[j].pril, i);
		}
	}

}
