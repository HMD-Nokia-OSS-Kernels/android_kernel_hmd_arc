/*
*  core.c  - unisoc mmc config
*
*  Copyright (C) 2019 Unisoc Communications Inc.
*  History:
*      2021-08-09 wenchao.chen@unisoc.com
*      Add core.c
*///ZOVERLAY_TAG_HMD_ONEIMAGE
#include <config.h>
#include <errno.h>
#include <mmc.h>
#include <malloc.h>
#include <sprd_div64.h>
#include <asm/arch/sdio_cfg.h>
#include <asm/arch/common.h>
#include <delay.h>
#include <stdio.h>
#include <lk/list.h>
#include <delay.h>
#include <part.h>
#include <storage_init.h>
#include <endian.h>
#include <sprd_regulator.h>
#include <sprd_common_rw.h>
#include <sprd_sizes.h>
#include <lk/board.h>
#ifdef CONFIG_EMMC_WP
#include "miscdata_def.h"
#endif
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define sdhci_printf(condition, x...) if (condition) {dprintf(INFO,x);}
#define sdhci_debugf(condition, x...) if (condition) {debugf(x);}
#define CID_MANFID_SAMSUNG	0x15

extern int sprd_sdhci_send_command_backstage(struct mmc *mmc,
	struct mmc_cmd *cmd, struct mmc_data *data);
extern int sprd_sdhci_query_command_backstage(struct mmc *mmc,
	struct mmc_data *data);
extern int sprd_sdhci_init(u32 regbase, u32 max_clk, u32 min_clk, u32 quirks);

static struct list_head mmc_devices;
static int cur_dev_num = -1;
static bool fixup_device_value = 0;

struct sdio_base_info *emmc_info;

int mmc_initialize(void);
int board_mmc_init(void);
static void do_preinit(void);
int sprd_host_reinit(int sdio_type);
static int mmc_select_mode_and_width_for_scan(struct mmc *sprd_mmc, uint cap);
typedef enum delay_value_type {
	CMD_DELAY,
	DATAR_DELAY,
	DATAW_DELAY
} DELAY_VALUE_TYPE_E;

static const int fbase[] = {
	10000,
	100000,
	1000000,
	10000000,
};

static const int multipliers[] = {
	0,			/* reserved */
	10,
	12,
	13,
	15,
	20,
	25,
	30,
	35,
	40,
	45,
	50,
	55,
	60,
	70,
	80,
};

#ifdef CONFIG_MMC_SUPPORTS_TUNING
static const u8 tuning_blk_pattern_8bit[] = {
	255, 255, 0, 255, 255, 255, 0, 0,
	255, 255, 204, 204, 204, 51, 204, 204,
	204, 51, 51, 204, 204, 204, 255, 255,
	255, 238, 255, 255, 255, 238, 238, 255,
	255, 255, 221, 255, 255, 255, 221, 221,
	255, 255, 255, 187, 255, 255, 255, 187,
	187, 255, 255, 255, 119, 255, 255, 255,
	119, 119, 255, 119, 187, 221, 238, 255,
	255, 255, 255, 0, 255, 255, 255, 0,
	0, 255, 255, 204, 204, 204, 51, 204,
	204, 204, 51, 51, 204, 204, 204, 255,
	255, 255, 238, 255, 255, 255, 238, 238,
	255, 255, 255, 221, 255, 255, 255, 221,
	221, 255, 255, 255, 187, 255, 255, 255,
	187, 187, 255, 255, 255, 119, 255, 255,
	255, 119, 119, 255, 119, 187, 221, 238,
};

static const u8 tuning_blk_pattern_4bit[] = {
	255, 15, 255, 0, 255, 204, 195, 204,
	195, 60, 204, 255, 254, 255, 254, 239,
	255, 223, 255, 221, 255, 251, 255, 251,
	191, 255, 127, 255, 119, 247, 189, 239,
	255, 240, 255, 240, 15, 252, 204, 60,
	204, 51, 204, 207, 255, 239, 255, 238,
	255, 253, 255, 253, 223, 255, 191, 255,
	187, 255, 247, 255, 247, 127, 123, 222,
};
#endif

struct fixup_mmc_device {
	unsigned int manfid;
	char name[8];
};

/* these devices currently have problems in cmdq mode */
static struct fixup_mmc_device fixup_device[] = {
	{CID_MANFID_SAMSUNG, "GX6BAB"},
	{CID_MANFID_SAMSUNG, "GX6BMB"},
	{CID_MANFID_SAMSUNG, "QE63BB"},
	{CID_MANFID_SAMSUNG, "QE63MB"},
};

int mmc_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd,
	struct mmc_data *data)
{
	int ret;

#ifdef CONFIG_MMC_TRACE
	int i;
	int j;
	u8 *ptr;

	dprintf(INFO,"CMD_SEND:%d\n", cmd->cmdidx);
	dprintf(INFO,"\t\tARG\t\t\t 0x%08X\n", cmd->cmdarg);
#endif

	ret = mmc->cfg->ops->send_cmd(mmc, cmd, data);

#ifdef CONFIG_MMC_TRACE
	switch (cmd->resp_type) {
	case MMC_RSP_NONE:
		dprintf(INFO,"MMC_RSP_NONE\n");
		break;
	case MMC_RSP_R1:
		dprintf(INFO,"MMC_RSP_R1,5,6,7 0x%08X\n", cmd->response[0]);
		break;
	case MMC_RSP_R1b:
		dprintf(INFO,"MMC_RSP_R1b 0x%08X\n", cmd->response[0]);
		break;
	case MMC_RSP_R2:
		dprintf(INFO,"MMC_RSP_R2 0x%08X\n", cmd->response[0]);
		dprintf(INFO,"           0x%08X\n", cmd->response[1]);
		dprintf(INFO,"           0x%08X\n", cmd->response[2]);
		dprintf(INFO,"           0x%08X\n", cmd->response[3]);
		dprintf(INFO,"\n");
		dprintf(INFO,"DUMPING DATA\n");
		for (i = 0; i < 4; i++) {
			dprintf(INFO,"%03d - ", i * 4);
			ptr = (u8 *)&cmd->response[i];
			ptr += 3;
			for (j = 0; j < 4; j++)
				dprintf(INFO,"%02X ", *ptr--);
			dprintf(INFO,"\n");
		}
		break;
	case MMC_RSP_R3:
		dprintf(INFO,"MMC_RSP_R3,4 0x%08X\n", cmd->response[0]);
		break;
	default:
		dprintf(INFO,"ERROR MMC rsp not supported\n");
		break;
	}
#endif

	return ret;
}

int mmc_send_status(struct mmc *sprd_mmc, int timeout)
{
	struct mmc_cmd sprd_cmd = {0};
	int err;
	int retries = 20;

	sprd_cmd.cmdidx = MMC_CMD_SEND_STATUS;
	sprd_cmd.resp_type = MMC_RSP_R1;
	sprd_cmd.cmdarg = sprd_mmc->rca << 16;

	for(;;) {
		err = mmc_send_cmd(sprd_mmc, &sprd_cmd, NULL);
		if (!err) {
			if ((MMC_STATUS_RDY_FOR_DATA & sprd_cmd.response[0])
				&& ((sprd_cmd.response[0] & MMC_STATUS_CURR_STATE) != MMC_STATE_PRG))
				break;
			else if (MMC_STATUS_MASK & sprd_cmd.response[0]) {
				errorf("Status Error: 0x%08x\n", sprd_cmd.response[0]);
				return COMM_ERR;
			}
		} else if (--retries < 0) {
			errorf("Card retries fail\n");
			return err;
		}

		if (timeout-- <= 0) {
			errorf("Timeout waiting card ready\n");
			return TIMEOUT;
		}

		mdelay(1);
	}

	if (MMC_STATUS_SWITCH_ERROR & sprd_cmd.response[0])
		return SWITCH_ERR;

	return 0;
}

int mmc_send_status_for_scan(struct mmc *sprd_mmc)
{
	struct mmc_cmd sprd_cmd = {0};
	int err;

	sprd_cmd.cmdidx = MMC_CMD_SEND_STATUS;
	sprd_cmd.resp_type = MMC_RSP_R1;
	sprd_cmd.cmdarg = sprd_mmc->rca << 16;


	err = mmc_send_cmd(sprd_mmc, &sprd_cmd, NULL);
	if (err)
		return SWITCH_ERR;
	if (MMC_STATUS_MASK & sprd_cmd.response[0])
		return COMM_ERR;
	if (MMC_STATUS_SWITCH_ERROR & sprd_cmd.response[0])
		return SWITCH_ERR;

	return 0;
}

struct mmc *find_mmc_device(int dev_num)
{
	struct mmc *m;
	struct list_head *mmc_entry;

	list_for_each(mmc_entry, &mmc_devices) {
		m = list_entry(mmc_entry, struct mmc, link);

		if (m->block_dev.dev_num == dev_num)
			return m;
	}

	errorf("%s: Don't find %s card.\n", __func__, dev_num ? "sd" : "emmc");

	return NULL;
}

int mmc_card_set_blocklen(struct mmc *mmc, int len)
{
	int ret;
	struct mmc_cmd cmd = {0};

	if (mmc == NULL)
		return -1;

	cmd.cmdarg = len;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdidx = MMC_CMD_SET_BLOCKLEN;

	ret = mmc_send_cmd(mmc, &cmd, NULL);

	return ret;
}

static ulong mmc_erase_t(struct mmc *mmc, ulong start,
			 lbaint_t blkcnt, uint erase_type)
{
	struct mmc_cmd cmd;
	ulong end;
	int ret, start_cmd, end_cmd;

	if (!mmc)
		return -1;

	if (IS_SD(mmc) == SD_VERSION_SD) {
		start_cmd = SD_CMD_ERASE_WR_BLK_START;
		end_cmd = SD_CMD_ERASE_WR_BLK_END;
	} else {
		start_cmd = MMC_CMD_ERASE_GROUP_START;
		end_cmd = MMC_CMD_ERASE_GROUP_END;
	}

	if (mmc->high_capacity) {
		end = start + blkcnt - 1;
	} else {
		end = (start + blkcnt - 1) * mmc->write_bl_len;
		start *= mmc->write_bl_len;
	}

	cmd.cmdarg = start;
	cmd.cmdidx = start_cmd;
	cmd.resp_type = MMC_RSP_R1;
	ret = mmc_send_cmd(mmc, &cmd, NULL);
	if (ret)
		goto err;

	cmd.cmdarg = end;
	cmd.cmdidx = end_cmd;
	cmd.resp_type = MMC_RSP_R1;
	ret = mmc_send_cmd(mmc, &cmd, NULL);
	if (ret)
		goto err;

	cmd.cmdarg = erase_type;
	cmd.cmdidx = MMC_CMD_ERASE;
	cmd.resp_type = MMC_RSP_R1b;
	ret = mmc_send_cmd(mmc, &cmd, NULL);
	if (ret)
		goto err;

	return 0;

err:
	puts("mmc erase failed\n");
	return ret;
}

static int mmc_send_stop_cmd(struct mmc *mmc)
{
	struct mmc_cmd cmd;

	cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_R1b;

	return mmc_send_cmd(mmc, &cmd, NULL);
}

static ulong mmc_write_blocks(struct mmc *sprd_mmc, lbaint_t start,
		lbaint_t blkcnt, const void *src)
{
	struct mmc_cmd sprd_cmd = {0};
	struct mmc_data sprd_data = {{NULL}, 0, 0, 0};
	int timeout = 1000;

	if (sprd_mmc == NULL)
		return 0;

	if (blkcnt == 0)
		return 0;

	if ((start + blkcnt) > sprd_mmc->block_dev.lba) {
		errorf("MMC: block number 0x" LBAF " exceeds max(0x" LBAF ")\n",
		       start + blkcnt, sprd_mmc->block_dev.lba);
		return 0;
	}

	if (blkcnt == 1)
		sprd_cmd.cmdidx = MMC_CMD_WRITE_SINGLE_BLOCK;
	else
		sprd_cmd.cmdidx = MMC_CMD_WRITE_MULTIPLE_BLOCK;

	if (sprd_mmc->high_capacity)
		sprd_cmd.cmdarg = start;
	else
		sprd_cmd.cmdarg = start * sprd_mmc->write_bl_len;

	sprd_cmd.resp_type = MMC_RSP_R1;

	sprd_data.src = src;
	sprd_data.blocks = blkcnt;
	sprd_data.blocksize = sprd_mmc->write_bl_len;
	sprd_data.flags = MMC_DATA_WRITE;
	if (mmc_send_cmd(sprd_mmc, &sprd_cmd, &sprd_data)) {
		errorf("Write data failed\n");
		return 0;
	}

	if (blkcnt > 1) {
		if (mmc_send_stop_cmd(sprd_mmc)) {
			errorf("mmc fail to send stop cmd\n");
			return 0;
		}
	}

	if (mmc_send_status(sprd_mmc, timeout))
		return 0;

	return blkcnt;
}

static uint max_erase_size = 0x80000000;//2G Byte
unsigned long mmc_berase(int dev_num, lbaint_t start, lbaint_t blkcnt)
{
	int err;
	lbaint_t blk = 0, blk_r = 0;
	int timeout = 1000;
	struct mmc *mmc = find_mmc_device(dev_num);
	lbaint_t max_erase_blk = 0;

	if (!mmc)
		return -1;

	max_erase_blk = max_erase_size / mmc->read_bl_len;

	if (start % mmc->erase_grp_size) {
		blk_r = (blkcnt < (mmc->erase_grp_size -
			start % mmc->erase_grp_size)) ? blkcnt :
			(mmc->erase_grp_size - start % mmc->erase_grp_size);
		err = mmc_erase_t(mmc, start, blk_r, 1);
		if (err)
			return -1;
		blk += blk_r;
		/* Waiting for the ready status */
		if (mmc_send_status(mmc, timeout))
			return 0;
	}

	while (blk < blkcnt) {
		blk_r = ((blkcnt - blk) >= mmc->erase_grp_size) ?
			((blkcnt - blk) & ~(mmc->erase_grp_size - 1)) : (blkcnt - blk);

		if ((blkcnt - blk) >= mmc->erase_grp_size) {
			if (blk_r > max_erase_blk) {
				err = mmc_erase_t(mmc, start + blk, max_erase_blk, 0);
				blk_r = max_erase_blk;
			} else {
				err = mmc_erase_t(mmc, start + blk, blk_r, 0);
			}
		} else {
			err = mmc_erase_t(mmc, start + blk, blk_r, 1);
		}

		if (err)
			break;

		blk += blk_r;

		if (mmc_send_status(mmc, timeout))
			return 0;
	}

	return blk;
}

static int mmc_switch(struct mmc *mmc, u8 set, u8 index, u8 value)
{
	struct mmc_cmd cmd;
	int timeout = 10000;
	int ret;

	cmd.cmdidx = MMC_CMD_SWITCH;
	cmd.resp_type = MMC_RSP_R1b;
	cmd.cmdarg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
			(index << 16) | (value << 8);

	ret = mmc_send_cmd(mmc, &cmd, NULL);
	if (index == 171)
		debugf("%s: send EXT_CSD_USER_WP return %d\n", __func__, ret);

	/* Waiting for the ready status */
	if (!ret)
		ret = mmc_send_status(mmc, timeout);
	if (index == 171)
		debugf("%s: send status return %d\n", __func__, ret);

	return ret;
}

static int mmc_send_ext_csd(struct mmc *mmc, u8 *ext_csd)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	int err;

	/* Get the Card Status Register */
	cmd.cmdidx = MMC_CMD_SEND_EXT_CSD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;

	data.dest = (char *)ext_csd;
	data.blocks = 1;
	data.blocksize = MMC_MAX_BLOCK_LEN;
	data.flags = MMC_DATA_READ;

	err = mmc_send_cmd(mmc, &cmd, &data);
	if (err)
		errorf("mmc send ext csd error:%d\n", err);

	return err;
}

int mmc_get_wp_grp_size(void)
{
	struct mmc *mmc = find_mmc_device(EMMC);
	if (mmc == NULL)
		return 0;

	return mmc->hc_wp_grp_size;
}

