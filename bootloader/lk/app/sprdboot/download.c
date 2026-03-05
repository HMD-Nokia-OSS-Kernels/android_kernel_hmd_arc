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
//ZOVERLAY_TAG_HMD_ONEIMAGE

#include <dl_packet.h>
#include <dl_cmd_def.h>
#include <dl_cmd_proc.h>
#include <lk/debug.h>
#include <boot_mode.h>
#include <sprd_usb_drv.h>
#include <string.h>
#include <app.h>
#include <dl_common.h>
#include <malloc.h>
#include <sprd_wdt.h>
#include <lk/board.h>
#include <miscdata_def.h>
#include <sprd_common_rw.h>
#include "asm/arch/check_reboot.h"

typedef enum {
	E_DISABLE_TRAN_CODE = 0,
	E_IS_OLD_MEMORY,
	E_SUPPORT_RAW_DATA,
	E_FLUSH_SIZE,
	E_RSA, //0: not support; 1:AES CBC  2: RSA2048
	E_TIME_STAMP,
#ifdef SPRD_SEND_STORAGE_TYPE
	E_STORAGE_TYPE,
#endif
	E_SUPPORT_DUMP_UBOOT_LOG = 7,
	E_MAX_ID = 0xFFFF
} E_DA_INFO_ID;

static struct dl_cmd *cmdlist = NULL;
extern int sprd_clean_rtc(void);
extern void usb_init (uint32_t autodl_mode);
extern void prand_gen(u8 *rand, size_t rand_len);
extern int aes_cbc_sw_encrypt(const uint8_t *raw_data, uint8_t *encrypt_data);
extern int rsa_encrypt_data_download(unsigned char *random_raw_data, unsigned char *send_data);
extern boot_device_t get_bootdevice(void);
#if defined CONFIG_ANDROID_AB
extern void do_select_ab(void);
#endif

uint8_t raw_random_data[32] = {0};

#if defined(CONFIG_DL_PCTOOL_AUTHORIZE) || defined(CONFIG_BLOCK_ENTER_MODE)
int dl_auth_sts = DL_AUTH_INIT;
#endif

typedef struct _DA_INFO_T_
{
	uint32_t   dwVersion;
	uint32_t   bDisableHDLC;	//0: Enable hdl; 1:Disable hdl
	uint8_t    bIsOldMemory;
	uint8_t    bSupportRawData;
	uint8_t    bReserve[2];
	uint32_t   dwFlushSize;		//unit KB
#ifdef SPRD_SEND_STORAGE_TYPE
	uint32_t   dwStorageType;
	uint32_t   dwReserve[59];	//Reserve
#else
	uint32_t   dwReserve[60];       //Reserve
#endif
}DA_INFO_T;

DA_INFO_T Da_Info = {0};

#ifdef SPRD_SEND_STORAGE_TYPE
typedef enum storage_type {
	STORAGE_TYPE_NAND = 0x101,
	STORAGE_TYPE_EMMC = 0x102,
	STORAGE_TYPE_UFS  = 0x103
}storage_type_t;
#endif

void dl_cmd_register(enum dl_cmd_type type,
		       int (*handle)(struct dl_packet *pkt, void *arg))
{
	struct dl_cmd *cmd = (void *)0;

	cmd = (struct dl_cmd *)malloc(sizeof(struct dl_cmd));
	//dprintf(INFO,"%s:malloc %d to %p mem\n", __FUNCTION__ , sizeof(struct dl_cmd), cmd);
	if (cmd) {
		cmd->type = type;
		cmd->handle = handle;
		cmd->next = cmdlist;
		cmdlist = cmd;
	} else {
		errorf("dl_cmd_register malloc failed!\n");
	}
	return;
}

