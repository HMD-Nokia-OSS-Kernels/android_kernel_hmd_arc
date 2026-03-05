#include <malloc.h>
#include <sdhci.h>
#include <asm/arch/common.h>
#include <asm/arch/sdio_cfg.h>
#include <delay.h>
#include <stdio.h>
#include <arch/sprd_cache.h>
#include <errno.h>
#include <sprd_regulator.h>

#define sdhci_debugf(condition, x...) if (condition) {debugf(x);}

extern uint32_t (*sprd_sdhci_get_delay)(uint32_t device_type);

void *aligned_buffer;

static void sprd_sdhci_reset(struct sprd_sdhci_host *host, u8 mask)
{
	u8 tmp = 0;
	/*
	* SPRD_SDHCI_SOFTWARE_RESET BIT24/BIT25/BIT26 is WO
	*/
	tmp = sprd_sdhci_readb(host, SPRD_SDHCI_SOFTWARE_RESET);
	tmp |= mask;
	sprd_sdhci_writeb(host, tmp, SPRD_SDHCI_SOFTWARE_RESET);
	mdelay(1);
}

static int sprd_sdhci_reset_for_scan(struct mmc *mmc)
{
	unsigned int time = 0;
	struct sprd_sdhci_host *host = mmc->priv;

	while(time++ < 4000) {//waiting for idle, incase of overwrite memry, max timeout 4S
		if (!(sprd_sdhci_readl(host, SPRD_SDHCI_PRESENT_STATE) & SPRD_SDHCI_DOING_READ))
			break;
		mdelay(1);
	}
	if (time >= 4000) {
		errorf("reset fail, waiting for idle timeout\n");
		return -1;
	}

	sprd_sdhci_reset(host, SPRD_SDHCI_RESET_CMD | SPRD_SDHCI_RESET_DATA);

	return 0;
}

static void sprd_sdhci_cmd_done(struct sprd_sdhci_host *host,
		struct mmc_cmd *cmd)
{
	u32 i;
	u32 flags = cmd->resp_type;
	uint *resp = cmd->response;
	uint offset;

	if (flags & MMC_RSP_136) {
		/* CRC is stripped so we need to do some shifting. */
		for (i = 0, offset = 12; i < 3; i++, offset -= 4) {
			resp[i] = sprd_sdhci_readl(host,
					SPRD_SDHCI_RESPONSE + offset) << 8;
			resp[i] |= sprd_sdhci_readb(host,
					SPRD_SDHCI_RESPONSE + offset - 1);
		}
		resp[3] = sprd_sdhci_readl(host, SPRD_SDHCI_RESPONSE) << 8;
	} else
		resp[0] = sprd_sdhci_readl(host, SPRD_SDHCI_RESPONSE);

	return;
}

static int sprd_sdhci_transfer_data(struct sprd_sdhci_host *host,
		struct mmc_data *data, unsigned long start_addr)
{
	u32 stat, mask;
	u32 timeout = 90000000;

	unsigned char ctrl;
	ctrl = sprd_sdhci_readb(host, SPRD_SDHCI_HOST_CONTROL_REG1);
	ctrl &= ~SPRD_SDHCI_CTRL_DMA_MASK;
	sprd_sdhci_writeb(host, ctrl, SPRD_SDHCI_HOST_CONTROL_REG1);

	do {
		stat = sprd_sdhci_readl(host, SPRD_SDHCI_INT_STATUS);
		if (stat & SPRD_SDHCI_INT_ERROR) {
			errorf("%s: Error detected in status(0x%X)!\n",
			       __func__, stat);
			return -1;
		}

		if (stat & SPRD_SDHCI_INT_DMA_END) {
			sprd_sdhci_writel(host, SPRD_SDHCI_INT_DMA_END, SPRD_SDHCI_INT_STATUS);
			start_addr &= ~(SPRD_SDHCI_DEFAULT_BOUNDARY_SIZE - 1);
			start_addr += SPRD_SDHCI_DEFAULT_BOUNDARY_SIZE;
			sprd_sdhci_writel(host,
				     (u32)(((u64)start_addr) & 0xFFFFFFFF),
				      SPRD_SDHCI_DMA_ADDRESS_LOW);
			sprd_sdhci_writel(host,
				     (u32)(((u64)start_addr >> 32) &
				     0xFFFFFFFF), SPRD_SDHCI_DMA_ADDRESS_HIGH);
		}

		if (timeout-- > 0)
			udelay(10);
		else
			return -1;
	} while (!(stat & SPRD_SDHCI_INT_DATA_END));

	return 0;
}

#define CONFIG_SPRD_SDHCI_CMD_MAX_TIMEOUT		32000
#define CONFIG_SPRD_SDHCI_CMD_DEFAULT_TIMEOUT	1000

static uint sprd_sdhci_calc_data_timeout(struct mmc *mmc)
{
	u32 target_timeout, current_timeout, count = 0;

	current_timeout = 1 << 16;
	target_timeout = 0xA * mmc->clock;

	while(target_timeout > current_timeout) {
		count++;
		current_timeout <<= 1;
	}

	count--;
	if (count >= 0xF)
		count = 0xE;

	return count;
}

static int sprd_sdhci_send_command(struct mmc *mmc, struct mmc_cmd *cmd,
		struct mmc_data *data)
{
	struct sprd_sdhci_host *host = mmc->priv;
	unsigned int stat = 0;
	int ret = 0;
	int trans_bytes = 0;
	u32 mask, flags, mode;
	unsigned int time = 0;
	unsigned long start_addr = 0;
	int mmc_dev = mmc->block_dev.dev_num;
	uint data_timeout_value = 0;

	unsigned int ultemp = 0;

	static unsigned int cmd_timeout = CONFIG_SPRD_SDHCI_CMD_DEFAULT_TIMEOUT;

	unsigned start = get_timer(0);
	unsigned int timeout = 0;

	sprd_sdhci_writel(host, SPRD_SDHCI_INT_ALL_MASK, SPRD_SDHCI_INT_STATUS);
	mask = SPRD_SDHCI_CMD_INHIBIT | SPRD_SDHCI_DATA_INHIBIT;

	if (MMC_CMD_STOP_TRANSMISSION == cmd->cmdidx)
		mask &= ~SPRD_SDHCI_DATA_INHIBIT;

	while (sprd_sdhci_readl(host, SPRD_SDHCI_PRESENT_STATE) & mask) {
		if (time >= cmd_timeout) {
			errorf("%s: MMC: %d busy ", __func__, mmc_dev);
			if (2 * cmd_timeout > CONFIG_SPRD_SDHCI_CMD_MAX_TIMEOUT) {
					errorf("timeout.\n");
					sdio_dump(host->ioaddr);
					return COMM_ERR;
			} else {
				cmd_timeout += cmd_timeout;
				errorf("timeout increasing to: %u ms.\n", cmd_timeout);
				sdio_dump(host->ioaddr);
			}
		}
		mdelay(1);
		time++;
	}

