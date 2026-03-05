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

#ifndef KEYMINT_SERIALIZABLE_H_
#define KEYMINT_SERIALIZABLE_H_

#include <trusty/sysdeps.h>
#include <sys/types.h>

/**
 * Performs an overflow-checked bounds check.
 * Returns true if |buf| + |len| is less than |end|.
 */
bool __buffer_bound_check(const uint8_t* buf, const uint8_t* end, size_t len);


/*******************************************************
 * Utility functions for writing Serialize() methods.
 ******************************************************/

/**
 * Appends |data_len| bytes at |data| to |buf|.
 * Performs no bounds checking, assumes sufficient memory allocated at |buf|.
 * Returns |buf| + |data_len|.
 */
uint8_t* km_append_to_buf(uint8_t* buf, const uint8_t* end, const void* data, size_t data_len);


/**
 * Appends |val| to |buf|. Performs no bounds checking.
 * Returns |buf| + sizeof(uint32_t).
 */
inline uint8_t* km_append_uint32_to_buf(uint8_t* buf, const uint8_t* end, uint32_t val) {
    return km_append_to_buf(buf, end, &val, sizeof(val));
}

/**
 * Appends a byte array to a buffer, prefixing it with a 32-bit size field.  Returns a pointer to
 * the first byte after the data written.
 *
 * See copy_size_and_data_from_buf().
 */
inline uint8_t* km_append_size_and_data_to_buf(uint8_t* buf, const uint8_t* end,
                                            const void* data, size_t data_len) {
    buf = km_append_uint32_to_buf(buf, end, data_len);
    return km_append_to_buf(buf, end, data, data_len);
}


/*********************************************************
 * Utility functions for writing Deserialize() methods.
 *********************************************************/

/**
 * Copy |size| bytes from |*buf_ptr| into |dest|.
 * If there are fewer than |size| bytes to read, returns false.
 * Advances |*buf_ptr| to the next byte to be read.
 */
bool km_copy_from_buf(const uint8_t** buf_ptr, const uint8_t* end, void* dest, size_t size);

/**
 * Copies a |value| convertible from uint32_t from |*buf_ptr|.
 * Returns false if there are less than four bytes remaining in |*buf_ptr|.
 * Advances |*buf_ptr| to the next byte to be read.
 */
inline bool km_copy_uint32_from_buf(const uint8_t** buf_ptr, const uint8_t* end, uint32_t* value) {
    if (!km_copy_from_buf(buf_ptr, end, value, sizeof(uint32_t))) return false;
    return true;
}


#endif /* KEYMINT_SERIALIZABLE_H_ */
