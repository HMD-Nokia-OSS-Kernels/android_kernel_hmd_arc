#include <linux/spi/spi.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include "spi_tiny_lcd_gc9a01.h"
#include "show.h"

/*-------------------------------------------------------------------------*/
static struct stl_data g_data;
static struct stl_data *gp_data = &g_data;
static u8 dst[25600];
static struct mutex mutex_spi_write;
static uint32_t lcd_current_suspend = 0;
static struct pic_info gp_pic_info ;


//static u8 light_step = 0;
#define LCDMAGIC 'v'
#define LCD_IOC_INIT _IO(LCDMAGIC, 0)
#define LCD_IOC_SUSPEND_RESUME_ON_OF  _IOW(LCDMAGIC, 1, int)
#define LCD_IOC_CURRENT_SUSPEND   _IOW(LCDMAGIC, 2, unsigned int)
#define LCD_IOC_SET_LIGHT_VALUE   _IOW(LCDMAGIC, 3, unsigned int)
/*-------------------------------------------------------------------------*/

static ssize_t
spidev_sync(struct stl_data *spidev, struct spi_message *message)
{
	int status;
	struct spi_device *spi;

	spin_lock_irq(&spidev->spi_lock);
	spi = spidev->spi;
	spin_unlock_irq(&spidev->spi_lock);

	if (spi == NULL)
		status = -ESHUTDOWN;
	else
		status = spi_sync(spi, message);

	if (status == 0)
		status = message->actual_length;

	return status;
}

static ssize_t spidev_sync_write_cmd(unsigned char cmd)
{
	int ret = 0;
	struct stl_data *spidev = gp_data;
	struct spi_message	m;
	struct spi_transfer	t = {
		.len    = 1,
		.speed_hz	= spidev->speed_hz,
	}; 

	spidev->tx_buffer = &cmd;
	t.tx_buf		= spidev->tx_buffer ;

	spi_message_init(&m);
	ret = gpio_direction_output(gp_data->rs_ctrl_gpio, 0);
	if (ret) {
		LOG_ERR("[GPIO]set_direction for rs_ctrl gpio failed");
		return ret;
	}
	spi_message_add_tail(&t, &m);
	return spidev_sync(spidev, &m);
}

static ssize_t spidev_sync_write_data(unsigned char dat)
{
	int ret = 0;
	struct stl_data *spidev = gp_data;
	struct spi_message	m;
	struct spi_transfer	t = {
		.len    = 1,
		.speed_hz	= spidev->speed_hz,
	}; 

	spidev->tx_buffer = &dat;
	t.tx_buf		= spidev->tx_buffer ;


	spi_message_init(&m);
	ret = gpio_direction_output(gp_data->rs_ctrl_gpio, 1);
	if (ret) {
		LOG_ERR("[GPIO]set_direction for rs_ctrl gpio failed");
		return ret;
	}
	
	spi_message_add_tail(&t, &m);
	return spidev_sync(spidev, &m);
}

static ssize_t spidev_sync_write_pic_data(const void *dat , unsigned int len)
{
	int ret = 0;
	struct stl_data *spidev = gp_data;
	struct spi_message	m;
	struct spi_transfer	t = {
		.tx_buf		= dat,
		.len    		= len,
		.speed_hz	= spidev->speed_hz,
	}; 

	spi_message_init(&m);
	ret = gpio_direction_output(gp_data->rs_ctrl_gpio, 1);
	if (ret) {
		LOG_ERR("[GPIO]set_direction for rs_ctrl gpio failed");
		return ret;
	}
	
	spi_message_add_tail(&t, &m);
	return spidev_sync(spidev, &m);
}


