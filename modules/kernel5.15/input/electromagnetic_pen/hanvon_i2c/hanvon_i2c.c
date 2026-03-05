/*
 *
 * Electromagnetic Pen I2C Driver for Hanvon
 *
 * Copyright (C) 1999-2012  Hanvon Technology Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * !!!!!!!!!!!!!!
 * Nvidia tegra 2 ventana demo board.
 * OS: android 4.0.3
 * version: 0.3.0
 * Features:
 * 		1. firmware update function
 *		2. work with calibration App
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/async.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/jiffies.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <soc/sprd/board.h>
//#include "tp_suspend.h"

#define HV_DEBUG(fmt, args...) do { \
    printk("[Hanvon emr]%s:"fmt"\n", __func__, ##args); \
} while (0)

#define HANVON_I2C_NAME                                 "hanvon0868"
#define MAX_EVENTS                                      5
#define SCREEN_PORTRAIT
#ifdef SCREEN_PORTRAIT
#define MAX_X                                          0x3692    // 0x1cfe
#define MAX_Y                                          0x5750    //0x27de
#else
#define MAX_X                                          0x5750    //0x27de
#define MAX_Y                                          0x3692    //0x1cfe
#endif
#define MAX_PRESSURE                                   4095          //1024
#define HW0868_CALIBRATE

static int exchange_x_y_flag 	= 0;
static int revert_x_flag 		= 0;
static int revert_y_flag 		= 0;
#ifdef ZCFG_LCM_WIDTH
static int screen_max_x = ZCFG_LCM_WIDTH;
#else
static int screen_max_x = 1872;
#endif
#ifdef ZCFG_LCM_HEIGHT
static int screen_max_y = ZCFG_LCM_HEIGHT;
#else
static int screen_max_y = 1404;
#endif
bool hanvon_is_update = false;
//extern bool touchscreen_report_flag;

//for srs2801r
#if defined(CONFIG_MATCH_SRS2801)
#define SCR_X                                   screen_max_x
#define SCR_Y                                   screen_max_y
#define DELTA_Y_T				10	//top delta y 
#define DELTA_Y_B                               20	//bottom delta y
#else
#define SCR_X                                   screen_max_x
#define SCR_Y                                   screen_max_y
#endif

#ifdef HANVONPEN_USE_SECURITY
#define SCREEN_SECURITY
#endif

#define MAX_PACKET_SIZE_S                                 28
#define MAX_PACKET_SIZE_C                                 10                    //7

#define DEBUG_SHOW_RAW                                  0X00000001
#define DEBUG_SHOW_COORD                                0X00000010

#define UPDATE_SLAVE_ADDR                               0x34

#define HW0868_CMD_RESET                                0x08680000
#define HW0868_CMD_CONFIG_HIGH                          0x08680001
#define HW0868_CMD_CONFIG_LOW                           0x08680002
#define HW0868_CMD_UPDATE                               0x08680003
#define HW0868_CMD_GET_VERSION                          0x08680004
#define HW0868_CMD_CALIBRATE                            0x08680005

/* define pen flags, 10-bytes protocal. */
#define PEN_POINTER_UP					   0xc0//0xa0
#define PEN_POINTER_DOWN				   0xc1//0xa1
#define PEN_BUTTON_UP					   0xc2//0xa2
#define PEN_BUTTON_DOWN					   0xc3//0xa3
#define PEN_BUTTON2_UP					   0xc4//
#define PEN_BUTTON2_DOWN				   0xc5//
#define PEN_BUTTON3_UP					   0xc8//
#define PEN_BUTTON3_DOWN				   0xc9//
#define PEN_RUBBER_UP					   0xd0//0xa4
#define PEN_RUBBER_DOWN					   0xd1//0xa5
#define PEN_ALL_LEAVE					   0x80//0xe0


//#define TWO_KEY
//#define THREE_KEY


//extern int cover_opened;
#if defined(CONFIG_MATCH_SRS5001) || defined(CONFIG_MATCH_SHENGTENG) || defined(CONFIG_MATCH_AITI) || defined(CONFIG_MATCH_HS_TOF)
int led_pen_flag = 0;
extern void srs5001r_set_led_r(int onoff);
#endif
#if defined(CONFIG_MATCH_SRS2801)
int led_pen_flag = 0;
extern void srs5001r_set_led_g(int onoff);
#endif

//static struct delayed_work	hanvon_resume_work;
//static struct workqueue_struct *resume_workqueue;

struct hanvon_pen_data
{
    u16 x;
    u16 y;
    u16 pressure;
    u8 flag;
};

struct hanvon_i2c_chip {
	unsigned char * chipname;
	struct workqueue_struct *ktouch_wq;
	struct work_struct work_irq;
	struct mutex mutex_wq;
	struct mutex fw_lock;
	bool fw_done;
	struct i2c_client *client;
	unsigned char work_state;
	struct input_dev *p_inputdev;

	int irq;
	int irq_pin;
	int cfg_pin;
	int pwr_pin;
	int rst_pin;
	int rst_val;
	int en_pin;
	//struct tp_device tp;
	struct regulator *supply;
};

//global I2C client.
static struct i2c_client *g_client;
struct hanvon_i2c_chip *g_phid;

// DEBUG micro, for user interface.
static unsigned int debug = 1;

/* when pen detected, this flag is set 1 */
static volatile int isPenDetected = 0;

// version number
static unsigned char ver_info[9] = {0};
//static int ver_size = 9;

#ifdef HW0868_CALIBRATE
// calibration parameter
static bool isCalibrated = false;
//static int a[7];
static int A = 65535, B = 0, C = 16, D = 0, E = 65535, F = 0, scale = 65536;
#endif

static int fw_update = -1;