ulong mmc_bwrite(int dev_num, lbaint_t start, lbaint_t blkcnt,
	const void *src)
{
	lbaint_t cur, blocks_todo = blkcnt;

	struct mmc *mmc = find_mmc_device(dev_num);
	if (mmc == NULL)
		return 0;

	if (mmc_card_set_blocklen(mmc, mmc->write_bl_len))
		return 0;

	do {
		cur = (blocks_todo > mmc->cfg->b_max) ? mmc->cfg->b_max : blocks_todo;
		if (mmc_write_blocks(mmc, start, cur, src) != cur)
			return 0;
		blocks_todo -= cur;
		start += cur;
		src += cur * mmc->write_bl_len;
	} while (blocks_todo > 0);

	return blkcnt;
}
//add by jinqiang for https://hmdpim.atlassian.net/browse/CMT-262 @20250523
#ifdef CONFIG_EMMC_WP
int g_part_protected = DISABLE_WRITE_PROTECT;
void part_protect_init(int enable)
{
	if (enable)
		g_part_protected = ENABLE_WRITE_PROTECT;
	else
		g_part_protected = DISABLE_WRITE_PROTECT;
}

ulong mmc_set_pwr_wp(int dev_num, lbaint_t start, int grp_cnt)
{
	int err = 0;
	struct mmc_cmd cmd;
	struct mmc_data data;
	int i;
	lbaint_t cur_start = start;
	ALLOC_CACHE_ALIGN_BUFFER(u8, wp_status, 4);
	ALLOC_CACHE_ALIGN_BUFFER(u8, wp_type, 8);
	unsigned long long actual_type = 0, expected_type = 0;
	uint wp_bits_sts = 0;

	struct mmc *mmc = find_mmc_device(dev_num);
	if (!mmc) {
		errorf("%s: no emmc device\n", __func__);
		return -1;
	}

	//if (!mmc->wp_enable) {
	//	errorf("emmc power-up write protection never be eanbled\n");
	//	return -1;
	//}

	debugf("%s: write protection start addr: 0x" LBAF ", group count: %d\n",
		__func__, cur_start, grp_cnt);
	for (i = 0; i < grp_cnt; i++) {
		cmd.cmdidx = MMC_CMD_SET_WR_PROT;
		cmd.cmdarg = cur_start;
		cmd.resp_type = MMC_RSP_R1b;
		if (mmc_send_cmd(mmc, &cmd, NULL))
			errorf("%s: send CMD%d fail\n", __func__, cmd.cmdidx);

		cur_start += mmc->hc_wp_grp_size;
	}

	cmd.cmdidx = MMC_CMD_SEND_WR_PROT;
	cmd.cmdarg = start;
	cmd.resp_type = MMC_RSP_R1;
	data.dest = (char *)wp_status;
	data.blocks = 1;
	data.blocksize = 4;
	data.flags = MMC_DATA_READ;
	err = mmc_send_cmd(mmc, &cmd, &data);
	if (err)
		errorf("%s: send CMD%d fail, return %d\n",
		       __func__, cmd.cmdidx, err);
	for (i = 0; i < 4; i++)
		wp_bits_sts = (wp_bits_sts << 8) + (u8)wp_status[i];
	dprintf(INFO,"%s: the status of write protection bits: 0x%x\n",
		__func__, wp_bits_sts);

	cmd.cmdidx = MMC_CMD_SEND_WR_PROT_TYPE;
	cmd.cmdarg = start;
	cmd.resp_type = MMC_RSP_R1;
	data.dest = (char *)wp_type;
	data.blocks = 1;
	data.blocksize = 8;
	data.flags = MMC_DATA_READ;
	err = mmc_send_cmd(mmc, &cmd, &data);
	if (err)
		errorf("%s: send CMD%d fail, return %d\n",
			__func__, cmd.cmdidx, err);

	for (i = 0; i < 8; i++)
		actual_type = (actual_type << 8) + (u8)wp_type[i];
	dprintf(INFO,"%s: the really type of write protection grout: 0x%llx\n",
		__func__, actual_type);

	for (i = 0; i < grp_cnt; i++)
		expected_type = (expected_type << 2) + 0x2;
	dprintf(INFO,"%s: the expected type of write protection group: 0x%llx\n",
		__func__, expected_type);

	if (actual_type != expected_type) {
		errorf("%s: power-on write protection set failed\n", __func__);
		return -1;
	}
	g_part_protected=ENABLE_WRITE_PROTECT;

	return 0;
}
#endif

static ulong mmc_write_blocks_backstage(struct mmc *mmc,
	lbaint_t start, lbaint_t blkcnt, const void *src)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	if ((start + blkcnt) > mmc->block_dev.lba) {
		errorf("MMC: block number 0x" LBAF " exceeds max(0x" LBAF ")\n",
		       start + blkcnt, mmc->block_dev.lba);
		return 0;
	}
	if (blkcnt == 0)
		return 0;
	else if (blkcnt == 1)
		cmd.cmdidx = MMC_CMD_WRITE_SINGLE_BLOCK;
	else
		cmd.cmdidx = MMC_CMD_WRITE_MULTIPLE_BLOCK;
	if (mmc->high_capacity)
		cmd.cmdarg = start;
	else
		cmd.cmdarg = start * mmc->write_bl_len;

	cmd.resp_type = MMC_RSP_R1;

	data.src = src;
	data.blocks = blkcnt;
	data.blocksize = mmc->write_bl_len;
	data.flags = MMC_DATA_WRITE;
	if (sprd_sdhci_send_command_backstage(mmc, &cmd, &data)) {
		errorf("mmc backstage write failed\n");
		return 0;
	}
	return blkcnt;
}

ulong mmc_query_bwrite_backstage(int dev_num, lbaint_t blkcnt,
		const void *src)
{
	int timeout = 1000;
	struct mmc *mmc = find_mmc_device(dev_num);

	if (blkcnt == 0)
		return 0;

	if (!mmc)
		return 0;

	if (mmc_send_status(mmc, timeout)) {
		errorf("mmc failed to send status cmd\n");
		return 0;
	}

	return blkcnt;
}

ulong mmc_query_bread_backstage(int dev_num, lbaint_t blkcnt,
		const void *src)
{
	int timeout = 1000;
	struct mmc *mmc = find_mmc_device(dev_num);
	struct mmc_data data;
	struct mmc_cmd cmd;

	if (blkcnt == 0)
		return 0;

	if (!mmc)
		return 0;

	data.src = src;
	data.blocks = blkcnt;
	data.blocksize = mmc->read_bl_len;
	data.flags = MMC_DATA_READ;
	if (sprd_sdhci_query_command_backstage(mmc, &data)) {
		errorf("mmc query write result failed\n");
		return 0;
	}
	if (!mmc_host_is_spi(mmc) && blkcnt > 1) {
		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_RSP_R1b;
		if (mmc_send_cmd(mmc, &cmd, NULL)) {
			errorf("mmc fail to send stop cmd\n");
			return 0;
		}
	}

	if (mmc_send_status(mmc, timeout)) {
		errorf("mmc failed to send status cmd\n");
		return 0;
	}

	return blkcnt;
}

static lbaint_t mmc_read_blocks(struct mmc *mmc, void *dst, lbaint_t start,
			   lbaint_t blkcnt)
{
	struct mmc_cmd cmd;
	struct mmc_data data;

	if (mmc == NULL)
		return 0;

	if (blkcnt == 0)
		return 0;

	if (mmc->high_capacity)
		cmd.cmdarg = start;
	else
		cmd.cmdarg = start * mmc->read_bl_len;

	if (blkcnt > 1)
		cmd.cmdidx = MMC_CMD_READ_MULTIPLE_BLOCK;
	else
		cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;

	data.dest = dst;
	data.blocks = blkcnt;
	data.blocksize = mmc->read_bl_len;
	data.flags = MMC_DATA_READ;

	cmd.resp_type = MMC_RSP_R1;

	if (mmc_send_cmd(mmc, &cmd, &data)) {
		errorf("mmc send cmd fail\n");
		return 0;
	}

	if (blkcnt > 1) {
		cmd.cmdarg = 0;
		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.resp_type = MMC_RSP_R1b;

		if (mmc_send_cmd(mmc, &cmd, NULL)) {
			errorf("mmc fail to send stop cmd\n");
			return 0;
		}
	}

	return blkcnt;
}

ulong mmc_bwrite_backstage(int dev_num, lbaint_t start,
	lbaint_t blkcnt, const void *src)
{
	lbaint_t cur = blkcnt;

	struct mmc *mmc = find_mmc_device(dev_num);
	if (!mmc)
		return 0;

	if (mmc_card_set_blocklen(mmc, mmc->write_bl_len))
		return 0;
#ifndef SPRD_SPARSE_SUPER_SPEEDUP
	if (blkcnt > mmc->cfg->b_max) {
		errorf("%s can not support more than <%d> blocks\n",
		       __func__, mmc->cfg->b_max);
		return 0;
	}
#endif
	if (mmc_write_blocks_backstage(mmc, start, blkcnt, src) != cur)
		return 0;

	return blkcnt;
}

static int _mmc_go_idle(struct mmc *mmc)
{
	int err;
	struct mmc_cmd cmd = {0};

	if (!mmc)
		return -1;

	mdelay(1);

	cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
	cmd.resp_type = MMC_RSP_NONE;
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;

	mdelay(2);

	return 0;
}

static int _mmc_send_op_cond_iter(struct mmc *mmc, int use_arg)
{
	struct mmc_cmd cmd ={0};
	int err;

	cmd.cmdidx = MMC_CMD_SEND_OP_COND;
	cmd.resp_type = MMC_RSP_R3;
	if (use_arg)
		cmd.cmdarg = OCR_HCS | (mmc->cfg->voltages &
			(mmc->ocr & OCR_VOLTAGE_MASK)) | (mmc->ocr & OCR_ACCESS_MODE);

	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;

	mmc->ocr = cmd.response[0];
	return 0;
}

static ulong mmc_bread(int dev_num, lbaint_t start, lbaint_t blkcnt,
			void *dst)
{
	int ret;
	lbaint_t cur;
	lbaint_t blocks_todo = blkcnt;
	struct mmc *mmc = find_mmc_device(dev_num);

	if (blkcnt == 0)
		return 0;

	if (mmc == NULL)
		return 0;

	if ((start + blkcnt) > mmc->block_dev.lba) {
		errorf("MMC: block number 0x" LBAF " exceeds max(0x" LBAF ")\n",
		       start + blkcnt, mmc->block_dev.lba);
		return 0;
	}

	ret = mmc_card_set_blocklen(mmc, mmc->read_bl_len);
	if (ret)
		return 0;

	do {
		cur = (blocks_todo > mmc->cfg->b_max) ? mmc->cfg->b_max : blocks_todo;
		if (mmc_read_blocks(mmc, dst, start, cur) != cur)
			return 0;
		blocks_todo -= cur;
		start += cur;
		dst += cur * mmc->read_bl_len;
	} while (blocks_todo > 0);

	return blkcnt;
}

static int mmc_send_cmd_app_cmd(struct mmc *mmc)
{
	struct mmc_cmd cmd = {0};

	cmd.cmdidx = MMC_CMD_APP_CMD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;

	return mmc_send_cmd(mmc, &cmd, NULL);
}

#ifdef CONFIG_MMC_UHS_SUPPORT
static int mmc_set_signal_voltage(struct mmc *sprd_mmc,
						enum mmc_voltage signal_voltage)
{
	int err = 0;
	int old_signal_voltage = sprd_mmc->select_voltage;

	sprd_mmc->select_voltage = signal_voltage;
	if (sprd_mmc->cfg->ops->start_signal_voltage_switch)
		err = sprd_mmc->cfg->ops->start_signal_voltage_switch(sprd_mmc);

	if (err)
		sprd_mmc->select_voltage = old_signal_voltage;

	return err;
}

static int mmc_switch_voltage(struct mmc *sprd_mmc,
						enum mmc_voltage signal_voltage)
{
	struct mmc_cmd sprd_cmd;
	int err = 0;

	/*
	 * Send CMD11 only if the request is to switch the card to
	 * 1.8V signalling.
	 */
	if (signal_voltage == MMC_SIGNAL_VOLTAGE_330)
		return mmc_set_signal_voltage(sprd_mmc, signal_voltage);

	sprd_cmd.cmdidx = SD_CMD_SWITCH_UHS18V;
	sprd_cmd.cmdarg = 0;
	sprd_cmd.resp_type = MMC_RSP_R1;

	err = mmc_send_cmd(sprd_mmc, &sprd_cmd, NULL);
	if (err) {
		errorf("mmc switch to uhs18v(cmd11) error:%d\n", err);
		return err;
	}

	if (sprd_cmd.response[0] & MMC_STATUS_ERROR)
		return -EIO;

	/*
	 * The card should drive cmd and dat[0:3] low immediately
	 * after the response of cmd11, but wait 100 us to be sure
	 */
	udelay(100);
	if (0 != sprd_mmc->cfg->ops->card_busy(sprd_mmc)) {
		errorf("the dat[0:3] is high, pls check\n");
		return -1;
	}

	/*
	 * During a signal voltage level switch, the clock must be gated
	 * for 5 ms according to the SD spec
	 */
	sprd_mmc->cfg->ops->close_clock(sprd_mmc);

	err = mmc_set_signal_voltage(sprd_mmc, signal_voltage);
	if (err)
		return err;

	/* Keep clock gated for at least 10 ms, though spec only says 5 ms */
	mdelay(10);
	mmc_set_clock(sprd_mmc, sprd_mmc->clock);

	/*
	 * Failure to switch is indicated by the card holding
	 * dat[0:3] low. Wait for at least 1 ms according to spec
	 */
	mdelay(1);
	if (0 == sprd_mmc->cfg->ops->card_busy(sprd_mmc)) {
		errorf("the dat[0:3] is low, pls check\n");
		return -1;
	}

	return 0;
}
#endif

static int _sd_send_op_cond(struct mmc *sprd_mmc, int sprd_uhs)
{
	int timeout = 1000;
	int err;
	struct mmc_cmd sprd_cmd = {0};

	if (!sprd_mmc)
		return -1;

	for(;;) {
		err = mmc_send_cmd_app_cmd(sprd_mmc);
		if (err) {
			errorf("mmc send app cmd(emmc ignore) error:%d\n", err);
			return err;
		}

		sprd_cmd.cmdidx = SD_CMD_APP_SEND_OP_COND;
		sprd_cmd.resp_type = MMC_RSP_R3;
		sprd_cmd.cmdarg = sprd_mmc->cfg->voltages & 0xff8000;

		if (sprd_mmc->version == SD_VERSION_2)
			sprd_cmd.cmdarg |= OCR_HCS;

		if (sprd_uhs)
			sprd_cmd.cmdarg |= OCR_S18R;

		err = mmc_send_cmd(sprd_mmc, &sprd_cmd, NULL);
		if (err) {
			errorf("mmc get ocr error(acmd41) error:%d\n", err);
			return err;
		}

		if (OCR_BUSY & sprd_cmd.response[0])
			break;

		if (timeout-- <= 0)
			return TIMEOUT;

		mdelay(1);
	}

	sprd_mmc->ocr = sprd_cmd.response[0];

#ifdef CONFIG_MMC_UHS_SUPPORT
	if (sprd_uhs && (sprd_mmc->ocr & 0x41000000) == 0x41000000) {
		err = mmc_switch_voltage(sprd_mmc, MMC_SIGNAL_VOLTAGE_180);
		if (err) {
			errorf("switch signal voltage error\n");
			return err;
		}
	}
#endif

	sprd_mmc->high_capacity = ((sprd_mmc->ocr & OCR_HCS) == OCR_HCS);
	sprd_mmc->rca = 0;

	if (sprd_mmc->version != SD_VERSION_2)
		sprd_mmc->version = SD_VERSION_1_0;

	return 0;
}

static int mmc_send_op_cond(struct mmc *mmc)
{
	int err, i;

	_mmc_go_idle(mmc);

	for (i = 0; i < 100; i++) {
		err = _mmc_send_op_cond_iter(mmc, i != 0);
		if (err)
			return err;

		if (mmc->ocr & OCR_BUSY)
			break;

		mdelay(10);
	}

	mmc->op_cond_pending = 1;

	return 0;
}

