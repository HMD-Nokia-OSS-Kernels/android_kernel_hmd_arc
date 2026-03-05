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

#include <serializable.h>

bool __buffer_bound_check(const uint8_t* buf, const uint8_t* end, size_t len) {
    uintptr_t buf_next = (uintptr_t)buf + len;

    bool overflow_occurred = (len > UINTPTR_MAX - (uintptr_t)buf) ? true : false;
    return (!overflow_occurred) && (buf_next <= (uintptr_t)end);
}

uint8_t* km_append_to_buf(uint8_t* buf, const uint8_t* end, const void* data, size_t data_len) {
    if (__buffer_bound_check(buf, end, data_len)) {
        trusty_memcpy(buf, data, data_len);
        return buf + data_len;
    } else {
        return buf;
    }
}

bool km_copy_from_buf(const uint8_t** buf_ptr, const uint8_t* end, void* dest, size_t size) {
    if (__buffer_bound_check(*buf_ptr, end, size)) {
        trusty_memcpy(dest, *buf_ptr, size);
        *buf_ptr += size;
        return true;
    } else {
        return false;
    }
}