static int stl_parse_dt(struct stl_data *pointer)
{
	int ret = 0;
	struct device_node *np = gp_data->spi->dev.of_node;

	LOG_INF("entry\n");
	/* reset gpio info */
	gp_data->reset_gpio = of_get_named_gpio(np, "tiny_lcd,reset-gpio", 0);
	if (gp_data->reset_gpio < 0) {
		LOG_ERR("Unable to get reset_gpio");
	}else{
		LOG_INF("tiny_lcd,rest-gpio:%d\n", gp_data->reset_gpio);
	}
	/* Datta/Command Conttrrol gpio info */
	gp_data->rs_ctrl_gpio = of_get_named_gpio(np, "tiny_lcd,rs_ctrl-gpio", 0);
	if (gp_data->rs_ctrl_gpio < 0) {
		LOG_ERR("Unable to get rs_ctrl_gpio");
	}else{
		LOG_INF("tiny_lcd,rs_ctrl-gpio:%d\n", gp_data->rs_ctrl_gpio);
	}

	/* request reset gpio */
	if (gpio_is_valid(gp_data->reset_gpio)) {
		ret = gpio_request(gp_data->reset_gpio, "lcd2_reset_gpio");
		if (ret) {
			LOG_ERR("[GPIO]reset gpio request failed");
			goto err_reset_gpio_req;
		}

		ret = gpio_direction_output(gp_data->reset_gpio, 0);
		if (ret) {
			LOG_ERR("[GPIO]set_direction for reset gpio failed");
			goto err_reset_gpio_dir;
		}
	}
	/* request rs_ctrl gpio */
	if (gpio_is_valid(gp_data->rs_ctrl_gpio)) {
		ret = gpio_request(gp_data->rs_ctrl_gpio, "lcd2_dc_gpio");
		if (ret) {
			LOG_ERR("[GPIO]rs_ctrl gpio request failed");
			goto err_reset_gpio_dir;
		}

		ret = gpio_direction_output(gp_data->rs_ctrl_gpio, 0);
		if (ret) {
			LOG_ERR("[GPIO]set_direction for rs_ctrl gpio failed");
			goto err_rs_ctrl_gpio_dir;
		}
	}

	ret = of_property_read_u32(np, "spi-max-frequency", &gp_data->spi_max_frequency);
	if (ret < 0) {
		gp_data->spi_max_frequency = MAX_SPI_SPEED;
		LOG_ERR("Unable to get spi-max-frequency, set default value %d\n", gp_data->spi_max_frequency);
	}else {
		LOG_INF("get spi-max-frequency, value = %d\n", gp_data->spi_max_frequency);
	}

	LOG_INF("successful \n");
	return 0;

err_rs_ctrl_gpio_dir:
	if (gpio_is_valid(gp_data->rs_ctrl_gpio))
		gpio_free(gp_data->rs_ctrl_gpio);

err_reset_gpio_dir:
	if (gpio_is_valid(gp_data->reset_gpio))
		gpio_free(gp_data->reset_gpio);
err_reset_gpio_req:

	LOG_ERR("error:%d \n", ret);
	return ret;
}

