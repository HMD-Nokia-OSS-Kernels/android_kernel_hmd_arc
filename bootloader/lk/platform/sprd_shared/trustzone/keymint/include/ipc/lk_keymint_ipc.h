/*
* Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
* Licensed under the Unisoc General Software License, version 1.0 (the License);
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
* Software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* See the Unisoc General Software License, version 1.0 for more details.
*/

#ifndef LK_KEYMINT_IPC_H
#define LK_KEYMINT_IPC_H

#include <trusty/trusty_dev.h>
#include <trusty/trusty_ipc.h>
#include <trusty/util.h>
#include <ipc/keymint_ipc.h>
#include <lk_keymint_messages.h>

#define KEYMINT_RECV_BUF_SIZE 4 * KM_IPC_SIZE
#define KEYMINT_SEND_BUF_SIZE \
          (2 * KM_IPC_SIZE - sizeof(struct keymaster_message) - 16 /* tipc header */)

// Initialize trusty device and trusty ipc device.
void km_ipc_init(void);

// Establish IPC connection between KM client and KM server.
int km_ipc_connect(void);

// Close the IPC connection between KM client and KM server.
void km_ipc_disconnect(void);

// Release the resources of trusty device and trusty ipc device.
void km_ipc_shutdown(void);

// Send and receive ipc data.
int lk_keymint_call(uint32_t cmd,
                    void* req_buffer, uint32_t req_size,
                    uint8_t* rsp_buffer, uint32_t* rsp_size);

// Process ipc data.
keymaster_error_t lk_keymint_send(struct km_ipc_task* ipc_task,
                                  void* req_buffer, void* rsp_buffer);


#endif /* LK_KEYMINT_IPC_H */