	mask = SPRD_SDHCI_INT_RESPONSE;
	if (!(cmd->resp_type & MMC_RSP_PRESENT)) {
		flags = SPRD_SDHCI_CMD_RESP_NONE;
	} else if ((cmd->resp_type & MMC_RSP_136) == MMC_RSP_136) {
		flags = SPRD_SDHCI_CMD_RESP_LONG;
	} else if ((cmd->resp_type & MMC_RSP_BUSY) == MMC_RSP_BUSY) {
		flags = SPRD_SDHCI_CMD_RESP_SHORT_BUSY;
		mask |= SPRD_SDHCI_INT_DATA_END;
	} else {
		flags = SPRD_SDHCI_CMD_RESP_SHORT;
	}

	if ((cmd->resp_type & MMC_RSP_CRC) == MMC_RSP_CRC)
		flags |= SPRD_SDHCI_CMD_CRC;

	if ((cmd->resp_type & MMC_RSP_OPCODE) == MMC_RSP_OPCODE)
		flags |= SPRD_SDHCI_CMD_INDEX;

	if (data != 0)
		flags |= SPRD_SDHCI_CMD_DATA;

	data_timeout_value = sprd_sdhci_calc_data_timeout(mmc);
	sprd_sdhci_writeb(host, data_timeout_value, SPRD_SDHCI_TIMEOUT_CONTROL);

	/* Set Transfer mode regarding to data flag */
	if (data != 0) {
		mode = SPRD_SDHCI_TRNS_BLK_CNT_EN;
		trans_bytes = data->blocks * data->blocksize;
		if (data->blocks > 1)
			mode |= SPRD_SDHCI_TRNS_MULTI;

		if (data->flags == MMC_DATA_READ) {
			mode |= SPRD_SDHCI_TRNS_READ;
			start_addr = (unsigned long)data->dest;
		} else {
			start_addr = (unsigned long)data->src;
		}

		sprd_sdhci_writel(host, (u32) (((u64) start_addr) & 0xFFFFFFFF),
			     SPRD_SDHCI_DMA_ADDRESS_LOW);
		sprd_sdhci_writel(host, (u32) (((u64)start_addr >> 32) & 0xFFFFFFFF),
			     SPRD_SDHCI_DMA_ADDRESS_HIGH);

		mode |= SPRD_SDHCI_TRNS_DMA;

		sprd_sdhci_writew(host,
			     SPRD_SDHCI_MAKE_BLKSZ(SPRD_SDHCI_DEFAULT_BOUNDARY_ARG,
			     data->blocksize),
			     SPRD_SDHCI_BLOCK_SIZE);
		sprd_sdhci_writel(host, data->blocks, SPRD_SDHCI_BLOCK_COUNT);
		ultemp = mode | (SPRD_SDHCI_MAKE_CMD(cmd->cmdidx, flags) << 16);
	} else {
		ultemp = SPRD_SDHCI_DEFAULT_TR_MODE;
		ultemp = ultemp | (SPRD_SDHCI_MAKE_CMD(cmd->cmdidx, flags) << 16);
	}

	sprd_sdhci_writel(host, cmd->cmdarg, SPRD_SDHCI_ARGUMENT);

	flush_cache(start_addr, trans_bytes);

	sprd_sdhci_writel(host, ultemp, SPRD_SDHCI_TRANSFER_MODE);
	if(cmd->cmdidx == MMC_CMD_ERASE)
		timeout = 10 * 1000; /* erase timeout 10s */
	else
		timeout = 3 * 1000; /* others timeout 3s */

	start = get_timer(0);
	do {
		stat = sprd_sdhci_readl(host, SPRD_SDHCI_INT_STATUS);
		if (stat & SPRD_SDHCI_INT_ERROR)
			break;
	} while (((stat & mask) != mask) &&
		 (get_timer(start) < timeout));

	if (get_timer(start) >= timeout) {
		if (host->quirks & SPRD_SDHCI_QUIRK_BROKEN_R1B)
			return 0;
		else {
			errorf("%s: Timeout %lu for status update!\n",
				__func__, get_timer(start));
			sdio_dump(host->ioaddr);
			return TIMEOUT;
		}
	}

	if ((stat & (SPRD_SDHCI_INT_ERROR | mask)) == mask) {
		sprd_sdhci_cmd_done(host, cmd);
		sprd_sdhci_writel(host, mask, SPRD_SDHCI_INT_STATUS);
	} else {
		errorf("%s: ret = -1, interrupt status: 0x%x\n", __func__, stat);
		ret = -1;
	}

	if (!ret && data)
		ret = sprd_sdhci_transfer_data(host, data, start_addr);

	if (host->quirks & SPRD_SDHCI_QUIRK_WAIT_SEND_CMD)
		udelay(800000);

	stat = sprd_sdhci_readl(host, SPRD_SDHCI_INT_STATUS);
	sprd_sdhci_writel(host, SPRD_SDHCI_INT_ALL_MASK, SPRD_SDHCI_INT_STATUS);
	if (!ret) {
		if (data && (host->quirks & SPRD_SDHCI_QUIRK_32BIT_DMA_ADDR) &&
		    (data->flags == MMC_DATA_READ))
			memcpy(data->dest, aligned_buffer, trans_bytes);
		if (data && (data->flags == MMC_DATA_READ))
			invalidate_dcache_range(start_addr, start_addr + trans_bytes);
		return 0;
	}
	sprd_sdhci_reset(host, SPRD_SDHCI_RESET_CMD);
	sprd_sdhci_reset(host, SPRD_SDHCI_RESET_DATA);
	dprintf(INFO,"%s interrupt status: 0x%x, CMD%d, delay: 0x%x\n", __func__,
		stat, cmd->cmdidx, sprd_sdhci_readl(host, SPRD_SDHCI_DLL_DLY));
	if (stat & SPRD_SDHCI_INT_TIMEOUT)
		return TIMEOUT;
	else
		return COMM_ERR;
}

int sprd_sdhci_send_command_backstage(struct mmc *mmc, struct mmc_cmd *cmd,
		struct mmc_data *data)
{
	struct sprd_sdhci_host *host = (struct sprd_sdhci_host *)mmc->priv;
	unsigned int stat = 0;
	int trans_bytes = 0;
	u32 mask, flags, mode;
	unsigned int time = 0;
	unsigned long start_addr = 0;
	unsigned int retry = 8000000;
	int mmc_dev = mmc->block_dev.dev_num;

	unsigned int ultemp = 0;

	/* Timeout unit - ms */
	static unsigned int cmd_timeout = CONFIG_SPRD_SDHCI_CMD_DEFAULT_TIMEOUT;

