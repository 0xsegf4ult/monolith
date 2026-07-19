#include <fs/filesystem.h>
#include <fs/super.h>
#include <libk/list.h>
#include <libk/string.h>

static list_head_t fs_list = {&fs_list, &fs_list};

void filesystem_register(struct filesystem* fs, const char* name)
{
	strncpy(fs->name, name, 32);
	list_node_init(&fs->list_node);

	list_add_tail(&fs_list, &fs->list_node);
}

struct filesystem* filesystem_lookup(const char* name)
{
	struct filesystem* cur = nullptr;
	list_for_each_entry(cur, &fs_list, list_node)
	{
		if(strncmp(cur->name, name, 32) == 0)
			return cur;
	}

	return nullptr;
}