/*************wyx add*****************/
#ifdef AGN_TFT_LCD_GC9A01
static inline void initial_gc9a01(void)
 {	
	spidev_sync_write_cmd(0xFE);			 
	spidev_sync_write_cmd(0xEF); 

	spidev_sync_write_cmd(0xEB);	
	spidev_sync_write_data(0x14); 

	spidev_sync_write_cmd(0x84);			
	spidev_sync_write_data(0x60); //40->60 0xb5 en  20210529  james

	spidev_sync_write_cmd(0x88);			
	spidev_sync_write_data(0x0A);

	spidev_sync_write_cmd(0x89);			
	spidev_sync_write_data(0x23);  ///spi 2data reg en  20210529

	spidev_sync_write_cmd(0x8A);			
	spidev_sync_write_data(0x00); 

	spidev_sync_write_cmd(0x8B);			
	spidev_sync_write_data(0x80); 

	spidev_sync_write_cmd(0x8C);			
	spidev_sync_write_data(0x01); 

	spidev_sync_write_cmd(0x8D);			
	spidev_sync_write_data(0x03); 

	spidev_sync_write_cmd(0x8F);			
	spidev_sync_write_data(0xFF); 
	spidev_sync_write_cmd(0xB5);   
	spidev_sync_write_data(0x08);
	spidev_sync_write_data(0x09);//  james add 20210529
	spidev_sync_write_data(0x14);
	spidev_sync_write_data(0x08);

	spidev_sync_write_cmd(0xB6);			
	spidev_sync_write_data(0x00); 
	spidev_sync_write_data(0x60); 

	spidev_sync_write_cmd(0x36);			
	spidev_sync_write_data(0x88);
	
	spidev_sync_write_cmd(0x35); //Tearing Effect Line On 
	spidev_sync_write_data(0x00); //The Tearing Effect output Line consists of both V-Blanking and H-Blanking information

	spidev_sync_write_cmd(0x3A);			
	spidev_sync_write_data(0x05); 


	spidev_sync_write_cmd(0x90);			
	spidev_sync_write_data(0x08);
	spidev_sync_write_data(0x08);
	spidev_sync_write_data(0x08);
	spidev_sync_write_data(0x08); 

	spidev_sync_write_cmd(0xBA);			
	spidev_sync_write_data(0x0a);//TE WIDTH

	spidev_sync_write_cmd(0xBD);			
	spidev_sync_write_data(0x06);

	spidev_sync_write_cmd(0xBC);			
	spidev_sync_write_data(0x00);	

	spidev_sync_write_cmd(0xFF);			
	spidev_sync_write_data(0x60);
	spidev_sync_write_data(0x01);
	spidev_sync_write_data(0x04);


	spidev_sync_write_cmd(0xC3);			
	spidev_sync_write_data(0x2F);
	spidev_sync_write_cmd(0xC4);			
	spidev_sync_write_data(0x2F);

	spidev_sync_write_cmd(0xC9);			
	spidev_sync_write_data(0x25);


	spidev_sync_write_cmd(0xBE);			
	spidev_sync_write_data(0x11); 

	spidev_sync_write_cmd(0xE1);			
	spidev_sync_write_data(0x10);
	spidev_sync_write_data(0x0E);

	spidev_sync_write_cmd(0xDF);			
	spidev_sync_write_data(0x21);
	spidev_sync_write_data(0x10);
	spidev_sync_write_data(0x02);

	spidev_sync_write_cmd(0xF0);   
	spidev_sync_write_data(0x49);
	spidev_sync_write_data(0x0e);
	spidev_sync_write_data(0x09);
	spidev_sync_write_data(0x09);
	spidev_sync_write_data(0x25);
	spidev_sync_write_data(0x2e);

	spidev_sync_write_cmd(0xF1);    
	spidev_sync_write_data(0x44);
	spidev_sync_write_data(0x70);
	spidev_sync_write_data(0x73);
	spidev_sync_write_data(0x2F);
	spidev_sync_write_data(0x30);  
	spidev_sync_write_data(0x6F);

	spidev_sync_write_cmd(0xF2);   
	spidev_sync_write_data(0x49);
	spidev_sync_write_data(0x0e);
	spidev_sync_write_data(0x09);
	spidev_sync_write_data(0x09);
	spidev_sync_write_data(0x25);
	spidev_sync_write_data(0x2e);

	spidev_sync_write_cmd(0xF3);   
	spidev_sync_write_data(0x44);
	spidev_sync_write_data(0x70);
	spidev_sync_write_data(0x73);
	spidev_sync_write_data(0x2F);
	spidev_sync_write_data(0x30);  
	spidev_sync_write_data(0x6F);

	spidev_sync_write_cmd(0xED);	
	spidev_sync_write_data(0x1B); 
	spidev_sync_write_data(0x8B); 

	spidev_sync_write_cmd(0xAE);			
	spidev_sync_write_data(0x77);

	spidev_sync_write_cmd(0xCD);			
	spidev_sync_write_data(0x63);		

	spidev_sync_write_cmd(0xAC);			
	spidev_sync_write_data(0x27);

	spidev_sync_write_cmd(0x70);			
	spidev_sync_write_data(0x07);
	spidev_sync_write_data(0x07);
	spidev_sync_write_data(0x04);
	spidev_sync_write_data(0x06);//VGH
	spidev_sync_write_data(0x0F); //VGL
	spidev_sync_write_data(0x09);
	spidev_sync_write_data(0x07);
	spidev_sync_write_data(0x08);
	spidev_sync_write_data(0x03);

	spidev_sync_write_cmd(0xE8);			
	spidev_sync_write_data(0x44);//24 2DOT

	spidev_sync_write_cmd(0x60);		
	spidev_sync_write_data(0x38);
	spidev_sync_write_data(0x0B);
	spidev_sync_write_data(0x6D);
	spidev_sync_write_data(0x6D);

	spidev_sync_write_data(0x39);
	spidev_sync_write_data(0xF0);
	spidev_sync_write_data(0x6D);
	spidev_sync_write_data(0x6D);


	spidev_sync_write_cmd(0x61);
	spidev_sync_write_data(0x38);
	spidev_sync_write_data(0xF4);
	spidev_sync_write_data(0x6D);
	spidev_sync_write_data(0x6D);

	spidev_sync_write_data(0x38);
	spidev_sync_write_data(0xF7);
	spidev_sync_write_data(0x6D);
	spidev_sync_write_data(0x6D);
	/////////////////////////////////////
	spidev_sync_write_cmd(0x62);
	spidev_sync_write_data(0x38);
	spidev_sync_write_data(0x0D);
	spidev_sync_write_data(0x71);
	spidev_sync_write_data(0xED);
	spidev_sync_write_data(0x70);
	spidev_sync_write_data(0x70);
	spidev_sync_write_data(0x38);
	spidev_sync_write_data(0x0F);
	spidev_sync_write_data(0x71);
	spidev_sync_write_data(0xEF);
	spidev_sync_write_data(0x70); 
	spidev_sync_write_data(0x70);

	spidev_sync_write_cmd(0x63);			
	spidev_sync_write_data(0x38);
	spidev_sync_write_data(0x11);
	spidev_sync_write_data(0x71);
	spidev_sync_write_data(0xF1);
	spidev_sync_write_data(0x70);
	spidev_sync_write_data(0x70);
	spidev_sync_write_data(0x38);
	spidev_sync_write_data(0x13);
	spidev_sync_write_data(0x71);
	spidev_sync_write_data(0xF3);
	spidev_sync_write_data(0x70); 
	spidev_sync_write_data(0x70);


	spidev_sync_write_cmd(0x64);			
	spidev_sync_write_data(0x28);
	spidev_sync_write_data(0x29);
	spidev_sync_write_data(0xF1);
	spidev_sync_write_data(0x01);
	spidev_sync_write_data(0xF1);
	spidev_sync_write_data(0x00);//
	spidev_sync_write_data(0x1a);//

	spidev_sync_write_cmd(0x66);
	spidev_sync_write_data(0x3C);
	spidev_sync_write_data(0x00);
	spidev_sync_write_data(0x98);
	spidev_sync_write_data(0x10);
	spidev_sync_write_data(0x32);
	spidev_sync_write_data(0x45);
	spidev_sync_write_data(0x01);
	spidev_sync_write_data(0x00);
	spidev_sync_write_data(0x00);
	spidev_sync_write_data(0x00);
	spidev_sync_write_cmd(0x67);
	spidev_sync_write_data(0x00);
	spidev_sync_write_data(0x3C);
	spidev_sync_write_data(0x00);
	spidev_sync_write_data(0x00);
	spidev_sync_write_data(0x00);
	spidev_sync_write_data(0x10);
	spidev_sync_write_data(0x54);
	spidev_sync_write_data(0x67);
	spidev_sync_write_data(0x45);
	spidev_sync_write_data(0xcd);


	spidev_sync_write_cmd(0x74);			
	spidev_sync_write_data(0x10);	
	spidev_sync_write_data(0x79);	//85
	spidev_sync_write_data(0x80);
	spidev_sync_write_data(0x00); 
	spidev_sync_write_data(0x00); 
	spidev_sync_write_data(0x4E);
	spidev_sync_write_data(0x00);					

	spidev_sync_write_cmd(0x98);			
	spidev_sync_write_data(0x3e);
	spidev_sync_write_data(0x07);
	spidev_sync_write_cmd(0x99);			
	spidev_sync_write_data(0x3e);
	spidev_sync_write_data(0x07);


	spidev_sync_write_cmd(0x35);	
	spidev_sync_write_cmd(0x21);
	mdelay(120);

	spidev_sync_write_cmd( 0x0011);
	mdelay(500);
	spidev_sync_write_cmd( 0x0029);
	mdelay(120);

	spidev_sync_write_cmd( 0x002c);
 }

