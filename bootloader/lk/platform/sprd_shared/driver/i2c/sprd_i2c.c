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

#include <asm/arch/i2c/sprd_i2c.h>
#include <stdio.h>
#include <lk/debug.h>
#include <sprd_common.h>

#define I2C_CTL				(0x000)
#define I2C_ADDR_CFG		(0x004)
#define I2C_COUNT			(0x008)
#define I2C_RX				(0x00C)
#define I2C_TX				(0x010)
#define I2C_STATUS			(0x014)
#define I2C_HSMODE_CFG		(0x018)
#define I2C_VERSION			(0x01C)
#define ADDR_DVD0			(0x020)
#define ADDR_DVD1			(0x024)
#define ADDR_STA0_DVD		(0x028)
#define ADDR_RST			(0x02C)

/* I2C_CTL */
#define I2C_NACK_EN			BIT(22)
#define I2C_TRANS_EN		BIT(21)
#define STP_EN				BIT(20)
#define FIFO_AF_LVL			(16)
#define FIFO_AE_LVL			(12)
#define I2C_DMA_EN			BIT(11)
#define FULL_INTEN			BIT(10)
#define EMPTY_INTEN			BIT(9)
#define I2C_DVD_OPT			BIT(8)
#define I2C_OUT_OPT			BIT(7)
#define I2C_TRIM_OPT		BIT(6)
#define I2C_HS_MODE			BIT(4)
#define I2C_MODE			BIT(3)
#define I2C_EN				BIT(2)
#define I2C_INT_EN			BIT(1)
#define I2C_START			BIT(0)

/* I2C_STATUS */
#define SDA_IN				BIT(21)
#define SCL_IN				BIT(20)
#define FIFO_FULL			BIT(4)
#define FIFO_EMPTY			BIT(3)
#define I2C_INT				BIT(2)
#define I2C_RX_ACK			BIT(1)
#define I2C_BUSY			BIT(0)

/* ADDR_RST */
#define I2C_RST				BIT(0)

#define CONFIG_SYS_SPRD_I2C_DEFAULT_SPEED	100000
//#define I2C_MAX_NUM				7
#define I2C_SRC_CLK			26000000

#define I2C_FIFO_DEEP			15
#define I2C_FIFO_FULL_THLD		8
#define I2C_FIFO_EMPTY_THLD		2
#define I2C_DATA_STEP			8

#define I2C_TIMEOUT				1000000
/* Absolutely safe for status update at 100 kHz I2C: */
#define I2C_WAIT	1

void i2c_udelay(unsigned int c)
{
	udelay(c);
}

static int sprd_i2c_clear_start(unsigned long i2c_base)
{
	unsigned int tmp = readl(i2c_base + I2C_CTL);

	writel(tmp & (~I2C_START), i2c_base + I2C_CTL);

	return 0;
}

static int sprd_i2c_clear_int(unsigned long i2c_base)
{
	unsigned int tmp = readl(i2c_base + I2C_STATUS);

	writel(tmp & (~I2C_INT), i2c_base+ I2C_STATUS);

	return 0;
}

static int sprd_i2c_clear_nack(unsigned long i2c_base)
{
	unsigned int tmp = readl(i2c_base + I2C_STATUS);

	writel(tmp & (~I2C_RX_ACK), i2c_base+ I2C_STATUS);

	return 0;
}

static void sprd_i2c_dump_reg(unsigned long i2c_base)
{
	dprintf(INFO,"I2C_CTL 	= 0x%x\n", readl(i2c_base + I2C_CTL));
	dprintf(INFO,"I2C_ADDR_CFG 	= 0x%x\n", readl(i2c_base + I2C_ADDR_CFG));
	dprintf(INFO,"I2C_COUNT = 0x%x\n", readl(i2c_base + I2C_COUNT));
	dprintf(INFO,"I2C_STATUS = 0x%x\n", readl(i2c_base + I2C_STATUS));
	dprintf(INFO,"ADDR_DVD0 = 0x%x\n", readl(i2c_base + ADDR_DVD0));
	dprintf(INFO,"ADDR_DVD1 = 0x%x\n", readl(i2c_base + ADDR_DVD1));
	dprintf(INFO,"ADDR_STA0_DVD = 0x%x\n", readl(i2c_base + ADDR_STA0_DVD));

	sprd_i2c_clear_start(i2c_base);
}

