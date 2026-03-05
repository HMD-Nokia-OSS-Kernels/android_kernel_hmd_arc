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

#include <i2c.h>
#include <stdio.h>
#include <lk/debug.h>

int i2c_transfer(i2cDev_t *i2c, struct i2c_msg *m, int n)
{
	return i2c->algo->master_xfer(i2c,m,n);
}

int i2c_send(int bus, uint32_t slaveAddr,
		uint8_t *buf, uint32_t num)
{
	int ret;
	struct i2c_msg msgs;

	i2cDev_t *i2c = (i2cDev_t *)&I2C_BUS[bus];
	msgs.buf = buf;
	msgs.len = num;
	msgs.flags = 0;
	msgs.addr = slaveAddr;
	ret = i2c_transfer(i2c, &msgs, 1);

	if (ret < 0)
		dprintf(INFO,"i2c transfer fail\n");

	return ret;
}

int i2c_receive(int bus, uint32_t slaveAddr,
		uint8_t *buf, uint32_t num)
{
	int ret;
	struct i2c_msg msgs;

	i2cDev_t *i2c = (i2cDev_t *)&I2C_BUS[bus];
	msgs.buf = buf;
	msgs.len = num;
	msgs.flags = I2C_M_RD;
	msgs.addr = slaveAddr;
	ret = i2c_transfer(i2c, &msgs, 1);

	if (ret < 0)
		dprintf(INFO,"i2c transfer fail\n");

	return ret;
}

int i2c_read_write(int bus, uint32_t slaveAddr,
		const uint8_t *wr_buff, uint32_t wr_size,
		uint8_t *rd_buff, uint32_t rd_size)
{
	int ret;
	struct i2c_msg msgs[] = {
		{
			.addr = slaveAddr,
			.flags = 0,
			.len = wr_size,
			.buf = (unsigned char *) wr_buff
		}, {
			.addr = slaveAddr,
			.flags = I2C_M_RD,
			.len = rd_size,
			.buf = rd_buff},
		};

	i2cDev_t *i2c = (i2cDev_t *)&I2C_BUS[bus];
	ret = i2c_transfer(i2c, msgs, 2);

	if (ret < 0)
		dprintf(INFO,"i2c transfer fail\n");

	return ret;
}

static inline uint16_t bswap_16(uint16_t x)
{
	return (x<<8)|(x>>8);
}

static inline uint32_t bswap_32(uint32_t x)
{
	x = ((x<<8)&0xff00ff00) | ((x>>8)&0x00ff00ff);
	return (x<<16)|(x>>16);
}

int iic2cmd_write(int bus, uint32_t dec_addr, uint32_t* buf, uint32_t len)
{
	uint8_t * write_buf = (uint8_t *)buf;
	uint32_t i=0;
	int ret;

	/*
	 * iic2cmd support 16bit reg addr and 32bit data, buf = reg_addr + reg_data .
	 * to avoid unnecessary memory operations, we need to ignore the first 16 bits.
	 */

	struct i2c_msg msg = {
		.addr = dec_addr,
		.flags = 0,
		.len = 4*len-2,
		.buf = write_buf+2,
	};

	write_buf[2] = buf[0] >> 0x8;
	write_buf[3] = buf[0] & 0xFF;

	for (i=1;i<len;i++){
		buf[i] = bswap_32(buf[i]);
	}

	i2cDev_t *i2c = (i2cDev_t *)&I2C_BUS[bus];
	ret = i2c_transfer(i2c, &msg, 1);

	for (i=1;i<len;i++){
		buf[i] = bswap_32(buf[i]);
	}

	write_buf[2] = buf[0] >> 0x8;
	write_buf[3] = buf[0] & 0xFF;

	return ret;
}

int iic2cmd_read(int bus, uint32_t dec_addr, uint16_t reg_addr , uint32_t* buf, uint32_t len)
{
	uint8_t * reg = (uint8_t *)&reg_addr;
	uint8_t * read_buf = (uint8_t *)buf;
	uint32_t i=0;
	int ret;
	struct i2c_msg msg[2];

	msg[0].addr = dec_addr;
	msg[0].flags = 0;
	msg[0].buf = reg;
	msg[0].len = 2;

	msg[1].addr = dec_addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = read_buf;
	msg[1].len = 4*len;

	reg_addr = bswap_16(reg_addr);

	i2cDev_t *i2c = (i2cDev_t *)&I2C_BUS[bus];
	ret = i2c_transfer(i2c, msg, 2);

	for (i=0;i<len;i++){
		buf[i] = bswap_32(buf[i]);
	}

	return ret;
}