#define hw0868_dbg_raw(fmt, args...)        \
	    if(debug & DEBUG_SHOW_RAW) \
        printk(KERN_INFO "[HW0868 raw]: "fmt, ##args);
#define hw0868_dbg_coord(fmt, args...)    \
	    if(debug & DEBUG_SHOW_COORD) \
        printk(KERN_INFO "[HW0868 coord]: "fmt, ##args);

//extern bool emr_driver_load_flag;

static void hw0868_reset(void)
{
	if(!g_phid)
		return;
	gpio_direction_output(g_phid->rst_pin, 0);
	msleep(150);
	gpio_direction_output(g_phid->rst_pin, 1);
	msleep(150);
}

#if 1
static void hw0868_set_config_pin(int i)
{
	if(!g_phid)
		return;

	if (i == 0)
		gpio_direction_output(g_phid->cfg_pin, 0);
	if (i == 1)
		gpio_direction_output(g_phid->cfg_pin, 1);
}

static	int hw0868_get_version(struct i2c_client *client, char *ver_buf)
{
	int ret = -1;
	unsigned char ver_cmd[] = {0xcd, 0x5f};
	#if 0
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(ver_cmd),
			.buf = ver_cmd,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = sizeof(ver_buf),
			.buf = (char *)ver_buf,
		},
	};

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0)
		return ret;
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;
	return ret;
	#endif
	ret = i2c_master_send(client, ver_cmd, 2);
	if (ret>=0) {
		ret = i2c_master_recv(client, ver_buf, sizeof(ver_buf));
	}
	return ret;
}

static int read_calibrate_param(void)
{
	int a[7];
	//mm_segment_t old_fs;
	struct file *file = NULL;
	//printk(KERN_INFO "kernel read calibrate param.\n");
	//old_fs = get_fs();
	//set_fs(get_ds());
	file = filp_open_block("/data/calibrate", O_RDONLY, 0);
	if (file == NULL)
		return -1;
	if (file->f_op->read == NULL)
		return -1;

	// TO DO: read file
	if (file->f_op->read(file, (unsigned char*)a, sizeof(int)*7, &file->f_pos) == 28)
	{
		//printk(KERN_INFO "calibrate param: %d, %d, %d, %d, %d, %d, %d\n", a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
		A = a[1];
		B = a[2];
		C = a[0];
		D = a[4];
		E = a[5];
		F = a[3];
		scale = a[6];
		isCalibrated = true;
	}
	else
	{
		filp_close(file, NULL);
		//set_fs(old_fs);
		return -1;
	}

	filp_close(file, NULL);
	//set_fs(old_fs);
	return 0;
}
#endif
#if 1
int fw_i2c_master_send(const struct i2c_client *client, const char *buf, int count)
{
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;

	//msg.addr = client->addr;
	//printk(KERN_INFO "11111111fw_i2c_master_send111111");
	msg.addr = UPDATE_SLAVE_ADDR;
	msg.flags = client->flags & I2C_M_TEN;
	msg.len = count;
	msg.buf = (char *)buf;
#if 0
	if (fw_update == 1)
		msg.scl_rate = 200 * 1000;
	else
		msg.scl_rate = 400 * 1000;
#endif
	ret = i2c_transfer(adap, &msg, 1);
	//printk(KERN_INFO "-------[hanvon] fw_i2c_master_send  ret = %d---------", ret);
	/* 
	 * If everything went ok (i.e. 1 msg transmitted), return #bytes
	 * transmitted, else error code.
	 */
	return (ret == 1) ? count : ret;
}

int fw_i2c_master_recv(const struct i2c_client *client, char *buf, int count)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;
	int ret;

	//msg.addr = client->addr;
	//printk(KERN_INFO "222222fw_i2c_master_recv22222");
	msg.addr = UPDATE_SLAVE_ADDR;
	msg.flags = client->flags & I2C_M_TEN;
	msg.flags |= I2C_M_RD;
	msg.len = count;
	msg.buf = buf;
#if 0
	if (fw_update == 1)
		msg.scl_rate = 200 * 1000;
	else
		msg.scl_rate = 400 * 1000;
#endif
	ret = i2c_transfer(adap, &msg, 1);
	//printk(KERN_INFO "-------[hanvon] fw_i2c_master_recv  ret = %d---------", ret);
	/*
	 * If everything went ok (i.e. 1 msg received), return #bytes received,
	 * else error code.
	 */
	return (ret == 1) ? count : ret;
}

static ssize_t hw0868_i2c_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count, i;
	unsigned char csw_packet[13] = {1};
	printk(KERN_INFO "Receive CSW package.\n");
	//struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	count = fw_i2c_master_recv(g_client, csw_packet, 13);
	if (count < 0)
	{
		return -1;
	}
	//memcpy(buf, csw_packet, count);
	printk(KERN_INFO "[num 01] read %d bytes.\n", count);
	for(i = 0; i < count; i++)
	{
		printk(KERN_INFO "%.2x \n", csw_packet[i]);
	}
	return sprintf(buf, "%s", csw_packet);
}

static ssize_t hw0868_i2c_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int cmd ;
	//struct i2c_client *client;
	//client = container_of(dev, struct i2c_client, dev);
	//printk(KERN_INFO "%s is called, count=%d.\n", __func__, (int )count);
	if ((count == 31) && (buf[1] == 0x57) && (buf[0] == 0x48))
	{
		//printk(KERN_INFO "Send CBW package.\n");
		ret = fw_i2c_master_send(g_client, buf, count);
		return ret;
	}

	// transfer file
	if (count == 32)
	{
		//printk(KERN_INFO "Transfer file.\n");
		ret = fw_i2c_master_send(g_client, buf, count);
		return ret;
	}
	
	cmd = *((int *)buf);
	if ((cmd & 0x08680000) != 0x08680000)
	{
		printk(KERN_INFO "Invalid command (0x%08x).\n", cmd);
		return -1;
	}

	switch(cmd)
	{
		case HW0868_CMD_RESET:
			printk(KERN_INFO "Command: reset.\n");
			hw0868_reset();
			break;
		case HW0868_CMD_CONFIG_HIGH:
			printk(KERN_INFO "Command: set config pin high.\n");
			hw0868_set_config_pin(1);
			hanvon_is_update = true;
			break;
		case HW0868_CMD_CONFIG_LOW:
			printk(KERN_INFO "Command: set config pin low.\n");
			hw0868_set_config_pin(0);
			hanvon_is_update = false;
			break;
		case HW0868_CMD_GET_VERSION:
			printk(KERN_INFO "Command: get firmware version.\n");
			hw0868_get_version(g_client,ver_info);
			break;
		case HW0868_CMD_CALIBRATE:
			printk(KERN_INFO "Command: Calibrate.\n");
			read_calibrate_param();
			break;
	}
	return count;
}

static DEVICE_ATTR(hw0868_entry, S_IRUGO | S_IWUSR, hw0868_i2c_show, hw0868_i2c_store);

