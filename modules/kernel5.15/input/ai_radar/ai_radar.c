#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/string.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include "ai_radar.h"
#ifdef CONFIG_AI_BSP_MTK_DEVICE_CHECK
#include  <linux/ai_device_check.h>
#endif
#include <linux/pm_wakeup.h>

#include <soc/sprd/board.h>

#define k60168_I2C_NAME "k60168_radar"


#define FW_BIN_UPGRADE_NAME "k60168_fw.bin"
int idle_flag = 0;
int sleep_mode = 0;
uint32_t k60168_fw_version = 0;
uint32_t k60168_module_id = 4;
static bool k60168_enable_flag = false;
//static struct task_struct *thread = NULL;

static int32_t read_gesture_id(struct k60168 *k60168, uint8_t *reg_val);
static int32_t generation_gesture_data(struct k60168 *k60168, uint32_t *reg_data32);
uint32_t get_idle_status(struct k60168 *k60168);
void k60168_erase_64k_flash(struct k60168 *k60168, uint8_t addr);
void k60168_erase_32k_flash(struct k60168 *k60168);
int k60168_irq_handle(struct k60168 *k60168);

struct k60168 *g_k60168_data=NULL;
bool update_enable = false;

extern void zyt_info_sx(char* c1,int x);

struct upgrade_func
{
    u32 fwveroff;
    bool (*upgrade)(u8 *, u32);
};

struct k60168_upgrade
{
    //struct k60168_ts_data *ts_data;
    struct upgrade_func *func;
//    struct upgrade_setting_nf *setting_nf;
//    int module_id;
    u8 *fw;
    u32 fw_length;
};

static struct k60168_upgrade *fwupgrade=NULL;

#ifdef CONFIG_AI_BSP_MTK_DEVICE_CHECK
int k60168_register_hardware_info(void)
{
    struct ai_device_info ai_radar_hw_info;
    ai_radar_hw_info.ai_dev_type = AI_DEVICE_TYPE_RADAR;
    snprintf(ai_radar_hw_info.name, AI_DEVICE_NAME_LEN, "radar_k60168_[%d]",k60168_fw_version);
    ai_set_device_info(ai_radar_hw_info);
    return 0;
}
#endif

void erase_all_flash(struct k60168 *k60168)
{
	uint32_t idle_status = 0;
	int32_t i = 0;

	while(i < 5){
	i++;
	idle_status = get_idle_status(k60168);
	if(idle_status == 0x01){
		k60168_erase_32k_flash(k60168);
		i = 0;
		break;
		}
	}

	while(i < 5){
	i++;
	idle_status = get_idle_status(k60168);
	if(idle_status == 0x01){
		k60168_erase_64k_flash(k60168, 0x01);
		i = 0;
		break;
		}
	}

	while(i < 5){
	i++;
	idle_status = get_idle_status(k60168);
	if(idle_status == 0x01){
		k60168_erase_64k_flash(k60168, 0x02);
		i = 0;
		break;
		}
	}

	while(i < 5){
	i++;
	idle_status = get_idle_status(k60168);
	if(idle_status == 0x01){
		k60168_erase_64k_flash(k60168, 0x03);
		i = 0;
		break;
		}
	}

	while(i < 5){
	i++;
	idle_status = get_idle_status(k60168);
	if(idle_status == 0x01){
		k60168_erase_64k_flash(k60168, 0x04);
		i = 0;
		break;
		}
	}

	while(i < 5){
	i++;
	idle_status = get_idle_status(k60168);
	if(idle_status == 0x01){
		k60168_erase_64k_flash(k60168, 0x05);
		i = 0;
		break;
		}
	}

	while(i < 5){
	i++;
	idle_status = get_idle_status(k60168);
	if(idle_status == 0x01){
		k60168_erase_64k_flash(k60168, 0x06);
		i = 0;
		break;
		}
	}

}

uint8_t checksum(uint8_t data[], uint32_t size)
{
    uint8_t  sum;
    uint32_t i;

    sum = 0;
    for(i=0; i<size; i++)
    {
        sum += data[i];
    }

    return (uint8_t) (0x100 - sum);
}

void k60168_erase_64k_flash(struct k60168 *k60168, uint8_t addr)
{
    uint8_t  message[20];
	int32_t ret =  -ENOMEM;
	struct i2c_client *i2c = k60168->i2c;
	struct i2c_msg msg[1];

	pr_info(" k60168_erase_64k_flash enter %d\n",addr);

    message[0] = 0xFF;
    message[1] = 0x50;
    message[2] = 0x00;
    message[3] = 0x00;
    message[4]  = 0x24;
    message[5]  = 0x4B;
    message[6]  = 0x3E;
    message[7]  = 0x54;
    message[8]  = 0x00;
    message[9]  = 0x00;
    message[10] = 0x08;
    message[11] = 0x00;    //address
    message[12]  = addr;
    message[13]  = 0x00;
    message[14]  = 0x00;
    message[15] = 0x00;    //erase size
    message[16] = 0x01;
    message[17]  = 0x00;
    message[18]  = 0x00;
    message[19] = checksum(&message[7], 12);

	msg[0].addr = i2c->addr;
	msg[0].flags = K60168_I2C_WR;
	msg[0].len = 20;
	msg[0].buf = (unsigned char *)message;

	ret = i2c_transfer(i2c->adapter, msg, 1);
	if (ret < 0){
		pr_info("%s: erase error %d\n", __func__, ret);
		//return -1;
	}
	//return 0;
}

void k60168_erase_32k_flash(struct k60168 *k60168)
{
    uint8_t  message[20];
	int32_t ret =  -ENOMEM;
	struct i2c_client *i2c = k60168->i2c;
	struct i2c_msg msg[1];

	pr_info(" k60168_erase_32k_flash enter\n");

    message[0] = 0xFF;
    message[1] = 0x50;
    message[2] = 0x00;
    message[3] = 0x00;
    message[4]  = 0x24;
    message[5]  = 0x4B;
    message[6]  = 0x3E;
    message[7]  = 0x54;
    message[8]  = 0x00;
    message[9]  = 0x00;
    message[10] = 0x08;
    message[11] = 0x00;
    message[12]  = 0x00;
    message[13]  = 0x80;
    message[14]  = 0x00;
    message[15] = 0x00;
    message[16] = 0x00;
    message[17]  = 0x80;
    message[18]  = 0x00;
    message[19]  = 0xA4;

	msg[0].addr = i2c->addr;
	msg[0].flags = K60168_I2C_WR;
	msg[0].len = 20;
	msg[0].buf = (unsigned char *)message;

	ret = i2c_transfer(i2c->adapter, msg, 1);
	if (ret < 0)
		pr_info("%s: erase error %d\n", __func__, ret);

}