	sprd_sdhci_writel(host, SPRD_SDHCI_INT_ALL_MASK, SPRD_SDHCI_INT_STATUS);
	mask = SPRD_SDHCI_CMD_INHIBIT | SPRD_SDHCI_DATA_INHIBIT;

	if (cmd->cmdidx == MMC_CMD_STOP_TRANSMISSION)
		mask &= ~SPRD_SDHCI_DATA_INHIBIT;

	while (sprd_sdhci_readl(host, SPRD_SDHCI_PRESENT_STATE) & mask) {
		if (time >= cmd_timeout) {
			errorf("%s: MMC: %d busy ", __func__, mmc_dev);
			if (2 * cmd_timeout > CONFIG_SPRD_SDHCI_CMD_MAX_TIMEOUT) {
					errorf("timeout.\n");
					sdio_dump(host->ioaddr);
					return COMM_ERR;
			} else {
				cmd_timeout += cmd_timeout;
				errorf("timeout increasing to: %u ms.\n", cmd_timeout);
				sdio_dump(host->ioaddr);
			}
		}
		time++;
		mdelay(1);
	}

	mask = SPRD_SDHCI_INT_RESPONSE;
	if (!(cmd->resp_type & MMC_RSP_PRESENT)) {
		flags = SPRD_SDHCI_CMD_RESP_NONE;
	} else if ((cmd->resp_type & MMC_RSP_136) == MMC_RSP_136) {
		flags = SPRD_SDHCI_CMD_RESP_LONG;
	} else if ((cmd->resp_type & MMC_RSP_BUSY) == MMC_RSP_BUSY) {
		flags = SPRD_SDHCI_CMD_RESP_SHORT_BUSY;
		mask |= SPRD_SDHCI_INT_DATA_END;
	} else {
		flags = SPRD_SDHCI_CMD_RESP_SHORT;
	}

	if ((cmd->resp_type & MMC_RSP_CRC) == MMC_RSP_CRC)
		flags |= SPRD_SDHCI_CMD_CRC;

	if ((cmd->resp_type & MMC_RSP_OPCODE) == MMC_RSP_OPCODE)
		flags |= SPRD_SDHCI_CMD_INDEX;

	if (data != 0)
		flags |= SPRD_SDHCI_CMD_DATA;

	sprd_sdhci_writeb(host, 0xe, SPRD_SDHCI_TIMEOUT_CONTROL);

	/* Set Transfer mode regarding to data flag */
	if (data != 0) {
		mode = SPRD_SDHCI_TRNS_BLK_CNT_EN;
		trans_bytes = data->blocks * data->blocksize;
		if (data->blocks > 1)
			mode |= SPRD_SDHCI_TRNS_MULTI;

		if (data->flags == MMC_DATA_READ) {
			mode |= SPRD_SDHCI_TRNS_READ;
			start_addr = (unsigned long)data->dest;
		} else {
			start_addr = (unsigned long)data->src;
		}

		sprd_sdhci_writel(host, (u32) (((u64) start_addr) & 0xFFFFFFFF),
			SPRD_SDHCI_DMA_ADDRESS_LOW);
		sprd_sdhci_writel(host, (u32) (((u64)start_addr >> 32) & 0xFFFFFFFF),
			SPRD_SDHCI_DMA_ADDRESS_HIGH);

		mode |= SPRD_SDHCI_TRNS_DMA;

		sprd_sdhci_writew(host,
			SPRD_SDHCI_MAKE_BLKSZ(SPRD_SDHCI_DEFAULT_BOUNDARY_ARG,
			data->blocksize),
			SPRD_SDHCI_BLOCK_SIZE);
		sprd_sdhci_writel(host, data->blocks, SPRD_SDHCI_BLOCK_COUNT);
		ultemp = mode | (SPRD_SDHCI_MAKE_CMD(cmd->cmdidx, flags) << 16);
	} else {
		ultemp = SPRD_SDHCI_DEFAULT_TR_MODE;
		ultemp = ultemp | (SPRD_SDHCI_MAKE_CMD(cmd->cmdidx, flags) << 16);
	}

	if (MMC_CMD_WRITE_MULTIPLE_BLOCK == cmd->cmdidx)
		ultemp |= SPRD_SDHCI_TRNS_ACMD12;

	sprd_sdhci_writel(host, cmd->cmdarg, SPRD_SDHCI_ARGUMENT);

	flush_cache(start_addr, trans_bytes);

	sprd_sdhci_writel(host, ultemp, SPRD_SDHCI_TRANSFER_MODE);
	do {
		stat = sprd_sdhci_readl(host, SPRD_SDHCI_INT_STATUS);
		if (stat & SPRD_SDHCI_INT_ERROR)
			break;
		if (--retry == 0)
			break;
	} while ((stat & mask) != mask);

	if (retry == 0) {
		if (host->quirks & SPRD_SDHCI_QUIRK_BROKEN_R1B) {
			return 0;
		} else {
			errorf("%s: Timeout for status update!\n", __func__);
			sdio_dump(host->ioaddr);
			return TIMEOUT;
		}
	}

	if ((stat & (SPRD_SDHCI_INT_ERROR | mask)) == mask) {
		sprd_sdhci_cmd_done(host, cmd);
		sprd_sdhci_writel(host, mask, SPRD_SDHCI_INT_STATUS);
		/*do not wait for transfer complete*/
		return 0;
	}

	stat = sprd_sdhci_readl(host, SPRD_SDHCI_INT_STATUS);
	sprd_sdhci_writel(host, SPRD_SDHCI_INT_ALL_MASK, SPRD_SDHCI_INT_STATUS);

	if (stat & SPRD_SDHCI_INT_TIMEOUT) {
		sprd_sdhci_reset(host, SPRD_SDHCI_RESET_CMD);
		sprd_sdhci_reset(host, SPRD_SDHCI_RESET_DATA);
		return TIMEOUT;
	} else {
		sprd_sdhci_reset(host, SPRD_SDHCI_RESET_CMD);
		sprd_sdhci_reset(host, SPRD_SDHCI_RESET_DATA);
		return COMM_ERR;
	}
}

int sprd_sdhci_query_command_backstage(struct mmc *mmc, struct mmc_data *data)
{
	struct sprd_sdhci_host *host = (struct sprd_sdhci_host *)mmc->priv;
	unsigned long start_addr = 0;
	int ret = 0;
	unsigned int stat = 0;

	if (NULL == data)
		return -1;

	ret = sprd_sdhci_transfer_data(host, data, start_addr);

	start_addr = (unsigned long)data->src;

	stat = sprd_sdhci_readl(host, SPRD_SDHCI_INT_STATUS);
	sprd_sdhci_writel(host, SPRD_SDHCI_INT_ALL_MASK, SPRD_SDHCI_INT_STATUS);
	if (!ret)
		return 0;

	if (stat & SPRD_SDHCI_INT_TIMEOUT) {
		sprd_sdhci_reset(host, SPRD_SDHCI_RESET_CMD);
		sprd_sdhci_reset(host, SPRD_SDHCI_RESET_DATA);
		return TIMEOUT;
	} else {
		sprd_sdhci_reset(host, SPRD_SDHCI_RESET_CMD);
		sprd_sdhci_reset(host, SPRD_SDHCI_RESET_DATA);
		return COMM_ERR;
	}
}