// get version
static ssize_t hw0868_version_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	char ver_buf[9] = {0};
	int ret = 0;
	ret = hw0868_get_version(g_client,ver_buf);
	if (ret < 0) return sprintf(buf, "get version failed");
	else
	return sprintf(buf, "version number: %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x\n", 
			ver_buf[0],ver_buf[1],ver_buf[2],ver_buf[3],ver_buf[4], ver_buf[5], ver_buf[6],ver_buf[7],ver_buf[8]);
}

static ssize_t hw0868_version_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}
static DEVICE_ATTR(version, S_IRUGO | S_IWUSR, hw0868_version_show, hw0868_version_store);

#define FW_BIN_FILEPATH        "/sdcard/"
#define FILE_NAME_LENGTH       128
#define FLASH_PACKET_LENGTH 32
static int fts_read_file(char *file_name, u8 **file_buf)
{
    int ret = 0;
	//ssize_t ret1 = 0;
    char file_path[FILE_NAME_LENGTH] = { 0 };
    struct file *filp = NULL;
    struct inode *inode;
    //mm_segment_t old_fs;
    loff_t pos;
    loff_t file_len = 0;

    if ((NULL == file_name) || (NULL == file_buf)) {
        HV_DEBUG("filename/filebuf is NULL");
        return -EINVAL;
    }

    snprintf(file_path, FILE_NAME_LENGTH, "%s%s", FW_BIN_FILEPATH, file_name);
    filp = filp_open_block(file_path, O_RDONLY, 0);
    if (IS_ERR(filp)) {
        HV_DEBUG("open %s file fail", file_path);
        return -ENOENT;
    }

#if 1
    inode = filp->f_inode;
#else
    /* reserved for linux earlier verion */
    inode = filp->f_dentry->d_inode;
#endif

    file_len = inode->i_size;
    *file_buf = (unsigned char *)vmalloc(file_len);
    if (NULL == *file_buf) {
        HV_DEBUG("file buf malloc fail");
        filp_close(filp, NULL);
        return -ENOMEM;
    }
    //old_fs = get_fs();
    //set_fs(KERNEL_DS);
    pos = 0;
    //ret1 = vfs_read(filp, *file_buf, file_len , &pos);
    //if (ret1 < 0)
        //HV_DEBUG("read file fail");
    HV_DEBUG("file len:%d read len:%d pos:%d", (unsigned int)file_len, ret, (unsigned int)pos);
    filp_close(filp, NULL);
    //set_fs(old_fs);

    return ret;
}

unsigned char calculate_checksum(unsigned char *buffer, int len)
{
	unsigned char checksum = 0;
	int	ix;
	for(ix = 0; ix < len; ix++)
	{
		checksum += buffer[ix];
		checksum &= 0xFF;
	}
	return checksum;
}

static int fw_update_fun(u8 *fw_buf, int fw_len)
{
	int ret = 0,i,j;
	int packet_number,remainder,packet_len,offset;
	unsigned char data_checksum = 0;
	unsigned char packet_buf[FLASH_PACKET_LENGTH] = { 0 };
	unsigned char csw_packet_header[]={0x48,0x57,0x30,0x38,0xaa,0x55,0x55,0xaa};
	unsigned char csw_packet[13] = {0};
	unsigned char cbw[32]={0x48,0x57,0x30,0x38,0xaa,0x55,0x55,0xaa,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x4b,0x04,0x00,0x00,0x00,0x00,0x80,0xd0,0x00,0x00,0x45,0x44,0x00,0x00,0x00,0x00};
	hw0868_set_config_pin(1);
	msleep(100);
	hw0868_reset();
	hanvon_is_update = true;
	msleep(500);
	HV_DEBUG("erase flash ...");
	cbw[21] = fw_len&0x000000ff;
	cbw[22] = (fw_len&0x0000ff00)>>8;
	cbw[23] = (fw_len&0x00ff0000)>>16;
	cbw[24] = (fw_len&0xff000000)>>24;
	ret = fw_i2c_master_send(g_client, cbw, sizeof(cbw));
    if (ret < 0) {
        HV_DEBUG("send cbw fail,mode=%2x",cbw[15]);
		goto i2c_err;
    }
	msleep(1800);
	ret = fw_i2c_master_recv(g_client, csw_packet, 13);
	if (ret < 0)
	{
		HV_DEBUG("Receive CSW package fail.\n");
		goto i2c_err;
	}
	if (!memcmp(csw_packet,csw_packet_header,sizeof(csw_packet_header)))
	{
		HV_DEBUG("CSW: dCSWSignature isn't H W 6 8\n");
		ret = -1;
		goto i2c_err;
	}
	if (csw_packet[12]==0)
	{
		HV_DEBUG("erase flash ok");
	}
	else
	{
		HV_DEBUG("erase flash err=%d",csw_packet[12]);
		ret = -1;
		goto i2c_err;
	}
	
	data_checksum = calculate_checksum(fw_buf, fw_len);
	
	cbw[8]=data_checksum;
	cbw[15]=0x3a;
	ret = fw_i2c_master_send(g_client, cbw, sizeof(cbw));
    if (ret < 0) {
        HV_DEBUG("send cbw fail,mode=%2x",cbw[15]);
		goto i2c_err;
    }
	msleep(800);
	packet_number = fw_len / FLASH_PACKET_LENGTH;
    remainder = fw_len % FLASH_PACKET_LENGTH;
    if (remainder > 0)
        packet_number++;
    packet_len = FLASH_PACKET_LENGTH;
	for (i = 0; i < packet_number; i++) {
        offset = i * FLASH_PACKET_LENGTH;
        /* last packet */
        if ((i == (packet_number - 1)) && remainder)
            packet_len = remainder;

        for (j = 0; j < packet_len; j++) {
            packet_buf[j] = fw_buf[offset + j];
        }
		ret = fw_i2c_master_send(g_client, packet_buf, packet_len);
        if (ret < 0) {
            HV_DEBUG("fw write fail,packet_number=%d",i);
            goto i2c_err;
        }
		else 
		{
			HV_DEBUG("fw write ,packet_number=%d",i);
		}
        //msleep(1);
    }
	msleep(80);
	memset(csw_packet,0,sizeof(csw_packet));
	ret = fw_i2c_master_recv(g_client, csw_packet, 13);
	if (ret < 0)
	{
		HV_DEBUG("Receive CSW package fail.\n");
		goto i2c_err;
	}
	if (!memcmp(csw_packet,csw_packet_header,sizeof(csw_packet_header)))
	{
		HV_DEBUG("CSW: dCSWSignature isn't H W 6 8\n");
		ret = -1;
		goto i2c_err;
	}
	if (csw_packet[12]==0&&csw_packet[8]==data_checksum)
	{
		HV_DEBUG("upgrade fw success");
	}
	else
	{
		HV_DEBUG("upgrade fw err=%d",csw_packet[12]);
		ret = -1;
		goto i2c_err;
	}
    ret = 0;
i2c_err:
	hw0868_set_config_pin(0);
	msleep(100);
	hw0868_reset();
	hanvon_is_update = false;
    return ret;
}