uint32_t get_idle_status(struct k60168 *k60168)
{
    uint8_t  message[12];
	int32_t ret =  -ENOMEM;
	struct i2c_client *i2c = k60168->i2c;
	struct i2c_msg msg[1];

	uint32_t idle_status = 0;
	uint8_t gesture_id[12];

    message[0] = 0xFF;
    message[1] = 0x50;
    message[2] = 0x00;
    message[3] = 0x00;
    message[4]  = 0x24;
    message[5]  = 0x4B;
    message[6]  = 0x3E;
    message[7]  = 0x53;
    message[8]  = 0x00;
    message[9]  = 0x00;
    message[10] = 0x00;
    message[11] = 0xAD;

	msg[0].addr = i2c->addr;
	msg[0].flags = K60168_I2C_WR;
	msg[0].len = 12;
	msg[0].buf = (unsigned char *)message;

	ret = i2c_transfer(i2c->adapter, msg, 1);
	if (ret < 0)
		pr_info("%s: write error %d\n", __func__, ret);

	msleep(150);

	read_gesture_id(k60168, gesture_id);    //idle status is 53
	if(gesture_id[3] == 0x53){
		pr_err("k60168 idle_flag enter");
		idle_status = 1;
	}else{
		idle_status = 0;
	}

	pr_info("%s:idle_status =  %d\n", __func__, idle_status);
	return idle_status;
}

void upgrade_firware_reset(struct k60168 *k60168)
{
    uint8_t  message[12];
	int32_t ret =  -ENOMEM;
	struct i2c_client *i2c = k60168->i2c;
	struct i2c_msg msg[1];

	pr_info(" k60168 upgrade_firware_reset enter\n");

    message[0] = 0xFF;
    message[1] = 0x50;  // cdc write
    message[2] = 0x00;
    message[3] = 0x00;
    message[4]  = 0x24;    // Start-of-packet, '$'
    message[5]  = 0x4B;    // Start-of-packet, 'K'
    message[6]  = 0x3E;    // Direction, '>'
    message[7]  = 0x81; // Command
    message[8]  = 0x00;    // Channel
    message[9]  = 0x00; // Payload Length[15:08]
    message[10] = 0x00;      // Payload Length[07:00]
    message[11] = 0xF8;    // Payload

	msg[0].addr = i2c->addr;
	msg[0].flags = K60168_I2C_WR;
	msg[0].len = 12;
	msg[0].buf = (unsigned char *)message;

	ret = i2c_transfer(i2c->adapter, msg, 1);
	if (ret < 0)
		pr_info("%s: write error %d\n", __func__, ret);

}

int get_chip_id(struct k60168 *k60168)
{
    uint8_t  message[12];
	int32_t ret =  -ENOMEM;
	struct i2c_client *i2c = k60168->i2c;
	struct i2c_msg msg[1];
	struct i2c_msg msg1[2];
	uint8_t w_buf[4];
	uint8_t buf[20];

    message[0] = 0xFF;
    message[1] = 0x50;  // cdc write
    message[2] = 0x00;
    message[3] = 0x00;
    message[4]  = 0x24;    // Start-of-packet, '$'
    message[5]  = 0x4B;    // Start-of-packet, 'K'
    message[6]  = 0x3E;    // Direction, '>'
    message[7]  = 0x08; // Command
    message[8]  = 0x00;    // Channel
    message[9]  = 0x00; // Payload Length[15:08]
    message[10] = 0x00;      // Payload Length[07:00]
    message[11] = 0xF8;    // Payload

	msg[0].addr = i2c->addr;
	msg[0].flags = K60168_I2C_WR;
	msg[0].len = 12;
	msg[0].buf = (unsigned char *)message;
	pr_info(" k60168 get_chip_id enter\n");
	ret = i2c_transfer(i2c->adapter, msg, 1);
	if (ret < 0)
		pr_info("%s: write error %d\n", __func__, ret);

	msleep(10);

		w_buf[0] = 0xFF;
		w_buf[1] = 0X51;
		w_buf[2] = 0x00;
		w_buf[3] = 0x00;

		msg1[0].addr = i2c->addr;
		msg1[0].flags = K60168_I2C_WR;
		msg1[0].len = 4;
		msg1[0].buf = (unsigned char *)w_buf;

		msg1[1].addr = i2c->addr;
		msg1[1].flags = K60168_I2C_RD;
		msg1[1].len = 20;
		msg1[1].buf = (unsigned char *)buf;

		ret = i2c_transfer(i2c->adapter, msg1, 2);
		if (ret < 0)
			pr_info("%s: read id error %d\n", __func__, ret);

		pr_err(" k60168 get_chip_id  buf[7]=0x%x,buf[8]=0x%x,buf[9]=0x%x,buf[10]=0x%x,buf[11]=0x%x,buf[12]=0x%x,buf[13]=0x%x\n",buf[7], buf[8], buf[9],buf[10],buf[11],buf[12],buf[13]);

		if( (buf[7] == 0x4B) && (buf[8] == 0x36) && (buf[9] == 0x30) && (buf[10] == 0x31) && (buf[11] == 0x36) && (buf[12] == 0x38))
			return 1;
		else
			return 0;

}

void upgrade_firware_power_off_on(struct k60168 *k60168)
{

	gpio_direction_output(k60168->rst_gpio, 0);

	gpio_direction_output(k60168->pwr_gpio, 0);
	gpio_direction_output(k60168->enable_gpio, 0);


	gpio_direction_output(k60168->rst_gpio, 1);
	gpio_direction_output(k60168->pwr_gpio, 1);
	gpio_direction_output(k60168->enable_gpio, 1);
	msleep(300);

}


