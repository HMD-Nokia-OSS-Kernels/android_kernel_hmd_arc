/*
*  rpmb.c  - unisoc rpmb config
*
*  Copyright (C) 2019 Unisoc Communications Inc.
*  History:
*      2021-07-29 wenchao.chen@unisoc.com
*      Add rpmb.c
*/
#include <config.h>
#include <asm/arch/common.h>
#include <mmc.h>
#include <malloc.h>
#include <rpmb.h>
#include <kernel/spinlock.h>

#ifdef CONFIG_SECBOOT_OPENSSL
#include <openssl/evp.h>
#include <openssl/hmac.h>
#endif

extern spin_lock_t block_rw_lock;
#ifdef CONFIG_SPRD_GICV3
#define SPRD_BLOCK_LOCK(state) spin_lock_saved_state_t state; spin_lock_irqsave(&block_rw_lock, state)
#define SPRD_BLOCK_UNLOCK(state) spin_unlock_irqrestore(&block_rw_lock, state)
#else
#define SPRD_BLOCK_LOCK(state) sprd_spin_lock(&block_rw_lock)
#define SPRD_BLOCK_UNLOCK(state) sprd_spin_unlock(&block_rw_lock)
#endif

#define BLOCK_SIZE 512
#define EMMC  0
#define RPMB_PARTITION 3
#define USER_PARTITION 0
#define MMC_SET_BLOCK_COUNT   23

static void u16_to_bytes(uint16_t raw_data, uint8_t * bytes)
{
	*bytes = (uint8_t) (raw_data >> 8);
	*(bytes + 1) = (uint8_t) raw_data;
}

static void bytes_to_u16(uint8_t * bytes, uint16_t * raw_data)
{
	*raw_data = (uint16_t) ((*bytes << 8) + *(bytes + 1));
}

static void bytes_to_u32(uint8_t * bytes, uint32_t * raw_data)
{
	*raw_data = (uint32_t) ((*(bytes) << 24) +
			   (*(bytes + 1) << 16) +
			   (*(bytes + 2) << 8) + (*(bytes + 3)));
}

static void u32_to_bytes(uint32_t raw_data, uint8_t * bytes)
{
	*bytes = (uint8_t) (raw_data >> 24);
	*(bytes + 1) = (uint8_t) (raw_data >> 16);
	*(bytes + 2) = (uint8_t) (raw_data >> 8);
	*(bytes + 3) = (uint8_t) raw_data;
}

void rpmb_write_cmd(struct mmc *mmc, uint blkcnt, uint blocksize,
		    const void *src)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	int timeout = 1000;
	cmd.cmdidx = MMC_CMD_WRITE_MULTIPLE_BLOCK;
	cmd.resp_type = MMC_RSP_R1;

	data.src = src;
	data.blocks = blkcnt;
	data.blocksize = blocksize;
	data.flags = MMC_DATA_WRITE;

	if (mmc_send_cmd(mmc, &cmd, &data)) {
		errorf("rpmb_write_cmd failed\n");
	}
}

void rpmb_read_cmd(struct mmc *mmc, void *dst, uint blkcnt)
{
	struct mmc_cmd cmd;
	struct mmc_data data;

	cmd.cmdidx = MMC_CMD_READ_MULTIPLE_BLOCK;
	cmd.resp_type = MMC_RSP_R1;

	data.dest = dst;
	data.blocks = blkcnt;
	data.blocksize = mmc->read_bl_len;
	data.flags = MMC_DATA_READ;

	if (mmc_send_cmd(mmc, &cmd, &data))
		errorf("rpmb_read_cmd error \n");
}

int mmc_set_blockcount(struct mmc *mmc, uint blockcount,
		       bool is_rel_write)
{
	struct mmc_cmd cmd = { 0 };

	cmd.cmdidx = MMC_SET_BLOCK_COUNT;
	cmd.cmdarg = blockcount & 0x0000FFFF;
	if (is_rel_write)
		cmd.cmdarg |= 1 << 31;
	cmd.resp_type = MMC_RSP_R1;

	return mmc_send_cmd(mmc, &cmd, NULL);
}

