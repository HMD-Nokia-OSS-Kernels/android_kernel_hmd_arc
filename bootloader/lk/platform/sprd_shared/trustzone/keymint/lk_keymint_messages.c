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

#include <lk_keymint_messages.h>

bool km_rsp_error_deserialize(keymaster_error_t* err, const uint8_t** buf_ptr, const uint8_t* end) {
    if(!err || !buf_ptr || !end) {
        return false;
    }

    uint32_t error;
    if (!km_copy_uint32_from_buf(buf_ptr, end, &error)) {
        return false;
    }

    *err = (keymaster_error_t)error;

    return true;
}

size_t km_empty_serialized_size(void* content_not_use) {
    (void)content_not_use;

    return 0;
}

uint8_t* km_empty_serialize(void* content, uint8_t* buf, const uint8_t* end) {
    return buf;
}

size_t km_empty_deserialized_size(void) {
    return 0;
}

bool km_empty_deserialize(void* content, const uint8_t** buf_ptr, const uint8_t* end) {
    return true;
}

bool km_get_version_deserialize(void* content, const uint8_t** buf_ptr, const uint8_t* end) {
    if(!content || !buf_ptr || !end) {
        return false;
    }

    struct km_get_version_rsp* data = (struct km_get_version_rsp*)content;
    const uint8_t* tmp = *buf_ptr;
    data->major_ver = *tmp++;
    data->minor_ver = *tmp++;
    data->subminor_ver = *tmp++;
    *buf_ptr = tmp;

    return true;
}

size_t km_boot_patchlevel_serialized_size(void* content_not_use) {
    (void)content_not_use;
    struct km_boot_patchlevel_req data;

    return sizeof(data.boot_patchlevel);
}

uint8_t* km_boot_patchlevel_serialize(void* content, uint8_t* buf, const uint8_t* end) {
    struct km_boot_patchlevel_req* data = (struct km_boot_patchlevel_req*)content;

    return km_append_uint32_to_buf(buf, end, data->boot_patchlevel);
}

size_t km_set_boot_params_serialized_size(void* req) {
    km_set_boot_params_req* data = (km_set_boot_params_req*)req;

    return (sizeof(data->os_version) +
            sizeof(data->os_patchlevel) +
            sizeof(data->device_locked) +
            sizeof((uint32_t)(data->verified_boot_state)) +
            sizeof(data->verified_boot_key_len) +
            data->verified_boot_key_len +
            sizeof(data->verified_boot_hash_len) +
            data->verified_boot_hash_len);
}

uint8_t* km_set_boot_params_serialize(void* content, uint8_t* buf, const uint8_t* end) {
    struct km_set_boot_params_req* data = (struct km_set_boot_params_req*)content;

    buf = km_append_uint32_to_buf(buf, end, data->os_version);
    buf = km_append_uint32_to_buf(buf, end, data->os_patchlevel);
    buf = km_append_uint32_to_buf(buf, end, data->device_locked);
    buf = km_append_uint32_to_buf(buf, end, data->verified_boot_state);
    buf = km_append_size_and_data_to_buf(buf, end, data->verified_boot_key, data->verified_boot_key_len);

    return km_append_size_and_data_to_buf(buf, end, data->verified_boot_hash, data->verified_boot_hash_len);
}