void get_firmware_version(struct k60168 *k60168)
{
    uint8_t  message[16];
	int32_t ret =  -ENOMEM;
	struct i2c_client *i2c = k60168->i2c;
	struct i2c_msg msg[1];
	struct i2c_msg msg1[2];
	uint8_t w_buf[4];
	uint8_t buf[14];
	//uint32_t reg_val = 0;
	uint32_t firware_version = 0;

	pr_info(" k60168 get_firmware_version enter\n");

    message[0] = 0xFF;
    message[1] = 0x50;  // cdc write
    message[2] = 0x00;
    message[3] = 0x00;
    message[4]  = 0x24;    // Start-of-packet, '$'
    message[5]  = 0x4B;    // Start-of-packet, 'K'
    message[6]  = 0x3E;    // Direction, '>'
    message[7]  = 0x01; // Command
    message[8]  = 0x00;    // Channel
    message[9]  = 0x00; // Payload Length[15:08]
    message[10] = 0x01;      // Payload Length[07:00]
    message[11] = 0x00;    // Payload
    message[12] = 0xFE;
	message[13] = 0xFF;
	message[14] = 0xFF;
	message[15] = 0xFF;

	msg[0].addr = i2c->addr;
	msg[0].flags = K60168_I2C_WR;
	msg[0].len = 16;
	msg[0].buf = (unsigned char *)message;

	ret = i2c_transfer(i2c->adapter, msg, 1);
	if (ret < 0)
		pr_info("%s: read error %d\n", __func__, ret);

	msleep(10);

	w_buf[0] = 0xFF;
	w_buf[1] = 0X51;
	w_buf[2] = 0x00;
	w_buf[3] = 0x00;

	msg1[0].addr = i2c->addr;
	msg1[0].flags = K60168_I2C_WR;
	msg1[0].len = 4;
	msg1[0].buf = (unsigned char *)w_buf;

	msg1[1].addr = i2c->addr;
	msg1[1].flags = K60168_I2C_RD;
	msg1[1].len = 14;
	msg1[1].buf = (unsigned char *)buf;

	ret = i2c_transfer(i2c->adapter, msg1, 2);
	if (ret < 0)
		pr_info("%s: read id error %d\n", __func__, ret);

	//pr_err(" k60168 read firmware_version  0x%x		0x%x     0x%x  \n",buf[7], buf[8], buf[9]);
	firware_version = ((u32)buf[7]) | ((u32)buf[8]<<8) |((u32)buf[9]<<16) ;
	k60168_fw_version = firware_version;
	pr_info(" k60168 firmware_version 0x%x\n",firware_version);


}

static int32_t
generation_gesture_data(struct k60168 *k60168, uint32_t *reg_data32)
{
	int32_t ret =  -ENOMEM;
	struct i2c_client *i2c = k60168->i2c;
	struct i2c_msg msg[2];
	uint8_t w_buf[4];
	uint8_t buf[4];

	w_buf[0] = 0xFF;    //(u8)(reg_addr32 >> 24)
	w_buf[1] = 0X52;    //(u8)(reg_addr32 >> 16)
	w_buf[2] = 0x00;    //(u8)(reg_addr32 >> 8)
	w_buf[3] = 0x00;    //(u8)(reg_addr32)
	msg[0].addr = i2c->addr;
	msg[0].flags = K60168_I2C_WR;
	msg[0].len = 4;
	msg[0].buf = (unsigned char *)w_buf;

	msg[1].addr = i2c->addr;
	msg[1].flags = K60168_I2C_RD;
	msg[1].len = 4;
	msg[1].buf = (unsigned char *)buf;

	ret = i2c_transfer(i2c->adapter, msg, 2);
	if (ret < 0)
		pr_info("%s: i2c read  error %d\n", __func__,ret);

	//pr_info("%s: read buf 0 %0x	1 %0x  2 %0x	3 %0x\n", __func__, buf[0], buf[1], buf[2], buf[3]);

	reg_data32[0] = ((u32)buf[3]) | ((u32)buf[2]<<8) |
			((u32)buf[1]<<16) | ((u32)buf[0]<<24);

	//pr_info("%s: read reg_data32[0] %0x \n", __func__, reg_data32[0]);

	return ret;
}

static int32_t
read_gesture_id(struct k60168 *k60168, uint8_t *reg_val)
{
	int32_t ret =  -ENOMEM;
	struct i2c_client *i2c = k60168->i2c;
	struct i2c_msg msg[2];
	uint8_t w_buf[4];
	uint8_t buf[12];

	w_buf[0] = 0xFF;
	w_buf[1] = 0X51;
	w_buf[2] = 0x00;
	w_buf[3] = 0x00;
	msg[0].addr = i2c->addr;
	msg[0].flags = K60168_I2C_WR;
	msg[0].len = 4;
	msg[0].buf = (unsigned char *)w_buf;

	msg[1].addr = i2c->addr;
	msg[1].flags = K60168_I2C_RD;
	msg[1].len = 12;
	msg[1].buf = (unsigned char *)buf;

	ret = i2c_transfer(i2c->adapter, msg, 2);
	if (ret < 0)
		pr_info("%s: read_gesture_id error %d\n", __func__, ret);

	reg_val[3] = buf[3];
	reg_val[8] = buf[8];
	reg_val[9] = buf[9];

	pr_err(" read  reg_val[3] 0x%x	\n",reg_val[3]);

	pr_err(" read 0x%x	0x%x\n",reg_val[8], reg_val[9]);

	return ret;
}


int generation_int_data(struct k60168 *k60168)
{
	int32_t ret =  -ENOMEM;
	struct i2c_client *i2c = k60168->i2c;
	struct i2c_msg msg[2];
	uint8_t w_buf[4];
	uint8_t buf[8];

	w_buf[0] = 0xFF;    //(u8)(reg_addr32 >> 24)
	w_buf[1] = 0X52;    //(u8)(reg_addr32 >> 16)
	w_buf[2] = 0x00;    //(u8)(reg_addr32 >> 8)
	w_buf[3] = 0x00;    //(u8)(reg_addr32)
	msg[0].addr = i2c->addr;
	msg[0].flags = K60168_I2C_WR;
	msg[0].len = 4;
	msg[0].buf = (unsigned char *)w_buf;

	msg[1].addr = i2c->addr;
	msg[1].flags = K60168_I2C_RD;
	msg[1].len = 8;
	msg[1].buf = (unsigned char *)buf;

	ret = i2c_transfer(i2c->adapter, msg, 2);
	if (ret < 0)
		pr_info("%s: i2c read  error %d\n", __func__,ret);

	pr_info("%s: read buf 8 %0x	\n", __func__, buf[7]);
	if(buf[7] == 0x1)
		return 1;
	else
		return 0;
}

