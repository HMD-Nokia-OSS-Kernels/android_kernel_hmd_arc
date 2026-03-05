/*
 * Copyright (c) 2013-2015 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

/*
 * Main entry point to the OS. Initializes modules in order and creates
 * the default thread.
 */
#include <lk/main.h>

#include <app.h>
#include <arch.h>
#include <kernel/mutex.h>
#include <kernel/novm.h>
#include <kernel/thread.h>
#include <lib/heap.h>
#include <lk/compiler.h>
#include <lk/debug.h>
#include <lk/init.h>
#include <platform.h>
#include <string.h>
#include <target.h>
#include <sprd_log.h>
#include <fdtdec.h>

/* saved boot arguments from whoever loaded the system */
ulong lk_boot_args[4];

// Declare __ctor_list and __ctor_end as pointers to array, see the comments in
// call_constructors for details.
extern void *__ctor_list[];
extern void *__ctor_end[];
extern int __bss_start;
extern int _end;

#if WITH_SMP
static thread_t *secondary_bootstrap_threads[SMP_MAX_CPUS - 1];
static uint secondary_bootstrap_thread_count;
#endif
volatile u32 cpu_num = 1;

static int bootstrap2(void *arg);

extern void kernel_init(void);

static void call_constructors(void) {
    void **ctor;

    // Do not declare __ctor_list and __ctor_end as just pointer and use "&__ctor_list" here,
    // because clang will consider the address of two global variables will never be the same,
    // and the result of '&' operator of the two operands will never be equal. In that case,
    // clang will canonicalize the while loop into a do-while form without a "if" loop guard,
    // causing instruction abort when ctor list is empty.
    ctor = __ctor_list;
    while (ctor != __ctor_end) {
        void (*func)(void);

        func = (void ( *)(void))*ctor;

        func();
        ctor++;
    }
}

/* called from arch code */
void lk_main(ulong arg0, ulong arg1, ulong arg2, ulong arg3) {
    /* check fdt header, We must have an FDT, but we cannot panic() yet since the console
       * is not ready. So for now, just assert().
    */
#ifdef CONFIG_DTS_MEM_LAYOUT
    fdtdec_check_fdt();
#endif
    /*
     * init sprdlog buffer first, to save more initialization logs
     */
    init_sprdlog(INIT_LOG_BUFFER);
    FTL_Panic_Flag_Write(1, LOG_RESERVED_ADDR);
    FTL_Savepoint_Private(PHASE_LK_ENTER);

    dprintf(CRITICAL, "\n");
    dprintf(CRITICAL, " **  LK (%d, restored from storage)  **\n", p_log_buffer->record_count);

    // save the boot args
    lk_boot_args[0] = arg0;
    lk_boot_args[1] = arg1;
    lk_boot_args[2] = arg2;
    lk_boot_args[3] = arg3;

    // get us into some sort of thread context
    thread_init_early();

    // early arch stuff
    FTL_Savepoint_Private(PHASE_ARCH_EARLY_INIT);
    lk_primary_cpu_init_level(LK_INIT_LEVEL_EARLIEST, LK_INIT_LEVEL_ARCH_EARLY - 1);
    arch_early_init();

    // do any super early platform initialization
    FTL_Savepoint_Private(PHASE_PLATFORM_EARLY_INIT);
    lk_primary_cpu_init_level(LK_INIT_LEVEL_ARCH_EARLY, LK_INIT_LEVEL_PLATFORM_EARLY - 1);
    platform_early_init();

    // do any super early target initialization
    lk_primary_cpu_init_level(LK_INIT_LEVEL_PLATFORM_EARLY, LK_INIT_LEVEL_TARGET_EARLY - 1);
    target_early_init();

    dprintf(ALWAYS, "\n");
#if WITH_SMP
    dprintf(CRITICAL, "welcome to lk/MP\n\n");
#else
    dprintf(CRITICAL, "welcome to lk\n\n");
#endif
    dprintf(ALWAYS, "boot args 0x%lx 0x%lx 0x%lx 0x%lx\n",
            lk_boot_args[0], lk_boot_args[1], lk_boot_args[2], lk_boot_args[3]);
    show_current_time();

    // bring up the kernel heap
    FTL_Savepoint_Private(PHASE_HEAP_INIT);
    lk_primary_cpu_init_level(LK_INIT_LEVEL_TARGET_EARLY, LK_INIT_LEVEL_HEAP - 1);
    dprintf(ALWAYS, "initializing heap\n");
    heap_init();

    // deal with any static constructors
    dprintf(ALWAYS, "calling constructors\n");
    call_constructors();

    // initialize the kernel
    FTL_Savepoint_Private(PHASE_KERNEL_INIT);
    lk_primary_cpu_init_level(LK_INIT_LEVEL_HEAP, LK_INIT_LEVEL_KERNEL - 1);
    kernel_init();

    lk_primary_cpu_init_level(LK_INIT_LEVEL_KERNEL, LK_INIT_LEVEL_THREADING - 1);

    // create a thread to complete system initialization
    dprintf(ALWAYS, "creating bootstrap completion thread\n");
    thread_t *t = thread_create("bootstrap2", &bootstrap2, NULL, DEFAULT_PRIORITY, 0x4000);
    thread_set_pinned_cpu(t, 0);
    thread_detach(t);
    thread_resume(t);

    // become the idle thread and enable interrupts to start the scheduler
    thread_become_idle();
}

