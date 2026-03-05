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
#include <chipram_env.h>
#include "calibration_detect.h"
#include "sprd_cpcmdline.h"
#include "boot_parse.h"
#include <linux/usb/ch9.h>
#include <linux/usb/usb_uboot.h>
#include <sprd_common_rw.h>
#include <miscdata_def.h>
#ifdef CONFIG_PCTOOL_AUTHORIZE
#include <../lib/crypto/inc/authentication.h>
extern void crypto_hexdump(const char *title, uint8_t * data, int len);
#endif
#include <malloc.h>
#include <sprd_log.h>
#include <serial_sprd.h>
#include <sprd_battery.h>
#include <secureboot/sec_common.h>

#define MAX_CALIBRATION_LEN   50
#define CMD_BUF_NUMBER 20
#define USB_MAX_PACKET_SIZE 64

static unsigned int nv_buffer[128]={0};
static int s_is_calibration_mode = 0;
static char calibration_cmd_buf[MAX_CALIBRATION_LEN] = {0};
uint8_t pctool_cmd_buf[CMD_BUF_NUMBER];
uint8_t pctool_cnf_buf[20];


#ifdef CONFIG_DOWNLOAD_FOR_CUSTOMER
int pctool_is_connect = 0;
int pctool_connected(void)
{
	return pctool_is_connect;
}
#endif

char *get_calibration_parameter(void)
{
	if(s_is_calibration_mode != 0)
		return calibration_cmd_buf;
	else
		return NULL;
}

bool is_calibration_by_uart(void)
{
    return (2 == s_is_calibration_mode);
}

#define CALIBERATE_STRING_LEN 10
#define CALIBERATE_HEAD 0x7e
#define CALIBERATE_COMMOND_T 0xfe
#define CALIBERATE_COMMAND_REQ  1

#define CALIBERATE_DEVICE_NULL  0
#define CALIBERATE_DEVICE_USB   1
#define CALIBERATE_DEVICE_UART  2

extern int get_mode_from_vchg(void);
typedef  struct tag_cali_command {
	unsigned int   	reserved;
	unsigned short  size;
	unsigned char   cmd;
	unsigned char   sub_cmd;
} COMMAND_T;

extern int serial_tstc(void);
static unsigned long start_time;
static unsigned long now_time;
static int caliberate_device = CALIBERATE_DEVICE_NULL;

static void send_caliberation_request(void)
{
        COMMAND_T cmd;
        unsigned int i;
        unsigned char *data = (unsigned char *)&cmd;

        cmd.reserved = 0;
        cmd.cmd = CALIBERATE_COMMOND_T;
        cmd.size = CALIBERATE_STRING_LEN-2;
        cmd.sub_cmd = CALIBERATE_COMMAND_REQ;

        sprd_serial_putc(CALIBERATE_HEAD);

        for (i = 0; i < sizeof(COMMAND_T); i++)
             sprd_serial_putc(data[i]);

        sprd_serial_putc(CALIBERATE_HEAD);
}

static int receive_caliberation_response(uint8_t *buf,int len)
{
        int count = 0;
        int ch;
        uint32_t is_not_empty = 0;
        uint32_t start_time = 0,current_time = 0;

        if ((buf == NULL) || (len == 0))
            return 0;

        is_not_empty = serial_tstc();
        if (is_not_empty) {
            start_time = get_timer_masked();
            do {
                do {
                    ch = sprd_serial_getc();
                    if (count < CALIBERATE_STRING_LEN)
                        buf[count++] = ch;
                } while (serial_tstc());

                if ((count >= CALIBERATE_STRING_LEN) || (count >= len)) {
                    caliberate_device = CALIBERATE_DEVICE_UART;
                    break;
                }

                current_time = get_timer_masked();
            } while((current_time - start_time) < 500);
        }

        return count;
}

unsigned int check_caliberate(uint8_t * buf, int len)
{
	unsigned int command = 0;
	unsigned int freq = 0;

	if (len != CALIBERATE_STRING_LEN)
		return 0;

	if ((*buf == CALIBERATE_HEAD) && (*(buf + len -1) == CALIBERATE_HEAD)) {
		if ((*(buf+7) == CALIBERATE_COMMOND_T) && (*(buf + len - 2) != 0x1)) {
			command = *(buf + len - 2);
			command &= 0x7f;

			freq = *(buf + 1);
			freq = freq << 8;
			freq += *(buf + 2);

			command += freq << 8;
		}
	}

	return command;
}

