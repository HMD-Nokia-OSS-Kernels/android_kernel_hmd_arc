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

typedef struct smc_param {
        uint32_t a0;
        uint32_t a1;
        uint32_t a2;
        uint32_t a3;
        uint32_t a4;
        uint32_t a5;
        uint32_t a6;
        uint32_t a7;
} smc_param;


smc_param *tee_common_call(uint32_t funcid, uint32_t start_addr, uint32_t lenth);

int tee_smc_sip_call(uint32_t sip_call_id, uint32_t *a1, uint32_t *a2, uint32_t *a3);
int tee_smc_fast_call(uint32_t fast_call_id, uint32_t paddr_l, uint32_t paddr_h, uint32_t size);


