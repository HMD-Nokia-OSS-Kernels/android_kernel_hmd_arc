/*
 * Copyright (c) 2008-2014 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#include <lk/debug.h>
#include <lk/bits.h>
#include <arch/arm.h>
#include <kernel/thread.h>
#include <platform.h>
#include <sprd_log.h>
#include <asm/system.h>

struct fault_handler_table_entry {
    uint32_t pc;
    uint32_t fault_handler;
};

extern struct fault_handler_table_entry __fault_handler_table_start[];
extern struct fault_handler_table_entry __fault_handler_table_end[];

static int is_valid_addr(uint32_t* addr)
{
#ifdef WITH_KERNEL_VM
   // ToDo: check more with mmu_initial_mappings
   return (addr != NULL);
#else
   return ((addr >= (uint32_t *)MEMBASE) && (addr <(uint32_t *)(MEMBASE+MEMSIZE)));
#endif
}

/*
 * dump backtrace since given frame
 */
char backtrace[512] = {0};
volatile int dump_backtrace_once = 0;
void dump_backtrace(struct arm_fault_frame *iframe)
{
    uint32_t pc = iframe->ulr;
    uint32_t* fp = (uint32_t *) iframe->r[11];

    if (dump_backtrace_once) return;

    dump_backtrace_once = 1;
    dprintf(INFO,"backtrace:\n");
    /*
     * if calling from panic() directly, the elr is 0
     * we should show the right PC
     */
    if (pc == 0) {
        pc = iframe->lr - 4;
    }
    dprintf(INFO,"\t#00 pc %pF\n", (void *)pc);
#if 0
    for (int i = 1; i<20; i++) {
        if (!is_valid_addr(fp)) break;
        pc = *(fp+1);
        if (pc == 0) break;
        dprintf(INFO,"\t#%02d pc %pF\n", i, (void *)(pc-4));
        fp = (uint32_t *)*fp;
    }
#endif
    dprintf(INFO,"\n");
}

static void show_data(uint32_t addr, int nbytes, const char *name)
{
    int i, j;
    int nlines;
    u32 *p;
    unsigned long end;

    /*
    * makesure addr is valid address for LK.
    */
    if ((addr < MEMBASE) || (addr > (MEMBASE+MEMSIZE)))
        return;

    end = addr + nbytes - 1;
    if (end > (MEMBASE+MEMSIZE)) {
        return;
    }

    dprintf(INFO,"\n%s: %#x:\n", name, addr);

    /*
    * round address down to a 32 bit.
    */
    p = (u32 *)(addr & ~(sizeof(u32) - 1));
    nbytes += (addr & (sizeof(u32) - 1));
    nlines = (nbytes + 31) / 32;

    for (i = 0; i < nlines; i++) {
        dprintf(INFO,"%04lx ", (unsigned long)p & 0xffff);
        for (j = 0; j < 8; j++) {
            dprintf(INFO," %08x", *p);
            ++p;
        }
        dprintf(INFO,"\n");
    }
}

static void show_extra_register_data(const struct arm_fault_frame *iframe, int nbytes)
{
    unsigned int i;

    show_data(iframe->lr - nbytes, nbytes * 2, "LR");
    show_data(iframe->usp - nbytes, nbytes * 2, "SP");
    for (i = 0; i < 12; i++) {
        char name[4];
        snprintf(name, sizeof(name), "X%u", i);
        show_data(iframe->r[i] - nbytes, nbytes * 2, name);
    }
}