int pctool_mode_detect_uart(void)
{
#ifdef CONFIG_MODEM_CALIBERATE
	int i, ret;
	unsigned int caliberate_mode;
	uint8_t buf[20];
	int got = 0;
#endif
	debugf("%s\n", "uart calibrate detecting");
	send_caliberation_request();

#ifdef CONFIG_MODEM_CALIBERATE
	for(i = 0; i < 20; i++)
		buf[i] = i + 'a';

	start_time = get_timer_masked();
	while (1) {
		got = receive_caliberation_response(buf, sizeof(buf));
		if (caliberate_device == CALIBERATE_DEVICE_UART)
			break;

		now_time = get_timer_masked();
		if ((now_time - start_time) > CALIBRATE_ENUM_MS) {
			debugf("usb calibrate configuration timeout\n");
			return -1;
		}
	}

	debugf("caliberate : what got from host total %d is \n", got);
	for (i = 0; i < got; i++)
		debugf("0x%x ", buf[i]);
	debugf("\n");

	caliberate_mode = check_caliberate(buf, CALIBERATE_STRING_LEN);
	if (!caliberate_mode) {
		debugf("func: %s line: %d caliberate failed\n", __func__, __LINE__);
		return -1;
    } else {
		memset(calibration_cmd_buf, 0, MAX_CALIBRATION_LEN);
		s_is_calibration_mode = 2;

		switch (caliberate_mode & 0xff){
			case CALIBERATE_COMMAND_AUTOTEST:
				sprintf(calibration_cmd_buf, MAX_CALIBRATION_LEN, "autotest=1 ");
				ret = CMD_AUTOTEST_MODE;
				break;
			default:
				sprintf(calibration_cmd_buf, MAX_CALIBRATION_LEN, "calibration=%d,%d,130 ", caliberate_mode&0xff, (caliberate_mode&(~0xff))>>8);
				ret = CMD_CALIBRATION_MODE;
				break;
		}
		return ret;
	}
#endif
	/* nerver come to here */
	return -1;
}


unsigned long get_cal_enum_ms(void)
{
    return CALIBRATE_ENUM_MS;
}

unsigned long get_cal_io_ms(void)
{
    return CALIBRATE_IO_MS;
}


int check_pctool_cmd(uint8_t* buf, int len)
{
    int command = 0;
    unsigned int freq = 0;
    MSG_HEAD_T* msg_head_ptr;
    uint8_t* msg_ptr = buf + 1;
    TOOLS_DIAG_AP_EXIT_CMD_T* msg_exit_prokey;
    TOOLS_DIAG_AP_CMD_T* msg_enter_prokey;

	if((*buf == CALIBERATE_HEAD) && (*(buf + len -1) == CALIBERATE_HEAD)){
	    msg_head_ptr = (MSG_HEAD_T*)(buf + 1);
	    switch(msg_head_ptr->type){
	        case CALIBERATE_COMMOND_T:
				command = msg_head_ptr->subtype;
				command &= 0x3f;
	            freq = *(buf+1);
	            freq = freq<<8;
	            freq += *(buf+2);
	            freq += ((*(buf+3)) << 16); /* bit16~bit23, new fre may > 65535 */
	            command += freq<<8;
	            break;
	        case CALIBERATE_PROKEY_COMMOND_T:
	            if(DIAG_AP_CMD_PROGRAM_KEY == *(msg_ptr + sizeof(MSG_HEAD_T))){
	                msg_enter_prokey =  (TOOLS_DIAG_AP_CMD_T*)(msg_ptr + sizeof(MSG_HEAD_T));
	                command = msg_enter_prokey->cmd;
	            }
	            else if(DIAG_AP_CMD_EXIT_PROGRAM_KEY == *(msg_ptr + sizeof(MSG_HEAD_T))){
	                msg_exit_prokey = (TOOLS_DIAG_AP_EXIT_CMD_T*)(msg_ptr + sizeof(MSG_HEAD_T));
	                command = msg_exit_prokey->para;
	            }
			    break;
	        default:
	            command = 0;
	            break;
		}
	}
	debugf("checked command from pc , and return value = %x\n" , command);
	return command;
}

extern int power_button_pressed(void);
static unsigned long count_ms;

int is_timeout(void)
{

    now_time = get_timer_masked();

    if((now_time - start_time)>count_ms)
      	return 1;
    else{
        return 0;
    }
}

void cali_usb_debug(uint8_t *buf)
{
	int i, ret;
 	for(i = 0; i<20; i++)
	buf[i] = i+'a';
 	while(!usb_serial_configed)
 		usb_gadget_handle_interrupts();
	debugf("USB SERIAL CONFIGED\n");
    gs_open();
#if WRITE_DEBUG
	while(1){
		ret = gs_write(buf, 20);
		debugf("func: %s waitting write done\n", __func__);
		if(usb_trans_status)
			debugf("func: %s line %d usb trans with error %d\n", __func__, __LINE__, usb_trans_status);
		usb_wait_trans_done(1);
		debugf("func: %s readly send %d\n", __func__, ret);
	}
#else
	while(1){
		int count = 20;
		usb_wait_trans_done(0);
		if(usb_trans_status)
			debugf("func: %s line %d usb trans with error %d\n", __func__, __LINE__, usb_trans_status);
		ret = gs_read(buf, &count);
		debugf("func: %s readly read %d，return ret %d\n", __func__, count, ret);
		if(usb_trans_status)
			debugf("func: %s line %d usb trans with error %d\n", __func__, __LINE__, usb_trans_status);
		for(i = 0; i<count; i++)
			debugf("%c ", buf[i]);
		debugf("\n");
	}

#endif
}