static int sprd_sdhci_set_clock(struct mmc *mmc, unsigned int clock)
{
	struct sprd_sdhci_host *host = mmc->priv;
	unsigned int div, clk, timeout;

	sprd_sdhci_writew(host, 0, SPRD_SDHCI_CLOCK_CONTROL);

	if (clock == 0)
		return 0;

	if (SPRD_SDHCI_GET_VERSION(host) >= SPRD_SDHCI_SPEC_300) {
		if (mmc->cfg->f_max <= clock)
			div = 0;
		else {
			/* Version 3.00 divisors must be a multiple of 2. */
			for (div = 1; div < SPRD_SDHCI_MAX_DIV_SPEC_300; div++) {
				if ((mmc->cfg->f_max / (div * 2)) <= clock)
					break;
			}
		}
	} else {
		if (mmc->cfg->f_max <= clock)
			div = 0;
		else {
			/* Version 2.00 divisors must be a power of 2. */
			for (div = 1; div < SPRD_SDHCI_MAX_DIV_SPEC_200; div++) {
				if ((mmc->cfg->f_max / (div * 2)) <= clock)
					break;
			}
		}
	}

	if (host->set_clock)
		host->set_clock(host->index, div);

	clk = (div & SPRD_SDHCI_DIV_MASK) << SPRD_SDHCI_DIVIDER_SHIFT;
	clk |= ((div & SPRD_SDHCI_DIV_HI_MASK) >> SPRD_SDHCI_DIV_MASK_LEN)
	    << SPRD_SDHCI_DIVIDER_HI_SHIFT;
	clk |= SPRD_SDHCI_CLOCK_INT_EN;
	sprd_sdhci_writew(host, clk, SPRD_SDHCI_CLOCK_CONTROL);

	/* Wait max 20 ms */
	timeout = 2000;
	while (!((clk = sprd_sdhci_readw(host, SPRD_SDHCI_CLOCK_CONTROL))
		 & SPRD_SDHCI_CLOCK_INT_STABLE)) {
		if (timeout == 0) {
			errorf("%s: Internal clock never stabilised.\n",
			       __func__);
			return -1;
		}
		timeout--;
		udelay(1000);
	}

	clk |= SPRD_SDHCI_CLOCK_CARD_EN;
	sprd_sdhci_writew(host, clk, SPRD_SDHCI_CLOCK_CONTROL);
	return 0;
}

static int sprd_sdhc_enable_dpll_scan(struct mmc *mmc)
{
	int timeout = 1000;
	u32 tmp = 0;
	struct sprd_sdhci_host *host = mmc->priv;
	int dll_cnt = 0;

	if ((mmc->selected_mode == MMC_HS) || (mmc->selected_mode == SD_HS))
		return 0xFF;

	sprd_sdhci_writel(host, tmp, SDHCI_REG_32_DLL_CFG);

	tmp = SDHCI_DLL_PHA_INTERNAL;
	sprd_sdhci_writel(host, tmp, SDHCI_REG_32_DLL_CFG);

	if ((mmc->selected_mode == UHS_SDR50) || (mmc->selected_mode == MMC_DDR_52)) {
		tmp |= SDHCI_DLL_CLK_SEL;
		sprd_sdhci_writel(host, tmp, SDHCI_REG_32_DLL_CFG);
	}

	tmp |= SDHCI_DLL_DATWR_CPST_EN | SDHCI_DLL_RDCMD_CPST_EN
		 | SDHCI_DLL_RDPOS_CPST_EN | SDHCI_DLL_RDNEG_CPST_EN;
	sprd_sdhci_writel(host, tmp, SDHCI_REG_32_DLL_CFG);

	tmp |= SDHCI_DLL_CPST_EN;
	sprd_sdhci_writel(host, tmp, SDHCI_REG_32_DLL_CFG);

	tmp |= 0xF << SDHCI_DLL_WAIT_CNT_SHIFT; //maybe useless, just copy from chipvld
	sprd_sdhci_writel(host, tmp, SDHCI_REG_32_DLL_CFG);

	tmp |= SDHCI_DLL_HALF_MODE;
	sprd_sdhci_writel(host, tmp, SDHCI_REG_32_DLL_CFG);

	tmp |= 2 << SDHCI_DLL_CPST_THRES_SHIFT;
	sprd_sdhci_writel(host, tmp, SDHCI_REG_32_DLL_CFG);

	tmp |= 0xD << SDHCI_DLL_INIT_COUNT_SHIFT;
	sprd_sdhci_writel(host, tmp, SDHCI_REG_32_DLL_CFG);

	tmp |= SDHCI_DLL_EN;
	sprd_sdhci_writel(host, tmp, SDHCI_REG_32_DLL_CFG);

	while (--timeout) {
		if ((sprd_sdhci_readl(host, SDHCI_REG_32_DLL_STS0) &
							SDHCI_DLL_LOCKED))
			break;
		mdelay(1);
	}

	if (!timeout) {
		errorf("%s dpll locked faile!\n", __func__);
		sdio_dump(host->ioaddr);
		errorf("DLL_STS0 : 0x%x, DLL_CFG : 0x%x\n",
			 sprd_sdhci_readl(host, SDHCI_REG_32_DLL_STS0),
			 sprd_sdhci_readl(host, SDHCI_REG_32_DLL_CFG));
		errorf("DLL_DLY : 0x%x, DLL_STS1 : 0x%x\n",
			 sprd_sdhci_readl(host, SDHCI_REG_32_DLL_DLY),
			 sprd_sdhci_readl(host, SDHCI_REG_32_DLL_STS1));
		return 0;
	} else
		dll_cnt = sprd_sdhci_readl(host, SDHCI_REG_32_DLL_STS0) & 0xFF;

	dll_cnt = (dll_cnt * 150 / 100) * 2;
#if DEBUG
	sdio_dump(host->ioaddr);
#endif

	return dll_cnt;
}

