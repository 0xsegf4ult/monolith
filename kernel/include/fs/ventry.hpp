#pragma once

#include <sys/reflock.hpp>
#include <list.hpp>
#include <types.hpp>

namespace vfs
{

struct vnode_t;
struct mount_t;

struct ventry_t
{
	char name[64];
	vnode_t* node;
	ventry_t* parent;

	list_head_t children;
	list_head_t sibling;
	ventry_t* next;	

	mount_t* mount;
	reflock_t ref;
};

ventry_t* ventry_new(const char* name, vnode_t* node);
void ventry_free(ventry_t* ventry);
void ventry_ref(ventry_t* ventry);
void ventry_put(ventry_t* ventry);

void dcache_init();
ventry_t* dcache_get(ventry_t* parent, const char* name);
void dcache_insert(ventry_t* entry);
void dcache_remove(ventry_t* entry);

}