int cali_usb_prepare(void)
{
	if(usb_driver_init(USB_SPEED_FULL) < 0)
	{
		debugf("%s\n", "usb_driver_init error");
		return 0;
	}

	if(usb_serial_init() < 0)
	{
		debugf("%s\n", "usb_serial_init error");
		return 0;
	}
	return 1;
}

int cali_usb_enum(void)
{
	int ret = 0;
	count_ms = get_cal_enum_ms();
	start_time = get_timer_masked();

	while(!usb_is_configured()){
		ret = is_timeout();
		if(ret == 0)
			continue;
		else{
			debugf("usb calibrate configuration timeout,%lu,%lu,%lu\n",now_time,start_time,count_ms);
			return 0;
		}
	}
	debugf("USB SERIAL CONFIGED\n");

	start_time = get_timer_masked();
	count_ms = get_cal_io_ms();
	while(!usb_is_port_open()){
		ret = is_timeout();
		if(ret == 0)
			continue;
		else{
			debugf("usb calibrate port open timeout%lu,%lu,%lu\n",now_time,start_time,count_ms);
			return 0;
			}
		}
	gs_open();
	debugf("USB SERIAL PORT OPENED\n");
	return 1;
}

int escape_char(uint8_t *buf,int got)
{
	int size = 2 * got;
	int i = 0;
	int t = 0;
	uint8_t* temp = (uint8_t*) malloc(size);
	if(!temp){
		debugf("escape malloc failed\n");
		return 0;
	}

	memset(temp, 0, size);

	for(; i < got; i++, t++){
		if (buf[i] == 0x7d){
			i++;
			temp[t] = buf[i] ^ 0x20;
		} else {
			temp[t] = buf[i];
		}
	}

	memcpy(buf, temp, size);

	free(temp);
	return 1;
}

int cali_get_cmd(uint8_t *pctool_cmd_buf, int receive_len)
{
	int got = 0;
	int count = receive_len;
	int ret = 0;
	int i;
	int max_len = CMD_BUF_NUMBER;
	if(max_len < receive_len){
		debugf("cmd_buf out of bounds\n");
		return 0;
	}

	start_time = get_timer_masked();

	while(1) {
		if(usb_is_trans_done(0))
		{
			if(usb_trans_status)
				debugf("func: %s line %d usb trans with error %d\n", __func__, __LINE__, usb_trans_status);
			gs_read(pctool_cmd_buf + got, &count);
			if(usb_trans_status)
				debugf("func: %s line %d usb trans with error %d\n", __func__, __LINE__, usb_trans_status);
			got+=count;
			if(got >= receive_len && pctool_cmd_buf[got-1] == 0x7e) {
			#ifdef CONFIG_DOWNLOAD_FOR_CUSTOMER
				pctool_is_connect = 1;
				dprintf(ALWAYS, "%s download tool connected successfully.\n", __func__);
			#endif
				break;
			}
		}
		if (got < receive_len) {
			ret = is_timeout();
			if(ret == 0){
				count = receive_len - got;
				continue;
			}else{
				debugf("usb read timeout:what got from host total %d is\n", got);
				for(i = 0; i < got; i++)
					debugf("0x%x ", pctool_cmd_buf[i]);
				debugf("\n");
				return 0;
			}
		} else {
			break;
		}
	}

 	debugf("caliberate:what got from host total %d is \n", got);
 	for(i=0; i<got;i++)
 		debugf("0x%x ", pctool_cmd_buf[i]);
 	debugf("\n");
	return got;
}

#ifdef CONFIG_PCTOOL_AUTHORIZE
/**
 * - get diag cmd data
 *    return if we found 0x7e, and receive length may not exceed 'receive_len'
 * Return:
 *    > 0, receive data length. 0 timeout
 */
static int cali_get_cmd_ex(uint8_t *buf, int receive_len)
{
	int count = receive_len > 512 ? 512 : receive_len;
	int got = 0;
	int i;

	start_time = get_timer_masked();

	while (got < receive_len) {
		if (usb_is_trans_done(0)) {
			if (usb_trans_status)
				errorf("func: %s line %d usb trans with error %d\n", __func__,
					__LINE__, usb_trans_status);
			gs_read(buf + got, &count);
			if (usb_trans_status)
				errorf("func: %s line %d usb trans with error %d\n", __func__,
					__LINE__, usb_trans_status);
			got += count;
			if (got >= receive_len || ((got > 1) && (buf[got - 1] == 0x7e)))
				break;
		}

		if (!is_timeout()) {
			count = receive_len - got;
			count = count > 512 ? 512 : count;
			continue;
		} else {
			debugf("usb read timeout:what got from host total %d is \n", got);
			for (i = 0; i < got; i++)
				debugf("%02x ", buf[i]);
			debugf("\n");
			return 0;
		}
	}

 	debugf("caliberate:what got from host total %d is \n", got);
 	for (i = 0; i < got; i++)
 		debugf("%02x ", buf[i]);
 	debugf("\n");
	return got;
}
#endif
int reply_to_pctool(uint8_t* buf, int len)
{
	int data_len = len;
	uint8_t *addr = buf;

	while(data_len) {
		if(data_len > USB_MAX_PACKET_SIZE) {
			data_len -= USB_MAX_PACKET_SIZE;
			gs_write(addr, USB_MAX_PACKET_SIZE);
			addr += USB_MAX_PACKET_SIZE;
		} else {
			gs_write(addr, data_len);
			data_len = 0;
		}
		if(usb_trans_status)
			debugf("%s line %d usb trans with error %d\n", __func__, __LINE__, usb_trans_status);
		usb_wait_trans_done(1);
	}

	return 1;
}

