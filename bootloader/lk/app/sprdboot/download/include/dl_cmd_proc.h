/*
# Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
# Licensed under the Unisoc General Software License, version 1.0 (the License);
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
# Software distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OF ANY KIND, either express or implied.
# See the Unisoc General Software License, version 1.0 for more details.
*/

#ifndef DL_CMD_PROC_H
#define DL_CMD_PROC_H
#include "dl_packet.h"
#include <sprd_sizes.h>

#define PARTITION_SIZE_LENGTH          (4)
#define MAX_PARTITION_NAME_SIZE   (36)
#define GAP_SIZE_LENGTH   (8)
#define PARTITION_SIZE_LENGTH_V1  (8)
#define REPARTITION_HEADER_MAGIC 0x3A726170
/* CMD_START_DATA format length */
#define BIT64_IDLEN_DATA_LENGTH 	(0x58)		/* ID(72)+ LEN(8) +Rev(8) [+CS] */
#define BIT32_IDLEN_DATA_LENGTH 	(0x4C)		/* ID(72)+ LEN(4) [+CS] */
#define BIT64_ADDRLEN_DATA_LENGTH 	(0x10)		/* ADDR(8)+ LEN(8) [+CS] */
#define BIT32_ADDRLEN_DATA_LENGTH 	(0x08)		/* ADDR(4)+ LEN(4) [+CS] */
#define BIT64_READ_MIDST_LENGTH 0x0C

#define REPARTITION_UNIT_LENGTH    (MAX_PARTITION_NAME_SIZE *2 + PARTITION_SIZE_LENGTH)
#define REPARTITION_UNIT_LENGTH_V1    (MAX_PARTITION_NAME_SIZE *2 +  PARTITION_SIZE_LENGTH_V1 + GAP_SIZE_LENGTH)
#define REF_INFO_OFF 0XFA000

/* For distinguish Windows/Linux PC Download tools in cmd 0x28 */
#define MUSB_DLOAD_FOR_LINUX_FLAG		0x08
#define MUSB_DLOAD_FOR_LINUX			1
#define MUSB_DLOAD_FOR_WIN			0
void usb_set_dload_sys(uint8_t dload_sys_flag);
uint8_t usb_get_dload_sys(void);

#if (defined (CONFIG_DL_PCTOOL_AUTHORIZE) || defined(CONFIG_BLOCK_ENTER_MODE) || defined(CONFIG_FASTBOOT_AUTHORIZE))
typedef enum DL_AUTH_STATUS {
	DL_AUTH_INIT = 0,
	DL_AUTH_REQ,
	DL_SECURE_VERIFY_PASS,
} DL_AUTH_STATUS;

typedef struct {
	unsigned char M1[256];
	unsigned char ProjectName[32];
	unsigned short mStatus; //0：需要鉴权；1：不需要鉴权
} FDL_OT_Auth_ack_t;
#endif

#ifdef CONFIG_VERIFY_GPT
typedef struct {
	char header[SZ_H];
	char entry[SZ_E];
	char key[SZ_K];
} gpt_cmd_data;
#endif

typedef enum OPERATE_STATUS {
	OPERATE_SUCCESS = 1,
	OPERATE_SYSTEM_ERROR,
	OPERATE_DEVICE_INIT_ERROR,
	OPERATE_INVALID_DEVICE_SIZE,
	OPERATE_INCOMPATIBLE_PART,
	OPERATE_INVALID_ADDR,
	OPERATE_INVALID_SIZE,
	OPERATE_WRITE_ERROR,
	OPERATE_CHECKSUM_DIFF,
	OPERATE_IGNORE,
	OPERATE_MAGIC_ERROR,
	OPERATE_REPARTITION_ERROR,
	OPERATE_READ_ERROR,
	OPERATE_MALLOC_ERROR,
	OPERATE_VERIFY_ERROR
} OPERATE_STATUS;

typedef struct _REPARTITION_TABLE_INFO
{
	unsigned char version;
	unsigned char unit;
	unsigned char table_count;
	unsigned char reserved;
	unsigned int     table_tag;
	unsigned short     table_offset;
	unsigned short     table_size;
} REPARTITION_TABLE_INFO;

typedef struct tool_partition {
	char name[72];
	u32 size;//unit: KB
} tool_partition_t;

