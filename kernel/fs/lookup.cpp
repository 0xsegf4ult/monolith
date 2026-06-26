#include <fs/lookup.hpp>
#include <fs/ventry.hpp>
#include <fs/vnode.hpp>
#include <fs/super.hpp>
#include <fs/vfs.hpp>

#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/smp.hpp>

#include <lib/kstd.hpp>
#include <lib/klog.hpp>

#include <mm/slab.hpp>

#include <sys/err.hpp>
#include <sys/mutex.hpp>
#include <sys/reflock.hpp>
#include <sys/thread.hpp>

namespace vfs
{

ventry_t* traverse_mount(ventry_t* source)
{
	ventry_t* mount_traverse = nullptr;
	auto traverse_status = source->mount->fs->sb_ops->root(source->mount->sb, &mount_traverse);
	if(traverse_status < 0)
		return nullptr;

	return mount_traverse;
}

int lookup_at(ventry_t* parent, const char* path, ventry_t** result, int flags)
{                
        auto* cache_entry = dcache_get(parent, path);
        if(cache_entry)
        {
                if(flags & LOOKUP_PARENT)
                        *result = cache_entry->parent;
                else
                        *result = cache_entry;

		ventry_t* res = *result;
		if(res != vfs::get()->root_node && res->mount)	
		{
			auto mtrav = traverse_mount(res);
			if(!mtrav)
				return -ENOENT;
			*result = mtrav;
		}

                return 0;
        }
                        
        auto len = string_length(path);

        char* cbuffer = (char*)kmalloc(len + 1);
        strncpy(cbuffer, path, len + 1);

        for(int i = 0; i < len; i++)
        {
                if(cbuffer[i] == '/')
                        cbuffer[i] = '\0';
        }

        ventry_t* current = parent;
        ventry_t* c_parent = nullptr;
        const char* basename = nullptr;

        for(int i = 0; i < len; i++)
        {
                if(cbuffer[i] == '\0')
                        continue;

                if(!current)
                        break;

                ventry_ref(current);
                if(!S_ISDIR(current->node->mode))
                {
                        ventry_put(current);
                        return -ENOTDIR;
                }

                basename = &path[i];

                char* component = &cbuffer[i];
                size_t clen = string_length(component);
                bool is_last = (i + clen) == len;
                if(!is_last)
                {
                        int j;
                        for(j = i + clen; j < len && component[j] == '\0'; ++j) {}
                        is_last = (j == len);
                }

                if(is_last && (flags & LOOKUP_PARENT))
                {
                        ventry_put(current);
                        break;
                }

                ventry_t* next;

                if(clen == 1 && component[0] == '.')
                        next = current;
                else if(clen == 2 && component[0] == '.' && component[1] == '.')
                {
                        if(i + clen >= len)
                                c_parent = current;

                        next = current->parent;

                        if(!current->parent)
                        {
                                mount_t* mp = vfs::get()->mounts;
                                while(mp)
                                {
                                        ventry_t* mount_root = nullptr;
                                        auto tstat = mp->fs->sb_ops->root(mp->sb, &mount_root);
                                        if(tstat >= 0 && mount_root == current)
                                        {
                                                next = (mp->mountpoint == vfs::get()->root_node) ? vfs::get()->root_node : mp->mountpoint->parent;
                                                break;
                                        }
                                        mp = mp->next;
                                }
                        }
                }
 		else
                {
                        if(current != vfs::get()->root_node && current->mount)
                        {
                                ventry_t* mount_traverse = traverse_mount(current);
                                if(!mount_traverse)
                                {
                                        ventry_put(current);
                                        return -ENOENT;
                                }

                                ventry_put(current);
                                current = mount_traverse;
                                ventry_ref(current);
                        }

        		auto* cache_entry = dcache_get(current, component);
        		if(cache_entry)
			{
				next = cache_entry;
			}
			else
			{
				if(!current->node->iops->lookup)
				{
					ventry_put(current);
					return -ENOENT;
				}

				next = current->node->iops->lookup(current, component);
				if(next) 
					dcache_insert(next);
			}
                }

                i += clen;
                ventry_put(current);
                current = next;
        }

        kfree(cbuffer);
        if(current && current != vfs::get()->root_node && current->mount)
        {
                ventry_t* mount_traverse = traverse_mount(current);
		if(!mount_traverse)
                        return -ENOENT;
                        
		current = mount_traverse;
        }

        *result = current;
        if(!current)
                return -ENOENT;

        return 0;
}

int lookup(const char* path, ventry_t** result, int flags)
{
        if(path[0] == '/')
                return lookup_at(get_root_dentry(), path + 1, result, flags);
        else
                return lookup_at(smp_current_cpu()->get_current_thread()->cwd, path, result, flags);
}

}