int translate_packet(char *dest,char *src,int size)
{
    int i;
    int translated_size = 0;

    dest[translated_size++] = 0x7E;

    for(i=0;i<size;i++){
        if(src[i] == 0x7E){
            dest[translated_size++] = 0x7D;
            dest[translated_size++] = 0x5E;
        } else if(src[i] == 0x7D) {
            dest[translated_size++] = 0x7D;
            dest[translated_size++] = 0x5D;
        } else
            dest[translated_size++] = src[i];
    }
    dest[translated_size++] = 0x7E;
    return translated_size;
}

int prepare_reply_buf(uint8_t* buf, int status)
{
    char *rsp_ptr;
    MSG_HEAD_T* msg_head_ptr;
    TOOLS_DIAG_AP_CNF_T* aprsp;
	int total_len,rsplen;
	debugf("preparing reply buf");
    if(NULL == buf){
	    debugf("in function prepare_reply_buf, buf = NULL\n");
        return 0;
    }

    msg_head_ptr = (MSG_HEAD_T*)(buf + 1);
    rsplen = sizeof(TOOLS_DIAG_AP_CNF_T) + sizeof(MSG_HEAD_T);
    rsp_ptr = (char*)malloc(rsplen);
    if(NULL == rsp_ptr){
	    debugf("in function prepare_reply_buf: Buffer malloc failed\n");
	    return 0;
    }
    aprsp = (TOOLS_DIAG_AP_CNF_T*)(rsp_ptr + sizeof(MSG_HEAD_T));
	msg_head_ptr->len = rsplen;
    memcpy(rsp_ptr,msg_head_ptr,sizeof(MSG_HEAD_T));

    aprsp->status = status;
    aprsp->length = CALIBERATE_CNF_LEN;

    total_len = translate_packet((char*)pctool_cnf_buf,rsp_ptr,((MSG_HEAD_T*)rsp_ptr)->len);
    free(rsp_ptr);
    return total_len;
}

#ifdef CONFIG_PCTOOL_AUTHORIZE
static void reply_packet(uint8_t *buf, int len)
{
	const int osz = 32;

	for (; len >= osz; len -= osz) {
		reply_to_pctool(buf, osz);
		buf += osz;
	}

	if (len > 0) {
		reply_to_pctool(buf, len);
	}
}

/**
 * - double check if we need to authorize pctool
 * Return: 0, no; other yes
 */
static int need_to_authorize(void)
{
	return 1;
}

/**
 * - update project information for m1
 * Notice:
 *   the length is limited to 32bytes
 */
static void update_project_info(uint8_t *buf)
{
	uint8_t project[32];

	/* update project info */
	memset(project, 0, sizeof(project));
	strcpy(project, "S6301");
	memcpy(buf, project, sizeof(project));
}

/* return >0 success, other fail */
static int authorize_pkt_translate(uint8_t *src, int len, int limit)
{
	uint8_t *tmp = NULL;
	int i, tl = len * 2;

	tmp = (uint8_t *)malloc(tl);
	if (!tmp) {
		errorf("memory fail len %d\n", tl);
		return -1;
	}

	tl = 0;
	tmp[tl++] = 0x7e;
	for (i = 1; i < len - 1; i++) {
		if (src[i] == 0x7e) {
			tmp[tl++] = 0x7d;
            tmp[tl++] = 0x5e;
		} else if (src[i] == 0x7d) {
            tmp[tl++] = 0x7d;
            tmp[tl++] = 0x5d;
        } else
            tmp[tl++] = src[i];
	}
	tmp[tl++] = 0x7e;

	if (tl > limit) {
		errorf("buffer translate len %d exceed the limit %d\n", tl, limit);
		free(tmp);
		return -2;
	}

	memcpy(src, tmp, tl);
	free(tmp);
	return tl;
}

/* return >0 success, other fail */
static int authorize_pkt_escape(uint8_t *src, int len)
{
	uint8_t *tmp = NULL;
	int i, t;

	tmp = (uint8_t *)malloc(len);
	if (!tmp) {
		errorf("memory fail len %d\n", len);
		return -1;
	}

	for (i = 0, t = 0; i < len; i++, t++) {
		if (src[i] == 0x7d) {
			i++;
			tmp[t] = src[i] ^ 0x20;
		} else {
			tmp[t] = src[i];
		}
	}

	memcpy(src, tmp, t);
	free(tmp);
	return t;
}

