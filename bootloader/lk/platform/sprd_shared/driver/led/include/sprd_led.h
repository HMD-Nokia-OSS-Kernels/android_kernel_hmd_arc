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

#ifndef __SPRD_LED_H__
#define __SPRD_LED_H__

//#include <common.h>

#define BRIGHTNESS_MAX (255)

typedef enum {
	Red,
	Green,
	Blue,
} LedType;

void sprd_led_init(void);
void sprd_led_set_brightness(LedType type, uint32_t brightness);
void sprd_led_disable(LedType type);
void sprd_led_pattern_clear(LedType type);
void sprd_led_pattern_set(LedType type, uint32_t brightness);

#endif
