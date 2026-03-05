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

#ifndef PACKET_H
#define PACKET_H
#include <sys/types.h>
#include "dl_cmd_def.h"

#define MAX_PKT_SIZE    0x10000//0x10000 /* Just data field of a packet excluding header and checksum */
#define PACKET_HEADER_SIZE   4   // (type + size)
#define PACKET_MAX_NUM    3
/* raw data packet length */
#define RECV_PKT_SIZE  0x400000
/* half packet length */
#define HALF_PKT_SIZE  RECV_PKT_SIZE/2
/* commond length */
#define MIN_PROC_LENGTH   0x08

#define CRC_16_POLYNOMIAL       0x1021
#define CRC_16_L_POLYNOMIAL     0x8000
#define CRC_16_L_SEED           0x80
#define CRC_16_L_OK             0x00
#define HDLC_FLAG               0x7E
#define HDLC_ESCAPE             0x7D
#define HDLC_ESCAPE_MASK        0x20
#define CRC_CHECK_SIZE          0x02

typedef enum
{
    PKT_NONE = 0,
    PKT_HEAD,
    PKT_GATHER,
    PKT_RECV,
    PKT_ERROR
} pkt_flag_s;


struct pkt_body {
    unsigned short  type;
    unsigned short  size;
    unsigned char   content[ MAX_PKT_SIZE ];
};

typedef struct dl_packet {
    struct dl_packet *next;
    int	pkt_state;
    int	data_size;
    int	ack_flag;
    struct pkt_body body;
}dl_packet_t;

dl_packet_t *FDL_MallocPacket (void);
static void dl_ackpkt_init(void);
void dl_packet_init (void);
dl_packet_t* dl_get_packet (void);
void dl_free_packet (struct dl_packet *pkt);
void dl_send_packet (struct dl_packet *pkt);
void dl_send_ack (dl_cmd_type_t  pkt_type);
void FDL_DisableHDLC (int disabled);
int * FDL_get_DisableHDLC(void);
int FDL_get_SupportRawDataProc(void);
int FDL_StartRxRawPacket(unsigned char *buf, unsigned int len);
int FDL_FinishRxRawPacket(unsigned char *buf, unsigned int len);
unsigned int crc_16_l_calc (char *buf_ptr,unsigned int len);
unsigned short frm_chk (const unsigned short *src, int len);
int is_usb_disconnected(void);

#endif  // PACKET_H

