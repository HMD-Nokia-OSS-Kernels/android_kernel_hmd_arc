/*
 *  <lcd_gc9305_spi.c> - <lcd g9305 spi>
 *
 *  Copyright (C) 2019 Unisoc Communications Inc.
 *  History:
 *      <2023-07-11> <pony.wu@unisoc.com>
 *
 *      The above copyright notice shall be
 *      included in all copies or substantial portions of the Software.
 *
 *      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *      EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *      MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *      IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 *      CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 *      TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *      SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#include "../sprdfb.h"
#include "../sprdfb_spi_panel.h"
#include "bmp_layout.h"
#include "logo.h"
#include "gpio_plus.h"

#define REG32(x)              (*((volatile uint32 *)(x)))
#define GC9305_SpiWriteCmd(cmd) \
{ \
	spi_send_cmd((cmd & 0xFF));\
}

#define  GC9305_SpiWriteData(data)\
{ \
	spi_send_data((data & 0xFF));\
}

#define  GC9305_SpiRead(data,len)\
{ \
	spi_read(data,len);\
}
#define BGRA32toRBG565(b,g,r)		((((r>>3)&0x1f)<<11)|(((g>>2)&0x3f)<<5)|(((b>>3)&0x1f)<<0))
#define LCM_GPIO_RSTN			(82)
#define LCM_GPIO_DC			(92)
#define GC9305_SPI_SPEED 		(48*1000*1000UL)
#define	SPI_MODE_0	(0|0)

extern unsigned char start_send_pixels_flag;

static int32_t gc9305_refresh(struct panel_spec *self,void *base)
{
	int i = 0;
	//int j = 0;
	//uint16_t *prgb = (uint16_t *)base;
	uint16_t *prgb = (uint16_t *)unisoc_WQVGA_240_320_16bit;
	uint16_t rgb;
	//uint16_t rgb_arr[240][320];
	//uint16_t *bmp_addr;
	//uint32_t data_offset = 0;
	//struct bmp_header *header = (struct bmp_header*)BMP_RESERVED_ADDR;
	printf("gc9305_freshbuffer\r\n");
	start_send_pixels_flag = 1;
	spi_send_cmd_t spi_send_cmd = self->spi->ops->spi_send_cmd;
	spi_send_data_t spi_send_data = self->spi->ops->spi_send_data;
	spi_read_t spi_read = self->spi->ops->spi_read;

	//data_offset = header->size;
	//bmp_addr = (uint16_t *)(BMP_RESERVED_ADDR + data_offset);
	/*
	for (i = 0; i < 320 ; i++) {
		for (j = 0; j < 240; j++) {
			rgb_arr[i][j] = *prgb++;
		}
	}

	sprd_gpio_set(LCM_GPIO_DC, 1);	//enable D/C PIN
	for (i = 319; i >= 0 ; i--) {
		for (j = 0; j < 240; j++) {
			rgb = rgb_arr[i][j];
			GC9305_SpiWriteData(rgb >> 8);
			GC9305_SpiWriteData(rgb & 0xff);
		}
	}
	*/
	sprd_gpio_set(LCM_GPIO_DC, 1);	//enable D/C PIN
	for(i = 0 ; i < (240 * 320); i++)
	{
		rgb = *prgb++;
		//rgb = *bmp_addr++;
		GC9305_SpiWriteData(rgb >> 8);
		GC9305_SpiWriteData(rgb & 0xff);
	}
	GC9305_SpiWriteCmd(0x29);

	return 0;
}
static int32_t gc9305_reset(void)
{
	static int32_t is_first_run = 1;
	if(is_first_run)
	{
		sprd_gpio_request(LCM_GPIO_RSTN);
		sprd_gpio_direction_output(LCM_GPIO_RSTN,1);
		is_first_run = 0;
	}
	sprd_gpio_set(LCM_GPIO_RSTN,1);
	mdelay(10);
	sprd_gpio_set(LCM_GPIO_RSTN,0);
	mdelay(10);
	sprd_gpio_set(LCM_GPIO_RSTN,1);
	mdelay(20);

	return 0;
}

