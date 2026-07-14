#include <fs/vnode.hpp>
#include <fs/ops.hpp>
#include <types.hpp>
#include <mm/slab.hpp>
#include <sys/mutex.hpp>
#include <sys/stat.hpp>
#include <net/socket.hpp>

namespace vfs
{

vnode_t* vnode_new(mode_t mode)
{
	vnode_t* node = (vnode_t*)kmalloc(sizeof(vnode_t));
	node->mode = mode;
	node->size = 0;
	node->uid = 0;
	node->gid = 0;
	node->dev = dev_t{0};
	node->nlinks = 0;
	node->ref = 0;
	node->data = nullptr;
	node->iops = nullptr;
	node->fops = nullptr;
	mutex_init(node->lock);
	return node;
}

void vnode_free(vnode_t* node)
{
	if(S_ISSOCK(node->mode))
		socket_put((socket_t*)node->data);

	kfree(node);
}

void vnode_ref(vnode_t* node)
{
	node->ref++;
}

void vnode_put(vnode_t* node)
{
	if(node->ref <= 1 && node->nlinks == 0)
	{
		vnode_free(node);
		return;
	}

	node->ref--;
}

}