static int bootstrap2(void *arg) {
    FTL_Savepoint_Private(PHASE_BOOTSTRAP2_ENTER);
    dprintf(SPEW, "top of bootstrap2()\n");
    lk_primary_cpu_init_level(LK_INIT_LEVEL_THREADING, LK_INIT_LEVEL_ARCH - 1);
    arch_init();

    // initialize the rest of the platform
    dprintf(SPEW, "initializing platform\n");
    lk_primary_cpu_init_level(LK_INIT_LEVEL_ARCH, LK_INIT_LEVEL_PLATFORM - 1);
    platform_init();

    // initialize the target
    FTL_Savepoint_Private(PHASE_TARGET_INIT);
    dprintf(SPEW, "initializing target\n");
    lk_primary_cpu_init_level(LK_INIT_LEVEL_PLATFORM, LK_INIT_LEVEL_TARGET - 1);
    target_init();

    // init sprdlog : init_write_log after MMC Initialization
    init_sprdlog(INIT_LOG_WRITER);
    FTL_Savepoint_Private(PHASE_LOG_PARTITION_DONE);

    dprintf(SPEW, "initializing apps\n");
    lk_primary_cpu_init_level(LK_INIT_LEVEL_TARGET, LK_INIT_LEVEL_APPS - 1);
    apps_init();

    lk_primary_cpu_init_level(LK_INIT_LEVEL_APPS, LK_INIT_LEVEL_LAST);

    return 0;
}

#if WITH_SMP
void lk_secondary_cpu_entry(void) {
    uint cpu = arch_curr_cpu_num();

    if (cpu > secondary_bootstrap_thread_count) {
        dprintf(ALWAYS, "Invalid secondary cpu num %d, SMP_MAX_CPUS %d, secondary_bootstrap_thread_count %d\n",
                cpu, SMP_MAX_CPUS, secondary_bootstrap_thread_count);
        return;
    }

    thread_secondary_cpu_init_early();
    thread_resume(secondary_bootstrap_threads[cpu - 1]);

    dprintf(SPEW, "entering scheduler on cpu %d\n", cpu);
    thread_secondary_cpu_entry();
}

extern thread_t *sprd_threads[5];
extern spin_lock_t thread_created_lock;
extern int psci_call(ulong arg0, ulong arg1, ulong arg2, ulong arg3);
static int secondary_cpu_bootstrap2(void *arg) {
    int cpu = arch_curr_cpu_num();
    /* secondary cpu initialize from threading level up. 0 to threading was handled in arch */
    lk_init_level(LK_INIT_FLAG_SECONDARY_CPUS, LK_INIT_LEVEL_THREADING, LK_INIT_LEVEL_LAST);

    spin_lock(&thread_created_lock);
    spin_unlock(&thread_created_lock);

    thread_resume(sprd_threads[cpu]);
    dprintf(INFO, "call the smc to power off the core%d\n", cpu);
    atomic_add(&cpu_num, -1);
    psci_call(PSCI_CPU_OFF, 0, 0, 0);

    return 0;
}

void lk_init_secondary_cpus(uint secondary_cpu_count) {
    if (secondary_cpu_count >= SMP_MAX_CPUS) {
        dprintf(ALWAYS, "Invalid secondary_cpu_count %d, SMP_MAX_CPUS %d\n",
                secondary_cpu_count, SMP_MAX_CPUS);
        secondary_cpu_count = SMP_MAX_CPUS - 1;
    }
    for (uint i = 0; i < secondary_cpu_count; i++) {
        dprintf(SPEW, "creating bootstrap completion thread for cpu %d\n", i + 1);
        thread_t *t = thread_create("secondarybootstrap2",
                                    &secondary_cpu_bootstrap2, NULL,
                                    DEFAULT_PRIORITY, DEFAULT_STACK_SIZE);
        t->pinned_cpu = i + 1;
        thread_detach(t);
        secondary_bootstrap_threads[i] = t;
    }
    secondary_bootstrap_thread_count = secondary_cpu_count;
}
#endif