static int32_t gc9305_init(struct panel_spec *self)
{
	uint32_t data = 0;
	spi_send_cmd_t spi_send_cmd = self->spi->ops->spi_send_cmd;
	spi_send_data_t spi_send_data = self->spi->ops->spi_send_data;
	spi_read_t spi_read = self->spi->ops->spi_read;

	gc9305_reset();

	GC9305_SpiWriteCmd(0xfe);
	GC9305_SpiWriteCmd(0xef);

	GC9305_SpiWriteCmd(0x35);
	GC9305_SpiWriteData(0x00);

	GC9305_SpiWriteCmd(0x36);
	GC9305_SpiWriteData(0x48);

	GC9305_SpiWriteCmd(0x3a);
	GC9305_SpiWriteData(0x05);

	GC9305_SpiWriteCmd(0xa4);
	GC9305_SpiWriteData(0x44);
	GC9305_SpiWriteData(0x44);

	GC9305_SpiWriteCmd(0xa5);
	GC9305_SpiWriteData(0x42);
	GC9305_SpiWriteData(0x42);

	GC9305_SpiWriteCmd(0xaa);
	GC9305_SpiWriteData(0x88);
	GC9305_SpiWriteData(0x88);

	GC9305_SpiWriteCmd(0xe8);
	GC9305_SpiWriteData(0x11);
	GC9305_SpiWriteData(0x77);

	GC9305_SpiWriteCmd(0xe3);
	GC9305_SpiWriteData(0x01);
	GC9305_SpiWriteData(0x18);

	GC9305_SpiWriteCmd(0xe1);
	GC9305_SpiWriteData(0x10);
	GC9305_SpiWriteData(0x0a);

	GC9305_SpiWriteCmd(0xAC);
	GC9305_SpiWriteData(0x00);

	GC9305_SpiWriteCmd(0xAf);
	GC9305_SpiWriteData(0x67);

	GC9305_SpiWriteCmd(0xa6);
	GC9305_SpiWriteData(0x29);
	GC9305_SpiWriteData(0x29);

	GC9305_SpiWriteCmd(0xa7);
	GC9305_SpiWriteData(0x27);
	GC9305_SpiWriteData(0x27);

	GC9305_SpiWriteCmd(0xa8);
	GC9305_SpiWriteData(0x17);
	GC9305_SpiWriteData(0x17);

	GC9305_SpiWriteCmd(0xa9);
	GC9305_SpiWriteData(0x26);
	GC9305_SpiWriteData(0x26);

	//----gamma setting---------//
	GC9305_SpiWriteCmd(0xf0);
	GC9305_SpiWriteData(0x02);
	GC9305_SpiWriteData(0x02);
	GC9305_SpiWriteData(0x00);
	GC9305_SpiWriteData(0x02);
	GC9305_SpiWriteData(0x07);
	GC9305_SpiWriteData(0x0c);

	GC9305_SpiWriteCmd(0xf1);
	GC9305_SpiWriteData(0x01);
	GC9305_SpiWriteData(0x01);
	GC9305_SpiWriteData(0x00);
	GC9305_SpiWriteData(0x03);
	GC9305_SpiWriteData(0x07);
	GC9305_SpiWriteData(0x0f);

	GC9305_SpiWriteCmd(0xf2);
	GC9305_SpiWriteData(0x0D);
	GC9305_SpiWriteData(0x08);
	GC9305_SpiWriteData(0x37);
	GC9305_SpiWriteData(0x04);
	GC9305_SpiWriteData(0x04);
	GC9305_SpiWriteData(0x4b);

	GC9305_SpiWriteCmd(0xf3);
	GC9305_SpiWriteData(0x11);
	GC9305_SpiWriteData(0x0c);
	GC9305_SpiWriteData(0x37);
	GC9305_SpiWriteData(0x04);
	GC9305_SpiWriteData(0x04);
	GC9305_SpiWriteData(0x47);

	GC9305_SpiWriteCmd(0xf4);
	GC9305_SpiWriteData(0x0a);
	GC9305_SpiWriteData(0x15);
	GC9305_SpiWriteData(0x15);
	GC9305_SpiWriteData(0x24);
	GC9305_SpiWriteData(0x3a);
	GC9305_SpiWriteData(0x0F);

	GC9305_SpiWriteCmd(0xf5);
	GC9305_SpiWriteData(0x07);
	GC9305_SpiWriteData(0x0f);
	GC9305_SpiWriteData(0x0d);
	GC9305_SpiWriteData(0x17);
	GC9305_SpiWriteData(0x3a);
	GC9305_SpiWriteData(0x0F);
	//---end gamma setting-----//

	GC9305_SpiWriteCmd(0x11);
	mdelay(120);
//	GC9305_SpiWriteCmd(0x29);
//	GC9305_SpiWriteCmd(0x2c);

//	GC9305_SpiWriteCmd(0x29);
//	GC9305_SpiWriteCmd(0x2c);

	printf("gc9305_init\n");

	return 0;
}

