#include <fs/filesystem.hpp>
#include <fs/super.hpp>
#include <fs/ops.hpp>
#include <fs/vfs.hpp>
#include <kstd.hpp>

namespace vfs
{

static vfilesystem_t* fs_list_head = nullptr;
static vfilesystem_t* fs_list_tail = nullptr;

void register_fs(vfilesystem_t* fs, const char* name)
{
	fs->next = nullptr;
        strncpy(fs->name, name, 32);

        if(!fs_list_head)
        {
                fs_list_head = fs;
                fs_list_tail = fs;
        }
        else
        {
                fs_list_tail->next = fs;
                fs_list_tail = fs;
        }
}

vfilesystem_t* lookup_fs(const char* name)
{
 	vfilesystem_t* fs = nullptr;
 	vfilesystem_t* list = fs_list_head;
        while(list)
        {
                if(strncmp(list->name, name, 32) == 0)
                {
                        fs = list;
                        break;
                }

                list = list->next;
        }
	return fs;
}

}