void dl_cmd_handler(void)
{
	struct dl_cmd *cmd;
	struct dl_packet *pkt;
	dprintf(INFO,"%s:enter\n", __FUNCTION__);
	for (;;) {
		pkt = dl_get_packet();
		pkt->body.type = (pkt->body.type >> 8 | pkt->body.type << 8);
		pkt->body.size = (pkt->body.size >> 8 | pkt->body.size << 8);
		for (cmd = cmdlist; cmd; cmd = cmd->next) {
			if(cmd->type != pkt->body.type)
				continue;
#if defined(CONFIG_DL_PCTOOL_AUTHORIZE) || defined(CONFIG_BLOCK_ENTER_MODE)
			if (dl_need_to_authorize()
					&& (dl_auth_sts != DL_SECURE_VERIFY_PASS)
					&& ((pkt->body.type != BSL_CMD_AUTH_BEGIN)
							&& (pkt->body.type != BSL_CMD_AUTH_END)
							&& (pkt->body.type != BSL_CMD_DIS_HDLC)))
				continue;
#endif
			cmd->handle(pkt,NULL);
			dl_free_packet(pkt);

		/*
		 * in for(;;) loop, there should be no other log except handle func,
		 * otherwise common_raw_write() will be called all the time and will
		 * slow the download speed
		 */
		//	write_log();
			break;
		}
		/* cannot found the cmd in cmdlist */
		if (NULL == cmd) {
			dprintf(ALWAYS, "cmd 0x%x not support!\n", pkt->body.type);
			dl_free_packet(pkt);
			dl_send_ack(BSL_UNSUPPORTED_CMD);
		}
	}
}

void dl_handshake_check_handler(void)
{
	struct dl_packet *pkt;

    dprintf(INFO,"%s:enter\n", __FUNCTION__);

	pkt = dl_get_packet();
	pkt->body.type = (pkt->body.type >> 8 | pkt->body.type << 8);
	pkt->body.size = (pkt->body.size >> 8 | pkt->body.size << 8);

	if (pkt->body.type == BSL_CMD_HANDSHAKE_CHECK)
	{
		dl_cmd_check_handshake_data(pkt, NULL);
		dl_free_packet(pkt);
	} else {
		errorf("[%s]: send DATA_ERROR ack\n", __func__);
		dl_send_ack(BSL_CHECK_DATA_ERROR);
	}
}

#ifdef CONFIG_DL_POWER_CONTROL
extern void dl_power_control(void);
#endif

static int dl_da_tlv(uint8_t *content, uint16_t type, uint16_t len,
	uint8_t *value)
{
	uint16_t *pt;
	uint16_t *pl;
	uint8_t *pv;

	pt = (uint16_t *)content;
	pl = pt + 1;
	pv = (uint8_t *)(pl + 1);

	*pt = type;
	*pl = len;
	if (len == 1)
		*pv = *value;
	else
		memcpy(pv, value, len);

	return sizeof(*pt) + sizeof(*pl) + len;
}

// #ifdef CONFIG_HMD_ONE_IMAGE
#define CHECK_STATUS_CONDITION (check_status)
#define DOWNLOAD_AUTH_DISABLED 0
#define DOWNLOAD_AUTH_ENABLED  1

extern int zyt_test_flag;
// static int check_status=DOWNLOAD_AUTH_DISABLED;

int check_download_status(void)
{
	char flag[1] = {0};
	zyt_test_flag=1;

	if (common_raw_read("miscdata",(uint64_t)MISCDATA_HMD_FASTBOOT_SALE_DOWNLOAD_MODE_DATA_LEN,(uint64_t)MISCDATA_HMD_FASTBOOT_SALE_DOWNLOAD_MODE, flag)) {
			printf("read MISCDATA_HMD_FASTBOOT_SALE_DOWNLOAD_MODE flag error!\n");
	}
	if(flag[0]==1)
	{
		printf("%s:MISCDATA_HMD_FASTBOOT_SALE_DOWNLOAD_MODE status=0\n", __FUNCTION__);
		return DOWNLOAD_AUTH_DISABLED;
	}
#if 0
	return DOWNLOAD_AUTH_ENABLED;
#else
	return DOWNLOAD_AUTH_DISABLED;
#endif
}
// #else
// #define CHECK_STATUS_CONDITION (1)
// #endif

static void dl_pre_init(void)
{
#ifdef CONFIG_USB_ENUM_IN_UBOOT
	int chip_version = 0;
#endif

// #ifdef CONFIG_HMD_ONE_IMAGE
	//  check_status=check_download_status();
// #endif

#ifdef CONFIG_ANDROID_AB
	do_select_ab();
#endif

#ifdef CONFIG_USB_ENUM_IN_UBOOT
	chip_version = sprd_get_chip_version();
	if ((dl_times==0) && (boot_role == BOOTLOADER_MODE_DOWNLOAD) && (chip_version < USB_FIX_VERSION))
	{
		dl_times=dl_times+1;
		autodloader_mainhandler();
	}
#endif
	dl_packet_init();

#ifdef CONFIG_NAND_BOOT //0
	fdl_ubi_dev_init();
#endif
	sprd_clean_rtc();

#ifdef CONFIG_DL_POWER_CONTROL //0
	dl_power_control();
#endif

//#ifdef CONFIG_DTS_MEM_LAYOUT
	dl_set_buf_base_size();
//#endif
}