int firmware_update(char *fw_name, bool force)
{
    int fw_file_len = 0, ret;
    unsigned char *fw_file_buf = NULL;
    HV_DEBUG("start firmware update\n");
    ret = fts_read_file(fw_name, &fw_file_buf);
    if (ret < 0xD070) {
        HV_DEBUG("read fw bin file(%s) fail, len:%d", fw_name, ret);
		ret = -1;
        goto err_bin;
    }
    fw_file_len = ret;
	ret = fw_update_fun(fw_file_buf,fw_file_len);
err_bin:
    if (fw_file_buf) {
        vfree(fw_file_buf);
        fw_file_buf = NULL;
    }
    return ret;
}

static ssize_t hw0868_updatefirmware_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	return sprintf(buf, "%d\n", fw_update);
}

static ssize_t hw0868_updatefirmware_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char fwname[FILE_NAME_LENGTH] = { 0 };

    if ((count <= 1) || (count >= FILE_NAME_LENGTH - 32)) {
        printk("fw bin name's length(%d) fail", (int)count);
        return -EINVAL;
    }
    memset(fwname, 0, sizeof(fwname));
    snprintf(fwname, FILE_NAME_LENGTH, "%s", buf);
    fwname[count - 1] = '\0';

    firmware_update(fwname, 0);

	return count;
}
static DEVICE_ATTR(updatefirmware, S_IRUGO | S_IWUSR, hw0868_updatefirmware_show, hw0868_updatefirmware_store);
#endif

// misc device
static long hw0868_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static struct file_operations hanvon_cdev_fops = {
	.owner= THIS_MODULE,
	.unlocked_ioctl= hw0868_ioctl,
};

static struct miscdevice hanvon_misc_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "hw0868",
	.fops = &hanvon_cdev_fops,
};

static struct input_dev * allocate_hanvon_input_dev(void)
{
	int ret;
	struct input_dev *p_inputdev=NULL;
	int max_x,max_y;

	p_inputdev = input_allocate_device();
	if(p_inputdev == NULL)
	{
		return NULL;
	}

	p_inputdev->name = "hanvon(DigitalPen)";
	p_inputdev->phys = "I2C";
	p_inputdev->id.bustype = BUS_I2C;
	
	set_bit(EV_ABS, p_inputdev->evbit);
	__set_bit(INPUT_PROP_DIRECT, p_inputdev->propbit);
	__set_bit(EV_ABS, p_inputdev->evbit);
	__set_bit(EV_KEY, p_inputdev->evbit);
	__set_bit(BTN_TOUCH, p_inputdev->keybit);
	__set_bit(BTN_TOOL_PEN, p_inputdev->keybit);
	__set_bit(BTN_STYLUS, p_inputdev->keybit);
	__set_bit(BTN_TOOL_RUBBER, p_inputdev->keybit);
	#ifdef TWO_KEY
	__set_bit(BTN_STYLUS2, p_inputdev->keybit);
	#endif
	#ifdef THREE_KEY
	__set_bit(BTN_STYLUS2, p_inputdev->keybit);
	__set_bit(BTN_STYLUS3, p_inputdev->keybit);
	#endif

#ifdef HW0868_CALIBRATE
	if (exchange_x_y_flag) {
		max_x = screen_max_y;
		max_y = screen_max_x;
    }
	else
	{
		max_x = screen_max_x;
		max_y = screen_max_y;
	}
	input_set_abs_params(p_inputdev, ABS_X, 0, max_x, 0, 0);
	input_set_abs_params(p_inputdev, ABS_Y, 0, max_y, 0, 0);
#else
	input_set_abs_params(p_inputdev, ABS_X, 0, MAX_X, 0, 0);
	input_set_abs_params(p_inputdev, ABS_Y, 0, MAX_Y, 0, 0);
#endif
	
	input_set_abs_params(p_inputdev, ABS_PRESSURE, 0, MAX_PRESSURE, 0, 0);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
	input_set_events_per_packet(p_inputdev, MAX_EVENTS);
#endif

	ret = input_register_device(p_inputdev);
	if(ret) 
	{
		printk(KERN_INFO "Unable to register input device.\n");
		input_free_device(p_inputdev);
		p_inputdev = NULL;
	}
	
	return p_inputdev;
}

static struct hanvon_pen_data hanvon_get_packet(struct hanvon_i2c_chip *phic)
{
	struct hanvon_pen_data data = {0};
	struct i2c_client *client = phic->client;
	u8 *x_buf, buf[MAX_PACKET_SIZE_S] = {};
	int count;
	int package_size = 0;
	int reserve = 0;
#ifdef HW0868_CALIBRATE
        int sum_x, sum_y;
#endif
	do {
		//mdelay(2);
		if(package_size == 0){
			count = i2c_master_recv(client, buf, MAX_PACKET_SIZE_C);
			if (buf[0] == 0xfa){
				//security
				package_size = MAX_PACKET_SIZE_S;
				return data;
			}else{
				package_size = MAX_PACKET_SIZE_C;
			}
		}else{
			count = i2c_master_recv(client, buf, package_size);
		}
	}
	while(count == EAGAIN);
	
