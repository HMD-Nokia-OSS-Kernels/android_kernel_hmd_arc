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

#include <dl_channel.h>

#define BOOT_FLAG_USB                   (0x5A)
#define BOOT_FLAG_UART1                 (0x6A)
#define BOOT_FLAG_UART0                 (0x7A)


/******************************************************************************/
//  Description:    find a useable channel
//  Global resource dependence:
//  Author:         junqiang.wang
//  Note:
/******************************************************************************/
extern struct FDL_ChannelHandler gUart0Channel, gUart1Channel;
extern struct FDL_ChannelHandler gUSBChannel;

struct FDL_ChannelHandler *FDL_ChannelGet(void)
{
    unsigned int bootMode = 0;

    struct FDL_ChannelHandler *channel;
	bootMode = BOOT_FLAG_USB;
#ifdef CONFIG_UART_DOWNLOAD
	bootMode = BOOT_FLAG_UART1;
#endif

    switch (bootMode)
    {
#ifdef CONFIG_UART_DOWNLOAD
        case BOOT_FLAG_UART1:
            channel = &gUart1Channel;
            channel -> Open(channel, 115200);
            break;
        case BOOT_FLAG_UART0:
            channel = &gUart0Channel;
            break;
#endif
        case BOOT_FLAG_USB:
            channel = &gUSBChannel;
            break;
        default:
            channel = &gUSBChannel;
            break;
    }
    return channel;
}

struct FDL_ChannelHandler *FDL_USBChannel(void)
{
	return &gUSBChannel;
}

