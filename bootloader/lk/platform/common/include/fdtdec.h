#ifndef __FDTDEC_H
#define __FDTDEC_H

#include <sprd_types.h>
#include <linux/byteorder/little_endian.h>
#include <linux/byteorder/generic.h>
#include <sprd_common.h>

/*Memory address where DTB is located*/
extern int  __dtb_start;
extern int  __dtb_end;

typedef phys_addr_t  fdt_addr_t;
typedef phys_size_t  fdt_size_t;

#ifdef CONFIG_PHYS_64BIT
#define FDT_ADDR_T_NONE (-1U)
#define fdt_addr_to_cpu(reg) be64_to_cpu(reg)
#define fdt_size_to_cpu(reg) be64_to_cpu(reg)
#define cpu_to_fdt_addr(reg) cpu_to_be64(reg)
#define cpu_to_fdt_size(reg) cpu_to_be64(reg)
#else
#define FDT_ADDR_T_NONE (-1U)
#define fdt_addr_to_cpu(reg) be32_to_cpu(reg)
#define fdt_size_to_cpu(reg) be32_to_cpu(reg)
#define cpu_to_fdt_addr(reg) cpu_to_be32(reg)
#define cpu_to_fdt_size(reg) cpu_to_be32(reg)
#endif
int fdt_getprop_u32(const void *fdt, int off, const char *prop, u32 *dflt);
int fdtdec_check_fdt(void);
int fdtdec_prepare_fdt(void);
int fdtdec_lookup_phandle(const void *blob, int node, const char *prop_name);
int fdtdec_decode_region(const void *blob, int node, const char *prop_name,
         fdt_addr_t *basep, fdt_size_t *sizep);
int get_buffer_base_size_from_dt(const char *name, unsigned long *basep, unsigned long *sizep);
int fdtdec_get_int(const void *blob, int node, const char *prop_name, int default_val);
int fdtdec_get_child_count(const void *blob, int node);

#endif
