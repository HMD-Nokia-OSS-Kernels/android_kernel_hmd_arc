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

#include <lk/err.h>
#include <lk/debug.h>
#include <platform.h>
#include <lk/reg.h>
#include <kernel/vm.h>
#include <platform/uart.h>
#include <chipram_env.h>
#include <sprd_types.h>

#define UART_BASE (UART1)
#define UARTREG(reg)  (*REG32((UART_BASE) + (reg)))

/* initial memory mappings. parsed by start.S */
#if WITH_KERNEL_VM
struct mmu_initial_mapping mmu_initial_mappings[] = {
	 { .phys = 0,
	   .virt =0,
	   .size = 0x80000000UL,
	   .flags = MMU_INITIAL_MAPPING_FLAG_DEVICE,
	   .name = "Peripheral" },

	 { .phys = PHYS_SDRAM_1,
	   .virt = PHYS_SDRAM_1,
	   .size = (PHYS_SDRAM_1_SIZE + 0x40000000), //only used for >2G
	   .flags = MMU_INITIAL_MAPPING_FLAG_DEVICE,
	   .name = "bank-0" },

#ifdef SPRD_DUAL_DDR
	{ .phys = PHYS_SDRAM_2,
	  .virt = PHYS_SDRAM_2,
	  .size = (PHYS_SDRAM_2_SIZE),
	  .flags = 0,
	  .name = "bank-1" },
#endif

	 /* null entry to terminate the list */
	 { 0 }
};

static pmm_arena_t ram_arena = {
    .name  = "ram",
    .base  =  MEMBASE,
    .size  =  MEMSIZE,
    .flags =  PMM_ARENA_FLAG_KMAP
};
#endif

u64 dram_error_addr = BIST_PASS_KEY;
phys_size_t real_ram_size = 0x40000000;
phys_size_t get_real_ram_size(void)
{
        return real_ram_size;
}

int dram_init(void)
{
#ifdef SPRD_DDR_AUTO_DETECT
	chipram_env_t * env = (chipram_env_t *)CHIPRAM_ENV_LOCATION;

	if (CHIPRAM_ENV_MAGIC != env->magic) {
		return 0;
	}

	real_ram_size = 0;

	if (env->cs_number == 1) {
		real_ram_size += env->cs0_size;
	} else if (env->cs_number == 2) {
		real_ram_size += env->cs0_size;
		real_ram_size += env->cs1_size;
	}

	dram_error_addr = env->bist_fail_addr;
#else
	real_ram_size = REAL_SDRAM_SIZE;
	dram_error_addr = BIST_PASS_KEY;
#endif

	return 0;
}

/*
 * default implementations of these routines, if the platform code
 * chooses not to implement.
 */

__WEAK void platform_init_mmu_mappings(void) {
#if WITH_KERNEL_VM
	pmm_add_arena(&ram_arena);
#else
#ifdef CONFIG_ARM64
	dprintf(CRITICAL, "dram size %llx\n", get_real_ram_size());
#else
	dprintf(CRITICAL, "dram size %lx\n", get_real_ram_size());
#endif
#endif
}

#if WITH_SMP
extern void smp_boot(void);
extern int psci_call(ulong arg0, ulong arg1, ulong arg2, ulong arg3);
#endif
uint32_t lk_start_time;
extern int sprd_timer_init(void);
__WEAK void platform_early_init(void) {
	sprd_timer_init();
	lk_start_time = SCI_GetTickCount();
	dprintf(INFO, "booting %d cpus\n", (uint)SMP_MAX_CPUS);
}

extern void setup_chipram_env(void);
__WEAK void platform_init(void) {
	dprintf(INFO,"myqin-UARTREG(0)=%d\n",UARTREG(0));
	setup_chipram_env();
}

__WEAK void platform_quiesce(void) {
}