static void dump_mode_regs(uint32_t spsr, uint32_t svc_r13, uint32_t svc_r14) {
    struct arm_mode_regs regs;
    arm_save_mode_regs(&regs);

    dprintf(INFO, "%c%s r13 0x%08x r14 0x%08x\n", ((spsr & CPSR_MODE_MASK) == CPSR_MODE_USR) ? '*' : ' ', "usr", regs.usr_r13, regs.usr_r14);
    dprintf(INFO, "%c%s r13 0x%08x r14 0x%08x\n", ((spsr & CPSR_MODE_MASK) == CPSR_MODE_FIQ) ? '*' : ' ', "fiq", regs.fiq_r13, regs.fiq_r14);
    dprintf(INFO, "%c%s r13 0x%08x r14 0x%08x\n", ((spsr & CPSR_MODE_MASK) == CPSR_MODE_IRQ) ? '*' : ' ', "irq", regs.irq_r13, regs.irq_r14);
    dprintf(INFO, "%c%s r13 0x%08x r14 0x%08x\n", 'a', "svc", regs.svc_r13, regs.svc_r14);
    dprintf(INFO, "%c%s r13 0x%08x r14 0x%08x\n", ((spsr & CPSR_MODE_MASK) == CPSR_MODE_SVC) ? '*' : ' ', "svc", svc_r13, svc_r14);
    dprintf(INFO, "%c%s r13 0x%08x r14 0x%08x\n", ((spsr & CPSR_MODE_MASK) == CPSR_MODE_UND) ? '*' : ' ', "und", regs.und_r13, regs.und_r14);
    dprintf(INFO, "%c%s r13 0x%08x r14 0x%08x\n", ((spsr & CPSR_MODE_MASK) == CPSR_MODE_SYS) ? '*' : ' ', "sys", regs.sys_r13, regs.sys_r14);

    // dump the bottom of the current stack
    addr_t stack;
    switch (spsr & CPSR_MODE_MASK) {
        case CPSR_MODE_FIQ:
            stack = regs.fiq_r13;
            break;
        case CPSR_MODE_IRQ:
            stack = regs.irq_r13;
            break;
        case CPSR_MODE_SVC:
            stack = svc_r13;
            break;
        case CPSR_MODE_UND:
            stack = regs.und_r13;
            break;
        case CPSR_MODE_SYS:
            stack = regs.sys_r13;
            break;
        default:
            stack = 0;
    }

    if (stack != 0) {
        dprintf(INFO, "bottom of stack at 0x%08x:\n", (unsigned int)stack);
        hexdump((void *)stack, 128);
    }
}

static void dump_fault_frame(struct arm_fault_frame *frame) {
    struct thread *current_thread = get_current_thread();

    dprintf(INFO, "current_thread %p, name %s\n",
            current_thread, current_thread ? current_thread->name : "");

    dprintf(INFO, "r0  0x%08x r1  0x%08x r2  0x%08x r3  0x%08x\n", frame->r[0], frame->r[1], frame->r[2], frame->r[3]);
    dprintf(INFO, "r4  0x%08x r5  0x%08x r6  0x%08x r7  0x%08x\n", frame->r[4], frame->r[5], frame->r[6], frame->r[7]);
    dprintf(INFO, "r8  0x%08x r9  0x%08x r10 0x%08x r11 0x%08x\n", frame->r[8], frame->r[9], frame->r[10], frame->r[11]);
    dprintf(INFO, "r12 0x%08x usp 0x%08x ulr 0x%08x pc  0x%08x\n", frame->r[12], frame->usp, frame->ulr, frame->pc);
    dprintf(INFO, "spsr 0x%08x\n", frame->spsr);

    dump_mode_regs(frame->spsr, (uintptr_t)(frame + 1), frame->lr);
    show_extra_register_data(frame, 128);
}

void dump_iframe(struct arm_iframe *frame) {
    dprintf(INFO, "r0  0x%08x r1  0x%08x r2  0x%08x r3  0x%08x\n", frame->r0, frame->r1, frame->r2, frame->r3);
    dprintf(INFO, "r12 0x%08x usp 0x%08x ulr 0x%08x pc  0x%08x\n", frame->r12, frame->usp, frame->ulr, frame->pc);
    dprintf(INFO, "spsr 0x%08x\n", frame->spsr);

    dump_mode_regs(frame->spsr, (uintptr_t)(frame + 1), frame->lr);
}

