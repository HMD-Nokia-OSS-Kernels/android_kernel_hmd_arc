#ifndef _LINUX_UNALIGNED_GENERIC_H
#define _LINUX_UNALIGNED_GENERIC_H

extern void __bad_unaligned_access_size(void);
#define sprd_choose_lex	__builtin_choose_expr

#define __get_unaligned_le(ptr) ((__force typeof(*(ptr)))({			\
	sprd_choose_lex(sizeof(*(ptr)) == 8, get_unaligned_le64((ptr)),	\
	sprd_choose_lex(sizeof(*(ptr)) == 4, get_unaligned_le32((ptr)),	\
	sprd_choose_lex(sizeof(*(ptr)) == 2, get_unaligned_le16((ptr)),	\
	sprd_choose_lex(sizeof(*(ptr)) == 1, *(ptr),			\
	__bad_unaligned_access_size()))));					\
	}))

#define __get_unaligned_be(ptr) ((__force typeof(*(ptr)))({			\
	sprd_choose_lex(sizeof(*(ptr)) == 1, *(ptr),			\
	sprd_choose_lex(sizeof(*(ptr)) == 2, get_unaligned_be16((ptr)),	\
	sprd_choose_lex(sizeof(*(ptr)) == 4, get_unaligned_be32((ptr)),	\
	sprd_choose_lex(sizeof(*(ptr)) == 8, get_unaligned_be64((ptr)),	\
	__bad_unaligned_access_size()))));					\
	}))

#define __put_unaligned_le(val, ptr) ({					\
	void *__gu_p = (ptr);						\
	if(sizeof(*(ptr)) == 1) \
		*(u8 *)__gu_p = (__force u8)(val);			\
	else if(sizeof(*(ptr)) == 2) \
		put_unaligned_le16((__force u16)(val), __gu_p); \
	else if(sizeof(*(ptr)) == 4) \
		put_unaligned_le32((__force u32)(val), __gu_p); \
	else if(sizeof(*(ptr)) == 8) \
		put_unaligned_le64((__force u64)(val), __gu_p); \
	else \
		__bad_unaligned_access_size(); \
	(void)0; })


#define __put_unaligned_be(val, ptr) ({					\
	void *__gu_p = (ptr);						\
	if (sizeof(*(ptr)) == 1)					\
		*(u8 *)__gu_p = (__force u8)(val);			\
	else if (sizeof(*(ptr)) == 2)								\
		put_unaligned_be16((__force u16)(val), __gu_p);		\
	else if (sizeof(*(ptr)) == 4)							\
		put_unaligned_be32((__force u32)(val), __gu_p);		\
	else if (sizeof(*(ptr)) == 8)								\
		put_unaligned_be64((__force u64)(val), __gu_p);		\
	else							\
		__bad_unaligned_access_size();				\
	(void)0; })

#endif /* _LINUX_UNALIGNED_GENERIC_H */