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

#include <lk/asm.h>

#define SMC_PARAM_W0_OFFS       0
#define SMC_PARAM_W2_OFFS       8
#define SMC_PARAM_W4_OFFS       16
#define SMC_PARAM_W6_OFFS       24

FUNCTION(arch_smc_call)
    stp     x28, x30, [sp, #-16]!
    mov     x28, x0
    ldp     w0, w1, [x28, #SMC_PARAM_W0_OFFS]
    ldp     w2, w3, [x28, #SMC_PARAM_W2_OFFS]
    ldp     w4, w5, [x28, #SMC_PARAM_W4_OFFS]
    ldp     w6, w7, [x28, #SMC_PARAM_W6_OFFS]
    smc     #0
    stp     w0, w1, [x28, #SMC_PARAM_W0_OFFS]
    stp     w2, w3, [x28, #SMC_PARAM_W2_OFFS]
    ldp     x28, x30, [sp], #16
    ret