	if(count <= 0)
	{
		//printk("hanvon pen: invalid data is found\r\n");
		hw0868_reset();
		return data;
	}
	if(package_size == MAX_PACKET_SIZE_S){
#if 0
	printk("count->%02d, data->", count);
	for (count = 0; count < MAX_PACKET_SIZE; count++)
		printk("%02x ", buf[count]);
	printk("\r\n");
#endif
		x_buf = &buf[3];
	}else{
		x_buf = buf;
	}
	//if (x_buf[0] == 0x80)
	//if (x_buf[0] == 0xa0)
	//{
	//	printk("[hanvon] Get version number ok!\n");
	//	memcpy(ver_info, x_buf, ver_size);
	//}
	data.flag = x_buf[0]&0xff;
	data.x |= ((x_buf[1]&0x7f) << 9) | (x_buf[2] << 2) | ((x_buf[9]&0x0c) >> 2); // x
	data.y |= ((x_buf[3]&0x7f) << 9) | (x_buf[4] << 2) | (x_buf[9]&0x03); // y
	data.pressure |= (x_buf[5] << 7) | (x_buf[6]);  // pressure

#ifdef HW0868_CALIBRATE
	//printk("[hanvon] raw data.x: %d, data.y:%d, data.flag: %d, data.pressure: %d \n",data.x,data.y,data.flag,data.pressure);
	// transform raw coordinate to srceen coord

	reserve = data.x * SCR_X / MAX_X;
	data.x = data.y * SCR_Y / MAX_Y;
	data.y = reserve;
	//data.y = SCR_Y - data.y;
	data.x  = SCR_X - data.x ;

#if defined(CONFIG_MATCH_SRS2801)
	if(data.y < DELTA_Y_T)
                data.y = 0;
	else if(data.y > (SCR_Y - DELTA_Y_B))
		data.y = SCR_Y;
	else
		data.y = (data.y - DELTA_Y_T) * SCR_Y / (SCR_Y - DELTA_Y_T - DELTA_Y_B);
#endif
	//printk("[hanvon] cali data.x: %d, data.y:%d \n",data.x, data.y);

	// perform calibrate
	if(isCalibrated)//false
	{
		sum_x = data.x*A + data.y*B + C;
		if ((sum_x % scale) > 32768)
		{
			data.x = (sum_x >> 16) + 1;
		}
		else
		{
			data.x = sum_x >> 16;
		}
		
		sum_y = data.x*D + data.y*E + F;
		if ((sum_y % scale) > 32768)
		{
			data.y = (sum_y >> 16) + 1;
		}
		else
		{
			data.y = sum_y >> 16;
		}
		//printk("calibrated data.x: %d, data.y:%d \n",data.x, data.y);
	}
#else
	{
#if defined(CONFIG_MATCH_SRS2801)
/*
		u16 temp;
		temp = data.x;
		data.x = data.y;
		data.y = temp;
		//data.x = MAX_X - data.x;
		data.y = MAX_Y - data.y;
*/
#else
		//#ifndef SCREEN_SECURITY
		//data.x = MAX_X - data.x;
		//#endif

//rotate 180
		//data.x = MAX_X - data.x;
		//data.y = MAX_Y - data.y;
//
#endif
	}
#endif
	return data;
}

static int hanvon_report_event(struct hanvon_i2c_chip *phic)
{
	struct hanvon_pen_data data = {0};
	static int last_status = 0;

	data = hanvon_get_packet(phic);
	// save version
	if (data.flag == 0xa0)
		return 0;
 //	printk("[hanvon_report_event]x:%d y:%d\r\n", data.x, data.y);

	switch(data.flag)
	{
		/* side key events */
		case PEN_BUTTON2_DOWN:
		case PEN_BUTTON3_DOWN:      /*key2和key3可以根据需要自定义上报不同的event,这里是不区分key*/     
		case PEN_BUTTON_DOWN:
		{
			input_report_abs(phic->p_inputdev, ABS_X, data.x);
			input_report_abs(phic->p_inputdev, ABS_Y, data.y);
			input_report_abs(phic->p_inputdev, ABS_PRESSURE, data.pressure);
			input_report_key(phic->p_inputdev, BTN_TOUCH, 1);
			//touchscreen_report_flag = false;
			input_report_key(phic->p_inputdev, BTN_TOOL_PEN, 1);
			input_report_key(phic->p_inputdev, BTN_STYLUS, 1);
			break;
		}
		case PEN_BUTTON2_UP:
		case PEN_BUTTON3_UP:
		case PEN_BUTTON_UP:
		{
			input_report_abs(phic->p_inputdev, ABS_X, data.x);
			input_report_abs(phic->p_inputdev, ABS_Y, data.y);
			input_report_abs(phic->p_inputdev, ABS_PRESSURE, data.pressure);
			input_report_key(phic->p_inputdev, BTN_TOUCH, 0);
			//touchscreen_report_flag = true;
			input_report_key(phic->p_inputdev, BTN_TOOL_PEN, 0);
			input_report_key(phic->p_inputdev, BTN_STYLUS, 0);
			break;
		}
		case PEN_RUBBER_DOWN:
		{   
			input_report_abs(phic->p_inputdev, ABS_X, data.x);
			input_report_abs(phic->p_inputdev, ABS_Y, data.y);
			input_report_abs(phic->p_inputdev, ABS_PRESSURE, data.pressure);
			input_report_key(phic->p_inputdev, BTN_TOOL_RUBBER, 1);
			input_report_key(phic->p_inputdev, BTN_TOUCH, 1);
			//touchscreen_report_flag = false;
			last_status = data.flag;
			break;
        	}
		case PEN_RUBBER_UP:
		{
			input_report_abs(phic->p_inputdev, ABS_X, data.x);
			input_report_abs(phic->p_inputdev, ABS_Y, data.y);
			input_report_abs(phic->p_inputdev, ABS_PRESSURE, data.pressure);
			input_report_key(phic->p_inputdev, BTN_TOUCH, 0);
			//touchscreen_report_flag = true;
			input_report_key(phic->p_inputdev, BTN_TOOL_RUBBER, 0);
			last_status = 0;
			break;
		}
		case PEN_POINTER_DOWN:
		{
			input_report_abs(phic->p_inputdev, ABS_X, data.x);
			input_report_abs(phic->p_inputdev, ABS_Y, data.y);
			input_report_abs(phic->p_inputdev, ABS_PRESSURE, data.pressure);
			input_report_key(phic->p_inputdev, BTN_TOUCH, 1);
			//touchscreen_report_flag = false;
			input_report_key(phic->p_inputdev, BTN_TOOL_PEN, 1);
			last_status = data.flag;
			#if defined(CONFIG_MATCH_SRS5001) || defined(CONFIG_MATCH_SHENGTENG) || defined(CONFIG_MATCH_AITI) || defined(CONFIG_MATCH_HS_TOF)
			if (led_pen_flag == 0)
                        {
                                led_pen_flag = 1;
                                srs5001r_set_led_r(1);
                        }
			#endif
			#if defined(CONFIG_MATCH_SRS2801)
			if (led_pen_flag == 0)
                        {
                                led_pen_flag = 1;
                                srs5001r_set_led_g(1);
                        }
			#endif
			break;
		}
		case PEN_POINTER_UP:
		{
			input_report_abs(phic->p_inputdev, ABS_X, data.x);
			input_report_abs(phic->p_inputdev, ABS_Y, data.y);
			input_report_abs(phic->p_inputdev, ABS_PRESSURE, data.pressure);
			input_report_key(phic->p_inputdev, BTN_TOUCH, 0);
			//touchscreen_report_flag = true;
			if (isPenDetected == 0)
				input_report_key(phic->p_inputdev, BTN_TOOL_PEN, 1);
			isPenDetected = 1;
			last_status = 0;
			break;
		}
		case PEN_ALL_LEAVE:
		{
			input_report_abs(phic->p_inputdev, ABS_X, data.x);
			input_report_abs(phic->p_inputdev, ABS_Y, data.y);
			input_report_abs(phic->p_inputdev, ABS_PRESSURE, data.pressure);
			input_report_key(phic->p_inputdev, BTN_TOUCH, 0);
			input_report_key(phic->p_inputdev, BTN_TOOL_PEN, 0);
			input_report_key(phic->p_inputdev, BTN_STYLUS, 0);
			last_status = 0;
			isPenDetected = 0;
			#if defined(CONFIG_MATCH_SRS5001) || defined(CONFIG_MATCH_SHENGTENG) || defined(CONFIG_MATCH_AITI) || defined(CONFIG_MATCH_TOF)
			if (led_pen_flag == 1)
			{
				led_pen_flag = 0;
				srs5001r_set_led_r(0);
			}
			#endif
			#if defined(CONFIG_MATCH_SRS2801)
			if (led_pen_flag == 1)
                        {
                                led_pen_flag = 0;
                                srs5001r_set_led_g(0);
                        }
			#endif
			break;
		}
	}
	input_sync(phic->p_inputdev);
	
	return 0;
}

