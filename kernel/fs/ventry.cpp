#include <fs/ventry.hpp>
#include <fs/vnode.hpp>
#include <fs/vfs.hpp>
#include <lib/types.hpp>
#include <lib/kstd.hpp>
#include <mm/slab.hpp>
#include <sys/reflock.hpp>

namespace vfs
{

ventry_t* ventry_new(const char* name, vnode_t* node)
{
	ventry_t* dentry = (ventry_t*)kmalloc(sizeof(ventry_t));
	strncpy(dentry->name, name, 64);
	auto namel = string_length(dentry->name);
	if(dentry->name[namel - 1] == '/' && namel > 1)
		dentry->name[namel - 1] = '\0';
	dentry->name[63] = '\0';
	node->nlinks++;

	dentry->node = node;
	dentry->parent = nullptr;
	dentry->children = nullptr;
	dentry->sibling_prev = nullptr;
	dentry->sibling_next = nullptr;
	dentry->mount = nullptr;
	reflock_init(dentry->ref);
	return dentry;
}

void ventry_free(ventry_t* ventry)
{
	kfree(ventry);
}

void ventry_ref(ventry_t* ventry)
{
	reflock_acquire(ventry->ref);
}

void ventry_put(ventry_t* ventry)
{
	if(ventry == vfs::get_root_dentry())
		return;

	auto released = reflock_release_or_lock(ventry->ref);
	if(released)
	{
		bool has_node = (ventry->node != nullptr);
		spinlock_release(ventry->ref.lock);
		if(!has_node)
			ventry_free(ventry);
	}
}

}
