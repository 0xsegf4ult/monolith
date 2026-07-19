#pragma once

#include <sys/reflock.h>
#include <libk/list.h>

struct vnode;
struct mount;

struct ventry
{
	char name[64];
	struct vnode* node;
	struct ventry* parent;

	list_head_t children;
	list_head_t sibling;
	struct ventry* next;	

	struct mount* mount;
	reflock_t ref;
};

struct ventry* ventry_new(const char* name, struct vnode* node);
void ventry_free(struct ventry* ventry);
void ventry_ref(struct ventry* ventry);
void ventry_put(struct ventry* ventry);

struct ventry* dcache_get(struct ventry* parent, const char* name);
void dcache_insert(struct ventry* entry);
void dcache_remove(struct ventry* entry);
