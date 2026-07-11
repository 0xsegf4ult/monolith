#include <fs/super.hpp>
#include <fs/vfs.hpp>
#include <fs/lookup.hpp>
#include <fs/filesystem.hpp>
#include <dev/block/block.hpp>
#include <mm/slab.hpp>
#include <sys/err.hpp>
#include <list.hpp>

namespace vfs
{

int mount(const char* src, const char* target, const char* fstype)
{
        vfilesystem_t* fs = lookup_fs(fstype);
        if(!fs)
		return -ENODEV;

	if((fs->flags & FS_FLAG_NODEV) && src)
                return -EINVAL;

	if(!(fs->flags & FS_FLAG_NODEV) && !src)
		return -EINVAL;

	ventry_t* query = nullptr;
        auto q_status = lookup(target, &query, 0);
        if(q_status < 0)
                return q_status;

	block_device_t* blockdev = nullptr;
	if(src)
	{
		ventry_t* s_query = nullptr;
		auto sq_status = lookup(src, &s_query, 0);
		if(sq_status < 0)
			return sq_status;

		blockdev = blockdev_get(s_query->node->dev);
	}

	superblock_t* sb = nullptr;
	auto sb_status = fs->sb_ops->init(blockdev, &sb);
	if(sb_status < 0)
	{
		return sb_status;	
	}

        auto* mp = (mount_t*)kmalloc(sizeof(mount_t));
        mp->mountpoint = query;
        mp->fs = fs;
	mp->sb = sb;
	mp->sb->fs = fs;
	mp->sb->bdev = blockdev;
	list_node_init(mp->list_node);

	auto* context = vfs::get();
	list_add_tail(context->mounts, mp->list_node);

        query->mount = mp;
        return 0;
}

}
