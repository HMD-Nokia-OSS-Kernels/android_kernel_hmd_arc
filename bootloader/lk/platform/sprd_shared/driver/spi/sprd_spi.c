/******************************************************************************
 ** File Name:    sprd_spi.c                                        *
 ** Author:       mengkai.zhao                                      *
 ** DATE:         2017-08-01                                        *
 ** Copyright:    2017 Spreatrum, Incoporated. All Rights Reserved. *
 ** Description:                                                    *
 ******************************************************************************/
/******************************************************************************
 **                   Edit    History                               *
 **---------------------------------------------------------------------------*
 **    DATE         NAME            DESCRIPTION                     *
 ** 2017-08-01   mengkai.zhao       First Draft
 ******************************************************************************/

#include <sprd_common.h>
#include <asm/arch/sprd_reg.h>
//#include <asm/io.h>
#include "sprd_spi.h"

#define SET_BIT(x,bit) (x |= (1 << bit))
#define SET_BIT0(x,bit) (x &= ~(1 << bit))

struct spi_base_addr {
	unsigned long spi_eb;
	unsigned long apb_base_eb;
	unsigned long spi_rst;
	unsigned long apb_base_rst;
	unsigned long spi_clk_base;
	unsigned long spi_base;
};

struct spi_base_addr spi_apb_bit[] = {
#if defined(CONFIG_PIKE2)
	{BIT_AP_APB_SPI0_EB, REG_AP_APB_APB_EB, BIT_AP_APB_SPI0_SOFT_RST, REG_AP_APB_APB_RST, \
		REG_AP_CLK_SPI0_CFG, SPRD_SPI0_PHYS},
#else
    {BIT_AP_APB_SPI0_EB, REG_AP_APB_APB_EB, BIT_AP_APB_SPI0_SOFT_RST, REG_AP_APB_APB_RST, \
		REG_AP_CLK_CORE_CGM_SPI0_CFG, SPRD_SPI0_PHYS},
	{BIT_AON_APB_AP_HS_SPI_EB, REG_AON_APB_CLK_EB0, BIT_AP_APB_SPI1_SOFT_RST, REG_AP_APB_APB_RST, \
		REG_AP_CLK_CORE_CGM_SPI1_CFG, SPRD_SPI1_PHYS},
	{BIT_AP_APB_SPI2_EB, REG_AP_APB_APB_EB, BIT_AP_APB_SPI2_SOFT_RST, REG_AP_APB_APB_RST, \
		REG_AP_CLK_CORE_CGM_SPI2_CFG, SPRD_SPI2_PHYS},
#endif
};

#define spi_max_id		(sizeof(spi_apb_bit)/sizeof(spi_apb_bit[0]))
#define spi_trans_timeout	(-1)

static unsigned int spi_use_id = 0;
static unsigned int spi_clk_src = 26000000;
static unsigned int bit_per_word = 8;

static unsigned long sprd_get_spi_base(void)
{
	return spi_apb_bit[spi_use_id].spi_base;
}

