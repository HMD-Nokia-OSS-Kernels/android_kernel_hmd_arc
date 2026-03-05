#ifndef __SPI_TINY_LCD_GC9A01_H__
#define __SPI_TINY_LCD_GC9A01_H__

#include <linux/cdev.h>

//default spi speed
#define MAX_SPI_SPEED 8000000
#define AGN_TFT_LCD_GC9A01

#define LCD_SPI_NAME "GC9A01"
#define LOG_INF(fmt, args...)    pr_info("[%s] %s %d: " fmt, LCD_SPI_NAME, __func__, __LINE__, ##args)
#define LOG_ERR(fmt, args...)    pr_err("[%s] %s %d: " fmt, LCD_SPI_NAME, __func__, __LINE__, ##args)

#define BUFSIZE 1024

struct stl_data {
	u8 		*tx_buffer;
	u8 		*rx_buffer;
	struct spi_device *spi;
	u32			speed_hz;
	uint32_t spi_max_frequency;
	int      reset_gpio;
	int      rs_ctrl_gpio;
	spinlock_t		spi_lock;
	unsigned		users;
	struct mutex		buf_lock;
	struct list_head	device_entry;
	
	//cdev
	struct   cdev	*tiny_lcd_cdev;
	dev_t  dev_no;
	
	//class
	struct   class  *tiny_lcd_class;
	struct   device *tiny_lcd_device;
	
};

struct pic_info{
	 u8 pic_data[115201];
	 u8 x_start;
	 u8 x_end;
	 u8 y_start;
	 u8 y_end; 
};

static ssize_t spidev_sync(struct stl_data *spidev, struct spi_message *message);

#endif //__SPI_TINY_LCD_GC9107_H__ 