static int _mmc_complete_op_cond(struct mmc *mmc)
{
	struct mmc_cmd cmd = {0};
	ulong timeout = 1000;
	uint start;
	int err;

	mmc->op_cond_pending = 0;
	if (!(mmc->ocr & OCR_BUSY)) {
		start = get_timer(0);
		while (1) {
			err = _mmc_send_op_cond_iter(mmc, 1);
			if (err)
				return err;
			if (mmc->ocr & OCR_BUSY)
				break;

			if (get_timer(start) > timeout)
				return UNUSABLE_ERR;

			udelay(100);
		}
	}

	mmc->high_capacity = ((mmc->ocr & OCR_HCS) == OCR_HCS);
	mmc->rca = 1;
	mmc->version = MMC_VERSION_UNKNOWN;

	return 0;
}

static int _mmc_change_freq(struct mmc *mmc)
{
	ALLOC_CACHE_ALIGN_BUFFER(u8, ext_csd, MMC_MAX_BLOCK_LEN);
	char cardtype;
	int err;

	mmc->card_caps = 0;

	if (mmc_host_is_spi(mmc))
		return 0;

	if (mmc->version < MMC_VERSION_4)
		return 0;

	mmc->card_caps |= MMC_MODE_4BIT | MMC_MODE_8BIT;

#ifndef CONFIG_FPGA
	err = mmc_send_ext_csd(mmc, ext_csd);
#else
	int temp = 0;
	/* check  ext_csd version and capacity */
	for (temp = 0; temp < 10; temp++) {
		err = mmc_send_ext_csd(mmc, ext_csd);
		if (!err)
			break;
	}

	if (temp == 10) {
		errorf("get ext_csd error\n");
		err = -1;
	}
#endif
	if (err)
		return err;

	cardtype = ext_csd[EXT_CSD_CARD_TYPE] & 0xf;

	err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_HS_TIMING, 1);

	if (err)
		return err == SWITCH_ERR ? 0 : err;

	/* Now check to see that it worked */
#ifndef CONFIG_FPGA
	err = mmc_send_ext_csd(mmc, ext_csd);
#else
	/* check ext_csd version and capacity */
	for (temp = 0; temp < 10; temp++) {
		err = mmc_send_ext_csd(mmc, ext_csd);
		if (!err)
			break;
	}

	if (temp == 10) {
		errorf("%s get ext_csd error\n", __func__);
		err = -1;
	}
#endif
	if (err)
		return err;

	if (ext_csd[EXT_CSD_HS_TIMING] == 0)
		return 0;

	if (cardtype & MMC_EXT_CSD_CARD_TYPE_52) {
		mmc->card_caps |= MMC_MODE_HS_52MHz | MMC_MODE_HS;
	} else {
		mmc->card_caps |= MMC_MODE_HS;
	}
	return 0;
}

#define USER_PARTITION 0
#define BOOT_PARTITION_1 1
#define BOOT_PARTITION_2 2
#define RPMB_PARTITION_NUM_3 3
#define GP_PARTITION_NUM_4 4
#define GP_PARTITION_NUM_5 5
#define GP_PARTITION_NUM_6 6
#define GP_PARTITION_NUM_7 7

static void mmc_set_blk_dev_lba(struct mmc *mmc)
{
	mmc->block_dev.lba = lldiv(mmc->capacity, mmc->read_bl_len);
}

static int sprd_mmc_set_capacity(struct mmc *mmc, int part_num)
{
	sdhci_printf(!mmc->in_scan, "capacity -user: 0x%llx, -boot: 0x%llx, -rpmb: 0x%llx\n",
	       mmc->capacity_user, mmc->capacity_boot, mmc->capacity_rpmb);

	switch (part_num) {
	case USER_PARTITION:
		mmc->capacity = mmc->capacity_user;
		break;
	case BOOT_PARTITION_1:
	case BOOT_PARTITION_2:
		mmc->capacity = mmc->capacity_boot;
		break;
	case RPMB_PARTITION_NUM_3:
		mmc->capacity = mmc->capacity_rpmb;
		break;
	case GP_PARTITION_NUM_4:
	case GP_PARTITION_NUM_5:
	case GP_PARTITION_NUM_6:
	case GP_PARTITION_NUM_7:
		mmc->capacity = mmc->capacity_gp[part_num - 4];
		break;
	default:
		return -1;
	}

	mmc_set_blk_dev_lba(mmc);

	return 0;
}

int sprd_mmc_select_hwpart(int dev_num, int hwpart)
{
	int ret;
	struct mmc *mmc = find_mmc_device(dev_num);

	if (mmc == NULL)
		return -ENODEV;

	if (mmc->part_config == MMCPART_NOAVAILABLE) {
		errorf("Card doesn't support part_switch\n");
		return -EOPNOTSUPP;
	}

	if (mmc->part_num == hwpart)
		return 0;

	ret = mmc_switch_part(dev_num, hwpart);
	if (ret) {
		errorf("Switch part error\n");
		return ret;
	}

	mmc->part_num = hwpart;

	return 0;
}

int mmc_switch_part(int dev_num, unsigned int part_num)
{
	int ret;
	struct mmc *sprd_mmc = find_mmc_device(dev_num);

	if (sprd_mmc == NULL)
		return -1;

	ret = mmc_switch(sprd_mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_PART_CONF,
			(sprd_mmc->part_config & ~PART_ACCESS_MASK) |
			(part_num & PART_ACCESS_MASK) |
			EXT_CSD_BOOT_PARTITION_ENABLE);

	if ((ret == 0) || ((ret == -ENODEV) && (part_num == 0)))
		ret = sprd_mmc_set_capacity(sprd_mmc, part_num);

	return ret;
}

static int _sd_switch(struct mmc *mmc, int mode, int group, u8 value,
		u8 *resp)
{
	struct mmc_cmd sprd_cmd = {0};
	struct mmc_data sprd_data = {{NULL}, 0, 0, 0};

	sprd_cmd.cmdarg = (mode << 31) | 0xffffff;
	sprd_cmd.cmdarg &= ~(0xf << (group * 4));
	sprd_cmd.cmdarg |= value << (group * 4);
	sprd_cmd.cmdidx = SD_CMD_SWITCH_FUNC;
	sprd_cmd.resp_type = MMC_RSP_R1;

	sprd_data.blocks = 1;
	sprd_data.flags = MMC_DATA_READ;
	sprd_data.dest = (char *)resp;
	sprd_data.blocksize = 64;

	return mmc_send_cmd(mmc, &sprd_cmd, &sprd_data);
}

static int _sd_change_freq(struct mmc *sprd_mmc)
{
	int err;
	int timeout;
	struct mmc_cmd sprd_cmd;
	struct mmc_data sprd_data;
	ALLOC_CACHE_ALIGN_BUFFER(uint, scr, 2);
	ALLOC_CACHE_ALIGN_BUFFER(uint, switch_status, 16);

	sprd_mmc->card_caps = 0;
	sprd_cmd.cmdidx = MMC_CMD_APP_CMD;
	sprd_cmd.resp_type = MMC_RSP_R1;
	sprd_cmd.cmdarg = sprd_mmc->rca << 16;
	err = mmc_send_cmd(sprd_mmc, &sprd_cmd, NULL);
	if (err) {
		errorf("send app cmd(cmd55) error:%d\n", err);
		return err;
	}

	timeout = 3;

	sprd_cmd.resp_type = MMC_RSP_R1;
	sprd_cmd.cmdarg = 0;
	sprd_cmd.cmdidx = SD_CMD_APP_SEND_SCR;
retry_scr:
	sprd_data.blocksize = 8;
	sprd_data.blocks = 1;
	sprd_data.flags = MMC_DATA_READ;
	sprd_data.dest = (char *)scr;
	err = mmc_send_cmd(sprd_mmc, &sprd_cmd, &sprd_data);
	if (err) {
		if (timeout--)
			goto retry_scr;
		errorf("mmc get scr(acmd51) error:%d\n", err);
		return err;
	}

	sprd_mmc->scr[0] = BE32(scr[0]);
	sprd_mmc->scr[1] = BE32(scr[1]);

	switch ((sprd_mmc->scr[0] >> 24) & 0xf) {
	case 0:
		sprd_mmc->version = SD_VERSION_1_0;
		break;
	case 1:
		sprd_mmc->version = SD_VERSION_1_10;
		break;
	case 2:
		sprd_mmc->version = SD_VERSION_2;
		if ((sprd_mmc->scr[0] >> 15) & 0x1)
			sprd_mmc->version = SD_VERSION_3;
		break;
	default:
		sprd_mmc->version = SD_VERSION_1_0;
		break;
	}

	if (SD_DATA_4BIT & sprd_mmc->scr[0])
		sprd_mmc->card_caps |= MMC_MODE_4BIT;

	if (sprd_mmc->version == SD_VERSION_1_0)
		return 0;

	timeout = 4;
	while (timeout--) {
		err = _sd_switch(sprd_mmc, SD_SWITCH_CHECK, 0, 1,
				(u8 *)switch_status);
		if (err)
			return err;

		if (!(BE32(switch_status[7]) & SD_HIGHSPEED_BUSY))
			break;
	}

	if (!(BE32(switch_status[3]) & SD_HIGHSPEED_SUPPORTED))
		return 0;

	if (!((sprd_mmc->cfg->host_caps & MMC_MODE_HS_52MHz) &&
	      (sprd_mmc->cfg->host_caps & MMC_MODE_HS)))
		return 0;

	err = _sd_switch(sprd_mmc, SD_SWITCH_SWITCH, 0, 1, (u8 *)switch_status);
	if (err)
		return err;

	if ((BE32(switch_status[4]) & 0x0f000000) == 0x01000000)
		sprd_mmc->card_caps |= MMC_MODE_HS;

	return 0;
}

static void mmc_set_ios(struct mmc *mmc)
{
	if (mmc->cfg->ops->set_ios)
		mmc->cfg->ops->set_ios(mmc);

	return;
}

void mmc_set_clock(struct mmc *mmc, uint clock)
{
	if (clock >= mmc->cfg->f_max)
		clock = mmc->cfg->f_max;

	if (clock <= mmc->cfg->f_min)
		clock = mmc->cfg->f_min;

	mmc->clock = clock;

	sdhci_printf(!mmc->in_scan, "mmc_set_clock clk = %dHZ\n", mmc->clock);

	mmc_set_ios(mmc);
}

static void mmc_set_bus_width(struct mmc *mmc, uint width)
{
	sdhci_printf(!mmc->in_scan, "mmc_set_bus_width width=%x\n", width);

	mmc->bus_width = width;

	mmc_set_ios(mmc);
}

static unsigned ext_csd_bits[] = {
	MMC_EXT_CSD_DDR_BUS_WIDTH_8,
	MMC_EXT_CSD_DDR_BUS_WIDTH_4,
	MMC_EXT_CSD_BUS_WIDTH_8,
	MMC_EXT_CSD_BUS_WIDTH_4,
	MMC_EXT_CSD_BUS_WIDTH_1,
};

static unsigned ext_to_hostcaps[] = {
	[MMC_EXT_CSD_DDR_BUS_WIDTH_4] =
		MMC_MODE_DDR_52MHz | MMC_MODE_4BIT,
	[MMC_EXT_CSD_DDR_BUS_WIDTH_8] =
		MMC_MODE_DDR_52MHz | MMC_MODE_8BIT,
	[MMC_EXT_CSD_BUS_WIDTH_4] = MMC_MODE_4BIT,
	[MMC_EXT_CSD_BUS_WIDTH_8] = MMC_MODE_8BIT,
};

static unsigned widths[] = {
	8, 4, 8, 4, 1,
};

void mmc_get_version(struct mmc *mmc)
{
	sdhci_printf(!mmc->in_scan, "mmc raw version = 0x%x\n", mmc->version);

	if (mmc->version == MMC_VERSION_UNKNOWN) {
		switch ((mmc->csd[0] >> 26) & 0xf) {
		case 0:
			mmc->version = MMC_VERSION_1_2;
			break;
		case 1:
			mmc->version = MMC_VERSION_1_4;
			break;
		case 2:
			mmc->version = MMC_VERSION_2_2;
			break;
		case 3:
			mmc->version = MMC_VERSION_3;
			break;
		case 4:
			mmc->version = MMC_VERSION_4;
			break;
		default:
			mmc->version = MMC_VERSION_1_2;
			break;
		}
	}

	sdhci_printf(!mmc->in_scan, "mmc spec version = 0x%x\n", mmc->version);
}

static const u32 sprd_mmc_card_versions[] = {
	MMC_VERSION_4,
	MMC_VERSION_4_1,
	MMC_VERSION_4_2,
	MMC_VERSION_4_3,
	0,//Obsolete
	MMC_VERSION_4_41,
	MMC_VERSION_4_5,
	MMC_VERSION_5_0,
	MMC_VERSION_5_1
};

static void mmc_boot_capacity(struct mmc *sprd_mmc, u8 *ext_csd)
{
	sprd_mmc->capacity_boot = ext_csd[EXT_CSD_BOOT_MULT] << 17;
}

static void mmc_rpmb_capacity(struct mmc *sprd_mmc, u8 *ext_csd)
{
	sprd_mmc->capacity_rpmb = ext_csd[EXT_CSD_RPMB_MULT] << 17;
}

struct mode_width_tuning {
	enum bus_mode mode;
	uint widths;
#ifdef CONFIG_MMC_SUPPORTS_TUNING
	uint tuning;
#endif
};

static const struct ext_csd_bus_width {
	uint cap;
	bool is_ddr;
	uint ext_csd_bits;
} ext_csd_bus_width[] = {
	{MMC_MODE_8BIT, true, MMC_EXT_CSD_DDR_BUS_WIDTH_8},
	{MMC_MODE_4BIT, true, MMC_EXT_CSD_DDR_BUS_WIDTH_4},
	{MMC_MODE_8BIT, false, MMC_EXT_CSD_BUS_WIDTH_8},
	{MMC_MODE_4BIT, false, MMC_EXT_CSD_BUS_WIDTH_4},
	{MMC_MODE_1BIT, false, MMC_EXT_CSD_BUS_WIDTH_1},
};

const char *mmc_speed_mode_type(enum bus_mode mode)
{
	static const char *const names[] = {
	      [MMC_LEGACY]	= "MMC legacy",
	      [MMC_HS]		= "MMC High Speed (26MHz)",
	      [SD_HS]		= "SD High Speed (50MHz)",
	      [UHS_SDR12]	= "UHS SDR12 (25MHz)",
	      [UHS_SDR25]	= "UHS SDR25 (50MHz)",
	      [UHS_SDR50]	= "UHS SDR50 (100MHz)",
	      [UHS_SDR104]	= "UHS SDR104 (208MHz)",
	      [UHS_DDR50]	= "UHS DDR50 (50MHz)",
	      [MMC_HS_52]	= "MMC High Speed (52MHz)",
	      [MMC_DDR_52]	= "MMC DDR52 (52MHz)",
	      [MMC_HS_200]	= "HS200 (200MHz)",
	      [MMC_HS_400]	= "HS400 (200MHz)",
	      [MMC_HS_400_ES]	= "HS400ES (200MHz)",
	};

	if (mode >= MMC_MODES_END)
		return "Unknown mode";
	else
		return names[mode];
}

static inline bool mmc_is_mode_ddr(enum bus_mode mode)
{
	if (mode == MMC_DDR_52)
		return true;
	else
		return false;
}

static uint mmc_mode_to_freq(struct mmc *sprd_mmc, enum bus_mode mode)
{
	static const int mode_freqs[] = {
	      [MMC_LEGACY]	= 25000000,
	      [MMC_HS]		= 26000000,
	      [SD_HS]		= 50000000,
	      [MMC_HS_52]	= 52000000,
	      [MMC_DDR_52]	= 52000000,
	      [UHS_SDR12]	= 25000000,
	      [UHS_SDR25]	= 50000000,
	      [UHS_SDR50]	= 100000000,
	      [UHS_DDR50]	= 50000000,
	      [UHS_SDR104]	= 208000000,
	      [MMC_HS_200]	= 200000000,
	      [MMC_HS_400]	= 200000000,
	      [MMC_HS_400_ES]	= 200000000,
	};

	if (mode >= MMC_MODES_END)
		return 0;
	else
		return mode_freqs[mode];
}