void k60168_close_int(struct k60168 *k60168)
{
    uint8_t  message[16];
	int32_t ret =  -ENOMEM;
	struct i2c_client *i2c = k60168->i2c;
	struct i2c_msg msg[1];

	pr_info(" k60168_close_int enter\n");

    message[0] = 0xFF;
    message[1] = 0x50;
    message[2] = 0x00;
    message[3] = 0x00;
    message[4]  = 0x24;
    message[5]  = 0x4B;
    message[6]  = 0x3E;
    message[7]  = 0x70;
    message[8]  = 0x00;
    message[9]  = 0x00;
    message[10] = 0x01;
    message[11] = 0x01;//cmd
    message[12] = 0x8E;
	message[13] = 0xFF;
	message[14] = 0xFF;
	message[15] = 0xFF;

	msg[0].addr = i2c->addr;
	msg[0].flags = K60168_I2C_WR;
	msg[0].len = 16;
	msg[0].buf = (unsigned char *)message;

	ret = i2c_transfer(i2c->adapter, msg, 1);
	if (ret < 0)
		pr_info("%s: write error %d\n", __func__, ret);

}

void k60168_sleep(struct k60168 *k60168)
{
    uint8_t  message[16];
	int32_t ret =  -ENOMEM;
	struct i2c_client *i2c = k60168->i2c;
	struct i2c_msg msg[1];

	pr_info(" k60168_sleep enter\n");

    message[0] = 0xFF;
    message[1] = 0x50;
    message[2] = 0x00;
    message[3] = 0x00;
    message[4]  = 0x24;
    message[5]  = 0x4B;
    message[6]  = 0x3E;
    message[7]  = 0x73;
    message[8]  = 0x00;
    message[9]  = 0x00;
    message[10] = 0x01;
    message[11] = 0x01;
    message[12] = 0x8B;
	message[13] = 0xFF;
	message[14] = 0xFF;
	message[15] = 0xFF;

	msg[0].addr = i2c->addr;
	msg[0].flags = K60168_I2C_WR;
	msg[0].len = 16;
	msg[0].buf = (unsigned char *)message;

	ret = i2c_transfer(i2c->adapter, msg, 1);
	if (ret < 0)
		pr_info("%s: write error %d\n", __func__, ret);

}

void k60168_wakeup(struct k60168 *k60168)
{
    uint8_t  message[16];
	int32_t ret =  -ENOMEM;
	struct i2c_client *i2c = k60168->i2c;
	struct i2c_msg msg[1];

	pr_info(" k60168_wakeup enter\n");

    message[0] = 0xFF;
    message[1] = 0x50;
    message[2] = 0x00;
    message[3] = 0x00;
    message[4]  = 0x24;
    message[5]  = 0x4B;
    message[6]  = 0x3E;
    message[7]  = 0x73;
    message[8]  = 0x00;
    message[9]  = 0x00;
    message[10] = 0x01;
    message[11] = 0x00;
    message[12] = 0x8C;
	message[13] = 0xFF;
	message[14] = 0xFF;
	message[15] = 0xFF;

	msg[0].addr = i2c->addr;
	msg[0].flags = K60168_I2C_WR;
	msg[0].len = 16;
	msg[0].buf = (unsigned char *)message;

	ret = i2c_transfer(i2c->adapter, msg, 1);
	if (ret < 0)
		pr_info("%s: write error %d\n", __func__, ret);

}

static ssize_t k60168_enable(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	ssize_t ret;
	uint32_t state;
	int irq_level = 0;

	struct k60168 *k60168 = dev_get_drvdata(dev);
	ret = kstrtouint(buf, 10, &state);
	sleep_mode = 0;

	

	if (ret)
	{
		pr_err("k60168 enable fail");
		return ret;
	}
	irq_level = gpio_get_value(k60168->irq_gpio);
	if( irq_level == 1){

		disable_irq(k60168->to_irq);
				
		ret = gpio_direction_output(k60168->irq_gpio, 0);
		
		msleep(80);
				
		ret = gpio_direction_output(k60168->irq_gpio, 1);
		msleep(1);
		ret = gpio_direction_input(k60168->irq_gpio);
		enable_irq(k60168->to_irq);
	}
	if (state == 1){
		gpio_direction_output(k60168->enable_gpio, 1);
		msleep(20);
		gpio_direction_output(k60168->pwr_gpio, 1);
	
		msleep(100);
		pr_err("k60168 enable 1");
		k60168_wakeup(k60168);
		msleep(10);
		k60168_wakeup(k60168);
		k60168_enable_flag = true;
	}
	else if (state == 0){
		pr_err("k60168 enable 0");
		k60168_sleep(k60168);
		msleep(10);
		if(sleep_mode){
			k60168_sleep(k60168);
			sleep_mode = 0;
		}
		k60168_enable_flag = false;
		msleep(20);
		gpio_direction_output(k60168->pwr_gpio, 0);
		gpio_direction_output(k60168->enable_gpio, 0);

	}
	else
		pr_err("k60168 enable fail");
	return count;
}

static ssize_t k60168_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	return sprintf(buf, "%s\n",k60168_enable_flag ? "1" : "0");
}

static ssize_t k60168_chipid_get(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	ssize_t len = 0;
	int rc = 2;
	struct k60168 *k60168 = dev_get_drvdata(dev);

	rc = get_chip_id(k60168);

	len += snprintf(buf+len, PAGE_SIZE-len, "%d\n", rc);

	return len;
}

static ssize_t k60168_update_enable(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	ssize_t len = 0;
	len += snprintf(buf+len, PAGE_SIZE-len, "%d\n", update_enable);

	return len;
}

static DEVICE_ATTR(enable, 0644, k60168_show, k60168_enable);
static DEVICE_ATTR(chipid, 0644, k60168_chipid_get, NULL);
static DEVICE_ATTR(upgrade, 0644, k60168_update_enable, NULL);


static struct attribute *k60168_radar_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_chipid.attr,
	&dev_attr_upgrade.attr,
	NULL
};

static struct attribute_group k60168_radar_attribute_group = {
	.attrs = k60168_radar_attributes
};

