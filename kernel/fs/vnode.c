#include <fs/vnode.h>
#include <mm/slab.h>
#include <sys/mutex.h>
#include <types.h>

struct vnode* vnode_new(mode_t mode)
{
	struct vnode* node = kmalloc(sizeof(struct vnode));
	node->mode = mode;
	node->size = 0;
	node->uid = 0;
	node->gid = 0;
	node->dev = 0;

	node->nlink = 0;
	atomic_store(&node->ref, 0);

	node->data = nullptr;
	node->sb = nullptr;
	node->iops = nullptr;
	node->fops = nullptr;
	mutex_init(&node->lock);

	return node;
}

void vnode_free(struct vnode* node)
{
	kfree(node);
}

void vnode_ref(struct vnode* node)
{
	atomic_fetch_add_explicit(&node->ref, 1, memory_order_relaxed);
}

void vnode_put(struct vnode* node)
{
	uint32_t cur_refcnt = atomic_fetch_add_explicit(&node->ref, -1, memory_order_relaxed);
	if(cur_refcnt <= 1 && node->nlink == 0)
		vnode_free(node);
}
