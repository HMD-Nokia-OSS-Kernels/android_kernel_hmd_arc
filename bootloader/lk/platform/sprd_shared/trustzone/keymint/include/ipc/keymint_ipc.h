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

#ifndef KEYMINT_IPC_H
#define KEYMINT_IPC_H

#define KM_IPC_SIZE 1024
#define KEYMINT_PORT "com.android.trusty.keymaster"
#define KEYMINT_MAX_BUFFER_LENGTH 4 * KM_IPC_SIZE

#include <hardware/keymint_defs.h>

// Commands
enum keymaster_command {
    KEYMASTER_RESP_BIT = 1,
    KEYMASTER_STOP_BIT = 2,
    KEYMASTER_REQ_SHIFT = 2,

    KM_GET_VERSION                   = (7 << KEYMASTER_REQ_SHIFT),

    // Bootloader/Provisioning calls.
    KM_SET_BOOT_PARAMS               = (0x1000 << KEYMASTER_REQ_SHIFT),
    KM_SET_ATTESTATION_KEY           = (0x2000 << KEYMASTER_REQ_SHIFT),
    KM_APPEND_ATTESTATION_CERT_CHAIN = (0x3000 << KEYMASTER_REQ_SHIFT),
    KM_ATAP_GET_CA_REQUEST           = (0x4000 << KEYMASTER_REQ_SHIFT),
    KM_ATAP_SET_CA_RESPONSE_BEGIN    = (0x5000 << KEYMASTER_REQ_SHIFT),
    KM_ATAP_SET_CA_RESPONSE_UPDATE   = (0x6000 << KEYMASTER_REQ_SHIFT),
    KM_ATAP_SET_CA_RESPONSE_FINISH   = (0x7000 << KEYMASTER_REQ_SHIFT),
    KM_ATAP_READ_UUID                = (0x8000 << KEYMASTER_REQ_SHIFT),
    KM_SET_PRODUCT_ID                = (0x9000 << KEYMASTER_REQ_SHIFT),
    KM_CLEAR_ATTESTATION_CERT_CHAIN  = (0xa000 << KEYMASTER_REQ_SHIFT),
    KM_SET_WRAPPED_ATTESTATION_KEY   = (0xb000 << KEYMASTER_REQ_SHIFT),
    KM_SET_ATTESTATION_IDS           = (0xc000 << KEYMASTER_REQ_SHIFT),
    KM_CONFIGURE_BOOT_PATCHLEVEL     = (0xd000 << KEYMASTER_REQ_SHIFT),

};

/**
 * km_ipc_task - Provide the functions and KM server processing results
 *               required for ipc communication with KM server.
 * @cmd: the command, one of keymaster_command.
 * @serialized_size: a function pointer to get the size of the data to send to KM server.
 * @serialize: a function pointer that stores the data to be sent to KM server in the buffer.
 * @deserialize: a function pointer to parse the data returned by keymint.
 * @error: the processing results returned by KM server.
 */
typedef struct km_ipc_task {
    enum keymaster_command cmd;
    size_t (*serialized_size)(void* req);
    uint8_t* (*serialize)(void* data, uint8_t* buf, const uint8_t* end);
    bool (*deserialize)(void* content, const uint8_t** buf_ptr, const uint8_t* end);
    keymaster_error_t error;
} km_ipc_task;

/**
 * keymaster_message - Serial header for communicating with KM server
 * @cmd: the command, one of keymaster_command.
 * @payload: start of the serialized command specific payload
 */
struct keymaster_message {
    uint32_t cmd;
    uint8_t payload[0];
};


#endif /* KEYMINT_IPC_H */