static void dl_register_func(void)
{
	/* register all cmd process functions */
	dl_cmd_register(BSL_CMD_START_DATA, dl_cmd_write_start);
	dl_cmd_register(BSL_CMD_MIDST_DATA, dl_cmd_write_midst);
	dl_cmd_register(BSL_CMD_END_DATA, dl_cmd_write_end);
	dl_cmd_register(BSL_CMD_READ_FLASH_START, dl_cmd_read_start);
	dl_cmd_register(BSL_CMD_READ_FLASH_MIDST, dl_cmd_read_midst);
	dl_cmd_register(BSL_CMD_READ_FLASH_END, dl_cmd_read_end);
	dl_cmd_register(BSL_ERASE_FLASH, dl_cmd_erase);
	dl_cmd_register(BSL_REPARTITION, dl_cmd_repartition);
#ifdef CONFIG_VERIFY_GPT
	dl_cmd_register(BSL_CMD_SIGNATURE, dl_cmd_get_gpt_key);
#endif
	dl_cmd_register(BSL_CMD_NORMAL_RESET, dl_cmd_reboot);
	dl_cmd_register(BSL_CMD_POWER_DOWN_TYPE, dl_powerdown_device);
	//dl_cmd_register(BSL_CMD_READ_CHIP_TYPE, dl_cmd_mcu_read_chiptype);
	dl_cmd_register(BSL_CMD_READ_MCP_TYPE, dl_cmd_read_mcptype);
	dl_cmd_register(BSL_CMD_CHECK_ROOTFLAG, dl_cmd_check_rootflag);
#ifdef CONFIG_SPRD_UID
	dl_cmd_register(BSL_CMD_READ_UID, dl_cmd_get_chip_uid);
#else
#ifdef CONFIG_X86
	dl_cmd_register(BSL_CMD_READ_UID, dl_cmd_get_uid_x86);
#else
	dl_cmd_register(BSL_CMD_READ_UID, dl_cmd_get_uid);
#endif
#endif
	dl_cmd_register(BSL_CMD_END_PROCESS, dl_cmd_end_process);
	dl_cmd_register(BSL_CMD_READ_REFINFO, dl_cmd_read_ref_info);
	dl_cmd_register(BSL_CMD_DIS_HDLC, dl_cmd_disable_hdlc);
	dl_cmd_register(BSL_CMD_WRITE_DATETIME, dl_cmd_write_datetime);
	dl_cmd_register(BSL_CMD_SET_DEBUGINFO, dl_cmd_set_debuginfo);
	dl_cmd_register(BSL_CMD_HANDSHAKE_CHECK, dl_cmd_check_handshake_data);
#ifdef CONFIG_PCTOOL_CHECK_MULTI_FIXNV
	dl_cmd_register(BSL_CMD_CHECK_NV_TYPE, dl_cmd_check_NV_type);
#endif
#ifdef CONFIG_PCTOOL_CHECK_WRITE_PROTECT
	dl_cmd_register(BSL_CMD_ENABLE_WRITE_FLASH, dl_enable_write_flash);
#endif
#ifdef CONFIG_EMMC_DDR_CHECK_TYPE
	dl_cmd_register(BSL_CMD_READ_FLASH_TYPE, dl_cmd_read_flashtype);
#endif
#if !defined(CONFIG_NAND_BOOT) && !defined(CONFIG_DDR_BOOT)
	dl_cmd_register(BSL_CMD_CHECK_PARTITION, dl_cmd_check_partition);
#endif
	dl_cmd_register(BSL_CMD_SET_FIRST_MODE, dl_set_first_mode);
	dl_cmd_register(BSL_CMD_WRITE_RAW_DATA_ENABLE, dl_cmd_write_raw_data_enable);
	dl_cmd_register(BSL_CMD_DLOAD_RAW_START, dl_cmd_per_raw_packet_start);
	dl_cmd_register(BSL_CMD_DLOAD_RAW_START2, dl_cmd_whole_raw_file_start);
	dl_cmd_register(BSL_CMD_WRITE_FLUSH_DATA, dl_cmd_write_flush_raw_data);
#if DEBUG
	dl_cmd_register(BSL_CMD_DISABLE_SELINUX, dl_cmd_disable_selinux);
	dl_cmd_register(BSL_CMD_IQ_DYN_EN, dl_cmd_enable_iq_mem);
#endif
	dl_cmd_register(BSL_CMD_SET_TIME_STAMP, dl_cmd_set_timestamp);
#ifdef CONFIG_SPRD_LOG
	dl_cmd_register(BSL_CMD_ENABLE_DL_LOG2PC, dl_cmd_set_log2pc_start);
	dl_cmd_register(BSL_CMD_DUMP_UBOOT_LOG, dl_cmd_dump_log);
#endif
	dl_cmd_register(BSL_CMD_READ_VCUR_DATA, dl_cmd_read_vcur);
	dl_cmd_register(BSL_CMD_WRITE_VPAC_DATA, dl_cmd_write_vcur);
	dl_cmd_register(BSL_CMD_START_WDG, dl_cmd_start_wdg);
	dl_cmd_register(BSL_CMD_STOP_WDG, dl_cmd_stop_wdg);
#if defined(CONFIG_DL_PCTOOL_AUTHORIZE) || defined(CONFIG_BLOCK_ENTER_MODE)
	dl_cmd_register(BSL_CMD_AUTH_BEGIN, dl_cmd_auth_request);
	dl_cmd_register(BSL_CMD_AUTH_END, dl_cmd_secure_verify);
#endif
#ifdef CONFIG_SUPPORT_PCTOOL_WRITE_EFUSE
	dl_cmd_register(BSL_CMD_WRITE_EFUSE, dl_cmd_write_efuse);
#endif
}