#define FW168_UP_SIZE (1024)
#define FW168_UP_PAYLOAD (FW168_UP_SIZE + 8)
void k60168_write_firmware(struct k60168 *k60168, unsigned char *data, uint32_t addr, int count)
{
    uint8_t  message[1044];
	int32_t ret =  -ENOMEM;
	struct i2c_client *i2c = k60168->i2c;
	struct i2c_msg msg[1];
	int i = 19;
	int n = 0;

	addr = addr + 0x0400 * count;
	pr_info(" k60168_write_firmware enter count %d\n",count);

	//pr_info("%s chen 1  %0x\n", __func__,data[32767]);//32K

    message[0] = 0xFF;
    message[1] = 0x50;  // cdc write
    message[2] = 0x00;
    message[3] = 0x00;
    message[4]  = 0x24;    // Start-of-packet, '$'
    message[5]  = 0x4B;    // Start-of-packet, 'K'
    message[6]  = 0x3E;    // Direction, '>'
    message[7]  = 0x50; // Command
    message[8]  = 0x00;    // Channel
    message[9]  = (uint8_t)(FW168_UP_PAYLOAD>>8 & 0xFF); // Payload Length[15:08]
    message[10] = (uint8_t)(FW168_UP_PAYLOAD & 0xFF);      // Payload Length[07:00]
    message[11] = (uint8_t)(addr>>24 & 0xFF);
    message[12] = (uint8_t)(addr>>16 & 0xFF);
    message[13] = (uint8_t)(addr>>8 & 0xFF);
    message[14] = (uint8_t)(addr & 0xFF);
	message[15] = 0x00;
	message[16] = 0x00;
	message[17] = (uint8_t)(FW168_UP_SIZE>>8 & 0xFF);
	message[18] = (uint8_t)(FW168_UP_SIZE & 0xFF);

	for(i = 19; i <= 1039 ; i=i+4){
		message[i] = data[FLASH_32KB + 3 + n + 0x0400 * count];
		message[i+1] = data[FLASH_32KB + 2 + n + 0x0400 * count];
		message[i+2] = data[FLASH_32KB + 1 + n + 0x0400 * count];
		message[i+3] = data[FLASH_32KB + n + 0x0400 * count];
		n = n + 4;
	}

	message[FW168_UP_PAYLOAD+11] = checksum(&message[7], FW168_UP_PAYLOAD+4);

	msg[0].addr = i2c->addr;
	msg[0].flags = K60168_I2C_WR;
	msg[0].len = FW168_UP_PAYLOAD+12;
	msg[0].buf = (unsigned char *)message;

	ret = i2c_transfer(i2c->adapter, msg, 1);
	if (ret < 0)
		pr_info("%s: write error %d  count %d\n", __func__, ret,count);

}

static void k60168_firmware_write(struct k60168_upgrade *upg)
{

	int count = 0;
	int temp = 0;
	int i = 0;
	uint32_t idle_status = 0;

	pr_info("%s enter\n", __func__);





	temp = upg->fw_length / 1024;
	for(count = 0; count < 356; count++){
		while(i < 10){
			i++;
			idle_status = get_idle_status(g_k60168_data);
			msleep(10);
			pr_err("k60168 enter  idle_status = %d count = %d \n",idle_status,count);
			if(idle_status == 1){
				//pr_err("k60168 chen enter\n");
				k60168_write_firmware(g_k60168_data,upg->fw,0x00008000,count);
				i = 0;
				idle_status = 0;
				msleep(10);
				break;
			}
		}
		i = 0;
		idle_status = 0;
	}

	msleep(150);

	while(i < 10){
		i++;
		idle_status = get_idle_status(g_k60168_data);
		msleep(10);
		if(idle_status == 1){
			pr_err("k60168 reset enter\n");
			upgrade_firware_reset(g_k60168_data);
			idle_status = 0;
			break;
		}
	}
	msleep(300);

	while(i < 10){
		i++;
		idle_status = get_idle_status(g_k60168_data);

		if(idle_status == 1){
			pr_err("k60168 reset enter\n");
			get_chip_id(g_k60168_data);
			idle_status = 0;
			break;
		}
	}


	upgrade_firware_power_off_on(g_k60168_data);

	
	msleep(150);
	while(i < 10){
		i++;
		idle_status = get_idle_status(g_k60168_data);

		if(idle_status == 1){
			pr_err("k60168 reset enter\n");

			get_firmware_version(g_k60168_data);
			idle_status = 0;
			break;
		}
	}
	
	pr_info(" k60168 upgrade after get k60168_fw_version %d\n",k60168_fw_version);

#ifdef CONFIG_AI_BSP_MTK_DEVICE_CHECK
	k60168_register_hardware_info();
#endif
	update_enable = false;

	if(!k60168_enable_flag){
		msleep(150);
		while(i < 10){
			i++;
			idle_status = get_idle_status(g_k60168_data);

			if(idle_status == 1){
				pr_err("k60168 reset enter\n");

				k60168_sleep(g_k60168_data);
				idle_status = 0;
				break;
			}
		}
	}
	zyt_info_sx("[Radar-k60168] : FW mid:", k60168_module_id);
    zyt_info_sx("[Radar-k60168] : FW Version:", k60168_fw_version);
	pr_info("%s bin writen completely: \n", __func__);
}


static bool fwupg_get_fw_file(struct k60168_upgrade *upg)
{

	int ret = 0;
	const struct firmware *fw = NULL;


    pr_info("get upgrade fw file");
    if (!upg)
    {
        pr_err("upg is null");
        return false;
    }


	upg->fw = NULL;
	


	if (request_firmware(&fw, FW_BIN_UPGRADE_NAME, g_k60168_data->dev)) 
    {
		pr_err("firmware read failed");
        return false;
	}

	if (fw) {
		pr_info("firmware size = %d",fw->size);
        upg->fw = vmalloc(fw->size);
        if (NULL == upg->fw) {
            pr_err("fw buffer vmalloc fail");
            ret = -ENOMEM;
        } else {
            memcpy(upg->fw, fw->data, fw->size);
            ret = fw->size;
        }
        upg->fw_length = fw->size;
		release_firmware(fw);
	}
	
	if (ret < 0)
	{
		pr_err("read fw bin file fail, len:%d", ret);
		return false;
	}


    pr_info("upgrade fw file len:%d", upg->fw_length);

    return true;
}

static int32_t k60168_cfg_update(struct k60168 *k60168)
{

	struct k60168_upgrade *upg = fwupgrade;

    pr_info("k60168_fwupg_work begin");
    if (!upg || !g_k60168_data)
    {
        pr_info("upg/g_k60168_data is null");
        return false;
    }
    
	if(update_enable)
    {
        /* run upgrade firmware*/
		erase_all_flash(g_k60168_data);
		msleep(200);
        k60168_firmware_write(upg);
    }
	
	return RADAR_SUCCESS;
}

