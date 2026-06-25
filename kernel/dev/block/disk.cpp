#include <dev/block/disk.hpp>
#include <dev/block/block.hpp>
#include <dev/block/partition.hpp>
#include <dev/device.hpp>

#include <fs/vfs.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>
#include <lib/kstd.hpp>
#include <lib/klog.hpp>

disk_t* disk_create(dev_t blk, const char* name, void* priv_data, vfs::fs_file_ops* fops, blockdev_ops* bops)
{
	auto* disk = (disk_t*)kmalloc(sizeof(disk_t));
	disk->dev = blk;
	strncpy(disk->name, name, 32);
	disk->data = priv_data;
	disk->partition_list_tail = nullptr;
	disk->partition_list_head = nullptr;
	disk->partcount = 0;
	disk->block_size = 512;
	disk->block_count = 0;

	auto* blkdev = blockdev_alloc(blk);
	blkdev->data = disk;
	blkdev->fops = fops;
	blkdev->bops = bops;

	char name_buf[64];
	format_to(string_span{&name_buf[0], 64}, "/dev/{}", disk->name);
	auto dev_node = vfs::mknod(name_buf, S_IFBLK | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, blk);

	return disk;
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

void disk_parse_gpt(disk_t* disk, byte* buffer)
{
	gpt_t* gpt = (gpt_t*)(buffer + 512);
	if(strncmp(gpt->signature, "EFI PART", 8) != 0)
		return;

	gpt_partition_t* partarray = (gpt_partition_t*)(buffer + 1024);
	for(int i = 1024; i < 4096; i += gpt->partentry_size)
	{
		if(partarray->guidl != 0 || partarray->guidh != 0)
		{
			log::debug("disk: found partition LBA {:x} - {:x}", partarray->start_lba, partarray->end_lba);
			auto* part = partition_create(disk, partarray->start_lba, partarray->end_lba - partarray->start_lba, disk->partcount + 1);
			if(disk->partition_list_head)
			{
				disk->partition_list_tail->next = part;
				disk->partition_list_tail = part;
			}
			else
			{
				disk->partition_list_head = part;
				disk->partition_list_tail = part;
			}
			disk->partcount++;
		}

		partarray++;
	}
}

void disk_scan(disk_t* disk)
{
	byte* buffer = (byte*)vmalloc(0x1000, vm_write);
	char path_buf[64];
	format_to(string_span{&path_buf[0], 64}, "/dev/{}", disk->name);
	int fd = vfs::open(path_buf, 0);
	if(fd < 0)
		return;

	vfs::read(fd, buffer, 0x1000);

	disk_parse_gpt(disk, buffer);

	vfs::close(fd);
	vmfree((virtaddr_t)buffer);
}
