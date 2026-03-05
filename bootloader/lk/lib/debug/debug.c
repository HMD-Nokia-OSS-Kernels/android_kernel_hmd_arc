/*
 * Copyright (c) 2008-2015 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <ctype.h>
#include <arch.h>
#include <lk/debug.h>
#include <stdlib.h>
#include <printf.h>
#include <stdio.h>
#include <string.h>
#include <lk/list.h>
#include <arch/ops.h>
#include <platform.h>
#include <platform/debug.h>
#include <kernel/spinlock.h>
#include <sprd_log.h>

#ifdef WITH_FUNCTION_SYMBOLS
#include <lib/elf_defines.h>

#ifdef ARCH_ARM64
#define ELF_SYMBOL Elf64_Sym
#define ELF_ST_TYPE ELF64_ST_TYPE
#else
#define ELF_SYMBOL Elf32_Sym
#define ELF_ST_TYPE ELF32_ST_TYPE
#endif

#ifndef CONFIG_FUNCTION_SYMTAB_MAXSIZE
#define CONFIG_FUNCTION_SYMTAB_MAXSIZE 10000
#endif
#ifndef CONFIG_FUNCTION_STRTAB_MAXSIZE
#define CONFIG_FUNCTION_STRTAB_MAXSIZE 0x25000
#endif
#if !DEBUG
#error "Please make sure you realy want enable WITH_FUNCTION_SYMBOLS in user build, all symbols strings will exist in lk.bin."
#endif
/*
 * The symbols and strings of thees two arrays, will be autofilled
 * by add_fsymbol_data.sh:
 *    function_symtab[]: data copied from .symtab to .fsymtab
 *    function_strtab[]: data copied from .strtab to .fstrtab
 */
const struct ELF_SYMBOL function_symtab[CONFIG_FUNCTION_SYMTAB_MAXSIZE] __ALIGNED(sizeof(void *)) __SECTION(".fsymtab") = {{0},};
const char function_strtab[CONFIG_FUNCTION_STRTAB_MAXSIZE] __ALIGNED(sizeof(void *)) __SECTION(".fstrtab") = {0};

/* support printf format %pF */
// ToDo: binary-search
char* pointer_function_to_string(char* buf, unsigned long address) {
    const struct ELF_SYMBOL *tmp = NULL;
    const char *name = NULL;
    int i, len;

    for (i = 0; i < CONFIG_FUNCTION_SYMTAB_MAXSIZE; i++) {
        tmp = &function_symtab[i];
        if (tmp->st_value == 0 || ELF_ST_TYPE(tmp->st_info) != STT_FUNC) {
                continue;
        }

        if ((unsigned long)tmp->st_value <= address
            && address <= (unsigned long)(tmp->st_value+tmp->st_size)) {
                if (tmp->st_name < CONFIG_FUNCTION_STRTAB_MAXSIZE) {
                    name = &function_strtab[tmp->st_name];
                    sprintf(buf, "%lx\t", address);
                    len = strlen(buf);
                    strncpy(buf+len, name, 64);
                    len = strlen(buf);
                    sprintf(buf+len, "+0x%lx", (unsigned long)(address - tmp->st_value));
                    return buf;
                }
                return NULL;
        }
    }

    return NULL;
}
#else
char* pointer_function_to_string(char* buf __UNUSED, unsigned long address __UNUSED) { return NULL;}
#endif

void spin(uint32_t usecs) {
    lk_bigtime_t start = current_time_hires();

    while ((current_time_hires() - start) < usecs)
        ;
}

extern volatile int always_printf_flag[SMP_MAX_CPUS];
void panic(const char *fmt, ...) {
    dprintf(INFO,"panic (caller %pF): ", __GET_CALLER());
    uint cpuid = arch_curr_cpu_num();

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    /* dump registers and backtrace */
    always_printf_flag[cpuid] = 1; //prevent log crossing of multi-core
#if ARCH_ARM64
    arch_show_regs_backtrace();
#else
    arch_show_hpy_regs_backtrace();
#endif
    sprdlog_panic_finish();
    always_printf_flag[cpuid] = 0;

    platform_halt(HALT_ACTION_HALT, HALT_REASON_SW_PANIC);
}

#if !DISABLE_DEBUG_OUTPUT

static int __panic_stdio_fgetc(void *ctx) {
    char c;
    int err;

    err = platform_pgetc(&c, false);
    if (err < 0)
        return err;
    return (unsigned char)c;
}

static ssize_t __panic_stdio_read(io_handle_t *io, char *s, size_t len) {
    if (len == 0)
        return 0;

    int err = platform_pgetc(s, false);
    if (err < 0)
        return err;

    return 1;
}

static ssize_t __panic_stdio_write(io_handle_t *io, const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        platform_pputc(s[i]);
    }
    return len;
}

FILE *get_panic_fd(void) {
    static const io_handle_hooks_t panic_hooks = {
        .write = __panic_stdio_write,
        .read = __panic_stdio_read,
    };
    static io_handle_t panic_io = {
        .magic = IO_HANDLE_MAGIC,
        .hooks = &panic_hooks
    };
    static FILE panic_fd = {
        .io = &panic_io
    };

    return &panic_fd;
}

void hexdump(const void *ptr, size_t len) {
    addr_t address = (addr_t)ptr;
    size_t count;

    for (count = 0 ; count < len; count += 16) {
        union {
            uint32_t buf[4];
            uint8_t  cbuf[16];
        } u;
        size_t s = ROUNDUP(MIN(len - count, 16), 4);
        size_t i;

        dprintf(INFO,"0x%08lx: ", address);
        for (i = 0; i < s / 4; i++) {
            u.buf[i] = ((const uint32_t *)address)[i];
            dprintf(INFO,"%08x ", u.buf[i]);
        }
        for (; i < 4; i++) {
            dprintf(INFO,"         ");
        }
        dprintf(INFO,"|");

        for (i=0; i < 16; i++) {
            char c = u.cbuf[i];
            if (i < s && isprint(c)) {
                dprintf(INFO,"%c", c);
            } else {
                dprintf(INFO,".");
            }
        }
        dprintf(INFO,"|\n");
        address += 16;
    }
}

void hexdump8_ex(const void *ptr, size_t len, uint64_t disp_addr) {
    addr_t address = (addr_t)ptr;
    size_t count;
    size_t i;
    const char *addr_fmt = ((disp_addr + len) > 0xFFFFFFFF)
                           ? "0x%016llx: "
                           : "0x%08llx: ";

    for (count = 0 ; count < len; count += 16) {
        dprintf(INFO,addr_fmt, disp_addr + count);

        for (i=0; i < MIN(len - count, 16); i++) {
            dprintf(INFO,"%02hhx ", *(const uint8_t *)(address + i));
        }

        for (; i < 16; i++) {
            dprintf(INFO,"   ");
        }

        dprintf(INFO,"|");

        for (i=0; i < MIN(len - count, 16); i++) {
            char c = ((const char *)address)[i];
            dprintf(INFO,"%c", isprint(c) ? c : '.');
        }

        dprintf(INFO,"\n");
        address += 16;
    }
}

#endif // !DISABLE_DEBUG_OUTPUT
