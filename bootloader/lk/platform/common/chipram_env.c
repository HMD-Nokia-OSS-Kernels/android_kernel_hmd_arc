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

#include <chipram_env.h>

chipram_env_t  local_chipram_env;

void setup_chipram_env(void)
{
	chipram_env_t * env = (chipram_env_t *)CHIPRAM_ENV_LOCATION;

	if (CHIPRAM_ENV_MAGIC != env->magic) {
		errorf("Chipram magic wrong ,mode data may be broken\n");
	}

	local_chipram_env = * env;
}

chipram_env_t* get_chipram_env(void)
{
	return &local_chipram_env;
}

boot_mode_t get_boot_role(void)
{
	boot_mode_t boot_role;
	chipram_env_t* cr_env = get_chipram_env();
	boot_role = cr_env->mode;
	return boot_role;
}
