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

#include <i2c.h>
#include <lk/init.h>
#include <stdio.h>

unsigned char data[2] = {
	0xaa, 0x51
};

int test_i2c_init(void)
{
	i2c_send(0, 0x55, data, 1);

	dprintf(INFO,"i2c test ok\n");
	return 0;
}

void tee_i2c_test_fn(int level)
{
	return test_i2c_init();
}

LK_INIT_HOOK(sprd_i2c_test, tee_i2c_test_fn, LK_INIT_LEVEL_TARGET)