static void sprd_sdhc_enable_dpll(struct sprd_sdhci_host *host)
{
	int timeout = 1000;
	u32 tmp = 0;

	tmp = sprd_sdhci_readl(host, SDHCI_REG_32_DLL_CFG);
	tmp &= ~(SDHCI_DLL_EN | SDHCI_DLL_ALL_CPST_EN |
		SDHCI_DLL_HALF_MODE);
	sprd_sdhci_writel(host, tmp, SDHCI_REG_32_DLL_CFG);
	mdelay(1);

	tmp = sprd_sdhci_readl(host, SDHCI_REG_32_DLL_CFG);
	tmp |= SDHCI_DLL_WAIT_CNT | SDHCI_DLL_ALL_CPST_EN |
		 SDHCI_DLL_INIT_COUNT | SDHCI_DLL_PHA_INTERNAL |
		 SDHCI_DLL_CPST_THRES | SDHCI_DLL_HALF_MODE;
	sprd_sdhci_writel(host, tmp, SDHCI_REG_32_DLL_CFG);
	mdelay(1);

	tmp = sprd_sdhci_readl(host, SDHCI_REG_32_DLL_CFG);
	tmp |= SDHCI_DLL_EN;
	sprd_sdhci_writel(host, tmp, SDHCI_REG_32_DLL_CFG);
	mdelay(1);

	while (--timeout) {
		if ((sprd_sdhci_readl(host, SDHCI_REG_32_DLL_STS0) &
							SDHCI_DLL_LOCKED))
			break;
		mdelay(1);
	}

	if (!timeout) {
		errorf("dpll locked faile!\n");
		errorf("DLL_STS0 : 0x%x, DLL_CFG : 0x%x\n",
			 sprd_sdhci_readl(host, SDHCI_REG_32_DLL_STS0),
			 sprd_sdhci_readl(host, SDHCI_REG_32_DLL_CFG));
		errorf("DLL_DLY : 0x%x, DLL_STS1 : 0x%x\n",
			 sprd_sdhci_readl(host, SDHCI_REG_32_DLL_DLY),
			 sprd_sdhci_readl(host, SDHCI_REG_32_DLL_STS1));
	} else
		sdhci_debugf(host->mmc->in_scan == false, "dpll locked done\n");
}

static void sprd_sdhci_close_clock(struct mmc *mmc)
{
	struct sprd_sdhci_host *host = mmc->priv;

	sprd_sdhc_sd_clk_off(host);
	host->clock = 0;

	sdhci_debugf(mmc->in_scan == false, "close sd host clock done\n");
}

static void sprd_sdhci_set_dll_dly(struct mmc *mmc, uint32_t delay_value, uint32_t mask)
{
	struct sprd_sdhci_host *host = mmc->priv;
	uint32_t delay;

	delay = sprd_sdhci_readl(host, SPRD_SDHCI_DLL_DLY);
	delay &= ~mask;
	delay |= delay_value;
	sprd_sdhci_writel(host, delay, SPRD_SDHCI_DLL_DLY);
}

static uint32_t sprd_sdhci_get_dll_dly(struct mmc *mmc)
{
	struct sprd_sdhci_host *host = mmc->priv;

	return sprd_sdhci_readl(host, SPRD_SDHCI_DLL_DLY);
}

static void sprd_sdhci_set_ios(struct mmc *mmc)
{
	u32 ctrl;
	u8 clkchg_flag = 0;
	struct sprd_sdhci_host *host = mmc->priv;
	uint32_t delay;

	if (host->set_control_reg)
		host->set_control_reg(host);
	if (mmc->clock != host->clock) {
		sprd_sdhci_set_clock(mmc, mmc->clock);
		clkchg_flag = 1;
		if (mmc->selected_mode == SD_HS || mmc->selected_mode == MMC_HS_52) {
			if(sprd_sdhci_get_delay && (mmc->clock == mmc->tran_speed)) {
				delay = sprd_sdhci_get_delay(mmc->block_dev.dev_num);
				sprd_sdhci_writel(host, delay, SPRD_SDHCI_DLL_DLY);
			}
		} else {
			delay = 0;
			sprd_sdhci_writel(host, delay, SPRD_SDHCI_DLL_DLY);
		}
	}

	ctrl = sprd_sdhci_readb(host, SPRD_SDHCI_HOST_CONTROL_REG1);
	if (mmc->bus_width == 8) {
		ctrl &= ~SPRD_SDHCI_CTRL_4BITBUS;
		if ((SPRD_SDHCI_GET_VERSION(host) >= SPRD_SDHCI_SPEC_300) ||
		    (host->quirks & SPRD_SDHCI_QUIRK_USE_WIDE8))
			ctrl |= SPRD_SDHCI_CTRL_8BITBUS;
	} else {
		if ((SPRD_SDHCI_GET_VERSION(host) >= SPRD_SDHCI_SPEC_300) ||
		    (host->quirks & SPRD_SDHCI_QUIRK_USE_WIDE8))
			ctrl &= ~SPRD_SDHCI_CTRL_8BITBUS;
		if (mmc->bus_width == 4)
			ctrl |= SPRD_SDHCI_CTRL_4BITBUS;
		else
			ctrl &= ~SPRD_SDHCI_CTRL_4BITBUS;
	}

	ctrl &= ~SPRD_SDHCI_CTRL_HISPD;
	ctrl &= ~SPRD_SDHCI_CTRL_DMA_MASK;
	if (host->quirks & SPRD_SDHCI_QUIRK_NO_HISPD_BIT)
		ctrl &= ~SPRD_SDHCI_CTRL_HISPD;

	sdhc_set_dll_invert(host, SPRD_SDHC_BIT_CMD_DLY_INV |
		SPRD_SDHC_BIT_POSRD_DLY_INV, 1);

	sprd_sdhci_writeb(host, ctrl, SPRD_SDHCI_HOST_CONTROL_REG1);

	if (mmc->clock != host->clock) {
		sprd_sdhc_sd_clk_off(host);
		switch (mmc->selected_mode) {
		case MMC_HS:
		case SD_HS:
			ctrl = SDHCI_BIT_TIMING_MODE_SDR25;
			break;
		case UHS_SDR12:
			ctrl = SDHCI_BIT_TIMING_MODE_SDR12;
			break;
		case UHS_SDR25:
			ctrl = SDHCI_BIT_TIMING_MODE_SDR25;
			break;
		case UHS_SDR50:
			ctrl = SDHCI_BIT_TIMING_MODE_SDR50;
			break;
		case UHS_SDR104:
			ctrl = SDHCI_BIT_TIMING_MODE_SDR104;
			break;
		case UHS_DDR50:
			ctrl = SDHCI_BIT_TIMING_MODE_DDR50;
			break;
		case MMC_DDR_52:
			ctrl = SDHCI_BIT_TIMING_MODE_DDR50;
			break;
		case MMC_HS_200:
			ctrl = SDHCI_BIT_TIMING_MODE_HS200;
			break;
		case MMC_HS_400:
			ctrl = SDHCI_BIT_TIMING_MODE_HS400;
			break;
		case MMC_HS_400_ES:
			ctrl = SDHCI_BIT_TIMING_MODE_HS400;
		default:
			break;
		}
		sprd_sdhc_set_uhs_mode(host, ctrl);

		/* 3 open SD clock */
		sprd_sdhc_sd_clk_on(host);
		host->clock = mmc->clock;
	}

	if (mmc->clock <= 400000 || mmc->selected_mode == MMC_HS ||
			mmc->selected_mode == SD_HS || mmc->selected_mode == MMC_LEGACY ||
			mmc->selected_mode == MMC_HS_52) {
		sdhc_set_dll_invert(host, SPRD_SDHC_BIT_CMD_DLY_INV |
				SPRD_SDHC_BIT_POSRD_DLY_INV, 1);
	} else {
		sdhc_set_dll_invert(host, SPRD_SDHC_BIT_CMD_DLY_INV |
				SPRD_SDHC_BIT_POSRD_DLY_INV, 0);
	}

	if ((mmc->clock > 52000000) && (clkchg_flag == 1)) {
		sprd_sdhc_enable_dpll(host);
	}
}