static int mmc_select_mode(struct mmc *sprd_mmc, enum bus_mode mode)
{
	sprd_mmc->selected_mode = mode;
	sprd_mmc->tran_speed = mmc_mode_to_freq(sprd_mmc, mode);
	sprd_mmc->ddr_mode = mmc_is_mode_ddr(mode);

	sdhci_debugf(!sprd_mmc->in_scan, "selecting mode %s (freq : %d MHz)\n",
 			mmc_speed_mode_type(mode), sprd_mmc->tran_speed / 1000000);

	return 0;
}

static int sd_select_bus_width(struct mmc *sprd_mmc, int width)
{
	int err;
	struct mmc_cmd sprd_cmd;

	if ((width != 4) && (width != 1)) {
		errorf("width value: %d error\n", width);
		return -EINVAL;
	}

	sprd_cmd.cmdidx = MMC_CMD_APP_CMD;
	sprd_cmd.resp_type = MMC_RSP_R1;
	sprd_cmd.cmdarg = sprd_mmc->rca << 16;

	err = mmc_send_cmd(sprd_mmc, &sprd_cmd, NULL);
	if (err) {
		errorf("send app cmd(cmd55) error:%d\n", err);
		return err;
	}

	sprd_cmd.cmdidx = SD_CMD_APP_SET_BUS_WIDTH;
	sprd_cmd.resp_type = MMC_RSP_R1;
	if (width == 4)
		sprd_cmd.cmdarg = 2;
	else if (width == 1)
		sprd_cmd.cmdarg = 0;
	err = mmc_send_cmd(sprd_mmc, &sprd_cmd, NULL);
	if (err) {
		errorf("mmc set bus width(acmd6) error: %d\n", err);
		return err;
	}

	return 0;
}

static int sd_set_card_speed(struct mmc *sprd_mmc, enum bus_mode mode)
{
	int err;
	ALLOC_CACHE_ALIGN_BUFFER(uint, switch_status, 16);
	uint speed_bits;
	uint retries;

	/* SD version 1.00 and 1.01 does not support CMD 6 */
	if (sprd_mmc->version == SD_VERSION_1_0) {
		errorf("the sd version err\n");
		return -EPROTONOSUPPORT;
	}

	switch (mode) {
	case MMC_LEGACY:
		speed_bits = UHS_SDR12_BUS_SPEED;
		break;
	case SD_HS:
		speed_bits = HIGH_SPEED_BUS_SPEED;
		break;
#ifdef CONFIG_MMC_UHS_SUPPORT
	case UHS_SDR104:
		speed_bits = UHS_SDR104_BUS_SPEED;
		break;
#endif
	default:
		return -EINVAL;
	}

	/*
	 * When sending cmd6 to switch hs mode, some special sd card's
	 * response edge change from the falling edge to the rising edge,
	 * leading to cmd6 crc error. Need to retry send cmd6 to avoid
	 * error causing the sd init fail.
	 */
	for (retries = 0; retries < 10; retries++) {
		err = _sd_switch(sprd_mmc, SD_SWITCH_SWITCH, 0, speed_bits,
				 (u8 *)switch_status);
		if (!err)
			break;
	}

	dprintf(INFO,"retry switching card speed mode, retries = %d\n", retries);

	if (err) {
		errorf("mmc get switch status(cmd6) error： %d\n", err);
		return err;
	}

	if (((BE32(switch_status[4]) >> 24) & 0xF) != speed_bits)
		return -EOPNOTSUPP;

	return 0;
}

static int mmc_set_card_speed(struct mmc *sprd_mmc, enum bus_mode mode,
			      bool downgrade_hs)
{
	int err;
	int speed_bits;

	ALLOC_CACHE_ALIGN_BUFFER(u8, test_csd, MMC_MAX_BLOCK_LEN);

	switch (mode) {
	case MMC_HS:
	case MMC_HS_52:
	case MMC_DDR_52:
		speed_bits = EXT_CSD_TIMING_HS;
		break;
#ifdef CONFIG_MMC_HS200_SUPPORT
	case MMC_HS_200:
		speed_bits = EXT_CSD_TIMING_HS200;
		break;
#endif
	case MMC_LEGACY:
		speed_bits = EXT_CSD_TIMING_LEGACY;
		break;
    default:
		return -EINVAL;
    }

	err = mmc_switch(sprd_mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_HS_TIMING,
			   speed_bits);
	if (err)
		return err;

#if defined(CONFIG_MMC_HS200_SUPPORT)
	if (downgrade_hs) {
		mmc_select_mode(sprd_mmc, MMC_HS);
		mmc_set_clock(sprd_mmc, mmc_mode_to_freq(sprd_mmc, MMC_HS));
	}
#endif

	if ((mode == MMC_HS) || (mode == MMC_HS_52)) {
		/* check whether it worked */
		err = mmc_send_ext_csd(sprd_mmc, test_csd);
		if (err)
			return err;

		/* Do not support high speed */
		if (!test_csd[EXT_CSD_HS_TIMING]) {
			errorf("mmc do not support high speed\n");
			return -EOPNOTSUPP;
		}
	}

	return 0;
}

static const struct mode_width_tuning mmc_modes_by_pref[] = {
#ifdef CONFIG_MMC_HS200_SUPPORT
		{
			.mode = MMC_HS_200,
			.widths = MMC_MODE_8BIT | MMC_MODE_4BIT,
#ifdef CONFIG_MMC_SUPPORTS_TUNING
			.tuning = MMC_CMD_SEND_TUNING_BLOCK_HS200
#endif
		},
#endif
		{
			.mode = MMC_HS_52,
			.widths = MMC_MODE_8BIT | MMC_MODE_4BIT | MMC_MODE_1BIT,
		},
		{
			.mode = MMC_HS,
			.widths = MMC_MODE_8BIT | MMC_MODE_4BIT | MMC_MODE_1BIT,
		},
		{
			.mode = MMC_LEGACY,
			.widths = MMC_MODE_8BIT | MMC_MODE_4BIT | MMC_MODE_1BIT,
		}
	};

#define for_each_mmc_mode_by_pref(caps, mwt) \
	for (mwt = mmc_modes_by_pref;\
	    mwt < mmc_modes_by_pref + ARRAY_SIZE(mmc_modes_by_pref);\
	    mwt++) \
		if (caps & MMC_CAP(mwt->mode))

#define for_each_supported_width(caps, ddr, ecbv) \
	for (ecbv = ext_csd_bus_width;\
	    ecbv < ext_csd_bus_width + ARRAY_SIZE(ext_csd_bus_width);\
	    ecbv++) \
		if ((ddr == ecbv->is_ddr) && (caps & ecbv->cap))

static inline int bus_width(uint cap)
{
	if (cap == MMC_MODE_8BIT)
		return 8;
	if (cap == MMC_MODE_4BIT)
		return 4;
	if (cap == MMC_MODE_1BIT)
		return 1;

	errorf("invalid bus witdh capability 0x%x\n", cap);
	return 0;
}

#ifdef CONFIG_MMC_SUPPORTS_TUNING
int mmc_send_tuning_cmd(struct mmc *sprd_mmc, u32 opcode, int *cmd_error)
{
	int err = 0;
	struct mmc_cmd cmd;

	cmd.cmdidx = MMC_CMD_SET_BLOCKLEN;
	cmd.cmdarg = 512;
	cmd.resp_type = MMC_RSP_R1;

	err = mmc_send_cmd(sprd_mmc, &cmd, NULL);
	if (err)
		return err;

	return 0;
}

int mmc_send_tuning_read(struct mmc *sprd_mmc, u32 opcode, int *cmd_error)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	int size = 512, err = 0;

	ALLOC_CACHE_ALIGN_BUFFER(u8, data_buf, size);

	cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;

	data.blocksize = size;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.dest = data_buf;

	err = mmc_send_cmd(sprd_mmc, &cmd, &data);
	if (err)
		return err;

	return 0;
}

int mmc_send_tuning(struct mmc *sprd_mmc, u32 opcode, int *cmd_error)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	const u8 *tuning_block_pattern;
	int size, err;

	if (sprd_mmc->bus_width == 8) {
		tuning_block_pattern = tuning_blk_pattern_8bit;
		size = sizeof(tuning_blk_pattern_8bit);
	} else if (sprd_mmc->bus_width == 4) {
		tuning_block_pattern = tuning_blk_pattern_4bit;
		size = sizeof(tuning_blk_pattern_4bit);
	} else
		return -EINVAL;

	ALLOC_CACHE_ALIGN_BUFFER(u8, data_buf, size);

	cmd.cmdidx = opcode;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;

	data.dest = data_buf;
	data.blocks = 1;
	data.blocksize = size;
	data.flags = MMC_DATA_READ;

	err = mmc_send_cmd(sprd_mmc, &cmd, &data);
	if (err)
		return err;

	if (memcmp(data_buf, tuning_block_pattern, size))
		return -EIO;

	return 0;
}

static int mmc_execute_tuning(struct mmc *sprd_mmc, uint opcode)
{
	if (!sprd_mmc->cfg->ops->execute_tuning)
		return -ENOSYS;

	return sprd_mmc->cfg->ops->execute_tuning(sprd_mmc, opcode);
}
#endif

/*
 * compare the content is specified in Ext CSD.
 * confirm that the transfer is working as expected.
 */
static int mmc_validate_ext_csd(struct mmc *sprd_mmc)
{
	int err;
	const u8 *ext_csd = sprd_mmc->ext_csd;
	ALLOC_CACHE_ALIGN_BUFFER(u8, test_csd, MMC_MAX_BLOCK_LEN);

	if (sprd_mmc->version < MMC_VERSION_4)
		return 0;

	err = mmc_send_ext_csd(sprd_mmc, test_csd);
	if (err)
		return err;

	/* Only compare read only fields */
	if (ext_csd[EXT_CSD_PARTITIONING_SUPPORT]
		== test_csd[EXT_CSD_PARTITIONING_SUPPORT] &&
	    ext_csd[EXT_CSD_HC_WP_GRP_SIZE]
		== test_csd[EXT_CSD_HC_WP_GRP_SIZE] &&
	    ext_csd[EXT_CSD_REV]
		== test_csd[EXT_CSD_REV] &&
	    ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE]
		== test_csd[EXT_CSD_HC_ERASE_GRP_SIZE] &&
	    memcmp(&ext_csd[EXT_CSD_SEC_CNT],
		   &test_csd[EXT_CSD_SEC_CNT], 4) == 0)
		return 0;

	return -EBADMSG;
}

static int emmc_scan_init(struct mmc *mmc, uint speed_mode)
{
	int err = 0;
	u32 cap = 0, dll_dly = 0;
	struct sprd_sdhci_host *host = mmc->priv;

	dll_dly = mmc->cfg->ops->get_dll_dly(mmc);

	sprd_host_reinit(EMMC);

	mmc->has_init = 0;
	mmc_init(mmc);
	do_preinit();

	switch (speed_mode) {
	case SCAN_MMC_HS:
		cap |= MMC_EXT_CSD_BUS_WIDTH_8;
		cap |= MMC_HS << 8;
		break;
	case SCAN_MMC_DDR50:
		cap |= MMC_EXT_CSD_DDR_BUS_WIDTH_8;
		cap |= MMC_DDR_52 << 8;
		break;
	case SCAN_MMC_HS200:
		cap |= MMC_EXT_CSD_BUS_WIDTH_8;
		cap |= MMC_HS_200 << 8;
		break;
	case SCAN_MMC_HS400:
		cap |= MMC_EXT_CSD_DDR_BUS_WIDTH_8;
		cap |= MMC_HS_400 << 8;
		break;
	case SCAN_MMC_HS401:
		cap |= MMC_EXT_CSD_DDR_BUS_WIDTH_8;
		cap |= MMC_HS_400_ES << 8;
		break;
	}

	err = mmc_select_mode_and_width_for_scan(mmc, cap);
	if (err) {
		errorf("reinit for scan fail\n");
		return err;
	}

	mmc->cfg->ops->set_dll_dly(mmc, dll_dly, 0xFFFFFFFF);

	return 0;
}

static int mmc_scan_cmd_delay_value(struct mmc *sprd_mmc, int dll_cnt, uint speed_mode)
{
	int i, timeout = 1000, start_window_a = 0, end_window_a = 0, start_window_b = 0, end_window_b = 0;

	for (i = 0; i <= dll_cnt; i++) {
		sprd_mmc->cfg->ops->set_dll_dly(sprd_mmc, i << 8, 0xFF << 8);//set cmd delay value
		//send cmd13 to judge if cmd dealy avaliable
		if (!mmc_send_status_for_scan(sprd_mmc)) {
			dprintf(INFO,"%s delay: %x success\r\n", __func__, i);
			if (!start_window_a) {//find first avaliable value, and mark
				start_window_a = i + 1;
				end_window_a = i + 1;
			}
			if (!(i - end_window_a))//to find if it's continuous avaliable value
				end_window_a = i + 1;
			else {// if it's not continuous avaliable value, means a new window
				if (!start_window_b) {//first backup window
					start_window_b = start_window_a;
					end_window_b = end_window_a;
				}
				if (end_window_b - start_window_b < end_window_a - start_window_a) {
					//better new window
					start_window_b = start_window_a;
					end_window_b = end_window_a;
				}
				start_window_a = i + 1;
				end_window_a = i + 1;
			}
		} else {
			if (emmc_scan_init(sprd_mmc, speed_mode)) {
				errorf("emmc_scan_init failed\r\n");
				return 0;
			}
			errorf("%s delay: %x failed\r\n", __func__, i);
		}
	}

	if (end_window_b - start_window_b < end_window_a - start_window_a) {
		//the start_window and end_window has been +1, therefore need -1
		return (end_window_a - start_window_a) / 2 + start_window_a - 1;
        } else {
		//the start_window and end_window has been +1, therefore need -1
		return (end_window_b - start_window_b) / 2 + start_window_b - 1;
	}
}

static int mmc_scan_read_delay_value(struct mmc *sprd_mmc, int dll_cnt,
					uint32_t start_blk, uint speed_mode)
{
	int i, timeout = 1000, start_window_a = 0, end_window_a = 0;
	int start_window_b = 0, end_window_b = 0, block_cnt = 8;
	uint32_t cur_start_blk = start_blk;

	ALLOC_CACHE_ALIGN_BUFFER(u8, rd_buf, sprd_mmc->read_bl_len * block_cnt);

	for (i = 0; i <= dll_cnt; i++) {
		//set data read delay value
		sprd_mmc->cfg->ops->set_dll_dly(sprd_mmc,
						i << 16 | i << 24, 0xFF << 16 | 0xFF << 24);
		//send read cmd to judge if data read dealy avaliable
		if (block_cnt == mmc_bread(EMMC, cur_start_blk, block_cnt, rd_buf)) {
			dprintf(INFO,"%s delay: %x success\r\n", __func__, i);
			if (!start_window_a) {//find first avaliable value, and mark
				start_window_a = i + 1;
				end_window_a = i + 1;
			}
			if (!(i - end_window_a))//to find if it's continuous avaliable value
				end_window_a = i + 1;
			else {// if it's not continuous avaliable value, means a new window
				if (!start_window_b) {//first backup window
					start_window_b = start_window_a;
					end_window_b = end_window_a;
				}
				//better new window
				if (end_window_b - start_window_b < end_window_a - start_window_a) {
					start_window_b = start_window_a;
					end_window_b = end_window_a;
				}
				start_window_a = i + 1;
				end_window_a = i + 1;
			}
		} else {
			if (emmc_scan_init(sprd_mmc, speed_mode)) {
				errorf("emmc_scan_init failed\r\n");
				return 0;
			}
			errorf("%s delay: %x failed\r\n", __func__, i);
		}
	}

	if (!start_window_a)
		return 0;
	if (end_window_b - start_window_b < end_window_a - start_window_a) {
		//the start_window and end_window has been +1, therefore need -1
		return (end_window_a - start_window_a) / 2 + start_window_a - 1;
        } else {
		//the start_window and end_window has been +1, therefore need -1
		return (end_window_b - start_window_b) / 2 + start_window_b - 1;
	}
}

