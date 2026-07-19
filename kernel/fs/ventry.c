#include <fs/ventry.h>
#include <fs/vnode.h>
#include <mm/slab.h>
#include <sys/reflock.h>
#include <sys/spinlock.h>
#include <libk/list.h>
#include <libk/string.h>
#include <types.h>

static struct ventry* hash_table[1024] = {};
static spinlock_t dcache_lock = {0};

uint64_t dcache_hash(struct ventry* parent, const char* name, size_t length)
{
	uint64_t hash = 0xcbf29ce484222325ull;
	constexpr uint64_t prime = 0x100000001b3ull;

	for(size_t i = 0; i < length; i++)
	{
		hash ^= (uint64_t)name[i];
		hash *= prime;
	}

	hash ^= (uint64_t)parent;
	return hash % 1024;
}

struct ventry* dcache_get(struct ventry* parent, const char* name)
{
	size_t len = strlen(name);
	uint64_t hash = dcache_hash(parent, name, len);

	uint64_t flags;
	spinlock_acquire_irqsave(&dcache_lock, &flags);
	struct ventry* ventry = hash_table[hash];
	while(ventry)
	{
		if(ventry->parent == parent && ventry->name[len] == '\0' && memcmp(ventry->name, name, len) == 0)
			break;

		ventry = ventry->next;
	}
	spinlock_release_irqsave(&dcache_lock, flags);

	return ventry;
}

void dcache_insert(struct ventry* ventry)
{
	uint64_t hash = dcache_hash(ventry->parent, ventry->name, strlen(ventry->name));

	uint64_t flags;
	spinlock_acquire_irqsave(&dcache_lock, &flags);
	ventry->next = hash_table[hash];
	hash_table[hash] = ventry;
	spinlock_release_irqsave(&dcache_lock, flags);
}

void dcache_remove(struct ventry* ventry)
{
	uint64_t hash = dcache_hash(ventry->parent, ventry->name, strlen(ventry->name));

	uint64_t flags;
	spinlock_acquire_irqsave(&dcache_lock, &flags);
	struct ventry** cur = &hash_table[hash];
	while(*cur)
	{
		if(*cur == ventry)
		{
			*cur = ventry->next;
			ventry->next = nullptr;
			break;
		}
		cur = &(*cur)->next;
	}
	spinlock_release_irqsave(&dcache_lock, flags);
}

struct ventry* ventry_new(const char* name, struct vnode* node)
{
	struct ventry* ventry = kmalloc(sizeof(struct ventry));
	strncpy(ventry->name, name, 64);
	size_t namel = strlen(ventry->name);
	if(namel > 1 && ventry->name[namel - 1] == '/')
		ventry->name[namel - 1] = '\0';
	ventry->name[63] = '\0';
	
	node->nlink++;

	ventry->node = node;
	ventry->parent = nullptr;

	list_node_init(&ventry->children);
	list_node_init(&ventry->sibling);
	ventry->next = nullptr;

	ventry->mount = nullptr;
	reflock_init(&ventry->ref);
	return ventry;
}

void ventry_free(struct ventry* ventry)
{
	kfree(ventry);
}

void ventry_ref(struct ventry* ventry)
{
	reflock_acquire(&ventry->ref);
}

void ventry_put(struct ventry* ventry)
{
	bool released = reflock_release_or_lock(&ventry->ref);
	if(released)
	{
		bool has_node = (ventry->node != nullptr);
		spinlock_release(&ventry->ref.lock);
		if(!has_node)
		{
			dcache_remove(ventry);
			ventry_free(ventry);
		}
	}
}
