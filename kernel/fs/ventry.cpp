#include <fs/ventry.hpp>
#include <fs/vnode.hpp>
#include <fs/vfs.hpp>
#include <types.hpp>
#include <kstd.hpp>
#include <klog.hpp>
#include <mm/slab.hpp>
#include <sys/reflock.hpp>
#include <sys/spinlock.hpp>

namespace vfs
{

ventry_t* ventry_new(const char* name, vnode_t* node)
{
	ventry_t* dentry = (ventry_t*)kmalloc(sizeof(ventry_t));
	strncpy(dentry->name, name, 64);
	auto namel = string_length(dentry->name);
	if(namel > 1 && dentry->name[namel - 1] == '/')
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
		{
			dcache_remove(ventry);
			ventry_free(ventry);
		}
	}
}

struct dcache_t
{
	spinlock_t lock;
	ventry_t* hash_table[1024];
};

static dcache_t* dcache = nullptr;

void dcache_init()
{
	dcache = (dcache_t*)kmalloc(sizeof(dcache_t));
	spinlock_init(dcache->lock);
	memset(dcache->hash_table, 0, sizeof(ventry_t*) * 1024);
}

uint64_t dcache_hash(ventry_t* parent, const char* name, size_t length)
{
	uint64_t hash = 0xcbf29ce484222325ULL;
	const uint64_t prime = 0x100000001b3ULL;

	for(size_t i = 0; i < length; i++)
	{
		hash ^= uint64_t(name[i]);
		hash *= prime;
	}

	hash ^= uint64_t(parent);
	return hash % 1024;
}

ventry_t* dcache_get(ventry_t* parent, const char* name)
{
	auto len = string_length(name);
	auto hash = dcache_hash(parent, name, len);

	uint64_t rflags;
	spinlock_acquire_irqsave(dcache->lock, rflags);
	ventry_t* dentry = dcache->hash_table[hash];
	while(dentry)
	{
		if(dentry->parent == parent && dentry->name[len] == '\0' && memcmp(dentry->name, name, len) == 0)
			break;

		dentry = dentry->next;
	}
	spinlock_release_irqsave(dcache->lock, rflags);

	return dentry;
}

void dcache_insert(ventry_t* entry)
{
	auto hash = dcache_hash(entry->parent, entry->name, string_length(entry->name));

	uint64_t rflags;
	spinlock_acquire_irqsave(dcache->lock, rflags);
	entry->next = dcache->hash_table[hash];
	dcache->hash_table[hash] = entry;
	spinlock_release_irqsave(dcache->lock, rflags);
}

void dcache_remove(ventry_t* entry)
{
	auto hash = dcache_hash(entry->parent, entry->name, string_length(entry->name));

	uint64_t rflags;
	spinlock_acquire_irqsave(dcache->lock, rflags);
	ventry_t** cur = &dcache->hash_table[hash];
	while(*cur)
	{
		if(*cur == entry)
		{
			*cur = entry->next;
			entry->next = nullptr;
			break;
		}
		cur = &(*cur)->next;
	}	
	spinlock_release_irqsave(dcache->lock, rflags);
}

}
