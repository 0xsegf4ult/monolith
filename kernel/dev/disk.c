#include <dev/disk.h>
#include <fs/ops.h>
#include <fs/stat.h>
#include <fs/vfs.h>
#include <mm/slab.h>
#include <mm/vmm.h>
#include <sys/device.h>
#include <libk/list.h>
#include <libk/string.h>
#include <libk/vsprintf.h>
#include <types.h>

struct disk* disk_create(dev_t blk, const char* name, void* priv_data, struct file_ops* fops, struct blockdev_ops* bops)
{
	struct disk* disk = kmalloc(sizeof(struct disk));
	disk->dev = blk;
	strncpy(disk->name, name, 32);
	disk->data = priv_data;
	list_node_init(&disk->partitions);
	disk->partcount = 0;
	disk->block_size = 512;
	disk->block_count = 0;

	struct block_device* blkdev = blockdev_new(blk);
	blkdev->data = disk;
	blkdev->fops = fops;
	blkdev->bd_ops = bops;

	char name_buf[64];
	sprintf(name_buf, "/dev/%s", disk->name);
	vfs_mknod(name_buf, S_IFBLK | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, blk);

	return disk;
}

ssize_t part_read(struct file_descriptor* file, byte* buffer, size_t length)
{
	struct block_device* bdev = blockdev_lookup(file->inode->dev);
	struct partition* part = (struct partition*)bdev->data;
	struct block_device* disk_bdev = blockdev_lookup(part->parent->dev);
	return disk_bdev->bd_ops->pread_blocks(disk_bdev, buffer, length * part->parent->block_size, part->start_lba + file->pos * part->parent->block_size) * part->parent->block_size;
}

static struct file_ops fops =
{
	.read = part_read
};

ssize_t part_pread_blocks(struct block_device* dev, byte* buffer, size_t blocks, off_t offset)
{
	struct partition* part = (struct partition*)dev->data;
	struct block_device* disk_bdev = blockdev_lookup(part->parent->dev);
	return disk_bdev->bd_ops->pread_blocks(disk_bdev, buffer, blocks, part->start_lba + offset);
}

static struct blockdev_ops bops =
{
	.pread_blocks = part_pread_blocks
};

struct partition* disk_create_partition(struct disk* parent, size_t start_block, size_t block_count, size_t index)
{
	struct partition* part = kmalloc(sizeof(struct partition));
	part->parent = parent;
	part->start_lba = start_block;
	part->block_count = block_count;
	list_node_init(&part->list_node);

	char name_buf[64];
	sprintf(name_buf, "/dev/%sp%u", parent->name, index);

	dev_t devid = make_dev(dev_major(parent->dev), dev_minor(parent->dev) + index);
	struct block_device* blkdev = blockdev_new(devid);
	blkdev->data = part;
	blkdev->fops = &fops;
	blkdev->bd_ops = &bops;

	vfs_mknod(name_buf, S_IFBLK | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, devid);
	return part;
}

typedef struct
{
        char signature[8];
        uint32_t revision;
        uint32_t header_size;
        uint32_t crc32;
        uint32_t reserved;
        uint64_t header_lba;
        uint64_t altheader_lba;
        uint64_t lba_firstusable;
        uint64_t lba_lastusable;
        uint64_t guid_low;
        uint64_t guid_high;
        uint64_t partarray_lba;
        uint32_t partarray_count;
        uint32_t partentry_size;
        uint32_t partarray_crc32;
} gpt_t;

typedef struct
{
        uint64_t parttype_guidl;
        uint64_t parttype_guidh;
        uint64_t guidl;
        uint64_t guidh;
        uint64_t start_lba;
        uint64_t end_lba;
        uint64_t attributes;
        char name[72];
} gpt_partition_t;

static void disk_parse_gpt(struct disk* disk, byte* buffer)
{
	gpt_t* gpt = (gpt_t*)(buffer + 512);
	if(strncmp(gpt->signature, "EFI PART", 8) != 0)
		return;

	gpt_partition_t* partarray = (gpt_partition_t*)(buffer + 1024);
	for(int i = 1024; i < 4096; i += gpt->partentry_size)
	{
		if(partarray->guidl != 0 || partarray->guidh != 0)
		{
			struct partition* part = disk_create_partition(disk, partarray->start_lba, partarray->end_lba - partarray->start_lba, disk->partcount + 1);
			list_add_tail(&disk->partitions, &part->list_node);
			disk->partcount++;
		}

		partarray++;
	}
}

void disk_scan(struct disk* disk)
{
	byte* buffer = (byte*)vmalloc(0x1000);
	char path_buf[64];
	sprintf(path_buf, "/dev/%s", disk->name);
	int fd = vfs_open(path_buf, 0);
	if(fd < 0)
		return;

	vfs_read(fd, buffer, 0x1000);

	disk_parse_gpt(disk, buffer);

	vfs_close(fd);
	vfree(buffer);
}
