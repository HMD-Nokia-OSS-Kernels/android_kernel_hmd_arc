/*
 * Copyright (c) 2012-2014 Travis Geiselbrecht
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <assert.h>
#include <sys/types.h>
#include <lk/debug.h>
#include <dev/interrupt/arm_gic.h>
#include <kernel/thread.h>
#include <kernel/debug.h>
#include <lk/init.h>
#include <platform/interrupts.h>
#include <arch/ops.h>
#include <platform/gic.h>
#include <lk/trace.h>
#include <lk/err.h>
#include <lk/bits.h>

#define LOCAL_TRACE 0

#define GICREG8(reg)	(*REG8(GIC_BASE_VIRT + (reg)))
#define GICREG32(reg) 	(*REG32(GIC_BASE_VIRT + (reg)))
#define GICREG64(reg)	(*REG64(GIC_BASE_VIRT + (reg)))

#define GIC_MPIDR_AFF1_SHIFT		(8)
#define GIC_MPIDR_AFF2_SHIFT		(16)
#define GIC_MPIDR_AFF3_SHIFT		(32)

/*******************************************************************************
 * GICv3 specific Distributor interface register offsets and constants.
 ******************************************************************************/
/* distribution regs */
#define GICD_CTLR               (GICD_OFFSET + 0x0000)
#define GICD_TYPER              (GICD_OFFSET + 0x0004)
#define GICD_IIDR               (GICD_OFFSET + 0x0008)
#define GICD_IGROUPR(n)         (GICD_OFFSET + 0x0080 + (n) * 4)
#define GICD_ISENABLER(n)       (GICD_OFFSET + 0x0100 + (n) * 4)
#define GICD_ICENABLER(n)       (GICD_OFFSET + 0x0180 + (n) * 4)
#define GICD_ISPENDR(n)         (GICD_OFFSET + 0x0200 + (n) * 4)
#define GICD_ICPENDR(n)         (GICD_OFFSET + 0x0280 + (n) * 4)
#define GICD_ISACTIVER(n)       (GICD_OFFSET + 0x0300 + (n) * 4)
#define GICD_ICACTIVER(n)       (GICD_OFFSET + 0x0380 + (n) * 4)
#define GICD_IPRIORITYR(n)      (GICD_OFFSET + 0x0400 + (n))
#define GICD_ITARGETSR(n)       (GICD_OFFSET + 0x0800 + (n) * 4)
#define GICD_ICFGR(n)           (GICD_OFFSET + 0x0c00 + (n) * 4)
#define GICD_IGRPMODR(n)	(GICD_OFFSET + 0x0d00 + (n) * 4)
#define GICD_NSACR(n)           (GICD_OFFSET + 0x0e00 + (n) * 4)
#define GICD_SGIR               (GICD_OFFSET + 0x0f00)
#define GICD_CPENDSGIR(n)       (GICD_OFFSET + 0x0f10 + (n) * 4)
#define GICD_SPENDSGIR(n)       (GICD_OFFSET + 0x0f20 + (n) * 4)
/*
 * GICD_IROUTER<n> register is at 0x6000 + 8n, where n is the interrupt id and
 * n >= 32, making the effective offset as 0x6100.
 */
#define GICD_IROUTER(n)		(GICD_OFFSET + 0x6000 + (n) * 8)
#define GICD_PIDR2		(GICD_OFFSET + 0xffe8)

/* GICD_CTLR bit definitions */
#define CTLR_ENABLE_G0_SHIFT		0
#define CTLR_ENABLE_G1NS_SHIFT		1
#define CTLR_ENABLE_G1S_SHIFT		2
#define CTLR_ARE_S_SHIFT		4
#define CTLR_ARE_NS_SHIFT		5
#define CTLR_DS_SHIFT			6
#define CTLR_E1NWF_SHIFT		7
#define CTLR_RWP_SHIFT			31

#define CTLR_ENABLE_G0_MASK		0x1
#define CTLR_ENABLE_G1NS_MASK		0x1
#define CTLR_ENABLE_G1S_MASK		0x1
#define CTLR_ARE_S_MASK			0x1
#define CTLR_ARE_NS_MASK		0x1
#define CTLR_DS_MASK			0x1
#define CTLR_E1NWF_MASK			0x1
#define CTLR_RWP_MASK			0x1

