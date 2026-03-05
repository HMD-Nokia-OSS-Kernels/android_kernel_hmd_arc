/*
 * Copyright (c) 2014 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#include <stdio.h>
#include <lk/debug.h>
#include <lk/bits.h>
#include <arch/arch_ops.h>
#include <arch/arm64.h>
#include <string.h>

#define SHUTDOWN_ON_FATAL 1

struct fault_handler_table_entry {
    uint64_t pc;
    uint64_t fault_handler;
};

struct fault_status_map {
    uint32_t fsc;
    const char *fault_msg;
};

/* Instruction and Data abort share the fault status encoding */
static const struct fault_status_map fsc_map[] = {
    {
        .fsc = 0b000000,
        .fault_msg = "Address size fault, level 0 of translation or translation table base register"
    },
    {
        .fsc = 0b000001,
        .fault_msg = "Address size fault, level 1"
    },
    {
        .fsc = 0b000010,
        .fault_msg = "Address size fault, level 2"
    },
    {
        .fsc = 0b000011,
        .fault_msg = "Address size fault, level 3"
    },
    {
        .fsc = 0b000100,
        .fault_msg = "Translation fault, level 0"
    },
    {
        .fsc = 0b000101,
        .fault_msg = "Translation fault, level 1"
    },
    {
        .fsc = 0b000110,
        .fault_msg = "Translation fault, level 2"
    },
    {
        .fsc = 0b000111,
        .fault_msg = "Translation fault, level 3"
    },
    {
        .fsc = 0b001001,
        .fault_msg = "Access flag fault, level 1"
    },
    {
        .fsc = 0b001010,
        .fault_msg = "Access flag fault, level 2"
    },
    {
        .fsc = 0b001011,
        .fault_msg = "Access flag fault, level 3"
    },
    {
        .fsc = 0b001101,
        .fault_msg = "Permission fault, level 1"
    },
    {
        .fsc = 0b001110,
        .fault_msg = "Permission fault, level 2"
    },
    {
        .fsc = 0b001111,
        .fault_msg = "Permission fault, level 3"
    },
    {
        .fsc = 0b010000,
        .fault_msg = "Synchronous External abort, not on translation table walk"
    },
    {
        .fsc = 0b010001,
        .fault_msg = "Synchronous Tag Check fail"
    },
    {
        .fsc = 0b010100,
        .fault_msg = "Synchronous External abort, on translation table walk, level 0"
    },
    {
        .fsc = 0b010101,
        .fault_msg = "Synchronous External abort, on translation table walk, level 1"
    },
    {
        .fsc = 0b010110,
        .fault_msg = "Synchronous External abort, on translation table walk, level 2"
    },
    {
        .fsc = 0b010111,
        .fault_msg = "Synchronous External abort, on translation table walk, level 3"
    },
    {
        .fsc = 0b100001,
        .fault_msg = "Alignment fault"
    },
    {
        .fsc = 0b110000,
        .fault_msg = "TLB conflict abort"
    },
    {
        .fsc = 0b111101,
        .fault_msg = "Section Domain Fault, used only for faults reported in the PAR_EL1"
    },
    {
        .fsc = 0b111110,
        .fault_msg = "Page Domain Fault, used only for faults reported in the PAR_EL1"
    },
};

static void print_fault_msg(uint32_t fsc)
{
    uint32_t i;

    for (i = 0; i < countof(fsc_map); i++) {
        if (fsc_map[i].fsc == fsc) {
            dprintf(INFO,"%s\n", fsc_map[i].fault_msg);
            break;
        }
    }
}

extern struct fault_handler_table_entry __fault_handler_table_start[];
extern struct fault_handler_table_entry __fault_handler_table_end[];

static int is_valid_addr(uint64_t* addr)
{
#ifdef WITH_KERNEL_VM
   // ToDo: check more with mmu_initial_mappings
   return (addr != NULL);
#else
   return ((addr >= (uint64_t *)MEMBASE) && (addr <(uint64_t *)(MEMBASE+MEMSIZE)));
#endif
}

static void show_data(uint64_t addr, int nbytes, const char *name)
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

    dprintf(INFO,"\n%s: %#llx:\n", name, addr);

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

static void show_extra_register_data(const struct arm64_iframe_long *iframe, int nbytes)
{
    unsigned int i;

    show_data(iframe->lr - nbytes, nbytes * 2, "LR");
    show_data(iframe->usp - nbytes, nbytes * 2, "SP");
    for (i = 0; i < 30; i++) {
        char name[4];
        snprintf(name, sizeof(name), "X%u", i);
        show_data(iframe->r[i] - nbytes, nbytes * 2, name);
    }
}

