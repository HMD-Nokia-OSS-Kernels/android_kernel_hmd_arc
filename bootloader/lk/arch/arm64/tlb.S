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

#include <arch/arm64/sprd_macro.h>
#include <lk/asm.h>

/*
 * void __arm64_invalidate_tlb_all(void)
 *
 * invalidate all tlb entries.
 */
FUNCTION(__arm64_invalidate_tlb_all)
	switch_el x9, 3f, 2f, 1f
3:	tlbi	alle3
	dsb	sy
	isb
	b	0f
2:	tlbi	alle2
	dsb	sy
	isb
	b	0f
1:	tlbi	vmalle1
	dsb	sy
	isb
0:
	ret