static int mmc_scan_write_delay_value(struct mmc *sprd_mmc, int dll_cnt,
					uint32_t start_blk, uint speed_mode)
{
	int i, timeout = 1000, start_window_a = 0, end_window_a = 0, start_window_b = 0;
	int end_window_b = 0, block_cnt = 8;
	uint32_t cur_start_blk = start_blk;

	ALLOC_CACHE_ALIGN_BUFFER(u8, wr_buf, sprd_mmc->read_bl_len * block_cnt);
	for (i = 0; i <= dll_cnt; i++) {
		sprd_mmc->cfg->ops->set_dll_dly(sprd_mmc, i, 0xFF);//set data write delay value
		//send write cmd to judge if data write dealy avaliable
		if (block_cnt == mmc_bwrite(EMMC, cur_start_blk, block_cnt, wr_buf)) {
			dprintf(INFO,"%s delay: %x success\r\n", __func__, i);
			if (!start_window_a) {//find first avaliable value, and mark
				start_window_a = i + 1;
				end_window_a = i + 1;
			}
			if (!(i - end_window_a))//to find if it's continuous avaliable value
				end_window_a = i + 1;
			else {// if it's not continuous avaliable value, means a new window
				if (!start_window_b) {//first backup window
					start_window_b = start_window_a;
					end_window_b = end_window_a;
				}
				//better new window
				if (end_window_b - start_window_b < end_window_a - start_window_a) {
					start_window_b = start_window_a;
					end_window_b = end_window_a;
				}
				start_window_a = i + 1;
				end_window_a = i + 1;
			}
		} else {
			if (emmc_scan_init(sprd_mmc, speed_mode)) {
				errorf("emmc_scan_init failed\r\n");
				return 0;
			}
			errorf("%s delay: %x failed\r\n", __func__, i);
		}
	}

	if (!start_window_a)
		return 0;
	if (end_window_b - start_window_b < end_window_a - start_window_a) {
		//the start_window and end_window has been +1, therefore need -1
		return (end_window_a - start_window_a) / 2 + start_window_a - 1;
        } else {
		//the start_window and end_window has been +1, therefore need -1
		return (end_window_b - start_window_b) / 2 + start_window_b - 1;
	}
}

int mmc_scan_delay_value(struct mmc *sprd_mmc, uint32_t start_blk,
			 u32 *delay_value, uint speed_mode)
{
	int dll_cnt = 0, delay_value_local = 0;

	dll_cnt = sprd_mmc->cfg->ops->enable_dpll_scan(sprd_mmc);
	if (!dll_cnt)
		return -ENOTSUP;
	dprintf(INFO,"%s dll_cnt: %d\r\n", __func__, dll_cnt);

	sprd_mmc->cfg->ops->set_dll_dly(sprd_mmc, 0, 0xFFFFFFFF); //clear all delay value
	delay_value_local = mmc_scan_cmd_delay_value(sprd_mmc, dll_cnt, speed_mode);
	if (!delay_value_local) {
		errorf("no avaliable delay value \n");
		return -ENOTSUP;
	}
	dprintf(INFO,"cmd delay: %x\r\n", delay_value_local);
	*(delay_value) = delay_value_local;

	sprd_mmc->cfg->ops->set_dll_dly(sprd_mmc, delay_value_local << 8, 0xFF << 8);
	delay_value_local = mmc_scan_read_delay_value(sprd_mmc, dll_cnt, start_blk, speed_mode);
	if (!delay_value_local) {
		errorf("no avaliable delay value \n");
		return -ENOTSUP;
	}
	dprintf(INFO,"data read delay: %x\r\n", delay_value_local);
	*(delay_value + 1) = delay_value_local;

	sprd_mmc->cfg->ops->set_dll_dly(sprd_mmc, delay_value_local << 16 | delay_value_local << 24,
					 0xFF << 8 | 0xFF << 24);
	delay_value_local = mmc_scan_write_delay_value(sprd_mmc, dll_cnt, start_blk, speed_mode);
	if (!delay_value_local) {
		errorf("no avaliable delay value \n");
		return -ENOTSUP;
	}
	dprintf(INFO,"data write delay: %x\r\n", delay_value_local);
	*(delay_value + 2) = delay_value_local;

	return 0;
}

static int mmc_select_mode_and_width_for_scan(struct mmc *sprd_mmc, uint cap)
{
	int err = 0, speed_mode;
	const struct mode_width_tuning *mwt;
	const struct ext_csd_bus_width *ecbw;

	/* Only version 4 of MMC supports wider bus widths */
	if (sprd_mmc->version < MMC_VERSION_4) {
		errorf("card does not support wider bus widths \n");
		return 0;
	}

	if (!sprd_mmc->ext_csd) {
		errorf("no ext_csd found!\n");
		return -EOPNOTSUPP;
	}

	speed_mode = (cap >> 8) & 0xF;
	/*
	 * In case the eMMC is in DDR mode, downgrade to HS mode
	 * before doing anything else, since a transition from either of
	 * the HS200/HS400 mode directly to legacy mode is not supported.
	 */
	if ((speed_mode == MMC_DDR_52) || (speed_mode == MMC_HS_400) || (speed_mode == MMC_HS_400_ES))
		mmc_set_card_speed(sprd_mmc, MMC_HS, true);
	else
		mmc_set_clock(sprd_mmc, sprd_mmc->legacy_speed);

	/* set bus width (card + host) */
	err = mmc_switch(sprd_mmc, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_BUS_WIDTH,
			(cap & 0xF) & ~EXT_CSD_DDR_FLAG);
	if (err) {
		errorf("mmc switch bus width error: %d\n", err);
		goto error;
	}

	/* set bus width */
	if ((cap & 0xF) & EXT_CSD_DDR_FLAG) {
		err = mmc_switch(sprd_mmc,
		EXT_CSD_CMD_SET_NORMAL,
		EXT_CSD_BUS_WIDTH,
		(cap & 0xFF));
		if (err) {
			errorf("mmc switch bus width error: %d\n", err);
			goto error;
		}
	}

	/* set bus speed (card) */
	err = mmc_set_card_speed(sprd_mmc, ((cap >> 8) & 0xF), false);
	if (err) {
		errorf("mmc set card speed error: %d\n", err);
		goto error;
	}

	/* set bus mode (host) */
	mmc_select_mode(sprd_mmc, ((cap >> 8) & 0xF));
	mmc_set_clock(sprd_mmc, sprd_mmc->tran_speed);

	return 0;

error:
	/* if an error occurred, revert to a safer bus mode */
	err = mmc_switch(sprd_mmc, EXT_CSD_CMD_SET_NORMAL,
		EXT_CSD_BUS_WIDTH, MMC_EXT_CSD_BUS_WIDTH_1);
	if (err)
		errorf("mmc switch bus width error: %d\n", err);

	mmc_select_mode(sprd_mmc, MMC_LEGACY);
	mmc_set_bus_width(sprd_mmc, 1);

	return -EOPNOTSUPP;
}

static int mmc_select_mode_and_width(struct mmc *sprd_mmc, uint card_caps)
{
	int err = 0;
	const struct mode_width_tuning *mwt;
	const struct ext_csd_bus_width *ecbw;

	if (mmc_host_is_spi(sprd_mmc)) {
		mmc_set_bus_width(sprd_mmc, 1);
		mmc_select_mode(sprd_mmc, MMC_LEGACY);
		mmc_set_clock(sprd_mmc, sprd_mmc->tran_speed);
		return 0;
	}

	/* Restrict card's capabilities by what the host can do */
	card_caps &= sprd_mmc->host_caps;

	/* Only version 4 of MMC supports wider bus widths */
	if (sprd_mmc->version < MMC_VERSION_4) {
		errorf("card does not support wider bus widths \n");
		return 0;
	}

	if (!sprd_mmc->ext_csd) {
		errorf("no ext_csd found!\n");
		return -EOPNOTSUPP;
	}

#if defined(CONFIG_MMC_HS200_SUPPORT)
	/*
	 * In case the eMMC is in HS200/HS400 mode, downgrade to HS mode
	 * before doing anything else, since a transition from either of
	 * the HS200/HS400 mode directly to legacy mode is not supported.
	 */
	if (card_caps & MMC_MODE_HS200)
		mmc_set_card_speed(sprd_mmc, MMC_HS, true);
	else
#endif
		mmc_set_clock(sprd_mmc, sprd_mmc->legacy_speed);

	for_each_mmc_mode_by_pref(card_caps, mwt) {
		for_each_supported_width(card_caps & mwt->widths,
						mmc_is_mode_ddr(mwt->mode), ecbw) {

			sdhci_debugf(!sprd_mmc->in_scan, "trying mode %s width %d \
					(at %d MHz)\n", mmc_speed_mode_type(mwt->mode),
				bus_width(ecbw->cap),
				mmc_mode_to_freq(sprd_mmc, mwt->mode) / 1000000);

			/* set bus width (card + host) */
			err = mmc_switch(sprd_mmc, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_BUS_WIDTH,
					ecbw->ext_csd_bits & ~EXT_CSD_DDR_FLAG);
			if (err) {
				errorf("mmc switch bus width error: %d\n", err);
				goto error;
			}

			mmc_set_bus_width(sprd_mmc, bus_width(ecbw->cap));

			/* set bus width */
			if (ecbw->ext_csd_bits & EXT_CSD_DDR_FLAG) {
				err = mmc_switch(sprd_mmc,
				EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_BUS_WIDTH,
				ecbw->ext_csd_bits);
				if (err) {
					errorf("mmc switch bus width error: %d\n", err);
					goto error;
				}
			}

			/* set bus speed (card) */
			err = mmc_set_card_speed(sprd_mmc, mwt->mode, false);
			if (err) {
				errorf("mmc set card speed error: %d\n", err);
				goto error;
			}

			/* set bus mode (host) */
			mmc_select_mode(sprd_mmc, mwt->mode);
			mmc_set_clock(sprd_mmc, sprd_mmc->tran_speed);

#ifdef CONFIG_MMC_SUPPORTS_TUNING
			/* execute tuning if needed */
			if (mwt->tuning) {
				err = mmc_execute_tuning(sprd_mmc, mwt->tuning);
				if (err) {
					errorf("tuning failed : %d\n", err);
					goto error;
				}
			}
#endif

			/* do a transfer to check the configuration */
			err = mmc_validate_ext_csd(sprd_mmc);
			if (!err)
				return 0;

error:
			/* if an error occurred, revert to a safer bus mode */
			err = mmc_switch(sprd_mmc, EXT_CSD_CMD_SET_NORMAL,
				   EXT_CSD_BUS_WIDTH, MMC_EXT_CSD_BUS_WIDTH_1);
			if (err)
				errorf("mmc switch bus width error: %d\n", err);

			mmc_select_mode(sprd_mmc, MMC_LEGACY);
			mmc_set_bus_width(sprd_mmc, 1);
		}
	}

	errorf("unable to select a mode : %d\n", err);

	return -EOPNOTSUPP;
}

static int mmc_get_capabilities(struct mmc *sprd_mmc)
{
	u8 *ext_csd = sprd_mmc->ext_csd;
	char cardtype;

	sprd_mmc->card_caps = 0;
	sprd_mmc->card_caps |= MMC_MODE_1BIT | MMC_MODE_LEGACY;

	if (mmc_host_is_spi(sprd_mmc))
		return 0;

	/* Only version 4 supports high-speed */
	if (sprd_mmc->version < MMC_VERSION_4)
		return 0;

	if (!ext_csd) {
		errorf("No ext_csd found!\n"); /* this should enver happen */
		return -EOPNOTSUPP;
	}

	sprd_mmc->card_caps |= MMC_MODE_4BIT | MMC_MODE_8BIT;

	cardtype = ext_csd[EXT_CSD_CARD_TYPE];
	sprd_mmc->cardtype = cardtype;

#if defined(CONFIG_MMC_HS200_SUPPORT)
	if (cardtype & (MMC_EXT_CSD_CARD_TYPE_HS200_1_2V |
			MMC_EXT_CSD_CARD_TYPE_HS200_1_8V)) {
		sprd_mmc->card_caps |= MMC_MODE_HS200;
	}
#endif
	if (cardtype & MMC_EXT_CSD_CARD_TYPE_52)
		sprd_mmc->card_caps |= MMC_MODE_HS_52MHz;
	if (cardtype & MMC_EXT_CSD_CARD_TYPE_26)
		sprd_mmc->card_caps |= MMC_MODE_HS;

	return 0;
}

static int sd_get_capabilities(struct mmc *sprd_mmc)
{
	int err;
	int timeout;
	struct mmc_cmd sprd_cmd;
	struct mmc_data sprd_data;
	ALLOC_CACHE_ALIGN_BUFFER(uint, scr, 2);
	ALLOC_CACHE_ALIGN_BUFFER(uint, switch_status, 16);
#ifdef CONFIG_MMC_UHS_SUPPORT
	u32 sd_uhs_support;
#endif

	sprd_mmc->card_caps = MMC_MODE_1BIT | MMC_MODE_LEGACY;

	sprd_cmd.cmdidx = MMC_CMD_APP_CMD;
	sprd_cmd.resp_type = MMC_RSP_R1;
	sprd_cmd.cmdarg = sprd_mmc->rca << 16;
	err = mmc_send_cmd(sprd_mmc, &sprd_cmd, NULL);
	if (err) {
		errorf("send app cmd(cmd55) error: %d\n", err);
		return err;
	}

	timeout = 3;

	sprd_cmd.resp_type = MMC_RSP_R1;
	sprd_cmd.cmdarg = 0;
	sprd_cmd.cmdidx = SD_CMD_APP_SEND_SCR;
retry_scr:
	sprd_data.blocksize = 8;
	sprd_data.blocks = 1;
	sprd_data.flags = MMC_DATA_READ;
	sprd_data.dest = (char *)scr;
	err = mmc_send_cmd(sprd_mmc, &sprd_cmd, &sprd_data);
	if (err) {
		if (timeout--)
			goto retry_scr;
		errorf("mmc get scr(acmd51) error: %d\n", err);
		return err;
	}

	sprd_mmc->scr[0] = BE32(scr[0]);
	sprd_mmc->scr[1] = BE32(scr[1]);

	switch ((sprd_mmc->scr[0] >> 24) & 0xf) {
	case 0:
		sprd_mmc->version = SD_VERSION_1_0;
		break;
	case 1:
		sprd_mmc->version = SD_VERSION_1_10;
		break;
	case 2:
		sprd_mmc->version = SD_VERSION_2;
		if ((sprd_mmc->scr[0] >> 15) & 0x1)
			sprd_mmc->version = SD_VERSION_3;
		break;
	default:
		sprd_mmc->version = SD_VERSION_1_0;
		break;
	}

	if (SD_DATA_4BIT & sprd_mmc->scr[0])
		sprd_mmc->card_caps |= MMC_MODE_4BIT;

	/* Version 1.0 doesn't support switching */
	if (sprd_mmc->version == SD_VERSION_1_0) {
		errorf("card version does not support switching cmd\n");
		return -EPROTONOSUPPORT;
	}

	timeout = 4;
	while (timeout--) {
		err = _sd_switch(sprd_mmc, SD_SWITCH_CHECK, 0, 1,
				(u8 *)switch_status);

		if (err) {
			errorf("mmc send cmd6 error:%d\n", err);
			return err;
		}

		if (!(BE32(switch_status[7]) & SD_HIGHSPEED_BUSY))
			break;
	}

	/* If high-speed isn't supported, we return */
	if (BE32(switch_status[3]) & SD_HIGHSPEED_SUPPORTED)
		sprd_mmc->card_caps |= MMC_MODE_HS;

#ifdef CONFIG_MMC_UHS_SUPPORT
	/* Version before 3.0 don't support UHS modes */
	if (sprd_mmc->version < SD_VERSION_3) {
		errorf("card version does not support UHS modes\n");
		return 0;
	}

	sd_uhs_support = BE32(switch_status[3]) >> 16 & 0x1f;
	debugf("sd_uhs_support: 0x%x\n", sd_uhs_support);

	if (sd_uhs_support & SD_MODE_UHS_SDR104)
		sprd_mmc->card_caps |= SD_MODE_SDR104;
#endif

	return 0;
}