void set_windows_xy(uint x_start,uint x_end,uint y_start,uint y_end)
{

	spidev_sync_write_cmd(0x2A);
	spidev_sync_write_data(x_start>>8);
	spidev_sync_write_data(x_start&0xFF);
	spidev_sync_write_data(x_end>>8);
	spidev_sync_write_data(x_end&0xFF);

	spidev_sync_write_cmd(0x2B);       
	spidev_sync_write_data(y_start>>8);
	spidev_sync_write_data(y_start&0xFF);  
	spidev_sync_write_data(y_end>>8);
	spidev_sync_write_data(y_end&0xFF);	

	spidev_sync_write_cmd(0x2C); //Memory Write
 }	 

/*
static void spi_lcd_change_backlight(u8 step)
{
	u8 i;
	if(step > 7)
	{
		LOG_ERR("backlight step setting err!");
		return;
	}
	gpio_direction_output(gp_data->en_12v_gpio, 0);		//关闭背光
	mdelay(5);
	gpio_direction_output(gp_data->en_12v_gpio, 1);
	udelay(35);
	for(i = 0; i < step; i++)
	{	
		gpio_direction_output(gp_data->en_12v_gpio, 0);
		udelay(5);
		gpio_direction_output(gp_data->en_12v_gpio, 1);
		udelay(5);
	}
	mdelay(1);
}
*/
static void spi_lcd_suspend(void)
{
	LOG_INF("enter\n");
	spidev_sync_write_cmd(0x28);
	mdelay(50);
	spidev_sync_write_cmd(0x10);
	mdelay(120);		
}

