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

#ifndef __DRIVERS_I2C_H__
#define __DRIVERS_I2C_H__

//#include <clk.h>

typedef struct deviceInfo deviceInfo_t;
typedef struct i2cDev i2cDev_t;

enum i2cState_t {
	IDLE,
	BUSY,
};

struct i2c_msg {
	unsigned char addr;	/* slave address			*/
	unsigned short flags;
#define I2C_M_TEN		0x0010	/* this is a ten bit chip address */
#define I2C_M_RD		0x0001	/* read data, from slave to master */
#define I2C_M_STOP		0x8000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NOSTART		0x4000	/* if I2C_FUNC_NOSTART */
#define I2C_M_REV_DIR_ADDR	0x2000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_IGNORE_NAK	0x1000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NO_RD_ACK		0x0800	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_RECV_LEN		0x0400	/* length will be first received byte */
	unsigned len;		/* msg length				*/
	unsigned char *buf;		/* pointer to msg data			*/
};

struct i2c_algorithm {
	int (*master_xfer)(i2cDev_t *i2c, struct i2c_msg *m, int n);
};

struct i2cDev {
	deviceInfo_t    *device;
	unsigned        index;
	int             check_transdone;
	enum i2cState_t state;
	const struct i2c_algorithm *algo; /* the algorithm to access the bus */
};

/* i2c data structure*/
struct sprd_i2c {
	i2cDev_t i2c;
	const char *bus_name;
	void *apb_base;//apb enble
	unsigned apb_eb;//apb enable bit
	void *base;//i2c phy address
//	clock_type 	clk;
	unsigned int freq;
	unsigned int src_clk;
};

extern struct sprd_i2c I2C_BUS[];

int i2c_transfer(i2cDev_t *i2c, struct i2c_msg *m, int n);
int i2c_send(int bus, unsigned slaveAddr, unsigned char *buf, unsigned num);
int i2c_receive(int bus, unsigned slaveAddr, unsigned char *buf, unsigned num);
int i2c_read_write( int bus, unsigned slaveAddr, const unsigned char *wr_buff,
		unsigned wr_size, unsigned char *rd_buff, unsigned rd_size);
void sprd_i2c_init(void);
int iic2cmd_write(int bus, unsigned dec_addr, unsigned int * buf, unsigned int len);
int iic2cmd_read(int bus, unsigned dec_addr, unsigned short reg_addr , unsigned int* buf, unsigned int len);

#endif /*__DRIVERS_I2C_H__*/