static void k60168_cfg_work_routine(struct work_struct *work)
{
	struct k60168
		*k60168 = container_of(work, struct k60168, cfg_work.work);

	pr_info("%s: enter\n", __func__);

	k60168_cfg_update(k60168);
}

static int32_t k60168_cfg_init(struct k60168 *k60168)
{
	pr_info("%s: enter\n", __func__);


	if (!g_k60168_data || !g_k60168_data->ts_workqueue)
    {
        pr_info("g_k60168_data/workqueue is NULL, can't run upgrade function");
        return false;
    }

    //fwupgrade->ts_data = ts_data;
    INIT_WORK(&g_k60168_data->fwupg_work, k60168_cfg_work_routine);
    queue_work(g_k60168_data->ts_workqueue, &g_k60168_data->fwupg_work);
	
	return RADAR_SUCCESS;
}

static int32_t k60168_input_sys_init(struct k60168 *k60168)
{
	int32_t ret = 0;
	pr_info("%s: enter\n", __func__);
	k60168->input = input_allocate_device();
	if (!(k60168->input))
		return -INPUT_ALLOCATE_FILED;

	k60168->input->name = k60168_I2C_NAME;
	input_set_capability(k60168->input, EV_KEY, 234 );
	input_set_capability(k60168->input, EV_KEY, 235 );
	input_set_capability(k60168->input, EV_KEY, 236 );
	input_set_capability(k60168->input, EV_KEY, 233 );

	ret = input_register_device(k60168->input);
	if (ret) {
		pr_err("%s: failed to register input device: %s\n", __func__,
						dev_name(&k60168->i2c->dev));
		input_free_device(k60168->input);
		return -INPUT_REGISTER_FAILED;
	}

	return RADAR_SUCCESS;
}

int k60168_irq_handle(struct k60168 *k60168)
{
	uint32_t reg_val = 0;
	uint8_t gesture_id[12];

	if(update_enable)
		return 0;
	usleep_range(5000, 6000);
	generation_gesture_data(k60168, &reg_val);
	pr_info("k60168_irq_handle = 0x%x\n", reg_val);

	if(reg_val == 0x4a4b000c){
		read_gesture_id(k60168, gesture_id);    //idle status is 53
		if(gesture_id[3] == 0x53){
			pr_err("k60168 idle_flag enter");
			idle_flag = 1;
			return 1;
		}
		//pr_err("k60168_irq_handle 0x%x   0x%x\n",gesture_id[8], gesture_id[9]);
		switch (gesture_id[9]) {
			case 0:
				pr_err("k60168 irq handle resume enter");
				sleep_mode = 1;
				k60168_enable_flag = true;
				break;
			case 1://pat pat
				input_report_key(k60168->input, 234, 1);
				input_sync(k60168->input);
				input_report_key(k60168->input, 234, 0);
				input_sync(k60168->input);
				pr_info("k60168_irq_handle report 1");
				break;
			case 2: //wave hands
				input_report_key(k60168->input, 235, 1);
				input_sync(k60168->input);
				input_report_key(k60168->input, 235, 0);
				input_sync(k60168->input);
				pr_info("k60168_irq_handle report 2");
				break;
		#ifdef ZCFG_RADAR_LEFT_RIGHT_SWAP
			case 4: 
		#else
			case 3: //right move
		#endif
				input_report_key(k60168->input, 236, 1);
				input_sync(k60168->input);
				input_report_key(k60168->input, 236, 0);
				input_sync(k60168->input);
				pr_info("k60168_irq_handle report 3");
				break;
		#ifdef ZCFG_RADAR_LEFT_RIGHT_SWAP
			case 3:
		#else
			case 4: //left move
		#endif
				input_report_key(k60168->input, 233, 1);
				input_sync(k60168->input);
				input_report_key(k60168->input, 233, 0);
				input_sync(k60168->input);
				pr_info("k60168_irq_handle report 4");
				break;
			case 0xff:
				pr_err("k60168_irq_handle report 0xff ,suspend enter");
				k60168_enable_flag = false;
				break;
			default:
				pr_err("k60168_irq_handle error");
				return 0;
		}
		return 0;
	}
	return 0;
}
/*
int k60168_test(void *ptr)
{
	struct k60168 *k60168 = (struct k60168 *)ptr;
	uint32_t reg_val = 0;
	uint8_t gesture_id[12];

	pr_err("chen while0\n");

	do{
		pr_err("chen while\n");

		usleep_range(5000, 6000);
		generation_gesture_data(k60168, &reg_val);
		pr_err("k60168_irq_handle = 0x%x\n", reg_val);

		if(reg_val == 0x4a4b000c){
			read_gesture_id(k60168, gesture_id);
			pr_err("k60168_irq_handle 0x%x   0x%x\n",gesture_id[8], gesture_id[9]);
		}
	}while(!kthread_should_stop());
	return 0;
}*/

static irqreturn_t k60168_irq(int32_t irq, void *data)
{
	struct k60168 *k60168 = data;
	pr_err("%s enter\n", __func__);

	/* System should keep wakeup for 2 seconds at least. */
	__pm_wakeup_event(k60168->radar_ws, jiffies_to_msecs(K60168_WAKELOCK_TIMEOUT));

	k60168_irq_handle(k60168);

	return IRQ_HANDLED;
}


static int32_t k60168_interrupt_init(struct k60168 *k60168)
{
	int32_t irq_flags = 0;
	int32_t ret = 0;

	pr_info("%s enter\n", __func__);

	if (gpio_is_valid(k60168->irq_gpio)) {
		ret = devm_gpio_request_one(&k60168->i2c->dev,
							k60168->irq_gpio,
							GPIOF_DIR_IN,
							"k60168_irq_gpio");
		//ret = devm_gpio_request(&k60168->i2c->dev,k60168->irq_gpio, "k60168_irq_gpio");
		if (ret) {
			pr_err("%s: request irq gpio failed, ret = %d\n",
							__func__, ret);
			ret = -IRQIO_FAILED;
		} else {
			irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT ;
			k60168->to_irq = gpio_to_irq(k60168->irq_gpio);
			ret = devm_request_threaded_irq(&k60168->i2c->dev,
						k60168->to_irq,
						NULL, k60168_irq, irq_flags,
						"k60168_irq", k60168);
			enable_irq_wake(k60168->to_irq);
			if (ret != 0) {
				pr_err("%s: failed to request IRQ %d: %d\n",
						__func__,
						k60168->to_irq,
						ret);
				ret = -IRQ_REQUEST_FAILED;
			} else {
				pr_info("%s: IRQ request successfully!\n",
								__func__);
				ret = RADAR_SUCCESS;
			}
		}
	} else {
		pr_err("%s: irq gpio invalid!\n", __func__);
		return -IRQIO_FAILED;
	}
	return ret;
}