static void spi_lcd_resume(void)
{
	LOG_INF("enter\n");
	spidev_sync_write_cmd(0x11);
	mdelay(120);
	spidev_sync_write_cmd(0x29);
	mdelay(10);
}

static ssize_t tiny_lcd_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	int err;
    err = copy_to_user(buf, dst, 1);
    if(err < 0){
        return err;
    }
	*f_pos += 1;
	LOG_INF("read on!!!\n");
    return 1;
}
			
static ssize_t tiny_lcd_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{

	int ret=0;
	unsigned int pic_pixel = 0 ;
    if(count > (sizeof(struct pic_info))){
		return -EINVAL;
    }

	mutex_lock(&mutex_spi_write);
	memset(&gp_pic_info, 0, sizeof(struct pic_info));
	ret = copy_from_user(&gp_pic_info, buf, count);
	
	LOG_INF("gp_pic_info.x_start = %d,gp_pic_info.x_end = %d,gp_pic_info.y_start = %d,gp_pic_info.y_end = %d, gp_pic_info.pic_data[0]=0x%x\n",gp_pic_info.x_start,gp_pic_info.x_end,gp_pic_info.y_start,gp_pic_info.y_end,gp_pic_info.pic_data[0]);
	set_windows_xy(gp_pic_info.x_start,gp_pic_info.x_end,gp_pic_info.y_start,gp_pic_info.y_end);	
	
	pic_pixel = (gp_pic_info.x_end-gp_pic_info.x_start+1) * (gp_pic_info.y_end-gp_pic_info.y_start+1)  * 2;  //picture pixel 16bit
	spidev_sync_write_pic_data(gp_pic_info.pic_data,pic_pixel); 
	*f_pos += count;
	mutex_unlock(&mutex_spi_write);
	
    return count;
}

static long tiny_lcd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
    mutex_lock(&mutex_spi_write);
    switch (cmd)
    {
        case LCD_IOC_INIT :
        {
            LOG_INF("oled init ...\n");
            break;
        }

        case LCD_IOC_SUSPEND_RESUME_ON_OF:
			LOG_INF("arg:%d\n", arg);
			if (arg)
			{
				spi_lcd_suspend();	
			}
			else
			{
				spi_lcd_resume();
			}
		break;
		/*
		case LCD_IOC_SET_LIGHT_VALUE :
        {
			LOG_INF("LCD_IOC_SET_LIGHT_VALUE arg=%d\n",arg);
            if(arg == 0){
				spi_lcd_change_backlight(7);	//最低亮度
				light_step = 7;
			}
			else if(arg == 1){
				spi_lcd_change_backlight(5);
				light_step = 5;
			}
			else if(arg == 2){
				spi_lcd_change_backlight(2);
				light_step = 2;
			}
			else if(arg == 3)
			{
				spi_lcd_change_backlight(0);	//最高亮度
				light_step = 0;
			}
            break;
        }
		*/
		case LCD_IOC_CURRENT_SUSPEND:
		{
			ret = copy_to_user((unsigned int*)arg, &lcd_current_suspend, sizeof(unsigned int));
			LOG_INF("lcd_current_suspend =%d, ret=%d\n",lcd_current_suspend,ret);
			break;
		}
		default:
	        printk(KERN_WARNING "Invalid command\n");
	        ret = -EINVAL; 
    }
	mutex_unlock(&mutex_spi_write);
    return 0;
}

