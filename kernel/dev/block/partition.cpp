#include <dev/block/partition.hpp>
#include <dev/block/disk.hpp>
#include <dev/block/block.hpp>
#include <fs/ops.hpp>
#include <fs/vfs.hpp>
#include <kstd.hpp>
#include <klog.hpp>
#include <mm/slab.hpp>
#include <list.hpp>

ssize_t part_read(vfs::file_descriptor_t* file, byte* buffer, size_t length)
{
	auto* bdev = blockdev_get(file->inode->dev);
	auto* part = (partition_t*)bdev->data;
	auto* disk_bdev = blockdev_get(part->parent->dev);
	return disk_bdev->bops->pread_blocks(disk_bdev, buffer, length * part->parent->block_size, part->start_lba + file->read_pos * part->parent->block_size) * part->parent->block_size;
}

static vfs::fs_file_ops fops = 
{
	.read = part_read
};

ssize_t part_pread_blocks(block_device_t* dev, byte* buffer, size_t blocks, size_t offset)
{
	auto* part = (partition_t*)dev->data;
	auto* disk_bdev = blockdev_get(part->parent->dev);
	log::debug("part_pread_blocks from disk {} blockdev {:#x}", part->parent->name, disk_bdev);
	return disk_bdev->bops->pread_blocks(disk_bdev, buffer, blocks, part->start_lba + offset);
}

static blockdev_ops bops =
{
	.pread_blocks = part_pread_blocks
};

partition_t* partition_create(disk_t* parent, size_t start_block, size_t block_count, size_t index)
{
	auto* part = (partition_t*)kmalloc(sizeof(partition_t));
	part->parent = parent;
	part->start_lba = start_block;
	part->block_count = block_count;
	list_node_init(part->list_node);

	char name_buf[64];
	format_to(string_span{&name_buf[0], 64}, "/dev/{}p{}", parent->name, index);

	//FIXME: maj:min allocation
	auto* blkdev = blockdev_alloc(dev_t{parent->dev.major(), uint16_t(parent->dev.minor() + index - 1)});
	blkdev->data = part;
	blkdev->fops = &fops;
	blkdev->bops = &bops;

	auto dev_node = vfs::mknod(name_buf, S_IFBLK | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, dev_t{parent->dev.major(), uint16_t(parent->dev.minor() + index - 1)});
	return part;
}
