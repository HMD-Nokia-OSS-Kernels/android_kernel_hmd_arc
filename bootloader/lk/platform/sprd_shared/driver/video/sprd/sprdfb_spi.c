/*
 *  <sprdfb_spi.c> - <sprd spi>
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

#include "sprdfb.h"
#include "sprdfb_spi_panel.h"
#include "sprd_spi.h"
#include "gpio_plus.h"

#define REG32(x)	(*((volatile uint32_t *)(x)))
#define LCM_GPIO_RS	LCM_GPIO_RSTN
#define LCM_GPIO_DC	(92)

unsigned char start_send_pixels_flag = 0;

struct spi_init_param spi_int_parm[] =
{
	{
		sck_reverse,
		tx_pos_edge,
		rx_pos_edge,
		tx_rx_msb,
		//tx_rx_lsb,
		rx_tx_mode,
		master_mode,
		8,
		24000000
	 },  //for spi_lcm test
	//{TX_POS_EDGE,RX_NEG_EDGE,TX_RX_LSB,RX_TX_MODE,NO_SWITCH,SLAVE_MODE,0x0,0x0,0xF0,0x0,0x0,SPI_TX_FIFO_DEPTH - 1,0x0,SPI_RX_FIFO_DEPTH - 1},
 };

static void DISPC_SpiWriteCmd(uint32_t cmd)
{
	sprd_spi_set_data_width(8);
	sprd_spi_set_cs(0, true);
	sprd_gpio_set(LCM_GPIO_DC, 0);
	// Write a data identical with buswidth
	sprd_spi_write_data(&cmd, 1, 0);

	sprd_spi_set_cs(0, false);
}

static void DISPC_SpiWriteData(uint32_t data)
{
	sprd_spi_set_data_width(8);

	sprd_spi_set_cs(0, true);

	if (!start_send_pixels_flag)
		sprd_gpio_set(LCM_GPIO_DC, 1);

	// Write a data identical with buswidth
	sprd_spi_write_data(&data, 1, 0);

	sprd_spi_set_cs(0, false);
}

static void SPI_Read( uint32_t* data,uint32_t len)
{
	sprd_spi_set_cs(0, true);
	sprd_gpio_set(LCM_GPIO_DC, 1);
	sprd_spi_set_data_width(8);

	sprd_spi_read_data(data, len, 2);  //unit of buswidth
	sprd_spi_set_cs(0, false);
}

bool sprdfb_sprd_spi_init(struct sprdfb_device *dev)
{
	unsigned int reg_val;

	sprd_gpio_request(LCM_GPIO_DC);
	sprd_gpio_direction_output(LCM_GPIO_DC, 1);

	sprd_spi_enable(0);
	sprd_spi_clk_set(0,3,0);
	sprd_spi_init(spi_int_parm);
	sprd_spi_set_spi_mode(SPIMODE_4WIRE_8BIT_SDA);

	return true;
}

bool sprdfb_spi_uninit(struct sprdfb_device *dev)
{
	return true;
}

void spi_clock_set(unsigned int speed)
{
	sprd_spi_clk_div(speed);
}

struct ops_spi sprdfb_spi_ops = {
	.spi_send_cmd = DISPC_SpiWriteCmd,
	.spi_send_data = DISPC_SpiWriteData,
	.spi_read = SPI_Read,
	.spi_clock_set = spi_clock_set,
};


static int32_t sprdfb_spi_panel_check(struct panel_spec *panel)
{
	if(NULL == panel){
		printf("sprdfb: sprdfb_spi_panel_check fail. (Invalid param)\n");
		return 0;
	}

	if(SPRDFB_PANEL_TYPE_SPI != panel->type){
		printf("sprdfb: sprdfb_spi_panel_check fail. (not spi param)\n");
		return 0;
	}

	printf("sprdfb: sprdfb_spi_panel_check\n");

	return 1;
}

static void sprdfb_spi_panel_mount(struct sprdfb_device *dev)
{
	if((NULL == dev) || (NULL == dev->panel)){
		printf("sprdfb: [%s]: Invalid Param\n", __FUNCTION__);
		return;
	}
	dev->panel_if_type = SPRDFB_PANEL_IF_SPI;
	if (dev->panel->spi)
	{
		printf("sprdfb:sprdfb_spi_panel_mount sprdfb_spi_ops\n");
		dev->panel->spi->ops = &sprdfb_spi_ops;
	}
}

static void sprdfb_spi_panel_init(struct sprdfb_device *dev)
{
	int ret = false;
	printf("sprdfb: [%s]\n",__FUNCTION__);
	ret = sprdfb_sprd_spi_init(dev);
	if(!ret)
	{
		printf("sprdfb: [%s]: bus init fail!\n", __FUNCTION__);
		return ;
	}
}

static void sprdfb_spi_panel_uninit(struct sprdfb_device *dev)
{
	int ret=false;
	printf("sprdfb: [%s]\n",__FUNCTION__);
	ret=sprdfb_spi_uninit(dev);
	if(!ret)
	{
		printf("sprdfb: [%s]: init fail!\n", __FUNCTION__);
		return;
	}
}

struct panel_if_ctrl sprdfb_spi_ctrl = {
	.if_name		= "spi",
	.panel_if_check	= sprdfb_spi_panel_check,
	.panel_if_mount	= sprdfb_spi_panel_mount,
	.panel_if_init		= sprdfb_spi_panel_init,
	.panel_if_before_refresh	= NULL,
	.panel_if_after_refresh	= NULL,
};


