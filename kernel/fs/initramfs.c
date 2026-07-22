#include <fs/initramfs.h>
#include <fs/stat.h>
#include <fs/vfs.h>

#include <libk/vsprintf.h>
#include <libk/string.h>
#include <klog.h>
#include <types.h>

typedef struct ustar_record
{
	char name[100];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char checksum[8];
	char type;
	char link_name[100];
	char ustar[8];
	char owner[32];
	char group[32];
	char major[8];
	char minor[8];
	char prefix[155];
} ustar_record;

static uint32_t oct2uint(char* oct, uint32_t size)
{
	uint32_t out = 0;
	int i = 0;
	while((i < size) && oct[i])
		out = (out << 3) | (uint32_t)(oct[i++] - '0');

	return out;
}

void initramfs_unpack(byte* data, size_t length)
{
	klog("init: unpacking initramfs...\n");
	byte* end = data + length;

	while(true)
	{
		ustar_record* record = (ustar_record*)data;

		char path[64];

		size_t namel = strlen(record->name);
		if(namel > 63)
		{
			klog("initramfs_tar: filename too long\n");
			return;
		}
		
		if(namel == 0)
			break;

		sprintf(path, "/%s", record->name);
		if(path[namel] == '/')
			path[namel] = '\0';

		uint32_t mode = oct2uint(record->mode, 7);		
		uint32_t size = oct2uint(record->size, 11);
		if(record->type == '5')
		{
			vfs_mkdir(path, mode);
		}
		else if(record->type == '0')
		{
			vfs_create(path, mode);
			int fd = vfs_open(path, 0);
			vfs_write(fd, data + 512, size);
			vfs_close(fd);
		}

		if(size % 512)
			size += 512 - (size % 512);

		data += (size + 512);
		if(data >= end)
			break;
	}
}