int sdhci_init(struct mmc *mmc)
{
	u32 temp = 0;
	struct sprd_sdhci_host *host = mmc->priv;

	temp = sprd_sdhci_readl(host, SPRD_SDHCI_HOST_CONTROL_REG2);
	temp |= SPRD_SDHCI_64BIT_ADDR_EN;
	sprd_sdhci_writel(host, temp, SPRD_SDHCI_HOST_CONTROL_REG2);

	sprd_sdhci_writel(host, SPRD_SDHCI_INT_DATA_MASK | SPRD_SDHCI_INT_CMD_MASK,
		SPRD_SDHCI_INT_ENABLE);

	sprd_sdhci_writel(host, SPRD_SDHCI_INT_DATA_MASK |
		SPRD_SDHCI_INT_CMD_MASK | SPRD_SDHCI_INT_ERROR, SPRD_SDHCI_SIGNAL_ENABLE);

	return 0;
}
#ifdef CONFIG_MMC_SUPPORTS_TUNING
static int sprd_calc_hs_mode_tuning_range(struct sprd_sdhci_host *host, int *value_t)
{
	u32 i;
	bool prev_vl = 0;
	int range_count = 0;
	u32 dll_cnt = host->dll_cnt;
	u32 mid_dll_cnt = host->mid_dll_cnt;
	struct ranges_t *ranges = host->ranges;

	for (i = 0; i < mid_dll_cnt; i++) {
		if ((!prev_vl) && value_t[i] && value_t[i + dll_cnt] && value_t[i + mid_dll_cnt]) {
			range_count++;
			ranges[range_count - 1].start = i;
		}

		if (value_t[i] && value_t[i + dll_cnt] && value_t[i + mid_dll_cnt])
			ranges[range_count - 1].end = i;

		prev_vl = value_t[i] && value_t[i + dll_cnt] && value_t[i + mid_dll_cnt];
	}

	host->ranges = ranges;

	return range_count;
}

static int sprd_calc_tuning_range(struct sprd_sdhci_host *host, int *value_t)
{
	u32 i;
	int prev_vl = 0;
	int range_count = 0;
	u32 dll_cnt = host->dll_cnt;
	u32 mid_dll_cnt = host->mid_dll_cnt;
	struct ranges_t *ranges = host->ranges;

	/*
	 * first: 0 <= i < mid_dll_cnt
	 * tuning range: (0 ~ mid_dll_cnt) && (dll_cnt ~ dll_cnt + mid_dll_cnt)
	 */
	for (i = 0; i < mid_dll_cnt; i++) {
		if ((!prev_vl) && value_t[i] && value_t[i + dll_cnt]) {
			range_count++;
			ranges[range_count - 1].start = i;
		}

		if (value_t[i] && value_t[i + dll_cnt]) {
			ranges[range_count - 1].end = i;
			sdhci_debugf(host->mmc->in_scan == false,
					"recalculate tuning ok: %d\n", i);
		} else {
			sdhci_debugf(host->mmc->in_scan == false,
					"recalculate tuning fail: %d\n", i);
		}
		prev_vl = value_t[i] && value_t[i + dll_cnt];
	}

	/*
	 * second: mid_dll_cnt <= i < dll_cnt
	 * tuning range: mid_dll_cnt ~ dll_cnt
	 */
	for (i = mid_dll_cnt; i < dll_cnt; i++) {
		if ((!prev_vl) && value_t[i]) {
			range_count++;
			ranges[range_count - 1].start = i;
		}

		if (value_t[i]) {
			ranges[range_count - 1].end = i;
			sdhci_debugf(host->mmc->in_scan == false,
					"recalculate tuning ok: %d\n", i);
		} else {
			sdhci_debugf(host->mmc->in_scan == false,
					"recalculate tuning fail: %d\n", i);
		}
		prev_vl = value_t[i];
	}

	host->ranges = ranges;

	return range_count;
}

extern const struct sdio_base_info sdio_ctrl_info[2];
static u32 get_delay_value_by_mode(int mode)
{
	if (mode == MMC_HS_200)
		return sdio_ctrl_info[0].hs200_dly;
	else if (mode == SD_HS)
		return sdio_ctrl_info[1].hs_dly;
	else if (mode == UHS_SDR104)
		return sdio_ctrl_info[1].hs200_dly;

	return 0;
}