static void sprd_spi_dump_regs(unsigned long spi_base)
{
	dprintf(INFO,"SPI_CLKD:0x%x\n", readl(spi_base + SPI_CLKD));
	dprintf(INFO,"SPI_CTL0:0x%x\n", readl(spi_base + SPI_CTL0));
	dprintf(INFO,"SPI_CTL1:0x%x\n", readl(spi_base + SPI_CTL1));
	dprintf(INFO,"SPI_CTL2:0x%x\n", readl(spi_base + SPI_CTL2));
	dprintf(INFO,"SPI_CTL3:0x%x\n", readl(spi_base + SPI_CTL3));
	dprintf(INFO,"SPI_CTL4:0x%x\n", readl(spi_base + SPI_CTL4));
	dprintf(INFO,"SPI_CTL5:0x%x\n", readl(spi_base + SPI_CTL5));
	dprintf(INFO,"SPI_INT_RAW_STS:0x%x\n", readl(spi_base + SPI_INT_RAW_STS));
	dprintf(INFO,"SPI_INT_EN:0x%x\n", readl(spi_base + SPI_INT_EN));
	dprintf(INFO,"SPI_STS2:0x%x\n", readl(spi_base + SPI_STS2));
	dprintf(INFO,"SPI_DSP_WAIT:0x%x\n", readl(spi_base + SPI_DSP_WAIT));
	dprintf(INFO,"SPI_CTL6:0x%x\n", readl(spi_base + SPI_CTL6));
	dprintf(INFO,"SPI_CTL7:0x%x\n", readl(spi_base + SPI_CTL7));
	dprintf(INFO,"SPI_CTL8:0x%x\n", readl(spi_base + SPI_CTL8));
	dprintf(INFO,"SPI_CTL9:0x%x\n", readl(spi_base + SPI_CTL9));
	dprintf(INFO,"SPI_CTL10:0x%x\n", readl(spi_base + SPI_CTL10));
	dprintf(INFO,"SPI_CTL11:0x%x\n", readl(spi_base + SPI_CTL11));
	dprintf(INFO,"SPI_STS6:0x%x\n", readl(spi_base + SPI_STS6));
	dprintf(INFO,"SPI_STS7:0x%x\n", readl(spi_base + SPI_STS7));
	dprintf(INFO,"SPI_STS8:0x%x\n", readl(spi_base + SPI_STS8));
	dprintf(INFO,"SPI_STS9:0x%x\n", readl(spi_base + SPI_STS9));
}

void sprd_spi_enable(unsigned int spi_id)
{
	unsigned int reg_val = 0;

	if (spi_max_id <= spi_id) {
		errorf("spi enable error, spi_id %u is out of range\n", spi_id);
		return;
	}

	spi_use_id = spi_id;
	reg_val = readl(spi_apb_bit[spi_id].apb_base_eb);
	reg_val |= spi_apb_bit[spi_id].spi_eb;
	writel(reg_val, spi_apb_bit[spi_id].apb_base_eb);
}

void sprd_spi_disable(unsigned int spi_id)
{
	unsigned int reg_val = 0;

	if (spi_max_id <= spi_id)
		return;

	reg_val = readl(spi_apb_bit[spi_id].apb_base_eb);
	reg_val &= ~(spi_apb_bit[spi_id].spi_eb);
	writel(reg_val, spi_apb_bit[spi_id].apb_base_eb);
}

void sprd_spi_reset(unsigned int spi_id)
{
	unsigned int reg_val = 0;

	if (spi_max_id <= spi_id)
		return;

	reg_val = readl(spi_apb_bit[spi_id].apb_base_rst);
	reg_val |= spi_apb_bit[spi_id].spi_rst;

	writel(reg_val, spi_apb_bit[spi_id].apb_base_rst);
	udelay(10);
	reg_val &= ~(spi_apb_bit[spi_id].spi_rst);
	writel(reg_val, spi_apb_bit[spi_id].apb_base_rst);
}

void sprd_spi_clk_set(unsigned int spi_id, unsigned int clk_src, unsigned int clk_div)
{
	unsigned int reg_val = 0;
	unsigned int clk = clk_src & 0x3;
	unsigned int div = clk_div & 0x7;
	if (spi_max_id <= spi_id)
		return;

	reg_val = clk | (div << 8);
	writel(reg_val, spi_apb_bit[spi_id].spi_clk_base);

	switch (clk) {
	case SPICLK_SEL_26M :
		spi_clk_src = 26000000 / (div + 1);
		break;
	case SPICLK_SEL_128M :
		spi_clk_src = 128000000 / (div + 1);
		break;
	case SPICLK_SEL_154M:
		spi_clk_src = 153600000 / (div + 1);
		break;
	case SPICLK_SEL_192M:
		spi_clk_src = 192000000 / (div + 1);
		break;
	}
}

void sprd_spi_set_cs(unsigned int spi_sel_csx, unsigned int is_low)
{
	unsigned long spi_base = sprd_get_spi_base();
	unsigned int reg_val = readl((spi_base + SPI_CTL0));

	if (is_low)
		reg_val &= ~((1<<spi_sel_csx)<<SPI_SEL_CS_SHIFT);
	else
		reg_val |= ((1<<spi_sel_csx)<<SPI_SEL_CS_SHIFT);

	writel(reg_val, (spi_base + SPI_CTL0));
}