static void dl_ackpkt_init(void)
{
	dl_packet_t *ack_packet = NULL;
	int offset = 0;
	uint64_t timestamp;
	uint32_t dump_log_support = 1;
#ifdef CONFIG_HANDSHAKE_DATA_CHECK
	uint32_t encrypt_type = 0;
	unsigned char encrypt_data[256] = {0};
	unsigned char type_encrypt_data[260] = {0};
#endif
	ack_packet = malloc(sizeof(dl_packet_t));
	if (NULL == ack_packet) {
		errorf("no enough heap for ack_packet\n");
		return;
	}
	memset(ack_packet, 0, sizeof(dl_packet_t));
	/* uart download doesn't supoort disable hdlc, so need check it */
	Da_Info.bDisableHDLC = 0;
	Da_Info.dwVersion = 2;
	Da_Info.bIsOldMemory = 0;
#ifndef CONFIG_UART_DOWNLOAD
	usb_init(0);
	if (FDL_get_SupportRawDataProc())
		Da_Info.dwVersion = 4;
	Da_Info.bDisableHDLC = 1;
#endif

#ifndef CONFIG_NAND_BOOT
	Da_Info.bIsOldMemory = flash_2ndhand_detect();
#endif

#ifndef CONFIG_UART_DOWNLOAD
	if (FDL_get_SupportRawDataProc()) {
		/* support raw data transfer */
		Da_Info.bSupportRawData = 2;
		Da_Info.dwFlushSize = HALF_PKT_SIZE/1024;//KB
	}
#endif

#ifdef SPRD_SEND_STORAGE_TYPE
#if defined(CONFIG_BLK_DEV_BOOT)
	if (get_bootdevice() == BOOT_DEVICE_EMMC)
		Da_Info.dwStorageType = STORAGE_TYPE_EMMC;
	else if (get_bootdevice() == BOOT_DEVICE_UFS)
		Da_Info.dwStorageType = STORAGE_TYPE_UFS;
#elif defined(CONFIG_EMMC_BOOT)
	Da_Info.dwStorageType = STORAGE_TYPE_EMMC;
#elif defined(CONFIG_NAND_BOOT)
	Da_Info.dwStorageType = STORAGE_TYPE_NAND;
#else
	#error Macros(CONFIG_BLK_DEV_BOOT\CONFIG_EMMC_BOOT\CONFIG_NAND_BOOT) \
		need to be defined in this project
#endif
#endif

	ack_packet->body.type = BSL_INCOMPATIBLE_PARTITION;
#if !defined(DA_DWVERSION) || (DA_DWVERSION > 4)
	/* specify magic for new da_info */
	{
		uint32_t *ptr = (uint32_t *)&(ack_packet->body.content[0]);
		*ptr = 0x7477656e;
	}
	offset += 4;
	offset += dl_da_tlv(ack_packet->body.content + offset,
				(uint16_t)E_DISABLE_TRAN_CODE, 4, (uint8_t *)&Da_Info.bDisableHDLC);
	offset += dl_da_tlv(ack_packet->body.content + offset,
		(uint16_t)E_IS_OLD_MEMORY, 1, &Da_Info.bIsOldMemory);
	offset += dl_da_tlv(ack_packet->body.content + offset,
				(uint16_t)E_SUPPORT_RAW_DATA, 1,
				&Da_Info.bSupportRawData);
	offset += dl_da_tlv(ack_packet->body.content + offset,
				(uint16_t)E_FLUSH_SIZE, 4, (uint8_t *)&Da_Info.dwFlushSize);
	if (dl_cmd_get_timestamp(&timestamp))
		timestamp = (uint64_t)-1;
	offset += dl_da_tlv(ack_packet->body.content + offset,
				(uint16_t)E_TIME_STAMP, 8, (uint8_t *)&timestamp);
#ifdef SPRD_SEND_STORAGE_TYPE
	offset += dl_da_tlv(ack_packet->body.content + offset,
				(uint16_t)E_STORAGE_TYPE, 4, &Da_Info.dwStorageType);
#endif
	offset += dl_da_tlv(ack_packet->body.content + offset,
					(uint16_t)E_SUPPORT_DUMP_UBOOT_LOG, 4, (uint8_t *)&dump_log_support);
#ifdef CONFIG_HANDSHAKE_DATA_CHECK
	if (CHECK_STATUS_CONDITION) {
        encrypt_type = 2;
        prand_gen(raw_random_data, 32);

        if (encrypt_type == 1) {
            aes_cbc_sw_encrypt(raw_random_data, encrypt_data);
        } else if (encrypt_type == 2) {
            rsa_encrypt_data_download(raw_random_data, encrypt_data);
        }

	memcpy(type_encrypt_data, &encrypt_type, 4);
	memcpy(type_encrypt_data + 4, encrypt_data, 256);

	offset += dl_da_tlv(ack_packet->body.content + offset,
				(uint16_t)E_RSA, 260, type_encrypt_data);
	}
#endif
	ack_packet->body.size = offset;
#else
	memcpy((unchar *)ack_packet->body.content, (unchar *)&Da_Info,sizeof(Da_Info));
	ack_packet->body.size = sizeof(Da_Info);
#endif
	dl_send_packet(ack_packet);
	free(ack_packet);
}