void dump_iframe(const struct arm64_iframe_long *iframe)
{
    static int dump_iframe_once = 0;
    if (dump_iframe_once) return;
    dump_iframe_once = 1;
    dprintf(INFO,"\n");
    dprintf(INFO,"iframe %p:\n", iframe);
    dprintf(INFO," x0  0x%16llx x1  0x%16llx x2  0x%16llx x3  0x%16llx\n", iframe->r[0], iframe->r[1], iframe->r[2], iframe->r[3]);
    dprintf(INFO," x4  0x%16llx x5  0x%16llx x6  0x%16llx x7  0x%16llx\n", iframe->r[4], iframe->r[5], iframe->r[6], iframe->r[7]);
    dprintf(INFO," x8  0x%16llx x9  0x%16llx x10 0x%16llx x11 0x%16llx\n", iframe->r[8], iframe->r[9], iframe->r[10], iframe->r[11]);
    dprintf(INFO," x12 0x%16llx x13 0x%16llx x14 0x%16llx x15 0x%16llx\n", iframe->r[12], iframe->r[13], iframe->r[14], iframe->r[15]);
    dprintf(INFO," x16 0x%16llx x17 0x%16llx x18 0x%16llx x19 0x%16llx\n", iframe->r[16], iframe->r[17], iframe->r[18], iframe->r[19]);
    dprintf(INFO," x20 0x%16llx x21 0x%16llx x22 0x%16llx x23 0x%16llx\n", iframe->r[20], iframe->r[21], iframe->r[22], iframe->r[23]);
    dprintf(INFO," x24 0x%16llx x25 0x%16llx x26 0x%16llx x27 0x%16llx\n", iframe->r[24], iframe->r[25], iframe->r[26], iframe->r[27]);
    dprintf(INFO," x28 0x%16llx x29 0x%16llx lr  0x%16llx usp 0x%16llx\n", iframe->r[28], iframe->r[29], iframe->lr, iframe->usp);
    dprintf(INFO," elr 0x%16llx\n", iframe->elr);
    dprintf(INFO,"spsr 0x%16llx\n\n", iframe->spsr);
    arch_stacktrace(iframe->r[29], iframe->elr);

    show_extra_register_data(iframe, 128);
}

/*
 * dump backtrace since given frame
 */
char backtrace[512] = {0};
volatile int dump_backtrace_once = 0;
extern volatile int always_printf_flag[SMP_MAX_CPUS];
void dump_backtrace(struct arm64_iframe_long *iframe)
{
    uint64_t pc = iframe->elr;
    uint64_t* fp = (uint64_t *) iframe->r[29];
    uint8_t *backtrace_p = backtrace;
    char backtrace_temp[32];

    if (dump_backtrace_once) return;

    dump_backtrace_once = 1;
    sprintf(backtrace_temp, "%s\n", "backtrace:");
    dprintf(INFO,"backtrace:\n");
    strcat(backtrace_p, backtrace_temp);
    /*
     * if calling from panic() directly, the elr is 0
     * we should show the right PC
     */
    if (pc == 0) {
        pc = iframe->lr - 4;
    }
    sprintf(backtrace_temp, "\t#00 pc %pF\n", (void *)pc);
    strcat(backtrace_p, backtrace_temp);
    dprintf(INFO,"\t#00 pc %pF\n", (void *)pc);

    for (int i = 1; i<20; i++) {
        if (!is_valid_addr(fp)) break;
        pc = *(fp+1);
        if (pc == 0) break;
        sprintf(backtrace_temp, "\t#%02d pc %pF\n", i, (void *)(pc-4));
        strcat(backtrace_p, backtrace_temp);
        dprintf(INFO,"\t#%02d pc %pF\n", i, (void *)(pc-4));
        fp = (uint64_t *)*fp;
    }
    dprintf(INFO,"\n");
}

__WEAK void arm64_syscall(struct arm64_iframe_long *iframe, bool is_64bit)
{
    panic("unhandled syscall vector\n");
}

void arm64_regsave_sysreg(struct arm64_iframe_long *iframe, int is_arm64_iframe_long)
{
    unsigned int current_el = ARM64_READ_SYSREG(CURRENTEL) >> 2;
    uint32_t spsr;
    uint64_t elr;

    switch (current_el) {
    case 2:
        spsr = ARM64_READ_SYSREG(spsr_el2);
        elr = ARM64_READ_SYSREG(elr_el2);
        break;
    case 3:
        spsr = ARM64_READ_SYSREG(spsr_el3);
        elr = ARM64_READ_SYSREG(elr_el3);
        break;
    case 1:
    default:
        spsr = ARM64_READ_SYSREG(spsr_el1);
        elr = ARM64_READ_SYSREG(elr_el1);
        break;
    }

    if (is_arm64_iframe_long) {
        iframe->spsr = spsr;
        iframe->elr = elr;
    } else {
        struct arm64_iframe_short* sframe = (struct arm64_iframe_short *) iframe;
        sframe->spsr = spsr;
        sframe->elr = elr;
    }
}

