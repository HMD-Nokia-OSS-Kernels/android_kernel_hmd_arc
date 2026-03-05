/*
 * Copyright (c) 2013 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#include <lk/debug.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <platform/debug.h>
#include <kernel/spinlock.h>

extern volatile int only_save_flag[SMP_MAX_CPUS];

#if WITH_SMP
#include <arch/arch_ops.h>
extern char core[4][8];
static int last_line = 1;
#endif

#define DEFINE_STDIO_DESC(id)   \
    [(id)]  = {                 \
        .io = &console_io,      \
    }

FILE __stdio_FILEs[3] = {
    DEFINE_STDIO_DESC(0), /* stdin */
    DEFINE_STDIO_DESC(1), /* stdout */
    DEFINE_STDIO_DESC(2), /* stderr */
};
#undef DEFINE_STDIO_DESC

int fputc(int _c, FILE *fp) {
    unsigned char c = _c;
    return io_write(fp->io, (char *)&c, 1);
}

int putchar(int c) {
    return fputc(c, stdout);
}

int puts(const char *str) {
    int err;
    uint cpu = arch_curr_cpu_num();

    only_save_flag[cpu] = 0;
    err = fputs(str, stdout);
    if (err >= 0)
        err = fputc('\n', stdout);
    return err;
}

int fputs(const char *s, FILE *fp) {
    size_t len = strlen(s);

    return io_write(fp->io, s, len);
}

size_t fwrite(const void *ptr, size_t size, size_t count, FILE *fp) {
    size_t bytes_written;

    if (size == 0 || count == 0)
        return 0;

    // fast path for size == 1
    if (likely(size == 1)) {
        return io_write(fp->io, ptr, count);
    }

    bytes_written = io_write(fp->io, ptr, size * count);
    return bytes_written / size;
}

int getc(FILE *fp) {
    char c;
    ssize_t ret = io_read(fp->io, &c, sizeof(c));

    return (ret > 0) ? c : ret;
}

int getchar(void) {
    return getc(stdin);
}

static int _fprintf_output_func(const char *str, size_t len, void *state) {
    FILE *fp = (FILE *)state;

    return io_write(fp->io, str, len);
}

int vfprintf(FILE *fp, const char *fmt, va_list ap) {
    return _printf_engine(&_fprintf_output_func, (void *)fp, fmt, ap);
}

static int _ufprintf_output_func(const char *str, size_t len, void *state) {
	size_t i;

#if WITH_SMP
    if(last_line) {
        uint cpu = arch_curr_cpu_num();
        for (i = 0; i < strlen(core[cpu]); i++)
            platform_dputc(core[cpu][i]);
        last_line = 0;
    }
#endif
    /* only write out the serial port */
    for (i = 0; i < len; i++)
        platform_dputc(str[i]);
#if WITH_SMP
    if (str[len-1] == '\n')
        last_line = 1;
#endif

    return len;
}

int ufprintf(FILE *fp, const char *fmt, va_list ap) {
    return _printf_engine(&_ufprintf_output_func, (void *)fp, fmt, ap);
}

int fprintf(FILE *fp, const char *fmt, ...) {
    va_list ap;
    int err;

    va_start(ap, fmt);
    err = vfprintf(fp, fmt, ap);
    va_end(ap);
    return err;
}

int log_save(const char *fmt, ...)
{
    va_list ap;
    int err;
    uint cpu = arch_curr_cpu_num();

    only_save_flag[cpu] = 1;
    va_start(ap, fmt);
    err = vfprintf(stdout, fmt, ap);
    va_end(ap);

    return err;
}

#if !DISABLE_DEBUG_OUTPUT
int printf(const char *fmt, ...) {
    va_list ap;
    int err;
    uint cpu = arch_curr_cpu_num();

    only_save_flag[cpu] = 0;
    va_start(ap, fmt);
    err = vfprintf(stdout, fmt, ap);
    va_end(ap);

    return err;
}

int uprintf(const char *fmt, ...) {
    va_list ap;
    int err;

    va_start(ap, fmt);
    err = ufprintf(stdout, fmt, ap);
    va_end(ap);

    return err;
}

int vprintf(const char *fmt, va_list ap) {
    return vfprintf(stdout, fmt, ap);
}
#endif // !DISABLE_DEBUG_OUTPUT
