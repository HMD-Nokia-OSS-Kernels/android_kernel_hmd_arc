/*
 *  <sprdfb_spi.h> - <sprd spi>
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

#ifndef _SPRDFB_SPI_H_
#define _SPRDFB_SPI_H_

#include <linux/types.h>
#include "sprdfb.h"

#define SPI_TX_FIFO_DEPTH 		16
#define SPI_RX_FIFO_DEPTH 		16

typedef enum
{
	TX_POS_EDGE = 0,
	TX_NEG_EDGE
}TX_EDGE;

typedef enum
{
	RX_POS_EDGE = 0,
	RX_NEG_EDGE
}RX_EDGE;

typedef enum
{
	TX_RX_MSB = 0,
	TX_RX_LSB
}MSB_LSB_SEL;

typedef enum
{
	IDLE_MODE = 0,
	RX_MODE,
	TX_MODE,
	RX_TX_MODE
}TRANCIEVE_MODE;

typedef enum
{
	NO_SWITCH    = 0,
	BYTE_SWITCH  = 1,
	HWORD_SWITCH = 2
}SWT_MODE;

typedef enum
{
	MASTER_MODE = 0,
	SLAVE_MODE = 1
}SPI_OPERATE_MODE_E;

typedef struct _init_param
{
	TX_EDGE tx_edge;
	RX_EDGE rx_edge;
	MSB_LSB_SEL msb_lsb_sel;
	TRANCIEVE_MODE tx_rx_mode;
	SWT_MODE switch_mode;
	SPI_OPERATE_MODE_E op_mode;
	uint32_t DMAsrcSize;
	uint32_t DMAdesSize;
	uint32_t clk_div;
	uint8_t data_width;
	uint8_t tx_empty_watermark;
	uint8_t tx_full_watermark;
	uint8_t rx_empty_watermark;
	uint8_t rx_full_watermark;
}SPI_INIT_PARM,*SPI_INIT_PARM_P;

void spi_lcd_disable(void);
void spi_lcd_refresh(void *base);
void spi_lcd_enable(void);
int32_t sprdfb_spi_refresh(struct sprdfb_device *dev);
int32_t sprdfb_spi_flip(void *base);
int sprdfb_spi_probe(void);

#endif
