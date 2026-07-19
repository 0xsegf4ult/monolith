#pragma once

struct ventry;	

enum LOOKUP_FLAGS
{
        LOOKUP_PARENT = 1
};

int vfs_lookup_at(struct ventry* parent, const char* path, struct ventry** result, int flags);
int vfs_lookup(const char* path, struct ventry** result, int flags);