static inline void sprd_i2c_ip_enable(struct sprd_i2c *pi2c)
{
	CHIP_REG_OR(pi2c->apb_base, pi2c->apb_eb);
}

static inline void sprd_i2c_ip_disable(struct sprd_i2c *pi2c)
{
	CHIP_REG_AND(pi2c->apb_base, ~pi2c->apb_eb);
}

static int sprd_wait_for_fifo_empty(unsigned long i2c_base)
{
	int status;
	int timeout = I2C_TIMEOUT;

	status = readl(i2c_base+I2C_STATUS);
	while (!(status &(FIFO_EMPTY)) && timeout--) {
		i2c_udelay(I2C_WAIT);
		status = readl(i2c_base + I2C_STATUS);
	}
	return status;
}

static int sprd_wait_for_data_ready(unsigned long i2c_base)
{
	int status;
	int timeout = I2C_TIMEOUT;
	status = readl(i2c_base + I2C_STATUS);
	while (!((status & (I2C_INT)) || status & (FIFO_FULL)) && timeout--) {
		i2c_udelay(I2C_WAIT);
		status = readl(i2c_base + I2C_STATUS);
	}
	sprd_i2c_clear_start(i2c_base);
	if (timeout <= 0) {
		dprintf(INFO,"Timed out in wait_for_data_ready: status=%04x\n", status);
		sprd_i2c_dump_reg(i2c_base);
		return -1;
	}

	return status;
}

static int sprd_wait_for_int(unsigned long i2c_base)
{
	int status;
	int timeout = I2C_TIMEOUT;
	status = readl(i2c_base + I2C_STATUS);
	while (!(status & (I2C_INT)) && timeout--) {
		i2c_udelay(I2C_WAIT);
		status = readl(i2c_base + I2C_STATUS);
	}


	if (timeout <= 0) {
		dprintf(INFO,"Timed out in wait_for_int: status=%04x\n", status);
		sprd_i2c_dump_reg(i2c_base);
		sprd_i2c_clear_start(i2c_base);
		return -1;
	}

	status = readl(i2c_base + I2C_STATUS);
	sprd_i2c_clear_int(i2c_base);
	sprd_i2c_clear_nack(i2c_base);
	sprd_i2c_clear_start(i2c_base);

	return !!(status & I2C_RX_ACK) ? -1 : 0;
}

static int sprd_i2c_opt_start(unsigned long i2c_base)
{
	int cmd = readl(i2c_base + I2C_CTL);

	writel(cmd | I2C_START, i2c_base + I2C_CTL);

	return 0;
}

static int sprd_i2c_send_stop(unsigned long i2c_base, int stop)
{
	unsigned int tmp = readl(i2c_base + I2C_CTL);

	if (stop)
		writel(tmp & (~STP_EN),i2c_base + I2C_CTL);
	else
		writel(tmp | (STP_EN), i2c_base + I2C_CTL);

	return 0;
}

static int
sprd_i2c_opt_mode(unsigned long i2c_base, int rw)
{
	int cmd = readl(i2c_base + I2C_CTL) & (~I2C_MODE);

	writel((cmd | rw << 3), i2c_base + I2C_CTL);

	return 0;
}

static int sprd_i2c_clear_irq(unsigned long  i2c_base)
{
	unsigned int tmp = readl(i2c_base + I2C_STATUS);

	writel(tmp & (~I2C_INT), i2c_base + I2C_STATUS);

	return 0;
}

static void
sprd_i2c_reset_fifo(unsigned long  i2c_base)
{
	writel(I2C_RST, i2c_base + ADDR_RST);
}