void sprd_spi_set_cd(unsigned int cd)
{
	unsigned long spi_base = sprd_get_spi_base();
	unsigned int reg_val = readl((spi_base + SPI_CTL8));

	if (0 == cd)
		reg_val &= ~(SPI_CD_MASK);
	else
		reg_val |= (SPI_CD_MASK);

	writel(reg_val, (spi_base + SPI_CTL8));
}

void sprd_spi_set_spi_mode(unsigned int spi_mode)
{
	unsigned long spi_base = sprd_get_spi_base();
	unsigned int reg_val = readl((spi_base + SPI_CTL7));

	reg_val &= ~SPI_MODE_MASK;
	reg_val |= ((spi_mode & 0x7) << SPI_MODE_SHIFT);
	writel(reg_val, (spi_base + SPI_CTL7));
}

void sprd_spi_set_data_width(unsigned int data_width)
{
	unsigned long spi_base = sprd_get_spi_base();
	unsigned int reg_val = readl((spi_base + SPI_CTL0));

	reg_val &= ~0x7C;
	if (32 != data_width)
		reg_val |= ((data_width & 0x1F) << 2);

	writel(reg_val, (spi_base + SPI_CTL0));
}

static void sprd_spi_set_tx_length(unsigned int data_len, unsigned int dummy_bit_len)
{
	unsigned long spi_base = sprd_get_spi_base();
	unsigned int reg_val = readl((spi_base + SPI_CTL8));

	data_len &= TX_MAX_LEN_MASK;
	dummy_bit_len &= TX_DUMY_LEN_MASK;

	reg_val &= ~((TX_DUMY_LEN_MASK << 4) | TX_DATA_LEN_H_MASK);
	reg_val |= ((dummy_bit_len << 4) | (data_len >> 16));
	writel(reg_val, (spi_base + SPI_CTL8));
	writel((data_len & TX_DATA_LEN_L_MASK), (spi_base + SPI_CTL9));

	if (0x18 == (readl(spi_base + SPI_CTL7) & (0x7 << 3))) {
		reg_val = readl(spi_base + SPI_CTL10);
		reg_val &= ~(0x3ff);
		writel(reg_val, (spi_base + SPI_CTL10));
		writel(0x0, (spi_base + SPI_CTL11));
	}
}

static void sprd_spi_set_rx_length(unsigned int data_len, unsigned int dummy_bit_len)
{
	unsigned long spi_base = sprd_get_spi_base();
	unsigned int reg_val = readl((spi_base + SPI_CTL10));

	data_len &= RX_MAX_LEN_MASK;
	dummy_bit_len &= RX_DUMY_LEN_MASK;

	reg_val &= ~((RX_DUMY_LEN_MASK << 4) | RX_DATA_LEN_H_MASK);
	reg_val |= ((dummy_bit_len << 4) | (data_len >> 16));
	writel(reg_val, (spi_base + SPI_CTL10));
	writel((data_len & RX_DATA_LEN_L_MASK), (spi_base + SPI_CTL11));

	if (0x18 == (readl(spi_base + SPI_CTL7) & (0x7 << 3))) {
		reg_val = readl(spi_base + SPI_CTL8);
		reg_val &= ~(0x3ff);
		writel(reg_val, (spi_base + SPI_CTL8));
		writel(0x0, (spi_base + SPI_CTL9));
	}
}

static void sprd_spi_tx_req(void)
{
	unsigned long spi_base = sprd_get_spi_base();

	writel(SW_TX_REQ_MASK, (spi_base + SPI_CTL12));
}

static void sprd_spi_rx_req(void)
{
	unsigned long spi_base = sprd_get_spi_base();

	writel(SW_RX_REQ_MASK, (spi_base + SPI_CTL12));
}