static void hanvon_i2c_wq(struct work_struct *work)
{
	struct hanvon_i2c_chip *phid = container_of(work, struct hanvon_i2c_chip, work_irq);
	struct i2c_client *client = phid->client;
	//int gpio = irq_to_gpio(client->irq);
	mutex_lock(&phid->mutex_wq);
	hanvon_report_event(phid);
	schedule();
	mutex_unlock(&phid->mutex_wq);
	enable_irq(client->irq);
}


static irqreturn_t hanvon_i2c_interrupt(int irq, void *dev_id)
{
	struct hanvon_i2c_chip *phid = (struct hanvon_i2c_chip *)dev_id;
	disable_irq_nosync(irq);
	if (!work_pending(&phid->work_irq)) 
		queue_work(phid->ktouch_wq, &phid->work_irq);

	return IRQ_HANDLED;
}

#if 0
static void resume_work_func(struct work_struct *work)
{
	int ret;
	if (g_phid->supply) {
		ret = regulator_enable(g_phid->supply);
		if (ret < 0)
			dev_err(&g_phid->client->dev, "failed to enable hanvon power supply\n");
		gpio_direction_input(g_phid->irq_pin);
		hw0868_reset();
	}
    enable_irq(g_phid->client->irq);
}
#endif
/*
static int __maybe_unused hanvon_i2c_suspend(struct tp_device *tp_d)
{
	struct hanvon_i2c_chip *hanvon_i2c = container_of(tp_d, struct hanvon_i2c_chip, tp);

	dev_dbg(&hanvon_i2c->client->dev, "%s\n", __func__);
	//cancel_delayed_work(&hanvon_resume_work);
	disable_irq(hanvon_i2c->client->irq);
	if (hanvon_i2c->supply) {
		gpio_direction_output(hanvon_i2c->irq_pin, 0);
		gpio_direction_output(hanvon_i2c->rst_pin, 0);
		if (regulator_is_enabled(hanvon_i2c->supply)) {
			regulator_disable(hanvon_i2c->supply);
		}
	}
	return 0;
}
*/
/*
static int __maybe_unused hanvon_i2c_resume(struct tp_device *tp_d)
{
	struct hanvon_i2c_chip *hanvon_i2c = container_of(tp_d, struct hanvon_i2c_chip, tp);
	#if 1
	int ret;

	dev_dbg(&hanvon_i2c->client->dev, "%s\n", __func__);
	if (hanvon_i2c->supply) {
		ret = regulator_enable(hanvon_i2c->supply);
		if (ret < 0)
			dev_err(&hanvon_i2c->client->dev, "failed to enable hanvon power supply\n");
		gpio_direction_input(hanvon_i2c->irq_pin);
		hw0868_reset();
	}
	enable_irq(hanvon_i2c->client->irq);
	#else
	queue_delayed_work(resume_workqueue, &hanvon_resume_work,
                msecs_to_jiffies(150));
	#endif

	return 0;
}
*/
static bool check_fw_ver(u8 *fw_data)
{
	if (fw_data[0xD070+4]>ver_info[4]) return true;
	if (fw_data[0xD070+5]>ver_info[5]) return true;
	if (fw_data[0xD070+6]>ver_info[6]) return true;
	if (fw_data[0xD070+1]>ver_info[1]) return true;
	return false;
}

static int __maybe_unused hanvon_i2c_really_update(struct hanvon_i2c_chip *hw_i2c,
						  const struct firmware *fw)
{
    int ret = 0;
    int fw_len = 0;
    unsigned char *fw_buf = (unsigned char *)(fw->data);
    HV_DEBUG("start firmware update\n");
    ret = fw->size;
    if (ret < 0xD070) {
        HV_DEBUG("invalid fw bin file, len:%d", ret);
        return -1;
    } else if (!check_fw_ver(fw_buf))
	{
		HV_DEBUG("fw ver in bin is old than chip,no need update");
		return -1;
	}
    fw_len = ret;
    
	ret = fw_update_fun(fw_buf,fw_len);

    return ret;
}

