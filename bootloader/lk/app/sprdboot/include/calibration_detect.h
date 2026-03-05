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
#ifndef __CALI_DETECT_H__
#define __CALI_DETECT_H__
#include <config.h>
#include <linux/types.h>
#include <android_bootimg.h>
#include <boot_mode.h>
#include "boot_parse.h"
#include <string.h>
#include <lk/board.h>
#ifdef ZCFG_DISABLE_DOWNLOAD
#define CALIBERATE_COMMAND_AUTODOWNLOAD 0x03
#else
#define CALIBERATE_COMMAND_AUTODOWNLOAD 0x02
#endif
#define CALIBERATE_COMMAND_AUTOTEST  0x15
#define CALIBERATE_COMMAND_PROGRAMKEY 0x17
#define ADC_MAGIC      (0x4144434D)	//ADCM, header flag of adc data

extern int dwc_otg_driver_init(void);
extern void dwc_otg_driver_cleanup(void);
extern int usb_fastboot_initialize(void);
extern int usb_serial_init(void);
extern void  usb_serial_cleanup(void);
extern int usb_serial_configed;
extern int usb_port_open;
extern int usb_read_done;
extern int usb_write_done;
extern int usb_trans_status;
extern int usb_gadget_handle_interrupts(void);

extern int gs_open(void);
extern int gs_close(void);
extern int gs_read(const unsigned char *buf, int *count);
extern int gs_write(const unsigned char *buf, int count);
extern void usb_wait_trans_done(int direct);
extern int usb_is_trans_done(int direct);
extern int usb_is_configured(void);
extern int usb_is_port_open(void);
extern void udc_power_on(void);
extern void udc_power_off(void);
extern void usb_in_cal(int flag);
int pctool_mode_detect(void);

#if 0
#define IO_DEBUG 1     // open write and read debug
#define WRITE_DEBUG 1  // 0: for read debug, 1 for write debug
#else
#define IO_DEBUG 0
#define WRITE_DEBUG 0
#endif


#define CALIBERATE_STRING_LEN 10
#define CALIBERATE_STRING_LEN_14 14
#define CALIBERATE_STRING_LEN_16 16
#define CALIBERATE_HEAD 0x7e
#define CALIBERATE_COMMOND_T 0xfe
#define CALIBERATE_PROKEY_COMMOND_T 0x62
#define CALIBERATE_CNF_LEN 0x02
#define CALIBERATE_CNF_SCS 0x0
#define CALIBERATE_CNF_FAIL 0x01
#define DIAG_AP_CMD_PROGRAM_KEY 0x09
#define DIAG_AP_CMD_EXIT_PROGRAM_KEY 0x0a


typedef struct msg_head_tag
{
    unsigned int  seq_num;      // Message sequence number, used for flow control
    unsigned short  len;          // The totoal size of the packet "sizeof(MSG_HEAD_T)
    // + packet size"
    unsigned char   type;         // Main command type
    unsigned char   subtype;      // Sub command type
}__attribute__((packed)) MSG_HEAD_T;

typedef struct {
    unsigned short status;   // ==0: success, != 0: fail
    unsigned short length;   // length of  result
}__attribute__((packed)) TOOLS_DIAG_AP_CNF_T;

typedef struct {
    unsigned short  cmd;        // DIAG_AP_CMD_E
    unsigned short  length;   // Length of structure
}__attribute__((packed)) TOOLS_DIAG_AP_CMD_T;

typedef struct {
    unsigned short  cmd;        // DIAG_AP_CMD_E
    unsigned short  length;   // Length of structure
    unsigned short  para;
}__attribute__((packed)) TOOLS_DIAG_AP_EXIT_CMD_T;

#if defined(CONFIG_PCTOOL_AUTHORIZE) || defined(CONFIG_DL_PCTOOL_AUTHORIZE)
typedef struct {
	uint16_t mReqType; // DIAG_AP_CMD_AUTH_BEGIN
	uint16_t mLength; //0
} __attribute__((packed)) OT_Auth_req_t;

typedef struct {
	uint16_t mRspType; // DIAG_AP_CMD_AUTH_BEGIN
	uint16_t mLength; //M1+project name len
	uint16_t mStatus;      // 0:success 1:fail 2:No authentication required
} __attribute__((packed)) OT_Auth_ack_t;

typedef struct {
	uint8_t nonce[32];      // random
	uint8_t S1[256];
} AUTH_SECURE_DATA_T;

#define DIAG_AP_CMD_AUTH_BEGIN	(0x0090)	/* auth request from PC to UBoot */
#define DIAG_AP_CMD_AUTH_END	(0x0091)	/* secure data from PC to UBoot */
#endif

#ifdef CONFIG_BLOCK_ENTER_MODE
typedef enum {
	BIT_STARTUP_BLOCK_CALI = 0,		/* if 1, could enter cali */
} startup_block_type_t;
#endif

#endif