/**
 * - pctool authorize
 * return:
 * CMD_CALIBRATION_MODE: 	enter calibration mode
 * CMD_NORMAL_MODE: 		bootup normally when authorize fail
 * CMD_UNDEFINED_MODE: 		don't care
 */
static int pctool_authorize(void)
{
//#define BYPASS_AUTH_TOOL
#define PKT_TYPE	(0x5D)
#define PKT_SUBTYPE	(0x91)
#define DATA_OFFS (9)
#define DATA_BUF_SIZE	((sizeof(AUTH_SECURE_DATA_T) + 64) * 2)
	char *buf;
	OT_Auth_req_t *req;
	OT_Auth_ack_t ack;
	AUTH_SECURE_DATA_T *data;
	int len, total, ret;
	uint8_t *type;
	uint8_t *subtype;

	debugf("authorize enter\n");
	/* 1. malloc memory */
	req = malloc(sizeof(OT_Auth_req_t));
	if (req == NULL) {
		errorf("No enough memory for req buf\n");
		goto auth_fail;
	}
	memset(req, 0, sizeof(OT_Auth_req_t));

	data = malloc(sizeof(AUTH_SECURE_DATA_T));
	if (data == NULL) {
		errorf("No enough memory for data buf\n");
		goto auth_fail;
	}
	memset(data, 0, sizeof(AUTH_SECURE_DATA_T));

	buf = malloc(DATA_BUF_SIZE);
	if (buf == NULL) {
		errorf("No enough memory for data buf\n");
		goto auth_fail;
	}
	memset(buf, 0, DATA_BUF_SIZE);
	type = &buf[7];
	subtype = &buf[8];

	dprintf(INFO,"type addr:%llx, subtype addr:%llx\n", &type, &subtype);
	/* 2. auth begin */
	/* 2.1 parse cmd */
	len = cali_get_cmd_ex(buf, CALIBERATE_STRING_LEN_14 * 2);
	if (!len) {
		errorf("auth begin fail error len\n");
		goto auth_fail;
	}

	if ((len = authorize_pkt_escape(buf, len)) <= 0) {
		errorf("auth begin escape fail\n");
		goto auth_fail;
	}

	if (len != CALIBERATE_STRING_LEN_14
			|| *type != PKT_TYPE || *subtype != PKT_SUBTYPE) {
		errorf("auth begin fail len %d type %d, subtype %d\n", len, type, subtype);
		goto auth_fail;
	}

	memcpy(req, &buf[DATA_OFFS], sizeof(OT_Auth_req_t));
	if (req->mReqType != DIAG_AP_CMD_AUTH_BEGIN) {
		errorf("auth begin fail err cmd %d\n", req->mReqType);
		goto auth_fail;
	}

	/* 2.2 generate m1 */
	ack.mRspType = DIAG_AP_CMD_AUTH_BEGIN;
	ack.mLength = sizeof(*data);
	ack.mStatus = 1;
#ifndef BYPASS_AUTH_TOOL
	if (!need_to_authorize()) {
		ack.mStatus = 2;
	} else {
		if ((len = authentication(buf + DATA_OFFS + sizeof(ack), 1))
						!= sizeof(data->S1)) {
			errorf("auth tools get m1 fail\n");
			len = sizeof(data->S1);
		} else {
			ack.mStatus = 0; /* success */
		}
	}
#else //debug
	len = 256;
	ack.mStatus = 0; /* success */
#endif

	memcpy(buf + DATA_OFFS, &ack, sizeof(ack));
	total = CALIBERATE_STRING_LEN + sizeof(ack) + len + 32;
	buf[5] = (total - 2) & 0xFF;				/* update length */
	buf[6] = ((total - 2) >> 8) & 0xFF;

	/* update project info */
	update_project_info(buf + total - 1 - 32);

	/* 2.3 uboot->pc send m1 */
	debugf("send m1\n");
	buf[total - 1] = 0x7E;				/* pkt end with 7E */

	total = authorize_pkt_translate(buf, total, DATA_BUF_SIZE);
	if (total <= 0) {
		errorf("auth begin translate fail\n");
		goto auth_fail;
	}

	reply_packet(buf, total);

	if (ack.mStatus == 1) {
		goto auth_fail;
	} else if (ack.mStatus == 2) {
		debugf("authorize was not need\n");
		return CMD_CALIBRATION_MODE;
	}

	/* 3. pc->uboot secure data */
	debugf("wait secure data\n");

	ack.mRspType = DIAG_AP_CMD_AUTH_END;
	ack.mLength = 0;
	ack.mStatus = 1;

	memset(buf, 0, DATA_BUF_SIZE);
	len = cali_get_cmd_ex(buf, DATA_BUF_SIZE);
	if (!len) {
		errorf("auth end fail error len\n");
		goto auth_fail;
	}
	crypto_hexdump("from pc M2-before translate:", buf, len);

	if ((len = authorize_pkt_escape(buf, len)) <= 0) {
		errorf("auth end escape fail\n");
		goto auth_fail;
	}

	dprintf(INFO,"type addr:%llx, subtype addr:%llx\n", &type, &subtype);
	if ((len != (CALIBERATE_STRING_LEN + sizeof(OT_Auth_req_t) + sizeof(AUTH_SECURE_DATA_T)))
			|| *type != PKT_TYPE || *subtype != PKT_SUBTYPE) {
		errorf("auth end fail len %d type %d, subtype %d\n", len, type, subtype);
		goto auth_fail;
	}

	memcpy(req, &buf[DATA_OFFS], sizeof(OT_Auth_req_t));
	if (req->mReqType != DIAG_AP_CMD_AUTH_END) {
		errorf("auth end fail err cmd %d\n", req->mReqType);
		goto auth_fail;
	}
	crypto_hexdump("from pc M2-after translate:", buf, len);

	crypto_hexdump("from pc M2-to auth:", buf + DATA_OFFS + sizeof(OT_Auth_req_t), len - (DATA_OFFS + sizeof(OT_Auth_req_t)));
	/* 4. secure data verify uboot->pc */
#ifndef BYPASS_AUTH_TOOL
	if ((authentication(buf + DATA_OFFS + sizeof(OT_Auth_req_t), 0) < 0)) {
		errorf("auth end secure data fail\n");
		goto auth_fail;
	}
#endif

	ack.mStatus = 0;
	memcpy(buf + DATA_OFFS, &ack, sizeof(ack));
	total = CALIBERATE_STRING_LEN + sizeof(ack);
	buf[5] = (total - 2) & 0xFF;
	buf[6] = 0;
	buf[total - 1] = 0x7E;

	total = authorize_pkt_translate(buf, total, DATA_BUF_SIZE);
	if (total <= 0) {
		errorf("auth begin translate fail\n");
		goto auth_fail;
	}

	reply_packet(buf, total);

	debugf("authorize success\n");
	return CMD_CALIBRATION_MODE;

auth_fail:
	buf[0] = 0x7E;
	//buf[1] = buf[2] = buf[3] = buf[4] = 0; //keep SN
	total = CALIBERATE_STRING_LEN + sizeof(ack);
	buf[5] = (total - 2) & 0xFF;
	buf[6] = 0;
	buf[7] = PKT_TYPE;
	buf[8] = PKT_SUBTYPE;
	memcpy(buf + DATA_OFFS, &ack, sizeof(ack));
	buf[total - 1] = 0x7E;

	total = authorize_pkt_translate(buf, total, DATA_BUF_SIZE);
	if (total > 0)
		reply_packet(buf, total);
	else
		errorf("auth fail send packet fail\n");

	if (buf != NULL) {
		free(buf);
	}
	if (data != NULL) {
		free(data);
	}
	if (req != NULL) {
		free(req);
	}
	return CMD_NORMAL_MODE;
#undef DATA_BUF_SIZE
#undef DATA_OFFS
#undef PKT_TYPE
#undef PKT_SUBTYPE
}
#endif