/**@retrun 0 get rpmb package successful*/
int check_mmc_rpmb_key(uint8_t *package, int package_size)
{
	//Must match in tos
	uint8_t nonce[RPMB_NONCE_SIZE] = {0xA5,0x5A,0xFF,0x00,0xBE,0xEF,0xBE,0xEF,0xBE,0xEF,0xBE,0xEF,0x00,0xFF,0x5A,0xA5};
	struct rpmb_data_frame *data_frame;
	uint16_t msg_type;
	uint16_t op_result;
	uint32_t writecount;
	struct mmc *mmc = find_mmc_device(0);
	if (NULL == mmc) {
		errorf("%s mmc is null\n", __func__);
		return -1;
	}

	if (NULL == package || package_size < sizeof(struct rpmb_data_frame)) {
		errorf("%s parameter package is invalid\n", __func__);
		return -1;
	}

	data_frame = package;
	memset(data_frame, 0, package_size);
	msg_type = RPMB_MSG_TYPE_REQ_WRITE_COUNTER_VAL_READ;
	u16_to_bytes(msg_type, data_frame->msg_type);
	//Must match in tos
	memcpy(data_frame->nonce, nonce, RPMB_NONCE_SIZE);
	mmc_switch_part(EMMC, RPMB_PARTITION);
	mmc_set_blockcount(mmc, RPMB_BLOCK_COUNT, 0);
	rpmb_write_cmd(mmc, RPMB_BLOCK_COUNT, BLOCK_SIZE, data_frame);
	memset(data_frame, 0, package_size);
	mmc_set_blockcount(mmc, RPMB_BLOCK_COUNT, 0);
	rpmb_read_cmd(mmc, data_frame, RPMB_BLOCK_COUNT);
	mmc_switch_part(EMMC, USER_PARTITION);

	return 0;
}

/**@retrun 0 rpmb key unwritten*/
int is_wr_mmc_rpmb_key(void)
{
	struct rpmb_data_frame *data_frame;
	uint16_t msg_type;
	uint16_t op_result;
	uint32_t writecount;
	int result;
	struct mmc *mmc = find_mmc_device(0);	//0 is emmc
	if (NULL == mmc) {
		errorf("%s mmc is null\n", __func__);
		return -1;
	}

	msg_type = RPMB_MSG_TYPE_REQ_WRITE_COUNTER_VAL_READ;
	data_frame = (uint8_t *) malloc(RPMB_DATA_FRAME_SIZE);
	if (NULL == data_frame) {
		errorf("%s malloc error\n", __func__);
		return -1;
	}

	memset(data_frame, 0, RPMB_DATA_FRAME_SIZE);
	u16_to_bytes(msg_type, data_frame->msg_type);
	mmc_switch_part(EMMC, RPMB_PARTITION);
	mmc_set_blockcount(mmc, RPMB_BLOCK_COUNT, 0);
	rpmb_write_cmd(mmc, RPMB_BLOCK_COUNT, BLOCK_SIZE, data_frame);
	memset(data_frame, 0, RPMB_DATA_FRAME_SIZE);
	mmc_set_blockcount(mmc, RPMB_BLOCK_COUNT, 0);
	rpmb_read_cmd(mmc, data_frame, RPMB_BLOCK_COUNT);

/*result check*/
	bytes_to_u16(data_frame->op_result, &op_result);
	bytes_to_u16(data_frame->msg_type, &msg_type);
	if (RPMB_RES_NO_AUTH_KEY == op_result) {
		dprintf(INFO,"%s rpmb key not write\n", __func__);
		result = 0;
	} else {
		errorf("%s rpmb key has been written\n", __func__);
		result = -1;
	}

	free(data_frame);
	mmc_switch_part(EMMC, USER_PARTITION);

	return result;
}

