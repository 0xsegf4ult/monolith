#pragma once

#include <container_of.hpp>

struct list_node_t
{
	list_node_t* prev;
	list_node_t* next;
};

using list_head_t = list_node_t;

inline void list_node_init(list_node_t& node)
{
	node.prev = &node;
	node.next = &node;
}

inline void __list_add(list_node_t* node, list_node_t* prev, list_node_t* next)
{
	next->prev = node;
	node->next = next;

	node->prev = prev;
	prev->next = node;
}

inline void list_add(list_head_t& head, list_node_t& node)
{
	__list_add(&node, &head, head.next);
}

inline void list_add_tail(list_head_t& head, list_node_t& node)
{
	__list_add(&node, head.prev, &head);
}

inline void __list_del(list_node_t* prev, list_node_t* next)
{
	next->prev = prev;
	prev->next = next;
}

inline void __list_del_entry(list_node_t* entry)
{
	__list_del(entry->prev, entry->next);
}

inline void list_del(list_node_t& entry)
{
	__list_del_entry(&entry);
	list_node_init(entry);
}

inline void list_move(list_head_t& head, list_node_t& entry)
{
	__list_del_entry(&entry);
	list_add(head, entry);
}

inline bool list_empty(list_head_t& head)
{
	return &head == head.next;
}

inline list_node_t* __list_first(list_head_t* head)
{
	return head->next;
}

inline list_node_t* __list_take_first(list_head_t* head)
{
	auto* node = __list_first(head);
	list_del(*node);
	return node;
}

#define list_take_first(head, type, member) \
	container_of(__list_take_first(head), type, member)

#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)

#define list_next_entry(pos, member) \
	list_entry((pos)->member.next, typeof(*(pos)), member)

#define list_for_each_entry(pos, head_ref, member) \
	for(pos = list_first_entry(&head_ref, typeof(*pos), member); \
		&pos->member != (&head_ref); \
		pos = list_next_entry(pos, member))

#define list_for_each_entry_safe(pos, tmp, head_ref, member) \
	for(pos = list_first_entry(&head_ref, typeof(*pos), member), \
		tmp = list_next_entry(pos, member); \
		&pos->member != (&head_ref); \
		pos = tmp, tmp = list_next_entry(tmp, member))
