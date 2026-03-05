/*
 *  stk3x1x.c - Linux kernel modules for sensortek stk301x, stk321x, stk331x 
 *  , and stk3410 proximity/ambient light sensor
 *
 *  Copyright (C) 2012~2015 Lex Hsieh / sensortek <lex_hsieh@sensortek.com.tw>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/errno.h>
//#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include   <linux/fs.h>   
#include  <asm/uaccess.h> 
#include <linux/of_gpio.h>
#include <soc/sprd/board.h>


struct i2c_client *tps65132_i2c_client;


/*****************************************************************************
 * Data Structure
 *****************************************************************************/


static int tps65132_i2c_txdata(char *txdata, int length)
{
	int ret = 0;
	struct i2c_client *client = tps65132_i2c_client;

	struct i2c_msg msg[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= length,
			.buf	= txdata,
		},
	};
	//printk("txdata: info==>name=%s addr=0x%x\n", client->name, client->addr);

	if (i2c_transfer(client->adapter, msg, 1) != 1) {
		ret = -EIO;
		printk("tps65132_i2c_txdata i2c write error\n");
	}

	return ret;
}

/***********************************************************************************************
Name	:	 ft5x0x_write_reg

Input	:	addr -- address
                     para -- parameter

Output	:

function	:	write register of ft5x0x

***********************************************************************************************/
int tps65132_write_bytes(unsigned char addr, unsigned char value)
{
  	int ret = 0;
	u8 buf[3];
  
 	buf[0] = addr;
	buf[1] = value;
	ret = tps65132_i2c_txdata(buf, 2);
	if (ret < 0) {
		pr_err("write reg failed! 0x%x ret: %d\n", buf[0], ret);
		return -1;
	}

  	//printk("tps65132 write data  addr=0x%x, value=0x%x !!\n",addr,value);
	return ret;
}

void tps65132_config_voltage(void)
{
	unsigned char cmd = 0x0;
	unsigned char data = 0xFF;
	unsigned char cmd1 = 0x1;
	unsigned char data1 = 0xFF;
	cmd = 0x00;
	cmd1 = 0x01;
#if defined(LCM_3POWER_SET_VOLTAGE)
	data = LCM_3POWER_SET_VOLTAGE;
	data1 = LCM_3POWER_SET_VOLTAGE;
#else
	data = 0x0f;
	data1 = 0x0f;
#endif

	tps65132_write_bytes(cmd,data);
	mdelay(10);
	tps65132_write_bytes(cmd,data);
	mdelay(10);
	tps65132_write_bytes(cmd1,data1);
	mdelay(10);

} 

/*****************************************************************************
 * Function
 *****************************************************************************/
static int tps65132_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
  //printk("tps65132_iic_probe\n");
  tps65132_i2c_client = client;
  return 0;
}

static int tps65132_remove(struct i2c_client *client)
{
  //printk("tps65132_remove\n");
  tps65132_i2c_client = NULL;
  i2c_unregister_device(client);
  return 0;
}

static const struct i2c_device_id tps65132_id[] =
{
    { "tps65132", 0},
    {}
};
	
MODULE_DEVICE_TABLE(i2c, tps65132_id);

static struct of_device_id tps65132_match_table[] = {
	{ .compatible = "bias_lcm,tps65132_power", },
	{ },
};

struct i2c_driver tps65132_i2c_driver =
{
    .driver = {
        .name = "tps65132",
		.owner = THIS_MODULE,	
#ifdef CONFIG_OF		
		.of_match_table = tps65132_match_table,		
#endif		
    },
    .probe = tps65132_probe,
    .remove = tps65132_remove,
    .id_table = tps65132_id,
};


MODULE_DESCRIPTION("Sensortek tps65132 lcm 5v_power driver");
MODULE_LICENSE("GPL");