#define CTLR_ENABLE_G0_BIT		(1 << CTLR_ENABLE_G0_SHIFT)
#define CTLR_ENABLE_G1NS_BIT		(1 << CTLR_ENABLE_G1NS_SHIFT)
#define CTLR_ENABLE_G1S_BIT		(1 << CTLR_ENABLE_G1S_SHIFT)
#define CTLR_ARE_S_BIT			(1 << CTLR_ARE_S_SHIFT)
#define CTLR_ARE_NS_BIT			(1 << CTLR_ARE_NS_SHIFT)
#define CTLR_DS_BIT			(1 << CTLR_DS_SHIFT)
#define CTLR_E1NWF_BIT			(1 << CTLR_E1NWF_SHIFT)
#define CTLR_RWP_BIT			(1 << CTLR_RWP_SHIFT)

/* GICD_IROUTER shifts and masks */
#define IROUTER_IRM_SHIFT	31
#define IROUTER_IRM_MASK	0x1ULL

/* Constants to indicate the status of the RWP bit */
#define RWP_TRUE		1
#define RWP_FALSE		0

/*
 * Macro to wait for updates to :
 * GICD_CTLR[2:0] - the Group Enables
 * GICD_CTLR[5:4] - the ARE bits
 * GICD_ICENABLERn - the clearing of enable state for SPIs
 */
#define gicd_wait_for_pending_write()			\
	do {						\
		;					\
	} while (GICREG32(GICD_CTLR) & CTLR_RWP_BIT)

/*******************************************************************************
 * GICv3 Re-distributor interface registers & constants
 ******************************************************************************/
#define GICR_PCPUBASE_SHIFT		0x11
#define GICR_RDBASE_OFFSET(_rd)		(GICR_OFFSET + 0x10000 * (_rd) * 2)
#define GICR_SGIBASE_OFFSET(_rd)	(GICR_OFFSET + 0x10000 * (_rd) * 2 + 0x10000)
#define GICR_CTLR(_rd)			(GICR_RDBASE_OFFSET(_rd) + 0x0000)
#define GICR_TYPER(_rd)			(GICR_RDBASE_OFFSET(_rd) + 0x0008)
#define GICR_WAKER(_rd)			(GICR_RDBASE_OFFSET(_rd) + 0x0014)
#define GICR_IGROUPR0(_rd)		(GICR_SGIBASE_OFFSET(_rd) + 0x0080)
#define GICR_ISENABLER0(_rd)		(GICR_SGIBASE_OFFSET(_rd) + 0x0100)
#define GICR_ICENABLER0(_rd)		(GICR_SGIBASE_OFFSET(_rd) + 0x0180)
#define GICR_ISPENDR0(_rd)		(GICR_SGIBASE_OFFSET(_rd) + 0x0200)
#define GICR_ICPENDR0(_rd)		(GICR_SGIBASE_OFFSET(_rd) + 0x0280)
#define GICR_ISACTIVER0(_rd)		(GICR_SGIBASE_OFFSET(_rd) + 0x0300)
#define GICR_ICACTIVER0(_rd)		(GICR_SGIBASE_OFFSET(_rd) + 0x0380)
#define GICR_IPRIORITYR(_rd, n)		(GICR_SGIBASE_OFFSET(_rd) + 0x0400 + (n))
#define GICR_ICFGR0(_rd)		(GICR_SGIBASE_OFFSET(_rd) + 0x0c00)
#define GICR_ICFGR1(_rd)		(GICR_SGIBASE_OFFSET(_rd) + 0x0c04)
#define GICR_IGRPMODR0(_rd)		(GICR_SGIBASE_OFFSET(_rd) + 0x0d00)

/* GICR_CTLR bit definitions */
#define GICR_CTLR_RWP_SHIFT	3
#define GICR_CTLR_RWP_MASK	0x1
#define GICR_CTLR_RWP_BIT	(1 << GICR_CTLR_RWP_SHIFT)

/* GICR_WAKER bit definitions */
#define WAKER_CA_SHIFT		2
#define WAKER_PS_SHIFT		1

#define WAKER_CA_MASK		0x1
#define WAKER_PS_MASK		0x1

#define WAKER_CA_BIT		(1 << WAKER_CA_SHIFT)
#define WAKER_PS_BIT		(1 << WAKER_PS_SHIFT)

/* GICR_TYPER bit definitions */
#define TYPER_AFF_VAL_SHIFT	32
#define TYPER_PROC_NUM_SHIFT	8
#define TYPER_LAST_SHIFT	4