static const struct mode_width_tuning sd_modes_by_pref[] = {
#ifdef CONFIG_MMC_UHS_SUPPORT
	{
		.mode = UHS_SDR104,
		.widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
#ifdef CONFIG_MMC_SUPPORTS_TUNING
		.tuning = MMC_CMD_SEND_TUNING_BLOCK
#endif
	},
#endif
	{
		.mode = SD_HS,
		.widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
#ifdef CONFIG_MMC_SUPPORTS_TUNING
		.tuning = MMC_CMD_SEND_TUNING_BLOCK
#endif
	},
	{
		.mode = MMC_LEGACY,
		.widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
	}
};

#define MMC_UHS_CAPS (SD_MODE_SDR12 | SD_MODE_SDR25 | \
						SD_MODE_SDR50 | SD_MODE_SDR104)

#define for_each_sd_mode_by_pref(caps, mwt) \
	for (mwt = sd_modes_by_pref;\
	     mwt < sd_modes_by_pref + ARRAY_SIZE(sd_modes_by_pref);\
	     mwt++)\
		if (caps & MMC_CAP(mwt->mode))

static int sd_select_mode_and_width(struct mmc *sprd_mmc, uint card_caps)
{
	int err = 0;
	uint bus_widths[] = {MMC_MODE_4BIT, MMC_MODE_1BIT};
	const struct mode_width_tuning *mwt;
#ifdef CONFIG_MMC_UHS_SUPPORT
	bool uhs_en = (sprd_mmc->ocr & OCR_S18R) ? true : false;
#else
	bool uhs_en = false;
#endif

	/* Restrict card's capabilities by what the host can do */
	card_caps &= sprd_mmc->host_caps;

	if (!uhs_en)
		card_caps &= ~MMC_UHS_CAPS;
	else
		debugf("the card supports UHS-I mode\n");

	for_each_sd_mode_by_pref(card_caps, mwt)
		if (card_caps & MMC_CAP(mwt->mode)) {
			uint *i;
			for (i = bus_widths; i < bus_widths + ARRAY_SIZE(bus_widths); i++) {
				if (*i & card_caps & mwt->widths) {
					debugf("trying mode %s width %d (at %d MHz)\n",
						mmc_speed_mode_type(mwt->mode),
						bus_width(*i),
						mmc_mode_to_freq(sprd_mmc, mwt->mode) / 1000000);

					/* configure the bus width (card + host) */
					err = sd_select_bus_width(sprd_mmc, bus_width(*i));
					if (err){
						errorf("sd select bus width error:%d\n", err);
						goto error;
					}

					mmc_set_bus_width(sprd_mmc, bus_width(*i));

					/* configure the bus mode (card) */
					err = sd_set_card_speed(sprd_mmc, mwt->mode);
					if (err) {
						errorf("sd set card speed error:%d\n", err);
						goto error;
					}

					/* configure the bus mode (host) */
					mmc_select_mode(sprd_mmc, mwt->mode);
					mmc_set_clock(sprd_mmc, sprd_mmc->tran_speed);

#ifdef CONFIG_MMC_SUPPORTS_TUNING
					/* execute tuning if needed */
					if (mwt->tuning) {
						err = mmc_execute_tuning(sprd_mmc, mwt->tuning);
						if (err) {
							errorf("tuning failed\n");
							goto error;
						}
					}
#endif
					if (!err)
						return 0;

error:
					/* revert to a safer bus speed */
					mmc_select_mode(sprd_mmc, MMC_LEGACY);
					mmc_set_clock(sprd_mmc, sprd_mmc->tran_speed);
				}
			}
		}

	errorf("unable to select a mode\n");
	return -EOPNOTSUPP;
}

bool fixup_device_status(void)
{
	return fixup_device_value;
}

static bool fixup_device_judge(struct mmc *sprd_mmc)
{
	unsigned int manfid;
	char prod_name[8];
	uint i;

	manfid = sprd_mmc->cid[0] >> 24;
	prod_name[0] = sprd_mmc->cid[0] & 0xff;
	prod_name[1] = (sprd_mmc->cid[1] >> 24) & 0xff;
	prod_name[2] = (sprd_mmc->cid[1] >> 16) & 0xff;
	prod_name[3] = (sprd_mmc->cid[1] >> 8) & 0xff;
	prod_name[4] = sprd_mmc->cid[1] & 0xff;
	prod_name[5] = (sprd_mmc->cid[2] >> 24) & 0xff;

	dprintf(INFO, "mmc: manfid= 0x%06x, name=%c%c%c%c%c%c\n", manfid, prod_name[0],
		prod_name[1], prod_name[2], prod_name[3], prod_name[4], prod_name[5]);

	for (i = 0; i < ARRAY_SIZE(fixup_device); i++)
		if (manfid == fixup_device[i].manfid &&
			!strcmp(prod_name, fixup_device[i].name))
			return true;

	return false;
}

int mmc_startup(struct mmc *sprd_mmc)
{
	int err, i;
	uint mult, freq;
	u64 cmult, csize, capacity;
	struct mmc_cmd cmd = {0};
	int timeout = 1000;
	bool has_parts = false;
	bool mmc_part_completed;

	ALLOC_CACHE_ALIGN_BUFFER(u8, ext_csd, MMC_MAX_BLOCK_LEN);

	sdhci_printf(!sprd_mmc->in_scan, "mmc init startup\n");

	if (!sprd_mmc)
		return -1;

	/* send cmd2: MMC_CMD_ALL_SEND_CID*/
	cmd.cmdidx = MMC_CMD_ALL_SEND_CID;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = 0;
	err = mmc_send_cmd(sprd_mmc, &cmd, NULL);
	if (err) {
		errorf("cmd: MMC_CMD_ALL_SEND_CID failed\n");
		return err;
	}

	memcpy((char *)sprd_mmc->cid, (char *)cmd.response, 16);

	sdhci_printf(!sprd_mmc->in_scan, "card cid register: ");
	for (i = 0; i < 4; i++)
		sdhci_printf(!sprd_mmc->in_scan, "%08x ", sprd_mmc->cid[i]);
	sdhci_printf(!sprd_mmc->in_scan, "\n");

	/* send cmd3: SD_CMD_SEND_RELATIVE_ADDR*/
	cmd.cmdidx = SD_CMD_SEND_RELATIVE_ADDR;
	cmd.cmdarg = sprd_mmc->rca << 16;
	cmd.resp_type = MMC_RSP_R6;
	err = mmc_send_cmd(sprd_mmc, &cmd, NULL);
	if (err) {
		errorf("cmd: SD_CMD_SEND_RELATIVE_ADDR failed\n");
		return err;
	}

	if (IS_SD(sprd_mmc))
		sprd_mmc->rca = (cmd.response[0] >> 16) & 0xffff;

	/* send cmd9: MMC_CMD_SEND_CSD*/
	cmd.cmdidx = MMC_CMD_SEND_CSD;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = sprd_mmc->rca << 16;
	err = mmc_send_cmd(sprd_mmc, &cmd, NULL);
	if (err) {
		errorf("cmd: MMC_CMD_SEND_CSD failed\n");
		return err;
	}

	err = mmc_send_status(sprd_mmc, timeout);
	if (err) {
		errorf("mmc_send_status err = %x\n", err);
		return err;
	}

	sprd_mmc->csd[0] = cmd.response[0];
	sprd_mmc->csd[1] = cmd.response[1];
	sprd_mmc->csd[2] = cmd.response[2];
	sprd_mmc->csd[3] = cmd.response[3];

	sdhci_printf(sprd_mmc->in_scan == false, "card csd register: ");
	for (i = 0; i < 4; i++)
		sdhci_printf(sprd_mmc->in_scan == false, "%08x ", sprd_mmc->csd[i]);
	sdhci_printf(sprd_mmc->in_scan == false, "\n");

	mmc_get_version(sprd_mmc);

	freq = fbase[(cmd.response[0] & 0x7)];
	mult = multipliers[((cmd.response[0] >> 3) & 0xf)];

	sprd_mmc->legacy_speed = freq * mult;
	sprd_mmc->tran_speed = freq * mult;

	sprd_mmc->dsr_imp = ((cmd.response[1] >> 12) & 0x1);
	sprd_mmc->read_bl_len = 1 << ((cmd.response[1] >> 16) & 0xf);

	if (IS_SD(sprd_mmc))
		sprd_mmc->write_bl_len = sprd_mmc->read_bl_len;
	else
		sprd_mmc->write_bl_len = 1 << ((cmd.response[3] >> 22) & 0xf);

	if (sprd_mmc->high_capacity) {
		csize = (sprd_mmc->csd[1] & 0x3f) << 16
			| (sprd_mmc->csd[2] & 0xffff0000) >> 16;
		cmult = 8;
	} else {
		csize = (sprd_mmc->csd[1] & 0x3ff) << 2
			| (sprd_mmc->csd[2] & 0xc0000000) >> 30;
		cmult = (sprd_mmc->csd[2] & 0x00038000) >> 15;
	}

	sprd_mmc->capacity_user = (csize + 1) << (cmult + 2);
	sprd_mmc->capacity_user *= sprd_mmc->read_bl_len;
	sprd_mmc->capacity_boot = 0;
	sprd_mmc->capacity_rpmb = 0;
	sprd_mmc->rel_wr_sec_c = 1;

	for (i = 0; i < 4; i++)
		sprd_mmc->capacity_gp[i] = 0;

	if (sprd_mmc->read_bl_len > MMC_MAX_BLOCK_LEN)
		sprd_mmc->read_bl_len = MMC_MAX_BLOCK_LEN;

	if (sprd_mmc->write_bl_len > MMC_MAX_BLOCK_LEN)
		sprd_mmc->write_bl_len = MMC_MAX_BLOCK_LEN;

	if ((sprd_mmc->dsr_imp) && (0xffffffff != sprd_mmc->dsr)) {
		cmd.cmdidx = MMC_CMD_SET_DSR;
		cmd.cmdarg = (sprd_mmc->dsr & 0xffff) << 16;
		cmd.resp_type = MMC_RSP_NONE;
		if (mmc_send_cmd(sprd_mmc, &cmd, NULL))
			errorf("MMC: SET_DSR failed\n");
	}

	cmd.cmdidx = MMC_CMD_SELECT_CARD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = sprd_mmc->rca << 16;
	err = mmc_send_cmd(sprd_mmc, &cmd, NULL);
	if (err) {
		errorf("MMC: SELECT_CARD failed\n");
		return err;
	}

	sprd_mmc->erase_grp_size = 1;
	sprd_mmc->part_config = MMCPART_NOAVAILABLE;
	if (!IS_SD(sprd_mmc) && (sprd_mmc->version >= MMC_VERSION_4)) {
		err = mmc_send_ext_csd(sprd_mmc, ext_csd);
		if (err)
			return err;

		if (ext_csd[EXT_CSD_REV] >= 2) {
			capacity = (ext_csd[EXT_CSD_SEC_CNT] << 0 &
				    0x00000000000000ff)
					| (ext_csd[EXT_CSD_SEC_CNT + 1] << 8 &
					   0x000000000000ff00)
					| (ext_csd[EXT_CSD_SEC_CNT + 2] << 16 &
					   0x0000000000ff0000)
					| (ext_csd[EXT_CSD_SEC_CNT + 3] << 24 &
					   0x00000000ff000000);
			capacity *= MMC_MAX_BLOCK_LEN;

			if ((capacity >> 20) > 2 * 1024)
				sprd_mmc->capacity_user = capacity;
		}

		sprd_mmc->ext_csd = ext_csd;

#ifdef CONFIG_MMC_TRACE
		dprintf(INFO,"\n============== ext_csd ================\n");
		for (i = 0; i < 512; i++) {
			if (i % 10 == 0)
				dprintf(INFO,"\n%4d:   ", i);
			dprintf(INFO,"0x%02x    ", ext_csd[i]);
		}
		dprintf(INFO,"\n");
#endif

		sprd_mmc->rel_wr_sec_c = ext_csd[EXT_CSD_REL_WR_SEC_C];

		if (ext_csd[EXT_CSD_REV] > 8)
			sprd_mmc->version = sprd_mmc_card_versions[8];
		else
			sprd_mmc->version = sprd_mmc_card_versions[ext_csd[EXT_CSD_REV]];
		sdhci_printf(!sprd_mmc->in_scan, "mmc->version = 0x%x\n", sprd_mmc->version);

		if ((PART_SUPPORT & ext_csd[EXT_CSD_PARTITIONING_SUPPORT]) ||
		    ext_csd[EXT_CSD_BOOT_MULT]) {
			sprd_mmc->part_config = ext_csd[EXT_CSD_PART_CONF];
		}

		sprd_mmc->part_support = ext_csd[EXT_CSD_PARTITIONING_SUPPORT];

		mmc_part_completed = !!(ext_csd[EXT_CSD_PARTITION_SETTING] &
			EXT_CSD_PARTITION_SETTING_COMPLETED);
		if (mmc_part_completed) {
		    if (ENHNCD_SUPPORT & ext_csd[EXT_CSD_PARTITIONING_SUPPORT])
				sprd_mmc->part_attr = ext_csd[EXT_CSD_PARTITIONS_ATTRIBUTE];
		}

		mmc_rpmb_capacity(sprd_mmc, ext_csd);

		mmc_boot_capacity(sprd_mmc, ext_csd);

#ifdef CONFIG_MMC_TRACE
		dprintf(INFO,"sprd_mmc->capacity_boot: %lld\n", sprd_mmc->capacity_boot);
		dprintf(INFO,"sprd_mmc->capacity_rpmb: %lld\n", sprd_mmc->capacity_rpmb);
#endif

		for (i = 0; i < 4; i++) {
			int idx = EXT_CSD_GP_SIZE_MULT + i * 3;
			uint mult = (ext_csd[idx + 2] << 16) + (ext_csd[idx + 1] << 8)
				+ ext_csd[idx];

			if (mult)
				has_parts = true;

			if (!mmc_part_completed)
				continue;

			sprd_mmc->capacity_gp[i] = mult;
			sprd_mmc->capacity_gp[i] *= ext_csd[EXT_CSD_HC_WP_GRP_SIZE];
			sprd_mmc->capacity_gp[i] *= ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE];
			sprd_mmc->capacity_gp[i] <<= 19;
#ifdef CONFIG_MMC_TRACE
			dprintf(INFO,"sprd_mmc->capacity_gp[%d]: %lld\n", i, sprd_mmc->capacity_gp[i]);
#endif
		}

		if (mmc_part_completed) {
			sprd_mmc->enh_user_size = (ext_csd[EXT_CSD_ENH_SIZE_MULT+2] << 16) +
				(ext_csd[EXT_CSD_ENH_SIZE_MULT+1] << 8) +
				ext_csd[EXT_CSD_ENH_SIZE_MULT];

			sprd_mmc->enh_user_size *= ext_csd[EXT_CSD_HC_WP_GRP_SIZE];
			sprd_mmc->enh_user_size *= ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE];
			sprd_mmc->enh_user_size <<= 19;

			sprd_mmc->enh_user_start =
				((u64)ext_csd[EXT_CSD_ENH_START_ADDR+3] << 24) +
				((u64)ext_csd[EXT_CSD_ENH_START_ADDR+2] << 16) +
				((u64)ext_csd[EXT_CSD_ENH_START_ADDR+1] << 8) +
				(u64)ext_csd[EXT_CSD_ENH_START_ADDR];

#ifdef CONFIG_MMC_TRACE
			dprintf(INFO,"sprd_mmc->enh_user_start: %lld\n", sprd_mmc->enh_user_start);
#endif

			if (sprd_mmc->high_capacity)
				sprd_mmc->enh_user_start <<= 9;
		}

#ifdef CONFIG_MMC_TRACE
		dprintf(INFO,"sprd_mmc->enh_user_size: %lld\n", sprd_mmc->enh_user_size);