static void k60168_parse_dt(struct device *dev, struct k60168 *k60168,
							struct device_node *np)
{
	uint32_t val = 0;

	k60168->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (k60168->irq_gpio < 0) {
		k60168->irq_gpio = -1;
		pr_err("%s: no irq gpio provided.\n", __func__);
		return;
	} else {
		pr_info("%s: irq gpio provided ok.\n", __func__);
	}

	val = of_property_read_string(np, "chip_name", &k60168->chip_name);
	if (val != 0) {
		k60168->chip_name = NULL;
		pr_info("%s: failed to find chip name\n", __func__);
	} else {
		pr_info("%s: the chip name is %s detected\n", __func__,
							k60168->chip_name);
	}

}

static void k60168_i2c_set(struct i2c_client *i2c, struct k60168 *k60168)
{
	pr_info("%s: enter\n", __func__);
	k60168->dev = &i2c->dev;
	k60168->i2c = i2c;
	i2c_set_clientdata(i2c, k60168);
}

static int32_t
k60168_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct k60168 *k60168;
	struct device_node *np = i2c->dev.of_node;
	int32_t ret = 0;
	int rc = 0;
	struct k60168_upgrade *upg = NULL;
	uint32_t reg_val = 0;
	uint32_t fw_bin_ver = 0;
	uint32_t fw_bin_mid = 0;
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	k60168 = devm_kzalloc(&i2c->dev, sizeof(struct k60168), GFP_KERNEL);
	if (k60168 == NULL) {
		pr_err("%s:failed to malloc memory for k60168!\n", __func__);
		ret = -MALLOC_FAILED;
		goto err_malloc;
	}

	g_k60168_data = k60168;

	k60168_i2c_set(i2c, k60168);



	k60168->rst_gpio = of_get_named_gpio(np, "rst-gpio", 0);
	if (k60168->rst_gpio < 0) {
		pr_err("k60168 falied to get rst gpio!\n");
	}
	rc = devm_gpio_request(&i2c->dev,k60168->rst_gpio, "k60168_gpio_reset_pin");
	if (rc) {
		pr_err("failed to request k60168 rst-gpio , rc = %d\n", rc);
	}
	pr_err("k60168 reset gpio request ok\n");
	rc = gpio_direction_output(k60168->rst_gpio, 1);
	// msleep(200);
	pr_err("k60168_pull reset_high\n");

	k60168->pwr_gpio = of_get_named_gpio(np, "pwr-gpio", 0);
	if (k60168->pwr_gpio < 0) {
		pr_err("k60168 falied to get pwr gpio!\n");
	}
	rc = devm_gpio_request(&i2c->dev,k60168->pwr_gpio, "k60168_gpio_power_pin");
	if (rc) {
		pr_err("failed to request k60168 pwr-gpio , rc = %d\n", rc);
	}
	pr_err("k60168 power gpio request ok\n");
	rc = gpio_direction_output(k60168->pwr_gpio, 1);
	pr_err("k60168 enable power\n");

	k60168->enable_gpio = of_get_named_gpio(np, "enable-gpio", 0);
	if (k60168->enable_gpio < 0) {
		pr_err("k60168 falied to get enable gpio!\n");
	}
	rc = devm_gpio_request(&i2c->dev,k60168->enable_gpio, "k60168_gpio_enable_pin");
	if (rc) {
		pr_err("failed to request k60168 enable-gpio , rc = %d\n", rc);
	}
	pr_err("k60168 enable gpio request ok\n");
	rc = gpio_direction_output(k60168->enable_gpio, 1);
	pr_err("k60168 enable ok\n");

	// msleep(100);

	generation_gesture_data(k60168, &reg_val);

	rc = get_chip_id(k60168);
	if(rc == 0)
	{
		return -ENODEV;
	}
	pr_err(" k60168 chip id rc = %d\n", rc);
	// msleep(100);

	get_firmware_version(k60168);
	pr_info(" k60168 get k60168_fw_version %d\n",k60168_fw_version);

	k60168->firmware_flag = true;

	if (NULL == fwupgrade)
    {
        fwupgrade = (struct k60168_upgrade *)kzalloc(sizeof(*fwupgrade), GFP_KERNEL);
        if (NULL == fwupgrade)
        {
            pr_info("malloc memory for upgrade fail");
        }else{
			upg = fwupgrade;

			if (!fwupg_get_fw_file(upg))
			{
				pr_info("get file fail, can't upgrade");
				update_enable = false;
				zyt_info_sx("[Radar-k60168] : FW mid:", k60168_module_id);
				zyt_info_sx("[Radar-k60168] : FW Version:", k60168_fw_version);
			}else{
				fw_bin_ver = upg->fw[0x10000];
				fw_bin_mid = upg->fw[0x10006];	
				if(k60168_module_id != fw_bin_mid){
					update_enable = true;
				}else{
					pr_info("k60168 fw version = 0x%x",upg->fw[0x10000]);
					if((k60168_fw_version != fw_bin_ver) ){
						update_enable = true;
					}else {
						update_enable = false;
						zyt_info_sx("[Radar-k60168] : FW mid:", k60168_module_id);
						zyt_info_sx("[Radar-k60168] : FW Version:", k60168_fw_version);
						
					}

				}
			}
		}
    }
	
	
	if(update_enable){
	
		k60168_close_int(k60168);
		
		k60168->ts_workqueue = create_singlethread_workqueue("radar_update_wq");
		if (!k60168->ts_workqueue)
		{
			pr_err("create ts_workqueue fail\n");
			goto err_wq;
		}

		ret = k60168_cfg_init(k60168);
		if (ret < 0) {
			pr_info("%s: cfg situation not confirmed!\n", __func__);
			goto err_cfg;
		}
	}
	//<AI_BSP_RADAR><chen.liang><2023-09-18> add for radar hw detect end

	k60168_parse_dt(&i2c->dev, k60168, np);

	ret = k60168_interrupt_init(k60168);
	if (ret == -IRQ_REQUEST_FAILED) {
		pr_err("%s: request irq failed!, ret=%d\n", __func__, ret);
		goto err_requst_irq;
	}

	/* input device */
	ret = k60168_input_sys_init(k60168);
	if (ret == -INPUT_ALLOCATE_FILED) {
		pr_err("%s:allocate input failed, ret = %d\n", __func__, ret);
		goto exit_input_alloc_failed;
	} else if (ret == -INPUT_REGISTER_FAILED) {
		pr_err("%s:register input failed, ret = %d\n", __func__, ret);
		goto exit_input_register_device_failed;
	}

	/* attribute */
	ret = sysfs_create_group(&i2c->dev.kobj, &k60168_radar_attribute_group);
	if (ret < 0) {
		dev_info(&i2c->dev, "%s error creating sysfs attr files\n",
			 __func__);
		goto err_sysfs;
	}

		ret = sysfs_create_link(NULL, &i2c->dev.kobj, "radar");
	if (ret < 0) {
		dev_info(&i2c->dev, "%s Failed to create link!\n",
			 __func__);
		sysfs_remove_link(NULL, "radar");
	}

	k60168->radar_ws =
	wakeup_source_register(NULL, "radar_wake_lock");
	if(!k60168->radar_ws) {
		pr_err("init radar wakeup_source fails");
	}

	//k60168 radar close default
	//<AI_BSP_RADAR><chen.liang><2023-09-18> add for radar hw detect start
	if(!update_enable){
#ifdef CONFIG_AI_BSP_MTK_DEVICE_CHECK
		k60168_register_hardware_info();
#endif
		k60168_sleep(k60168);
	}
	//<AI_BSP_RADAR><chen.liang><2023-09-18> add for radar hw detect end

