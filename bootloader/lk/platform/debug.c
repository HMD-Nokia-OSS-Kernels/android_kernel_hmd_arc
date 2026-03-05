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

#include <lk/compiler.h>
#include <lk/debug.h>
#include <lk/trace.h>
#include <serial_sprd.h>

void platform_dputc(char c)
{
	sprd_serial_putc(c);
}

int platform_dgetc(char *c, bool wait)
{
	if (!wait) {
		if (!sprd_rxfifo_cnt()) {
			return -1;
		}
		*c = sprd_serial_getc();
		return 0;
	} else {
		*c = sprd_serial_getc();
		return 0;
	}
}

/* Default implementation of panic time getc/putc.
 * Just calls through to the underlying dputc/dgetc implementation
 * unless the platform overrides it.
 */
__WEAK void platform_pputc(char c) {
    sprd_serial_putc(c);
}

__WEAK int platform_pgetc(char *c, bool wait) {
    return platform_dgetc(c, wait);
}
