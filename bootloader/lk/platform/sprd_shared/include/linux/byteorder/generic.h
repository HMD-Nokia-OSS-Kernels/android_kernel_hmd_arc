#ifndef _LINUX_BYTEORDER_GENERIC_H
#define _LINUX_BYTEORDER_GENERIC_H

#if defined(__KERNEL__)
/*
 * inside the kernel, we can use nicknames;
 * outside of it, we must avoid POSIX namespace pollution...
 */
#define cpu_to_le64 __cpu_to_le64
#define le64_to_cpu __le64_to_cpu
#define cpu_to_be64 __cpu_to_be64
#define be64_to_cpu __be64_to_cpu

#define cpu_to_le32 __cpu_to_le32
#define le32_to_cpu __le32_to_cpu
#define cpu_to_be32 __cpu_to_be32
#define be32_to_cpu __be32_to_cpu
#define be32_to_cpup __be32_to_cpup

#define cpu_to_le16 __cpu_to_le16
#define le16_to_cpu __le16_to_cpu
#define cpu_to_be16 __cpu_to_be16

#define be16_to_cpu __be16_to_cpu

#endif


#endif /* _LINUX_BYTEORDER_GENERIC_H */
