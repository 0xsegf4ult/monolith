#include <fs/filesystem.hpp>
#include <fs/super.hpp>
#include <fs/ops.hpp>
#include <fs/vfs.hpp>
#include <kstd.hpp>

namespace vfs
{

static list_head_t fs_list = {&fs_list, &fs_list};

void register_fs(vfilesystem_t* fs, const char* name)
{
        strncpy(fs->name, name, 32);
	list_node_init(fs->list_node);

	list_add_tail(fs_list, fs->list_node);
}

vfilesystem_t* lookup_fs(const char* name)
{
	vfilesystem_t* fs = nullptr;
	vfilesystem_t* result = nullptr;
	list_for_each_entry(fs, fs_list, list_node)
	{
		if(strncmp(fs->name, name, 32) == 0)
		{
			result = fs;
			break;
		}
	}

	return result;
}

}
