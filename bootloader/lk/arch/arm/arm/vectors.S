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

.globl _start_hyp

/*
 * Vectors have their own section
 */

.section ".text.boot.vectab1"

/*
 * Exception vectors as described in ARM reference manuals
 */
.align 5
_start_hyp:
	ldr     pc, _handle_hyp
	ldr     pc, _handle_hyp
	ldr     pc, _handle_hyp
	ldr     pc, _handle_hyp
	ldr     pc, _handle_hyp
	ldr     pc, _irq_hyp
	ldr     pc, _handle_hyp

/*
 * Indirect vectors table
 */
_handle_hyp:	.word handle_hyp
_irq_hyp:		.word irq_hyp
	.balignl 16,0xdeadbeef

/*
 * Interrupt handling
 */
#define S_FRAME_SIZE	72

handle_hyp:
	b	arm_hyp_exception

irq_hyp:
	sub		sp, sp, #S_FRAME_SIZE
	stmia	sp, {r0 - r12,r14}
	bl		arm_irq
	ldmia	sp, {r0 - r12,r14}
	add		sp, sp, #S_FRAME_SIZE
	eret