uint32_t mmc_rpmb_read_writecount(void)
{
	struct rpmb_data_frame *data_frame;
	uint16_t msg_type;
	uint16_t op_result;
	uint32_t writecount;
	struct mmc *mmc = find_mmc_device(0);	//0 is emmc
	if (NULL == mmc)
		return 1;

	msg_type = RPMB_MSG_TYPE_REQ_WRITE_COUNTER_VAL_READ;
	data_frame = (uint8_t *) malloc(RPMB_DATA_FRAME_SIZE);
	if (NULL == data_frame) {
		errorf("%s malloc error\n", __func__);
		return -1;
	}

	memset(data_frame, 0, RPMB_DATA_FRAME_SIZE);
	u16_to_bytes(msg_type, data_frame->msg_type);
	mmc_switch_part(EMMC, RPMB_PARTITION);
	mmc_set_blockcount(mmc, RPMB_BLOCK_COUNT, 0);
	rpmb_write_cmd(mmc, RPMB_BLOCK_COUNT, BLOCK_SIZE, data_frame);
	memset(data_frame, 0, RPMB_DATA_FRAME_SIZE);
	mmc_set_blockcount(mmc, RPMB_BLOCK_COUNT, 0);
	rpmb_read_cmd(mmc, data_frame, RPMB_BLOCK_COUNT);

/*result check*/
	bytes_to_u16(data_frame->op_result, &op_result);
	bytes_to_u16(data_frame->msg_type, &msg_type);
	if ((op_result == RPMB_RESULT_OK)
	    && (msg_type == RPMB_MSG_TYPE_RESP_WRITE_COUNTER_VAL_READ)) {
		bytes_to_u32(data_frame->write_counter, &writecount);
		dprintf(INFO,"read write count successed\n");
		free(data_frame);
		mmc_switch_part(EMMC, USER_PARTITION);
		return writecount;
	} else {
		dprintf(INFO," read write count:0x%x ,msg_type:0x%x\n", op_result,
		       msg_type);
	}

	free(data_frame);
	mmc_switch_part(EMMC, USER_PARTITION);
	return 1;
}

/*
blk_data: for save read data;
blk_index: the block index for read;
block_count: the read count;
success return 0 ;
*/
uint8_t mmc_rpmb_blk_read(char *blk_data, uint16_t blk_index, uint8_t block_count)
{
	struct rpmb_data_frame *data_frame;
	struct rpmb_data_frame *resp_buf;
	uint16_t msg_type;
	uint16_t op_result;
	struct mmc *mmc = find_mmc_device(0);
	if (NULL == mmc)
		return 1;

	if (blk_data == NULL) {
		errorf("rpmb_blk_read null \n");
		return 1;
	}

	msg_type = RPMB_MSG_TYPE_REQ_AUTH_DATA_READ;
	data_frame = (uint8_t *) malloc(RPMB_DATA_FRAME_SIZE);
	if (data_frame == NULL) {
		errorf("%s malloc error\n", __func__);
		return 1;
	}

	memset(data_frame, 0, RPMB_DATA_FRAME_SIZE);
	u16_to_bytes(msg_type, data_frame->msg_type);
	u16_to_bytes(blk_index, data_frame->address);
	mmc_switch_part(EMMC, RPMB_PARTITION);
	mmc_set_blockcount(mmc, RPMB_BLOCK_COUNT, 0);
	rpmb_write_cmd(mmc, RPMB_BLOCK_COUNT, BLOCK_SIZE, data_frame);
	resp_buf = (uint8_t *) malloc(RPMB_DATA_FRAME_SIZE * block_count);
	if (resp_buf == NULL) {
		errorf("resp_buf null\n");
		free(data_frame);
		return 1;
	}

	memset(resp_buf, 0, RPMB_DATA_FRAME_SIZE * block_count);
	mmc_set_blockcount(mmc, block_count, 0);
	rpmb_read_cmd(mmc, resp_buf, block_count);

/*result check*/
	bytes_to_u16((resp_buf + block_count - 1)->op_result, &op_result);
	bytes_to_u16((resp_buf + block_count - 1)->msg_type, &msg_type);
	if ((op_result == RPMB_RESULT_OK)
	    && (msg_type == RPMB_MSG_TYPE_RESP_AUTH_DATA_READ)) {
		uint8_t i = 0;
		for (i = 0; i < block_count; i++)
			memcpy((blk_data + i * RPMB_DATA_SIZE),
			       ((uint8_t *) (resp_buf + i) +
				RPMB_STUFF_DATA_SIZE + RPMB_KEY_MAC_SIZE),
			       RPMB_DATA_SIZE);
		dprintf(INFO,"read  successed\n");
		free(resp_buf);
		free(data_frame);
		mmc_switch_part(EMMC, USER_PARTITION);
		return 0;
	} else {
		dprintf(INFO," read write count:0x%x ,msg_type:0x%x\n", op_result,
		       msg_type);
	}

	free(resp_buf);
	free(data_frame);
	mmc_switch_part(EMMC, USER_PARTITION);

	return 1;
}