static int
sprd_i2c_write_byte(unsigned long i2c_base, unsigned char *byte, int c)
{
	int i =0;

	for(i = 0; i < c; i++) {
		writel(byte[i], i2c_base + I2C_TX);
		//dprintf(INFO,"sprd_i2c_write_byte =%x \n", byte[i]);
	}

	return 0;
}

static int
sprd_i2c_read_byte(unsigned long i2c_base, unsigned char *byte,  int c)
{
	int i =0;

	for(i = 0; i < c; i++) {
		byte[i] = (unsigned char)(readl(i2c_base + I2C_RX));
		//dprintf(INFO,"sprd_i2c_read_byte =%x \n", byte[i]);
	}

	return 0;
}

static int
sprd_i2c_writebytes(struct sprd_i2c *pi2c, struct i2c_msg *m)
{
	int rc = 0;
	unsigned long  i2c_base = (unsigned long) pi2c->base;
	int s_len = 0, msg_len = m->len;
	unsigned char *p_buf = m->buf;

	sprd_i2c_opt_mode(i2c_base, 0);
	s_len = msg_len > I2C_FIFO_DEEP ?  I2C_FIFO_DEEP : msg_len;
	rc = sprd_i2c_write_byte(i2c_base, p_buf, s_len);
	sprd_i2c_opt_start(i2c_base);
	sprd_wait_for_fifo_empty(i2c_base);
	msg_len = msg_len -s_len;
	p_buf +=s_len;

	while(msg_len) {
		s_len = msg_len < I2C_FIFO_DEEP ? msg_len : I2C_FIFO_DEEP;
		rc = sprd_i2c_write_byte(i2c_base, p_buf, s_len);
		msg_len -= s_len;
		p_buf +=s_len;
		sprd_wait_for_fifo_empty(i2c_base);
	}

	return sprd_wait_for_int(i2c_base);
	//return rc;
}

static int
sprd_i2c_readbytes(struct sprd_i2c *pi2c, struct i2c_msg *m)
{
	int rc = 0;
	unsigned long  i2c_base = (unsigned long) pi2c->base;
	int get_len = 0, msg_len = m->len;
	unsigned char *p_buf = m->buf;

	sprd_i2c_opt_mode(i2c_base, 1);
	sprd_i2c_opt_start(i2c_base);

	while (msg_len){
		sprd_wait_for_data_ready(i2c_base);
		get_len = msg_len < I2C_FIFO_DEEP ? msg_len : I2C_FIFO_DEEP;
		rc = sprd_i2c_read_byte(i2c_base,p_buf, get_len);
		msg_len -= get_len;
		p_buf +=get_len;
		sprd_i2c_clear_int(i2c_base);
	}

	return rc;
}

static void sprd_i2c_set_count(unsigned long i2c_base, uint32_t count)
{
	writel(count, i2c_base + I2C_COUNT);
}

static int sprd_i2c_set_clk(struct sprd_i2c *pi2c, int speed)
{
	unsigned long  i2c_base = (unsigned long) pi2c->base;
	uint32_t APB_clk = I2C_SRC_CLK;
	uint32_t i2c_dvd = APB_clk / (4 * speed) - 1;
	uint32_t high = ((i2c_dvd << 1) * 2) / 5;
	uint32_t  low = ((i2c_dvd << 1) * 3) / 5;
	uint32_t div0 = (high & 0xffff) << 16 | (low & 0xffff);
	uint32_t div1 =  (high & 0xffff0000) | ((low & 0xffff0000) >> 16);

	writel(div0, i2c_base + ADDR_DVD0);
	writel(div1, i2c_base + ADDR_DVD1);

	if (speed == 400000)
		writel(((6 * APB_clk) / 10000000),
			i2c_base + ADDR_STA0_DVD);
	else if (speed == 100000)
		writel(((4 * APB_clk) / 1000000),
			i2c_base + ADDR_STA0_DVD);
	else if (speed == 1000000)
		writel(((8 * APB_clk) / 10000000),
			i2c_base + ADDR_STA0_DVD);
	else if (speed == 1700000)
		writel(((8 * APB_clk) / 10000000),
			i2c_base + ADDR_STA0_DVD);
	else if (speed == 3400000)
		writel(((8 * APB_clk) / 10000000),
			i2c_base + ADDR_STA0_DVD);

	return 0;
}