static int pctool_mode_detect_cali_command_process(void)
{
	int command , ret , mode;
	mode = -1;
	ret = cali_get_cmd(pctool_cmd_buf, CALIBERATE_STRING_LEN_14);
	if(!ret)
		return -1;
	command = check_pctool_cmd(pctool_cmd_buf, CALIBERATE_STRING_LEN_14);
	if(!command)
		return -1;
	else if(DIAG_AP_CMD_PROGRAM_KEY == command)
	{
#if defined (SPRD_SECBOOT)
	ret = secure_efuse_program();
	if(!ret)
		ret = prepare_reply_buf(pctool_cmd_buf ,CALIBERATE_CNF_SCS);
	else{
		debugf("write efuse failed and error code = %d \n" , ret);
		ret = prepare_reply_buf(pctool_cmd_buf , CALIBERATE_CNF_FAIL);
	}
	if(ret)
		ret = reply_to_pctool(pctool_cnf_buf, ret);
	if(!ret)
		return -1;
#endif
	}
	ret = cali_get_cmd(pctool_cmd_buf,CALIBERATE_STRING_LEN_16);
	if(!ret)
		return -1;
	command = check_pctool_cmd(pctool_cmd_buf , CALIBERATE_STRING_LEN_16);
	if(!command)
		return -1;
	switch(command){
		case 0xffff:
			ret = prepare_reply_buf(pctool_cmd_buf ,CALIBERATE_CNF_SCS);
			reply_to_pctool(pctool_cnf_buf, ret);
			mode = CMD_POWER_DOWN_DEVICE;
			return mode;
			break;
		case 0xfffe:
			ret = prepare_reply_buf(pctool_cmd_buf ,CALIBERATE_CNF_SCS);
			reply_to_pctool(pctool_cnf_buf, ret);
			mode = CMD_CHARGE_MODE;
			return mode;
			break;
		default:
			ret = prepare_reply_buf(pctool_cmd_buf ,CALIBERATE_CNF_SCS);
			reply_to_pctool(pctool_cnf_buf, ret);
			break;
	}
	return mode;
}

