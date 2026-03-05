/*
 * Copyright (c) 2008 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#pragma once

#include <stdint.h>

/* low level macros for accessing memory mapped hardware registers */
#define REG64(addr) ((volatile uint64_t *)(uintptr_t)(addr))
#define REG32(addr) ((volatile uint32_t *)(uintptr_t)(addr))
#define REG16(addr) ((volatile uint16_t *)(uintptr_t)(addr))
#define REG8(addr) ((volatile uint8_t *)(uintptr_t)(addr))

#define RMWREG64(addr, startbit, width, val) *REG64(addr) = (*REG64(addr) & ~(((1<<(width)) - 1) << (startbit))) | ((val) << (startbit))
#define RMWREG32(addr, startbit, width, val) *REG32(addr) = (*REG32(addr) & ~(((1<<(width)) - 1) << (startbit))) | ((val) << (startbit))
#define RMWREG16(addr, startbit, width, val) *REG16(addr) = (*REG16(addr) & ~(((1<<(width)) - 1) << (startbit))) | ((val) << (startbit))
#define RMWREG8(addr, startbit, width, val) *REG8(addr) = (*REG8(addr) & ~(((1<<(width)) - 1) << (startbit))) | ((val) << (startbit))

#define writel(v, a) (*REG32(a) = (v))
#define readl(a) (*REG32(a))
#define writeb(v, a) (*REG8(a) = (v))
#define readb(a) (*REG8(a))


#define writew(v, a) (*(REG16(a)) = (v))
#define readw(a) (*(REG16(a)))


#define CHIP_REG_OR(reg_addr, value)    (*(volatile u32 *)(reg_addr) |= (u32)(value))
#define CHIP_REG_AND(reg_addr, value)   (*(volatile u32 *)(reg_addr) &= (u32)(value))
#define CHIP_REG_GET(reg_addr)          (*(volatile u32 *)(reg_addr))
#define CHIP_REG_SET(reg_addr, value)   (*(volatile u32 *)(reg_addr)  = (u32)(value))

#define __arch_putb(v,a)                (*(volatile unsigned char *)(a) = (v))
#define __arch_putw(v,a)                (*(volatile unsigned short *)(a) = (v))
#define __arch_putl(v,a)                (*(volatile unsigned int *)(a) = (v))
#define __arch_putq(v,a)                (*(volatile unsigned long long *)(a) = (v))

#define __arch_getb(a)                  (*(volatile unsigned char *)(a))
#define __arch_getw(a)                  (*(volatile unsigned short *)(a))
#define __arch_getl(a)                  (*(volatile unsigned int *)(a))
#define __arch_getq(a)                  (*(volatile unsigned long long *)(a))

#define __raw_readb(a)          __arch_getb(a)
#define __raw_readw(a)          __arch_getw(a)
#define __raw_readl(a)          __arch_getl(a)
#define __raw_readq(a)          __arch_getq(a)

#define __raw_writeb(v,a)       __arch_putb(v,a)
#define __raw_writew(v,a)       __arch_putw(v,a)
#define __raw_writel(v,a)       __arch_putl(v,a)
#define __raw_writeq(v,a)       __arch_putq(v,a)

#define read32(c)       __raw_readl(c)
#define write32(v, c)   __raw_writel(v, c)