int dl_cmd_write_start(dl_packet_t * packet, void *arg);
int dl_cmd_write_midst(dl_packet_t * packet, void *arg);
int dl_cmd_write_end(dl_packet_t * packet, void *arg);
int dl_cmd_read_start(dl_packet_t * packet, void *arg);
int dl_cmd_read_midst(dl_packet_t * packet, void *arg);
int dl_cmd_read_end(dl_packet_t * packet, void *arg);
int dl_cmd_erase(dl_packet_t * packet, void *arg);
int dl_cmd_repartition(dl_packet_t * pakcet, void *arg);

#ifdef CONFIG_VERIFY_GPT
int32_t dl_cmd_get_gpt_key(dl_packet_t *packet);
#endif

int dl_cmd_reboot (dl_packet_t *pakcet, void *arg);
int dl_powerdown_device(dl_packet_t *packet, void *arg);
int dl_cmd_read_mcptype(dl_packet_t * packet, void *arg);
int dl_cmd_check_rootflag(dl_packet_t * packet, void *arg);
int dl_cmd_get_uid(dl_packet_t *packet, void *arg);
int dl_cmd_get_chip_uid(dl_packet_t *packet, void *arg);
int dl_cmd_get_uid_x86(dl_packet_t *packet, void *arg);
int dl_cmd_end_process(dl_packet_t *packet, void *arg);
int dl_cmd_read_ref_info(dl_packet_t *packet, void *arg);
int dl_cmd_disable_hdlc(dl_packet_t *packet, void *arg);
int dl_cmd_write_datetime(dl_packet_t *packet, void *arg);
int dl_cmd_set_debuginfo(dl_packet_t *packet, void *arg);
#ifdef CONFIG_EMMC_DDR_CHECK_TYPE
int dl_cmd_read_flashtype(dl_packet_t *pakcet, void *arg);
#endif
#ifdef CONFIG_PCTOOL_CHECK_MULTI_FIXNV
int dl_cmd_check_NV_type(dl_packet_t *pakcet, void *arg);
#endif
#ifdef CONFIG_PCTOOL_CHECK_WRITE_PROTECT
void dl_enable_write_flash(void);
#endif
//#ifdef CONFIG_DTS_MEM_LAYOUT
int dl_set_buf_base_size(void);
//#endif

#ifdef CONFIG_DOWNLOAD_RECORD
int dl_pre_record_flag(void);
typedef struct record_partition{
	uint32_t index;
	char partition_name[36];
} record_partition_t;
#endif

#if !defined(CONFIG_NAND_BOOT) && !defined(CONFIG_DDR_BOOT)
int dl_cmd_check_partition(dl_packet_t *packet, void *arg);
#endif
int dl_set_first_mode(dl_packet_t *packet, void *arg);
int dl_cmd_write_raw_data_enable(dl_packet_t *packet, void *arg);
int dl_cmd_whole_raw_file_start(dl_packet_t *packet, void *arg);
int dl_cmd_per_raw_packet_start(dl_packet_t *packet, void *arg);
int dl_cmd_write_flush_raw_data(dl_packet_t *packet, void *arg);
int dl_set_selinux(char* selinux_buf);
int dl_cmd_disable_selinux(dl_packet_t *packet, void *arg);
int dl_cmd_enable_iq_mem(dl_packet_t *packet, void *arg);
int dl_cmd_get_timestamp(u64 *timestamp);
int dl_cmd_set_timestamp(dl_packet_t *packet, void *arg);
int dl_cmd_check_handshake_data(dl_packet_t *packet, void *arg);
#ifdef CONFIG_SPRD_LOG
int dl_cmd_set_log2pc_start(dl_packet_t *packet, void *arg);
int dl_cmd_dump_log(dl_packet_t *packet, void *arg);
#endif
int dl_cmd_read_vcur(dl_packet_t *packet, void *arg);
int dl_cmd_write_vcur(dl_packet_t *packet, void *arg);
int dl_cmd_start_wdg(dl_packet_t *packet, void *arg);
int dl_cmd_stop_wdg(dl_packet_t *packet, void *arg);
#if defined(CONFIG_DL_PCTOOL_AUTHORIZE) || defined(CONFIG_BLOCK_ENTER_MODE)
int dl_need_to_authorize(void);
int dl_cmd_auth_request(dl_packet_t *packet, void *arg);
int dl_cmd_secure_verify(dl_packet_t *packet, void *arg);
#endif
#ifdef CONFIG_SUPPORT_PCTOOL_WRITE_EFUSE
int dl_cmd_write_efuse(dl_packet_t *packet, void *arg);
#endif
void dl_get_downloadflag(char *buf);
void dl_clear_downloadflag(void);

#endif /*DL_CMD_PROC_H */