/**
write_data: data for write
blk_index: the block will be write to;
success return 0
*/
uint8_t mmc_rpmb_blk_write(char *write_data, uint8_t *key, uint16_t blk_index)
{
	struct rpmb_data_frame *data_frame;
	uint16_t msg_type;
	uint16_t op_result;
	uint16_t block_count = 1;
	uint32_t writecount;
	struct mmc *mmc = find_mmc_device(0);	//0 is emmc

	if (NULL == mmc)
		return 1;

	if (write_data == NULL)	//|| (RPMB_DATA_SIZE != strlen(write_data)))
		return 1;

	/*for write rpmb key req */
	msg_type = RPMB_MSG_TYPE_REQ_AUTH_DATA_WRITE;
	data_frame = (uint8_t *) malloc(RPMB_DATA_FRAME_SIZE);
	if (data_frame == NULL) {
		errorf("%s malloc error\n", __func__);
		return 1;
	}

	memset(data_frame, 0, RPMB_DATA_FRAME_SIZE);
	writecount = mmc_rpmb_read_writecount();
	u16_to_bytes(msg_type, data_frame->msg_type);
	u16_to_bytes(blk_index, data_frame->address);
	u16_to_bytes(block_count, data_frame->block_count);
	u32_to_bytes(writecount, data_frame->write_counter);
	memcpy(data_frame->data, write_data, RPMB_DATA_SIZE);

#ifdef CONFIG_SECBOOT_OPENSSL
	//for key_mac calc;
	HMAC(EVP_sha256(), key, 32, data_frame->data, 284, data_frame->key_mac, 32);
#endif

	mmc_switch_part(EMMC, RPMB_PARTITION);
	mmc_set_blockcount(mmc, RPMB_BLOCK_COUNT, 1);
	rpmb_write_cmd(mmc, RPMB_BLOCK_COUNT, BLOCK_SIZE, data_frame);

	/*for read result req */
	memset(data_frame, 0, RPMB_DATA_FRAME_SIZE);
	msg_type = RPMB_MSG_TYPE_REQ_RESULT_READ;
	u16_to_bytes(msg_type, data_frame->msg_type);
	mmc_set_blockcount(mmc, RPMB_BLOCK_COUNT, 0);
	rpmb_write_cmd(mmc, RPMB_BLOCK_COUNT, BLOCK_SIZE, data_frame);
	memset(data_frame, 0, RPMB_DATA_FRAME_SIZE);
	mmc_set_blockcount(mmc, RPMB_BLOCK_COUNT, 0);
	rpmb_read_cmd(mmc, data_frame, RPMB_BLOCK_COUNT);

/*result check*/
	bytes_to_u16(data_frame->op_result, &op_result);
	bytes_to_u16(data_frame->msg_type, &msg_type);
	if ((op_result == RPMB_RESULT_OK)
	    && (msg_type == RPMB_MSG_TYPE_RESP_AUTH_DATA_WRITE)) {
		dprintf(INFO," data  write successed\n");
		free(data_frame);
		mmc_switch_part(EMMC, USER_PARTITION);
		return 0;
	} else {
		errorf(" data write fail op_result:0x%x ,msg_type:0x%x\n",
		       op_result, msg_type);
	}

	free(data_frame);
	mmc_switch_part(EMMC, USER_PARTITION);

	return 1;
}

