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

#ifndef LK_KEYMINT_MESSAGES_H_
#define LK_KEYMINT_MESSAGES_H_

#include <hardware/keymint_defs.h>
#include <serializable.h>

/*********************************************
 * Get error code from keymint TA response.
 ********************************************/
bool km_rsp_error_deserialize(keymaster_error_t* err, const uint8_t** buf_ptr, const uint8_t* end);


/***********************************************
 * Some command does't need to transmit data.
 **********************************************/
size_t km_empty_serialized_size(void* content_not_use);
uint8_t* km_empty_serialize(void* content, uint8_t*buf, const uint8_t* end);
bool km_empty_deserialize(void* content, const uint8_t** buf_ptr, const uint8_t* end);


/************************************
 * Used by command KM_GET_VERSION.
 ***********************************/
typedef struct km_get_version_rsp {
    uint8_t major_ver;
    uint8_t minor_ver;
    uint8_t subminor_ver;
} TRUSTY_ATTR_PACKED km_get_version_rsp;

bool km_get_version_deserialize(void* content, const uint8_t** buf_ptr, const uint8_t* end);


/**************************************************
 * Used by command KM_CONFIGURE_BOOT_PATCHLEVEL.
 *************************************************/
typedef struct km_boot_patchlevel_req {
    uint32_t boot_patchlevel;
} TRUSTY_ATTR_PACKED km_boot_patchlevel_req;

size_t km_boot_patchlevel_serialized_size(void* content_not_use);
uint8_t* km_boot_patchlevel_serialize(void* content, uint8_t* buf,  const uint8_t* end);


/**************************************************
 * Used by command KM_SET_BOOT_PARAMS.
 *************************************************/
#define VERIFIED_BOOT_KEY_MAX_LENGTH        32U
#define VERIFIED_BOOT_HASH_MAX_LENGTH       32U

typedef struct km_set_boot_params_req {
    uint32_t os_version;
    uint32_t os_patchlevel;
    uint32_t device_locked;
    keymaster_verified_boot_t verified_boot_state;
    uint32_t verified_boot_key_len;
    uint8_t verified_boot_key[VERIFIED_BOOT_KEY_MAX_LENGTH];
    uint32_t verified_boot_hash_len;
    uint8_t verified_boot_hash[VERIFIED_BOOT_HASH_MAX_LENGTH];
} TRUSTY_ATTR_PACKED km_set_boot_params_req;

size_t km_set_boot_params_serialized_size(void* req);
uint8_t* km_set_boot_params_serialize(void* content, uint8_t* buf, const uint8_t* end);


#endif /* LK_KEYMINT_MESSAGES_H_ */