static void __maybe_unused hanvon_i2c_update(const struct firmware *fw, void *ctx)
{
	struct hanvon_i2c_chip *hw_i2c = ctx;
	int ret = 0;

	HV_DEBUG("%s enter.\n", __func__);
	mutex_lock(&hw_i2c->fw_lock);
	if (!hw_i2c->fw_done)
		goto err;
    mutex_unlock(&hw_i2c->fw_lock);

	ret = hanvon_i2c_really_update(hw_i2c, fw);

	mutex_lock(&hw_i2c->fw_lock);
	hw_i2c->fw_done = true;

err:
	release_firmware(fw);
	mutex_unlock(&hw_i2c->fw_lock);
	return;
}

static int hanvon_i2c_probe(struct i2c_client * client, const struct i2c_device_id * idp)
{
	int result = -1, i, ret;
	struct hanvon_i2c_chip *phid = NULL;
	struct device_node *np = client->dev.of_node;
	enum of_gpio_flags cfg_flags,rst_flags,en_flags;
	unsigned long irq_flags;
	struct regulator *power_supply;
	const struct firmware *fw = NULL;

	printk("Enter %s\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C functionality not supported\n");
		return -ENODEV;
	}
	if (!np) {
		dev_err(&client->dev, "no device tree\n");
		return -EINVAL;
	}
	
	g_client = client;
	g_phid = NULL;
	phid = kzalloc(sizeof(struct hanvon_i2c_chip), GFP_KERNEL);
	if(!phid) {
		printk(KERN_INFO "request memory failed.\n");
		return -ENOMEM;
	}
	of_property_read_u32(np, "revert_x", &revert_x_flag);
	of_property_read_u32(np, "revert_y", &revert_y_flag);
	of_property_read_u32(np, "xy_exchange", &exchange_x_y_flag);
	of_property_read_u32(np, "screen_max_x", &screen_max_x);
	of_property_read_u32(np, "screen_max_y", &screen_max_y);
	power_supply = devm_regulator_get(&client->dev, "pwr");
	if (power_supply) {
		dev_info(&client->dev, "hanvon power supply = %dmv\n", regulator_get_voltage(power_supply));
		if (!regulator_is_enabled(power_supply)) {
		result = regulator_enable(power_supply);
		if (result < 0)
			dev_err(&client->dev, "failed to enable hanvon power supply\n");
		}
		phid->supply = power_supply;
	}
	phid->irq_pin = of_get_named_gpio_flags(np, "gpio_intr",  0, (enum of_gpio_flags *)&irq_flags);
	phid->rst_pin = of_get_named_gpio_flags(np, "gpio_rst",  0, &rst_flags);
	phid->cfg_pin = of_get_named_gpio_flags(np, "gpio_cfg", 0, &cfg_flags);
	phid->en_pin = of_get_named_gpio_flags(np, "gpio_en", 0 , &en_flags);
	if (gpio_is_valid(phid->rst_pin)) {
		phid->rst_val = (rst_flags & OF_GPIO_ACTIVE_LOW) ? 0 : 1;
		result = devm_gpio_request_one(&client->dev, phid->rst_pin, (rst_flags & OF_GPIO_ACTIVE_LOW) ? GPIOF_OUT_INIT_LOW : GPIOF_OUT_INIT_HIGH, "hanvon pen reset pin");
		if (result != 0) {
			dev_err(&client->dev, "hanvon pen gpio_request error - rst_pin\n");
			kfree(phid);
			return -EIO;
		}
		gpio_direction_output(phid->rst_pin, 1);
	} else {
		dev_info(&client->dev, "reset pin invalid\n");
	}
	if (gpio_is_valid(phid->cfg_pin)) {
		result = devm_gpio_request_one(&client->dev, phid->cfg_pin, (cfg_flags & OF_GPIO_ACTIVE_LOW) ? GPIOF_OUT_INIT_LOW : GPIOF_OUT_INIT_HIGH, "hanvon pen config pin");
		if (result != 0) {
			dev_err(&client->dev, "hanvon pen gpio_request error - cfg_pin\n");
			kfree(phid);
			return -EIO;
		}
		gpio_direction_output(phid->cfg_pin, 0);
	} else {
		dev_info(&client->dev, "config pin invalid\n");
	}
	
	if (gpio_is_valid(phid->en_pin)) {
		result = devm_gpio_request_one(&client->dev, phid->en_pin, (en_flags & OF_GPIO_ACTIVE_LOW) ? GPIOF_OUT_INIT_LOW : GPIOF_OUT_INIT_HIGH, "hanvon pen en pin");
		if (result != 0) {
			dev_err(&client->dev, "hanvon pen gpio_request error - en_pin\n");
			kfree(phid);
			return -EIO;
		}
		gpio_direction_output(phid->en_pin, 1);
	} else {
		dev_info(&client->dev, "config pin invalid\n");
	}
	dev_info(&client->dev, "Riverwave result= %d\n", result);
	g_phid  = phid;
	phid->client = client;
	hw0868_reset();
	for (i = 0;i<3;i++)
	{
		result = hw0868_get_version(g_client,ver_info);
		dev_info(&client->dev, "Riverwave result11= %d\n", hw0868_get_version(g_client,ver_info));
		if (result>=0) 
		{
			break;
		} else if (result<0&&i==2) {
			unsigned char cbw[32]={0x48,0x57,0x30,0x38,0xaa,0x55,0x55,0xaa,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x4b,0x04,0x00,0x00,0x00,0x00,0x80,0xd0,0x00,0x00,0x45,0x44,0x00,0x00,0x00,0x00};
			hw0868_set_config_pin(1);
			msleep(100);
			hw0868_reset();
			hanvon_is_update = true;
			msleep(500);
			HV_DEBUG("erase flash ...");
			cbw[21] = 0xD07F&0x000000ff;
			cbw[22] = (0xD07F&0x0000ff00)>>8;
			cbw[23] = (0xD07F&0x00ff0000)>>16;
			cbw[24] = (0xD07F&0xff000000)>>24;
			ret = fw_i2c_master_send(g_client, cbw, sizeof(cbw));
			hanvon_is_update = false;
			hw0868_set_config_pin(0);
    		if (ret < 0) {
				HV_DEBUG("send cbw fail,mode=%2x",cbw[15]);
				if (regulator_is_enabled(power_supply)) {
				regulator_disable(power_supply);
				}
				devm_regulator_put(power_supply);
				kfree(phid);
				return result;
    		} else {
				memset(ver_info,0,sizeof(ver_info));
				break;
			}
		}
	}
	// setup input device.
	phid->p_inputdev = allocate_hanvon_input_dev();
	// setup work queue.
	phid->ktouch_wq = create_singlethread_workqueue("hanvon0868");
	mutex_init(&phid->mutex_wq);
	INIT_WORK(&phid->work_irq, hanvon_i2c_wq);
	i2c_set_clientdata(client, phid);
	// request irq.
	phid->irq=gpio_to_irq(phid->irq_pin);	
	if (phid->irq)
	{
		client->irq = phid->irq;
		result = devm_request_threaded_irq(&client->dev, phid->irq, NULL, hanvon_i2c_interrupt, irq_flags | IRQF_ONESHOT, client->name, phid);
		//result = request_irq(phid->irq, hanvon_i2c_interrupt, IRQF_DISABLED | IRQF_TRIGGER_FALLING /*IRQF_TRIGGER_FALLING IRQF_TRIGGER_LOW*/, client->name, phid);
		if (result != 0) {
			printk(KERN_ALERT "Cannot allocate ts INT!ERRNO:%d\n", result);
			goto fail2;
		}
	}
	dev_info(&client->dev, "Riverwave result enter");
	//resume_workqueue = create_singlethread_workqueue("hanvonresume");
	//INIT_DELAYED_WORK(&hanvon_resume_work, resume_work_func);
	device_init_wakeup(&client->dev, 1);
	enable_irq_wake(client->irq);
	//phid->tp.tp_resume = hanvon_i2c_resume;
	//phid->tp.tp_suspend = hanvon_i2c_suspend;
	//tp_register_fb(&phid->tp);
	//define a entry for update use.
	//register misc device
	result = misc_register(&hanvon_misc_dev);
	device_create_file(hanvon_misc_dev.this_device, &dev_attr_hw0868_entry);
	device_create_file(hanvon_misc_dev.this_device, &dev_attr_version);
	device_create_file(hanvon_misc_dev.this_device, &dev_attr_updatefirmware);

	printk(KERN_INFO "%s done.\n", __func__);
	printk(KERN_INFO "Name of device: %s.\n", client->dev.kobj.name);

	//emr_driver_load_flag = true;
	mutex_init(&phid->fw_lock);
	phid->fw_done = true;
	//result = request_firmware_nowait(THIS_MODULE, FW_ACTION_NOHOTPLUG,
	//		"hw0868_fw.bin", &client->dev, GFP_KERNEL, phid,
	//		hanvon_i2c_update);
	
	result = request_firmware(&fw,"hw0868_fw.bin", &client->dev);
	if (result < 0) {
		dev_err(&client->dev, "%s: Fail request firmware class file load\n",__func__);
	} else {
		hanvon_i2c_update(fw,(void*)phid);
	}

	{
	   extern void zyt_info_s2(char* s1,char* s2);
       extern void zyt_info_sx(char* c1,int x);

       zyt_info_s2("[EMR] : ","hw0868");
       zyt_info_sx("[hw0868] : FW Version:", ver_info[1]);
	}


	return 0;