static void sprd_i2c_enable(struct sprd_i2c *pi2c)
{
	unsigned long  i2c_base = (unsigned long)  pi2c->base;
	unsigned int tmp = readl(i2c_base + I2C_CTL);

	tmp = tmp & ~(I2C_EN | I2C_TRIM_OPT | (0xF << FIFO_AF_LVL) |
		(0xF << FIFO_AE_LVL));
	tmp	|=(I2C_FIFO_FULL_THLD << FIFO_AF_LVL) |
		(I2C_FIFO_EMPTY_THLD << FIFO_AE_LVL) |
		I2C_DVD_OPT;
	writel(tmp, i2c_base + I2C_CTL);

	//dprintf(INFO,"i2c%d, freq=%d\n",  pi2c->i2c.index, pi2c->freq);

	sprd_i2c_set_clk(pi2c, pi2c->freq);
	sprd_i2c_reset_fifo(i2c_base);
	sprd_i2c_clear_irq(i2c_base);
	tmp = readl(i2c_base + I2C_CTL);
	writel(tmp | I2C_EN | I2C_INT_EN | I2C_NACK_EN | I2C_TRANS_EN, i2c_base + I2C_CTL);
}

static int
sprd_i2c_handle_msg(struct sprd_i2c *pi2c, struct i2c_msg *m, int last)
{
	unsigned long  i2c_base = (unsigned long) pi2c->base;

	writel(m->addr << 1, i2c_base + I2C_ADDR_CFG);
	sprd_i2c_send_stop(i2c_base, last);
	sprd_i2c_set_count(i2c_base, m->len);

	if ((m->flags & I2C_M_RD))
		return sprd_i2c_readbytes(pi2c, m);
	 else
		return sprd_i2c_writebytes(pi2c, m);
}

static int
sprd_i2c_master_xfer(i2cDev_t *i2c, struct i2c_msg *m, int n)
{
	int im = 0, ret = 0;
	struct sprd_i2c *pi2c = (struct sprd_i2c *)i2c;

	sprd_i2c_ip_enable(pi2c);
	sprd_i2c_enable(pi2c);

	for (im = 0; ret >= 0 && im != n; im++){
		ret = sprd_i2c_handle_msg(pi2c, &m[im], im == n - 1);
		if(ret < 0)
			break;
	}

	sprd_i2c_ip_disable(pi2c);
	return (ret >= 0) ? im : -1;
}

static int sprd_i2c_probe(struct sprd_i2c *pi2c)
{
	sprd_i2c_enable(pi2c);

	return 0;
}

static const struct i2c_algorithm sprd_i2c_algo = {
	.master_xfer = sprd_i2c_master_xfer,
};

static int sprd_i2c_bus_init(int num)
{
	struct sprd_i2c *pi2c = &I2C_BUS[num];
	i2cDev_t *i2c = &I2C_BUS[num].i2c;

	i2c->state = IDLE;
	i2c->index = num;
	i2c->algo = &sprd_i2c_algo;

	sprd_i2c_ip_enable(pi2c);
	sprd_i2c_probe(&I2C_BUS[num]);
	sprd_i2c_dump_reg(pi2c->base);
	sprd_i2c_ip_disable(pi2c);

	dprintf(INFO,"%s is ok!\n", I2C_BUS[num].bus_name);
	return 0;
}

void sprd_i2c_init(void)
{
	int i  = 0;
	for (i =0 ; i < SPRD_I2C_NUM; i++) {
//		dprintf(INFO,"sprd_i2c_init %d %d  \n",i,SPRD_I2C_NUM);
		sprd_i2c_bus_init(i);
	}
	return;
}