void arm64_regrestore_sysreg(struct arm64_iframe_long *iframe, int is_arm64_iframe_long)
{
    unsigned int current_el = ARM64_READ_SYSREG(CURRENTEL) >> 2;
    uint32_t spsr = iframe->spsr;
    uint64_t elr = iframe->elr;

    if (!is_arm64_iframe_long) {
        struct arm64_iframe_short* sframe = (struct arm64_iframe_short *) iframe;
        spsr = sframe->spsr;
        elr = sframe->elr;
    }

    switch (current_el) {
    case 2:
        ARM64_WRITE_SYSREG(spsr_el2, spsr);
        ARM64_WRITE_SYSREG(elr_el2, elr);
        break;
    case 3:
        ARM64_WRITE_SYSREG(spsr_el3, spsr);
        ARM64_WRITE_SYSREG(elr_el3, elr);
        break;
    case 1:
    default:
        ARM64_WRITE_SYSREG(spsr_el1, spsr);
        ARM64_WRITE_SYSREG(elr_el1, elr);
        break;
    }
}

void arm64_sync_exception(struct arm64_iframe_long *iframe)
{
    struct fault_handler_table_entry *fault_handler;
    unsigned int current_el = ARM64_READ_SYSREG(CURRENTEL) >> 2;
    uint cpuid = arch_curr_cpu_num();
    uint32_t esr, ec, il, iss;
    uint64_t far;
    switch (current_el) {
    case 2:
        esr = ARM64_READ_SYSREG(esr_el2);
        far = ARM64_READ_SYSREG(far_el2);
        break;
    case 3:
        esr = ARM64_READ_SYSREG(esr_el3);
        far = ARM64_READ_SYSREG(far_el3);
        break;
    case 1:
    default:
        esr = ARM64_READ_SYSREG(esr_el1);
        far = ARM64_READ_SYSREG(far_el1);
        break;
    }

    ec = BITS_SHIFT(esr, 31, 26);
    il = BIT(esr, 25);
    iss = BITS(esr, 24, 0);

    switch (ec) {
        case 0b000111: /* floating point */
            arm64_fpu_exception(iframe);
            return;
        case 0b010001: /* syscall from arm32 */
        case 0b010101: /* syscall from arm64 */
#ifdef WITH_LIB_SYSCALL
            void arm64_syscall(struct arm64_iframe_long *iframe);
#ifdef CONFIG_SPRD_GICV3
            arch_enable_irqs();
            arm64_syscall(iframe);
            arch_disable_irqs();
#else
            arch_enable_fiqs();
            arm64_syscall(iframe);
            arch_disable_fiqs();
#endif
            return;
#else
            arm64_syscall(iframe, (ec == 0x15) ? true : false);
            return;
#endif
        case 0b100000: /* instruction abort from lower level */
        case 0b100001: /* instruction abort from same level */
            dprintf(INFO,"instruction abort: PC at 0x%llx\n", iframe->elr);
            print_fault_msg(BITS(iss, 5, 0));
            break;
        case 0b100100: /* data abort from lower level */
        case 0b100101: { /* data abort from same level */
            for (fault_handler = __fault_handler_table_start;
                    fault_handler < __fault_handler_table_end;
                    fault_handler++) {
                if (fault_handler->pc == iframe->elr) {
                    iframe->elr = fault_handler->fault_handler;
                    return;
                }
            }

            dprintf(INFO,"data fault: %s access from PC 0x%llx, FAR 0x%llx, iss 0x%x (DFSC 0x%lx)\n",
                   BIT(iss, 6) ? "Write" : "Read", iframe->elr, far, iss, BITS(iss, 5, 0));
            print_fault_msg(BITS(iss, 5, 0));
            break;
        }
        default:
            dprintf(INFO,"unhandled synchronous exception\n");
    }

    /* unhandled exception, die here */
    always_printf_flag[cpuid] = 1; //prevent log crossing of multi-core
    dprintf(INFO,"ESR 0x%x: ec 0x%x, il 0x%x, iss 0x%x, EL%1d\n", esr, ec, il, iss, current_el);
    dump_iframe(iframe);
    dump_backtrace(iframe);
    always_printf_flag[cpuid] = 0;
    panic("die\n");
}

void arm64_invalid_exception(struct arm64_iframe_long *iframe, unsigned int which)
{
    uint cpuid = arch_curr_cpu_num();

    always_printf_flag[cpuid] = 1; //prevent log crossing of multi-core
    dprintf(INFO,"invalid exception, which 0x%x\n", which);
    dump_iframe(iframe);
    dump_backtrace(iframe);
    always_printf_flag[cpuid] = 0;

    panic("die\n");
}
