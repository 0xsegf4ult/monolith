#include <fs/fat/fatfs.h>
#include <fs/filesystem.h>
#include <fs/ops.h>
#include <fs/stat.h>
#include <fs/super.h>
#include <fs/vfs.h>
#include <mm/slab.h>
#include <mm/vmm.h>
#include <sys/device.h>
#include <libk/string.h>
#include <errno.h>
#include <types.h>
#include <klog.h>

struct fatfs_superblock
{
	struct superblock* sb;
	struct ventry* root;

	size_t cluster_count;
	size_t cluster_size;
	size_t fat_count;
	size_t fat_size;
	size_t fat_offset;
	size_t data_offset;

	uint32_t root_cluster;
};

struct fatfs_node
{
	struct vnode* node;
	uint32_t cluster;
	size_t size;
};

struct __attribute__((packed)) fatfs_dirent
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

struct __attribute__((packed)) fatfs_lfn
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

struct __attribute__((packed)) ebp_fat32
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

struct __attribute__((packed)) bpb
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
		struct ebp_fat32 ebp_fat32;
	};
};

ssize_t fatfs_getdents(struct file_descriptor* file, byte* buffer, size_t length)
{
	byte* write_head = buffer;
	struct vnode* vnode = file->inode;
	struct fatfs_node* fnode = (struct fatfs_node*)vnode->data;
	struct fatfs_superblock* sb = (struct fatfs_superblock*)vnode->sb->data;

	byte clu_data[512];
	sb->sb->bdev->bd_ops->pread_blocks(sb->sb->bdev, clu_data, 1, sb->data_offset + (fnode->cluster - 2) * sb->cluster_size);

	char fname[64];

	size_t offset = 0;
	while(write_head < buffer + length)
	{
		struct fatfs_dirent* fat_dirent = (struct fatfs_dirent*)((byte*)clu_data + offset * sizeof(struct fatfs_dirent));
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
			struct fatfs_lfn* lfn = (struct fatfs_lfn*)fat_dirent;
			while(!rdstop)
			{
				for(int i = 0; i < 5; i++)
					fname[name_len++] = (char)lfn->name1[i];
				for(int i = 0; i < 6; i++)
					fname[name_len++] = (char)lfn->name2[i];
				fname[name_len++] = (char)lfn->name3[0];
				fname[name_len++] = (char)lfn->name3[1];

				if(lfn->order & 0x40)
				{
					fname[name_len++] = '\0';
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
			name_len = strlen(fname) + 1;
		}

		struct posix_dent* dirent = (struct posix_dent*)write_head;
		dirent->d_reclen = offsetof(struct posix_dent, d_name) + name_len;
		dirent->d_ino = 0;
		dirent->d_off = 0;
		dirent->d_type = 0;

		write_head += offsetof(struct posix_dent, d_name);
		memcpy(write_head, fname, name_len);
		write_head += name_len;

		offset++;
	}

	return (ssize_t)(write_head - buffer);
}

static struct inode_ops fatfs_iops =
{
	.getdents = fatfs_getdents
};

static struct file_ops fatfs_fops =
{
};

static int fatfs_super_init(struct block_device* bdev, struct superblock** out_sb)
{
	struct superblock* sb = kmalloc(sizeof(struct superblock));
	struct fatfs_superblock* priv_data = kmalloc(sizeof(struct fatfs_superblock));
	
	sb->data = priv_data;
	priv_data->sb = sb;
	priv_data->root = nullptr;

	byte* buffer = vmalloc(0x1000);
	bdev->bd_ops->pread_blocks(bdev, buffer, 2, 0);

	struct bpb* bpb = (struct bpb*)buffer;

	priv_data->fat_offset = bpb->reserved_sector_count * bpb->sectors_per_cluster;
	priv_data->fat_count = bpb->fat_count;
	priv_data->cluster_size = bpb->sectors_per_cluster;
	if(bpb->fat_size16 == 0)
	{
		size_t total_sectors = bpb->sector_count16 ? bpb->sector_count16 : bpb->sector_count32;
		size_t data_sectors = total_sectors - bpb->reserved_sector_count - bpb->fat_count * bpb->ebp_fat32.fat_size;
		size_t cluster_count = data_sectors / bpb->sectors_per_cluster;
		priv_data->cluster_count = cluster_count;
		priv_data->root_cluster = bpb->ebp_fat32.root_cluster;
		priv_data->fat_size = bpb->ebp_fat32.fat_size;
		priv_data->data_offset = priv_data->fat_offset + priv_data->fat_count * priv_data->fat_size;
	}
	else
	{
		vfree(buffer);
		return -EINVAL;
	}

	
	*out_sb = sb;
	vfree(buffer);
	return 0;
}

static int fatfs_super_root(struct superblock* sb, struct ventry** out_root)
{
	struct fatfs_superblock* fatfs_sb = (struct fatfs_superblock*)sb->data;
	if(fatfs_sb->root)
	{
		*out_root = fatfs_sb->root;
		return 0;
	}

	struct vnode* node = vnode_new(S_IFDIR | 0755);
	node->sb = sb;
	node->iops = sb->fs->iops;
	node->fops = sb->fs->fops;

	struct ventry* ventry = ventry_new("fatfs", node);
	fatfs_sb->root = ventry;
	*out_root = ventry;

	struct fatfs_node* fatnode = kmalloc(sizeof(struct fatfs_node));
	fatnode->node = node;
	node->data = fatnode;
	fatnode->cluster = fatfs_sb->root_cluster;

	return 0;
}

static struct super_ops fatfs_sb_ops =
{
	.init = fatfs_super_init,
	.root = fatfs_super_root
};

void fatfs_init()
{
	struct filesystem* fs = kmalloc(sizeof(struct filesystem));
	fs->flags = 0;
	fs->iops = &fatfs_iops;
	fs->fops = &fatfs_fops;
	fs->sb_ops = &fatfs_sb_ops;

	filesystem_register(fs, "fatfs");
}