static long tiny_lcd_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return tiny_lcd_ioctl(filp,cmd,arg);
}
static const struct file_operations tiny_lcd_fops = {
	.owner = THIS_MODULE,
	.read = tiny_lcd_read,	//read for test
	.write = tiny_lcd_write,
	.unlocked_ioctl = tiny_lcd_ioctl,
	#ifdef CONFIG_COMPAT
	.compat_ioctl = tiny_lcd_compat_ioctl,
	#endif
};

static int init_file_node(void)
{
	int ret = 0 ;
	
	gp_data->tiny_lcd_cdev = cdev_alloc();
	if(gp_data->tiny_lcd_cdev == NULL) {
		goto err_cdev_alloc;
		LOG_ERR("cdev_alloc failed!\n");
	}
	
	ret = alloc_chrdev_region(&gp_data->dev_no, 0, 1, "spi_tiny_lcd");
	if(ret){
		LOG_ERR("alloc_chrdev_region failed!\n");
		goto err_alloc_chrdev_region;
	}

	cdev_init(gp_data->tiny_lcd_cdev, &tiny_lcd_fops);

	gp_data->tiny_lcd_cdev->owner = THIS_MODULE;
	if(cdev_add(gp_data->tiny_lcd_cdev, gp_data->dev_no, 1)){
		LOG_ERR("cdev_add failed!\n");
		goto err_cdev_add;
	}
	
	gp_data->tiny_lcd_class = class_create(THIS_MODULE, "tiny_lcd_cls");
	if(IS_ERR(gp_data->tiny_lcd_class)) {
		ret = PTR_ERR(gp_data->tiny_lcd_class);
		LOG_ERR("class_create failed!\n");
		goto err_class_create;
	}
  
	gp_data->tiny_lcd_device = device_create(gp_data->tiny_lcd_class, NULL, gp_data->dev_no, NULL, "tiny_lcd"); // /dev/tiny_lcd 
	if (IS_ERR(gp_data->tiny_lcd_device)) {
		ret = PTR_ERR(gp_data->tiny_lcd_device);   
		goto err_device_create;
    }
	LOG_INF(" pass\n");
 	return ret;	
err_device_create:
//	device_destroy(gp_data->tiny_lcd_class, 0);     
	class_destroy(gp_data->tiny_lcd_class);    
err_class_create:
	cdev_del(gp_data->tiny_lcd_cdev);                         
err_cdev_add:	 
	unregister_chrdev_region(gp_data->dev_no, 1);
err_alloc_chrdev_region:
	kfree(gp_data->tiny_lcd_cdev); 
err_cdev_alloc:		
	return ret;		
}

#endif

/**************wyx add end************/

// void show_style1pic(void)
// {
// 	//unsigned int x ,y , k=0;
// 	unsigned int size = sizeof(style1) / sizeof(style1[0]);
   
// 	set_windows_xy(0,79,0,159);

// 	spidev_sync_write_pic_data(style1,size);
// #if 0
// 	for (x=0;x < 80;x++)
// 	{	
// 		for (y=0;y < 160; y++)
// 		{	
// 			spidev_sync_write_pic_data(style1[k]); 
// 			spidev_sync_write_pic_data(style1[k+1]);  
// 			k+=2;
// 		}
// 	}
// #endif

// }

