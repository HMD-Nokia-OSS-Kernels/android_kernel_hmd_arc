#ifndef _LINUX_UNALIGNED_LE_BYTESHIFT_H
#define _LINUX_UNALIGNED_LE_BYTESHIFT_H

#include <linux/types.h>

#define sprd_shift(x, n)	(x << n)

#define __get_unaligned_le16(p)	(*(p) | sprd_shift(*((p)+1), 8))
#define get_unaligned_le16(p)	__get_unaligned_le16((const u8 *)p)

#define __get_unaligned_le32(p)	(get_unaligned_le16(p) | (u32)sprd_shift((u32)get_unaligned_le16(p+2), 16))
#define get_unaligned_le32(p)	__get_unaligned_le32((const u8 *)p)

#define __get_unaligned_le64(p)	(get_unaligned_le32(p) | (u64)sprd_shift((u64)get_unaligned_le32(p+4), 32))
#define get_unaligned_le64(p)	__get_unaligned_le64((const u8 *)p)

static inline void __put_unaligned_le16(u16 val, u8 *p)
{
	*p++ = val;
	*p++ = val >> 8;
}

static inline void __put_unaligned_le32(u32 val, u8 *p)
{
	__put_unaligned_le16(val >> 16, p + 2);
	__put_unaligned_le16(val, p);
}

static inline void __put_unaligned_le64(u64 val, u8 *p)
{
	__put_unaligned_le32(val >> 32, p + 4);
	__put_unaligned_le32(val, p);
}

#define get_unaligned_le16(p)	__get_unaligned_le16((const u8 *)p)
#define get_unaligned_le32(p)	__get_unaligned_le32((const u8 *)p)
#define get_unaligned_le64(p)	__get_unaligned_le64((const u8 *)p)
#define put_unaligned_le16(val, p)	__put_unaligned_le16((u16)val, (u8 *)p)
#define put_unaligned_le32(val, p)	__put_unaligned_le32((u32)val, (u8 *)p)
#define put_unaligned_le64(val, p)	__put_unaligned_le64((u64)val, (u8 *)p)

#endif /* _LINUX_UNALIGNED_LE_BYTESHIFT_H */