void dump_backtrace_hyp(struct arm_hyp_fault_frame *iframe)
{
    uint32_t pc = iframe->h_elr;
    uint32_t* fp = (uint32_t *) iframe->r[11];

    if (dump_backtrace_once) return;

    dump_backtrace_once = 1;
    dprintf(INFO,"backtrace:\n");
    /*
     * if calling from panic() directly, the elr is 0
     * we should show the right PC
     */
    if ((pc < MEMBASE) || (pc > (MEMBASE+MEMSIZE+0x10000))) {
        pc = iframe->lr - 4;
    }
    dprintf(INFO,"\t#00 pc %pF\n", (void *)pc);
#if 0
    for (int i = 1; i<20; i++) {
        if (!is_valid_addr(fp)) break;
        pc = *(fp+1);
        if (pc == 0) break;
        dprintf(INFO,"\t#%02d pc %pF\n", i, (void *)(pc-4));
        fp = (uint32_t *)*fp;
    }
#endif
    dprintf(INFO,"\n");
}

static void show_extra_register_data_hyp(const struct arm_hyp_fault_frame *iframe, int nbytes)
{
    unsigned int i;

    show_data(iframe->lr - nbytes, nbytes * 2, "LR");
    show_data(iframe->sp - nbytes, nbytes * 2, "SP");
    for (i = 0; i < 12; i++) {
        char name[4];
        snprintf(name, sizeof(name), "R%u", i);
        show_data(iframe->r[i] - nbytes, nbytes * 2, name);
    }
}

static void dump_fault_frame_hyp(struct arm_hyp_fault_frame *frame) {
    struct thread *current_thread = get_current_thread();

    dprintf(INFO, "current_thread %p, name %s\n",
            current_thread, current_thread ? current_thread->name : "");

    dprintf(INFO, "r0  0x%08x r1  0x%08x r2  0x%08x r3  0x%08x\n", frame->r[0], frame->r[1], frame->r[2], frame->r[3]);
    dprintf(INFO, "r4  0x%08x r5  0x%08x r6  0x%08x r7  0x%08x\n", frame->r[4], frame->r[5], frame->r[6], frame->r[7]);
    dprintf(INFO, "r8  0x%08x r9  0x%08x r10 0x%08x r11 0x%08x\n", frame->r[8], frame->r[9], frame->r[10], frame->r[11]);
    dprintf(INFO, "r12 0x%08x sp 0x%08x lr 0x%08x pc  0x%08x\n", frame->r[12], frame->sp, frame->lr, frame->pc);
    dprintf(INFO, "spsr 0x%08x\n", frame->spsr);
    dprintf(INFO, "elr_hyp 0x%08x\n", frame->h_elr);

    show_extra_register_data_hyp(frame, 128);
}

void dump_iframe_hyp(struct arm_hyp_fault_frame *frame) {
    dump_fault_frame_hyp(frame);
    dump_backtrace_hyp(frame);
}

static void exception_die_hyp(struct arm_hyp_fault_frame *frame, const char *msg) {
    dprintf(INFO, msg);
    dump_iframe_hyp(frame);

    sprdlog_panic_finish();
    platform_halt(HALT_ACTION_HALT, HALT_REASON_SW_PANIC);
    for (;;);
}

static void exception_die(struct arm_fault_frame *frame, const char *msg) {
    dprintf(INFO, msg);
    dump_fault_frame(frame);
    dump_backtrace(frame);

    sprdlog_panic_finish();
    platform_halt(HALT_ACTION_HALT, HALT_REASON_SW_PANIC);
    for (;;);
}

static void exception_die_iframe(struct arm_iframe *frame, const char *msg) {
    dprintf(INFO, msg);
    dump_iframe(frame);

    sprdlog_panic_finish();
    platform_halt(HALT_ACTION_HALT, HALT_REASON_SW_PANIC);
    for (;;);
}

__WEAK void arm_syscall_handler(struct arm_fault_frame *frame) {
    exception_die(frame, "unhandled syscall, halting\n");
}