extern int mmc_send_tuning_cmd(struct mmc *mmc, u32 opcode, int *cmd_error);
extern int mmc_send_tuning_read(struct mmc *mmc, u32 opcode, int *cmd_error);
extern int mmc_send_tuning(struct mmc *mmc, u32 opcode, int *cmd_error);
static int sprd_sdhci_execute_tuning(struct mmc *mmc, uint opcode)
{
	struct sprd_sdhci_host *host = mmc->priv;

	int err = 0;
	int i = 0;
	bool value, first_vl, prev_vl = 0;
	int *value_t;
	struct ranges_t *ranges;

	int length = 0;
	unsigned int range_count = 0;
	int longest_range_len = 0;
	int longest_range = 0;
	int mid_step;
	int final_phase = 0;
	u32 dll_cfg = 0;
	u32 mid_dll_cnt = 0;
	u32 dll_cnt = 0;
	u32 dll_dly = 0;

	sprd_sdhci_reset(host, SPRD_SDHCI_RESET_CMD | SPRD_SDHCI_RESET_DATA);

	dll_cfg = sprd_sdhci_readl(host, SDHCI_REG_32_DLL_CFG);
	dll_cfg &= ~(0xf << 24);
	sprd_sdhci_writel(host, dll_cfg, SDHCI_REG_32_DLL_CFG);

	if (mmc->selected_mode == SD_HS)
		dll_cnt = 128;
	else
		dll_cnt = sprd_sdhci_readl(host, SDHCI_REG_32_DLL_STS0) & 0xff;
	dll_cnt = dll_cnt << 1;
	length = (dll_cnt * 150) / 100;

	sdhci_debugf(mmc->in_scan == false, "dll config 0x%08x, dll count %d, tuning length: %d\n",
		 dll_cfg, dll_cnt, length);

	ranges = (struct ranges_t*)malloc((length + 1) * sizeof(*ranges));
	if (!ranges)
		return -ENOMEM;

	value_t = (int*)malloc((length + 1) * sizeof(*value_t));
	if (!value_t) {
		free(ranges);
		return -ENOMEM;
	}

	do {
		dll_dly = get_delay_value_by_mode(mmc->selected_mode);
		if (!dll_dly) {
			errorf("get default delay value fail\n");
			goto out;
		}

		host->dll_dly = dll_dly;
		if (mmc->selected_mode == SD_HS) {
			if (opcode == MMC_CMD_SET_BLOCKLEN) {
				dll_dly &= ~(SDHCI_CMD_DLY_MASK);
				dll_dly |= ((i << 8) & SDHCI_CMD_DLY_MASK);
			} else {
				dll_dly &= ~(SDHCI_POSRD_DLY_MASK | SDHCI_NEGRD_DLY_MASK |
							SDHCI_CMD_DLY_MASK);
				dll_dly |= (((i << 16) & SDHCI_POSRD_DLY_MASK) |
							((i << 24) & SDHCI_NEGRD_DLY_MASK) |
							((i << 8) & SDHCI_CMD_DLY_MASK));
			}
		} else {
			dll_dly &= ~(SDHCI_CMD_DLY_MASK | SDHCI_POSRD_DLY_MASK);
			dll_dly |= (((i << 8) & SDHCI_CMD_DLY_MASK) |
					((i << 16) & SDHCI_POSRD_DLY_MASK));
		}
		sprd_sdhci_writel(host, dll_dly, SDHCI_REG_32_DLL_DLY);
		if (mmc->selected_mode == SD_HS) {
			if (opcode == MMC_CMD_SET_BLOCKLEN)
				value = !mmc_send_tuning_cmd(mmc, opcode, NULL);
			else
				value = !mmc_send_tuning_read(mmc, opcode, NULL);
		} else {
			value = !mmc_send_tuning(mmc, opcode, NULL);
		}

		if ((!prev_vl) && value) {
			range_count++;
			ranges[range_count - 1].start = i;
		}

		if (value) {
			sdhci_debugf(mmc->in_scan == false, "tuning ok: %d\n", i);
			ranges[range_count - 1].end = i;
			value_t[i] = value;
		} else {
			sdhci_debugf(mmc->in_scan == false, "tuning fail: %d\n", i);
			value_t[i] = value;
		}

		prev_vl = value;
	} while (++i <= length);

	mid_dll_cnt = length - dll_cnt;
	host->dll_cnt = dll_cnt;
	host->mid_dll_cnt = mid_dll_cnt;
	host->ranges = ranges;

	first_vl = (value_t[0] && value_t[dll_cnt]);
	if (mmc->selected_mode == SD_HS)
		range_count = sprd_calc_hs_mode_tuning_range(host, value_t);
	else
		range_count = sprd_calc_tuning_range(host, value_t);

	if (range_count == 0) {
		errorf("all tuning phases fail!\n");
		err = -EIO;
		goto out;
	}
	if (mmc->selected_mode == SD_HS) {
		if ((range_count > 1) && (ranges[range_count - 1].end == 127)
				&& (ranges[0].start == 0)) {
			ranges[0].start = ranges[range_count - 1].start;
			range_count--;

			if (ranges[0].end >= mid_dll_cnt)
				ranges[0].end = mid_dll_cnt;
		}
	} else {
		if ((range_count > 1) && first_vl && value) {
			ranges[0].start = ranges[range_count - 1].start;
			range_count--;

			if (ranges[0].end >= mid_dll_cnt)
				ranges[0].end = mid_dll_cnt;
		}
	}

	for (i = 0; i < range_count; i++) {
		int len = (ranges[i].end - ranges[i].start + 1);

		if (len < 0) {
			if (mmc->selected_mode == SD_HS)
				len += mid_dll_cnt;
			else
				len += dll_cnt;
		}

		sdhci_debugf(mmc->in_scan == false, "good tuning phase range %d ~ %d\n",
			 ranges[i].start, ranges[i].end);

		if (longest_range_len < len) {
			longest_range_len = len;
			longest_range = i;
		}

	}
	sdhci_debugf(mmc->in_scan == false, "the best tuning step range %d-%d(the length is %d)\n",
		 ranges[longest_range].start, ranges[longest_range].end,
		 longest_range_len);

	mid_step = ranges[longest_range].start + longest_range_len / 2;
	mid_step %= dll_cnt;

	if (mmc->selected_mode == SD_HS) {
		dll_cfg &= ~(0xf << 24);
		sprd_sdhci_writel(host, dll_cfg, SDHCI_REG_32_DLL_CFG);
	} else {
		dll_cfg |= 0xf << 24;
		sprd_sdhci_writel(host, dll_cfg, SDHCI_REG_32_DLL_CFG);
	}

	if (mid_step <= dll_cnt)
		final_phase = (mid_step * 256) / dll_cnt;
	else
		final_phase = 0xff;
	if (mmc->selected_mode == SD_HS) {
		if (opcode == MMC_CMD_SET_BLOCKLEN) {
			host->dll_dly &= ~(SDHCI_CMD_DLY_MASK);
			host->dll_dly |= ((final_phase << 8) & SDHCI_CMD_DLY_MASK);
		} else {
			host->dll_dly &= ~(SDHCI_POSRD_DLY_MASK |
							SDHCI_NEGRD_DLY_MASK | SDHCI_CMD_DLY_MASK);
			host->dll_dly |= (((final_phase << 16) & SDHCI_POSRD_DLY_MASK) |
							((final_phase << 24) & SDHCI_NEGRD_DLY_MASK) |
							((final_phase << 8) & SDHCI_CMD_DLY_MASK));
		}
	} else {
		host->dll_dly &= ~(SDHCI_CMD_DLY_MASK | SDHCI_POSRD_DLY_MASK);
		host->dll_dly |= (((final_phase << 8) & SDHCI_CMD_DLY_MASK) |
				((final_phase << 16) & SDHCI_POSRD_DLY_MASK));
	}

	sdhci_debugf(mmc->in_scan == false, "the best step %d, phase 0x%02x, delay value 0x%08x\n",
		 mid_step, final_phase, host->dll_dly);
	sprd_sdhci_writel(host, host->dll_dly, SDHCI_REG_32_DLL_DLY);
	err = 0;

out:
	free(ranges);
	free(value_t);
	return err;
}
#endif

