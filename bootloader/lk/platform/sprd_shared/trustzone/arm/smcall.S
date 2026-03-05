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

FUNCTION(arch_smc_call)
        push    {r4-r8, lr}
        mov     r8, r0
        ldm     r8, {r0-r7}
.arch_extension sec
        smc     #0
        stm     r8, {r0-r7}
        pop     {r4-r8, pc}
