/*
 * <sprd_compat.h>
 * Copyright (C) 2021 Unisoc Communications Inc.
 * History:
 * 	<2021> <sprd_compat.h>
 *
 */

#ifndef __SPRD_LIST_H
#define __SPRD_LIST_H
struct list_head {
	struct list_head *next, *prev;
};

static inline void prefetch(const void *x) {;}

#define list_for_each(pos, head) \
	for (pos = (head)->next; prefetch(pos->next), pos != (head); \
		pos = pos->next)

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

#define list_entry(ptr, type, member) \
		container_of(ptr, type, member)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
	     prefetch(pos->member.next), &pos->member != (head);	\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)

static inline void sprd_list_add(struct list_head *lk_new, struct list_head *prev, struct list_head *next)
{
	next->prev = lk_new;
	lk_new->next = next;
	lk_new->prev = prev;
	prev->next = lk_new;
}

static inline void list_add(struct list_head *lk_new, struct list_head *head)
{
	sprd_list_add(lk_new, head, head->next);
}

static inline void sprd_list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void list_move(struct list_head *list, struct list_head *head)
{
	sprd_list_del(list->prev, list->next);
	list_add(list, head);
}

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		n = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

#endif /*__SPRD_LIST_H*/