#endif

		if (mmc_part_completed) {
			has_parts = true;
#ifdef CONFIG_MMC_TRACE
			dprintf(INFO,"mmc_part_completed = %d\n", mmc_part_completed);
#endif
		}

		if ((PART_SUPPORT & ext_csd[EXT_CSD_PARTITIONING_SUPPORT]) &&
		    (PART_ENH_ATTRIB & ext_csd[EXT_CSD_PARTITIONS_ATTRIBUTE]))
			has_parts = true;

		if (has_parts) {
			err = mmc_switch(sprd_mmc, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_ERASE_GROUP_DEF, 1);

			if (err) {
				errorf("enable high-capacity erase unit size "
				       "fail, return %d\n", err);
				return err;
			} else {
				ext_csd[EXT_CSD_ERASE_GROUP_DEF] = 1;
			}
		}

		if (ext_csd[EXT_CSD_ERASE_GROUP_DEF] & 0x01) {
			/* Read out group size from ext_csd */
			sprd_mmc->erase_grp_size =
				ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE] * 1024;

			if (sprd_mmc->high_capacity && mmc_part_completed) {
				capacity = (ext_csd[EXT_CSD_SEC_CNT] << 0 &
				    0x00000000000000ff)
					| (ext_csd[EXT_CSD_SEC_CNT + 1] << 8 &
					   0x000000000000ff00)
					| (ext_csd[EXT_CSD_SEC_CNT + 2] << 16 &
					   0x0000000000ff0000)
					| (ext_csd[EXT_CSD_SEC_CNT + 3] << 24 &
					   0x00000000ff000000);
				capacity *= MMC_MAX_BLOCK_LEN;
				sprd_mmc->capacity_user = capacity;
			}
		} else {
			int erase_gsz, erase_gmul;
			erase_gsz = (sprd_mmc->csd[2] & 0x00007c00) >> 10;
			erase_gmul = (sprd_mmc->csd[2] & 0x000003e0) >> 5;
			sprd_mmc->erase_grp_size = (erase_gsz + 1)
				* (erase_gmul + 1) * 2;
		}

		sprd_mmc->hc_wp_grp_size = 1024
			* ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE]
			* ext_csd[EXT_CSD_HC_WP_GRP_SIZE];

		sprd_mmc->wr_rel_set = ext_csd[EXT_CSD_WR_REL_SET];
		sdhci_printf(!sprd_mmc->in_scan, "mmc->part_num = %x, erase_grp_size \
				= 0x%x\n", sprd_mmc->part_num, sprd_mmc->erase_grp_size);

#ifdef CONFIG_MMC_RST_N_FUNCTION
		if (ext_csd[EXT_CSD_RST_N_FUNCTION] == 0) {
			errorf("RST_n signal is will be permanently enabled\n");
			mmc_set_rst_n_function(sprd_mmc, 0x1);
		} else if (ext_csd[EXT_CSD_RST_N_FUNCTION] == 1) {
			errorf("RST_n signal(0x%x) has already be permanently enabled\n",
			       ext_csd[EXT_CSD_RST_N_FUNCTION]);
		} else if (ext_csd[EXT_CSD_RST_N_FUNCTION] == 2) {
			errorf("The eMMC RST_n signal is permanently disnabled\n");
		}
#endif

#ifdef CONFIG_EMMC_WP
        errorf("jinqiang  mmc_enable_pwr_wp\n");
		err = mmc_enable_pwr_wp(sprd_mmc);
		if (err)
			errorf("%s: emmc power-up protection can't be eanbled\n",
			       __func__);
	//	sprd_mmc->wp_enable = !err;
		sprd_mmc->wp_enable =0;
		//part_protect_init(ENABLE_WRITE_PROTECT);
#endif

#ifdef CONFIG_MMC_TEST
		sprd_mmc->erase_mem_cont = ext_csd[EXT_CSD_ERASE_MEM_CONT];
#endif
	}

	err = sprd_mmc_set_capacity(sprd_mmc, sprd_mmc->part_num);
	if (err) {
		errorf("mmc_set_capacityerr=%x\n", err);
		return err;
	}

	sprd_mmc->host_caps = sprd_mmc->cfg->host_caps;

	if (IS_SD(sprd_mmc)) {
		err = sd_get_capabilities(sprd_mmc);
		if (err)
			return err;

		err = sd_select_mode_and_width(sprd_mmc, sprd_mmc->card_caps);
		if (err)
			return err;
	} else {
		err = mmc_get_capabilities(sprd_mmc);
		if (err)
			return err;

		err = mmc_select_mode_and_width(sprd_mmc, sprd_mmc->card_caps);
		if (err)
			return err;
	}

	sdhci_printf(!sprd_mmc->in_scan, "mmc speed mode: %s(freq:%dMHz),blksz:%d,\
			card_caps:0x%x\n", mmc_speed_mode_type(sprd_mmc->selected_mode),
		sprd_mmc->tran_speed / 1000000, sprd_mmc->read_bl_len, sprd_mmc->card_caps);

	sprd_mmc->block_dev.target_lun = 0;
	sprd_mmc->block_dev.dev_type = 0;
	sprd_mmc->block_dev.blksz = sprd_mmc->read_bl_len;
	sprd_mmc->block_dev.log2blksz = LOG2(sprd_mmc->block_dev.blksz);
	sprd_mmc->block_dev.lba = lldiv(sprd_mmc->capacity, sprd_mmc->read_bl_len);

	fixup_device_value = fixup_device_judge(sprd_mmc);
	sprintf(sprd_mmc->block_dev.vendor_model, "Man %06x Snr %04x%04x",
		sprd_mmc->cid[0] >> 24, (sprd_mmc->cid[2] & 0xffff),
		(sprd_mmc->cid[3] >> 16) & 0xffff);
	sprintf(sprd_mmc->block_dev.product_model, "%c%c%c%c%c%c", sprd_mmc->cid[0] & 0xff,
		(sprd_mmc->cid[1] >> 24), (sprd_mmc->cid[1] >> 16) & 0xff,
		(sprd_mmc->cid[1] >> 8) & 0xff, sprd_mmc->cid[1] & 0xff,
		(sprd_mmc->cid[2] >> 24) & 0xff);
	sprintf(sprd_mmc->block_dev.firmware_revision, "%d.%d", (sprd_mmc->cid[2] >> 20) & 0xf,
		(sprd_mmc->cid[2] >> 16) & 0xf);

	init_part(&sprd_mmc->block_dev);

	sdhci_printf(!sprd_mmc->in_scan, "mmc startup end\n");

	return 0;
}

static int _mmc_send_if_cond(struct mmc *mmc)
{
	struct mmc_cmd cmd = {0};
	int err;

	cmd.cmdidx = SD_CMD_SEND_IF_COND;
	cmd.cmdarg = ((mmc->cfg->voltages & 0xff8000) != 0) << 8 | 0xaa;
	cmd.resp_type = MMC_RSP_R7;
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;

	if ((cmd.response[0] & 0xff) != 0xaa)
		return UNUSABLE_ERR;

	mmc->version = SD_VERSION_2;

	return 0;
}

int emmc_scan_delay_value(uint speed_mode, uint32_t start_blk, u32 *delay_value)
{
	struct mmc *mmc;
	int err = 0;
	uint host_caps;
	struct sprd_sdhci_host *host;

	mmc = find_mmc_device(EMMC);
	if (!mmc)
		return -ENOTSUP;

	host = mmc->priv;
	mmc->in_scan = true;
	/* store caps
	 * and clear mmc->host_caps for further setting
	 */
	host_caps = mmc->cfg->host_caps;
	mmc->cfg->host_caps &= ~MODE_MASK;
	mmc->cfg->host_caps |= MMC_MODE_HS;//reinit in HS mode, save time

	if (emmc_scan_init(mmc, speed_mode)) {
		mmc->cfg->host_caps &= ~MODE_MASK;
		mmc->cfg->host_caps |= host_caps;//restore mmc->cfg->host_caps
		mmc->in_scan = false;
		return -ENOTSUP;
	}

	err = mmc_scan_delay_value(mmc, start_blk, delay_value, speed_mode);
	mmc->cfg->host_caps &= ~MODE_MASK;
	mmc->cfg->host_caps |= host_caps;//restore mmc->cfg->host_caps
	mmc->in_scan = false;
	return err;
}

int sd_scan_delay_value(uint host, uint speed_mode, u32 *delay_value)
{
	//not ready
	return 0;
}

int sdio_scan_delay_value(uint host, uint speed_mode, u32 *delay_value)
{
	//not ready
	return 0;
}

int mmc_scan_delay(uint host, uint slave, uint speed_mode, uint32_t start_blk, u32 *delay_value)
{
	if (host == MMC) {
		dprintf(INFO,"emmc scan speed_mode: %x\r\n", speed_mode);
		return emmc_scan_delay_value(speed_mode, start_blk, delay_value);
	}

	if (slave == SD) {
		dprintf(INFO,"sd scan speed_mode: %x\r\n", speed_mode);
		return sd_scan_delay_value(host, speed_mode, delay_value);
	}

	if (slave == SDIO) {
		dprintf(INFO,"sdio scan speed_mode: %x\r\n", speed_mode);
		return sdio_scan_delay_value(host, speed_mode, delay_value);
	}

	return -ENOTSUP;
}

int mmc_finished_scan_delay(uint host, uint slave)
{
	if (host == MMC) {//finish scan, reinit
		mmc_initialize();
		board_mmc_init();
		do_preinit();
	}

	return 0;
}

static void init_mmc_interface(struct mmc *sprd_mmc)
{
	sprd_mmc->block_dev.api_type = API_TYPE_MMC;
	sprd_mmc->block_dev.dev_num = cur_dev_num++;
	sprd_mmc->block_dev.removable = 1;
	sprd_mmc->block_dev.block_sync = NULL;
#ifdef CONFIG_EMMC_WP
	sprd_mmc->block_dev.block_pwr_wp = mmc_set_pwr_wp;
#endif
	sprd_mmc->block_dev.backstage_block_write = emmc_write_backstage;
	sprd_mmc->block_dev.backstage_write_query = emmc_query_backstage;
	sprd_mmc->block_dev.backstage_block_read = emmc_read_backstage;
	sprd_mmc->block_dev.backstage_read_query = emmc_query_read_backstage;
	sprd_mmc->block_dev.block_read = mmc_bread;
	sprd_mmc->block_dev.block_write = mmc_bwrite;
	sprd_mmc->block_dev.block_erase = mmc_berase;
}

struct mmc *mmc_create(const struct mmc_config *cfg, void *priv)
{
	struct mmc *sprd_mmc;

	if (cfg == NULL || cfg->ops == NULL || cfg->ops->send_cmd == NULL ||
	    cfg->f_min == 0 || cfg->f_max == 0 || cfg->b_max == 0)
		return NULL;

	sprd_mmc = calloc(1, sizeof(*sprd_mmc));
	if (sprd_mmc == NULL)
		return NULL;

	sprd_mmc->cfg = cfg;
	sprd_mmc->priv = priv;
	sprd_mmc->dsr_imp = 0;
	sprd_mmc->dsr = 0xffffffff;
	init_mmc_interface(sprd_mmc);
	sprd_mmc->block_dev.part_type = sprd_mmc->cfg->part_type;

	INIT_LIST_HEAD(&sprd_mmc->link);

	list_add_tail(&mmc_devices, &sprd_mmc->link);

	return sprd_mmc;
}

static void mmc_set_part_num_user_part(struct mmc *mmc)
{
	mmc->part_num = 0;
}

block_dev_desc_t *mmc_get_dev(int dev)
{
	struct mmc *mmc = find_mmc_device(dev);

	if (!mmc)
		return NULL;

	if (mmc_init(mmc))
		return NULL;

	return &mmc->block_dev;
}

static void mmc_init_bus_clk(struct mmc *mmc)
{
	int err;

#ifdef CONFIG_MMC_UHS_SUPPORT
	if (mmc->block_dev.dev_num == 1) {
		err = mmc_set_signal_voltage(mmc, MMC_SIGNAL_VOLTAGE_330);
		if (err != 0)
			err = mmc_set_signal_voltage(mmc, MMC_SIGNAL_VOLTAGE_180);
		if (err != 0)
			errorf("mmc:failed to set signal voltage\n");
	}
#endif

	mmc_set_bus_width(mmc, 1);
	mmc_set_clock(mmc, 100000);

}

static inline int mmc_uhs_support(uint caps)
{
#ifdef CONFIG_MMC_UHS_SUPPORT
	return (caps & MMC_UHS_CAPS) ? 1 : 0;
#else
	return 0;
#endif
}

int mmc_start_init(struct mmc *mmc)
{
	int err;
	int sprd_uhs = mmc_uhs_support(mmc->cfg->host_caps);

	if (mmc->has_init)
		return 0;

	if (!mmc->cfg->ops->init)
		return 0;

	err = mmc->cfg->ops->init(mmc);
	if (err)
		return err;

	mmc->ddr_mode = 0;

	mmc_init_bus_clk(mmc);

	err = _mmc_go_idle(mmc);
	if (err) {
		errorf("mmc go idle(cmd0) error\n");
		return err;
	}

	mmc_set_part_num_user_part(mmc);

	/* no need to error check , or cause fail */
	_mmc_send_if_cond(mmc);

	err = _sd_send_op_cond(mmc, sprd_uhs);
	if (err == TIMEOUT) {
		err = mmc_send_op_cond(mmc);
		if (err) {
			errorf("Card did not respond to voltage select!\n");
			return UNUSABLE_ERR;
		}
	}

	if (!err)
		mmc->init_in_progress = 1;

	return err;
}

static int mmc_complete_init(struct mmc *mmc)
{
	int err = 0;

	mmc->init_in_progress = 0;
	if (mmc->op_cond_pending)
		err = _mmc_complete_op_cond(mmc);

	if (!err)
		err = mmc_startup(mmc);
	if (err)
		mmc->has_init = 0;
	else
		mmc->has_init = 1;
	return err;
}

int mmc_init(struct mmc *mmc)
{
	int err = 0;
	unsigned start;

	if (mmc->has_init)
		return 0;

	start = get_timer(0);

	if (!mmc->init_in_progress)
		err = mmc_start_init(mmc);

	if (!err)
		err = mmc_complete_init(mmc);

	sdhci_printf(!mmc->in_scan, "mmc init: %d, cost time %lu\n", err, get_timer(start));
	return err;
}

void print_mmc_devices(char separator)
{
	struct mmc *m;
	struct list_head *entry;
	char *mmc_type;

	list_for_each(entry, &mmc_devices) {
		m = list_entry(entry, struct mmc, link);

		if (m->has_init)
			mmc_type = IS_SD(m) ? "SD" : "eMMC";
		else
			mmc_type = NULL;
		dprintf(INFO,"%s: %d", m->cfg->name, m->block_dev.dev_num);

		if (mmc_type)
			dprintf(INFO," (%s)", mmc_type);

		if (entry->next != &mmc_devices) {
			dprintf(INFO,"%c", separator);
			if (separator != '\n')
				puts(" ");
		}
	}

	dprintf(INFO,"\n");
}

static void do_preinit(void)
{
	struct mmc *m;
	struct list_head *entry;

	list_for_each(entry, &mmc_devices) {
		m = list_entry(entry, struct mmc, link);

		if (m->preinit)
			mmc_start_init(m);
	}
}