static int sprd_spi_wait_tx_finish(void)
{
	unsigned long spi_base = sprd_get_spi_base();
	unsigned int reg_val = 0;
	unsigned int timeout = 0;

#if 0
	while (!(readl(spi_base + SPI_INT_RAW_STS) & SPI_TX_END_RAW_STS)) {
		if (++timeout > SPI_TIME_OUT) {
			errorf("spi wait tx error, spi tx end timeout\n");
			sprd_spi_dump_regs(spi_base);
			return spi_trans_timeout;
		}
	}
	writel(SPI_TX_END_CLR, (spi_base + SPI_INT_CLR));
#endif
	reg_val = readl(spi_base + SPI_STS2);
	while (!(reg_val & SPI_TX_FIFO_REALLY_EMPTY)) {
		if (++timeout > SPI_TIME_OUT) {
			errorf("spi wait tx error, spi fifo really empty timeout\n");
			sprd_spi_dump_regs(spi_base);
			return spi_trans_timeout;
		}
		reg_val = readl(spi_base + SPI_STS2);
	}

	reg_val = readl(spi_base + SPI_STS2);
	while ((reg_val & SPI_BUSY)) {
		if (++timeout > SPI_TIME_OUT) {
			errorf("spi wait tx error, spi busy timeout\n");
			sprd_spi_dump_regs(spi_base);
			return spi_trans_timeout;
		}
		reg_val = readl(spi_base + SPI_STS2);
	}

	return 0;
}

static int sprd_spi_wait_rx_finish(void)
{
	unsigned long spi_base = sprd_get_spi_base();
	unsigned int timeout = 0;

	while (!(readl(spi_base + SPI_INT_RAW_STS) & SPI_RX_END_RAW_STS)) {
		if (++timeout > SPI_TIME_OUT) {
			errorf("spi wait rx error, spi rx end timeout\n");
			sprd_spi_dump_regs(spi_base);
			return spi_trans_timeout;
		}
	}
	writel(SPI_RX_END_CLR, (spi_base + SPI_INT_CLR));

	while(readl(spi_base + SPI_STS2) & SPI_BUSY) {
		if (++timeout > SPI_TIME_OUT) {
			errorf("spi wait rx error, spi busy timeout\n");
			sprd_spi_dump_regs(spi_base);
			return spi_trans_timeout;
		}
	}

	return 0;
}

static void sprd_spi_fifo_rst(void)
{
	unsigned long spi_base = sprd_get_spi_base();

	writel(0x1, (spi_base + SPI_FIFO_RST));
	writel(0x0, (spi_base + SPI_FIFO_RST));
}

void sprd_spi_clk_div(unsigned int speed)
{
	unsigned long spi_base = sprd_get_spi_base();
	unsigned int clk_div;

	clk_div = spi_clk_src/(speed << 1) - 1;
	writel(clk_div, (spi_base + SPI_CLKD));
}

void sprd_spi_init(struct spi_init_param *spi_param)
{
	unsigned long spi_base = sprd_get_spi_base();
	unsigned int clk_div;
	unsigned int reg_val;

	clk_div = spi_clk_src/(spi_param->max_speed << 1) - 1;
	writel(clk_div, (spi_base + SPI_CLKD));

#ifdef CONFIG_SPI_SLAVER_PANEL
	spi_param->data_width = 8;
#endif

	reg_val = (spi_param->sck_sel << 13) | (0xF << 8) | (spi_param->msb_lsb_sel << 7) |
			((spi_param->data_width & 0x1F) << 2) | (spi_param->tx_edge << 1) |
				 (spi_param->rx_edge);
	bit_per_word = spi_param->data_width;

	writel(reg_val, (spi_base + SPI_CTL0));
	writel((spi_param->tx_rx_mode << 12), (spi_base + SPI_CTL1));
	writel((spi_param->op_mode << 5), (spi_base + SPI_CTL2));
	writel((RX_EMPTY_THLD_DEFAULT_VALUE << RX_EMPTY_THLD_SHIFT) | RX_EMPTY_THLD_SHIFT, (spi_base + SPI_CTL3));
	writel(0x0, (spi_base + SPI_CTL4));
	writel(0x9, (spi_base + SPI_CTL5));
	writel(0x0, (spi_base + SPI_INT_EN));
	writel(SPI_ALL_INT_CLR, (spi_base + SPI_INT_CLR));
	writel((TX_EMPTY_THLD_DEFAULT_VALUE << RX_EMPTY_THLD_SHIFT) | RX_EMPTY_THLD_SHIFT, (spi_base + SPI_CTL6));
	sprd_spi_fifo_rst();
}

