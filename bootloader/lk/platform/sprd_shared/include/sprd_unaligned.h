#ifndef _SPRD_UNALIGNED_H
#define _SPRD_UNALIGNED_H

#include <linux/unaligned/le_byteshift.h>
#include <linux/unaligned/generic.h>

/*
 * Base on the value of __BYTE_ORDER Select endianness
 */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define get_unaligned	__get_unaligned_le
#define put_unaligned	__put_unaligned_le
#else
#define get_unaligned	__get_unaligned_be
#define put_unaligned	__put_unaligned_be
#endif

#endif /* _SPRD_UNALIGNED_H */
