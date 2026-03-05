/*
 * * Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
 * * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * * you may not use this file except in compliance with the License.
 * * You may obtain a copy of the License at
 * * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * * Software distributed under the License is distributed on an "AS IS" BASIS,
 * * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * * See the Unisoc General Software License, version 1.0 for more details.
 * */

#ifndef _SPRD_RPMB_H
#define _SPRD_RPMB_H

#ifndef _RPMB_H

#define RPMB_DATA_FRAME_SIZE        512

#define RPMB_MSG_TYPE_REQ_SWP_CFG_BLK_READ          (0x0007)
#define RPMB_MSG_TYPE_REQ_SWP_CFG_BLK_WRITE         (0x0006)
#define RPMB_MSG_TYPE_REQ_RESULT_READ               (0x0005)
#define RPMB_MSG_TYPE_REQ_AUTH_DATA_READ            (0x0004)
#define RPMB_MSG_TYPE_REQ_AUTH_DATA_WRITE           (0x0003)
#define RPMB_MSG_TYPE_REQ_WRITE_COUNTER_VAL_READ    (0x0002)
#define RPMB_MSG_TYPE_REQ_AUTH_KEY_PROGRAM          (0x0001)

#define RPMB_MSG_TYPE_RESP_SWP_CFG_BLK_READ         (0x0700)
#define RPMB_MSG_TYPE_RESP_SWP_CFG_BLK_WRITE        (0x0600)
#define RPMB_MSG_TYPE_RESP_AUTH_DATA_READ           (0x0400)
#define RPMB_MSG_TYPE_RESP_AUTH_DATA_WRITE          (0x0300)
#define RPMB_MSG_TYPE_RESP_WRITE_COUNTER_VAL_READ   (0x0200)
#define RPMB_MSG_TYPE_RESP_AUTH_KEY_PROGRAM         (0x0100)

#define RPMB_KEY_MAC_SIZE                           (32)
#define RPMB_STUFF_DATA_SIZE                        (196)
#define RPMB_DATA_SIZE                              256
#define RPMB_ENTRY_SIZE                             16
#define RPMB_NONCE_SIZE                             16
#define RPMB_RESULT_OK                              0x00
#define RPMB_RES_NO_AUTH_KEY                        0x0007

#define MAX_ENTRY_COUNT     4
#define RPMB_BLOCK_COUNT    1

struct sec_wp_entry
{
	uint8_t wpt_wpf;
	uint8_t reserved[3];
	uint8_t blk_address[8];
	uint8_t blk_num[4];
};

struct sec_wp_cfg_blk
{
	uint8_t lun;
	uint8_t data_length;
	uint8_t reserved[14];
	struct sec_wp_entry entry[4];
};

struct rpmb_data_frame {
	uint8_t	stuff_bytes[RPMB_STUFF_DATA_SIZE];
	uint8_t	key_mac[RPMB_KEY_MAC_SIZE];
	uint8_t	data[RPMB_DATA_SIZE];
	uint8_t	nonce[RPMB_NONCE_SIZE];
	uint8_t	write_counter[4];
	uint8_t	address[2];
	uint8_t	block_count[2];
	uint8_t	op_result[2];
	uint8_t	msg_type[2];
};

#endif

/*
 *blk_data: for save read data;
 *blk_index: the block index for read;
 *block_count: the read count;
 *success return 0 ;
 */
extern struct lu_info_tbl rpmb_lu_info;
uint8_t ufs_rpmb_blk_write(char *write_data, uint8_t *key, uint16_t blk_index);
uint8_t ufs_rpmb_blk_read(char *blk_data, uint16_t blk_index,
			  uint8_t block_count);
int check_ufs_rpmb_key(uint8_t *package, int package_size);
/* @retrun 0 rpmb key unwritten */
int is_wr_ufs_rpmb_key(void);
int prepare_rpmb_lu(void);
uint8_t ufs_rpmb_write_key(uint8_t * key, uint8_t len);
uint32_t ufs_rpmb_read_writecount(void);
int ufs_rpmb_write_pac(uint8_t *pac_data, uint16_t blk_index);
/*
 * swp_config_data: The secure Write Protect Configuration Block;
 * success return 0;
 */
int ufs_rpmb_swp_config_read(char *swp_config_data);


int storage_write_protect_set(const char *ptn_name, uint64_t offset, uint64_t bytes);
int storage_write_protect_remove(const char *ptn_name, uint64_t offset, uint64_t bytes);
int storage_write_protect_check(const char *ptn_name, uint64_t offset, uint64_t bytes);

#endif // _SPRD_RPMB_H
