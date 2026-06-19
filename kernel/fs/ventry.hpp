#pragma once

#include <lib/types.hpp>
#include <sys/reflock.hpp>

namespace vfs
{

struct vnode_t;
struct mount_t;

struct ventry_t
{
	char name[64];
	vnode_t* node;
	ventry_t* parent;
	ventry_t* children;
	ventry_t* sibling_prev;
	ventry_t* sibling_next;

	mount_t* mount;
	reflock_t ref;
};

ventry_t* ventry_new(const char* name, vnode_t* node);
void ventry_free(ventry_t* ventry);
void ventry_ref(ventry_t* ventry);
void ventry_put(ventry_t* ventry);

}
