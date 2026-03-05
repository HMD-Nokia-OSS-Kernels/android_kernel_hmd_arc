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

#ifndef __FDL_CHANNEL_H_
#define __FDL_CHANNEL_H_


typedef struct FDL_ChannelHandler
{
    int (*Open) (struct FDL_ChannelHandler *channel, unsigned int  baudrate);
    int (*Read) (struct FDL_ChannelHandler *channel, const unsigned char *buf, unsigned int  len);
    int (*StartReadRawData) (struct FDL_ChannelHandler *channel, const unsigned char *buf, unsigned int  len);
    int (*FinishReadRawData) (struct FDL_ChannelHandler *channel, const unsigned char *buf, unsigned int  len);
    char (*GetChar) (struct FDL_ChannelHandler *channel);
    int (*GetSingleChar) (struct FDL_ChannelHandler *channel);
    int (*Write) (struct FDL_ChannelHandler *channel, const unsigned char *buf, unsigned int  len);
    int (*PutChar) (struct FDL_ChannelHandler *channel, const unsigned char ch);
    int (*SetBaudrate) (struct FDL_ChannelHandler *channel, unsigned int  baudrate);
    int (*DisableHDLC) (struct FDL_ChannelHandler *channel, int  disabled);
    int (*SupportRawDataProc) (struct FDL_ChannelHandler  *channel);
    int (*Close) (struct FDL_ChannelHandler *channel);
    void   *priv;
} FDL_ChannelHandler_T;

struct FDL_ChannelHandler *FDL_ChannelGet(void);
struct FDL_ChannelHandler *FDL_USBChannel(void);

#endif