static unsigned int sprd_get_spi_data_mask(void)
{
	unsigned long spi_base = sprd_get_spi_base();
	unsigned int data_width = 0;
	unsigned int data_mask = 0xFFFFFFFF;

	data_width = (readl((spi_base + SPI_CTL0)) >> 2) & 0x1F;
	if (0 != data_width)
		data_mask = data_mask >> (32 - data_width);

	return data_mask;
}

static unsigned int sprd_spi_write_bufs_u8(void *tx_buf, unsigned int num)
{
	unsigned int i;
	unsigned long spi_base = sprd_get_spi_base();
	uint8_t *tx_p = (uint8_t  *)tx_buf;

	for (i = 0; i < num; i++, tx_p++)
		writeb(*tx_p, (spi_base + SPI_TXD));

	return num;
}

static unsigned int sprd_spi_write_bufs_u16(void *tx_buf, unsigned int num)
{
	unsigned int i;
	unsigned long spi_base = sprd_get_spi_base();
	uint16_t *tx_p = (uint16_t  *)tx_buf;

	for (i = 0; i < num; i++, tx_p++)
		writew(*tx_p, (spi_base + SPI_TXD));

	return (num << 1);
}

static unsigned int sprd_spi_write_bufs_u32(void *tx_buf, unsigned int num)
{
	unsigned int i;
	unsigned long spi_base = sprd_get_spi_base();
	uint32_t *tx_p = (uint32_t  *)tx_buf;

	for (i = 0; i < num; i++, tx_p++)
		writel(*tx_p, (spi_base + SPI_TXD));

	return (num <<2);
}

static unsigned int sprd_spi_write_bufs(void *tx_buf, unsigned int num)
{
	switch (bit_per_word) {
	case 8:
		return sprd_spi_write_bufs_u8(tx_buf, num);
		break;
	case 16:
		return sprd_spi_write_bufs_u16(tx_buf, num);
		break;
	case 32:
		return sprd_spi_write_bufs_u32(tx_buf, num);
		break;
	default:
		return -1;
	}
}

static unsigned int sprd_spi_read_bufs_u8(void *rx_buf, unsigned int num)
{
	unsigned int i;
	unsigned long spi_base = sprd_get_spi_base();
	unsigned int data_mask = sprd_get_spi_data_mask();
	uint8_t *rx_p = (uint8_t*)rx_buf;

	for (i = 0; i < num; i++, rx_p++) {
		while ((readl(spi_base + SPI_STS2) & SPI_RX_FIFO_REALLY_EMPTY));
		*rx_p = data_mask & readb(spi_base + SPI_TXD);
	}

	return num;
}

static unsigned int sprd_spi_read_bufs_u16(void *rx_buf, unsigned int num)
{
	unsigned int i;
	unsigned long spi_base = sprd_get_spi_base();
	unsigned int data_mask = sprd_get_spi_data_mask();
	uint16_t *rx_p = (uint16_t*)rx_buf;

	for (i = 0; i < num; i++, rx_p++) {
		while ((readl(spi_base + SPI_STS2) & SPI_RX_FIFO_REALLY_EMPTY));
		*rx_p = data_mask & readw(spi_base + SPI_TXD);
	}

	return (num << 1);
}

static unsigned int sprd_spi_read_bufs_u32(void *rx_buf, unsigned int num)
{
	unsigned int i;
	unsigned long spi_base = sprd_get_spi_base();
	unsigned int data_mask = sprd_get_spi_data_mask();
	uint32_t *rx_p = (uint32_t*)rx_buf;

	for (i = 0; i < num; i++, rx_p++) {
		while ((readl(spi_base + SPI_STS2) & SPI_RX_FIFO_REALLY_EMPTY));
		*rx_p = data_mask & readl(spi_base + SPI_TXD);
	}

	return (num << 2);
}