static int cali_mode_authorize_process(int command) {
	int ret;
#ifdef CONFIG_PCTOOL_AUTHORIZE
	/* pctool authorize */
	ret = pctool_authorize();
	if (ret != CMD_UNDEFINED_MODE) {
		if (ret == CMD_CALIBRATION_MODE) {
			snprintf(calibration_cmd_buf, MAX_CALIBRATION_LEN, "calibration=%d,%d,146 ", command&0xff, (command&(~0xff))>>8);
			usb_driver_exit();
		} else {
			s_is_calibration_mode = 0;
		}
		return ret;
	}
#else
#if defined(SPRD_SECBOOT) && defined(CONFIG_BLOCK_ENTER_MODE)
	extern int get_lcs(uint32_t *p_lcs);
	unsigned int __aligned(ARCH_DMA_MINALIGN) block_val;
	unsigned int *t_lcs = memalign(SZ_4K, sizeof(unsigned int));
	if (t_lcs == NULL) {
		errorf("cali mode no enough heap for set getlcs\n");
		return -1;
	}

	if (!(get_lcs(&t_lcs) || (5 != t_lcs))) {
		debugf("check efuse secboot was enabled\n");
		if (!common_raw_read("miscdata", STARTUP_BLOCK_FLAG_LEN,
					STARTUP_BLOCK_FLAG_OFFSET, (char *)&block_val)) {
			if ((0x5A == ((block_val & 0xFF000000) >> 24))
					&& (block_val & BIT(BIT_STARTUP_BLOCK_CALI))) {
				snprintf(calibration_cmd_buf, MAX_CALIBRATION_LEN,
					"calibration=%d,%d,146 ", command&0xff,
					(command&(~0xff))>>8);
				ret = CMD_CALIBRATION_MODE;
				usb_driver_exit();
				return ret;
			}
		}
		dprintf(INFO,"calibration mode was block\n");
		ret = CMD_NORMAL_MODE;
		usb_driver_exit();
		return ret;
	}

	free(t_lcs);
#endif
	snprintf(calibration_cmd_buf, MAX_CALIBRATION_LEN, "calibration=%d,%d,146 ", command&0xff, (command&(~0xff))>>8);
	ret = CMD_CALIBRATION_MODE;
	usb_driver_exit();
	return ret;
#endif
	return -1;
}

#if defined(PLATFORM_QOGIRN6PRO) && defined(CONFIG_UMS9620_WATCH)
static int download_without_press(int recive_char, uint8_t *pctool_cmd_buf) {
	int i = 0;
	int j = 0;

	printf("enter:%s\n", __func__);
	if (recive_char != CALIBERATE_STRING_LEN) {
		errorf("receive length not equal to %d\n", CALIBERATE_STRING_LEN);
		return -1;
	}

	for(i = 0; i < recive_char; i++) {
		if (pctool_cmd_buf[i] == 0x7e) {
			j++;
		} else {
			break;
		}
	}

	if (j == CALIBERATE_STRING_LEN) {
		printf("got 10 times 7e and enter autodloader_mode\n");
		return 0;
	}
	return -1;
}
#endif

int pctool_mode_detect(void)
{

	int ret , command , bat_info , mode;
	int recive_char = 0;
	uint8_t pctool_cmd_backup_buf[CMD_BUF_NUMBER] = {0};

	memset(pctool_cmd_buf, 0, CMD_BUF_NUMBER);
    //if(get_mode_from_gpio() && (get_poweron_src() == POWERON_JIG))
	if(get_mode_from_gpio()) {
		return pctool_mode_detect_uart();
	}

	if (get_mode_from_vchg()) {
		bat_info = sprdchg_charger_is_adapter();
		debugf("%s: charger connected, the adapter is %d\n", __func__, bat_info);
		if ((bat_info == ADP_TYPE_DCP) || (bat_info == ADP_TYPE_UNKNOW))
			return -1;
	}else{
		errorf("%s: charger is not connected\n", __func__);
		return -1;
	}

	debugf("%s: usb cooperating with pc tool\n", __func__);

	ret = cali_usb_prepare();
	if (!ret) {
		usb_serial_cleanup();
		usb_driver_exit();
		return -1;
	}

#if IO_DEBUG
	cali_usb_debug(pctool_cmd_buf);
#endif
	ret = cali_usb_enum();
	if (!ret) {
		usb_serial_cleanup();
		usb_driver_exit();
		return -1;
	}

	recive_char = cali_get_cmd(pctool_cmd_buf, CALIBERATE_STRING_LEN);
	if (!recive_char) {
		usb_serial_cleanup();
		usb_driver_exit();
		return -1;
	}

	memcpy(pctool_cmd_backup_buf, pctool_cmd_buf, recive_char);

	ret = escape_char(pctool_cmd_buf, recive_char);
	if (!ret) {
		usb_serial_cleanup();
		usb_driver_exit();
		return -1;
	}

#if defined(PLATFORM_QOGIRN6PRO) && defined(CONFIG_UMS9620_WATCH)
	if (!download_without_press(recive_char, pctool_cmd_buf)) {
		usb_serial_cleanup();
		usb_driver_exit();
		return CMD_AUTODLOADER_REBOOT;
	}
#endif

	command = check_pctool_cmd(pctool_cmd_buf, CALIBERATE_STRING_LEN);
	debugf("%s caliberate_mode: %x \n",__func__, command&0xff);
	if (!command) {
		debugf("func: %s line: %d caliberate failed\n", __func__, __LINE__);
		usb_serial_cleanup();
		usb_driver_exit();
		return -1;
	}

	ret = reply_to_pctool(pctool_cmd_backup_buf, recive_char);
	if (!ret) {
		usb_serial_cleanup();
		usb_driver_exit();
		return -1;
	}

	if (CALIBERATE_COMMAND_PROGRAMKEY == command)
	{
		return pctool_mode_detect_cali_command_process();
	}else if (CALIBERATE_COMMAND_AUTODOWNLOAD == command) {
		usb_serial_cleanup();
		usb_driver_exit();
		return CMD_AUTODLOADER_REBOOT;
	}

	memset(calibration_cmd_buf, 0, MAX_CALIBRATION_LEN);
	s_is_calibration_mode=1;

	switch (command & 0xff){
		case CALIBERATE_COMMAND_AUTOTEST:
			snprintf(calibration_cmd_buf, MAX_CALIBRATION_LEN, "autotest=1 ");
			ret = CMD_AUTOTEST_MODE;
			usb_driver_exit();
			break;
		default:
			ret = cali_mode_authorize_process(command);
			break;
	}
	return ret;
}

