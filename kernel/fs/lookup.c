#include <fs/lookup.h>
#include <fs/stat.h>
#include <fs/super.h>
#include <fs/ventry.h>
#include <fs/vnode.h>
#include <fs/vfs.h>

#include <mm/slab.h>

#include <sched/task.h>
#include <sys/mutex.h>
#include <sys/reflock.h>
#include <sys/smp.h>

#include <libk/list.h>
#include <libk/string.h>

#include <errno.h>
#include <klog.h>

static struct ventry* traverse_mount(struct ventry* source)
{
	struct ventry* mount_traverse = nullptr;
	int traverse_status = source->mount->fs->sb_ops->root(source->mount->sb, &mount_traverse);
	if(traverse_status < 0)
		return nullptr;

	return mount_traverse;
}

int vfs_lookup_at(struct ventry* parent, const char* path, struct ventry** result, int flags)
{                
        struct ventry* cache_entry = dcache_get(parent, path);
        if(cache_entry)
        {
                if(flags & LOOKUP_PARENT)
                        *result = cache_entry->parent;
                else
                        *result = cache_entry;

		struct ventry* res = *result;
		if(res != vfs_context()->root_node && res->mount)	
		{
			struct ventry* mtrav = traverse_mount(res);
			if(!mtrav)
				return -ENOENT;
			*result = mtrav;
		}

                return 0;
        }
                        
        size_t len = strlen(path);

        char* cbuffer = (char*)kmalloc(len + 1);
        strncpy(cbuffer, path, len + 1);

        for(size_t i = 0; i < len; i++)
        {
                if(cbuffer[i] == '/')
                        cbuffer[i] = '\0';
        }

        struct ventry* current = parent;
        const char* basename = nullptr;

        for(size_t i = 0; i < len; i++)
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
                size_t clen = strlen(component);
                bool is_last = (i + clen) == len;
                if(!is_last)
                {
                        size_t j;
                        for(j = i + clen; j < len && component[j] == '\0'; ++j) {}
                        is_last = (j == len);
                }

                if(is_last && (flags & LOOKUP_PARENT))
                {
                        ventry_put(current);
                        break;
                }

                struct ventry* next;

                if(clen == 1 && component[0] == '.')
                        next = current;
                else if(clen == 2 && component[0] == '.' && component[1] == '.')
                {
                        next = current->parent;

                        if(!current->parent)
                        {
				struct mount* mp;
                                list_head_t* mplist = &vfs_context()->mounts;
				list_for_each_entry(mp, mplist, list_node)
                                {
                                        struct ventry* mount_root = nullptr;
                                        int tstat = mp->fs->sb_ops->root(mp->sb, &mount_root);
                                        if(tstat >= 0 && mount_root == current)
                                        {
                                                next = (mp->mountpoint == vfs_context()->root_node) ? vfs_context()->root_node : mp->mountpoint->parent;
                                                break;
                                        }
                                }
                        }
                }
 		else
                {
                        if(current != vfs_context()->root_node && current->mount)
                        {
                                struct ventry* mount_traverse = traverse_mount(current);
                                if(!mount_traverse)
                                {
                                        ventry_put(current);
                                        return -ENOENT;
                                }

                                ventry_put(current);
                                current = mount_traverse;
                                ventry_ref(current);
                        }

        		struct ventry* cache_entry = dcache_get(current, component);
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
        if(current && current != vfs_context()->root_node && current->mount)
        {
                struct ventry* mount_traverse = traverse_mount(current);
		if(!mount_traverse)
                        return -ENOENT;
                        
		current = mount_traverse;
        }

        *result = current;
        if(!current)
                return -ENOENT;

        return 0;
}

int vfs_lookup(const char* path, struct ventry** result, int flags)
{
        if(path[0] == '/')
                return vfs_lookup_at(vfs_context()->root_node, path + 1, result, flags);
        else
                return vfs_lookup_at(smp_current_task()->cwd, path, result, flags);
}