#define TYPER_AFF_VAL_MASK	0xffffffff
#define TYPER_PROC_NUM_MASK	0xffff
#define TYPER_LAST_MASK		0x1

#define TYPER_LAST_BIT		(1 << TYPER_LAST_SHIFT)

/*******************************************************************************
 * Definitions for CPU system register interface to GICv3
 ******************************************************************************/
#define ICC_SRE_EL1     S3_0_C12_C12_5
#define ICC_SRE_EL2     S3_4_C12_C9_5
#define ICC_SRE_EL3     S3_6_C12_C12_5
#define ICC_CTLR_EL1    S3_0_C12_C12_4
#define ICC_CTLR_EL3    S3_6_C12_C12_4
#define ICC_PMR_EL1     S3_0_C4_C6_0
#define ICC_IGRPEN1_EL1 S3_0_c12_c12_7
#define ICC_IGRPEN0_EL1 S3_0_c12_c12_6
#define ICC_HPPIR0_EL1  S3_0_c12_c8_2
#define ICC_HPPIR1_EL1  S3_0_c12_c12_2
#define ICC_IAR0_EL1    S3_0_c12_c8_0
#define ICC_IAR1_EL1    S3_0_c12_c12_0
#define ICC_EOIR0_EL1   S3_0_c12_c8_1
#define ICC_EOIR1_EL1   S3_0_c12_c12_1

/*******************************************************************************
 * GICv3 CPU interface registers & constants
 ******************************************************************************/
/* ICC_SRE bit definitions*/
#define ICC_SRE_EN_BIT          (1 << 3)
#define ICC_SRE_DIB_BIT         (1 << 2)
#define ICC_SRE_DFB_BIT         (1 << 1)
#define ICC_SRE_SRE_BIT         (1 << 0)

/* ICC_IGRPEN1_EL1 bit definitions */
#define IGRPEN1_EL1_ENABLE_G1S_SHIFT    0
#define IGRPEN1_EL1_ENABLE_G1S_BIT      (1 << IGRPEN1_EL1_ENABLE_G1S_SHIFT)

/* ICC_IGRPEN0_EL1 bit definitions */
#define IGRPEN1_EL1_ENABLE_G0_SHIFT     0
#define IGRPEN1_EL1_ENABLE_G0_BIT       (1 << IGRPEN1_EL1_ENABLE_G0_SHIFT)

/* ICC_HPPIR0_EL1 bit definitions */
#define HPPIR0_EL1_INTID_SHIFT          0
#define HPPIR0_EL1_INTID_MASK           0xffffff

/* ICC_HPPIR1_EL1 bit definitions */
#define HPPIR1_EL1_INTID_SHIFT          0
#define HPPIR1_EL1_INTID_MASK           0xffffff

/* ICC_IAR0_EL1 bit definitions */
#define IAR0_EL1_INTID_SHIFT            0
#define IAR0_EL1_INTID_MASK             0xffffff

/* ICC_IAR1_EL1 bit definitions */
#define IAR1_EL1_INTID_SHIFT            0
#define IAR1_EL1_INTID_MASK             0xffffff

/**********************************************************************
 * Macros which create inline functions to read or write CPU system
 * registers
 *********************************************************************/