/*	thread = kthread_run(k60168_test, k60168, "K60168_test");
	if (IS_ERR(thread))
	{
		ret = PTR_ERR(thread);
		pr_err("failed to create kernel thread: %d\n", ret);
		goto err_malloc;
	}
*/

	return RADAR_SUCCESS;



err_sysfs:
	sysfs_remove_group(&i2c->dev.kobj, &k60168_radar_attribute_group);
	input_unregister_device(k60168->input);
exit_input_register_device_failed:
	input_free_device(k60168->input);
exit_input_alloc_failed:
err_requst_irq:
	if (gpio_is_valid(k60168->irq_gpio))
		devm_gpio_free(&i2c->dev, k60168->irq_gpio);

err_cfg:
err_wq:    
	if (k60168->ts_workqueue)
        destroy_workqueue(k60168->ts_workqueue);
//err_pinctrl:
//err_first_irq:
//err_chipid:
err_malloc:
	return ret;
}


static int32_t k60168_i2c_remove(struct i2c_client *i2c)
{
	struct k60168 *k60168 = i2c_get_clientdata(i2c);

	wakeup_source_unregister(k60168->radar_ws);
	destroy_workqueue(k60168->ts_workqueue);
//	kthread_stop(thread);
	gpio_direction_output(k60168->rst_gpio, 0);
	gpio_direction_output(k60168->pwr_gpio, 0);
	gpio_direction_output(k60168->enable_gpio, 0);
	msleep(5);
	pr_err("k60168_power_off\n");

//	k60168_pinctrl_deinit(k60168);
	sysfs_remove_group(&i2c->dev.kobj, &k60168_radar_attribute_group);
	input_unregister_device(k60168->input);
	if (gpio_is_valid(k60168->irq_gpio))
		devm_gpio_free(&i2c->dev, k60168->irq_gpio);
	if (gpio_is_valid(k60168->rst_gpio))
		devm_gpio_free(&i2c->dev, k60168->rst_gpio);
	if (gpio_is_valid(k60168->pwr_gpio))
		devm_gpio_free(&i2c->dev, k60168->pwr_gpio);
	if (gpio_is_valid(k60168->enable_gpio))
		devm_gpio_free(&i2c->dev, k60168->enable_gpio);

	return 0;
}

/*
static int k60168_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct k60168 *k60168 = i2c_get_clientdata(client);

	uint32_t reg_val = 0;

	pr_info("k60168 suspend enter\n");

	usleep_range(5000, 6000);
	generation_gesture_data(k60168, &reg_val);
	pr_err("k60168 suspend = 0x%x\n", reg_val);

	disable_irq(k60168->to_irq);
	if (device_may_wakeup(dev))
		enable_irq_wake(k60168->to_irq);
	enable_irq_wake(k60168->to_irq);

	return 0;
}

static int k60168_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct k60168 *k60168 = i2c_get_clientdata(client);

	uint32_t reg_val = 0;

	pr_info("k60168 resume enter\n");

	usleep_range(5000, 6000);
	generation_gesture_data(k60168, &reg_val);
	pr_err("k60168 resume = 0x%x\n", reg_val);

	if (device_may_wakeup(dev))
		disable_irq_wake(k60168->to_irq);
	enable_irq(k60168->to_irq);


	return 0;
}

static const struct dev_pm_ops k60168_pm_ops = {
	.suspend = k60168_suspend,
	.resume = k60168_resume,
};
*/
static const struct of_device_id k60168_dt_match[] = {
	{ .compatible = "radar,k60168_radar" },
	{ },
};

static const struct i2c_device_id k60168_i2c_id[] = {
	{ k60168_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver k60168_i2c_driver = {
	.driver = {
		.name = k60168_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(k60168_dt_match),
//		.pm = &k60168_pm_ops,
	},
	.probe = k60168_i2c_probe,
	.remove = k60168_i2c_remove,
	.id_table = k60168_i2c_id,
};

static int32_t __init k60168_i2c_init(void)
{
	int32_t ret = 0;

	pr_err("k60168 driver init \n");

	ret = i2c_add_driver(&k60168_i2c_driver);
	if (ret) {
		pr_err("fail to add k60168 device into i2c\n");
		return ret;
	}

	return 0;
}

static void __exit k60168_i2c_exit(void)
{
	i2c_del_driver(&k60168_i2c_driver);
}

module_init(k60168_i2c_init);
module_exit(k60168_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("chen.liang<liangc@ant-ai.cn>");
MODULE_DESCRIPTION("Ant K60168 radar Driver");