static int32_t gc9305_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	spi_send_cmd_t spi_send_cmd = self->spi->ops->spi_send_cmd;
	spi_send_data_t spi_send_data = self->spi->ops->spi_send_data;
	spi_read_t spi_read = self->spi->ops->spi_read;

	if (is_sleep == 1) {
		//Sleep In
		GC9305_SpiWriteCmd(0x28);
		mdelay(120);
		GC9305_SpiWriteCmd(0x10);
		mdelay(10);
	} else {
		//Sleep Out
		GC9305_SpiWriteCmd(0x11);
		mdelay(120);
		GC9305_SpiWriteCmd(0x29);
		mdelay(10);
	}
	return 0;
}



static int32_t gc9305_set_window(struct panel_spec *self,
		uint16_t left, uint16_t top, uint16_t right, uint16_t bottom)
{
	uint32_t *test_data[4] = {0};
	spi_send_cmd_t spi_send_cmd = self->spi->ops->spi_send_cmd;
	spi_send_data_t spi_send_data = self->spi->ops->spi_send_data;
	spi_read_t spi_read = self->spi->ops->spi_read;


	GC9305_SpiWriteCmd(0x2A);
	GC9305_SpiWriteData((left >> 8));// set left address
	GC9305_SpiWriteData((left & 0xff));
	GC9305_SpiWriteData((right >> 8));// set right address
	GC9305_SpiWriteData((right & 0xff));

	GC9305_SpiWriteCmd(0x2B);
	GC9305_SpiWriteData((top >> 8));// set top address
	GC9305_SpiWriteData((top & 0xff));
	GC9305_SpiWriteData((bottom >> 8));// set bottom address
	GC9305_SpiWriteData((bottom & 0xff));
	GC9305_SpiWriteCmd(0x2C);

	return 0;
}
static int32_t gc9305_invalidate(struct panel_spec *self)
{
	printf("gc9305_invalidate\n");

	return self->ops->panel_set_window(self, 0, 0,
		self->width - 1, self->height - 1);
}

static int32_t gc9305_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{
	printf("gc9305_invalidate_rect \n");

	return self->ops->panel_set_window(self, left, top,
			right, bottom);
}
static int32_t gc9305_read_id(struct panel_spec *self)
{
	spi_send_cmd_t spi_send_cmd = self->spi->ops->spi_send_cmd;
	spi_send_data_t spi_send_data = self->spi->ops->spi_send_data;
	spi_read_t spi_read = self->spi->ops->spi_read;
	spi_clock_set_t spi_clock_set = self->spi->ops->spi_clock_set;
	//uint32_t i = 0;
	uint32_t lcm_id[4] = {0};

	spi_clock_set(6000000);
	gc9305_reset();
	GC9305_SpiWriteCmd(0xFE);
	GC9305_SpiWriteCmd(0xEF);
	GC9305_SpiWriteCmd(0x04);
	GC9305_SpiRead(lcm_id,4);
	printf("sprdfb:gc9305_read_id lcm id[0-3] = 0x%x %x %x %x\n",lcm_id[0],lcm_id[1],lcm_id[2],lcm_id[3]);
	if(lcm_id[0] == 0xff059300)
	{
			spi_clock_set(24000000);
			return 0x9305;
	}
	return 0x00;

	/*
	spi_clock_set_t spi_clock_set = self->spi->ops->spi_clock_set;
	spi_clock_set(24000000);
	return 0x9305;
	*/
}

static struct panel_operations lcd_gc9305_spi_operations = {
	.panel_init = gc9305_init,
	.panel_set_window = gc9305_set_window,
	.panel_invalidate_rect= gc9305_invalidate_rect,
	.panel_invalidate = gc9305_invalidate,
	.panel_enter_sleep = gc9305_enter_sleep,
	.panel_readid = gc9305_read_id,
	.panel_refresh = gc9305_refresh,
	//.panel_reset = gc9305_reset,
};

static struct info_spi lcd_gc9305_spi_info = {
	.bus_num = 0,
	.cs = 0,
	.cd_gpio = 92,//spi0 DI
	.spi_mode = 1,
	.spi_pol_mode = SPI_MODE_0,
	.speed = GC9305_SPI_SPEED,
	.spi_panel_name = "lcd_gc9305_spi_qvga",
};

struct panel_spec lcd_gc9305_spi_spec = {
	.width = 240,
	.height = 320,
	.fps = 33,
	.type = SPRDFB_PANEL_TYPE_SPI,
	.direction = LCD_DIRECT_NORMAL,
	.spi = &lcd_gc9305_spi_info,
	.ops = &lcd_gc9305_spi_operations,
};
