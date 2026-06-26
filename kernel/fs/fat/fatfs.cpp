#include <fs/fat/fatfs.hpp>
#include <fs/generic.hpp>
#include <fs/vfs.hpp>
#include <dev/block/block.hpp>
#include <mm/slab.hpp>
#include <mm/vmm.hpp>
#include <kstd.hpp>
#include <types.hpp>

#include <klog.hpp>

#include <sys/err.hpp>

using namespace vfs;

struct fatfs_superblock
{
	superblock_t* sb;
	ventry_t* root;

	size_t cluster_count;
	size_t cluster_size;

	size_t fat_size;
	size_t fat_count;

	size_t fat_offset;
	size_t data_offset;

	uint32_t root_cluster;
};

struct fatfs_node
{
	vnode_t* node;
	uint32_t cluster;
	size_t size;
};

struct __attribute__((packed)) fatfs_dirent_t
{
	char short_name[11];
	uint8_t attrib;
	uint8_t reserved;
	uint8_t ctime_hundredths;
	uint16_t ctime;
	uint16_t cdate;
	uint16_t adate;
	uint16_t cluster_high;
	uint16_t mtime;
	uint16_t mdate;
	uint16_t cluster_low;
	uint32_t size_bytes;
};

struct __attribute__((packed)) fatfs_lfn_t
{
	uint8_t order;
	uint16_t name1[5];
	uint8_t attrib;
	uint8_t reserved;
	uint8_t checksum;
	uint16_t name2[6];
	uint16_t zero;
	uint16_t name3[2];
};

ssize_t fatfs_getdents(file_descriptor_t* file, byte* buffer, size_t length)
{
	log::debug("fatfs_getdents");

	byte* write_head = buffer;

	vnode_t* vnode = file->inode;
	fatfs_node* fnode = (fatfs_node*)vnode->data;
	fatfs_superblock* sb = (fatfs_superblock*)vnode->sb->data;
	
	byte clu_data[512];
	sb->sb->bdev->bops->pread_blocks(sb->sb->bdev, clu_data, 1, sb->data_offset + (fnode->cluster - 2) * sb->cluster_size);

	auto* dentry = file->path;

	char fname[64];
	
	size_t offset = 0;
	ventry_t* entry = nullptr;
	while(write_head < buffer + length)
	{
		fatfs_dirent_t* fat_dirent = (fatfs_dirent_t*)((byte*)clu_data + offset * sizeof(fatfs_dirent_t));
		size_t name_len = 0;
		if(fat_dirent->short_name[0] == '\0')
			break;

		if(*(uint8_t*)(fat_dirent) == 0xe5)
		{
			offset++;
			continue;
		}

		if(fat_dirent->attrib == 0x0F)
		{
			bool rdstop = false;
			fatfs_lfn_t* lfn = (fatfs_lfn_t*)fat_dirent;
			while(!rdstop)
			{
				for(int i = 0; i < 5; i++)
					fname[name_len++] = char(lfn->name1[i]);
				for(int i = 0; i < 6; i++)
					fname[name_len++] = char(lfn->name2[i]);
				
				fname[name_len++] = char(lfn->name3[0]);
				fname[name_len++] = char(lfn->name3[1]);

				log::debug("read lfn");
				if(lfn->order & 0x40)
				{
					fname[name_len++] = '\0';
					log::debug("write out {}", fname);
					rdstop = true;
				}
				else
				{
					lfn++;
				}
				offset++;
			}
		}
		else
		{
			strncpy(fname, fat_dirent->short_name, 11);
			fname[11] = '\0';
			name_len = string_length(fname) + 1;
		}

		dirent_info* dirent = (dirent_info*)write_head;
		
		dirent->length = sizeof(dirent_info) + name_len;
		dirent->type = 0;

		write_head += sizeof(dirent_info);
		memcpy(write_head, fname, name_len);
		write_head += name_len;

		offset++;
	}

	return static_cast<ssize_t>(write_head - buffer);
}

static fs_inode_ops fatfs_iops =
{
	.getdents = fatfs_getdents
};

static fs_file_ops fatfs_fops =
{
	.open = generic_fs_open,
	.close = generic_fs_close,
};