int poweron_by_calibration(void)
{
	return s_is_calibration_mode;
}

int cali_file_check(void)
{

#define CALI_MAGIC      (0x49424143) //CALI
#define CALI_COMP       (0x504D4F43) //COMP

	if(do_fs_file_read("prodnv", "/adc.bin", (char *)nv_buffer,sizeof(nv_buffer)))
		return 1;

	if((nv_buffer[0] != CALI_MAGIC)||(nv_buffer[1]!=CALI_COMP))
		return 1;
	else
		return 0;
}

#define VLX_ADC_ID   2
#define VLX_RAND_TO_U32( _addr )	if( (_addr) & 0x3 ){_addr += 0x4 -((_addr) & 0x3); }

int read_adc_calibration_data(char *buffer,int size)
{
	chipram_env_t* cr_env = get_chipram_env();
	boot_mode_t boot_role = cr_env->mode;
	if (boot_role == BOOTLOADER_MODE_DOWNLOAD)
		return 0;

	unsigned int adc_magic_flag = 0;
	int adc_magic_len = sizeof(unsigned int);
	int tmp_size = adc_magic_len + sizeof(nv_buffer);
	char *tmp = malloc_cache_aligned(tmp_size);
	if (tmp == NULL) {
		errorf("read_adc_calibration_data malloc failed!\n");
		return -1;
	}

	debugf("start read miscdata adc data...\n");
	if(common_raw_read("miscdata", (uint64_t)tmp_size, ADC_DATA_START, tmp)) {
		errorf("read miscdata adc data error.\n");
		free(tmp);
		return 0;
	}
	adc_magic_flag = *((unsigned int *)tmp);
	if(adc_magic_flag == ADC_MAGIC) {
		debugf("ADC MAGIC is true!\n");
		memcpy(nv_buffer, tmp + adc_magic_len, sizeof(nv_buffer));

	} else {
		debugf("ADC MAGIC is false!\nstart read prodnv /adc.bin...\n");
		if(do_fs_file_read("prodnv", "/adc.bin", (char *)nv_buffer,sizeof(nv_buffer))) {
			debugf("%s: cannot read prodnv /adb.bin.\n", __func__);
			free(tmp);
			return 0;
		}

	}

	if(size>48)
		size=48;
	memcpy(buffer,&nv_buffer[2],size);
	free(tmp);

	return size;
}

void download_after_enter_cali_mode(u8 cail_mode)
{
	memset(calibration_cmd_buf, 0, MAX_CALIBRATION_LEN);
	s_is_calibration_mode = 1;
	snprintf(calibration_cmd_buf, MAX_CALIBRATION_LEN, "calibration=%d,%d,146 ", cail_mode, 0);
}

void download_after_enter_autotest_mode(u8 cali_mode)
{
	memset(calibration_cmd_buf, 0, MAX_CALIBRATION_LEN);
	s_is_calibration_mode = 1;
	snprintf(calibration_cmd_buf, MAX_CALIBRATION_LEN, "autotest=1 ");
}

#ifdef CONFIG_HMD_FASTBOOT
void fastboot_enter_cali_mode(u8 cail_mode, u8 cail_freq)
{
	memset(calibration_cmd_buf, 0, MAX_CALIBRATION_LEN);
	s_is_calibration_mode = 1;
	snprintf(calibration_cmd_buf, MAX_CALIBRATION_LEN, "calibration=%d,%d,146 ", cail_mode, cail_freq);
}
#endif
