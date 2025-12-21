#include <init/initramfs.hpp>

#include <fs/vfs.hpp>

#include <lib/klog.hpp>
#include <lib/types.hpp>

struct ustar_record
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
};

uint32_t oct2uint(char* oct, uint32_t size)
{
	uint32_t out = 0;
	int i = 0;
	while((i < size) && oct[i])
		out = (out << 3) | static_cast<uint32_t>(oct[i++] - '0');

	return out;
}

void initramfs_unpack(byte* data, size_t length)
{
	log::info("init: unpacking initramfs...");
	auto* end = data + length;

	while(true)
	{
		ustar_record* record = reinterpret_cast<ustar_record*>(data);

		auto namel = string_length(record->name);
		if(namel > 64)
		{
			log::error("tar: filename too long");
			return;
		}
		
		if(namel == 0)
			break;
		
		auto size = oct2uint(record->size, 11);
		if(record->type == '5')
		{
			vfs::mkdir(record->name);
		}
		else if(record->type == '0')
		{
			vfs::create(record->name);
			auto fd = vfs::open(record->name);
			vfs::write(fd, data + 512, size);
			vfs::close(fd);
		}

		if(size % 512)
			size += 512 - (size % 512);

		data += (size + 512);
		if(data >= end)
			break;
	}
}
