/*
 * Copyright (c) 2008 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <stdint.h>

uint32_t ascii_to_hex(uint8_t *src, uint8_t *dst, uint16_t len) {
        uint16_t i, j;
        uint16_t tmp_len = 0;
        uint8_t tmpData;
        uint8_t *ascii_data = src;

        for (i = 0; i<len; i++) {
                if ((ascii_data[i] >= '0')&&(ascii_data[i] <= '9')) {
                        tmpData = ascii_data[i] - '0';
                } else if ((ascii_data[i] >= 'A')&&(ascii_data[i] <= 'F')) { //A..F
                        tmpData = ascii_data[i] - 'A' + 10;
                } else if ((ascii_data[i] >= 'a')&&(ascii_data[i] <= 'f')) { //a..f
                        tmpData = ascii_data[i] - 'a' + 10;
                } else {
                        continue;
                }
                ascii_data[i] = tmpData;
        }

        for (tmp_len = 0, j = 0; j < i; j += 2) {
                dst[tmp_len++] = (ascii_data[j] << 4) | ascii_data[j + 1];
        }
        return tmp_len;
}