/*
  key: the key will write must 32 len;
  len :must be 32;
  reutrn 0 success
*/
uint8_t mmc_rpmb_write_key(uint8_t * key, uint8_t len)
{
	struct rpmb_data_frame *data_frame = NULL;
	uint16_t msg_type;
	uint16_t op_result;
	struct mmc *mmc = find_mmc_device(0);
	if (NULL == mmc)
		return 1;

	if (key == NULL || RPMB_KEY_MAC_SIZE != len)
		return 1;

/*for write rpmb key req */
	msg_type = RPMB_MSG_TYPE_REQ_AUTH_KEY_PROGRAM;
	data_frame = (uint8_t *) malloc(RPMB_DATA_FRAME_SIZE);
	if (data_frame == NULL) {
		errorf("%s malloc error\n", __func__);
		return 1;
	}

	memset(data_frame, 0, RPMB_DATA_FRAME_SIZE);
	u16_to_bytes(msg_type, data_frame->msg_type);
	memcpy(data_frame->key_mac, key, RPMB_KEY_MAC_SIZE);
	mmc_switch_part(EMMC, RPMB_PARTITION);
	mmc_set_blockcount(mmc, RPMB_BLOCK_COUNT, 1);
	rpmb_write_cmd(mmc, RPMB_BLOCK_COUNT, BLOCK_SIZE, data_frame);

/*for read result req*/
	memset(data_frame, 0, RPMB_DATA_FRAME_SIZE);
	msg_type = RPMB_MSG_TYPE_REQ_RESULT_READ;
	u16_to_bytes(msg_type, data_frame->msg_type);
	mmc_set_blockcount(mmc, RPMB_BLOCK_COUNT, 0);
	rpmb_write_cmd(mmc, RPMB_BLOCK_COUNT, BLOCK_SIZE, data_frame);
	memset(data_frame, 0, RPMB_DATA_FRAME_SIZE);
	mmc_set_blockcount(mmc, RPMB_BLOCK_COUNT, 0);
	rpmb_read_cmd(mmc, data_frame, RPMB_BLOCK_COUNT);

/*result check*/
	bytes_to_u16(data_frame->op_result, &op_result);
	bytes_to_u16(data_frame->msg_type, &msg_type);
	if ((op_result == RPMB_RESULT_OK)
	    && (msg_type == RPMB_MSG_TYPE_RESP_AUTH_KEY_PROGRAM)) {
		dprintf(INFO," key write successed\n");
		free(data_frame);
		mmc_switch_part(EMMC, USER_PARTITION);
		return 0;
	} else {
		errorf(" key write fail op_result:0x%x ,msg_type:0x%x\n",
		       op_result, msg_type);
	}

	free(data_frame);
	mmc_switch_part(EMMC, USER_PARTITION);

	return 1;
};

uint8_t mmc_rpmb_get_wr_cnt(void)
{
	struct mmc *mmc = find_mmc_device(0);
  	if (NULL == mmc)
		return -1;

	dprintf(INFO,"rpmb_get_wr_cnt:0x%x\n", mmc->rel_wr_sec_c);

	return mmc->rel_wr_sec_c;
}

uint8_t mmc_rpmb_write_pac(uint8_t *pac_data, uint16_t blk_index)
{
	struct rpmb_data_frame *data_frame;
	uint16_t msg_type;
	uint16_t op_result;
	uint16_t block_count = 1;
	uint32_t writecount;
	struct mmc *mmc = find_mmc_device(0);	//0 is emmc
	if (NULL == mmc)
		return -1;

	if (pac_data == NULL)	//|| (RPMB_DATA_SIZE != strlen(write_data)))
		return -1;

	SPRD_BLOCK_LOCK(state);
	data_frame = (uint8_t *) malloc(RPMB_DATA_FRAME_SIZE);
	memset(data_frame, 0, RPMB_DATA_FRAME_SIZE);
	memcpy(data_frame, pac_data, RPMB_DATA_FRAME_SIZE);

	/*for write req */
	mmc_switch_part(EMMC, RPMB_PARTITION);
	mmc_set_blockcount(mmc, RPMB_BLOCK_COUNT, 1);
	rpmb_write_cmd(mmc, RPMB_BLOCK_COUNT, BLOCK_SIZE, data_frame);

	/*for read result req */
	memset(data_frame, 0, RPMB_DATA_FRAME_SIZE);
	msg_type = RPMB_MSG_TYPE_REQ_RESULT_READ;
	u16_to_bytes(msg_type, data_frame->msg_type);
	mmc_set_blockcount(mmc, RPMB_BLOCK_COUNT, 0);
	rpmb_write_cmd(mmc, RPMB_BLOCK_COUNT, BLOCK_SIZE, data_frame);
	memset(data_frame, 0, RPMB_DATA_FRAME_SIZE);
	mmc_set_blockcount(mmc, RPMB_BLOCK_COUNT, 0);
	rpmb_read_cmd(mmc, data_frame, RPMB_BLOCK_COUNT);

/*result check*/
	bytes_to_u16(data_frame->op_result, &op_result);
	bytes_to_u16(data_frame->msg_type, &msg_type);
	if ((op_result == RPMB_RESULT_OK)
	    && (msg_type == RPMB_MSG_TYPE_RESP_AUTH_DATA_WRITE)) {
		dprintf(INFO," data  write successed\n");
		free(data_frame);
		mmc_switch_part(EMMC, USER_PARTITION);
		SPRD_BLOCK_UNLOCK(state);
		return 0;
	} else
		errorf(" data write fail op_result:0x%x ,msg_type:0x%x\n",
		       op_result, msg_type);

	free(data_frame);
	mmc_switch_part(EMMC, USER_PARTITION);
	SPRD_BLOCK_UNLOCK(state);

	return -1;
}