void arm_undefined_handler(struct arm_iframe *frame) {
    /* look at the undefined instruction, figure out if it's something we can handle */
    bool in_thumb = frame->spsr & (1<<5);
    if (in_thumb) {
        frame->pc -= 2;
    } else {
        frame->pc -= 4;
    }

    __UNUSED uint32_t opcode = *(uint32_t *)frame->pc;
    //dprintf(INFO, "undefined opcode 0x%x\n", opcode);

#if ARM_WITH_VFP
    if (in_thumb) {
        /* look for a 32bit thumb instruction */
        if (opcode & 0x0000e800) {
            /* swap the 16bit words */
            opcode = (opcode >> 16) | (opcode << 16);
        }

        if (((opcode & 0xec000e00) == 0xec000a00) || // vfp
                ((opcode & 0xef000000) == 0xef000000) || // advanced simd data processing
                ((opcode & 0xff100000) == 0xf9000000)) { // VLD

            //dprintf(INFO, "vfp/neon thumb instruction 0x%08x at 0x%x\n", opcode, frame->pc);
            goto fpu;
        }
    } else {
        /* look for arm vfp/neon coprocessor instructions */
        if (((opcode & 0x0c000e00) == 0x0c000a00) || // vfp
                ((opcode & 0xfe000000) == 0xf2000000) || // advanced simd data processing
                ((opcode & 0xff100000) == 0xf4000000)) { // VLD
            //dprintf(INFO, "vfp/neon arm instruction 0x%08x at 0x%x\n", opcode, frame->pc);
            goto fpu;
        }
    }
#endif

    exception_die_iframe(frame, "undefined abort, halting\n");
    return;

#if ARM_WITH_VFP
fpu:
    arm_fpu_undefined_instruction(frame);
#endif
}

void arm_data_abort_handler(struct arm_fault_frame *frame) {
    struct fault_handler_table_entry *fault_handler;
    uint32_t fsr = arm_read_dfsr();
    uint32_t far = arm_read_dfar();

    uint32_t fault_status = (BIT(fsr, 10) ? (1<<4) : 0) |  BITS(fsr, 3, 0);

    for (fault_handler = __fault_handler_table_start; fault_handler < __fault_handler_table_end; fault_handler++) {
        if (fault_handler->pc == frame->pc) {
            frame->pc = fault_handler->fault_handler;
            return;
        }
    }

    dprintf(INFO, "\n\ncpu %u data abort, ", arch_curr_cpu_num());
    bool write = !!BIT(fsr, 11);

    /* decode the fault status (from table B3-23) */
    switch (fault_status) {
        case 0b00001: // alignment fault
            dprintf(INFO, "alignment fault on %s\n", write ? "write" : "read");
            break;
        case 0b00101:
        case 0b00111: // translation fault
            dprintf(INFO, "translation fault on %s\n", write ? "write" : "read");
            break;
        case 0b00011:
        case 0b00110: // access flag fault
            dprintf(INFO, "access flag fault on %s\n", write ? "write" : "read");
            break;
        case 0b01001:
        case 0b01011: // domain fault
            dprintf(INFO, "domain fault, domain %lu\n", BITS_SHIFT(fsr, 7, 4));
            break;
        case 0b01101:
        case 0b01111: // permission fault
            dprintf(INFO, "permission fault on %s\n", write ? "write" : "read");
            break;
        case 0b00010: // debug event
            dprintf(INFO, "debug event\n");
            break;
        case 0b01000: // synchronous external abort
            dprintf(INFO, "synchronous external abort on %s\n", write ? "write" : "read");
            break;
        case 0b10110: // asynchronous external abort
            dprintf(INFO, "asynchronous external abort on %s\n", write ? "write" : "read");
            break;
        case 0b10000: // TLB conflict event
        case 0b11001: // synchronous parity error on memory access
        case 0b00100: // fault on instruction cache maintenance
        case 0b01100: // synchronous external abort on translation table walk
        case 0b01110: //    "
        case 0b11100: // synchronous parity error on translation table walk
        case 0b11110: //    "
        case 0b11000: // asynchronous parity error on memory access
        default:
            dprintf(INFO, "unhandled fault\n");
            ;
    }

    dprintf(INFO, "DFAR 0x%x (fault address)\n", far);
    dprintf(INFO, "DFSR 0x%x (fault status register)\n", fsr);

    exception_die(frame, "halting\n");
}