static unsigned int sprd_spi_read_bufs(void *rx_buf, unsigned int num)
{
	switch (bit_per_word) {
	case 8:
		return sprd_spi_read_bufs_u8(rx_buf, num);
		break;
	case 16:
		return sprd_spi_read_bufs_u16(rx_buf, num);
		break;
	case 32:
		return sprd_spi_read_bufs_u32(rx_buf, num);
		break;
	default:
		return -1;
	}

}

static int sprd_spi_read(struct spi_transfer *transfer)
{
	unsigned long spi_base = sprd_get_spi_base();
	unsigned int block_num = transfer->data_len;
	unsigned int reg_val = 0;
	unsigned int trans_num = 0;
	unsigned int read_num = 0;
	unsigned int timeout = 0;
	unsigned int *pbuf = transfer->rx_buf;

	reg_val = readl(spi_base + SPI_CTL4);
	reg_val &= ~(SPI_START_RX | 0x1FF);
	writel(reg_val, (spi_base + SPI_CTL4));

	while (block_num) {
		trans_num = block_num > SPRD_SPI_FIFO_SIZE ? SPRD_SPI_FIFO_SIZE : block_num;
		reg_val = readl(spi_base + SPI_CTL4);
		reg_val |= trans_num;
		writel(reg_val, (spi_base + SPI_CTL4));
		reg_val = readl(spi_base + SPI_CTL4);
		reg_val |= SPI_START_RX;
		writel(reg_val, (spi_base + SPI_CTL4));

		while ((readl(spi_base + SPI_STS3)) != trans_num) {
			if (++timeout > SPI_TIME_OUT) {
				errorf("spi read error, at_only_read mode spi send timeout!\n");
				return spi_trans_timeout;
			}
		}

		read_num += sprd_spi_read_bufs(pbuf, trans_num);
		pbuf += trans_num;
		block_num -= trans_num;
		timeout = 0;
	}

	return transfer->data_len;
}

static int sprd_spi_write_read(struct spi_transfer *transfer)
{
	unsigned int write_num = 0;
	unsigned int read_num = 0;
	unsigned int trans_num = 0;
	void *tx_buf = transfer->tx_buf;
	void *rx_buf = transfer->rx_buf;
	unsigned int block_num = transfer->data_len;

	while (block_num) {
		trans_num = block_num > SPRD_SPI_FIFO_SIZE ? SPRD_SPI_FIFO_SIZE : block_num;
		if (rx_tx_mode == transfer->rt_mode) {
			sprd_spi_set_tx_length(trans_num, 0);
			sprd_spi_set_rx_length(trans_num, 0);
		}
		else
			sprd_spi_set_tx_length(trans_num, 0);

		write_num += sprd_spi_write_bufs(tx_buf, trans_num);

		if (spi_trans_timeout == sprd_spi_wait_tx_finish())
			return spi_trans_timeout;

		if (rx_mode != transfer->rt_mode) {
			if (spi_trans_timeout == sprd_spi_wait_rx_finish())
				return spi_trans_timeout;
			read_num += sprd_spi_read_bufs(rx_buf, trans_num);
			rx_buf += trans_num;
		}
		tx_buf += trans_num;
		block_num -= trans_num;
	}

	return transfer->data_len;
}

int sprd_spi_transfer(struct spi_transfer *transfer)
{
	sprd_spi_fifo_rst();
	switch (bit_per_word) {
	case 8:
		break;
	case 16:
		transfer->data_len = transfer->data_len  >> 1;
		break;
	case 32:
		transfer->data_len = transfer->data_len  >> 2;
		break;
	default:
		return -1;
	}

	if (rx_mode == transfer->rt_mode)
		return sprd_spi_read(transfer);
	else
		return sprd_spi_write_read(transfer);
}

