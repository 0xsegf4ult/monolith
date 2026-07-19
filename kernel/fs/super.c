#include <fs/super.h>
#include <fs/filesystem.h>
#include <fs/lookup.h>
#include <fs/vfs.h>
#include <mm/slab.h>
#include <sys/device.h>
#include <libk/list.h>
#include <errno.h>

int vfs_mount(const char* src, const char* target, const char* fstype)
{
	struct filesystem* fs = filesystem_lookup(fstype);
	if(!fs)
		return -ENODEV;

	if((fs->flags & FS_FLAG_NODEV) && src)
		return -EINVAL;

	if(!(fs->flags & FS_FLAG_NODEV) && !src)
		return -EINVAL;

	struct ventry* query = nullptr;
	int q_status = vfs_lookup(target, &query, 0);
	if(q_status < 0)
		return q_status;

	struct block_device* blockdev = nullptr;
	if(src)
	{
		struct ventry* s_query = nullptr;
		int sq_status = vfs_lookup(src, &s_query, 0);
		if(sq_status < 0)
			return sq_status;

		blockdev = blockdev_lookup(s_query->node->dev);
	}

	struct superblock* sb = nullptr;
	int sb_status = fs->sb_ops->init(blockdev, &sb);
	if(sb_status < 0)
		return sb_status;

	struct mount* mp = kmalloc(sizeof(struct mount));
	query->mount = mp;
	mp->mountpoint = query;
	mp->fs = fs;
	mp->sb = sb;
	mp->sb->fs = fs;
	mp->sb->bdev = blockdev;
	list_node_init(&mp->list_node);

	list_add_tail(&vfs_context()->mounts, &mp->list_node);
	return 0;
}