void arm_prefetch_abort_handler(struct arm_fault_frame *frame) {
    uint32_t fsr = arm_read_ifsr();
    uint32_t far = arm_read_ifar();

    uint32_t fault_status = (BIT(fsr, 10) ? (1<<4) : 0) |  BITS(fsr, 3, 0);

    dprintf(INFO, "\n\ncpu %u prefetch abort, ", arch_curr_cpu_num());

    /* decode the fault status (from table B3-23) */
    switch (fault_status) {
        case 0b00001: // alignment fault
            dprintf(INFO, "alignment fault\n");
            break;
        case 0b00101:
        case 0b00111: // translation fault
            dprintf(INFO, "translation fault\n");
            break;
        case 0b00011:
        case 0b00110: // access flag fault
            dprintf(INFO, "access flag fault\n");
            break;
        case 0b01001:
        case 0b01011: // domain fault
            dprintf(INFO, "domain fault, domain %lu\n", BITS_SHIFT(fsr, 7, 4));
            break;
        case 0b01101:
        case 0b01111: // permission fault
            dprintf(INFO, "permission fault\n");
            break;
        case 0b00010: // debug event
            dprintf(INFO, "debug event\n");
            break;
        case 0b01000: // synchronous external abort
            dprintf(INFO, "synchronous external abort\n");
            break;
        case 0b10110: // asynchronous external abort
            dprintf(INFO, "asynchronous external abort\n");
            break;
        case 0b10000: // TLB conflict event
        case 0b11001: // synchronous parity error on memory access
        case 0b00100: // fault on instruction cache maintenance
        case 0b01100: // synchronous external abort on translation table walk
        case 0b01110: //    "
        case 0b11100: // synchronous parity error on translation table walk
        case 0b11110: //    "
        case 0b11000: // asynchronous parity error on memory access
        default:
            dprintf(INFO, "unhandled fault\n");
            ;
    }

    dprintf(INFO, "IFAR 0x%x (fault address)\n", far);
    dprintf(INFO, "IFSR 0x%x (fault status register)\n", fsr);

    exception_die(frame, "halting\n");
}

void arm_hyp_regsave_sysreg(struct arm_hyp_fault_frame *iframe)
{
    uint32_t spsr = ARM_READ_SYSREG(spsr);
    uint32_t h_elr = ARM_READ_SYSREG(elr_hyp);

    iframe->spsr = spsr;
    iframe->h_elr= h_elr;

}

void arm_hyp_excetion_handler(struct arm_hyp_fault_frame *frame)
{
    uint32_t hsr = arm_read_hsr();
    uint32_t hdfar = arm_read_hdfar();
    uint32_t hifar = arm_read_hifar();
    uint32_t h_ec, h_il, h_iss;

    h_ec = BITS_SHIFT(hsr, 31, 26);
    h_il = BIT(hsr, 25);
    h_iss = BITS(hsr, 24, 0);

    switch (h_ec) {
        case 0b100000: // prefetch abort
        case 0b100001: // prefetch abort
            dprintf(INFO, "prefetch abort in hpy mode!\n");
            dprintf(INFO, "HIFAR 0x%x (fault address)\n", hifar);
            break;
        case 0b100010: // alignment  fault
            dprintf(INFO, "PC alignment exception in hpy mode!\n");
            break;
        case 0b100100: // data abort
        case 0b100101: // data abort
            dprintf(INFO, "data abort in hpy mode!\n");
            dprintf(INFO, "HDFAR 0x%x (fault address)\n", hdfar);
            break;
        default:
            dprintf(INFO, "unhandled fault\n");
    }

    dprintf(INFO, "HSR (fault status register) 0x%x: h_ec 0x%x, h_il 0x%x, h_iss 0x%x\n", hsr, h_ec, h_il, h_iss);
    exception_die_hyp(frame, "HYP mode exception die, rebooting \n");
}