static int sprd_sdhci_card_busy(struct mmc *mmc)
{
	struct sprd_sdhci_host *host = mmc->priv;
	u32 present_state;

	/* Check whether DAT[0] is 0 */
	present_state = sprd_sdhci_readl(host, SPRD_SDHCI_PRESENT_STATE);
	return present_state & SPRD_SDHCI_DATA_0_LVL_MAST;
}

static int sprd_sdhci_voltage_switch(struct mmc *mmc)
{
	struct sprd_sdhci_host *host = mmc->priv;
	uint ret;
	struct sdio_base_info *sprd_host_info;

	sprd_host_info = get_sdcontrol_info(mmc->block_dev.dev_num);

	switch (mmc->select_voltage) {
	case MMC_SIGNAL_VOLTAGE_180:
		if (sprd_host_info->ldo_io != 0) {
			ret = regulator_set_voltage(sprd_host_info->ldo_io, 1800);
			if (ret) {
				errorf("can not switching to 1.8v signal voltage\n");
				return ret;
			}
		}
		break;
	default:
	/* fall-through */
	case MMC_SIGNAL_VOLTAGE_330:
		if (sprd_host_info->ldo_io != 0) {
			ret = regulator_set_voltage(sprd_host_info->ldo_io, 3000);
			if (ret) {
				errorf("can not switching to 3.0v signal voltage\n");
				return ret;
			}
		}
		break;
	}

	/*
	 * need reset here because voltage have changed.
	 * Otherwise the controller will not work normally.
	 * We had ever encoutered CMD2 timeout before.
	 */
	udelay(300);
	sprd_sdhci_reset(host, SPRD_SDHCI_RESET_CMD | SPRD_SDHCI_RESET_DATA);

	return 0;
}

static const struct mmc_ops sprd_sdhci_ops = {
	.send_cmd	= sprd_sdhci_send_command,
	.set_ios	= sprd_sdhci_set_ios,
	.init		= sdhci_init,
#ifdef CONFIG_MMC_SUPPORTS_TUNING
	.execute_tuning	= sprd_sdhci_execute_tuning,
#endif
	.start_signal_voltage_switch	= sprd_sdhci_voltage_switch,
	.card_busy	= sprd_sdhci_card_busy,
	.close_clock	= sprd_sdhci_close_clock,
	.set_dll_dly	= sprd_sdhci_set_dll_dly,
	.get_dll_dly	= sprd_sdhci_get_dll_dly,
	.enable_dpll_scan	= sprd_sdhc_enable_dpll_scan,
	.reset_for_scan	= sprd_sdhci_reset_for_scan,
};

int add_sprd_sdhci(struct sprd_sdhci_host *host, u32 max_clk, u32 min_clk)
{
	u32 caps1, caps2;

	if (host->name)
		host->cfg.name = host->name;

	host->cfg.ops = &sprd_sdhci_ops;

	if (max_clk)
		host->cfg.f_max = max_clk;
	else
		host->cfg.f_max = 26000000;

	if (min_clk)
		host->cfg.f_min = min_clk;
	else
		host->cfg.f_min = 100000;

	host->cfg.voltages = 0;

	caps1 = sprd_sdhci_readl(host, SPRD_SDHCI_CAPABILITIES_1);
	caps1 |= SPRD_SDHCI_CAN_VDD_300 | SPRD_SDHCI_CAN_VDD_180;
	if (caps1 & SPRD_SDHCI_CAN_VDD_330)
		host->cfg.voltages |= MMC_VDD_32_33 | MMC_VDD_33_34;

	if (caps1 & SPRD_SDHCI_CAN_VDD_300)
		host->cfg.voltages |= MMC_VDD_29_30 | MMC_VDD_30_31;

	if (caps1 & SPRD_SDHCI_CAN_VDD_180)
		host->cfg.voltages |= MMC_VDD_165_195;

	host->cfg.host_caps |= MMC_MODE_LEGACY | MMC_MODE_1BIT;
	host->cfg.host_caps |= MMC_MODE_HS | MMC_MODE_HS_52MHz | MMC_MODE_4BIT;
#ifdef CONFIG_MMC_HS200_SUPPORT
	host->cfg.host_caps |= MMC_MODE_HS200;
#endif

	caps2 = sprd_sdhci_readl(host, SPRD_SDHCI_CAPABILITIES_2);
#ifdef CONFIG_MMC_UHS_SUPPORT
	if (caps2 & SPRD_SDHCI_SUPPORT_SDR104)
		host->cfg.host_caps |= SD_MODE_SDR104;
#endif

	if (SPRD_SDHCI_GET_VERSION(host) >= SPRD_SDHCI_SPEC_300) {
		if (caps1 & SPRD_SDHCI_CAN_DO_8BIT)
			host->cfg.host_caps |= MMC_MODE_8BIT;
	}

	if (host->host_caps)
		host->cfg.host_caps |= host->host_caps;

	host->cfg.b_max = CONFIG_SYS_MMC_MAX_BLK_COUNT;

	sprd_sdhci_reset(host, SPRD_SDHCI_RESET_ALL);

	host->mmc = mmc_create(&host->cfg, host);

	if (!host->mmc) {
		errorf("%s: mmc create fail!\n", __func__);
		return -1;
	}

	sdhci_debugf(host->mmc->in_scan == false, "sprd host caps: 0x%x\n", host->cfg.host_caps);

	return 0;
}

int sprd_sdhci_init(u32 regbase, u32 max_clk, u32 min_clk, u32 quirks)
{
	struct sprd_sdhci_host *host = NULL;

	host = (struct sprd_sdhci_host *)malloc(sizeof(struct sprd_sdhci_host));
	if (!host) {
		errorf("sdh_host malloc fail!\n");
		return 1;
	}

	memset(host, 0, sizeof(*host));

	host->name = "sprd_sdhci";
	host->ioaddr = (void *)regbase;
	host->quirks = quirks;
	host->set_clock = NULL;
	host->version = sprd_sdhci_readw(host, SPRD_SDHCI_HOST_VERSION);
	dprintf(INFO,"%s: host version: %d\n", __func__, host->version);

	return add_sprd_sdhci(host, max_clk, min_clk);
}

void sdio_dump(void *addr)
{
	u32 i, regbase;

	dprintf(INFO,"*****SDIO REGISTER DUMP*****\n");
	regbase = (u32)addr;
	for (i = 0; i < 41; i++) {
		dprintf(INFO,"%16x: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n", i * 16,
			CHIP_REG_GET(regbase + i * 16 + 0),
			CHIP_REG_GET(regbase + i * 16 + 4),
			CHIP_REG_GET(regbase + i * 16 + 8),
			CHIP_REG_GET(regbase + i * 16 + 0xc));
	}
}
