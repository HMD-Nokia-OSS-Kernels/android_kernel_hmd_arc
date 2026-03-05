/*
 * <sprd_types.h> - <Definition type of the function of this file>
 *
 * Copyright (C) 2021 Unisoc Communications Inc.
 * History:
 * 	<2021> <sprd_types.h>
 */

#ifndef _SPRD_TYPES_H
#define _SPRD_TYPES_H

#include <stdbool.h>

typedef unsigned char __u8;
typedef unsigned short __u16;
typedef unsigned int __u32;

typedef __signed__ char __s8;
typedef __signed__ short __s16;
typedef __signed__ int __s32;

#if defined(__GNUC__)
__extension__ typedef __signed__ long long __s64;
__extension__ typedef unsigned long long __u64;
#endif

#ifdef CONFIG_ARM64
#define BITS_PER_LONG 64
typedef unsigned long long phys_size_t;
typedef unsigned long long phys_addr_t;
typedef unsigned long __kernel_size_t;
#else	/* CONFIG_ARM64 */
#define BITS_PER_LONG 32
typedef unsigned long phys_size_t;
typedef unsigned long phys_addr_t;
typedef unsigned int __kernel_size_t;
#endif	/* CONFIG_ARM64 */

typedef unsigned long resource_size_t;
typedef unsigned long dma_addr_t;

typedef		__u8		uint8_t;
typedef		__u16		uint16_t;
typedef		__u32		uint32_t;
typedef		__u64		uint64_t;
typedef		__s8		int8_t;
typedef		__s16		int16_t;
typedef		__s32		int32_t;
typedef		__s64		int64_t;

typedef unsigned short		dev_t;
typedef unsigned long		ulong;
typedef unsigned short		ushort;
typedef unsigned char		u_char;
typedef unsigned int		uint;

#ifndef _SIZE_T
#define _SIZE_T
typedef __kernel_size_t		size_t;
#endif


#if defined(CONFIG_USE_STDINT) && defined(__INT64_TYPE__)
typedef		__UINT64_TYPE__	uint64_t;
typedef		__UINT64_TYPE__	u_int64_t;
typedef		__INT64_TYPE__		int64_t;
#endif

#ifdef __CHECKER__
#define __bitwise__ __attribute__((bitwise))
#else
#define __bitwise__
#endif
#ifdef __CHECK_ENDIAN__
#define __bitwise __bitwise__
#else
#define __bitwise
#endif

typedef unsigned __bitwise__	gfp_t;

struct ustat {
	int			f_tfree;
	unsigned long		f_tinode;
	char			f_fname[6];
	char			f_fpack[6];
};
typedef unsigned char	BOOLEAN;
#define	VOLATILE volatile

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef __u16 __bitwise __le16;
typedef __u16 __bitwise __be16;
typedef __u32 __bitwise __le32;
typedef __u32 __bitwise __be32;
#if defined(__GNUC__)
typedef __u64 __bitwise __le64;
typedef __u64 __bitwise __be64;
#endif
typedef __u16 __bitwise __sum16;
typedef __u32 __bitwise __wsum;

#endif /* _SPRD_TYPES_H */
