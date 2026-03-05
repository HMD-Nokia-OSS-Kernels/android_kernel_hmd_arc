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

#ifndef __ADI_H__
#define __ADI_H__

#define DCDC_CORE_VAL_ADDR  0x1864
#define DCDC_CORE_VAL_ADI_CHN  21
int adi_hwchannel_set(unsigned int chn, unsigned int config);

void sci_adi_init(void);

/*
 * sci_get_adie_chip_id - read a-die chip id
 */
u32 sci_get_adie_chip_id(void);

int sci_adi_read(u32 reg);

/*
 * WARN: the arguments (reg, value) is different from
 * the general __raw_writel(value, reg)
 * For sci_adi_write_fast: if set sync 1, then this function will
 * return until the val have reached hardware.otherwise, just
 * async write(is maybe in software buffer)
 */
int sci_adi_write_fast(u32 reg, u16 val, u32 sync);
int sci_adi_write(u32 reg, u16 or_val, u16 clear_msk);

static inline int sci_adi_raw_write(u32 reg, u16 val)
{
#if defined(CONFIG_FPGA)
	return 0;
#else
	return sci_adi_write_fast(reg, val, 1);
#endif  /* CONFIG_FPGA */
}

static inline int sci_adi_set(u32 reg, u16 bits)
{
#if defined(CONFIG_FPGA)
	return 0;
#else
	return sci_adi_write(reg, bits, 0);
#endif  /* CONFIG_FPGA */
}

static inline int sci_adi_clr(u32 reg, u16 bits)
{
#if defined(CONFIG_FPGA)
	return 0;
#else
	return sci_adi_write(reg, 0, bits);
#endif  /* CONFIG_FPGA */
}

#endif