int sprd_spi_write_data(unsigned int *pbuf, unsigned int data_len, unsigned int dummy_bit_len)
{
	unsigned int block_num = data_len;
	unsigned int trans_num = 0;
	unsigned int write_num = 0;
	unsigned int first_write = 1;

	sprd_spi_fifo_rst();
	while (block_num) {
		trans_num = block_num > SPRD_SPI_FIFO_SIZE ? SPRD_SPI_FIFO_SIZE : block_num;
		if (first_write)
			sprd_spi_set_tx_length(trans_num, dummy_bit_len);
		else
			sprd_spi_set_tx_length(trans_num, 0);

		write_num += sprd_spi_write_bufs(pbuf, trans_num);
		sprd_spi_tx_req();

		if (spi_trans_timeout == sprd_spi_wait_tx_finish())
			return spi_trans_timeout;

		pbuf += trans_num;
		block_num -= trans_num;
		first_write = 0;
	}

	return data_len;
}

int sprd_spi_read_data(unsigned int *pbuf, unsigned int data_len, unsigned int dummy_bit_len)
{
	unsigned int block_num = data_len;
	unsigned int trans_num = 0;
	unsigned int read_num = 0;
	unsigned int first_read = 1;

	sprd_spi_fifo_rst();
	while (block_num) {
		trans_num = block_num > SPRD_SPI_FIFO_SIZE ? SPRD_SPI_FIFO_SIZE : block_num;
		if (first_read)
			sprd_spi_set_rx_length(trans_num, dummy_bit_len);
		else
			sprd_spi_set_rx_length(trans_num, 0);

		sprd_spi_rx_req();
		if (spi_trans_timeout == sprd_spi_wait_rx_finish())
			return spi_trans_timeout;

		read_num += sprd_spi_read_bufs(pbuf, trans_num);
		pbuf += trans_num;
		block_num -= trans_num;
		first_read = 0;
	}

	return data_len;
}


#if defined(CONFIG_RFSPIRW)
int spi_reg_read(u16 reg)
{
	uint32_t rfspicmd1_data;
	uint32_t rfspicmd1_val;
	uint32_t rfspicmd0_data;
	uint32_t rfspicmd0_val;
	uint32_t val;

	//0x645D0034 set bit0 to 1
	rfspicmd1_data = CHIP_REG_GET(SPRD_RFSPI_CMD1);
	udelay(100);
	rfspicmd1_data = CHIP_REG_GET(SPRD_RFSPI_CMD1);
	rfspicmd1_val = SET_BIT(rfspicmd1_data,0);
	udelay(100);
	CHIP_REG_SET(SPRD_RFSPI_CMD1,rfspicmd1_val);
	//0x645D0030 bit
	rfspicmd0_data = CHIP_REG_GET(SPRD_RFSPI_CMD0);
	udelay(100);
	rfspicmd0_data &= 0x0000FFFF;//set bit15-31 to 0
	udelay(100);
	rfspicmd0_data |= (reg<<16*1);//write the address to be read
	udelay(100);
	rfspicmd0_val = SET_BIT(rfspicmd0_data,31); //set bit31 to 1
	udelay(100);
	CHIP_REG_SET(SPRD_RFSPI_CMD0,rfspicmd0_val);

	val = CHIP_REG_GET(SPRD_RFSPI_RDATA);
	udelay(100);
	val &= 0x0000FFFF;
	udelay(100);

	return val;
}

int spi_reg_write(u16 reg, u16 val)
{
	uint32_t rfspicmd1_data;
	uint32_t rfspicmd1_val;
	uint32_t rfspicmd0_data;
	uint32_t rfspicmd0_val;

	rfspicmd1_data = CHIP_REG_GET(SPRD_RFSPI_CMD1);
	rfspicmd1_val = SET_BIT(rfspicmd1_data,0);
	CHIP_REG_SET(SPRD_RFSPI_CMD1,rfspicmd1_val);

	rfspicmd0_val = reg*0x10000;
	rfspicmd0_data = rfspicmd0_val|val;
	CHIP_REG_SET(SPRD_RFSPI_CMD0,rfspicmd0_data);

	return 0;
}
#endif