static int spi_probe(struct spi_device *spi)
{
	int ret = 0;
	struct device_node *node;
    struct platform_device *pdev;
    struct device *dev;
	struct regmap *sc2730_regmap;
	unsigned int reg_val;

	LOG_INF("entry, of_node_full_name:%s \n",of_node_full_name(spi->dev.of_node));

	//fill driver private data 
	gp_data->spi = spi;

	stl_parse_dt(gp_data);
	gp_data->speed_hz = gp_data->spi_max_frequency;
	LOG_INF("speed_hz:%d \n", gp_data->speed_hz);
	if (!gp_data->tx_buffer) {
		gp_data->tx_buffer = kmalloc(BUFSIZE, GFP_KERNEL);
		if (!gp_data->tx_buffer) {
			//LOG_ERR("kmalloc error:%u \n", gp_data->tx_buffer);
			LOG_ERR("kmalloc error!\n");
			ret = -ENOMEM;
			goto err_alloc_tx_buf;
		}
	}
	if (!gp_data->rx_buffer) {
		gp_data->rx_buffer = kmalloc(BUFSIZE, GFP_KERNEL);
		if (!gp_data->rx_buffer) {
			dev_dbg(&gp_data->spi->dev, "open/ENOMEM\n");
			ret = -ENOMEM;
			goto err_alloc_rx_buf;
		}
	}
	//config spi param
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;

	init_file_node();
	mdelay(10);	

 	gpio_direction_output(gp_data->reset_gpio, 1);
	mdelay(20);
	gpio_direction_output(gp_data->reset_gpio, 0);
	mdelay(80);
	gpio_direction_output(gp_data->reset_gpio, 1);
	mdelay(80);

	//initialize LCD
#ifdef AGN_TFT_LCD_GC9A01
	initial_gc9a01();
	//show_style1pic();
	//mdelay(20);
	spi_lcd_suspend();
#endif

    node = of_find_compatible_node(NULL, NULL, "sprd,sc2730-bltc");
    if (!node) {
        LOG_INF("Failed to find compatible sprd,sc2730-bltc device tree node\n");
        return -ENODEV;
    }
    pdev = of_find_device_by_node(node);
    of_node_put(node);
    if (!pdev) {
        LOG_INF("Failed to find sprd,sc2730-bltc platform device\n");
        return -ENODEV;
    }

    dev = &pdev->dev;
	sc2730_regmap = dev_get_regmap(dev->parent, NULL);
	ret = regmap_update_bits(sc2730_regmap, 0x1B4, GENMASK(7, 0),0xf8);
	if (ret < 0) {
		LOG_INF("Failed to update 0x1B4 register: %d\n", ret);
	}
	ret = regmap_read(sc2730_regmap, 0x1B4, &reg_val);
	if (ret < 0) {
		LOG_INF("Failed to read 0x1B4 register: %d\n", ret);
	}
	LOG_INF("addr :0x1B4 val = 0x%02x\n",reg_val);
	LOG_INF("successful \n");
	return 0;

err_alloc_rx_buf:
	kfree(gp_data->rx_buffer);
	gp_data->rx_buffer = NULL;

err_alloc_tx_buf:
	kfree(gp_data->tx_buffer);
	gp_data->tx_buffer = NULL;

	return ret;
}

static int spi_remove(struct spi_device *spi)
{
	LOG_INF("entry\n");

	if(gp_data->tx_buffer){
		kfree(gp_data->tx_buffer);
		gp_data->tx_buffer = NULL;
	}

	if(gp_data->rx_buffer){
		kfree(gp_data->rx_buffer);
		gp_data->rx_buffer = NULL;
	}
	
	return 0;
}

static const struct of_device_id spidev_dt_ids[] = {
	{.compatible = "hxytech, spi_tiny_lcd"},
	{},
};

MODULE_DEVICE_TABLE(of, spidev_dt_ids);

static struct spi_driver spi_tiny_lcd_driver = {
	.driver = {
		   .name = "spi_tiny_lcd",	//stl
		   .bus = &spi_bus_type,
		   .owner = THIS_MODULE,
		   .of_match_table = spidev_dt_ids,
		   },
	.probe = spi_probe,
	.remove = spi_remove,
};

static int __init spi_tiny_lcd_init(void)
{
	int err = 0;
	LOG_INF("entry, build\n");
	mutex_init(&mutex_spi_write);
	err = spi_register_driver(&spi_tiny_lcd_driver);
	if (err < 0) {
		LOG_ERR("status:%d \n", err);
		goto err_spi_register_driver;
	}
	return 0;

err_spi_register_driver:
	return err;
}

static void __exit spi_tiny_lcd_exit(void)
{
	LOG_INF("entry\n");

	spi_unregister_driver(&spi_tiny_lcd_driver);

	return;
}

module_init(spi_tiny_lcd_init);
module_exit(spi_tiny_lcd_exit);

MODULE_AUTHOR("wangdong,wangyuxiang, <wangdong@agenewtech.com>,<wangyuxiang@agenewtech.com>");
MODULE_DESCRIPTION
    ("GC9107, TFT_LCD Display Module, 240X240 , 4-wire I SPI, 8bit 1-data-line");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("spi:GC9107");
MODULE_VERSION("v0.2");