#ifdef CONFIG_MMC_TEST
int board_mmc_test(struct mmc *mmc)
{
	int i, j;
	ulong err = 0;
	lbaint_t start = 0x800000;	/* start data address: 4GB */
	uint mrw_size = MMC_MAX_BLOCK_LEN * 16;		/* 8MByte */
	int wp_cnt = 2;	/* wp_cnt <= 32 (32 * 8Mbyte) */
	u8 cont;
	u8 srd_tst_dat = 0xa5;
	u8 mrd_tst_dat = 0xaa;

	ALLOC_CACHE_ALIGN_BUFFER(u8, wr_buf, MMC_MAX_BLOCK_LEN);
	ALLOC_CACHE_ALIGN_BUFFER(u8, rd_buf, MMC_MAX_BLOCK_LEN);
	ALLOC_CACHE_ALIGN_BUFFER(u8, mrw_buf, mrw_size);

#ifdef CONFIG_MMC_POWP_SUPPORT
	lbaint_t wp_start;
	u8 wp_tst_dat = 0x11;	/* can't be equal to mrd_tst_dat */

	wp_start = start;
#endif

	dprintf(INFO,"%s: test start addr: 0x" LBAF "\n", __func__, start);
	for (i = 0; i < MMC_MAX_BLOCK_LEN; i++) {
		wr_buf[i] = srd_tst_dat;
		rd_buf[i] = 0;
	}
	/* single block erase test */
	dprintf(INFO,"%s: emmc erased memory content shall be 0x%x\n",
	       __func__, mmc->erase_mem_cont);
	if (mmc->erase_mem_cont)
		cont = 0xff;
	else
		cont = 0;

	mmc->block_dev.block_write(EMMC, start, 1, wr_buf);
	mmc->block_dev.block_erase(EMMC, start, 1);
	mmc->block_dev.block_read(EMMC, start, 1, rd_buf);
	for (i = 0, err = 0; i < MMC_MAX_BLOCK_LEN; i++) {
		if (rd_buf[i] != cont) {
			errorf("single block erase test fail\n");
			err = 1;
			break;
		}
	}
	if (!err)
		dprintf(INFO,"%s: single block erase test is successful\n", __func__);

	/* single block read test */
	mmc->block_dev.block_write(EMMC, start, 1, wr_buf);
	mmc->block_dev.block_read(EMMC, start, 1, rd_buf);
	for (i = 0, err = 0; i < MMC_MAX_BLOCK_LEN; i++) {
		if (rd_buf[i] != srd_tst_dat) {
			errorf("single block read test fail\n");
			err = 1;
			break;
		}
	}
	if (!err)
		dprintf(INFO,"%s: single block read test is successful\n", __func__);

	/* multiple block read test */
	for (i = 0; i < wp_cnt; i++) {
		memset(mrw_buf, mrd_tst_dat, mrw_size);
		mmc->block_dev.block_write(EMMC, start , 16, mrw_buf);
		mmc->block_dev.block_read(EMMC, start, 16, mrw_buf);
		for (j = 0; j < mrw_size; j++) {
			if (mrw_buf[j] != mrd_tst_dat)
				goto mul_fail;
		}
		start += 0x4000;
	}
	dprintf(INFO,"%s: multiple block read test is successful\n", __func__);

#ifdef CONFIG_MMC_POWP_SUPPORT
	/* emmc power-on write protection test */
	dprintf(INFO,"%s: emm write protection test start addr: 0x" LBAF "\n",
	       __func__, wp_start);
	err = mmc->block_dev.block_pwr_wp(EMMC, wp_start, wp_cnt);
	if (err) {
		errorf("emmc power-on write protection set fail\n");
		goto wp_fail;
	}

	for (i = 0; i < wp_cnt; i++) {
		memset(mrw_buf, wp_tst_dat, mrw_size);
		mmc->block_dev.block_write(EMMC, wp_start , 16, mrw_buf);
		mmc->block_dev.block_read(EMMC, wp_start, 16, mrw_buf);
		for (j = 0; j < mrw_size; j++) {
			if (mrw_buf[j] != mrd_tst_dat)
				goto wp_fail;
		}
		wp_start += 0x4000;
	}
	dprintf(INFO,"%s: emmc write protect test is successful\n", __func__);
#endif

	return 0;

mul_fail:
	errorf("%s: emmc multiple block read/write test fail\n", __func__);
#ifdef CONFIG_MMC_POWP_SUPPORT
wp_fail:
	errorf("%s: emmc power-on write protection test fail\n", __func__);
#endif
	return -1;
}
#endif /* CONFIG_MMC_TEST */

int sprd_host_reinit(int sdio_type)
{
	struct sdio_base_info *sprd_host_info;
	struct mmc *mmc;

	/*
	 * 1: sd, 0: emmc
	 * Setting cur_dev_num to 1 is used for nand.
	 * We need a good idea to deal with the cur_dev_num for nand.
	 */
	sprd_host_info = get_sdcontrol_info(sdio_type);

	mmc = find_mmc_device(sdio_type);
	if (mmc && mmc->has_init == 1) {
		mmc->clock = 0;
		mmc_set_ios(mmc);
		if (sprd_host_info->ldo_core != 0)
			regulator_disable(sprd_host_info->ldo_core);
		if ((sprd_host_info->ldo_io != 0) && (EMMC != sdio_type))
			regulator_disable(sprd_host_info->ldo_io);
		CHIP_REG_AND(sprd_host_info->ahb_enable_reg,
					 ~(sprd_host_info->ahb_enable_bit));
	}

	mdelay(30);

	if (sprd_host_info->ldo_core != 0)
		regulator_enable(sprd_host_info->ldo_core);
	if (sprd_host_info->ldo_io != 0)
		regulator_enable(sprd_host_info->ldo_io);

	CHIP_REG_SET(sprd_host_info->baseclk_reg, sprd_host_info->baseclk_mask);

	dprintf(INFO,"mmc ldo_core:%s, ldo_io:%s\n",
	       sprd_host_info->ldo_core, sprd_host_info->ldo_io);
	udelay(1000);
	CHIP_REG_OR(sprd_host_info->ahb_enable_reg,
		    sprd_host_info->ahb_enable_bit);
	CHIP_REG_OR(sprd_host_info->ahb_reset_reg,
		    sprd_host_info->ahb_reset_bit);
	CHIP_REG_AND(sprd_host_info->ahb_reset_reg,
			~(sprd_host_info->ahb_reset_bit));
	if (sprd_host_info->aon_clk_reg != 0)
		CHIP_REG_OR(sprd_host_info->aon_clk_reg,
			sprd_host_info->aon_clk_bit);
	else
		dprintf(INFO,"%s, no aon_clk_reg\n", __func__);

	return 0;
}

int sprd_host_init(int sdio_type)
{
	uint32_t ret;
	struct sdio_base_info *sprd_host_info;
	sprd_host_info = get_sdcontrol_info(sdio_type);

	if (sprd_host_info->ldo_core != 0)
		regulator_enable(sprd_host_info->ldo_core);
	if (sprd_host_info->ldo_io != 0)
		regulator_enable(sprd_host_info->ldo_io);

	CHIP_REG_SET(sprd_host_info->baseclk_reg, sprd_host_info->baseclk_mask);

	dprintf(INFO,"mmc ldo_core:%s, ldo_io:%s\n",
	       sprd_host_info->ldo_core, sprd_host_info->ldo_io);
	udelay(1000);
	CHIP_REG_OR(sprd_host_info->ahb_enable_reg,
		    sprd_host_info->ahb_enable_bit);
	CHIP_REG_OR(sprd_host_info->ahb_reset_reg,
		    sprd_host_info->ahb_reset_bit);
	CHIP_REG_AND(sprd_host_info->ahb_reset_reg,
			~(sprd_host_info->ahb_reset_bit));
	if (sprd_host_info->aon_clk_reg != 0)
		CHIP_REG_OR(sprd_host_info->aon_clk_reg,
			sprd_host_info->aon_clk_bit);
	else
		dprintf(INFO,"%s, no aon_clk_reg\n", __func__);

	ret = sprd_sdhci_init(sprd_host_info->regbase,
		sprd_host_info->maxclk, sprd_host_info->minclk, 0);

	dprintf(INFO,"%s return %d\n", __func__, ret);
	return ret;
}

void sprd_mmc_exit(int sdio_type)
{
	struct sdio_base_info *sprd_host_info = get_sdcontrol_info(sdio_type);
	struct mmc *mmc = find_mmc_device(sdio_type);

	/* TODO:  Now only support SD card power off */
	if (!mmc || (mmc && sdio_type != SD))
		return;

	mmc_set_bus_width(mmc, 1);
	mmc_set_clock(mmc, 0);

	if (sprd_host_info) {
		regulator_disable(sprd_host_info->ldo_core);
		regulator_disable(sprd_host_info->ldo_io);
	}

	dprintf(INFO,"%s: sd power off\n", __func__);
}

struct mmc *board_sd_init(void)
{
	struct mmc *mmc;
	int ret, sd_dev;

	/*
	 * 1: sd, 0: emmc
	 * Setting cur_dev_num to 1 is used for nand.
	 * We need a good idea to deal with the cur_dev_num for nand.
	 */
	cur_dev_num = 1;
	sd_dev = cur_dev_num;
	sprd_host_init(sd_dev);
	mmc = find_mmc_device(sd_dev);
	if (mmc) {
		ret = mmc_init(mmc);
		if (ret < 0) {
			errorf("sdcard init failed %d\n", ret);
			return NULL;
		}
	} else {
		errorf("no sdcard found\n");
		return NULL;
	}

	return mmc;
}

int mmc_initialize(void)
{
	INIT_LIST_HEAD(&mmc_devices);
	cur_dev_num = 0;

	return 0;
}

int board_mmc_init(void)
{
	struct mmc *mmc;
	uint32_t ret;

	sprd_host_init(EMMC);
	mmc = find_mmc_device(EMMC);
	if (mmc) {
		mmc->in_scan = false;
		ret = mmc_init(mmc);
	} else {
		errorf("no emmc found\n");
		return -1;
	}

	/* code as below for emmc test */
	if (!ret)
		mmc_switch_part(EMMC, 0);

#ifdef CONFIG_MMC_TEST
	board_mmc_test(mmc);
#endif
	return 0;
}

int board_mmc_initialize(void)
{
	dprintf(CRITICAL,"%s entry\n", __func__);

	mmc_initialize();

	board_mmc_init();

	print_mmc_devices(',');

	do_preinit();

	return 0;
}

int mmc_set_rst_n_function(struct mmc *mmc, u8 enable)
{
	return mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_RST_N_FUNCTION,
			  enable);
}

int mmc_enable_pwr_wp(struct mmc *mmc)
{
	int err = 0;
	ALLOC_CACHE_ALIGN_BUFFER(u8, ext_csd, MMC_MAX_BLOCK_LEN);

	err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL,
			 EXT_CSD_ERASE_GROUP_DEF, 1);
	if (err) {
		errorf("%s: set EXT_CSD_ERASE_GROUP_DEF fail, err: %d\n",
		       __func__, err);
		goto out;
	}

	err = mmc_send_ext_csd(mmc, ext_csd);
	if (err) {
		errorf("%s-->mmc_send_ext_csd fail, err: %d\n", __func__, err);
		goto out;
	}
	if (ext_csd[EXT_CSD_USER_WP] & EXT_CSD_US_PWR_WP_DIS) {
		errorf("%s: power-on protection can't be enabled\n", __func__);
		goto out;
	}

	err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_USER_WP,
					EXT_CSD_US_PWR_WP_EN);
	if (err) {
		errorf("%s-->mmc_switch fail, err: %d\n", __func__, err);
		goto out;
	}

	err = mmc_send_ext_csd(mmc, ext_csd);
	if (err) {
		errorf("%s-->mmc_send_ext_csd fail, err: %d\n", __func__, err);
		goto out;
	}

	mmc->hc_wp_grp_size = 1024 *
		ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE] *
		ext_csd[EXT_CSD_HC_WP_GRP_SIZE];

	dprintf(INFO,"%s: emmc write protection group size: 0x%x\n",
		__func__, mmc->hc_wp_grp_size);

out:
	return err;
}

ulong emmc_write_backstage(int part_num, uint32_t start_block,
			uint32_t num, uint8_t *buf)
{
	struct mmc *mmc = find_mmc_device(EMMC);
	int ret = 0;

	if (!mmc)
		return 0;

	if (mmc->part_num != part_num)
		mmc_switch_part(EMMC, part_num);

	mmc->part_num = part_num;
	ret = mmc_bwrite_backstage(EMMC, start_block, num, buf);

	return ret;
}

ulong emmc_query_backstage(int part_num, uint32_t num, uint8_t *buf)
{
	struct mmc *mmc = find_mmc_device(EMMC);
	int ret = 0;

	if (!mmc)
		return 0;

	if (mmc->part_num != part_num)
		mmc_switch_part(EMMC, part_num);

	mmc->part_num = part_num;

	ret = mmc_query_bwrite_backstage(EMMC, num, buf);
	if (part_num) {
		mmc->part_num = 0;
		mmc_switch_part(EMMC, 0);
	 }

	return ret;
}

ulong emmc_query_read_backstage(int part_num, uint32_t num, uint8_t *buf)
{
	struct mmc *mmc = find_mmc_device(EMMC);
	int ret = 0;

	if (!mmc)
		return 0;

	if (mmc->part_num != part_num)
		mmc_switch_part(EMMC, part_num);

	mmc->part_num = part_num;

	ret = mmc_query_bread_backstage(EMMC, num, buf);
	if (part_num) {
		mmc->part_num = 0;
		mmc_switch_part(EMMC, 0);
	 }

	return ret;
}

static ulong mmc_read_blocks_backstage(struct mmc *mmc,
	lbaint_t start, lbaint_t blkcnt, const void *dst)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	if ((start + blkcnt) > mmc->block_dev.lba) {
		dprintf(INFO,"MMC: block number 0x" LBAF " exceeds max(0x" LBAF ")\n",
		       start + blkcnt, mmc->block_dev.lba);
		return 0;
	}
	if (blkcnt == 0)
		return 0;
	else if (blkcnt == 1)
		cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;
	else
		cmd.cmdidx = MMC_CMD_READ_MULTIPLE_BLOCK;
	if (mmc->high_capacity)
		cmd.cmdarg = start;
	else
		cmd.cmdarg = start * mmc->read_bl_len;

	cmd.resp_type = MMC_RSP_R1;

	data.src = dst;
	data.blocks = blkcnt;
	data.blocksize = mmc->read_bl_len;
	data.flags = MMC_DATA_READ;

	if (sprd_sdhci_send_command_backstage(mmc, &cmd, &data)) {
		errorf("mmc backstage read failed\n");
		return 0;
	}
	return blkcnt;
}

ulong mmc_bread_backstage(int dev_num, lbaint_t start,
	lbaint_t blkcnt, const void *src)
{
	lbaint_t cur = blkcnt;

	struct mmc *mmc = find_mmc_device(dev_num);
	if (!mmc) {
		errorf("no mmc device, return\n");
		return 0;
	}

	if (mmc_card_set_blocklen(mmc, mmc->write_bl_len))
		return 0;
#ifndef SPRD_SPARSE_SUPER_SPEEDUP
	if (blkcnt > mmc->cfg->b_max) {
		errorf("%s can not support more than <%d> blocks\n",
		       __func__, mmc->cfg->b_max);
		return 0;
	}
#endif
	if (mmc_read_blocks_backstage(mmc, start, blkcnt, src) != cur)
		return 0;

	return blkcnt;
}


ulong emmc_read_backstage(int part_num, uint32_t start_block,
			uint32_t num, uint8_t *buf)
{
	struct mmc *mmc = find_mmc_device(EMMC);
	int ret = 0;

	if (!mmc) {
		errorf("no mmc device, return\n");
		return 0;
	}

	if (mmc->part_num != part_num)
		mmc_switch_part(EMMC, part_num);

	mmc->part_num = part_num;
	ret = mmc_bread_backstage(EMMC, start_block, num, buf);

	return ret;
}

u64 emmc_get_capacity(char part_num)
{
	struct mmc *mmc = find_mmc_device(EMMC);
	if (!mmc)
		return 0;

	switch (part_num) {
	case 0:
		return mmc->capacity_user;
	case 1:
	case 2:
		return mmc->capacity_boot;
	case 3:
		return mmc->capacity_rpmb;
	default:
		return 0;
	}
}

u64 mmc_get_hwpartsize(int hwpart)
{
	return emmc_get_capacity((char)hwpart);
}

void mmc_get_cid(char *cid)
{
	struct mmc *mmc = find_mmc_device(EMMC);

	if (!mmc) {
		errorf("mmc is null!\n");
		return;
	}

	memcpy(cid, (char *)mmc->cid, 16);
}

void mmc_get_ext_csd(char *ext_csd)
{
	struct mmc *mmc = find_mmc_device(EMMC);

	if (!mmc) {
		errorf("mmc is null!\n");
		return;
	}

	memcpy(ext_csd, (char *)mmc->ext_csd, 512);
}