#define _DEFINE_SYSREG_READ_FUNC(_name, _reg_name)		\
static inline uint64_t read_ ## _name(void)			\
{								\
	uint64_t v;						\
	__asm__ volatile ("mrs %0, " #_reg_name : "=r" (v));	\
	return v;						\
}

#define _DEFINE_SYSREG_WRITE_FUNC(_name, _reg_name)			\
static inline void write_ ## _name(uint64_t v)				\
{									\
	__asm__ volatile ("msr " #_reg_name ", %0" : : "r" (v));	\
}

/* Define read & write function for renamed system register */
#define DEFINE_RENAME_SYSREG_RW_FUNCS(_name, _reg_name)	\
	_DEFINE_SYSREG_READ_FUNC(_name, _reg_name)	\
	_DEFINE_SYSREG_WRITE_FUNC(_name, _reg_name)

/* Define read function for renamed system register */
#define DEFINE_RENAME_SYSREG_READ_FUNC(_name, _reg_name)\
	_DEFINE_SYSREG_READ_FUNC(_name, _reg_name)

/* Define write function for renamed system register */
#define DEFINE_RENAME_SYSREG_WRITE_FUNC(_name, _reg_name)\
	_DEFINE_SYSREG_WRITE_FUNC(_name, _reg_name)
#if ARCH_ARM64
DEFINE_RENAME_SYSREG_RW_FUNCS(icc_sre_el1, ICC_SRE_EL1)
DEFINE_RENAME_SYSREG_RW_FUNCS(icc_sre_el2, ICC_SRE_EL2)
DEFINE_RENAME_SYSREG_RW_FUNCS(icc_sre_el3, ICC_SRE_EL3)
DEFINE_RENAME_SYSREG_RW_FUNCS(icc_pmr_el1, ICC_PMR_EL1)
DEFINE_RENAME_SYSREG_RW_FUNCS(icc_igrpen1_el1, ICC_IGRPEN1_EL1)
DEFINE_RENAME_SYSREG_RW_FUNCS(icc_igrpen0_el1, ICC_IGRPEN0_EL1)
DEFINE_RENAME_SYSREG_READ_FUNC(icc_hppir0_el1, ICC_HPPIR0_EL1)
DEFINE_RENAME_SYSREG_READ_FUNC(icc_hppir1_el1, ICC_HPPIR1_EL1)
DEFINE_RENAME_SYSREG_READ_FUNC(icc_iar0_el1, ICC_IAR0_EL1)
DEFINE_RENAME_SYSREG_READ_FUNC(icc_iar1_el1, ICC_IAR1_EL1)
DEFINE_RENAME_SYSREG_WRITE_FUNC(icc_eoir0_el1, ICC_EOIR0_EL1)
DEFINE_RENAME_SYSREG_WRITE_FUNC(icc_eoir1_el1, ICC_EOIR1_EL1)
#else
GEN_CP15_REG_FUNCS(icc_igrpen1, 0, c12, c12, 7);
GEN_CP15_REG_FUNCS(icc_pmr, 0, c4, c6, 6);
GEN_CP15_REG_FUNCS(icc_eoir1, 0, c12, c12, 1);
GEN_CP15_REG_FUNCS(icc_iar1, 0, c12, c12, 0);

#define write_icc_igrpen1_el1(reg) arm_write_icc_igrpen1(reg)
#define write_icc_pmr_el1(reg) arm_write_icc_pmr(reg)
#define write_icc_eoir1_el1(reg) arm_write_icc_eoir1(reg)
#define read_icc_igrpen1_el1() arm_read_icc_igrpen1()
#define read_icc_iar1_el1() arm_read_icc_iar1()
#endif


static spin_lock_t gicd_lock;
#define GICD_LOCK_FLAGS SPIN_LOCK_FLAG_IRQ_FIQ

#define GIC_MAX_PER_CPU_INT 	32
#define INT_DEFAULT_PRIORITY	0x40

#define GIC_G0		0x00
#define GIC_G1S		0x01
#define GIC_G1NS	0x02

#define GIC_DISABLE	0x0
#define GIC_ENABLE	0x1

struct int_handler_struct {
	int_handler handler;
	void *arg;
	uint8_t group;
	uint8_t priority;
	uint8_t enable;
	uint64_t route;
};

static struct int_handler_struct int_handler_table_per_cpu[SMP_MAX_CPUS][GIC_MAX_PER_CPU_INT]
	= {[0 ... SMP_MAX_CPUS-1] = {[0 ... GIC_MAX_PER_CPU_INT-1] = {NULL, NULL, GIC_G1NS, 0x80, GIC_DISABLE, 0}}};
static struct int_handler_struct int_handler_table_shared[MAX_INT-GIC_MAX_PER_CPU_INT]
	= {[0 ... MAX_INT-GIC_MAX_PER_CPU_INT-1] = {NULL, NULL, GIC_G1NS, 0x80, GIC_DISABLE, 0}};

typedef struct {
	uint32_t curr_irq;
	uint32_t state;
} __CPU_ALIGN int_state_t;

static int_state_t int_state[SMP_MAX_CPUS] = { [0 ... SMP_MAX_CPUS-1] = {0x3ff, 0xFFFFFFFF}};
static bool arm_gic_interrupt_change_allowed(uint32_t vector)
{
	return true;
}

static struct int_handler_struct *get_int_handler(uint32_t vector, uint32_t cpu)
{
	if (vector < GIC_MAX_PER_CPU_INT) {
		return &int_handler_table_per_cpu[cpu][vector];
	} else {
		return &int_handler_table_shared[vector - GIC_MAX_PER_CPU_INT];
	}
}

static status_t arm_gic_set_enable_locked(uint32_t vector, bool enable)
{
	uint32_t reg = vector / 32;
	uint32_t mask = 1ULL << (vector % 32);
	uint32_t cpu = arch_curr_cpu_num();
	struct int_handler_struct *handler = NULL;

	if (vector >= MAX_INT) {
		return ERR_INVALID_ARGS;
	}

	handler = get_int_handler(vector, cpu);

	if (vector < GIC_MAX_PER_CPU_INT) {
		if (enable) {
			GICREG32(GICR_ISENABLER0(cpu)) = mask;
			handler->enable = GIC_ENABLE;
		} else {
			GICREG32(GICR_ICENABLER0(cpu)) = mask;
			handler->enable = GIC_DISABLE;
		}

	} else {
		if (enable) {
			GICREG32(GICD_ISENABLER(reg)) = mask;
			handler->enable = GIC_ENABLE;
		} else {
			GICREG32(GICD_ICENABLER(reg)) = mask;
			handler->enable = GIC_DISABLE;
		}
	}

	return NO_ERROR;
}

status_t mask_interrupt(uint32_t vector)
{
	spin_lock_saved_state_t state;

	if (vector >= MAX_INT) {
		return ERR_INVALID_ARGS;
	}

	if (arm_gic_interrupt_change_allowed(vector)) {
		spin_lock_save(&gicd_lock, &state, GICD_LOCK_FLAGS);
		arm_gic_set_enable_locked(vector, false);
		spin_unlock_restore(&gicd_lock, state, GICD_LOCK_FLAGS);
	}

	return NO_ERROR;
}

status_t unmask_interrupt(uint32_t vector)
{
	spin_lock_saved_state_t state;

	if (vector >= MAX_INT) {
		return ERR_INVALID_ARGS;
	}

	if (arm_gic_interrupt_change_allowed(vector)) {
		spin_lock_save(&gicd_lock, &state, GICD_LOCK_FLAGS);
		arm_gic_set_enable_locked(vector, true);
		spin_unlock_restore(&gicd_lock, state, GICD_LOCK_FLAGS);
	}

	return NO_ERROR;
}

static status_t arm_gic_set_secure_locked(uint32_t vector, bool secure)
{
	uint32_t reg = vector / 32;
	uint32_t mask = 1ULL << (vector % 32);
	uint32_t cpu = arch_curr_cpu_num();
	struct int_handler_struct *handler = NULL;

	if (vector >= MAX_INT) {
		return ERR_INVALID_ARGS;
	}

	if (vector < GIC_MAX_PER_CPU_INT) {
		handler = get_int_handler(vector, cpu);
		if (secure) {
			handler->group = GIC_G1S;
			GICREG32(GICR_IGROUPR0(cpu)) &= ~mask;
			GICREG32(GICR_IGRPMODR0(cpu)) |= mask;
		} else {
			handler->group = GIC_G1NS;
			GICREG32(GICR_IGROUPR0(cpu)) |= mask;
			GICREG32(GICR_IGRPMODR0(cpu)) &= ~mask;
		}

		LTRACEF_LEVEL(2, "vector %d, secure %d, GICR_IGROUP0(%d) = %x GICR_IGRPMODR0(%d) = %x\n", vector, secure,
			cpu, GICREG32(GICR_IGROUPR0(cpu)),
			cpu, GICREG32(GICR_IGRPMODR0(cpu)));
	} else {
		handler = get_int_handler(vector, 0xFFFFFFFF);
		if (secure) {
			handler->group = GIC_G1S;
			GICREG32(GICD_IGROUPR(reg)) &= ~mask;
			GICREG32(GICD_IGRPMODR(reg)) |= mask;
		} else {
			handler->group = GIC_G1NS;
			GICREG32(GICD_IGROUPR(reg)) |= mask;
			GICREG32(GICD_IGRPMODR(reg)) &= ~mask;
		}

		LTRACEF_LEVEL(2, "vector %d, secure %d, GICD_IGROUP%d = %x GICD_IGRPMODR%d = %x\n", vector, secure,
			reg, GICREG32(GICD_IGROUPR(reg)),
			reg, GICREG32(GICD_IGRPMODR(reg)));
	}

	return NO_ERROR;
}

static status_t arm_gic_get_priority(uint32_t vector)
{
	uint32_t cpu = arch_curr_cpu_num();

	if (vector >= MAX_INT) {
		return ERR_INVALID_ARGS;
	}

	if (vector < GIC_MAX_PER_CPU_INT) {
		return GICREG8(GICR_IPRIORITYR(cpu, vector));
	} else {
		return GICREG8(GICD_IPRIORITYR(vector));
	}
}

static status_t arm_gic_set_priority_locked(uint32_t vector, uint8_t priority)
{
	uint32_t cpu = arch_curr_cpu_num();
	struct int_handler_struct *handler = NULL;

	if (vector >= MAX_INT) {
		return ERR_INVALID_ARGS;
	}

	if (vector < GIC_MAX_PER_CPU_INT) {
		LTRACEF_LEVEL(2, "vector %i, old GICR_IPRIORITYR%d(%d) = %x\n", vector, vector, cpu, GICREG8(GICR_IPRIORITYR(cpu, vector)));

		handler = get_int_handler(vector, cpu);
		handler->priority = priority;

		GICREG8(GICR_IPRIORITYR(cpu, vector)) = priority;
		LTRACEF_LEVEL(2, "vector %i, new GICR_IPRIORITYR%d(%d) = %x\n", vector, vector, cpu, GICREG8(GICR_IPRIORITYR(cpu, vector)));
	} else {
		LTRACEF_LEVEL(2, "vector %i, old GICD_IPRIORITYR%d = %x\n", vector, vector, GICREG8(GICD_IPRIORITYR(vector)));

		handler = get_int_handler(vector, 0xFFFFFFFF);
		handler->priority = priority;

		GICREG8(GICD_IPRIORITYR(vector)) = priority;
		LTRACEF_LEVEL(2, "vector %i, new GICD_IPRIORITYR%d = %x\n", vector, vector, GICREG8(GICD_IPRIORITYR(vector)));
	}

	return NO_ERROR;
}

void register_int_handler(uint32_t vector, int_handler handler, void *arg)
{
	struct int_handler_struct *h;
	uint32_t cpu = arch_curr_cpu_num();
	spin_lock_saved_state_t state;

	if (vector >= MAX_INT) {
		panic("register_int_handler: vector out of range %d\n", vector);
	}

	spin_lock_save(&gicd_lock, &state, GICD_LOCK_FLAGS);

	if (arm_gic_interrupt_change_allowed(vector)) {
		if (arm_gic_set_enable_locked(vector, false)) {
			panic("register_int_handler: can not disable %d\n", vector);
		}

		h = get_int_handler(vector, cpu);
		h->handler = handler;
		h->arg = arg;

		if (arm_gic_set_secure_locked(vector, false)) {
			panic("register_int_handler: can not set it as secure %d\n", vector);
		}

		if (arm_gic_set_priority_locked(vector, INT_DEFAULT_PRIORITY)) {
			panic("register_int_handler: can not set int priority %d\n", vector);
		}
	}

	spin_unlock_restore(&gicd_lock, state, GICD_LOCK_FLAGS);
}

void unregister_int_handler(uint32_t vector)
{
	struct int_handler_struct *h;
	uint32_t cpu = arch_curr_cpu_num();
	spin_lock_saved_state_t state;

	if (vector >= MAX_INT) {
		panic("unregister_int_handler: vector out of range %d\n", vector);
	}

	spin_lock_save(&gicd_lock, &state, GICD_LOCK_FLAGS);

	if (arm_gic_interrupt_change_allowed(vector)) {
		if (arm_gic_set_enable_locked(vector, false)) {
			panic("unregister_int_handler: can not disable %d\n", vector);
		}

		h = get_int_handler(vector, cpu);
		h->handler = NULL;
		h->arg = NULL;

		if (arm_gic_set_secure_locked(vector, false)) {
			panic("unregister_int_handler: can not set it as non-secure %d\n", vector);
		}

		if (arm_gic_set_priority_locked(vector, 0x80)) {
			panic("unregister_int_handler: can not set int priority %d\n", vector);
		}
	}

	spin_unlock_restore(&gicd_lock, state, GICD_LOCK_FLAGS);
}

static void arm_gic_init_percpu(uint32_t level)
{
#if 0
	uint32_t cpu = arch_curr_cpu_num();
	uint32_t temp = 0xFFFFFFFF;
	uint32_t i = 0;
	struct int_handler_struct *handler = NULL;

	/* Disable Group1 Secure interrupts */
	write_icc_igrpen1_el1(read_icc_igrpen1_el1() & ~IGRPEN1_EL1_ENABLE_G1S_BIT);

	/* get G0 interrupt */
	temp = GICREG32(GICR_IGROUPR0(cpu)) | GICREG32(GICR_IGRPMODR0(cpu));

	/* clear enable and clear pengding */
	GICREG32(GICR_ICENABLER0(cpu)) = temp;
	GICREG32(GICR_ICPENDR0(cpu)) = temp;

	/*
	 * GICR_IGROUPR0 and GICR_IGRPMODR0 are banked.
	 * Some of SGI & PPI are seen as G0 interrupt and set by SML.
	 * But some of bits of GICR_IGROUPR0 are read only.
	 * GICR_IGROUPR0 and GICR_IGRPMODR0 are set by SML.
	 */
	GICREG32(GICR_IGROUPR0(cpu)) = temp;
	GICREG32(GICR_IGRPMODR0(cpu)) = 0x0;

	while (~temp) {
		i = _ffz(temp);
		temp |= 1UL << i;

		handler = get_int_handler(i, cpu);
		handler->group = GIC_G0;
	}

	for (i = 0; i < GIC_MAX_PER_CPU_INT; i++) {
		handler = get_int_handler(i, cpu);
		if (handler->group != GIC_G0) {
			GICREG8(GICR_IPRIORITYR(cpu, i)) = handler->priority;
		}
	}

	/* unmask interrupts at all priority levels */
	write_icc_pmr_el1(0xFF);

	/* Enable Group1 Secure interrupts */
	write_icc_igrpen1_el1(read_icc_igrpen1_el1() | IGRPEN1_EL1_ENABLE_G1S_BIT);
#else
	/* This code for secure Group0. So, remove G0 interrupt config in LK state.*/
	/* Disable Group1 Secure interrupts */
	write_icc_igrpen1_el1(read_icc_igrpen1_el1() & ~IGRPEN1_EL1_ENABLE_G1S_BIT);

	/* unmask interrupts at all priority levels */
	write_icc_pmr_el1(0xFF);

	/* Enable Group1 Secure interrupts */
	write_icc_igrpen1_el1(read_icc_igrpen1_el1() | IGRPEN1_EL1_ENABLE_G1S_BIT);
#endif
}

LK_INIT_HOOK_FLAGS(arm_gic_init_percpu, arm_gic_init_percpu,
		LK_INIT_LEVEL_PLATFORM_EARLY, LK_INIT_FLAG_SECONDARY_CPUS);

static int arm_gic_max_cpu(void)
{
	return SMP_MAX_CPUS - 1;
	/*
	 * GICD_TYPER[7:5] = 000b, because our gic does not support ARE = 0
	 * return (GICREG32(GICD_TYPER) >> 5) & 0x7;
	 */
}

void arm_gic_init(void)
{
	int32_t i, j, reg;
	uint32_t temp = 0xFFFFFFFF;
	struct int_handler_struct *handler = NULL;
#if ARCH_ARM64
	uint64_t mpidr =  ARM64_READ_SYSREG(mpidr_el1);
#else
	uint32_t mpidr = arm_read_mpidr();
#endif

	/* Disable G1S interrupts */
	GICREG32(GICD_CTLR) &= ~CTLR_ENABLE_G1NS_BIT;
	gicd_wait_for_pending_write();

	for (i = 32; i < MAX_INT; i += 32) {
		reg = i / 32;
		temp = GICREG32(GICD_IGROUPR(reg)) | GICREG32(GICD_IGRPMODR(reg));

		/* clear enable and clear pengding */
		GICREG32(GICD_ICENABLER(reg)) = temp;
		GICREG32(GICD_ICPENDR(reg)) = temp;

		/*
		 * Iterate through all IRQs and set them to non-secure
		 * mode. This will allow the non-secure side to handle
		 * all the interrupts we don't explicitly claim.
		 */
		GICREG32(GICD_IGROUPR(reg)) = temp;
		GICREG32(GICD_IGRPMODR(reg)) = 0x0;

		while (~temp) {
			j = _ffz(temp);
			temp |= 1UL << j;

			handler = get_int_handler(i + j, 0xFFFFFFFF);
			handler->group = GIC_G1NS;
		}
	}

	for (i = 32; i < MAX_INT; i++) {
		handler = get_int_handler(i, 0xFFFFFFFF);

		if (handler->group != GIC_G1NS) {
			GICREG8(GICD_IPRIORITYR(i)) = handler->priority;

			/* Set external interrupts to target LOGIC cpu 0 */
			GICREG64(GICD_IROUTER(i)) = mpidr & 0xFF00FFFFFF;
			handler->route = mpidr & 0xFF00FFFFFF;
		}
	}

	/* enable G1NS interrupts */
	GICREG32(GICD_CTLR) |= CTLR_ENABLE_G1NS_BIT;
	gicd_wait_for_pending_write();
	arm_gic_init_percpu(0);
}

status_t arm_gic_sgi(uint32_t vector, uint32_t flags, uint32_t cpu_mask)
{
	uint32_t val =
		((flags & ARM_GIC_SGI_FLAG_TARGET_FILTER_MASK) << 24) |
		((cpu_mask & 0xff) << 16) |
		((flags & ARM_GIC_SGI_FLAG_NS) ? (1U << 15) : 0) |
		(vector & 0xf);

	if (vector >= 16)
		return ERR_INVALID_ARGS;

	LTRACEF_LEVEL(1, "GICD_SGIR: %x\n", val);

	GICREG32(GICD_SGIR) = val;

	return NO_ERROR;
}

enum handler_return platform_irq(struct arm_iframe *frame)
{
	enum handler_return ret = INT_NO_RESCHEDULE;
	uint32_t irq = 0x3ff;
	struct int_handler_struct *h = NULL;
	uint32_t cpu = arch_curr_cpu_num();

	irq = read_icc_iar1_el1() & 0x3ff;
	DSB;

	KEVLOG_IRQ_ENTER(irq);

	if (irq < MAX_INT) {

		LTRACEF_LEVEL(1, "platform_irq: iar 0x%x cpu %u spsr 0x%x, pc 0x%x, currthread %p\n",
				irq, cpu, frame->spsr, frame->pc, get_current_thread());

		int_state[cpu].curr_irq = irq;

		h = get_int_handler(irq, cpu);
		if (h->handler)
			ret = h->handler(h->arg);
		else
			TRACEF("unregistered but enabled irq %d!\n", irq);

		int_state[cpu].curr_irq = 0x3FF;

		write_icc_eoir1_el1(irq);
	} else if (irq >= 0x3fe) {
		// spurious
		ret = INT_NO_RESCHEDULE;
	} else {
		panic("unexpected irq %d\n", irq);
	}

	KEVLOG_IRQ_EXIT(irq);

	return ret;
}

void platform_fiq(struct arm_iframe *frame)
{
	LTRACEF_LEVEL(1, "platform_fiq ... \n");
	PANIC_UNIMPLEMENTED;
}


extern void arm_generic_timer_init(int irq, uint32_t freq_override);

#ifndef TIMER_ARM_GENERIC_SELECTED
#define TIMER_ARM_GENERIC_SELECTED CNTP
#endif

#define ARM_GENERIC_TIMER_INT_CNTHP 26
#define ARM_GENERIC_TIMER_INT_CNTV 27
#define ARM_GENERIC_TIMER_INT_CNTPS 29
#define ARM_GENERIC_TIMER_INT_CNTP 30

#define ARM_GENERIC_TIMER_INT_SELECTED(timer) ARM_GENERIC_TIMER_INT_##timer
#define XARM_GENERIC_TIMER_INT_SELECTED(timer) ARM_GENERIC_TIMER_INT_SELECTED(timer)
#define ARM_GENERIC_TIMER_INT XARM_GENERIC_TIMER_INT_SELECTED(TIMER_ARM_GENERIC_SELECTED)

static void platform_after_vm_init(uint level)
{
	uint32_t hcr;

	dprintf(INFO, "gic 0x%lx\n", (long unsigned int)GIC_BASE_VIRT);

	/* initialize the interrupt controller */
	arm_gic_init();

	/* initialize the timer block */
	arm_generic_timer_init(ARM_GENERIC_TIMER_INT, 0);
#if ARCH_ARM64
	/*enable HCR_EL2.IMO is bit(4)*/
	hcr = ARM64_READ_SYSREG(hcr_el2);
	hcr |= 0x10;
	ARM64_WRITE_SYSREG(hcr_el2, hcr);
#else
	/*enable HCR.IMO is bit(4)*/
	if (is_hyp_mode()) {
		hcr = arm_read_hcr();
		hcr |= 0x10;
		arm_write_hcr(hcr);
	}
#endif

}

LK_INIT_HOOK(platform_after_vm, platform_after_vm_init, LK_INIT_LEVEL_VM + 1);