int do_download(void)
{
	dprintf(INFO,"%s:enter\n", __func__);

	dl_pre_init();
	dprintf(INFO,"[%s] pre init is ok\n", __func__);

	dl_set_selinux("Selinux:1");
	dprintf(INFO,"[%s] set Selinux:1 as default\n", __func__);

	//add by hyinfeng for CMT-275,NYX-274 fdl verify repair or factory permission begin 
	#ifdef ZCFG_ENABLE_FDL_DOWNLOAD_AUTH
	extern int fdl_download_verify_data(void);
	if(fdl_download_verify_data()) {
		dprintf(INFO,"[%s] verify fdl download ok\n", __func__);
	} else {
		dprintf(INFO,"[%s] verify fdl download fail\n", __func__);
		power_down_devices(0);
		while(1);
		return 1;
	}
	#endif
	//add by hyinfeng for CMT-275,NYX-274 fdl verify repair or factory permission end

	dl_register_func();
	dprintf(INFO,"[%s] register cmd process function is ok\n", __func__);

	dl_ackpkt_init();

#ifdef CONFIG_HANDSHAKE_DATA_CHECK //0
    if (CHECK_STATUS_CONDITION) {
        dl_handshake_check_handler();
    }
#endif

	/* enter command handler */
	dl_cmd_handler();

	return 0;
}