fail2:
	i2c_set_clientdata(client, NULL);
	destroy_workqueue(phid->ktouch_wq);
	free_irq(client->irq, phid);
	input_unregister_device(phid->p_inputdev);
	phid->p_inputdev = NULL;

	kfree(phid);
	phid = NULL;
	return result;
}

static int hanvon_i2c_remove(struct i2c_client * client)
{
	return 0;
}

static const struct i2c_device_id hanvon_i2c_idtable[] = {
	{HANVON_I2C_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, hanvon_i2c_idtable);

static struct of_device_id hanvon_dt_ids[] = {
	{ .compatible = HANVON_I2C_NAME },
	{},
};
#if 1 //def CONFIG_PM
static int hanvon_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	printk("hanvon_i2c_suspend\n");
	//first disable irq
	disable_irq(client->irq);
	//if (g_phid->supply) {
		gpio_direction_output(g_phid->irq_pin, 0);
		gpio_direction_output(g_phid->rst_pin, 0);
		gpio_direction_output(g_phid->en_pin, 0);
		//regulator_disable(g_phid->supply);
	//}
	return 0;
}

static int hanvon_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	//int ret;
	printk("hanvon_i2c_resume\n");
	//first rest
	//if (g_phid->supply) {
		//ret = regulator_enable(g_phid->supply);
		gpio_direction_output(g_phid->en_pin, 1);
		gpio_direction_input(g_phid->irq_pin);
		hw0868_reset();
	//}
	//second enable irq
	enable_irq(client->irq);
	return 0;
}
static const struct dev_pm_ops hanvon_i2c_pm_ops = {
	.suspend = hanvon_i2c_suspend,
	.resume = hanvon_i2c_resume,
};
#endif


static struct i2c_driver hanvon_i2c_driver = {
	.driver = {
		.name	= HANVON_I2C_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(hanvon_dt_ids),
#if 1 //def CONFIG_PM
		.pm     = &hanvon_i2c_pm_ops,
#endif
	},
	.probe		= hanvon_i2c_probe,
	.remove		= hanvon_i2c_remove,
	.id_table	= hanvon_i2c_idtable,
};

static int hw0868_i2c_init(void)
{
	/*
	if (emr_driver_load_flag) {
		return 0;
	}
	*/
	printk(KERN_INFO "hw0868 chip initializing ....\n");
	return i2c_add_driver(&hanvon_i2c_driver);
}

static void hw0868_i2c_exit(void)
{
	printk(KERN_INFO "hw0868 driver exit.\n");
	i2c_del_driver(&hanvon_i2c_driver);
}

late_initcall(hw0868_i2c_init);
module_exit(hw0868_i2c_exit);

module_param(debug, uint, S_IRUGO | S_IWUSR);

MODULE_AUTHOR("Zhang Nian");
MODULE_DESCRIPTION("Hanvon Electromagnetic Pen");
MODULE_LICENSE("GPL");