struct __attribute__((packed)) ebp_fat32_t
{
	uint32_t fat_size;
	uint16_t flags;
	uint16_t version;
	uint32_t root_cluster;
	uint16_t fsinfo_cluster;
	uint16_t bkp_boot_cluster;
	uint8_t reserved[12];
	uint8_t drive_number;
	uint8_t reserved2;
	uint8_t signature;
	uint32_t serial_number;
	uint8_t label[11];
	uint8_t type[8];
};

struct __attribute__((packed)) bpb_t
{
	uint8_t reserved[3];
	uint8_t oem[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sector_count;
	uint8_t fat_count;
	uint16_t root_dirent_count;
	uint16_t sector_count16;
	uint8_t media_desc_type;
	uint16_t fat_size16;
	uint16_t sectors_per_track;
	uint16_t num_heads;
	uint32_t num_hidden_sectors;
	uint32_t sector_count32;

	union
	{
		ebp_fat32_t ebp_fat32;
	};
};


int fatfs_super_init(block_device_t* bdev, superblock_t** out_sb)
{
        auto* sb = (superblock_t*)kmalloc(sizeof(superblock_t));

	auto* priv_data = (fatfs_superblock*)kmalloc(sizeof(fatfs_superblock));
	sb->data = (void*)priv_data;
	priv_data->sb = sb;
	priv_data->root = nullptr;

	auto buffer = vmalloc(0x1000, vm_write);
	bdev->bops->pread_blocks(bdev, (byte*)buffer, 2, 0);

	bpb_t* bpb = (bpb_t*)buffer;
	log::debug("fatfs: oem {}", (const char*)&bpb->oem[0]);
	log::debug("fatfs: root entry count {}", bpb->root_dirent_count);
	log::debug("fatfs: reserved sectors {}", bpb->reserved_sector_count);
	log::debug("fatfs: fat count {}", bpb->fat_count);

	priv_data->fat_offset = bpb->reserved_sector_count * bpb->sectors_per_cluster; 
	priv_data->fat_count = bpb->fat_count;
	priv_data->cluster_size = bpb->sectors_per_cluster;
	if(bpb->fat_size16 == 0)
	{
		log::debug("fatfs: fs is FAT32");
		log::debug("fatfs: sector_count16 {}", bpb->sector_count16);
		log::debug("fatfs: sector_count32 {}", bpb->sector_count32);
		size_t total_sectors = bpb->sector_count16 ? bpb->sector_count16 : bpb->sector_count32;
		size_t data_sectors = total_sectors - bpb->reserved_sector_count - bpb->fat_count * bpb->ebp_fat32.fat_size;
		size_t cluster_count = data_sectors / bpb->sectors_per_cluster;
		priv_data->cluster_count = cluster_count;
		log::debug("fatfs: cluster count {}", cluster_count);
		log::debug("fatfs: cluster size {}", priv_data->cluster_size);
		priv_data->root_cluster = bpb->ebp_fat32.root_cluster;
		priv_data->fat_size = bpb->ebp_fat32.fat_size;
		priv_data->data_offset = priv_data->fat_offset + priv_data->fat_count * priv_data->fat_size;
		log::debug("fatfs: data at LBA {}", priv_data->data_offset);
	}
	else
	{
		return -EINVAL;		
	}

	*out_sb = sb;
        return 0;
}


int fatfs_super_root(superblock_t* sb, ventry_t** out_root)
{
	auto* fatfs_sb = (fatfs_superblock*)sb->data;
	if(fatfs_sb->root)
	{
		*out_root = fatfs_sb->root;
	}
	else
	{
		auto* node = vnode_new(S_IFDIR | 0755);
		node->sb = sb;
		node->iops = sb->fs->iops;
		node->fops = sb->fs->fops;

		auto* dentry = ventry_new("fatfs", node);
		fatfs_sb->root = dentry;
	       	*out_root = dentry;	
		
		auto* fatnode = (fatfs_node*)kmalloc(sizeof(fatfs_node));
		fatnode->node = node;
		node->data = (void*)fatnode;
		fatnode->cluster = fatfs_sb->root_cluster;

		return 0;
	}

	return 0;
}

static fs_super_ops fatfs_sb_ops
{
	.init = fatfs_super_init,
	.root = fatfs_super_root
};

void fatfs_init()
{
	auto* fs = (vfilesystem_t*)kmalloc(sizeof(vfilesystem_t));
	fs->flags = 0;
	fs->iops = &fatfs_iops;
	fs->fops = &fatfs_fops;
	fs->sb_ops = &fatfs_sb_ops;

	register_fs(fs, "fatfs");
}

